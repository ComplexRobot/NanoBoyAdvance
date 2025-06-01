/*
 * Copyright (C) 2024 fleroviux
 *
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#include <algorithm>
#include <cmath>

#include "hw/apu/apu.hpp"

namespace nba::core {

void AudioCallback(APU* apu, s16* stream, int byte_len) {
  std::lock_guard<std::mutex> guard(apu->buffer_mutex);

  // Do not try to access the buffer if it wasn't setup yet.
  if(!apu->buffer) {
    return;
  }

  StereoSample<s16>* sampleStream = (StereoSample<s16>*)stream;
  const StereoSample<s16>* streamEnd = (StereoSample<s16>*)((u8*)stream + byte_len);

  auto CopyIncompleteBlock = [&] () {
    std::memcpy(sampleStream, apu->buffer->Data(), apu->buffer->Size() * sizeof(StereoSample<s16>));
  };

  while (sampleStream < streamEnd) {
    const size_t available = apu->buffer->Size();
    const size_t sampleCount = streamEnd - sampleStream;

    if (available >= sampleCount) {
      if (sampleCount >= apu->buffer->CountPerBlock()) {
        std::memcpy(sampleStream, apu->buffer->PopBlock(), apu->buffer->BlockByteSize());
        sampleStream += apu->buffer->CountPerBlock();

      // Potential error here if the requested byte length is consistently lower
      } else {
        CopyIncompleteBlock();
        break;
      }

    } else {
      CopyIncompleteBlock();

      sampleStream += apu->buffer->Size();

      // Fill the rest of the stream with the last element
      // - Packing the data into 32-bit integers for single operation copy
      const u32& backBytes = *(u32*)&apu->buffer->Back();
      while (sampleStream < streamEnd) {
        *(u32*)(sampleStream++) = backBytes;
      }
      break;
    }
  }
 
}

} // namespace nba::core
