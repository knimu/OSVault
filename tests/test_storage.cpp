#include "doctest.h"
#include "osvault/block_allocator.hpp"
#include "osvault/database.hpp"
#include "osvault/storage_engine.hpp"
#include <filesystem>

namespace fs = std::filesystem;
using namespace osvault;

TEST_SUITE("storage") {
    TEST_CASE("File payload maps to allocated logical blocks") {
        std::string db_file = "test_storage_blocks.db";
        std::error_code ec;
        fs::remove(db_file, ec);

        Database db;
        CHECK(db.open(db_file).is_ok());
        BlockAllocator alloc(db, 1024);
        StorageEngine engine(db, alloc, 4096);

        std::string payload(4097, 'Z');
        auto file = engine.create_file("mapped.bin", payload);
        CHECK(file.is_ok());
        CHECK(file.value().block_count == 2);
        CHECK(file.value().block_ids.size() == 2);
        CHECK(file.value().size == payload.size());
        CHECK(file.value().block_count == (payload.size() + 4095) / 4096);

        CHECK(alloc.get_allocated_block_count().value() == 2);
        auto block0 = alloc.get_block(file.value().block_ids[0]);
        CHECK(block0.is_ok());
        CHECK(block0.value().is_allocated);
        CHECK(block0.value().owner_file_id == file.value().id);
        CHECK(block0.value().payload.size() == 4096);

        auto block1 = alloc.get_block(file.value().block_ids[1]);
        CHECK(block1.is_ok());
        CHECK(block1.value().is_allocated);
        CHECK(block1.value().owner_file_id == file.value().id);
        CHECK(block1.value().payload == "Z");

        CHECK(engine.delete_file("mapped.bin").is_ok());
        CHECK(alloc.get_allocated_block_count().value() == 0);

        db.close();
        fs::remove(db_file, ec);
    }

    TEST_CASE("Block allocation strategies select real free runs") {
        std::string db_file = "test_storage_strategy.db";
        std::error_code ec;
        fs::remove(db_file, ec);

        Database db;
        CHECK(db.open(db_file).is_ok());
        BlockAllocator alloc(db, 1024);

        alloc.set_strategy("BEST_FIT");
        auto blocks = alloc.allocate_blocks(2, 77);
        CHECK(blocks.is_ok());
        CHECK(blocks.value() == std::vector<int>{0, 1});

        db.close();
        fs::remove(db_file, ec);
    }

    TEST_CASE("Persistent directory and nested file operations") {
        std::string db_file = "test_storage_directories.db";
        std::error_code ec;
        fs::remove(db_file, ec);

        Database db;
        CHECK(db.open(db_file).is_ok());
        BlockAllocator alloc(db, 1024);
        StorageEngine engine(db, alloc, 4096);

        auto dir = engine.create_directory("documents", 0);
        CHECK(dir.is_ok());
        CHECK(dir.value().name == "documents");

        auto nested_dir = engine.create_directory("notes", dir.value().id);
        CHECK(nested_dir.is_ok());

        auto file = engine.create_file("todo.txt", "first", dir.value().id);
        CHECK(file.is_ok());
        CHECK(file.value().directory_id == dir.value().id);

        auto read = engine.read_file("todo.txt", dir.value().id);
        CHECK(read.is_ok());
        CHECK(read.value() == "first");

        CHECK(engine.write_file("todo.txt", "updated", dir.value().id).is_ok());
        CHECK(engine.read_file("todo.txt", dir.value().id).value() == "updated");

        auto list = engine.list_directory(dir.value().id);
        CHECK(list.is_ok());
        CHECK(list.value().size() == 1);

        CHECK(engine.delete_directory(dir.value().id).is_error());
        CHECK(engine.delete_file("todo.txt", dir.value().id).is_ok());
        CHECK(engine.delete_directory(dir.value().id).is_error()); // Child directory must be removed first
        CHECK(engine.delete_directory(nested_dir.value().id).is_ok());
        CHECK(engine.delete_directory(dir.value().id).is_ok());

        db.close();
        fs::remove(db_file, ec);
    }

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
