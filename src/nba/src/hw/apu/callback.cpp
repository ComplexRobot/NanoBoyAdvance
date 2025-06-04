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
      // TODO: Make this a function
      // Alternative to AVX: use __stosd or __stosq

      // Last element packed as a 32-bit int
      const u32& backBytes = *(u32*)&apu->buffer->Back();
      // Store repeated 32-bit value in a 256-bit vector
      const __m256i back256 = _mm256_set1_epi32(backBytes);
      constexpr size_t COUNT_ALIGN16 = 16 / sizeof(u32);
      constexpr size_t COUNT_ALIGN32 = 32 / sizeof(u32);

      auto CheckAlign = [&] () -> bool {
        // If aligned 128-bit copy is impossible - just do a simple loop
        const size_t address = (size_t)sampleStream / COUNT_ALIGN16;
        const size_t count = streamEnd - sampleStream;
        if (count < COUNT_ALIGN16 || count - ((0 - address) % COUNT_ALIGN16) < COUNT_ALIGN16) {
          while (sampleStream < streamEnd) {
            *(u32*)sampleStream++ = backBytes;
          }
          return true;
        }
        return false;
      };

      auto AlignAddress32 = [&] () -> bool {
        // Align the address to 16 byte alignment
        while ((size_t)sampleStream % 16 != 0) {
          if (sampleStream >= streamEnd) {
            return true;
          }
          *(u32*)sampleStream++ = backBytes;
        }

        // After aligning to 16, check for exact 32 byte alignment
        const bool UNALIGNED = (size_t)sampleStream / 16 % 2 != 0;

        // Align the address to 32 byte alignment
        if (sampleStream < streamEnd && UNALIGNED) {
          *(__m128i*)sampleStream = *(__m128i*)&back256;
          sampleStream += COUNT_ALIGN16;
        }

        return sampleStream >= streamEnd;
      };

      // Split into 2 functions to force inlining
      if (CheckAlign() || AlignAddress32()) {
        break;
      }

      // Copy 256 bits at a time
      for (
        // 32 byte aligned end pointer (rounded down)
        StereoSample<s16>* const endAlign32 = sampleStream + (streamEnd - sampleStream & 0 - COUNT_ALIGN32);
        sampleStream < endAlign32;
        sampleStream += COUNT_ALIGN32
        ) {
        *(__m256i*)sampleStream = back256;
      }

      // Copy to any remaining elements

      if (CheckAlign() || AlignAddress32()) {
        break;
      }

      while (sampleStream < streamEnd) {
        *(u32*)sampleStream++ = backBytes;
      }
      
      break;
    }
  }
 
}

} // namespace nba::core
