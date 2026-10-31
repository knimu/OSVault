#pragma once
#include <string>
#include <vector>
#include "osvault/models.hpp"
#include "osvault/result.hpp"

namespace osvault {

class DeadlockDetector {
public:
    static Result<DeadlockState> analyze(
        const std::vector<std::string>& processes,
        const std::vector<std::string>& resources,
        const std::vector<int>& available,
        const std::vector<std::vector<int>>& allocation,
        const std::vector<std::vector<int>>& request);

    static Result<DeadlockState> simulate_recovery(
        DeadlockState state,
        const std::string& process_to_terminate);

    static DeadlockState create_preset_safe();
    static DeadlockState create_preset_deadlock();
};

} // namespace osvault
