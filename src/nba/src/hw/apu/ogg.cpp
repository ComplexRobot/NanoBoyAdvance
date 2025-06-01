#include <cmath>
#include <limits>
#include <iostream>
#include <iomanip>
#include <windows.h>
#include "ogg.h"
#include "SongData.h"
#include "hw\apu\apu.hpp"

namespace nba::core {

  ogg_stream_state OGG::os;
  ogg_page         OGG::og;
  ogg_packet       OGG::op;
  vorbis_info      OGG::vi;
  vorbis_comment   OGG::vc;
  vorbis_dsp_state OGG::vd;
  vorbis_block     OGG::vb;

  std::vector<StereoSample<float>> OGG::buffer;
  int OGG::id = 0;
  std::ofstream OGG::outFile;
  size_t OGG::sampleCount;
  static constexpr float SAMPLE_THRESHOLD = 0.004;
  bool OGG::stereo = true;
  bool OGG::autoFlush = true;
  size_t OGG::maxSamples = std::numeric_limits<size_t>::max();
  std::vector<OGG::CommandData> OGG::commandQueue;


  void OGG::Start(const std::filesystem::path& path, bool stereo) {
    vorbis_info_init(&vi);
    if (vorbis_encode_init_vbr(&vi, (stereo ? 2 : 1), core::SAMPLE_RATE, 1) != 0) {
      /// TODO: error recovery
      vorbis_info_clear(&vi);
      return;
    }

    sampleCount = 0;
    OGG::stereo = stereo;
    autoFlush = true;
    maxSamples = std::numeric_limits<size_t>::max();

    vorbis_comment_init(&vc);
    vorbis_analysis_init(&vd, &vi);
    vorbis_block_init(&vd, &vb);
    ogg_stream_init(&os, id++);

    ogg_packet header;
    ogg_packet header_comm;
    ogg_packet header_code;

    vorbis_analysis_headerout(&vd, &vc, &header, &header_comm, &header_code);
    ogg_stream_packetin(&os, &header); /* automatically placed in its own page */
    ogg_stream_packetin(&os, &header_comm);
    ogg_stream_packetin(&os, &header_code);

    outFile.open(path, outFile.out | outFile.trunc | outFile.binary);

    while ([&] () {
      if (ogg_stream_flush(&os, &og) == 0) {
        return false;
      }
      outFile.write((const char*)og.header, og.header_len);
      outFile.write((const char*)og.body, og.body_len);
      return true;
    }());
  }

  void OGG::AddSample(const StereoSample<float>& sample) {
    if (sampleCount < maxSamples) {
      buffer.push_back(sample);
      ++sampleCount;
    }
    
    if (autoFlush && buffer.size() >= MAX_BUFFER_SIZE) {
      Flush();
    }
  }

  void OGG::Flush() {
    if (buffer.empty()) {
      return;
    }

    float** stream = vorbis_analysis_buffer(&vd, buffer.size());

    if (stereo) {
      float* left = stream[0];
      float* right = stream[1];

      for (const StereoSample<float>& sample : buffer) {
        *left++ = sample[0];
        *right++ = sample[1];
      }

    } else {
      float* mono = stream[0];

      for (const StereoSample<float>& sample : buffer) {
        *mono++ = sample[0];
      }
    }

    vorbis_analysis_wrote(&vd, buffer.size());
    buffer.clear();
    WriteDataToFile();
  }

  void OGG::End() {
    if (maxSamples == std::numeric_limits<size_t>::max()) {
      TrimBuffer();
    }
    Flush();
    vorbis_analysis_wrote(&vd, 0);
    WriteDataToFile();
    outFile.close();

    ogg_stream_clear(&os);
    vorbis_block_clear(&vb);
    vorbis_dsp_clear(&vd);
    vorbis_comment_clear(&vc);
    vorbis_info_clear(&vi);
  }

  size_t OGG::GetSampleCount() {
    return sampleCount;
  }

  void OGG::SetAutoFlush(bool value) {
    autoFlush = value;
  }

  bool OGG::GetAutoFlush() {
    return autoFlush;
  }

  void OGG::SetMaxSamples(size_t value) {
    maxSamples = value;
  }

  size_t OGG::GetMaxSamples() {
    return maxSamples;
  }

  size_t OGG::GetBufferSize() {
    return buffer.size();
  }

