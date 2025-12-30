#include "deque.hpp"

// ===================== ConcurrentDeque start ===================== //
// Capacity methods
template <typename T>
bool ConcurrentDeque<T>::empty() const
{
  std::lock_guard<std::mutex> g(mutex_);
  return deque_.empty();
}

template <typename T>
std::size_t ConcurrentDeque<T>::size() const
{
  std::lock_guard<std::mutex> g(mutex_);
  return deque_.size();
}

// Modifiers
template <typename T>
void ConcurrentDeque<T>::push_back(const T& value)
{
  std::lock_guard<std::mutex> lock(mutex_);
  
  // Optional: Wait until there is space in the deque
  // std::unique_lock<std::mutex> lock(mutex_);
  // cv_.wait(lock, [&]() {
  //   return deque_.size() < maxSize_;
  // });
  
  deque_.push_back(value);
  
  // Notify any waiting threads that a new element has been added
  // lock.unlock();
  // cv_.notify_one();
}

template <typename T>
T ConcurrentDeque<T>::pop_front()
{
  std::lock_guard<std::mutex> lock(mutex_);
  
  // Optional: Wait until deque is non-empty
  // std::unique_lock<std::mutex> lock(mutex_);
  // cv_.wait(lock, [&]() {
  //   return !deque_.empty();
  // });
  
  
  T value = deque_.front();
  deque_.pop_front();
  
  // Notify waiting threads that space is available
  // lock.unlock();
  // cv_.notify_one();
  
  return value;
}

template <typename T>
std::vector<T> ConcurrentDeque<T>::pop_k_front(size_t k)
{
  std::lock_guard<std::mutex> lock(mutex_);

  size_t count = std::min(k, deque_.size());

  // advance the deque iterator by count
  auto end = deque_.begin();
  std::advance(end, count); 

  // Create output
  std::vector<T> output(
      std::make_move_iterator(deque_.begin()),
      std::make_move_iterator(end)
  );

  // Erase the moved elements from the beginning of the deque.
  deque_.erase(deque_.begin(), end);
  
  return output;
}

template <typename T>
void ConcurrentDeque<T>::clear()
{
  std::lock_guard<std::mutex> lock(mutex_);
  deque_.clear();
  
  // Notify all waiting threads
  // cv_.notify_all();
}

// Lookup
template <typename T>
T ConcurrentDeque<T>::front() const
{
  std::lock_guard<std::mutex> g(mutex_);
  return deque_.front();
}

template <typename T>
T ConcurrentDeque<T>::back() const
{
  std::lock_guard<std::mutex> g(mutex_);
  return deque_.back();
}

template <typename T>
std::vector<T> ConcurrentDeque<T>::snapshot() const
{
  std::lock_guard<std::mutex> g(mutex_);
  return std::vector<T>(deque_.begin(), deque_.end());
}
// ===================== ConcurrentDeque end ===================== //

// Explicit template instantiation
template class ConcurrentDeque<std::pair<pkt_seq_t, std::shared_ptr<Message>>>;
template class ConcurrentDeque<std::pair<prop_nb_t, std::set<proposal_t>>>;
template class ConcurrentDeque<std::pair<uint32_t, std::set<proc_id_t>>>;