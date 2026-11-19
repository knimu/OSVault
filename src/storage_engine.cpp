#include "osvault/storage_engine.hpp"
#include <algorithm>
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

bool StorageEngine::is_valid_filename(const std::string& name) {
    if (name.empty() || name.length() > 128) return false;
    if (name == "." || name == "..") return false;
    for (char c : name) {
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '.' && c != '_' && c != '-') {
            return false;
        }
    }
    return true;
}

bool StorageEngine::is_valid_directory_name(const std::string& name) {
    return is_valid_filename(name);
}

size_t StorageEngine::calculate_blocks_needed(size_t byte_count, size_t block_size) {
    if (byte_count == 0 || block_size == 0) return 0;
    return (byte_count + block_size - 1) / block_size;
}

StorageEngine::StorageEngine(Database& db, BlockAllocator& allocator, size_t block_size)
    : db_(db), allocator_(allocator), block_size_(block_size) {}

Result<std::vector<int>> StorageEngine::get_file_block_ids(int64_t file_id) {
    std::string sql = "SELECT block_id FROM file_blocks WHERE file_id = ? ORDER BY block_index ASC;";
    auto stmt_res = db_.prepare(sql);
    if (stmt_res.is_error()) return Result<std::vector<int>>::failure(stmt_res.error());
    auto stmt = std::move(stmt_res.value());
    stmt.bind_int64(1, file_id);

    std::vector<int> ids;
    while (stmt.step()) {
        ids.push_back(stmt.column_int(0));
    }
    return Result<std::vector<int>>::ok(std::move(ids));
}

Result<FileRecord> StorageEngine::create_file(const std::string& name, const std::string& content) {
    return create_file(name, content, 0);
}

