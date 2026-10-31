#pragma once
#include <vector>
#include "osvault/models.hpp"
#include "osvault/result.hpp"

namespace osvault {

class ProcessScheduler {
public:
    static Result<SchedulingResult> run_fcfs(std::vector<ProcessTask> tasks);
    static Result<SchedulingResult> run_sjf_non_preemptive(std::vector<ProcessTask> tasks);
    static Result<SchedulingResult> run_round_robin(std::vector<ProcessTask> tasks, int time_quantum);
    static Result<SchedulingResult> run_priority_non_preemptive(std::vector<ProcessTask> tasks);

    static Result<std::vector<SchedulingResult>> run_comparison(std::vector<ProcessTask> tasks, int time_quantum);

    static Result<void> validate_tasks(const std::vector<ProcessTask>& tasks);
};

} // namespace osvault
