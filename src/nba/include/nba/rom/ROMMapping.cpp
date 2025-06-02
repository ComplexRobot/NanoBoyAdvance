#include "ROMMapping.h"
#include <fstream>
#include <cctype>
#include <algorithm>

namespace nba {

  std::unordered_map<std::string, uint32_t> ROMAddresses;
  static Main* gMain = nullptr;
  static MusicPlayer* gMPlayTable = nullptr;
  static Song* gSongTable = nullptr;

  namespace ROMMap {
    core::Bus* bus = nullptr;

    Main& gMain() {
      return *nba::gMain;
    }

    MusicPlayer* const gMPlayTable() {
      return nba::gMPlayTable;
    }

    Song* const gSongTable() {
      return nba::gSongTable;
    }
  }

  void LoadROMMappings(const std::filesystem::path& mappingFilePath) {
    if (!std::filesystem::exists(mappingFilePath)) {
      return;
    }

    std::filesystem::directory_entry directoryEntry{ mappingFilePath };

    std::vector<char> data(directoryEntry.file_size());

    std::ifstream mappingFile{ directoryEntry.path() };
    mappingFile.read(data.data(), data.size());
    mappingFile.close();

    char* c = data.data();
    const char* const end = data.data() + data.size();

    std::string currentLine;
    for (; c < end; ++c) {
      if (*c == '\n') {

        try {
          size_t pos;
          uint32_t address = std::stoul(currentLine, &pos, 16);

          while (std::isblank(currentLine[pos])) {
            ++pos;
          }

          if (currentLine.ends_with(" = .")) {
            currentLine.erase(currentLine.size() - (_countof(" = .") - 1));
          }

          if (std::none_of(currentLine.begin() + pos, currentLine.end(), [] (const char& c) { return !std::isalnum(c) && c != '_'; })) {
            std::string name{ currentLine.substr(pos) };

            ROMAddresses.insert({ name, address });

            if (name == "gMain") {
              gMain = ROMAddressToPointer<Main>(address);
            } else if (name == "gMPlayTable") {
              gMPlayTable = ROMAddressToPointer<MusicPlayer>(address);
            } else if (name == "gSongTable") {
              gSongTable = ROMAddressToPointer<Song>(address);
            }
          }

        } catch (const std::invalid_argument&) { }

        currentLine.clear();

      } else {
        currentLine.push_back(*c);
      }
    }
  }

}
