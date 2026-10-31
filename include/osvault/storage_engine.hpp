#pragma once
#include <cstddef>
#include <string>
#include <vector>
#include "osvault/block_allocator.hpp"
#include "osvault/database.hpp"
#include "osvault/models.hpp"
#include "osvault/result.hpp"

namespace osvault {

class StorageEngine {
public:
    StorageEngine(Database& db, BlockAllocator& allocator, size_t block_size = 4096);

    Result<FileRecord> create_file(const std::string& name, const std::string& content = "");
    Result<std::string> read_file(const std::string& name);
    Result<FileRecord> get_file_metadata(const std::string& name);
    Result<FileRecord> get_file_metadata_by_id(int64_t file_id);
    Result<void> write_file(const std::string& name, const std::string& new_content);
    Result<void> rename_file(const std::string& old_name, const std::string& new_name);
    Result<void> delete_file(const std::string& name);
    Result<std::vector<FileRecord>> list_files(const std::string& search_query = "");

    Result<SystemMetrics> get_system_metrics();

    size_t block_size() const noexcept { return block_size_; }
    size_t total_capacity_bytes() const noexcept { return block_size_ * allocator_.total_blocks(); }

    static bool is_valid_filename(const std::string& name);
    static size_t calculate_blocks_needed(size_t byte_count, size_t block_size);

private:
    Result<std::vector<int>> get_file_block_ids(int64_t file_id);

    Database& db_;
    BlockAllocator& allocator_;
    size_t block_size_;
};

} // namespace osvault
