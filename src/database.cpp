#include "osvault/database.hpp"
#include "sqlite3.h"
#include <chrono>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace fs = std::filesystem;

namespace osvault {

static std::string current_iso_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&in_time_t), "%Y-%m-%dT%H:%M:%SZ");
    return ss.str();
}

// Statement Implementation
Statement::Statement(sqlite3_stmt* stmt) : stmt_(stmt) {}

Statement::~Statement() {
    if (stmt_) {
        sqlite3_finalize(stmt_);
        stmt_ = nullptr;
    }
}

Statement::Statement(Statement&& other) noexcept : stmt_(other.stmt_) {
    other.stmt_ = nullptr;
}

Statement& Statement::operator=(Statement&& other) noexcept {
    if (this != &other) {
        if (stmt_) sqlite3_finalize(stmt_);
        stmt_ = other.stmt_;
        other.stmt_ = nullptr;
    }
    return *this;
}

void Statement::reset() {
    if (stmt_) sqlite3_reset(stmt_);
}

void Statement::clear_bindings() {
    if (stmt_) sqlite3_clear_bindings(stmt_);
}

bool Statement::step() {
    if (!stmt_) return false;
    int rc = sqlite3_step(stmt_);
    if (rc == SQLITE_ROW) return true;
    if (rc == SQLITE_DONE) return false;
    throw std::runtime_error("SQLite step error: " + std::string(sqlite3_errstr(rc)));
}

void Statement::execute() {
    if (!stmt_) return;
    int rc = sqlite3_step(stmt_);
    if (rc != SQLITE_DONE && rc != SQLITE_ROW) {
        throw std::runtime_error("SQLite execute error: " + std::string(sqlite3_errstr(rc)));
    }
}

void Statement::bind_int(int index, int val) {
    if (!stmt_) return;
    int rc = sqlite3_bind_int(stmt_, index, val);
    if (rc != SQLITE_OK) throw std::runtime_error("Failed to bind int: " + std::string(sqlite3_errstr(rc)));
}

void Statement::bind_int64(int index, int64_t val) {
    if (!stmt_) return;
    int rc = sqlite3_bind_int64(stmt_, index, val);
    if (rc != SQLITE_OK) throw std::runtime_error("Failed to bind int64: " + std::string(sqlite3_errstr(rc)));
}

void Statement::bind_text(int index, const std::string& val) {
    if (!stmt_) return;
    int rc = sqlite3_bind_text(stmt_, index, val.c_str(), -1, SQLITE_TRANSIENT);
    if (rc != SQLITE_OK) throw std::runtime_error("Failed to bind text: " + std::string(sqlite3_errstr(rc)));
}

void Statement::bind_blob(int index, const void* data, int size) {
    if (!stmt_) return;
    int rc = sqlite3_bind_blob(stmt_, index, data, size, SQLITE_TRANSIENT);
    if (rc != SQLITE_OK) throw std::runtime_error("Failed to bind blob: " + std::string(sqlite3_errstr(rc)));
}

void Statement::bind_null(int index) {
    if (!stmt_) return;
    int rc = sqlite3_bind_null(stmt_, index);
    if (rc != SQLITE_OK) throw std::runtime_error("Failed to bind null: " + std::string(sqlite3_errstr(rc)));
}

int Statement::column_int(int col) const {
    return sqlite3_column_int(stmt_, col);
}

int64_t Statement::column_int64(int col) const {
    return sqlite3_column_int64(stmt_, col);
}

std::string Statement::column_text(int col) const {
    const unsigned char* text = sqlite3_column_text(stmt_, col);
    return text ? std::string(reinterpret_cast<const char*>(text)) : "";
}

std::string Statement::column_blob(int col) const {
    const void* b = sqlite3_column_blob(stmt_, col);
    int bytes = sqlite3_column_bytes(stmt_, col);
    return b && bytes > 0 ? std::string(reinterpret_cast<const char*>(b), bytes) : "";
}