Result<FileRecord> StorageEngine::create_file(const std::string& name, const std::string& content, int64_t directory_id) {
    if (!is_valid_filename(name)) {
        return Result<FileRecord>::failure("Invalid file name. Must be 1-128 alphanumeric, dot, underscore, or dash characters.", 400);
    }
    if (directory_id < 0) {
        return Result<FileRecord>::failure("Invalid directory ID.", 400);
    }
    if (directory_id > 0) {
        auto dir_stmt_res = db_.prepare("SELECT id FROM directories WHERE id = ?;");
        if (dir_stmt_res.is_error()) return Result<FileRecord>::failure(dir_stmt_res.error());
        auto dir_stmt = std::move(dir_stmt_res.value());
        dir_stmt.bind_int64(1, directory_id);
        if (!dir_stmt.step()) return Result<FileRecord>::failure("Directory not found.", 404);
    }

    // Check if file already exists
    auto check_stmt_res = db_.prepare("SELECT id FROM files WHERE name = ? AND directory_id = ?;");
    if (check_stmt_res.is_error()) return Result<FileRecord>::failure(check_stmt_res.error());
    auto check_stmt = std::move(check_stmt_res.value());
    check_stmt.bind_text(1, name);
    check_stmt.bind_int64(2, directory_id);
    if (check_stmt.step()) return Result<FileRecord>::failure("File already exists in this directory: " + name, 409);

    size_t size = content.size();
    size_t blocks_needed = calculate_blocks_needed(size, block_size_);

    std::string now = current_iso_time();
    std::lock_guard<std::mutex> lock(db_.mutex());

    Transaction tx(db_);

    // 1. Insert file record
    std::string ins_file_sql = "INSERT INTO files (name, directory_id, size, block_count, created_at, updated_at) VALUES (?, ?, ?, ?, ?, ?);";
    auto ins_stmt_res = db_.prepare(ins_file_sql);
    if (ins_stmt_res.is_error()) return Result<FileRecord>::failure(ins_stmt_res.error());
    auto ins_stmt = std::move(ins_stmt_res.value());
    ins_stmt.bind_text(1, name);
    ins_stmt.bind_int64(2, directory_id);
    ins_stmt.bind_int64(3, static_cast<int64_t>(size));
    ins_stmt.bind_int64(4, static_cast<int64_t>(blocks_needed));
    ins_stmt.bind_text(5, now);
    ins_stmt.bind_text(6, now);
    ins_stmt.execute();

    // Get inserted file id
    auto id_stmt_res = db_.prepare("SELECT last_insert_rowid();");
    if (id_stmt_res.is_error()) return Result<FileRecord>::failure(id_stmt_res.error());
    auto id_stmt = std::move(id_stmt_res.value());
    int64_t file_id = 0;
    if (id_stmt.step()) {
        file_id = id_stmt.column_int64(0);
    }

    // 2. Allocate blocks if needed
    std::vector<int> allocated_blocks;
    if (blocks_needed > 0) {
        auto alloc_res = allocator_.allocate_blocks(blocks_needed, file_id);
        if (alloc_res.is_error()) {
            return Result<FileRecord>::failure(alloc_res.error(), alloc_res.code());
        }
        allocated_blocks = std::move(alloc_res.value());

        // 3. Insert into file_blocks with chunks
        std::string ins_fb_sql = "INSERT INTO file_blocks (file_id, block_index, block_id, data) VALUES (?, ?, ?, ?);";
        auto fb_stmt_res = db_.prepare(ins_fb_sql);
        if (fb_stmt_res.is_error()) return Result<FileRecord>::failure(fb_stmt_res.error());
        auto fb_stmt = std::move(fb_stmt_res.value());

        for (size_t i = 0; i < blocks_needed; ++i) {
            size_t offset = i * block_size_;
            size_t chunk_len = std::min(block_size_, size - offset);
            const char* chunk_ptr = content.data() + offset;

            fb_stmt.reset();
            fb_stmt.clear_bindings();
            fb_stmt.bind_int64(1, file_id);
            fb_stmt.bind_int(2, static_cast<int>(i));
            fb_stmt.bind_int(3, allocated_blocks[i]);
            fb_stmt.bind_blob(4, chunk_ptr, static_cast<int>(chunk_len));
            fb_stmt.execute();
        }
    }

    // Log operation
    auto log_stmt_res = db_.prepare("INSERT INTO operation_logs (timestamp, event_type, description, result, related_id) VALUES (?, 'FILE_CREATE', ?, 'SUCCESS', ?);");
    if (log_stmt_res.is_ok()) {
        auto log_stmt = std::move(log_stmt_res.value());
        log_stmt.bind_text(1, now);
        log_stmt.bind_text(2, "Created file '" + name + "' (" + std::to_string(size) + " bytes, " + std::to_string(blocks_needed) + " blocks)");
        log_stmt.bind_text(3, std::to_string(file_id));
        log_stmt.execute();
    }

    tx.commit();

    FileRecord rec;
    rec.id = file_id;
    rec.name = name;
    rec.size = size;
    rec.block_count = blocks_needed;
    rec.block_ids = std::move(allocated_blocks);
    rec.created_at = now;
    rec.updated_at = now;
    return Result<FileRecord>::ok(std::move(rec));
}

Result<std::string> StorageEngine::read_file(const std::string& name) {
    return read_file(name, 0);
}

Result<std::string> StorageEngine::read_file(const std::string& name, int64_t directory_id) {
    auto meta_res = get_file_metadata(name, directory_id);
    if (meta_res.is_error()) return Result<std::string>::failure(meta_res.error(), meta_res.code());
    const auto& meta = meta_res.value();

    if (meta.size == 0 || meta.block_count == 0) {
        return Result<std::string>::ok("");
    }

    std::string sql = "SELECT data FROM file_blocks WHERE file_id = ? ORDER BY block_index ASC;";
    auto stmt_res = db_.prepare(sql);
    if (stmt_res.is_error()) return Result<std::string>::failure(stmt_res.error());
    auto stmt = std::move(stmt_res.value());
    stmt.bind_int64(1, meta.id);

    std::string content;
    content.reserve(meta.size);
    while (stmt.step()) {
        content.append(stmt.column_blob(0));
    }

    return Result<std::string>::ok(std::move(content));
}

