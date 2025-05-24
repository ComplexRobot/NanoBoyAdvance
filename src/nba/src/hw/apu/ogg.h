#pragma once

#include <filesystem>
#include <fstream>
#include <vorbis/vorbisenc.h>

#include "apu.hpp"

namespace nba::core {

  class OGG {
    static ogg_stream_state os; /* take physical pages, weld into a logical stream of packets */
    static ogg_page         og; /* one Ogg bitstream page.  Vorbis packets are inside */
    static ogg_packet       op; /* one raw packet of data for decode */
    static vorbis_info      vi; /* struct that stores all the static vorbis bitstream settings */
    static vorbis_comment   vc; /* struct that stores all the user comments */
    static vorbis_dsp_state vd; /* central working state for the packet->PCM decoder */
    static vorbis_block     vb; /* local working space for packet->PCM decode */

    static std::vector<StereoSample<float>> buffer;
    static int id;
    static std::ofstream outFile;
    static bool stereo;
    static bool autoFlush;
    static size_t maxSamples;
    static const size_t MAX_BUFFER_SIZE = 36864;

  public:
    static size_t sampleCount;
    static const float SAMPLE_THRESHOLD;
    static void Start(const std::filesystem::path& path, bool stereo = true);
    static void AddSample(const StereoSample<float>& sample);
    static void Flush();
    static void End();
    static size_t GetSampleCount();
    static void SetAutoFlush(bool value);
    static void SetMaxSamples(size_t value);
    static size_t GetMaxSamples();
  private:
    static void WriteDataToFile();
    static void TrimBuffer();
  };

}
