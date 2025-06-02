/*
 * Copyright (C) 2024 fleroviux
 *
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#pragma once

#include <nba/integer.hpp>
#include <string.h>

namespace nba {

template<typename T>
const T& read(void const* data, uint offset) {
  return *(T*)((u8*)data + offset);
}

template<typename T>
void write(void* data, uint offset, const T& value) {
  *(T*)((u8*)data + offset) = value;
}

} // namespace nba
