#include "osvault/deadlock_detector.hpp"
#include <algorithm>
#include <sstream>

namespace osvault {

static std::string format_vec(const std::vector<int>& v) {
    std::ostringstream ss;
    ss << "[";
    for (size_t i = 0; i < v.size(); ++i) {
        if (i) ss << ", ";
        ss << v[i];
    }
    ss << "]";
    return ss.str();
}

Result<DeadlockState> DeadlockDetector::analyze(
    const std::vector<std::string>& processes,
    const std::vector<std::string>& resources,
    const std::vector<int>& available,
    const std::vector<std::vector<int>>& allocation,
    const std::vector<std::vector<int>>& request) {

    size_t np = processes.size();
    size_t nr = resources.size();

    if (np == 0 || nr == 0) {
        return Result<DeadlockState>::failure("Processes and resources must not be empty.", 400);
    }
    if (available.size() != nr) {
        return Result<DeadlockState>::failure("Available vector size does not match resource types count.", 400);
    }
    if (allocation.size() != np || request.size() != np) {
        return Result<DeadlockState>::failure("Matrix row dimensions do not match process count.", 400);
    }
    for (size_t i = 0; i < np; ++i) {
        if (allocation[i].size() != nr || request[i].size() != nr) {
            return Result<DeadlockState>::failure("Matrix column dimensions do not match resource count for process " + processes[i], 400);
        }
    }

    DeadlockState state;
    state.processes = processes;
    state.resource_types = resources;
    state.available = available;
    state.allocation = allocation;
    state.request = request;

    std::vector<int> work = available;
    std::vector<bool> finish(np, false);
    std::vector<std::string> seq;
    std::vector<std::string> logs;

    logs.push_back("Initial Available Work vector: " + format_vec(work));

    // Banker's safety algorithm loop
    bool progress = true;
    while (progress) {
        progress = false;
        for (size_t i = 0; i < np; ++i) {
            if (!finish[i]) {
                // Check if request[i] <= work
                bool can_satisfy = true;
                for (size_t r = 0; r < nr; ++r) {
                    if (request[i][r] > work[r]) {
                        can_satisfy = false;
                        break;
                    }
                }

                if (can_satisfy) {
                    // Process can complete and release its allocated resources
                    for (size_t r = 0; r < nr; ++r) {
                        work[r] += allocation[i][r];
                    }
                    finish[i] = true;
                    seq.push_back(processes[i]);
                    progress = true;

                    logs.push_back("Process " + processes[i] + " can satisfy request " +
                                   format_vec(request[i]) + " with Work. Process finishes and releases " +
                                   format_vec(allocation[i]) + ". New Work: " + format_vec(work));
                }
            }
        }
    }

    std::vector<std::string> deadlocked;
    for (size_t i = 0; i < np; ++i) {
        if (!finish[i]) {
            deadlocked.push_back(processes[i]);
        }
    }

    if (deadlocked.empty()) {
        state.is_safe = true;
        state.is_deadlock = false;
        state.safe_sequence = seq;
        logs.push_back("SAFE STATE confirmed: All processes finished successfully.");
    } else {
        state.is_safe = false;
        state.is_deadlock = true;
        state.deadlocked_processes = deadlocked;
        std::ostringstream ss;
        ss << "DEADLOCK DETECTED among processes: ";
        for (size_t i = 0; i < deadlocked.size(); ++i) {
            if (i) ss << ", ";
            ss << deadlocked[i];
        }
        logs.push_back(ss.str());
    }

    state.log_steps = std::move(logs);
    return Result<DeadlockState>::ok(std::move(state));
}

Result<DeadlockState> DeadlockDetector::simulate_recovery(
    DeadlockState state, const std::string& process_to_terminate) {

    auto it = std::find(state.processes.begin(), state.processes.end(), process_to_terminate);
    if (it == state.processes.end()) {
        return Result<DeadlockState>::failure("Process to terminate not found: " + process_to_terminate, 404);
    }

    size_t idx = std::distance(state.processes.begin(), it);
    size_t nr = state.resource_types.size();

    // Release allocated resources of terminated process into available
    for (size_t r = 0; r < nr; ++r) {
        state.available[r] += state.allocation[idx][r];
    }

    // Remove process from state
    state.processes.erase(state.processes.begin() + idx);
    state.allocation.erase(state.allocation.begin() + idx);
    state.request.erase(state.request.begin() + idx);

    // Re-analyze
    auto re_res = analyze(state.processes, state.resource_types, state.available, state.allocation, state.request);
    if (re_res.is_error()) return re_res;

    auto result = re_res.value();
    result.log_steps.insert(result.log_steps.begin(),
        "Simulated recovery: Terminated simulated process '" + process_to_terminate + "' and released its resources.");
    return Result<DeadlockState>::ok(std::move(result));
}

DeadlockState DeadlockDetector::create_preset_safe() {
    std::vector<std::string> procs = {"P0", "P1", "P2", "P3", "P4"};
    std::vector<std::string> res = {"A", "B", "C"};
    std::vector<int> avail = {3, 3, 2};
    std::vector<std::vector<int>> alloc = {
        {0, 1, 0},
        {2, 0, 0},
        {3, 0, 2},
        {2, 1, 1},
        {0, 0, 2}
    };
    std::vector<std::vector<int>> req = {
        {0, 0, 0},
        {2, 0, 2},
        {0, 0, 0},
        {1, 0, 0},
        {0, 0, 2}
    };

    auto res_calc = analyze(procs, res, avail, alloc, req);
    return res_calc.is_ok() ? res_calc.value() : DeadlockState{};
}

DeadlockState DeadlockDetector::create_preset_deadlock() {
    std::vector<std::string> procs = {"P0", "P1", "P2", "P3"};
    std::vector<std::string> res = {"R1", "R2", "R3"};
    std::vector<int> avail = {0, 0, 0};
    std::vector<std::vector<int>> alloc = {
        {1, 0, 0},
        {0, 1, 0},
        {0, 0, 1},
        {1, 1, 0}
    };
    std::vector<std::vector<int>> req = {
        {0, 1, 0},
        {0, 0, 1},
        {1, 0, 0},
        {0, 0, 1}
    };

    auto res_calc = analyze(procs, res, avail, alloc, req);
    return res_calc.is_ok() ? res_calc.value() : DeadlockState{};
}

} // namespace osvault
