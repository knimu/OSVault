#pragma once
#include <cstddef>
#include <deque>
#include <list>
#include <string>
#include <unordered_map>
#include <vector>
#include "osvault/models.hpp"
#include "osvault/result.hpp"

namespace osvault {

class CacheManager {
public:
    explicit CacheManager(size_t capacity = 16, std::string policy = "LRU");

    // In-memory data cache
    void set_policy(const std::string& policy);
    std::string policy() const noexcept { return policy_; }

    void set_capacity(size_t capacity);
    size_t capacity() const noexcept { return capacity_; }

    bool get(const std::string& key, std::string& value_out);
    void put(const std::string& key, const std::string& value);
    void invalidate(const std::string& key);
    void clear();

    size_t hits() const noexcept { return hits_; }
    size_t misses() const noexcept { return misses_; }
    size_t evictions() const noexcept { return evictions_; }
    size_t size() const noexcept { return cache_map_.size(); }
    double hit_ratio() const noexcept;

    // Educational Page-Replacement Simulator
    static Result<PageReplacementResult> simulate_fifo(const std::vector<int>& sequence, int frame_count, int page_size = 4096);
    static Result<PageReplacementResult> simulate_lru(const std::vector<int>& sequence, int frame_count, int page_size = 4096);
    static Result<std::pair<PageReplacementResult, PageReplacementResult>> simulate_comparison(
        const std::vector<int>& sequence, int frame_count, int page_size = 4096);

private:
    size_t capacity_{16};
    std::string policy_{"LRU"};
    size_t hits_{0};
    size_t misses_{0};
    size_t evictions_{0};

    // For LRU / FIFO tracking
    std::list<std::pair<std::string, std::string>> items_list_;
    std::unordered_map<std::string, std::list<std::pair<std::string, std::string>>::iterator> cache_map_;
};

} // namespace osvault
