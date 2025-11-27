#pragma once

#include <cstdint>
#include <set>
#include <vector>
#include <mutex>
#include <functional>

#include "globals.hpp"

template <typename T, typename Compare = std::less<T>>
class ConcurrentSet {
public:
    using value_type = T;

    ConcurrentSet() = default;
    ~ConcurrentSet() = default;

    void insert(const T& value);
    std::size_t size() const;
    std::vector<T> pop_consecutive_from(T start);
    std::size_t remove_consecutive_from(T &next_expected);
    std::vector<T> snapshot() const;

private:
    mutable std::mutex mutex_;
    std::set<T, Compare> set_;
};