Result<FileRecord> StorageEngine::get_file_metadata(const std::string& name) {
    return get_file_metadata(name, 0);
}

Result<FileRecord> StorageEngine::get_file_metadata(const std::string& name, int64_t directory_id) {
    std::string sql = "SELECT id, name, directory_id, size, block_count, created_at, updated_at FROM files WHERE name = ? AND directory_id = ?;";
    auto stmt_res = db_.prepare(sql);
    if (stmt_res.is_error()) return Result<FileRecord>::failure(stmt_res.error());
    auto stmt = std::move(stmt_res.value());
    stmt.bind_text(1, name);
    stmt.bind_int64(2, directory_id);

    if (stmt.step()) {
        FileRecord rec;
        rec.id = stmt.column_int64(0);
        rec.name = stmt.column_text(1);
        rec.directory_id = stmt.column_int64(2);
        rec.size = static_cast<size_t>(stmt.column_int64(3));
        rec.block_count = static_cast<size_t>(stmt.column_int64(4));
        rec.created_at = stmt.column_text(5);
        rec.updated_at = stmt.column_text(6);

        auto blocks_res = get_file_block_ids(rec.id);
        if (blocks_res.is_ok()) {
            rec.block_ids = blocks_res.value();
        }
        return Result<FileRecord>::ok(std::move(rec));
    }
    return Result<FileRecord>::failure("File not found: " + name, 404);
}

Result<FileRecord> StorageEngine::get_file_metadata_by_id(int64_t file_id) {
    std::string sql = "SELECT id, name, directory_id, size, block_count, created_at, updated_at FROM files WHERE id = ?;";
    auto stmt_res = db_.prepare(sql);
    if (stmt_res.is_error()) return Result<FileRecord>::failure(stmt_res.error());
    auto stmt = std::move(stmt_res.value());
    stmt.bind_int64(1, file_id);

    if (stmt.step()) {
        FileRecord rec;
        rec.id = stmt.column_int64(0);
        rec.name = stmt.column_text(1);
        rec.directory_id = stmt.column_int64(2);
        rec.size = static_cast<size_t>(stmt.column_int64(3));
        rec.block_count = static_cast<size_t>(stmt.column_int64(4));
        rec.created_at = stmt.column_text(5);
        rec.updated_at = stmt.column_text(6);

        auto blocks_res = get_file_block_ids(rec.id);
        if (blocks_res.is_ok()) {
            rec.block_ids = blocks_res.value();
        }
        return Result<FileRecord>::ok(std::move(rec));
    }
    return Result<FileRecord>::failure("File not found with ID: " + std::to_string(file_id), 404);
}

Result<void> StorageEngine::write_file(const std::string& name, const std::string& new_content) {
    return write_file(name, new_content, 0);
}

