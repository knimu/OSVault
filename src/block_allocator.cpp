#include "osvault/block_allocator.hpp"
#include <chrono>
#include <iomanip>
#include <sstream>

namespace osvault {

static std::string current_iso_time() {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&in_time_t), "%Y-%m-%dT%H:%M:%SZ");
    return ss.str();
}

BlockAllocator::BlockAllocator(Database& db, size_t total_blocks)
    : db_(db), total_blocks_(total_blocks) {}

void BlockAllocator::set_strategy(const std::string& strategy) {
    if (strategy == "BEST_FIT" || strategy == "WORST_FIT") {
        strategy_ = strategy;
    } else {
        strategy_ = "FIRST_FIT";
    }
}

Result<size_t> BlockAllocator::get_free_block_count() {
    auto stmt_res = db_.prepare("SELECT COUNT(*) FROM block_allocations WHERE is_allocated = 0;");
    if (stmt_res.is_error()) return Result<size_t>::failure(stmt_res.error());
    auto stmt = std::move(stmt_res.value());
    if (stmt.step()) {
        return Result<size_t>::ok(static_cast<size_t>(stmt.column_int64(0)));
    }
    return Result<size_t>::ok(0);
}

Result<size_t> BlockAllocator::get_allocated_block_count() {
    auto stmt_res = db_.prepare("SELECT COUNT(*) FROM block_allocations WHERE is_allocated = 1;");
    if (stmt_res.is_error()) return Result<size_t>::failure(stmt_res.error());
    auto stmt = std::move(stmt_res.value());
    if (stmt.step()) {
        return Result<size_t>::ok(static_cast<size_t>(stmt.column_int64(0)));
    }
    return Result<size_t>::ok(0);
}

Result<std::vector<int>> BlockAllocator::allocate_blocks(size_t count, int64_t file_id) {
    if (count == 0) return Result<std::vector<int>>::ok({});

    auto free_res = get_free_block_count();
    if (free_res.is_error()) return Result<std::vector<int>>::failure(free_res.error());
    if (free_res.value() < count) {
        return Result<std::vector<int>>::failure("Out of storage space: requested " + std::to_string(count) +
                                                " blocks, but only " + std::to_string(free_res.value()) + " are free.", 507);
    }

    std::string select_sql = "SELECT block_id FROM block_allocations WHERE is_allocated = 0 ORDER BY block_id ASC;";
    auto sel_stmt_res = db_.prepare(select_sql);
    if (sel_stmt_res.is_error()) return Result<std::vector<int>>::failure(sel_stmt_res.error());
    auto sel_stmt = std::move(sel_stmt_res.value());
    std::vector<int> free_ids;
    while (sel_stmt.step()) {
        free_ids.push_back(sel_stmt.column_int(0));
    }

    std::vector<int> allocated_ids;
    if (strategy_ == "FIRST_FIT") {
        allocated_ids.assign(free_ids.begin(), free_ids.begin() + static_cast<ptrdiff_t>(count));
    } else {
        std::vector<std::vector<int>> runs;
        for (int block_id : free_ids) {
            if (runs.empty() || block_id != runs.back().back() + 1) {
                runs.push_back({block_id});
            } else {
                runs.back().push_back(block_id);
            }
        }

        const std::vector<int>* selected = nullptr;
        size_t selected_size = strategy_ == "BEST_FIT" ? ~size_t(0) : 0;
        for (const auto& run : runs) {
            if (run.size() < count) continue;
            if ((strategy_ == "BEST_FIT" && run.size() < selected_size) ||
                (strategy_ == "WORST_FIT" && run.size() > selected_size)) {
                selected = &run;
                selected_size = run.size();
            }
        }
        if (!selected) {
            return Result<std::vector<int>>::failure("No contiguous free block run large enough for " +
                                                     std::to_string(count) + " blocks.", 507);
        }
        allocated_ids.assign(selected->begin(), selected->begin() + static_cast<ptrdiff_t>(count));
    }

    std::string update_sql = "UPDATE block_allocations SET is_allocated = 1, owner_file_id = ?, block_index_in_file = ?, updated_at = ? WHERE block_id = ?;";
    auto upd_stmt_res = db_.prepare(update_sql);
    if (upd_stmt_res.is_error()) return Result<std::vector<int>>::failure(upd_stmt_res.error());
    auto upd_stmt = std::move(upd_stmt_res.value());
    std::string now = current_iso_time();

    for (size_t i = 0; i < allocated_ids.size(); ++i) {
        upd_stmt.reset();
        upd_stmt.clear_bindings();
        upd_stmt.bind_int64(1, file_id);
        upd_stmt.bind_int(2, static_cast<int>(i));
        upd_stmt.bind_text(3, now);
        upd_stmt.bind_int(4, allocated_ids[i]);
        upd_stmt.execute();
    }

    return Result<std::vector<int>>::ok(std::move(allocated_ids));
}

