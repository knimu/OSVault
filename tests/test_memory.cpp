#include "doctest.h"
#include "osvault/memory_manager.hpp"

using namespace osvault;

TEST_SUITE("memory") {
    TEST_CASE("Duplicate and invalid allocations are rejected") {
        MemoryManager mm(1024);

        CHECK(mm.allocate_first_fit("P1", 128).is_ok());
        CHECK(mm.allocate_first_fit("P1", 64).code() == 409);
        CHECK(mm.allocate_first_fit("", 64).code() == 400);
        CHECK(mm.allocate_first_fit("P2", 0).code() == 400);
        CHECK(mm.allocate_first_fit("P2", 2048).code() == 422);
    }

    TEST_CASE("First Fit, Best Fit, and Worst Fit selection") {
        // Initial custom partitions: 100, 500, 200, 300, 600
        MemoryManager mm;
        mm.reset_with_custom_partitions({100, 500, 200, 300, 600});

        // Request 212 KB
        // First Fit should choose 500 (index 1)
        auto ff = mm.allocate_first_fit("Proc_FF", 212);
        CHECK(ff.is_ok());
        CHECK(ff.value().partitions[1].process_name == "Proc_FF");
        CHECK(ff.value().partitions[1].size == 212);

        // Reset and test Best Fit
        mm.reset_with_custom_partitions({100, 500, 200, 300, 600});
        // Best fit for 212 should choose 300 (smallest >= 212)
        auto bf = mm.allocate_best_fit("Proc_BF", 212);
        CHECK(bf.is_ok());
        // 300 was at index 3
        CHECK(bf.value().partitions[3].process_name == "Proc_BF");
        CHECK(bf.value().partitions[3].size == 212);

        // Reset and test Worst Fit
        mm.reset_with_custom_partitions({100, 500, 200, 300, 600});
        // Worst fit for 212 should choose 600 (largest hole)
        auto wf = mm.allocate_worst_fit("Proc_WF", 212);
        CHECK(wf.is_ok());
        // 600 was at index 4
        CHECK(wf.value().partitions[4].process_name == "Proc_WF");
        CHECK(wf.value().partitions[4].size == 212);
    }

    TEST_CASE("Deallocation and free block coalescing") {
        MemoryManager mm(1000);
        auto a1 = mm.allocate_first_fit("P1", 200);
        auto a2 = mm.allocate_first_fit("P2", 300);
        auto a3 = mm.allocate_first_fit("P3", 200);

        // State: [P1:200, P2:300, P3:200, Free:300]
        CHECK(mm.get_state().partitions.size() == 4);

        // Deallocate P2
        auto d2 = mm.deallocate_by_process("P2");
        CHECK(d2.is_ok());
        // State: [P1:200, Free:300, P3:200, Free:300]
        CHECK(mm.get_state().partitions.size() == 4);

        // Deallocate P3 -> adjacent Free:300 and Free:300 merge into Free:800!
        auto d3 = mm.deallocate_by_process("P3");
        CHECK(d3.is_ok());
        // State: [P1:200, Free:800]
        CHECK(mm.get_state().partitions.size() == 2);
        CHECK(mm.get_state().partitions[1].size == 800);
    }

    TEST_CASE("External fragmentation reflects separate free holes") {
        MemoryManager mm;
        mm.reset_with_custom_partitions({100, 500, 200, 300});

        CHECK(mm.allocate_first_fit("P1", 300).is_ok());
        auto state = mm.get_state();
        CHECK(state.allocated_memory == 300);
        CHECK(state.free_memory == 800);
        CHECK(state.external_fragmentation == 500);
    }
}