Result<void> StorageEngine::write_file(const std::string& name, const std::string& new_content, int64_t directory_id) {
    auto meta_res = get_file_metadata(name, directory_id);
    if (meta_res.is_error()) return Result<void>::failure(meta_res.error(), meta_res.code());
    const auto& meta = meta_res.value();

    size_t new_size = new_content.size();
    size_t new_blocks_needed = calculate_blocks_needed(new_size, block_size_);

    std::string now = current_iso_time();
    std::lock_guard<std::mutex> lock(db_.mutex());

    Transaction tx(db_);

    // Free previously allocated blocks for this file
    allocator_.free_blocks_for_file(meta.id);

    // Delete existing file_blocks
    std::string del_fb_sql = "DELETE FROM file_blocks WHERE file_id = ?;";
    auto del_stmt_res = db_.prepare(del_fb_sql);
    if (del_stmt_res.is_error()) return Result<void>::failure(del_stmt_res.error());
    auto del_stmt = std::move(del_stmt_res.value());
    del_stmt.bind_int64(1, meta.id);
    del_stmt.execute();

    // Allocate new blocks if needed
    std::vector<int> new_blocks;
    if (new_blocks_needed > 0) {
        auto alloc_res = allocator_.allocate_blocks(new_blocks_needed, meta.id);
        if (alloc_res.is_error()) {
            return Result<void>::failure(alloc_res.error(), alloc_res.code());
        }
        new_blocks = std::move(alloc_res.value());

        // Insert new file_blocks
        std::string ins_fb_sql = "INSERT INTO file_blocks (file_id, block_index, block_id, data) VALUES (?, ?, ?, ?);";
        auto fb_stmt_res = db_.prepare(ins_fb_sql);
        if (fb_stmt_res.is_error()) return Result<void>::failure(fb_stmt_res.error());
        auto fb_stmt = std::move(fb_stmt_res.value());

        for (size_t i = 0; i < new_blocks_needed; ++i) {
            size_t offset = i * block_size_;
            size_t chunk_len = std::min(block_size_, new_size - offset);
            const char* chunk_ptr = new_content.data() + offset;

            fb_stmt.reset();
            fb_stmt.clear_bindings();
            fb_stmt.bind_int64(1, meta.id);
            fb_stmt.bind_int(2, static_cast<int>(i));
            fb_stmt.bind_int(3, new_blocks[i]);
            fb_stmt.bind_blob(4, chunk_ptr, static_cast<int>(chunk_len));
            fb_stmt.execute();
        }
    }

    // Update file record
    std::string upd_file_sql = "UPDATE files SET size = ?, block_count = ?, updated_at = ? WHERE id = ?;";
    auto upd_stmt_res = db_.prepare(upd_file_sql);
    if (upd_stmt_res.is_error()) return Result<void>::failure(upd_stmt_res.error());
    auto upd_stmt = std::move(upd_stmt_res.value());
    upd_stmt.bind_int64(1, static_cast<int64_t>(new_size));
    upd_stmt.bind_int64(2, static_cast<int64_t>(new_blocks_needed));
    upd_stmt.bind_text(3, now);
    upd_stmt.bind_int64(4, meta.id);
    upd_stmt.execute();

    // Log operation
    auto log_stmt_res = db_.prepare("INSERT INTO operation_logs (timestamp, event_type, description, result, related_id) VALUES (?, 'FILE_WRITE', ?, 'SUCCESS', ?);");
    if (log_stmt_res.is_ok()) {
        auto log_stmt = std::move(log_stmt_res.value());
        log_stmt.bind_text(1, now);
        log_stmt.bind_text(2, "Updated file '" + name + "' to " + std::to_string(new_size) + " bytes (" + std::to_string(new_blocks_needed) + " blocks)");
        log_stmt.bind_text(3, std::to_string(meta.id));
        log_stmt.execute();
    }

    tx.commit();
    return Result<void>::ok();
}

Result<void> StorageEngine::rename_file(const std::string& old_name, const std::string& new_name) {
    return rename_file(old_name, new_name, 0);
}

Result<void> StorageEngine::rename_file(const std::string& old_name, const std::string& new_name, int64_t directory_id) {
    if (!is_valid_filename(new_name)) {
        return Result<void>::failure("Invalid new file name.", 400);
    }
    if (old_name == new_name) {
        return Result<void>::ok();
    }

    auto existing_res = get_file_metadata(new_name, directory_id);
    if (existing_res.is_ok()) {
        return Result<void>::failure("Target file name already exists: " + new_name, 409);
    }

    auto meta_res = get_file_metadata(old_name, directory_id);
    if (meta_res.is_error()) return Result<void>::failure(meta_res.error(), meta_res.code());
    const auto& meta = meta_res.value();

    std::string now = current_iso_time();
    std::lock_guard<std::mutex> lock(db_.mutex());

    Transaction tx(db_);

    std::string sql = "UPDATE files SET name = ?, updated_at = ? WHERE id = ?;";
    auto stmt_res = db_.prepare(sql);
    if (stmt_res.is_error()) return Result<void>::failure(stmt_res.error());
    auto stmt = std::move(stmt_res.value());
    stmt.bind_text(1, new_name);
    stmt.bind_text(2, now);
    stmt.bind_int64(3, meta.id);
    stmt.execute();

    // Log rename
    auto log_stmt_res = db_.prepare("INSERT INTO operation_logs (timestamp, event_type, description, result, related_id) VALUES (?, 'FILE_RENAME', ?, 'SUCCESS', ?);");
    if (log_stmt_res.is_ok()) {
        auto log_stmt = std::move(log_stmt_res.value());
        log_stmt.bind_text(1, now);
        log_stmt.bind_text(2, "Renamed file '" + old_name + "' to '" + new_name + "'");
        log_stmt.bind_text(3, std::to_string(meta.id));
        log_stmt.execute();
    }

    tx.commit();
    return Result<void>::ok();
}

