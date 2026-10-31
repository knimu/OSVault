#pragma once
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include "osvault/result.hpp"

struct sqlite3;
struct sqlite3_stmt;

namespace osvault {

class Statement {
public:
    explicit Statement(sqlite3_stmt* stmt = nullptr);
    ~Statement();

    Statement(const Statement&) = delete;
    Statement& operator=(const Statement&) = delete;
    Statement(Statement&& other) noexcept;
    Statement& operator=(Statement&& other) noexcept;

    void reset();
    void clear_bindings();
    bool step(); // returns true if SQLITE_ROW, false if SQLITE_DONE
    void execute(); // steps until done or error

    void bind_int(int index, int val);
    void bind_int64(int index, int64_t val);
    void bind_text(int index, const std::string& val);
    void bind_blob(int index, const void* data, int size);
    void bind_null(int index);

    int column_int(int col) const;
    int64_t column_int64(int col) const;
    std::string column_text(int col) const;
    std::string column_blob(int col) const;
    bool column_is_null(int col) const;

    sqlite3_stmt* get() const noexcept { return stmt_; }
    explicit operator bool() const noexcept { return stmt_ != nullptr; }

private:
    sqlite3_stmt* stmt_{nullptr};
};

class Database;

class Transaction {
public:
    explicit Transaction(Database& db);
    ~Transaction();

    Transaction(const Transaction&) = delete;
    Transaction& operator=(const Transaction&) = delete;

    void commit();
    void rollback();

private:
    Database& db_;
    bool committed_{false};
    bool active_{true};
};

class Database {
public:
    Database();
    ~Database();

    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    Result<void> open(const std::string& path);
    void close();

    Result<void> execute(const std::string& sql);
    Result<Statement> prepare(const std::string& sql);

    Result<void> reset_demo_data();
    int get_schema_version();

    sqlite3* raw_handle() const noexcept { return db_; }
    std::mutex& mutex() noexcept { return db_mutex_; }

private:
    Result<void> initialize_schema();

    sqlite3* db_{nullptr};
    std::mutex db_mutex_;
    std::string path_;
};

} // namespace osvault
