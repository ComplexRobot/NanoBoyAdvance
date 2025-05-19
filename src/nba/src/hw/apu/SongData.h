#pragma once

#include <string>


namespace nba {

  // Index,Filename,Tempo,Tick Length,Looping,Loop Start,Stereo
  class SongData {
  public:
    std::string name;
    int64_t tempo;
    int64_t duration;
    bool looping;
    int64_t loopStartPoint;
    bool stereo;
  };

  extern SongData songData[512];

  void LoadSongData();

}
