#include "sets.hpp"

// ===================== ConcurrentSet start ===================== //
template <typename T, typename Compare>
ConcurrentSet<T, Compare>::ConcurrentSet(): maxSize_(MAX_MESSAGES_PER_PACKET * SEND_WINDOW_SIZE * MAX_QUEUE_SIZE) {}

template <typename T, typename Compare>
ConcurrentSet<T, Compare>::ConcurrentSet(size_t maxSize): maxSize_(maxSize), set_({}) {}

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
  std::lock_guard<std::mutex> lock(mutex_);
  // Wait until there is space in the set
  // std::unique_lock<std::mutex> lock(mutex_);
  // cv_.wait(lock, [&]() {
  //   return set_.size() < maxSize_;
  // });
  
  // maximum efficiency insertion at the end of the set (we know m_seq is always increasing)
  auto result = set_.insert(set_.end(), value); 
  
  // Notify any waiting threads that a new message has been enqueued
  // lock.unlock();

  return result;
}

template <typename T, typename Compare>
void ConcurrentSet<T, Compare>::erase(const T &value)
{
  // Lock the set while modifying it
  // std::unique_lock<std::mutex> lock(mutex_);
  std::lock_guard<std::mutex> lock(mutex_);
  
  // Remove message from set (correctly delivered at target)
  set_.erase(value);
    
  // Notify the waiting enqueuer thread that space is available in the set
  // lock.unlock();
  // cv_.notify_one();
}

template <typename T, typename Compare>
void ConcurrentSet<T, Compare>::erase(const std::vector<T> &values)
{
  // Lock the set while modifying it
  // std::unique_lock<std::mutex> lock(mutex_);
  std::lock_guard<std::mutex> lock(mutex_);
  
  // Remove message from set (correctly delivered at target)
  for (T value: values) {
    set_.erase(value);
  }
  
  // Notify the waiting enqueuer thread that space is available in the set
  // lock.unlock();
  // cv_.notify_one();
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
template class ConcurrentSet<std::tuple<msg_seq_t, proc_id_t, msg_seq_t>, TupleFirstElementComparator>;


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
      popConsecutiveFront();
    }
  }

  return insertion_result;
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
