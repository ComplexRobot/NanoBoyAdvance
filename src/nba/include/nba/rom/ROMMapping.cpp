#include "ROMMapping.h"
#include <fstream>
#include <cctype>
#include <algorithm>

namespace nba {

  std::unordered_map<std::string, uint32_t> ROMAddresses;
  core::Bus* ROMMap::bus = nullptr;

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
            ROMAddresses.insert({ currentLine.substr(pos), address });
          }

        } catch (const std::invalid_argument&) { }

        currentLine.clear();

      } else {
        currentLine.push_back(*c);
      }
    }
  }

}
