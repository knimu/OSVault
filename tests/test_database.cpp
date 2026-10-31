#include "doctest.h"
#include "osvault/database.hpp"
#include <filesystem>

namespace fs = std::filesystem;
using namespace osvault;

TEST_SUITE("database") {
    TEST_CASE("Database initialization and schema creation") {
        std::string db_file = "test_db_init.db";
        std::error_code ec;
        fs::remove(db_file, ec);

        Database db;
        auto res = db.open(db_file);
        CHECK(res.is_ok());
        CHECK(db.get_schema_version() == 1);

        // Verify block_allocations table has 1024 blocks
        auto stmt_res = db.prepare("SELECT COUNT(*) FROM block_allocations;");
        CHECK(stmt_res.is_ok());
        auto stmt = std::move(stmt_res.value());
        CHECK(stmt.step());
        CHECK(stmt.column_int(0) == 1024);

        db.close();
        fs::remove(db_file, ec);
    }

    TEST_CASE("Prepared statements and binding") {
        std::string db_file = "test_db_stmt.db";
        std::error_code ec;
        fs::remove(db_file, ec);

        Database db;
        CHECK(db.open(db_file).is_ok());

        auto ins_res = db.prepare("INSERT INTO operation_logs (timestamp, event_type, description, result, related_id) VALUES (?, ?, ?, ?, ?);");
        CHECK(ins_res.is_ok());
        auto stmt = std::move(ins_res.value());
        stmt.bind_text(1, "2026-10-31T10:00:00Z");
        stmt.bind_text(2, "TEST_EVENT");
        stmt.bind_text(3, "Prepared statement binding test");
        stmt.bind_text(4, "SUCCESS");
        stmt.bind_text(5, "ITEM_1");
        stmt.execute();

        auto q_res = db.prepare("SELECT event_type, description FROM operation_logs WHERE related_id = ?;");
        CHECK(q_res.is_ok());
        auto q_stmt = std::move(q_res.value());
        q_stmt.bind_text(1, "ITEM_1");
        CHECK(q_stmt.step());
        CHECK(q_stmt.column_text(0) == "TEST_EVENT");
        CHECK(q_stmt.column_text(1) == "Prepared statement binding test");

        db.close();
        fs::remove(db_file, ec);
    }

    TEST_CASE("Transaction rollback on failure") {
        std::string db_file = "test_db_tx.db";
        std::error_code ec;
        fs::remove(db_file, ec);

        Database db;
        CHECK(db.open(db_file).is_ok());

        // Check initial count
        auto count_rows = [&]() -> int {
            auto s_res = db.prepare("SELECT COUNT(*) FROM files;");
            auto s = std::move(s_res.value());
            s.step();
            return s.column_int(0);
        };

        CHECK(count_rows() == 0);

        try {
            Transaction tx(db);
            auto ins = db.prepare("INSERT INTO files (name, size, block_count, created_at, updated_at) VALUES ('tx_test.txt', 10, 1, 'now', 'now');");
            ins.value().execute();
            CHECK(count_rows() == 1);
            // Simulate error before commit
            throw std::runtime_error("Simulated failure inside transaction");
            tx.commit();
        } catch (...) {
            // Expected
        }

        // Must have rolled back to 0 rows
        CHECK(count_rows() == 0);

        db.close();
        fs::remove(db_file, ec);
    }
}
