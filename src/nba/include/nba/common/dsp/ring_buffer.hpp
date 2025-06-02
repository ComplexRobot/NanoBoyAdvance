/*
 * Copyright (C) 2024 fleroviux
 *
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#pragma once

#include <memory>
#include <nba/common/dsp/stereo.hpp>
#include <nba/common/dsp/stream.hpp>

namespace nba {

template<typename T, size_t LENGTH>
struct RingBuffer : Stream<T> {
  RingBuffer() {
    Reset();
  }

  size_t Available() {
    return tail - head;
  }

  void Reset() {
    head = 0;
    tail  = 0;
    std::memset(data, 0, sizeof(data));
  }

  const T& Peek(size_t offset = 0) {
    return data[(head + offset) % LENGTH];
  }

  void Push(T const& value) {
    data[++tail % LENGTH] = value;
  }

  T Pop() {
    return data[++head % LENGTH];
  }

  T Read() {
    return Pop();
  }

  void Write(T const& value) {
    Push(value);
  }

private:
  T data[LENGTH];

  size_t head;
  size_t tail;
};

// Like a ring buffer, but pops values in a single contiguous block for a fast memcpy
// Since the parameters are compile-time constants, it's relying upon dependent systems working predictably
template<typename T, size_t LENGTH, size_t BLOCKS = 2>
struct RingBlockBuffer {
  RingBlockBuffer() {}

  void Push(const T& value) {
    blocks[(LENGTH * currentBlock + size++) % BUFFER_SIZE] = value;
  }

  T* PopBlock() {
    T* result = &blocks[LENGTH * currentBlock];
    currentBlock = (currentBlock + 1) % BLOCKS;
    size -= LENGTH;
    return result;
  }

  T* Data() {
    return &blocks[LENGTH * currentBlock];
  }

  T& Back() {
    if (size == 0) {
      return blocks[LENGTH * currentBlock];
    }
    return blocks[(LENGTH * currentBlock + size - 1) % BUFFER_SIZE];
  }

  const size_t& Size() {
    return size;
  }

  constexpr size_t BlockByteSize() {
    return BLOCK_SIZE;
  }

  constexpr size_t CountPerBlock() {
    return LENGTH;
  }

private:
  static constexpr size_t BUFFER_SIZE = LENGTH * BLOCKS;
  static constexpr size_t BLOCK_SIZE = LENGTH * sizeof(T);

  T blocks[BUFFER_SIZE] = {};
  size_t currentBlock = 0;
  size_t size = 0;
};

template <typename T, size_t LENGTH>
using StereoRingBuffer = RingBuffer<StereoSample<T>, LENGTH>;

template <typename T, size_t LENGTH, size_t BLOCKS = 2>
using StereoRingBlockBuffer = RingBlockBuffer<StereoSample<T>, LENGTH, BLOCKS>;

} // namespace nba
