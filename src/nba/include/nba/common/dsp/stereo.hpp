/*
 * Copyright (C) 2024 fleroviux
 *
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#pragma once

#include <memory>
#include <nba/common/dsp/stream.hpp>
#include <stdexcept>

namespace nba {

template<typename T>
struct StereoSample {
  union {
    struct {
      T left;
      T right;
    };
    T channels[2] = {};
  };

  StereoSample(const T& l = T{}, const T& r = T{}) : left(l), right(r) {}
  
  template <typename U>
  operator StereoSample<U>() {
    return { (U)left, (U)right };
  }
  
  T& operator[](int index) {
    return channels[index];
  }

  const T& operator[](int index) const {
    return channels[index];
  }
  
  StereoSample<T> operator+(T scalar) const {
    return { left + scalar, right + scalar };
  }
  
  StereoSample<T> operator+(StereoSample<T> const& other) const {
    return { left + other.left, right + other.right };
  }
  
  StereoSample<T>& operator+=(T scalar) {
    left  += scalar;
    right += scalar;
    return *this;
  }
  
  StereoSample<T>& operator+=(StereoSample<T> const& other) {
    left  += other.left;
    right += other.right;
    return *this;
  }
  
  StereoSample<T> operator-(T scalar) const {
    return { left - scalar, right - scalar };
  }
  
  StereoSample<T> operator-(StereoSample<T> const& other) const {
    return { left - other.left, right - other.right };
  }
  
  StereoSample<T>& operator-=(T scalar) {
    left  -= scalar;
    right -= scalar;
    return *this;
  }
  
  StereoSample<T>& operator-=(StereoSample<T> const& other) {
    left  -= other.left;
    right -= other.right;
    return *this;
  }
  
  StereoSample<T> operator*(T scalar) const {
    return { left * scalar, right * scalar };
  }
  
  StereoSample<T> operator*(StereoSample<T> const& other) const {
    return { left * other.left, right * other.right };
  }
  
  StereoSample<T>& operator*=(T scalar) {
    left  *= scalar;
    right *= scalar;
    return *this;
  }
  
  StereoSample<T>& operator*=(StereoSample<T> const& other) {
    left  *= other.left;
    right *= other.right;
    return *this;
  }
};
  
} // namespace nba
