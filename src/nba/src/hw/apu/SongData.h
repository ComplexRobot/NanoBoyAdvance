#pragma once

#include <string>


namespace nba {

  // Index,Filename,Duration,Looping,Loop Start,Stereo
  class SongData {
  public:
    std::string name;
    double duration;
    bool looping;
    double loopStartPoint;
    bool stereo;
  };

  extern SongData songData[512];

  void LoadSongData();

}
