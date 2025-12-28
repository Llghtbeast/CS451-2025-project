#pragma once

#include <cstdint>
#include <map>
#include <vector>
#include <mutex>
#include <functional>
#include <condition_variable>
#include <set> // for template instantiation

#include "globals.hpp"
#include "deque.hpp"

// Concurrent map wrapper around std::map
template <typename Key, typename Value, typename Compare = std::less<Key>>
class ConcurrentMap {
public:
  using key_type = Key;
  using mapped_type = Value;
  using value_type = std::pair<const Key, Value>;
  using map_type = std::map<Key, Value, Compare>;
  using iterator = typename map_type::iterator;

  ConcurrentMap();
  ConcurrentMap(bool bounded);
  ~ConcurrentMap() = default;

  // Capacity methods
  bool empty() const;
  std::size_t size() const;

  // Modifiers
  std::pair<iterator, bool> insert(const Key &key, const Value &value);
  void erase(const Key &key);
  void erase(const std::vector<Key> &keys);
  void erase(const std::array<Key, MAX_MESSAGES_PER_PACKET>& keys);

  std::pair<std::array<value_type, MAX_CONTAINER_SIZE>, size_t> complete(ConcurrentDeque<std::pair<Key, Value>>& queue);

  // Convenience helpers for when Value is a container (eg std::set<proc_id_t>):
  // Insert a member into the mapped container. If key does not exist, create it.
  // Returns true if the member was inserted (was not present).
  template <typename Member>
  bool add_to_mapped_set(const Key &key, const Member &member);

  // Get size of mapped container for a key (0 if not present).
  size_t mapped_set_size(const Key &key) const;

  // Return a copy of the mapped container for a key (empty if not present).
  Value get_mapped_copy(const Key &key) const;
  
  // Lookup
  iterator find(const Key &key);
  bool contains(const Key &key) const;
  std::vector<value_type> snapshot() const;

public:
  static constexpr size_t max_size = MAX_CONTAINER_SIZE;

private:
  bool bounded_;
  map_type map_;
  mutable std::mutex mutex_;
};