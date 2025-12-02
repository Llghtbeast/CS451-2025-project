#pragma once

#include <cstdint>
#include <deque>
#include <vector>
#include <mutex>
#include <condition_variable>

#include "globals.hpp"

// Concurrent deque
template <typename T>
class ConcurrentDeque {
public:
  using value_type = T;

  ConcurrentDeque();
  ConcurrentDeque(size_t maxSize);
  ~ConcurrentDeque() = default;

  // Capacity methods
  bool empty() const;
  std::size_t size() const;

  // Modifiers
  void push_back(const T& value);
  T pop_front();
  std::vector<T> pop_k_front(size_t k);
  void clear();

  // Lookup
  T front() const;
  T back() const;
  std::vector<T> snapshot() const;

private:
  size_t maxSize_;
  std::deque<T> deque_;
  mutable std::mutex mutex_;
  std::condition_variable cv_;
};