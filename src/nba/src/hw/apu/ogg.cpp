#include <limits>
#include "ogg.h"

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
  const float OGG::SAMPLE_THRESHOLD = 0.000001;
  bool OGG::stereo = true;
  bool OGG::autoFlush = true;
  size_t OGG::maxSamples = std::numeric_limits<size_t>::max();


  void OGG::Start(const std::filesystem::path& path, bool stereo) {
    OGG::stereo = stereo;
    autoFlush = true;
    maxSamples = std::numeric_limits<size_t>::max();
    vorbis_info_init(&vi);
    if (vorbis_encode_init_vbr(&vi, (stereo ? 2 : 1), 65835, 1) != 0) {
      /// TODO: error recovery
      vorbis_info_clear(&vi);
      return;
    }

    sampleCount = 0;

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

  void OGG::SetMaxSamples(size_t value) {
    maxSamples = value;
  }

  size_t OGG::GetMaxSamples() {
    return maxSamples;
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

}