bool Statement::column_is_null(int col) const {
    return sqlite3_column_type(stmt_, col) == SQLITE_NULL;
}

// Transaction Implementation
Transaction::Transaction(Database& db) : db_(db) {
    auto res = db_.execute("BEGIN TRANSACTION;");
    if (res.is_error()) {
        active_ = false;
        throw std::runtime_error("Failed to begin transaction: " + res.error());
    }
}

Transaction::~Transaction() {
    if (active_ && !committed_) {
        rollback();
    }
}

void Transaction::commit() {
    if (!active_ || committed_) return;
    auto res = db_.execute("COMMIT;");
    if (res.is_error()) {
        throw std::runtime_error("Failed to commit transaction: " + res.error());
    }
    committed_ = true;
    active_ = false;
}

void Transaction::rollback() {
    if (!active_ || committed_) return;
    db_.execute("ROLLBACK;");
    active_ = false;
}

// Database Implementation
Database::Database() = default;

Database::~Database() {
    close();
}

Result<void> Database::open(const std::string& path) {
    std::lock_guard<std::mutex> lock(db_mutex_);
    if (db_) close();

    path_ = path;
    try {
        fs::path p(path);
        if (p.has_parent_path()) {
            fs::create_directories(p.parent_path());
        }
    } catch (const std::exception& e) {
        return Result<void>::failure("Failed to create database directory: " + std::string(e.what()), 500);
    }

    int rc = sqlite3_open(path.c_str(), &db_);
    if (rc != SQLITE_OK) {
        std::string err = db_ ? sqlite3_errmsg(db_) : "Unable to open SQLite db";
        close();
        return Result<void>::failure("SQLite open error: " + err, 500);
    }

    // Configure WAL mode, foreign keys, and busy timeout
    char* errmsg = nullptr;
    sqlite3_exec(db_, "PRAGMA journal_mode = WAL;", nullptr, nullptr, &errmsg);
    if (errmsg) { sqlite3_free(errmsg); errmsg = nullptr; }

    sqlite3_exec(db_, "PRAGMA foreign_keys = ON;", nullptr, nullptr, &errmsg);
    if (errmsg) { sqlite3_free(errmsg); errmsg = nullptr; }

    sqlite3_exec(db_, "PRAGMA busy_timeout = 5000;", nullptr, nullptr, &errmsg);
    if (errmsg) { sqlite3_free(errmsg); errmsg = nullptr; }

    return initialize_schema();
}

void Database::close() {
    if (db_) {
        sqlite3_close_v2(db_);
        db_ = nullptr;
    }
}

Result<void> Database::execute(const std::string& sql) {
    if (!db_) return Result<void>::failure("Database not open", 500);
    char* errmsg = nullptr;
    int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &errmsg);
    if (rc != SQLITE_OK) {
        std::string err = errmsg ? errmsg : "Execution error";
        sqlite3_free(errmsg);
        return Result<void>::failure("SQL error (" + std::to_string(rc) + "): " + err, 500);
    }
    return Result<void>::ok();
}

Result<Statement> Database::prepare(const std::string& sql) {
    if (!db_) return Result<Statement>::failure("Database not open", 500);
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql.c_str(), static_cast<int>(sql.size()), &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return Result<Statement>::failure("Prepare error: " + std::string(sqlite3_errmsg(db_)), 500);
    }
    return Result<Statement>::ok(Statement(stmt));
}

int Database::get_schema_version() {
    std::string sql = "SELECT MAX(version) FROM schema_version;";
    auto stmt_res = prepare(sql);
    if (stmt_res.is_error()) return 0;
    auto stmt = std::move(stmt_res.value());
    if (stmt.step() && !stmt.column_is_null(0)) {
        return stmt.column_int(0);
    }
    return 0;
}

