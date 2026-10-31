#pragma once
#include <cstddef>
#include <string>
#include <vector>
#include "osvault/models.hpp"
#include "osvault/result.hpp"

namespace osvault {

class MemoryManager {
public:
    explicit MemoryManager(size_t total_size = 1024);

    void reset(size_t total_size = 1024);
    void reset_with_custom_partitions(const std::vector<size_t>& partition_sizes);

    Result<MemoryAllocationResult> allocate_first_fit(const std::string& process_name, size_t size);
    Result<MemoryAllocationResult> allocate_best_fit(const std::string& process_name, size_t size);
    Result<MemoryAllocationResult> allocate_worst_fit(const std::string& process_name, size_t size);

    Result<MemoryAllocationResult> deallocate(int partition_id);
    Result<MemoryAllocationResult> deallocate_by_process(const std::string& process_name);

    MemoryAllocationResult get_state(const std::string& strategy = "Current") const;

    size_t total_size() const noexcept { return total_size_; }

private:
    void coalesce_free_partitions();
    Result<MemoryAllocationResult> perform_allocation(const std::string& strategy, const std::string& process_name, size_t size, int chosen_index);

    size_t total_size_;
    std::vector<MemoryPartition> partitions_;
    int next_partition_id_{1};
};

} // namespace osvault
