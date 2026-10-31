#include "doctest.h"
#include "osvault/logical_clock.hpp"

using namespace osvault;

TEST_SUITE("clocks") {
    TEST_CASE("Lamport and Vector clock event updates") {
        LogicalClockSystem sys(3);

        // Node_0 local event
        auto ev0 = sys.trigger_local_event("Node_0", "Task 1");
        CHECK(ev0.is_ok());
        CHECK(ev0.value().lamport_clock == 1);
        CHECK(ev0.value().vector_clock == std::vector<int>{1, 0, 0});

        // Node_0 sends message to Node_1
        auto msg_pair = sys.send_message("Node_0", "Node_1", "Ping");
        CHECK(msg_pair.is_ok());

        // Send event on Node_0
        CHECK(msg_pair.value().first.lamport_clock == 2);
        CHECK(msg_pair.value().first.vector_clock == std::vector<int>{2, 0, 0});

        // Receive event on Node_1: clock must be > send clock
        CHECK(msg_pair.value().second.lamport_clock == 3);
        CHECK(msg_pair.value().second.vector_clock == std::vector<int>{2, 1, 0});
    }

    TEST_CASE("Causality and concurrency detection") {
        std::vector<int> v1 = {1, 0, 0};
        std::vector<int> v2 = {2, 1, 0};
        std::vector<int> v3 = {0, 2, 0};

        // v1 happened before v2
        CHECK(LogicalClockSystem::is_causally_dependent(v1, v2) == true);
        CHECK(LogicalClockSystem::is_causally_dependent(v2, v1) == false);

        // v1 and v3 are concurrent
        CHECK(LogicalClockSystem::is_concurrent(v1, v3) == true);
    }
}