Result<void> Database::initialize_schema() {
    const char* schema_sql = R"(
        CREATE TABLE IF NOT EXISTS schema_version (
            version INTEGER PRIMARY KEY,
            applied_at TEXT NOT NULL
        );

        CREATE TABLE IF NOT EXISTS files (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT UNIQUE NOT NULL,
            size INTEGER NOT NULL,
            block_count INTEGER NOT NULL,
            created_at TEXT NOT NULL,
            updated_at TEXT NOT NULL
        );

        CREATE TABLE IF NOT EXISTS file_blocks (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            file_id INTEGER NOT NULL REFERENCES files(id) ON DELETE CASCADE,
            block_index INTEGER NOT NULL,
            block_id INTEGER NOT NULL UNIQUE,
            data BLOB,
            UNIQUE(file_id, block_index)
        );

        CREATE TABLE IF NOT EXISTS block_allocations (
            block_id INTEGER PRIMARY KEY,
            is_allocated INTEGER NOT NULL DEFAULT 0,
            owner_file_id INTEGER REFERENCES files(id) ON DELETE SET NULL,
            block_index_in_file INTEGER DEFAULT -1,
            updated_at TEXT
        );

        CREATE TABLE IF NOT EXISTS operation_logs (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp TEXT NOT NULL,
            event_type TEXT NOT NULL,
            description TEXT NOT NULL,
            result TEXT NOT NULL,
            related_id TEXT
        );

        CREATE TABLE IF NOT EXISTS recovery_records (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            experiment_name TEXT NOT NULL,
            timestamp TEXT NOT NULL,
            status TEXT NOT NULL,
            details TEXT
        );

        CREATE TABLE IF NOT EXISTS saved_experiments (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            module_name TEXT NOT NULL,
            title TEXT NOT NULL,
            config_json TEXT NOT NULL,
            created_at TEXT NOT NULL
        );
    )";

    auto res = execute(schema_sql);
    if (res.is_error()) return res;

    // Check schema version and populate blocks table if empty
    int current_version = get_schema_version();
    if (current_version < 1) {
        // Populate 1024 blocks if empty
        auto count_stmt = prepare("SELECT COUNT(*) FROM block_allocations;");
        if (count_stmt.is_ok() && count_stmt.value().step()) {
            if (count_stmt.value().column_int(0) == 0) {
                execute("BEGIN TRANSACTION;");
                auto ins_stmt = prepare("INSERT INTO block_allocations (block_id, is_allocated, owner_file_id, block_index_in_file, updated_at) VALUES (?, 0, NULL, -1, ?);");
                if (ins_stmt.is_ok()) {
                    std::string now = current_iso_timestamp();
                    for (int i = 0; i < 1024; ++i) {
                        ins_stmt.value().reset();
                        ins_stmt.value().clear_bindings();
                        ins_stmt.value().bind_int(1, i);
                        ins_stmt.value().bind_text(2, now);
                        ins_stmt.value().execute();
                    }
                }
                execute("COMMIT;");
            }
        }

        // Set version 1
        auto ver_stmt = prepare("INSERT OR REPLACE INTO schema_version (version, applied_at) VALUES (1, ?);");
        if (ver_stmt.is_ok()) {
            ver_stmt.value().bind_text(1, current_iso_timestamp());
            ver_stmt.value().execute();
        }
    }

    return Result<void>::ok();
}

Result<void> Database::reset_demo_data() {
    std::lock_guard<std::mutex> lock(db_mutex_);
    execute("BEGIN TRANSACTION;");
    execute("DELETE FROM file_blocks;");
    execute("DELETE FROM files;");
    execute("UPDATE block_allocations SET is_allocated = 0, owner_file_id = NULL, block_index_in_file = -1, updated_at = datetime('now');");
    execute("DELETE FROM operation_logs;");
    execute("DELETE FROM recovery_records;");
    execute("DELETE FROM saved_experiments;");
    execute("COMMIT;");

    return Result<void>::ok();
}

} // namespace osvault