Result<void> StorageEngine::delete_file(const std::string& name) {
    return delete_file(name, 0);
}

Result<void> StorageEngine::delete_file(const std::string& name, int64_t directory_id) {
    auto meta_res = get_file_metadata(name, directory_id);
    if (meta_res.is_error()) return Result<void>::failure(meta_res.error(), meta_res.code());
    const auto& meta = meta_res.value();

    std::string now = current_iso_time();
    std::lock_guard<std::mutex> lock(db_.mutex());

    Transaction tx(db_);

    // 1. Free blocks in allocator
    allocator_.free_blocks_for_file(meta.id);

    // 2. Delete file blocks
    std::string del_fb_sql = "DELETE FROM file_blocks WHERE file_id = ?;";
    auto del_fb_stmt_res = db_.prepare(del_fb_sql);
    if (del_fb_stmt_res.is_error()) return Result<void>::failure(del_fb_stmt_res.error());
    auto del_fb_stmt = std::move(del_fb_stmt_res.value());
    del_fb_stmt.bind_int64(1, meta.id);
    del_fb_stmt.execute();

    // 3. Delete file record
    std::string del_f_sql = "DELETE FROM files WHERE id = ?;";
    auto del_f_stmt_res = db_.prepare(del_f_sql);
    if (del_f_stmt_res.is_error()) return Result<void>::failure(del_f_stmt_res.error());
    auto del_f_stmt = std::move(del_f_stmt_res.value());
    del_f_stmt.bind_int64(1, meta.id);
    del_f_stmt.execute();

    // Log deletion
    auto log_stmt_res = db_.prepare("INSERT INTO operation_logs (timestamp, event_type, description, result, related_id) VALUES (?, 'FILE_DELETE', ?, 'SUCCESS', ?);");
    if (log_stmt_res.is_ok()) {
        auto log_stmt = std::move(log_stmt_res.value());
        log_stmt.bind_text(1, now);
        log_stmt.bind_text(2, "Deleted file '" + name + "' (freed " + std::to_string(meta.block_count) + " blocks)");
        log_stmt.bind_text(3, std::to_string(meta.id));
        log_stmt.execute();
    }

    tx.commit();
    return Result<void>::ok();
}

Result<std::vector<FileRecord>> StorageEngine::list_files(const std::string& search_query) {
    std::string sql;
    if (search_query.empty()) {
        sql = "SELECT id, name, directory_id, size, block_count, created_at, updated_at FROM files ORDER BY name ASC;";
    } else {
        sql = "SELECT id, name, directory_id, size, block_count, created_at, updated_at FROM files WHERE name LIKE ? ORDER BY name ASC;";
    }

    auto stmt_res = db_.prepare(sql);
    if (stmt_res.is_error()) return Result<std::vector<FileRecord>>::failure(stmt_res.error());
    auto stmt = std::move(stmt_res.value());
    if (!search_query.empty()) {
        stmt.bind_text(1, "%" + search_query + "%");
    }

    std::vector<FileRecord> files;
    while (stmt.step()) {
        FileRecord rec;
        rec.id = stmt.column_int64(0);
        rec.name = stmt.column_text(1);
        rec.directory_id = stmt.column_int64(2);
        rec.size = static_cast<size_t>(stmt.column_int64(3));
        rec.block_count = static_cast<size_t>(stmt.column_int64(4));
        rec.created_at = stmt.column_text(5);
        rec.updated_at = stmt.column_text(6);

        auto blocks_res = get_file_block_ids(rec.id);
        if (blocks_res.is_ok()) {
            rec.block_ids = blocks_res.value();
        }
        files.push_back(std::move(rec));
    }
    return Result<std::vector<FileRecord>>::ok(std::move(files));
}

