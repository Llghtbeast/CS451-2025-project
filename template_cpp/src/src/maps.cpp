#include "maps.hpp"

// ===================== ConcurrentMap start ===================== //
template <typename Key, typename Value, typename Compare>
ConcurrentMap<Key, Value, Compare>::ConcurrentMap()
  : bounded_(false), maxSize_(0), map_({})
{}

template <typename Key, typename Value, typename Compare>
ConcurrentMap<Key, Value, Compare>::ConcurrentMap(bool bounded)
  : bounded_(bounded), maxSize_(0), map_({})
{
  if (bounded) {
    maxSize_ = MAX_MESSAGES_PER_PACKET * SEND_WINDOW_SIZE * MAX_CONTAINER_SIZE;
  }
  else {
    maxSize_ = 0;
  }
}

// Capacity methods
template <typename Key, typename Value, typename Compare>
bool ConcurrentMap<Key, Value, Compare>::empty() const
{
  std::lock_guard<std::mutex> g(mutex_);
  return map_.empty();
}

template <typename Key, typename Value, typename Compare>
std::size_t ConcurrentMap<Key, Value, Compare>::size() const
{
  std::lock_guard<std::mutex> g(mutex_);
  return map_.size();
}

// Modifiers
template <typename Key, typename Value, typename Compare>
std::pair<typename ConcurrentMap<Key, Value, Compare>::iterator, bool>
ConcurrentMap<Key, Value, Compare>::insert(const Key &key, const Value &value)
{
  assert(!bounded_);
  std::lock_guard<std::mutex> g(mutex_);

  // Insert only if key not present
  return map_.emplace(key, value);
}

template <typename Key, typename Value, typename Compare>
void ConcurrentMap<Key, Value, Compare>::erase(const Key &key)
{
  std::lock_guard<std::mutex> g(mutex_);
  map_.erase(key);
}

template <typename Key, typename Value, typename Compare>
void ConcurrentMap<Key, Value, Compare>::erase(const std::vector<Key> &keys)
{
  std::lock_guard<std::mutex> g(mutex_);
  for (const Key &k : keys) {
    map_.erase(k);
  }
}

template <typename Key, typename Value, typename Compare>
void ConcurrentMap<Key, Value, Compare>::erase(const std::array<Key, MAX_MESSAGES_PER_PACKET> &keys)
{
  std::lock_guard<std::mutex> g(mutex_);
  for (const Key &key: keys)
  {
    map_.erase(key);
  }
}

template <typename Key, typename Value, typename Compare>
std::vector<std::pair<const Key, Value>> ConcurrentMap<Key, Value, Compare>::complete(ConcurrentDeque<std::pair<Key, Value>> &queue)
{
  assert(bounded_);
  std::lock_guard<std::mutex> lock(mutex_);

  // get size of map
  size_t map_size = map_.size();

  // insert k first elements of concurrent queue into map
  for (const auto& [key, value]: queue.pop_k_front(maxSize_ - map_size))
  {
    map_.emplace(key, value); 
  }

  // return snapshot of map
  std::vector<value_type> result;
  result.reserve(map_.size());
  for (const auto &p : map_) {
    result.push_back(p);
  }
  return result;
}

// Helpers for container-like Value
template <typename Key, typename Value, typename Compare>
template <typename Member>
bool ConcurrentMap<Key, Value, Compare>::add_to_mapped_set(const Key &key, const Member &member) {
  std::lock_guard<std::mutex> g(mutex_);
  auto it = map_.find(key);
  if (it == map_.end()) {
    auto p = map_.emplace(key, Value{}).first;
    it = p;
  }
  // Value is expected to be a container that supports insert(Member) -> pair<iterator,bool>
  auto pr = it->second.insert(member);
  return pr.second;
}

template <typename Key, typename Value, typename Compare>
size_t ConcurrentMap<Key, Value, Compare>::mapped_set_size(const Key &key) const {
  std::lock_guard<std::mutex> g(mutex_);
  auto it = map_.find(key);
  if (it == map_.end()) return 0;
  return it->second.size();
}

template <typename Key, typename Value, typename Compare>
Value ConcurrentMap<Key, Value, Compare>::get_mapped_copy(const Key &key) const {
  std::lock_guard<std::mutex> g(mutex_);
  auto it = map_.find(key);
  if (it == map_.end()) return Value{};
  return it->second;
}

// Lookup
template <typename Key, typename Value, typename Compare>
typename ConcurrentMap<Key, Value, Compare>::iterator
ConcurrentMap<Key, Value, Compare>::find(const Key &key)
{
  std::lock_guard<std::mutex> g(mutex_);
  return map_.find(key);
}

template <typename Key, typename Value, typename Compare>
bool ConcurrentMap<Key, Value, Compare>::contains(const Key &key) const
{
  std::lock_guard<std::mutex> g(mutex_);
  return map_.find(key) != map_.end();
}

template <typename Key, typename Value, typename Compare>
std::vector<typename ConcurrentMap<Key, Value, Compare>::value_type>
ConcurrentMap<Key, Value, Compare>::snapshot() const
{
  std::lock_guard<std::mutex> g(mutex_);
  std::vector<value_type> s;
  s.reserve(map_.size());
  for (const auto &p : map_) {
    s.push_back(p);
  }
  return s;
}
// ===================== ConcurrentMap end ===================== //

// Explicit instantiation for ConcurrentMap<msg_seq_t, std::set<proc_id_t>>
template class ConcurrentMap<msg_seq_t, std::set<proc_id_t>>;
template bool ConcurrentMap<msg_seq_t, std::set<proc_id_t>>::add_to_mapped_set<proc_id_t>(const msg_seq_t&, const proc_id_t&);

// Explicit instantiation for ConcurrentMap<pkt_seq_t, Message>
// Only instantiate the methods actually used for this type
template ConcurrentMap<pkt_seq_t, Message>::ConcurrentMap(bool);
template bool ConcurrentMap<pkt_seq_t, Message>::empty() const;
template std::vector<std::pair<const pkt_seq_t, Message>> 
  ConcurrentMap<pkt_seq_t, Message>::complete(ConcurrentDeque<std::pair<pkt_seq_t, Message>>&);
template void ConcurrentMap<pkt_seq_t, Message>::erase(const std::array<pkt_seq_t, MAX_MESSAGES_PER_PACKET>&);
