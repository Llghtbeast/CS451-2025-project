#include "sets.hpp"

// ===================== ConcurrentSet start ===================== //
template <typename T, typename Compare>
ConcurrentSet<T, Compare>::ConcurrentSet(): bounded_(false), maxSize_(0), set_({}) {}

template <typename T, typename Compare>
ConcurrentSet<T, Compare>::ConcurrentSet(bool bounded): bounded_(bounded), set_({}) 
{
  // initialize maxSize to default if set is bounded
  if (bounded) {
    maxSize_ = MAX_MESSAGES_PER_PACKET * SEND_WINDOW_SIZE * MAX_CONTAINER_SIZE;
  }
  else {
    maxSize_ = 0;
  }
}

// Capacity methods
template <typename T, typename Compare>
bool ConcurrentSet<T, Compare>::empty()
{
  std::lock_guard<std::mutex> g(mutex_);
  return set_.empty();
}

template <typename T, typename Compare>
std::size_t ConcurrentSet<T, Compare>::size() const
{
  std::lock_guard<std::mutex> g(mutex_);
  return set_.size();
}

// Modifiers
template <typename T, typename Compare>
typename std::set<T, Compare>::iterator ConcurrentSet<T, Compare>::insert(const T &value)
{
  // assert that this method is only used when the concurrent set is NOT bounded
  assert(!bounded_);
  std::lock_guard<std::mutex> lock(mutex_);
  
  // maximum efficiency insertion at the end of the set (we know m_seq is always increasing)
  auto result = set_.insert(set_.end(), value); 
  
  return result;
}

template <typename T, typename Compare>
void ConcurrentSet<T, Compare>::erase(const T &value)
{
  // Lock the set while modifying it
  std::lock_guard<std::mutex> lock(mutex_);
  
  // Remove message from set (correctly delivered at target)
  set_.erase(value);
}

template <typename T, typename Compare>
void ConcurrentSet<T, Compare>::erase(const std::vector<T> &values)
{
  // Lock the set while modifying it
  std::lock_guard<std::mutex> lock(mutex_);
  
  // Remove message from set (correctly delivered at target)
  for (T value: values) {
    set_.erase(value);
  }
}

template <typename T, typename Compare>
std::vector<T> ConcurrentSet<T, Compare>::complete(ConcurrentDeque<T>& queue)
{
  // assert that this method is only used when the concurrent set is bounded
  assert(bounded_);
  std::lock_guard<std::mutex> lock(mutex_);

  // get size of set
  size_t set_size = set_.size();

  // insert k first elements of concurrent queue into set_
  for (T& value: queue.pop_k_front(maxSize_ - set_size))
  {
    set_.insert(set_.end(), value); 
  }
  
  // Return snapshot of the concurrent set
  return std::vector<T>(set_.begin(), set_.end());
}

// Lookup
template <typename T, typename Compare>
typename std::set<T, Compare>::iterator ConcurrentSet<T, Compare>::find(const T &value)
{
  std::lock_guard<std::mutex> g(mutex_);
  return set_.find(value);
}

template <typename T, typename Compare>
bool ConcurrentSet<T, Compare>::contains(const T &value)
{
  std::lock_guard<std::mutex> g(mutex_);
  return set_.find(value) != set_.end();
}

template <typename T, typename Compare>
std::vector<T> ConcurrentSet<T, Compare>::snapshot() const
{
  std::lock_guard<std::mutex> g(mutex_);
  return std::vector<T>(set_.begin(), set_.end());
}
// ===================== ConcurrentSet end ===================== //

// Explicit template instantiation for msg_seq_t
template class ConcurrentSet<msg_seq_t>;


// ===================== SlidingSet start ===================== //
template <typename T, typename Compare>
SlidingSet<T, Compare>::SlidingSet(): set_({INITIAL_SLIDING_SET_PREFIX}) {}

template <typename T, typename Compare>
SlidingSet<T, Compare>::SlidingSet(T first_prefix): set_({first_prefix}) {}

// Debug methods
template <typename T, typename Compare>
void SlidingSet<T, Compare>::display() const
{
  std::cout << "Sliding set: 0 <= _ < " << *set_.begin();
  for (T value: set_) {
    std::cout << ", " << value;
  }
  std::cout << "" << std::endl;
}

// Capacity methods
template <typename T, typename Compare>
size_t SlidingSet<T, Compare>:: size() const
{
  return set_.size();
}

// Modifiers
template <typename T, typename Compare>
std::pair<typename std::set<T, Compare>::iterator, bool> SlidingSet<T, Compare>::insert(const T &value)
{
  std::pair<typename std::set<T, Compare>::iterator, bool> result;
  if (!contains(value)) {
    result = set_.insert(value);
    popConsecutiveFront();
  }
  return result;
}

template <typename T, typename Compare>
std::vector<bool> SlidingSet<T, Compare>::insert(const std::vector<T> &values)
{
  std::vector<bool> insertion_result;
  std::set<msg_seq_t>::iterator it = set_.begin();

  for (T value : values) {
    // return true if element not in set
    insertion_result.push_back(!contains(value));
    if (!contains(value)) {
      set_.insert(value);
    }
  }
  popConsecutiveFront();

  return insertion_result;
}

template <typename T, typename Compare>
std::array<bool, MAX_MESSAGES_PER_PACKET> SlidingSet<T, Compare>::insert(
  const std::array<T, MAX_MESSAGES_PER_PACKET> &values, 
  uint8_t count)
{
  std::array<bool, MAX_MESSAGES_PER_PACKET> insertion_results; 
  
  for (size_t i = 0; i < count; i++)
  {
    insertion_results[i] = !contains(values[i]);
    if (!contains(values[i]))
    {
      set_.insert(values[i]);
    }
  }
  popConsecutiveFront();
  
  return insertion_results;
}

template <typename T, typename Compare>
void SlidingSet<T, Compare>::popConsecutiveFront()
{
  auto it = set_.begin();

  // Remove leading consecutive messages to avoid unbounded growth
  while (it != set_.end()) {
    if (*it + 1 != *(++it)) {
      break;
    }
  }
  set_.erase(set_.begin(), --it);
}

// Lookup
template <typename T, typename Compare>
bool SlidingSet<T, Compare>::contains(const T &value)
{
  if (value < *set_.begin()) return true;
  else return set_.find(value) != set_.end();
}
// ===================== SlidingSet end ===================== //
template class SlidingSet<msg_seq_t>;
// template class SlidingSet<std::tuple<proc_id_t, msg_seq_t>>;