Result<std::vector<FileRecord>> StorageEngine::list_directory(int64_t directory_id, const std::string& search_query) {
    std::string sql = search_query.empty()
        ? "SELECT id, name, directory_id, size, block_count, created_at, updated_at FROM files WHERE directory_id = ? ORDER BY name ASC;"
        : "SELECT id, name, directory_id, size, block_count, created_at, updated_at FROM files WHERE directory_id = ? AND name LIKE ? ORDER BY name ASC;";
    auto stmt_res = db_.prepare(sql);
    if (stmt_res.is_error()) return Result<std::vector<FileRecord>>::failure(stmt_res.error());
    auto stmt = std::move(stmt_res.value());
    stmt.bind_int64(1, directory_id);
    if (!search_query.empty()) stmt.bind_text(2, "%" + search_query + "%");

    std::vector<FileRecord> files;
    while (stmt.step()) {
        FileRecord rec;
        rec.id = stmt.column_int64(0);
        rec.name = stmt.column_text(1);
        rec.directory_id = stmt.column_int64(2);
        rec.size = static_cast<size_t>(stmt.column_int64(3));
        rec.block_count = static_cast<size_t>(stmt.column_int64(4));
        rec.created_at = stmt.column_text(5);
        rec.updated_at = stmt.column_text(6);
        auto blocks_res = get_file_block_ids(rec.id);
        if (blocks_res.is_ok()) rec.block_ids = blocks_res.value();
        files.push_back(std::move(rec));
    }
    return Result<std::vector<FileRecord>>::ok(std::move(files));
}

Result<DirectoryRecord> StorageEngine::create_directory(const std::string& name, int64_t parent_id) {
    if (!is_valid_directory_name(name)) {
        return Result<DirectoryRecord>::failure("Invalid directory name. Use 1-128 alphanumeric, dot, underscore, or dash characters.", 400);
    }
    if (parent_id > 0) {
        auto parent_res = db_.prepare("SELECT id FROM directories WHERE id = ?;");
        if (parent_res.is_error()) return Result<DirectoryRecord>::failure(parent_res.error());
        auto parent = std::move(parent_res.value());
        parent.bind_int64(1, parent_id);
        if (!parent.step()) return Result<DirectoryRecord>::failure("Parent directory not found.", 404);
    }

    auto check_res = db_.prepare("SELECT id FROM directories WHERE parent_id = ? AND name = ?;");
    if (check_res.is_error()) return Result<DirectoryRecord>::failure(check_res.error());
    auto check = std::move(check_res.value());
    check.bind_int64(1, parent_id);
    check.bind_text(2, name);
    if (check.step()) return Result<DirectoryRecord>::failure("Directory already exists: " + name, 409);

    std::string now = current_iso_time();
    auto ins_res = db_.prepare("INSERT INTO directories (name, parent_id, created_at) VALUES (?, ?, ?);");
    if (ins_res.is_error()) return Result<DirectoryRecord>::failure(ins_res.error());
    auto ins = std::move(ins_res.value());
    ins.bind_text(1, name);
    ins.bind_int64(2, parent_id);
    ins.bind_text(3, now);
    ins.execute();

    auto id_res = db_.prepare("SELECT last_insert_rowid();");
    if (id_res.is_error()) return Result<DirectoryRecord>::failure(id_res.error());
    auto id_stmt = std::move(id_res.value());
    int64_t id = id_stmt.step() ? id_stmt.column_int64(0) : 0;

    DirectoryRecord record;
    record.id = id;
    record.name = name;
    record.parent_id = parent_id;
    record.created_at = now;
    return Result<DirectoryRecord>::ok(std::move(record));
}

