#include "osvault/scheduler.hpp"
#include <algorithm>
#include <numeric>
#include <queue>
#include <unordered_map>
#include <unordered_set>

namespace osvault {

Result<void> ProcessScheduler::validate_tasks(const std::vector<ProcessTask>& tasks) {
    if (tasks.empty()) {
        return Result<void>::failure("Process list cannot be empty.", 400);
    }
    std::unordered_set<std::string> seen_pids;
    for (const auto& t : tasks) {
        if (t.pid.empty()) {
            return Result<void>::failure("Process ID cannot be empty.", 400);
        }
        if (!seen_pids.insert(t.pid).second) {
            return Result<void>::failure("Duplicate process ID: " + t.pid, 400);
        }
        if (t.arrival_time < 0) {
            return Result<void>::failure("Arrival time cannot be negative for PID: " + t.pid, 400);
        }
        if (t.burst_time <= 0) {
            return Result<void>::failure("Burst time must be greater than 0 for PID: " + t.pid, 400);
        }
        if (t.priority < 0) {
            return Result<void>::failure("Priority cannot be negative for PID: " + t.pid, 400);
        }
    }
    return Result<void>::ok();
}

static void finalize_metrics(SchedulingResult& res) {
    if (res.processes.empty()) return;
    double sum_tat = 0.0, sum_wt = 0.0, sum_rt = 0.0;
    for (const auto& p : res.processes) {
        sum_tat += p.turnaround_time;
        sum_wt += p.waiting_time;
        sum_rt += p.response_time;
    }
    res.avg_turnaround_time = sum_tat / res.processes.size();
    res.avg_waiting_time = sum_wt / res.processes.size();
    res.avg_response_time = sum_rt / res.processes.size();

    int idle_acc = 0;
    for (const auto& g : res.gantt_chart) {
        if (g.is_idle) {
            idle_acc += (g.end_time - g.start_time);
        }
    }
    res.idle_time = idle_acc;
    if (!res.gantt_chart.empty()) {
        res.total_time = res.gantt_chart.back().end_time;
    }
}

Result<SchedulingResult> ProcessScheduler::run_fcfs(std::vector<ProcessTask> tasks) {
    auto val_res = validate_tasks(tasks);
    if (val_res.is_error()) return Result<SchedulingResult>::failure(val_res.error());

    // Sort by arrival time, tie-breaking by original index/pid
    std::sort(tasks.begin(), tasks.end(), [](const ProcessTask& a, const ProcessTask& b) {
        return a.arrival_time < b.arrival_time;
    });

    SchedulingResult res;
    res.algorithm = "FCFS";

    int current_time = 0;
    for (const auto& t : tasks) {
        if (current_time < t.arrival_time) {
            // Idle interval
            GanttInterval idle_slot{"IDLE", current_time, t.arrival_time, true};
            res.gantt_chart.push_back(idle_slot);
            current_time = t.arrival_time;
        }

        int start = current_time;
        int end = current_time + t.burst_time;
        res.gantt_chart.push_back(GanttInterval{t.pid, start, end, false});

        ProcessResult p;
        p.pid = t.pid;
        p.arrival_time = t.arrival_time;
        p.burst_time = t.burst_time;
        p.priority = t.priority;
        p.completion_time = end;
        p.turnaround_time = p.completion_time - p.arrival_time;
        p.waiting_time = p.turnaround_time - p.burst_time;
        p.response_time = start - p.arrival_time;
        res.processes.push_back(p);

        current_time = end;
    }

    finalize_metrics(res);
    return Result<SchedulingResult>::ok(std::move(res));
}

Result<SchedulingResult> ProcessScheduler::run_sjf_non_preemptive(std::vector<ProcessTask> tasks) {
    auto val_res = validate_tasks(tasks);
    if (val_res.is_error()) return Result<SchedulingResult>::failure(val_res.error());

    SchedulingResult res;
    res.algorithm = "SJF";

    std::vector<bool> finished(tasks.size(), false);
    size_t completed = 0;
    int current_time = 0;

    while (completed < tasks.size()) {
        int best_idx = -1;
        int min_burst = 1e9;

        for (size_t i = 0; i < tasks.size(); ++i) {
            if (!finished[i] && tasks[i].arrival_time <= current_time) {
                if (tasks[i].burst_time < min_burst) {
                    min_burst = tasks[i].burst_time;
                    best_idx = static_cast<int>(i);
                } else if (tasks[i].burst_time == min_burst) {
                    if (best_idx >= 0 && tasks[i].arrival_time < tasks[best_idx].arrival_time) {
                        best_idx = static_cast<int>(i);
                    }
                }
            }
        }

        if (best_idx == -1) {
            // Find next arriving process
            int next_arr = 1e9;
            for (size_t i = 0; i < tasks.size(); ++i) {
                if (!finished[i]) {
                    next_arr = std::min(next_arr, tasks[i].arrival_time);
                }
            }
            res.gantt_chart.push_back(GanttInterval{"IDLE", current_time, next_arr, true});
            current_time = next_arr;
            continue;
        }

        const auto& t = tasks[best_idx];
        int start = current_time;
        int end = current_time + t.burst_time;
        res.gantt_chart.push_back(GanttInterval{t.pid, start, end, false});

        ProcessResult p;
        p.pid = t.pid;
        p.arrival_time = t.arrival_time;
        p.burst_time = t.burst_time;
        p.priority = t.priority;
        p.completion_time = end;
        p.turnaround_time = p.completion_time - p.arrival_time;
        p.waiting_time = p.turnaround_time - p.burst_time;
        p.response_time = start - p.arrival_time;
        res.processes.push_back(p);

        finished[best_idx] = true;
        ++completed;
        current_time = end;
    }

    finalize_metrics(res);
    return Result<SchedulingResult>::ok(std::move(res));
}

Result<SchedulingResult> ProcessScheduler::run_priority_non_preemptive(std::vector<ProcessTask> tasks) {
    auto val_res = validate_tasks(tasks);
    if (val_res.is_error()) return Result<SchedulingResult>::failure(val_res.error());

    SchedulingResult res;
    res.algorithm = "Priority (Non-Preemptive)";

    std::vector<bool> finished(tasks.size(), false);
    size_t completed = 0;
    int current_time = 0;

    while (completed < tasks.size()) {
        int best_idx = -1;
        int highest_pri = 1e9; // Lower number = higher priority

        for (size_t i = 0; i < tasks.size(); ++i) {
            if (!finished[i] && tasks[i].arrival_time <= current_time) {
                if (tasks[i].priority < highest_pri) {
                    highest_pri = tasks[i].priority;
                    best_idx = static_cast<int>(i);
                } else if (tasks[i].priority == highest_pri) {
                    if (best_idx >= 0 && tasks[i].arrival_time < tasks[best_idx].arrival_time) {
                        best_idx = static_cast<int>(i);
                    }
                }
            }
        }

        if (best_idx == -1) {
            int next_arr = 1e9;
            for (size_t i = 0; i < tasks.size(); ++i) {
                if (!finished[i]) {
                    next_arr = std::min(next_arr, tasks[i].arrival_time);
                }
            }
            res.gantt_chart.push_back(GanttInterval{"IDLE", current_time, next_arr, true});
            current_time = next_arr;
            continue;
        }

        const auto& t = tasks[best_idx];
        int start = current_time;
        int end = current_time + t.burst_time;
        res.gantt_chart.push_back(GanttInterval{t.pid, start, end, false});

        ProcessResult p;
        p.pid = t.pid;
        p.arrival_time = t.arrival_time;
        p.burst_time = t.burst_time;
        p.priority = t.priority;
        p.completion_time = end;
        p.turnaround_time = p.completion_time - p.arrival_time;
        p.waiting_time = p.turnaround_time - p.burst_time;
        p.response_time = start - p.arrival_time;
        res.processes.push_back(p);

        finished[best_idx] = true;
        ++completed;
        current_time = end;
    }

    finalize_metrics(res);
    return Result<SchedulingResult>::ok(std::move(res));
}

Result<SchedulingResult> ProcessScheduler::run_round_robin(std::vector<ProcessTask> tasks, int time_quantum) {
    auto val_res = validate_tasks(tasks);
    if (val_res.is_error()) return Result<SchedulingResult>::failure(val_res.error());
    if (time_quantum <= 0) {
        return Result<SchedulingResult>::failure("Time quantum must be greater than 0.", 400);
    }

    SchedulingResult res;
    res.algorithm = "Round Robin";
    res.time_quantum = time_quantum;

    // Track state
    std::unordered_map<std::string, int> remaining_burst;
    std::unordered_map<std::string, int> first_response;
    std::unordered_map<std::string, int> completion_time;
    std::unordered_map<std::string, ProcessTask> task_map;

    for (const auto& t : tasks) {
        remaining_burst[t.pid] = t.burst_time;
        first_response[t.pid] = -1;
        task_map[t.pid] = t;
    }

    // Sort by arrival time
    std::vector<size_t> arrival_order(tasks.size());
    std::iota(arrival_order.begin(), arrival_order.end(), 0);
    std::sort(arrival_order.begin(), arrival_order.end(), [&](size_t a, size_t b) {
        return tasks[a].arrival_time < tasks[b].arrival_time;
    });

    std::queue<std::string> ready_queue;
    std::vector<bool> in_queue_or_done(tasks.size(), false);
    size_t next_arrival_idx = 0;
    int current_time = 0;
    size_t completed_count = 0;

    auto push_arrived = [&](int time_limit) {
        while (next_arrival_idx < arrival_order.size()) {
            size_t idx = arrival_order[next_arrival_idx];
            if (tasks[idx].arrival_time <= time_limit) {
                ready_queue.push(tasks[idx].pid);
                in_queue_or_done[idx] = true;
                ++next_arrival_idx;
            } else {
                break;
            }
        }
    };

    push_arrived(current_time);

    while (completed_count < tasks.size()) {
        if (ready_queue.empty()) {
            // CPU is idle until next process arrives
            if (next_arrival_idx < arrival_order.size()) {
                int next_arr = tasks[arrival_order[next_arrival_idx]].arrival_time;
                res.gantt_chart.push_back(GanttInterval{"IDLE", current_time, next_arr, true});
                current_time = next_arr;
                push_arrived(current_time);
            }
            continue;
        }

        std::string cur_pid = ready_queue.front();
        ready_queue.pop();

        if (first_response[cur_pid] == -1) {
            first_response[cur_pid] = current_time;
        }

        int run_time = std::min(time_quantum, remaining_burst[cur_pid]);
        int start = current_time;
        int end = current_time + run_time;
        res.gantt_chart.push_back(GanttInterval{cur_pid, start, end, false});

        remaining_burst[cur_pid] -= run_time;
        current_time = end;

        // Check for any newly arrived processes during this time slice
        push_arrived(current_time);

        if (remaining_burst[cur_pid] > 0) {
            ready_queue.push(cur_pid);
        } else {
            completion_time[cur_pid] = current_time;
            ++completed_count;
        }
    }

    // Build process results in original order
    for (const auto& t : tasks) {
        ProcessResult p;
        p.pid = t.pid;
        p.arrival_time = t.arrival_time;
        p.burst_time = t.burst_time;
        p.priority = t.priority;
        p.completion_time = completion_time[t.pid];
        p.turnaround_time = p.completion_time - p.arrival_time;
        p.waiting_time = p.turnaround_time - p.burst_time;
        p.response_time = first_response[t.pid] - p.arrival_time;
        res.processes.push_back(p);
    }

    finalize_metrics(res);
    return Result<SchedulingResult>::ok(std::move(res));
}

Result<std::vector<SchedulingResult>> ProcessScheduler::run_comparison(std::vector<ProcessTask> tasks, int time_quantum) {
    std::vector<SchedulingResult> results;

    auto r_fcfs = run_fcfs(tasks);
    if (r_fcfs.is_ok()) results.push_back(r_fcfs.value());

    auto r_sjf = run_sjf_non_preemptive(tasks);
    if (r_sjf.is_ok()) results.push_back(r_sjf.value());

    auto r_prio = run_priority_non_preemptive(tasks);
    if (r_prio.is_ok()) results.push_back(r_prio.value());

    auto r_rr = run_round_robin(tasks, time_quantum > 0 ? time_quantum : 2);
    if (r_rr.is_ok()) results.push_back(r_rr.value());

    return Result<std::vector<SchedulingResult>>::ok(std::move(results));
}

} // namespace osvault
