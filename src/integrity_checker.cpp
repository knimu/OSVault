#include "osvault/integrity_checker.hpp"
#include <unordered_map>
#include <unordered_set>

namespace osvault {

IntegrityChecker::IntegrityChecker(Database& db, size_t total_blocks)
    : db_(db), total_blocks_(total_blocks) {}

Result<IntegrityReport> IntegrityChecker::run_check() {
    std::lock_guard<std::mutex> lock(db_.mutex());

    IntegrityReport report;
    report.is_healthy = true;

    // 1. Files checked and block count verification
    auto f_stmt_res = db_.prepare("SELECT id, name, size, block_count FROM files;");
    if (f_stmt_res.is_error()) return Result<IntegrityReport>::failure(f_stmt_res.error());
    auto f_stmt = std::move(f_stmt_res.value());

    std::unordered_set<int64_t> valid_file_ids;
    while (f_stmt.step()) {
        ++report.files_checked;
        int64_t fid = f_stmt.column_int64(0);
        std::string fname = f_stmt.column_text(1);
        size_t fsize = static_cast<size_t>(f_stmt.column_int64(2));
        size_t recorded_blocks = static_cast<size_t>(f_stmt.column_int64(3));
        valid_file_ids.insert(fid);

        size_t expected_blocks = (fsize == 0) ? 0 : (fsize + 4096 - 1) / 4096;
        if (recorded_blocks != expected_blocks) {
            report.block_count_mismatches++;
            report.is_healthy = false;
            report.errors.push_back("File '" + fname + "' size " + std::to_string(fsize) +
                                   " bytes expects " + std::to_string(expected_blocks) +
                                   " blocks, but record states " + std::to_string(recorded_blocks));
        }

        // Verify count of rows in file_blocks matches recorded_blocks
        auto fb_cnt_res = db_.prepare("SELECT COUNT(*) FROM file_blocks WHERE file_id = ?;");
        if (fb_cnt_res.is_ok()) {
            auto fb_cnt_stmt = std::move(fb_cnt_res.value());
            fb_cnt_stmt.bind_int64(1, fid);
            if (fb_cnt_stmt.step()) {
                size_t actual_fb_rows = static_cast<size_t>(fb_cnt_stmt.column_int64(0));
                if (actual_fb_rows != recorded_blocks) {
                    report.block_count_mismatches++;
                    report.is_healthy = false;
                    report.errors.push_back("File '" + fname + "' has " + std::to_string(actual_fb_rows) +
                                           " file_blocks entries, but header states " + std::to_string(recorded_blocks));
                }
            }
        }
    }

    // 2. Check file_blocks for duplicates and invalid block IDs
    auto fb_stmt_res = db_.prepare("SELECT file_id, block_id FROM file_blocks;");
    if (fb_stmt_res.is_error()) return Result<IntegrityReport>::failure(fb_stmt_res.error());
    auto fb_stmt = std::move(fb_stmt_res.value());

    std::unordered_map<int, std::vector<int64_t>> block_to_files;
    while (fb_stmt.step()) {
        ++report.blocks_referenced;
        int64_t fid = fb_stmt.column_int64(0);
        int bid = fb_stmt.column_int(1);

        if (bid < 0 || static_cast<size_t>(bid) >= total_blocks_) {
            report.invalid_block_ids++;
            report.is_healthy = false;
            report.errors.push_back("Invalid block ID referenced: " + std::to_string(bid));
        }

        block_to_files[bid].push_back(fid);
    }

    for (const auto& [bid, owners] : block_to_files) {
        if (owners.size() > 1) {
            report.duplicate_references++;
            report.is_healthy = false;
            report.errors.push_back("Duplicate block ownership on block " + std::to_string(bid) +
                                   " across " + std::to_string(owners.size()) + " files.");
        }
    }

    // 3. Cross-reference with block_allocations table
    auto ba_stmt_res = db_.prepare("SELECT block_id, is_allocated, owner_file_id FROM block_allocations;");
    if (ba_stmt_res.is_error()) return Result<IntegrityReport>::failure(ba_stmt_res.error());
    auto ba_stmt = std::move(ba_stmt_res.value());

    size_t total_alloc_bits = 0;
    while (ba_stmt.step()) {
        int bid = ba_stmt.column_int(0);
        bool is_alloc = ba_stmt.column_int(1) != 0;
        bool has_owner = !ba_stmt.column_is_null(2);

        if (is_alloc) {
            ++total_alloc_bits;
            // Should be referenced in file_blocks
            if (block_to_files.find(bid) == block_to_files.end()) {
                report.orphaned_blocks++;
                report.is_healthy = false;
                report.errors.push_back("Orphaned block: Block " + std::to_string(bid) +
                                       " is marked allocated but belongs to no file.");
            }
        } else {
            // Should not be in file_blocks
            if (block_to_files.find(bid) != block_to_files.end()) {
                report.is_healthy = false;
                report.errors.push_back("Missing allocation bit: Block " + std::to_string(bid) +
                                       " is used by file but marked free in allocation table.");
            }
        }
    }

    if (total_alloc_bits > total_blocks_) {
        report.capacity_exceeded = true;
        report.is_healthy = false;
        report.errors.push_back("Total allocated blocks (" + std::to_string(total_alloc_bits) +
                               ") exceeds total capacity (" + std::to_string(total_blocks_) + ")");
    }

    // Log the check
    auto log_stmt_res = db_.prepare("INSERT INTO operation_logs (timestamp, event_type, description, result, related_id) VALUES (datetime('now'), 'INTEGRITY_CHECK', ?, ?, 'STORAGE');");
    if (log_stmt_res.is_ok()) {
        auto log_stmt = std::move(log_stmt_res.value());
        log_stmt.bind_text(1, "Checked " + std::to_string(report.files_checked) + " files, " +
                              std::to_string(report.blocks_referenced) + " blocks. " +
                              (report.is_healthy ? "All invariants verified." : "Issues found."));
        log_stmt.bind_text(2, report.is_healthy ? "SUCCESS" : "INCONSISTENT");
        log_stmt.execute();
    }

    return Result<IntegrityReport>::ok(std::move(report));
}

Result<void> IntegrityChecker::fix_orphaned_blocks() {
    std::lock_guard<std::mutex> lock(db_.mutex());
    std::string sql = R"(
        UPDATE block_allocations 
        SET is_allocated = 0, owner_file_id = NULL, block_index_in_file = -1, updated_at = datetime('now')
        WHERE block_id NOT IN (SELECT block_id FROM file_blocks);
    )";
    return db_.execute(sql);
}

} // namespace osvault
