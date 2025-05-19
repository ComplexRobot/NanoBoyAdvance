#include <fstream>
#include <filesystem>
#include <vector>

#include "SongData.h"


namespace nba {

  SongData songData[512];

  void LoadSongData() {
    std::filesystem::directory_entry directoryEntry{ "pokegbasoundmetaparser\\song-data.csv" };

    std::vector<char> data(directoryEntry.file_size());

    std::ifstream songDataFile{ directoryEntry.path() };
    songDataFile.read(data.data(), data.size());
    songDataFile.close();

    char* c = data.data();
    const char* const end = data.data() + data.size();
    // Skip first line
    while (c < end && *c++ != '\n');

    // Index,Filename,Tempo,Tick Length,Looping,Loop Start,Stereo
    enum Column : size_t {
      INDEX,
      FILENAME,
      TEMPO,
      TICK_LENGTH,
      LOOPING,
      LOOP_START,
      STEREO,
      MAX_COLUMNS
    };

    int64_t index = 0;
    size_t column = 0;
    std::string currentLine;
    for (; c < end; ++c) {
      if (*c == ',' || *c == '\n') {

        switch (column) {
        case INDEX:
          index = std::stoll(currentLine);
          break;
        case FILENAME:
          songData[index].name = currentLine;
          break;
        case TEMPO:
          songData[index].tempo = std::stoll(currentLine);
          break;
        case TICK_LENGTH:
          songData[index].duration = std::stoll(currentLine);
          break;
        case LOOPING:
          songData[index].looping = (currentLine == "TRUE" ? true : false);
          break;
        case LOOP_START:
          songData[index].loopStartPoint = std::stoll(currentLine);
          break;
        case STEREO:
          songData[index].stereo = (currentLine == "TRUE" ? true : false);
          break;
        }

        currentLine.clear();

        if (++column >= MAX_COLUMNS) {
          column = 0;
        }
      } else {
        currentLine.push_back(*c);
      }
    }

    std::ofstream loopPointFile("sounds\\loop-points.csv");
    loopPointFile << "Name,Loop Point" << std::endl;
  }

}
