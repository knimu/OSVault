#include "doctest.h"
#include "osvault/synchronization.hpp"

using namespace osvault;

TEST_SUITE("synchronization") {
    TEST_CASE("Peterson's Critical Section Mutual Exclusion") {
        SynchronizationSimulator sim;
        auto s0 = sim.get_cs_state();
        CHECK(s0.current_in_cs == -1);

        // Thread 0 requests entry
        auto s1 = sim.step_cs(0);
        CHECK(s1.is_ok());
        CHECK(s1.value().current_in_cs == 0);

        // Thread 1 requests entry while Thread 0 is in CS -> Thread 1 must be WAITING
        auto s2 = sim.step_cs(1);
        CHECK(s2.is_ok());
        CHECK(s2.value().thread_states[1] == "WAITING");
        CHECK(s2.value().current_in_cs == 0);

        // Thread 0 exits CS -> Thread 1 proceeds into CS
        auto s3 = sim.step_cs(0);
        CHECK(s3.is_ok());
        CHECK(s3.value().thread_states[0] == "REMAINDER");
        CHECK(s3.value().current_in_cs == 1);
        CHECK(s3.value().thread_states[1] == "CRITICAL");
    }

    TEST_CASE("Producer-Consumer bounded buffer") {
        SynchronizationSimulator sim;
        sim.reset_pc(2); // Capacity 2

        // Produce 1
        CHECK(sim.step_produce(10).is_ok());
        CHECK(sim.get_pc_state().full_slots == 1);

        // Produce 2
        CHECK(sim.step_produce(20).is_ok());
        CHECK(sim.get_pc_state().full_slots == 2);
        CHECK(sim.get_pc_state().empty_slots == 0);

        // Produce 3 -> Buffer full, blocked
        auto p3 = sim.step_produce(30);
        CHECK(p3.is_ok());
        CHECK(p3.value().producer_state.find("BLOCKED") != std::string::npos);

        // Consume 1 -> retrieves 10
        auto c1 = sim.step_consume();
        CHECK(c1.is_ok());
        CHECK(c1.value().full_slots == 1);

        // Consume 2 -> retrieves 20
        auto c2 = sim.step_consume();
        CHECK(c2.is_ok());
        CHECK(c2.value().full_slots == 0);

        // Consume 3 -> Buffer empty, blocked
        auto c3 = sim.step_consume();
        CHECK(c3.is_ok());
        CHECK(c3.value().consumer_state.find("BLOCKED") != std::string::npos);
    }

    TEST_CASE("Readers-Writers mutual exclusion") {
        SynchronizationSimulator sim;

        // Multiple readers can read simultaneously
        CHECK(sim.start_read("R1").is_ok());
        CHECK(sim.start_read("R2").is_ok());
        CHECK(sim.get_rw_state().reader_count == 2);

        // Writer cannot write while readers are active
        CHECK(sim.start_write("W1").is_ok());
        CHECK(sim.get_rw_state().writer_active == false);
        CHECK(sim.get_rw_state().waiting_writers == 1);

        // Readers exit
        CHECK(sim.stop_read("R1").is_ok());
        CHECK(sim.stop_read("R2").is_ok());
        CHECK(sim.get_rw_state().reader_count == 0);

        // Now writer can start
        CHECK(sim.start_write("W1").is_ok());
        CHECK(sim.get_rw_state().writer_active == true);

        // Reader cannot read while writer is active
        CHECK(sim.start_read("R3").is_ok());
        CHECK(sim.get_rw_state().reader_count == 0);
        CHECK(sim.get_rw_state().waiting_readers == 1);

        // Writer finishes
        CHECK(sim.stop_write("W1", 500).is_ok());
        CHECK(sim.get_rw_state().writer_active == false);
        CHECK(sim.get_rw_state().shared_resource_val == 500);
    }

    TEST_CASE("Dining Philosophers state transitions") {
        SynchronizationSimulator sim;
        // Philosopher 0 thinking -> hungry -> eating
        auto p0_1 = sim.step_philosopher(0);
        CHECK(p0_1.is_ok());
        CHECK(p0_1.value().states[0] == "EATING");

        // Neighboring philosopher 1 cannot eat because fork 1 is held by Philosopher 0
        auto p1_1 = sim.step_philosopher(1);
        CHECK(p1_1.is_ok());
        CHECK(p1_1.value().states[1] == "HUNGRY"); // Fork not available

        // Philosopher 0 finishes eating
        auto p0_2 = sim.step_philosopher(0);
        CHECK(p0_2.is_ok());
        CHECK(p0_2.value().states[0] == "THINKING");

        // Now philosopher 1 can eat
        auto p1_2 = sim.step_philosopher(1);
        CHECK(p1_2.is_ok());
        CHECK(p1_2.value().states[1] == "EATING");
    }
}