  void OGG::WriteDataToFile() {
    while(vorbis_analysis_blockout(&vd, &vb) == 1) {
      /* analysis, assume we want to use bitrate management */
      vorbis_analysis(&vb, NULL);
      vorbis_bitrate_addblock(&vb);

      while(vorbis_bitrate_flushpacket(&vd, &op)) {
        /* weld the packet into the bitstream */
        ogg_stream_packetin(&os, &op);

        /* write out pages (if any) */
        while ([&] () {
          if (ogg_stream_pageout(&os, &og) == 0) {
            return false;
          }
          outFile.write((const char*)og.header, og.header_len);
          outFile.write((const char*)og.body, og.body_len);

          return ogg_page_eos(&og) != 0;
        }());
      }
    }
  }

  void OGG::TrimBuffer() {
    while (!buffer.empty() && std::abs(buffer.back()[0]) < SAMPLE_THRESHOLD && std::abs(buffer.back()[1]) < SAMPLE_THRESHOLD) {
      buffer.pop_back();
    }
  }

  static bool writingOgg = false;
  static size_t songNumber = 0;
  static size_t loopPoint = 0;
  static std::vector<StereoSample<float>> initialSamples;
  static std::unordered_map<u32, size_t> commandTimings;
  static u32 loopAddress = 0;
  static u32 endAddress = 0;
  static bool route1Check = true;

