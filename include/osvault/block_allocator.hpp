#pragma once
#include <cstddef>
#include <vector>
#include "osvault/database.hpp"
#include "osvault/models.hpp"
#include "osvault/result.hpp"

namespace osvault {

class BlockAllocator {
public:
    explicit BlockAllocator(Database& db, size_t total_blocks = 1024);

    Result<std::vector<int>> allocate_blocks(size_t count, int64_t file_id);
    Result<void> free_blocks_for_file(int64_t file_id);
    Result<void> free_specific_blocks(const std::vector<int>& block_ids);

    Result<std::vector<BlockInfo>> get_all_blocks();
    Result<BlockInfo> get_block(int block_id);

    size_t total_blocks() const noexcept { return total_blocks_; }
    Result<size_t> get_free_block_count();
    Result<size_t> get_allocated_block_count();

private:
    Database& db_;
    size_t total_blocks_;
};

} // namespace osvault
