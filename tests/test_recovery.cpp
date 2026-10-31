#include "doctest.h"
#include "osvault/block_allocator.hpp"
#include "osvault/database.hpp"
#include "osvault/integrity_checker.hpp"
#include "osvault/recovery_manager.hpp"
#include "osvault/storage_engine.hpp"
#include <filesystem>

namespace fs = std::filesystem;
using namespace osvault;

TEST_SUITE("recovery") {
    TEST_CASE("Interrupted transaction atomicity and rollback") {
        std::string db_file = "test_recovery_atomic.db";
        std::error_code ec;
        fs::remove(db_file, ec);

        Database db;
        CHECK(db.open(db_file).is_ok());
        BlockAllocator alloc(db, 1024);
        StorageEngine storage(db, alloc, 4096);
        IntegrityChecker checker(db, 1024);
        RecoveryManager rm(db, storage, checker);

        auto rep = rm.run_interrupted_transaction_demo("test_fault.dat");
        CHECK(rep.is_ok());
        CHECK(rep.value().recovered_cleanly == true);
        CHECK(rep.value().success == true);

        // Verify that no orphaned files or corrupt blocks exist
        auto chk = checker.run_check();
        CHECK(chk.is_ok());
        CHECK(chk.value().is_healthy == true);
        CHECK(chk.value().orphaned_blocks == 0);

        db.close();
        fs::remove(db_file, ec);
    }

    TEST_CASE("Restart persistence verification") {
        std::string db_file = "test_restart_durability.db";
        std::error_code ec;
        fs::remove(db_file, ec);

        // Phase 1: create file
        {
            Database db;
            CHECK(db.open(db_file).is_ok());
            BlockAllocator alloc(db, 1024);
            StorageEngine storage(db, alloc, 4096);

            auto cr = storage.create_file("persistent_note.txt", "Hello OSVault Crash Consistency!");
            CHECK(cr.is_ok());
            db.close();
        }

        // Phase 2: simulate restart by opening a new Database instance on same file
        {
            Database db;
            CHECK(db.open(db_file).is_ok());
            BlockAllocator alloc(db, 1024);
            StorageEngine storage(db, alloc, 4096);
            IntegrityChecker checker(db, 1024);

            auto rd = storage.read_file("persistent_note.txt");
            CHECK(rd.is_ok());
            CHECK(rd.value() == "Hello OSVault Crash Consistency!");

            auto chk = checker.run_check();
            CHECK(chk.is_ok());
            CHECK(chk.value().is_healthy == true);
            CHECK(chk.value().files_checked == 1);

            db.close();
        }

        fs::remove(db_file, ec);
    }
}
