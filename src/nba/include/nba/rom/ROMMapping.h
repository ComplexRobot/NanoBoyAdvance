#pragma once

#include <filesystem>
#include <unordered_map>
#include <string>
#include "bus/bus.hpp"
#include "gba\m4a_internal.h"
#include "main.h"
#include <nba/integer.hpp>

namespace nba {

  extern std::unordered_map<std::string, uint32_t> ROMAddresses;

  namespace ROMMap {
    extern core::Bus* bus;
    Main& gMain();
    MusicPlayer* const gMPlayTable();
    Song* const gSongTable();
  }

  void LoadROMMappings(const std::filesystem::path& mappingFilePath);

  template <typename T>
  T* ROMAddressToPointer(u32 address) {
    return (T*)ROMMap::bus->GetHostAddress(address, sizeof(T));
  }

}