Result<std::vector<DirectoryRecord>> StorageEngine::list_directories(int64_t parent_id) {
    auto stmt_res = db_.prepare("SELECT id, name, parent_id, created_at FROM directories WHERE parent_id = ? ORDER BY name ASC;");
    if (stmt_res.is_error()) return Result<std::vector<DirectoryRecord>>::failure(stmt_res.error());
    auto stmt = std::move(stmt_res.value());
    stmt.bind_int64(1, parent_id);

    std::vector<DirectoryRecord> directories;
    while (stmt.step()) {
        DirectoryRecord record;
        record.id = stmt.column_int64(0);
        record.name = stmt.column_text(1);
        record.parent_id = stmt.column_int64(2);
        record.created_at = stmt.column_text(3);
        directories.push_back(std::move(record));
    }
    return Result<std::vector<DirectoryRecord>>::ok(std::move(directories));
}

Result<void> StorageEngine::delete_directory(int64_t directory_id) {
    if (directory_id <= 0) return Result<void>::failure("Invalid directory ID.", 400);
    auto file_check = db_.prepare("SELECT id FROM files WHERE directory_id = ? LIMIT 1;");
    if (file_check.is_error()) return Result<void>::failure(file_check.error());
    auto file = std::move(file_check.value());
    file.bind_int64(1, directory_id);
    if (file.step()) return Result<void>::failure("Directory is not empty. Delete or move files first.", 409);

    auto child_check = db_.prepare("SELECT id FROM directories WHERE parent_id = ? LIMIT 1;");
    if (child_check.is_error()) return Result<void>::failure(child_check.error());
    auto child = std::move(child_check.value());
    child.bind_int64(1, directory_id);
    if (child.step()) return Result<void>::failure("Directory contains subdirectories. Remove them first.", 409);

    auto del_res = db_.prepare("DELETE FROM directories WHERE id = ?;");
    if (del_res.is_error()) return Result<void>::failure(del_res.error());
    auto del = std::move(del_res.value());
    del.bind_int64(1, directory_id);
    del.execute();
    return Result<void>::ok();
}

Result<SystemMetrics> StorageEngine::get_system_metrics() {
    SystemMetrics m;
    m.total_blocks = allocator_.total_blocks();
    m.block_size = block_size_;
    m.total_capacity_bytes = total_capacity_bytes();

    auto alloc_res = allocator_.get_allocated_block_count();
    m.allocated_blocks = alloc_res.is_ok() ? alloc_res.value() : 0;

    auto free_res = allocator_.get_free_block_count();
    m.free_blocks = free_res.is_ok() ? free_res.value() : 0;

    if (m.total_blocks > 0) {
        m.storage_utilization_pct = (static_cast<double>(m.allocated_blocks) / m.total_blocks) * 100.0;
    }

    // Total files and bytes
    auto f_stmt_res = db_.prepare("SELECT COUNT(*), COALESCE(SUM(size), 0) FROM files;");
    if (f_stmt_res.is_ok()) {
        auto f_stmt = std::move(f_stmt_res.value());
        if (f_stmt.step()) {
            m.file_count = static_cast<size_t>(f_stmt.column_int64(0));
            m.total_bytes = static_cast<size_t>(f_stmt.column_int64(1));
        }
    }

    // Recent operations count
    auto op_stmt_res = db_.prepare("SELECT COUNT(*) FROM operation_logs;");
    if (op_stmt_res.is_ok()) {
        auto op_stmt = std::move(op_stmt_res.value());
        if (op_stmt.step()) {
            m.recent_operations_count = static_cast<size_t>(op_stmt.column_int64(0));
        }
    }

    return Result<SystemMetrics>::ok(m);
}

} // namespace osvault
