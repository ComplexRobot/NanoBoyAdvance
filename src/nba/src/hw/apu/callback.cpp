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
    std::memset(stream, 0, byte_len);
    return;
  }

  StereoSample<s16>* sampleStream = (StereoSample<s16>*)stream;
  StereoSample<s16>* const streamEnd = (StereoSample<s16>*)((u8*)stream + byte_len);

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

      // Experimental optimization - 32-bit memset using AVX

      // Last element packed as a 32-bit int
      const u32& backBytes = *(u32*)&apu->buffer->Back();
      // Store repeated 32-bit value in a 256-bit vector
      const __m256i back256 = _mm256_set1_epi32(backBytes);

      auto CheckAlign = [&] () -> bool {
        // Check if aligned 128-bit copy is impossible
        const size_t address = (size_t)sampleStream / 4;
        const size_t count = streamEnd - sampleStream;
        if (count < 4 || count - (0 - address & 3) < 4) {
          while (sampleStream < streamEnd) {
            *(u32*)sampleStream++ = backBytes;
          }
          return true;
        }
        return false;
      };

      auto AlignAddress32 = [&] () -> bool {
        // Align the address to 16 byte alignment (maximum of 3 iterations)
        while ((size_t)sampleStream & 0xF) {
          if (sampleStream >= streamEnd) {
            return true;
          }
          *(u32*)sampleStream++ = backBytes;
        }

        const bool UNALIGNED = (((size_t)sampleStream >> 4) & 1);

        // Align the address to 32 byte alignment
        if (sampleStream < streamEnd && UNALIGNED) {
          _mm_store_si128((__m128i*)sampleStream, *(__m128i*)&back256);
          sampleStream += 4;
        }

        return sampleStream >= streamEnd;
      };

      if (CheckAlign() || AlignAddress32()) {
        break;
      }

      // Copy 256 bits at a time
      StereoSample<s16>* const end8 = sampleStream + ((streamEnd - sampleStream) & ~7);
      while (sampleStream < end8) {
        _mm256_store_si256((__m256i*)sampleStream, back256);
        sampleStream += 8;
      }

      CheckAlign() || AlignAddress32();
      
      break;
    }
  }
 
}

} // namespace nba::core
