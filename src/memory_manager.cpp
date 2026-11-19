#include "osvault/memory_manager.hpp"
#include <algorithm>

namespace osvault {

MemoryManager::MemoryManager(size_t total_size) {
    reset(total_size);
}

void MemoryManager::reset(size_t total_size) {
    total_size_ = total_size > 0 ? total_size : 1024;
    partitions_.clear();
    next_partition_id_ = 1;

    MemoryPartition initial;
    initial.id = next_partition_id_++;
    initial.start_address = 0;
    initial.size = total_size_;
    initial.is_allocated = false;
    initial.process_name = "";
    initial.internal_fragmentation = 0;
    partitions_.push_back(initial);
}

void MemoryManager::reset_with_custom_partitions(const std::vector<size_t>& partition_sizes) {
    partitions_.clear();
    next_partition_id_ = 1;
    size_t current_addr = 0;

    for (size_t sz : partition_sizes) {
        if (sz == 0) continue;
        MemoryPartition p;
        p.id = next_partition_id_++;
        p.start_address = current_addr;
        p.size = sz;
        p.is_allocated = false;
        p.process_name = "";
        p.internal_fragmentation = 0;
        partitions_.push_back(p);
        current_addr += sz;
    }

    total_size_ = current_addr;
}

void MemoryManager::coalesce_free_partitions() {
    if (partitions_.size() < 2) return;

    std::vector<MemoryPartition> merged;
    merged.reserve(partitions_.size());

    for (const auto& p : partitions_) {
        if (!merged.empty() && !merged.back().is_allocated && !p.is_allocated) {
            // Merge with previous free partition
            merged.back().size += p.size;
        } else {
            merged.push_back(p);
        }
    }

    partitions_ = std::move(merged);
}

MemoryAllocationResult MemoryManager::get_state(const std::string& strategy) const {
    MemoryAllocationResult res;
    res.strategy = strategy;
    res.partitions = partitions_;
    res.total_memory = total_size_;
    res.allocated_memory = 0;
    res.free_memory = 0;
    res.total_internal_fragmentation = 0;

    size_t largest_free_block = 0;
    for (const auto& p : partitions_) {
        if (p.is_allocated) {
            res.allocated_memory += p.size;
            res.total_internal_fragmentation += p.internal_fragmentation;
        } else {
            res.free_memory += p.size;
            largest_free_block = std::max(largest_free_block, p.size);
        }
    }

    // External fragmentation exists if free memory is split across multiple smaller blocks
    res.external_fragmentation = (res.free_memory > largest_free_block) ? (res.free_memory - largest_free_block) : 0;
    res.success = true;
    res.message = "Current memory state retrieved.";
    return res;
}

Result<MemoryAllocationResult> MemoryManager::perform_allocation(
    const std::string& strategy, const std::string& process_name, size_t size, int chosen_index) {
    if (process_name.empty()) {
        return Result<MemoryAllocationResult>::failure("Process name cannot be empty.", 400);
    }
    for (const auto& partition : partitions_) {
        if (partition.is_allocated && partition.process_name == process_name) {
            return Result<MemoryAllocationResult>::failure("Process '" + process_name + "' already owns allocated memory.", 409);
        }
    }
    if (chosen_index < 0 || chosen_index >= static_cast<int>(partitions_.size())) {
        auto fail_state = get_state(strategy);
        fail_state.success = false;
        fail_state.message = "Allocation failed: No suitable free partition found for " + std::to_string(size) + " KB.";
        return Result<MemoryAllocationResult>::failure(fail_state.message, 422);
    }

    auto& target = partitions_[chosen_index];
    size_t leftover = target.size - size;

    if (leftover > 0) {
        // Split partition
        MemoryPartition allocated_part;
        allocated_part.id = next_partition_id_++;
        allocated_part.start_address = target.start_address;
        allocated_part.size = size;
        allocated_part.is_allocated = true;
        allocated_part.process_name = process_name;
        allocated_part.internal_fragmentation = 0;

        target.start_address += size;
        target.size = leftover;

        partitions_.insert(partitions_.begin() + chosen_index, allocated_part);
    } else {
        target.is_allocated = true;
        target.process_name = process_name;
        target.internal_fragmentation = 0;
    }

    auto state = get_state(strategy);
    state.success = true;
    state.message = "Allocated " + std::to_string(size) + " KB for " + process_name + " using " + strategy + ".";
    return Result<MemoryAllocationResult>::ok(std::move(state));
}

Result<MemoryAllocationResult> MemoryManager::allocate_first_fit(const std::string& process_name, size_t size) {
    if (size == 0) return Result<MemoryAllocationResult>::failure("Allocation size must be > 0.", 400);

    int chosen = -1;
    for (size_t i = 0; i < partitions_.size(); ++i) {
        if (!partitions_[i].is_allocated && partitions_[i].size >= size) {
            chosen = static_cast<int>(i);
            break;
        }
    }
    return perform_allocation("First Fit", process_name, size, chosen);
}

Result<MemoryAllocationResult> MemoryManager::allocate_best_fit(const std::string& process_name, size_t size) {
    if (size == 0) return Result<MemoryAllocationResult>::failure("Allocation size must be > 0.", 400);

    int chosen = -1;
    size_t min_diff = ~0ULL;

    for (size_t i = 0; i < partitions_.size(); ++i) {
        if (!partitions_[i].is_allocated && partitions_[i].size >= size) {
            size_t diff = partitions_[i].size - size;
            if (diff < min_diff) {
                min_diff = diff;
                chosen = static_cast<int>(i);
            }
        }
    }
    return perform_allocation("Best Fit", process_name, size, chosen);
}

Result<MemoryAllocationResult> MemoryManager::allocate_worst_fit(const std::string& process_name, size_t size) {
    if (size == 0) return Result<MemoryAllocationResult>::failure("Allocation size must be > 0.", 400);

    int chosen = -1;
    size_t max_diff = 0;
    bool found = false;

    for (size_t i = 0; i < partitions_.size(); ++i) {
        if (!partitions_[i].is_allocated && partitions_[i].size >= size) {
            size_t diff = partitions_[i].size - size;
            if (!found || diff > max_diff) {
                max_diff = diff;
                chosen = static_cast<int>(i);
                found = true;
            }
        }
    }
    return perform_allocation("Worst Fit", process_name, size, chosen);
}

Result<MemoryAllocationResult> MemoryManager::deallocate(int partition_id) {
    bool found = false;
    for (auto& p : partitions_) {
        if (p.id == partition_id && p.is_allocated) {
            p.is_allocated = false;
            p.process_name = "";
            p.internal_fragmentation = 0;
            found = true;
            break;
        }
    }

    if (!found) {
        return Result<MemoryAllocationResult>::failure("Allocated partition not found with ID: " + std::to_string(partition_id), 404);
    }

    coalesce_free_partitions();
    auto state = get_state("Deallocation");
    state.message = "Partition " + std::to_string(partition_id) + " freed and merged.";
    return Result<MemoryAllocationResult>::ok(std::move(state));
}

Result<MemoryAllocationResult> MemoryManager::deallocate_by_process(const std::string& process_name) {
    bool found = false;
    for (auto& p : partitions_) {
        if (p.is_allocated && p.process_name == process_name) {
            p.is_allocated = false;
            p.process_name = "";
            p.internal_fragmentation = 0;
            found = true;
        }
    }

    if (!found) {
        return Result<MemoryAllocationResult>::failure("No allocated partitions found for process: " + process_name, 404);
    }

    coalesce_free_partitions();
    auto state = get_state("Deallocation");
    state.message = "Partitions for process '" + process_name + "' freed and merged.";
    return Result<MemoryAllocationResult>::ok(std::move(state));
}

} // namespace osvault
