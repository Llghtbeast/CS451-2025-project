#pragma once

#include <cstdint>
#include <set>
#include <vector>
#include <mutex>
#include <functional>
#include <stdbool.h>
#include <condition_variable>
#include <iostream>

#include "globals.hpp"

// Concurrent set
template <typename T, typename Compare = std::less<T>>
class ConcurrentSet {
public:
  using value_type = T;

  ConcurrentSet(size_t maxSize);
  ~ConcurrentSet() = default;

  // Capacity methods
  bool empty();
  std::size_t size() const;

  // Modifiers
  typename std::set<T, Compare>::iterator insert(const T& value);
  void erase(const std::vector<T>& values);
  void erase(const T& value);
  
  // Lookup
  typename std::set<T, Compare>::iterator find(const T &value);
  bool contains(const T &value);
  std::vector<T> snapshot() const;

private:
  size_t maxSize_;
  std::set<T, Compare> set_;
  mutable std::mutex mutex_;
  std::condition_variable cv_;
};

// Sliding set that continuously trims the starting consecutive elements to avoid unbounded growth
template <typename T, typename Compare = std::less<T>>
class SlidingSet {
public:
  using value_type = T;

  SlidingSet(T first_prefix);
  ~SlidingSet() = default;

  // Debug methods
  void display() const;

  // Capacity methods
  std::size_t size() const;

  // Modifiers
  std::pair<typename std::set<T, Compare>::iterator, bool> insert(const T& value);
  std::vector<bool> insert(const std::vector<T> &values);
private:
  void popConsecutiveFront();
  
public:
  // Lookup
  bool contains(const T &value);

private:
  std::set<T, Compare> set_;
};