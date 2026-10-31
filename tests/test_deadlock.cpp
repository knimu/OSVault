#include "doctest.h"
#include "osvault/deadlock_detector.hpp"

using namespace osvault;

TEST_SUITE("deadlock") {
    TEST_CASE("Banker's Algorithm safe state detection") {
        auto state = DeadlockDetector::create_preset_safe();
        CHECK(state.is_safe == true);
        CHECK(state.is_deadlock == false);
        CHECK(state.safe_sequence.size() == 5);
        CHECK(state.deadlocked_processes.empty());
    }

    TEST_CASE("Deadlock state detection and identification") {
        auto state = DeadlockDetector::create_preset_deadlock();
        CHECK(state.is_deadlock == true);
        CHECK(state.is_safe == false);
        CHECK(state.safe_sequence.empty());
        CHECK_FALSE(state.deadlocked_processes.empty());
    }

    TEST_CASE("Simulated recovery terminates process and recovers safe state") {
        auto dl_state = DeadlockDetector::create_preset_deadlock();
        CHECK(dl_state.is_deadlock == true);

        // Terminate P0 in deadlock
        auto rec = DeadlockDetector::simulate_recovery(dl_state, "P0");
        CHECK(rec.is_ok());
        CHECK(rec.value().processes.size() == 3);
    }
}
