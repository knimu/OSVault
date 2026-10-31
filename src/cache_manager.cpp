#include "osvault/cache_manager.hpp"
#include <algorithm>
#include <unordered_set>

namespace osvault {

CacheManager::CacheManager(size_t capacity, std::string policy)
    : capacity_(capacity), policy_(std::move(policy)) {}

void CacheManager::set_policy(const std::string& policy) {
    if (policy == "LRU" || policy == "FIFO") {
        policy_ = policy;
        clear();
    }
}

void CacheManager::set_capacity(size_t capacity) {
    capacity_ = std::max<size_t>(1, capacity);
    while (items_list_.size() > capacity_) {
        cache_map_.erase(items_list_.back().first);
        items_list_.pop_back();
        ++evictions_;
    }
}

void CacheManager::clear() {
    items_list_.clear();
    cache_map_.clear();
    hits_ = 0;
    misses_ = 0;
    evictions_ = 0;
}

double CacheManager::hit_ratio() const noexcept {
    size_t total = hits_ + misses_;
    return total > 0 ? (static_cast<double>(hits_) / total) * 100.0 : 0.0;
}

bool CacheManager::get(const std::string& key, std::string& value_out) {
    auto it = cache_map_.find(key);
    if (it != cache_map_.end()) {
        ++hits_;
        value_out = it->second->second;
        if (policy_ == "LRU") {
            // Move accessed item to the front
            items_list_.splice(items_list_.begin(), items_list_, it->second);
        }
        return true;
    }
    ++misses_;
    return false;
}

void CacheManager::put(const std::string& key, const std::string& value) {
    auto it = cache_map_.find(key);
    if (it != cache_map_.end()) {
        it->second->second = value;
        if (policy_ == "LRU") {
            items_list_.splice(items_list_.begin(), items_list_, it->second);
        }
        return;
    }

    if (items_list_.size() >= capacity_) {
        // Evict item from back
        auto last = items_list_.end();
        --last;
        cache_map_.erase(last->first);
        items_list_.pop_back();
        ++evictions_;
    }

    items_list_.emplace_front(key, value);
    cache_map_[key] = items_list_.begin();
}

void CacheManager::invalidate(const std::string& key) {
    auto it = cache_map_.find(key);
    if (it != cache_map_.end()) {
        items_list_.erase(it->second);
        cache_map_.erase(it);
    }
}

Result<PageReplacementResult> CacheManager::simulate_fifo(const std::vector<int>& sequence, int frame_count) {
    if (frame_count <= 0) {
        return Result<PageReplacementResult>::failure("Frame count must be greater than 0.", 400);
    }
    if (sequence.empty()) {
        return Result<PageReplacementResult>::failure("Reference sequence cannot be empty.", 400);
    }

    PageReplacementResult res;
    res.algorithm = "FIFO";
    res.sequence = sequence;
    res.frame_count = frame_count;

    std::vector<int> frames;
    frames.reserve(frame_count);
    size_t fifo_index = 0;

    for (int page : sequence) {
        PageStep step;
        step.page = page;

        auto it = std::find(frames.begin(), frames.end(), page);
        if (it != frames.end()) {
            step.hit = true;
            step.evicted_page = -1;
            ++res.total_hits;
        } else {
            step.hit = false;
            ++res.total_misses;

            if (static_cast<int>(frames.size()) < frame_count) {
                frames.push_back(page);
                step.evicted_page = -1;
            } else {
                step.evicted_page = frames[fifo_index];
                frames[fifo_index] = page;
                fifo_index = (fifo_index + 1) % frame_count;
            }
        }

        step.frames = frames;
        res.steps.push_back(step);
    }

    int total = res.total_hits + res.total_misses;
    res.hit_ratio = total > 0 ? (static_cast<double>(res.total_hits) / total) * 100.0 : 0.0;
    res.miss_ratio = total > 0 ? (static_cast<double>(res.total_misses) / total) * 100.0 : 0.0;
    res.final_frames = frames;

    return Result<PageReplacementResult>::ok(std::move(res));
}

Result<PageReplacementResult> CacheManager::simulate_lru(const std::vector<int>& sequence, int frame_count) {
    if (frame_count <= 0) {
        return Result<PageReplacementResult>::failure("Frame count must be greater than 0.", 400);
    }
    if (sequence.empty()) {
        return Result<PageReplacementResult>::failure("Reference sequence cannot be empty.", 400);
    }

    PageReplacementResult res;
    res.algorithm = "LRU";
    res.sequence = sequence;
    res.frame_count = frame_count;

    std::vector<int> frames;
    frames.reserve(frame_count);
    // Tracks last used time for each page
    std::unordered_map<int, int> last_used;

    for (int time = 0; time < static_cast<int>(sequence.size()); ++time) {
        int page = sequence[time];
        PageStep step;
        step.page = page;

        auto it = std::find(frames.begin(), frames.end(), page);
        if (it != frames.end()) {
            step.hit = true;
            step.evicted_page = -1;
            ++res.total_hits;
        } else {
            step.hit = false;
            ++res.total_misses;

            if (static_cast<int>(frames.size()) < frame_count) {
                frames.push_back(page);
                step.evicted_page = -1;
            } else {
                // Find least recently used page in frames
                int lru_index = 0;
                int min_time = last_used[frames[0]];
                for (size_t i = 1; i < frames.size(); ++i) {
                    if (last_used[frames[i]] < min_time) {
                        min_time = last_used[frames[i]];
                        lru_index = static_cast<int>(i);
                    }
                }
                step.evicted_page = frames[lru_index];
                frames[lru_index] = page;
            }
        }

        last_used[page] = time;
        step.frames = frames;
        res.steps.push_back(step);
    }

    int total = res.total_hits + res.total_misses;
    res.hit_ratio = total > 0 ? (static_cast<double>(res.total_hits) / total) * 100.0 : 0.0;
    res.miss_ratio = total > 0 ? (static_cast<double>(res.total_misses) / total) * 100.0 : 0.0;
    res.final_frames = frames;

    return Result<PageReplacementResult>::ok(std::move(res));
}

Result<std::pair<PageReplacementResult, PageReplacementResult>> CacheManager::simulate_comparison(
    const std::vector<int>& sequence, int frame_count) {
    auto fifo_res = simulate_fifo(sequence, frame_count);
    if (fifo_res.is_error()) return Result<std::pair<PageReplacementResult, PageReplacementResult>>::failure(fifo_res.error());

    auto lru_res = simulate_lru(sequence, frame_count);
    if (lru_res.is_error()) return Result<std::pair<PageReplacementResult, PageReplacementResult>>::failure(lru_res.error());

    return Result<std::pair<PageReplacementResult, PageReplacementResult>>::ok({fifo_res.value(), lru_res.value()});
}

} // namespace osvault
