#pragma once

#include <stddef.h>

template <typename T, size_t Capacity>
class RingBuffer {
 public:
  bool push(const T& value) {
    if (Capacity == 0) {
      return false;
    }

    if (size_ < Capacity) {
      data_[tail_] = value;
      tail_ = (tail_ + 1) % Capacity;
      ++size_;
      return true;
    }

    data_[tail_] = value;
    tail_ = (tail_ + 1) % Capacity;
    head_ = (head_ + 1) % Capacity;
    return false;
  }

  bool pop(T& out) {
    if (size_ == 0) {
      return false;
    }
    out = data_[head_];
    head_ = (head_ + 1) % Capacity;
    --size_;
    return true;
  }

  bool peek(T& out) const {
    if (size_ == 0) {
      return false;
    }
    out = data_[head_];
    return true;
  }

  size_t copyTo(T* out, size_t maxCount) const {
    const size_t count = (size_ < maxCount) ? size_ : maxCount;
    for (size_t i = 0; i < count; ++i) {
      out[i] = data_[(head_ + i) % Capacity];
    }
    return count;
  }

  size_t size() const { return size_; }
  T& operator[](size_t index) { return data_[(head_ + index) % Capacity]; }
  const T& operator[](size_t index) const { return data_[(head_ + index) % Capacity]; }
  static constexpr size_t capacity() { return Capacity; }
  bool empty() const { return size_ == 0; }
  void clear() {
    head_ = 0;
    tail_ = 0;
    size_ = 0;
  }

 private:
  T data_[Capacity]{};
  size_t head_ = 0;
  size_t tail_ = 0;
  size_t size_ = 0;
};
