#include "concurrent_set.hpp"

template <typename T, typename Compare>
void ConcurrentSet<T, Compare>::insert(const T& value) {
    std::lock_guard<std::mutex> g(mutex_);
    set_.insert(value);
}

template <typename T, typename Compare>
std::size_t ConcurrentSet<T, Compare>::size() const {
    std::lock_guard<std::mutex> g(mutex_);
    return set_.size();
}

template <typename T, typename Compare>
std::vector<T> ConcurrentSet<T, Compare>::pop_consecutive_from(T start) {
    std::lock_guard<std::mutex> g(mutex_);
    std::vector<T> removed;
    auto it = set_.find(start);
    while (it != set_.end() && *it == start) {
        removed.push_back(*it);
        it = set_.erase(it);
        ++start;
    }
    return removed;
}

template <typename T, typename Compare>
std::size_t ConcurrentSet<T, Compare>::remove_consecutive_from(T &next_expected) {
    std::lock_guard<std::mutex> g(mutex_);
    std::size_t removed = 0;
    while (true) {
        auto it = set_.find(next_expected);
        if (it == set_.end() || *it != next_expected) break;
        set_.erase(it);
        ++next_expected;
        ++removed;
    }
    return removed;
}

template <typename T, typename Compare>
std::vector<T> ConcurrentSet<T, Compare>::snapshot() const {
    std::lock_guard<std::mutex> g(mutex_);
    return std::vector<T>(set_.begin(), set_.end());
}

// Explicitly instantiate the template for msg_seq_t so consumers can link
// without needing the method definitions in scope.
template class ConcurrentSet<msg_seq_t>;