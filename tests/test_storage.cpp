#include "doctest.h"
#include "osvault/block_allocator.hpp"
#include "osvault/database.hpp"
#include "osvault/storage_engine.hpp"
#include <filesystem>

namespace fs = std::filesystem;
using namespace osvault;

TEST_SUITE("storage") {
    TEST_CASE("Empty file creation and read") {
        std::string db_file = "test_storage_empty.db";
        std::error_code ec;
        fs::remove(db_file, ec);

        Database db;
        CHECK(db.open(db_file).is_ok());
        BlockAllocator alloc(db, 1024);
        StorageEngine engine(db, alloc, 4096);

        auto cr = engine.create_file("empty.txt", "");
        CHECK(cr.is_ok());
        CHECK(cr.value().size == 0);
        CHECK(cr.value().block_count == 0);
        CHECK(cr.value().block_ids.empty());

        auto read_res = engine.read_file("empty.txt");
        CHECK(read_res.is_ok());
        CHECK(read_res.value() == "");

        db.close();
        fs::remove(db_file, ec);
    }

    TEST_CASE("File CRUD and block boundaries") {
        std::string db_file = "test_storage_crud.db";
        std::error_code ec;
        fs::remove(db_file, ec);

        Database db;
        CHECK(db.open(db_file).is_ok());
        BlockAllocator alloc(db, 1024);
        StorageEngine engine(db, alloc, 4096);

        // 1. Exact block boundary: 4096 bytes
        std::string payload_4k(4096, 'A');
        auto cr_4k = engine.create_file("exact_4k.bin", payload_4k);
        CHECK(cr_4k.is_ok());
        CHECK(cr_4k.value().block_count == 1);
        CHECK(cr_4k.value().block_ids.size() == 1);

        auto rd_4k = engine.read_file("exact_4k.bin");
        CHECK(rd_4k.is_ok());
        CHECK(rd_4k.value() == payload_4k);

        // 2. Exact boundary + 1: 4097 bytes -> 2 blocks
        std::string payload_mult(4097, 'B');
        auto cr_mult = engine.create_file("multi.bin", payload_mult);
        CHECK(cr_mult.is_ok());
        CHECK(cr_mult.value().block_count == 2);
        CHECK(cr_mult.value().block_ids.size() == 2);

        auto rd_mult = engine.read_file("multi.bin");
        CHECK(rd_mult.is_ok());
        CHECK(rd_mult.value() == payload_mult);

        // Check metrics: 3 allocated blocks
        auto m = engine.get_system_metrics();
        CHECK(m.is_ok());
        CHECK(m.value().allocated_blocks == 3);
        CHECK(m.value().file_count == 2);

        // 3. Update file: overwrite with smaller content
        auto wr_res = engine.write_file("multi.bin", "small");
        CHECK(wr_res.is_ok());
        auto rd_small = engine.read_file("multi.bin");
        CHECK(rd_small.is_ok());
        CHECK(rd_small.value() == "small");

        // Now allocated blocks should be 1 (4k) + 1 (small) = 2
        auto m_after_wr = engine.get_system_metrics();
        CHECK(m_after_wr.value().allocated_blocks == 2);

        // 4. Rename file
        auto rn_res = engine.rename_file("multi.bin", "renamed.txt");
        CHECK(rn_res.is_ok());
        CHECK(engine.get_file_metadata("multi.bin").is_error());
        CHECK(engine.get_file_metadata("renamed.txt").is_ok());

        // 5. Delete file
        auto del_res = engine.delete_file("renamed.txt");
        CHECK(del_res.is_ok());
        CHECK(engine.get_file_metadata("renamed.txt").is_error());

        auto m_after_del = engine.get_system_metrics();
        CHECK(m_after_del.value().allocated_blocks == 1); // Only exact_4k remains

        db.close();
        fs::remove(db_file, ec);
    }

    TEST_CASE("Duplicate names and invalid names rejection") {
        std::string db_file = "test_storage_errors.db";
        std::error_code ec;
        fs::remove(db_file, ec);

        Database db;
        CHECK(db.open(db_file).is_ok());
        BlockAllocator alloc(db, 1024);
        StorageEngine engine(db, alloc, 4096);

        CHECK(engine.create_file("valid.txt", "abc").is_ok());

        // Duplicate
        auto dup = engine.create_file("valid.txt", "def");
        CHECK(dup.is_error());
        CHECK(dup.code() == 409);

        // Invalid names
        CHECK(engine.create_file("", "abc").is_error());
        CHECK(engine.create_file("../traversal.txt", "abc").is_error());
        CHECK(engine.create_file("bad/name.txt", "abc").is_error());

        db.close();
        fs::remove(db_file, ec);
    }
}