  void OGG::ProcessSample(StereoSample<float> sample, Scheduler& scheduler) {

    constexpr size_t MAX_SONGS = 346;

    enum MainState : u8 {
      INITIALIZING = 0,
      START_UP = 1,
      SONG_START = 2,
      SONG_START_COMPLETE = 3,
      SONG_STOP = 4,
      SONG_STOP_COMPLETE = 5
    };

    Main& main = *ROMAddressToPointer<Main>(ROMAddresses["gMain"]);
    if (main.state == INITIALIZING) {
      return;
    }
    if (main.state == START_UP) {
      main.state = SONG_START;
      return;
    }

    MusicPlayer* const mplayTable = ROMAddressToPointer<MusicPlayer>(ROMAddresses["gMPlayTable"]);
    Song* const songTable = ROMAddressToPointer<Song>(ROMAddresses["gSongTable"]);
    MusicPlayerInfo& mplayInfo = *ROMAddressToPointer<MusicPlayerInfo>(mplayTable[songTable[main.currentSong].ms].info);

    bool PLAYBACK_ACTIVE = !(mplayInfo.status & MUSICPLAYER_STATUS_PAUSE);
    auto LOOPING = [&] () { return OGG::GetMaxSamples() != std::numeric_limits<size_t>::max(); };

    // Assuming the loop point is within 30 seconds of the start
    constexpr size_t INITIAL_SAMPLE_SIZE = 30 * core::SAMPLE_RATE;

    if (PLAYBACK_ACTIVE) {

      // Reading MP2K script commands to find the loop point
      for (const CommandData& command : commandQueue) {
        u32 commandAddress = command.patternLevel != 0 ? command.patternAddress : command.address;
        u8* commandPointer = ROMAddressToPointer<u8>(commandAddress);

        // 0xB2 == GOTO (MP2K script)
        if (*commandPointer == (u8)0xB2) {
          // 291 = index of Route 1 music
          // Only Route 1 changes after looping for an unknown reason
          if (songNumber == 291 && route1Check) {
            route1Check = false;
            commandTimings.clear();
          } else {
            loopAddress = *(u32*)(commandPointer + 1);
            endAddress = commandAddress;
          }

        // The command pointer changes before the command is processed, therefore the timing is at the command immediately following
        } else if (!LOOPING() && loopAddress != 0 && commandAddress < endAddress && commandAddress != loopAddress) {
          // Offset the loop point to implement smooth tonal overlap
          // 292 = Route 24/Intro Scene (the start bleeds over a second into the beginning)
          const double LOOP_OFFSET = songNumber == 292 ? 1 : 0.5;
          // Loop point is shifted to a multiple of 0.1 seconds for clean numbers
          loopPoint = std::round(std::ceil((commandTimings[commandAddress] / (double)core::SAMPLE_RATE + LOOP_OFFSET) * 10) * (core::SAMPLE_RATE / 10.0));
          size_t maxSamples = loopPoint + (OGG::GetSampleCount() - commandTimings[commandAddress]);
          OGG::SetMaxSamples(maxSamples);

        } else if (OGG::GetSampleCount() < INITIAL_SAMPLE_SIZE) {
          commandTimings.insert({ commandAddress, OGG::GetSampleCount() });
        }
      }
      
    }

    commandQueue.clear();

    sample[0] = std::clamp(sample[0], -1.0f, 1.0f);
    sample[1] = std::clamp(sample[1], -1.0f, 1.0f);

    auto AddSample = [&] () {
      if (OGG::GetSampleCount() < INITIAL_SAMPLE_SIZE) {
        initialSamples.push_back(sample);
      }
      OGG::AddSample(sample);
    };

    // Used to trim silence at the end
    auto SILENT_SAMPLE = [&] () { return std::abs(sample[0]) < SAMPLE_THRESHOLD && std::abs(sample[1]) < SAMPLE_THRESHOLD; };

    // Prevent sounds from bleeding together
    constexpr float EPSILON = 1.0e-15;

    if (writingOgg) {

      if (LOOPING()) {
        // Implementing a tiny crossfade at the end for perfectly smooth loops
        constexpr size_t CROSS_FADE_LENGTH = 3 * core::SAMPLE_RATE / 60.0 + 0.5;

        if (OGG::GetSampleCount() >= OGG::GetMaxSamples() - CROSS_FADE_LENGTH && OGG::GetSampleCount() < OGG::GetMaxSamples()) {
          size_t i = OGG::GetSampleCount() - (OGG::GetMaxSamples() - CROSS_FADE_LENGTH);
          float weight = (float)(i + 1) / CROSS_FADE_LENGTH;
          sample[0] = (1 - weight) * sample[0] + weight * initialSamples[loopPoint - CROSS_FADE_LENGTH + i][0];
          sample[1] = (1 - weight) * sample[1] + weight * initialSamples[loopPoint - CROSS_FADE_LENGTH + i][1];
        }
      }

      if (SILENT_SAMPLE()) {
        constexpr size_t MAX_SILENT_LENGTH = 1.5 * SAMPLE_RATE + 0.5;
        if (OGG::GetAutoFlush()) {
          OGG::Flush();
          OGG::SetAutoFlush(false);
        } else if (OGG::GetBufferSize() >= MAX_SILENT_LENGTH) {
          PLAYBACK_ACTIVE = false;
        }
      } else {
        OGG::SetAutoFlush(true);
      }

      if (!PLAYBACK_ACTIVE) {
        if (LOOPING() || SILENT_SAMPLE()) {
          OGG::End();
          writingOgg = false;
          if (LOOPING()) {
            std::ofstream loopPointFile("sounds\\loop-points.csv", std::ofstream::app | std::ofstream::ate);
            loopPointFile << songData[songNumber].name << ',' << std::setprecision(4) << ((double)loopPoint / core::SAMPLE_RATE) << std::endl;
          } else {
            main.state = (mplayInfo.status & MUSICPLAYER_STATUS_PAUSE) ? SONG_STOP_COMPLETE : SONG_STOP;
          }

        } else {
          AddSample();
        }
      } else if (main.state < SONG_STOP) {
        AddSample();
        if (LOOPING() && OGG::GetSampleCount() >= OGG::GetMaxSamples()) {
          main.state = SONG_STOP;
        }
      }

    } else if (main.state == SONG_STOP_COMPLETE && std::abs(sample[0]) < EPSILON && std::abs(sample[1]) < EPSILON) {
      if (main.currentSong < MAX_SONGS) {
        ++main.currentSong;
        main.state = SONG_START;
      }

    } else if (main.state < SONG_STOP && PLAYBACK_ACTIVE && !SILENT_SAMPLE()) {
      songNumber = main.currentSong;
      std::string name = songData[songNumber].name;
      std::string numberString = std::string{} + (songNumber < 100 ? "0" : "") + (songNumber < 10 ? "0" : "") + std::to_string(songNumber);
      std::string outputString = numberString + " " + name + "\n";
      OutputDebugStringA(outputString.c_str());

      std::filesystem::create_directory("sounds");
      OGG::Start("sounds\\" + (name.empty() ? numberString : name) + ".ogg", songData[songNumber].stereo);

      writingOgg = true;
      initialSamples.clear();
      commandTimings.clear();
      loopAddress = 0;
      endAddress = 0;

      AddSample();

    }

  }

}
