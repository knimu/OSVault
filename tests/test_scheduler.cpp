#include "doctest.h"
#include "osvault/scheduler.hpp"

using namespace osvault;

TEST_SUITE("scheduler") {
    TEST_CASE("Priority scheduling with lower number as higher priority") {
        std::vector<ProcessTask> tasks = {
            {"P1", 0, 4, 3},
            {"P2", 1, 3, 1},
            {"P3", 2, 2, 2}
        };

        auto res = ProcessScheduler::run_priority_non_preemptive(tasks);
        CHECK(res.is_ok());
        const auto& r = res.value();

        CHECK(r.processes[0].pid == "P1");
        CHECK(r.processes[0].completion_time == 4);
        CHECK(r.processes[1].pid == "P2");
        CHECK(r.processes[1].completion_time == 7);
        CHECK(r.processes[2].pid == "P3");
        CHECK(r.processes[2].completion_time == 9);
        CHECK(r.avg_waiting_time == doctest::Approx(8.0 / 3.0));
    }

    TEST_CASE("FCFS calculation and Gantt chart") {
        std::vector<ProcessTask> tasks = {
            {"P1", 0, 4, 1},
            {"P2", 1, 3, 2},
            {"P3", 2, 1, 3}
        };

        auto res = ProcessScheduler::run_fcfs(tasks);
        CHECK(res.is_ok());
        const auto& r = res.value();

        // P1 runs 0-4: CT=4, TAT=4, WT=0, RT=0
        // P2 runs 4-7: CT=7, TAT=6, WT=3, RT=3
        // P3 runs 7-8: CT=8, TAT=6, WT=5, RT=5
        CHECK(r.processes[0].completion_time == 4);
        CHECK(r.processes[1].completion_time == 7);
        CHECK(r.processes[2].completion_time == 8);

        CHECK(r.processes[0].waiting_time == 0);
        CHECK(r.processes[1].waiting_time == 3);
        CHECK(r.processes[2].waiting_time == 5);

        CHECK(r.avg_waiting_time == doctest::Approx(8.0 / 3.0));
        CHECK(r.idle_time == 0);
    }

    TEST_CASE("FCFS with idle interval") {
        std::vector<ProcessTask> tasks = {
            {"P1", 0, 2, 1},
            {"P2", 5, 2, 1}
        };

        auto res = ProcessScheduler::run_fcfs(tasks);
        CHECK(res.is_ok());
        const auto& r = res.value();

        CHECK(r.gantt_chart.size() == 3); // P1, IDLE (2-5), P2
        CHECK(r.gantt_chart[1].is_idle == true);
        CHECK(r.gantt_chart[1].start_time == 2);
        CHECK(r.gantt_chart[1].end_time == 5);
        CHECK(r.idle_time == 3);
    }

    TEST_CASE("SJF non-preemptive") {
        std::vector<ProcessTask> tasks = {
            {"P1", 0, 7, 1},
            {"P2", 2, 4, 1},
            {"P3", 4, 1, 1},
            {"P4", 5, 4, 1}
        };

        auto res = ProcessScheduler::run_sjf_non_preemptive(tasks);
        CHECK(res.is_ok());
        const auto& r = res.value();

        // At t=0, only P1 is available -> runs 0 to 7.
        // At t=7, P2 (burst 4), P3 (burst 1), P4 (burst 4) have arrived.
        // P3 has shortest burst (1) -> runs 7 to 8.
        // Then P2 (burst 4, arrived 2) -> runs 8 to 12.
        // Then P4 (burst 4, arrived 5) -> runs 12 to 16.
        CHECK(r.processes[0].pid == "P1");
        CHECK(r.processes[0].completion_time == 7);

        CHECK(r.processes[2].pid == "P3");
        CHECK(r.processes[2].completion_time == 8);

        CHECK(r.processes[1].pid == "P2");
        CHECK(r.processes[1].completion_time == 12);

        CHECK(r.processes[3].pid == "P4");
        CHECK(r.processes[3].completion_time == 16);
    }

    TEST_CASE("Round Robin with quantum 2") {
        std::vector<ProcessTask> tasks = {
            {"P1", 0, 5, 1},
            {"P2", 1, 3, 1},
            {"P3", 2, 1, 1}
        };

        auto res = ProcessScheduler::run_round_robin(tasks, 2);
        CHECK(res.is_ok());
        const auto& r = res.value();

        // Execution trace:
        // t=0-2: P1 (rem 3), arrives P2 (t=1), P3 (t=2)
        // ready queue: [P2, P3, P1]
        // t=2-4: P2 (rem 1)
        // ready queue: [P3, P1, P2]
        // t=4-5: P3 (finishes CT=5)
        // ready queue: [P1, P2]
        // t=5-7: P1 (rem 1)
        // ready queue: [P2, P1]
        // t=7-8: P2 (finishes CT=8)
        // ready queue: [P1]
        // t=8-9: P1 (finishes CT=9)
        CHECK(r.processes[0].pid == "P1");
        CHECK(r.processes[0].completion_time == 9);

        CHECK(r.processes[1].pid == "P2");
        CHECK(r.processes[1].completion_time == 8);

        CHECK(r.processes[2].pid == "P3");
        CHECK(r.processes[2].completion_time == 5);
    }

    TEST_CASE("Validation of invalid inputs") {
        CHECK(ProcessScheduler::validate_tasks({}).is_error());
        CHECK(ProcessScheduler::validate_tasks({{"P1", -1, 5, 0}}).is_error());
        CHECK(ProcessScheduler::validate_tasks({{"P1", 0, 0, 0}}).is_error());
        CHECK(ProcessScheduler::validate_tasks({{"P1", 0, 1, -1}}).is_error());
        CHECK(ProcessScheduler::validate_tasks({{"P1", 0, 1, 1}, {"P1", 1, 2, 2}}).is_error());
        CHECK(ProcessScheduler::run_round_robin({{"P1", 0, 5, 0}}, 0).is_error());
    }
}