Result<void> BlockAllocator::free_blocks_for_file(int64_t file_id) {
    std::string sql = "UPDATE block_allocations SET is_allocated = 0, owner_file_id = NULL, block_index_in_file = -1, updated_at = ? WHERE owner_file_id = ?;";
    auto stmt_res = db_.prepare(sql);
    if (stmt_res.is_error()) return Result<void>::failure(stmt_res.error());
    auto stmt = std::move(stmt_res.value());
    stmt.bind_text(1, current_iso_time());
    stmt.bind_int64(2, file_id);
    stmt.execute();
    return Result<void>::ok();
}

Result<void> BlockAllocator::free_specific_blocks(const std::vector<int>& block_ids) {
    if (block_ids.empty()) return Result<void>::ok();
    std::string sql = "UPDATE block_allocations SET is_allocated = 0, owner_file_id = NULL, block_index_in_file = -1, updated_at = ? WHERE block_id = ?;";
    auto stmt_res = db_.prepare(sql);
    if (stmt_res.is_error()) return Result<void>::failure(stmt_res.error());
    auto stmt = std::move(stmt_res.value());
    std::string now = current_iso_time();
    for (int bid : block_ids) {
        stmt.reset();
        stmt.clear_bindings();
        stmt.bind_text(1, now);
        stmt.bind_int(2, bid);
        stmt.execute();
    }
    return Result<void>::ok();
}

Result<std::vector<BlockInfo>> BlockAllocator::get_all_blocks() {
    std::string sql = R"(
        SELECT b.block_id, b.is_allocated, b.owner_file_id, b.block_index_in_file, f.name 
        FROM block_allocations b
        LEFT JOIN files f ON b.owner_file_id = f.id
        ORDER BY b.block_id ASC;
    )";
    auto stmt_res = db_.prepare(sql);
    if (stmt_res.is_error()) return Result<std::vector<BlockInfo>>::failure(stmt_res.error());
    auto stmt = std::move(stmt_res.value());

    std::vector<BlockInfo> blocks;
    blocks.reserve(total_blocks_);
    while (stmt.step()) {
        BlockInfo info;
        info.block_id = stmt.column_int(0);
        info.is_allocated = stmt.column_int(1) != 0;
        info.owner_file_id = stmt.column_is_null(2) ? 0 : stmt.column_int64(2);
        info.block_index_in_file = stmt.column_int(3);
        info.owner_file_name = stmt.column_is_null(4) ? "" : stmt.column_text(4);
        blocks.push_back(std::move(info));
    }
    return Result<std::vector<BlockInfo>>::ok(std::move(blocks));
}

Result<BlockInfo> BlockAllocator::get_block(int block_id) {
    std::string sql = R"(
        SELECT b.block_id, b.is_allocated, b.owner_file_id, b.block_index_in_file, f.name 
        FROM block_allocations b
        LEFT JOIN files f ON b.owner_file_id = f.id
        WHERE b.block_id = ?;
    )";
    auto stmt_res = db_.prepare(sql);
    if (stmt_res.is_error()) return Result<BlockInfo>::failure(stmt_res.error());
    auto stmt = std::move(stmt_res.value());
    stmt.bind_int(1, block_id);

    if (stmt.step()) {
        BlockInfo info;
        info.block_id = stmt.column_int(0);
        info.is_allocated = stmt.column_int(1) != 0;
        info.owner_file_id = stmt.column_is_null(2) ? 0 : stmt.column_int64(2);
        info.block_index_in_file = stmt.column_int(3);
        info.owner_file_name = stmt.column_is_null(4) ? "" : stmt.column_text(4);
        if (info.is_allocated) {
            auto payload_res = db_.prepare(
                "SELECT data FROM file_blocks WHERE file_id = ? AND block_id = ?;");
            if (payload_res.is_ok()) {
                auto payload_stmt = std::move(payload_res.value());
                payload_stmt.bind_int64(1, info.owner_file_id);
                payload_stmt.bind_int(2, info.block_id);
                if (payload_stmt.step()) {
                    info.payload = payload_stmt.column_blob(0);
                }
            }
        }
        return Result<BlockInfo>::ok(std::move(info));
    }
    return Result<BlockInfo>::failure("Block not found: " + std::to_string(block_id), 404);
}

} // namespace osvault
