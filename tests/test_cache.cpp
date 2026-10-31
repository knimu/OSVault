#include "doctest.h"
#include "osvault/cache_manager.hpp"

using namespace osvault;

TEST_SUITE("cache") {
    TEST_CASE("FIFO textbook page replacement validation") {
        // Standard textbook sequence: 7, 0, 1, 2, 0, 3, 0, 4, 2, 3, 0, 3, 2, 1, 2, 0, 1, 7, 0, 1
        std::vector<int> seq = {7, 0, 1, 2, 0, 3, 0, 4, 2, 3, 0, 3, 2, 1, 2, 0, 1, 7, 0, 1};
        int frames = 3;

        auto res = CacheManager::simulate_fifo(seq, frames);
        CHECK(res.is_ok());
        const auto& r = res.value();

        CHECK(r.total_hits == 5);
        CHECK(r.total_misses == 15);
        CHECK(r.steps.size() == seq.size());
    }

    TEST_CASE("LRU textbook page replacement validation") {
        // Standard textbook sequence: 7, 0, 1, 2, 0, 3, 0, 4, 2, 3, 0, 3, 2, 1, 2, 0, 1, 7, 0, 1
        std::vector<int> seq = {7, 0, 1, 2, 0, 3, 0, 4, 2, 3, 0, 3, 2, 1, 2, 0, 1, 7, 0, 1};
        int frames = 3;

        auto res = CacheManager::simulate_lru(seq, frames);
        CHECK(res.is_ok());
        const auto& r = res.value();

        CHECK(r.total_hits == 8);
        CHECK(r.total_misses == 12);
        CHECK(r.steps.size() == seq.size());
    }

    TEST_CASE("Edge cases and validation") {
        // Empty sequence
        CHECK(CacheManager::simulate_fifo({}, 3).is_error());
        CHECK(CacheManager::simulate_lru({}, 3).is_error());

        // Invalid frame count <= 0
        CHECK(CacheManager::simulate_fifo({1, 2}, 0).is_error());
        CHECK(CacheManager::simulate_lru({1, 2}, -1).is_error());

        // Comparison mode
        auto cmp = CacheManager::simulate_comparison({1, 2, 1, 3}, 2);
        CHECK(cmp.is_ok());
        CHECK(cmp.value().first.algorithm == "FIFO");
        CHECK(cmp.value().second.algorithm == "LRU");
    }

    TEST_CASE("In-memory cache operations") {
        CacheManager cm(2, "LRU");
        cm.put("a", "alpha");
        cm.put("b", "beta");

        std::string val;
        CHECK(cm.get("a", val));
        CHECK(val == "alpha");
        CHECK(cm.hits() == 1);

        // Put "c" triggers eviction of "b" (since "a" was accessed most recently)
        cm.put("c", "gamma");
        CHECK(cm.evictions() == 1);
        CHECK_FALSE(cm.get("b", val));
        CHECK(cm.misses() == 1);
        CHECK(cm.get("a", val));
        CHECK(cm.get("c", val));
    }
}
