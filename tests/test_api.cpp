#include "doctest.h"
#include "osvault/database.hpp"
#include "osvault/operation_logger.hpp"
#include "osvault/storage_engine.hpp"
#include <filesystem>

namespace fs = std::filesystem;
using namespace osvault;

TEST_SUITE("api") {
    TEST_CASE("Operation logs search and pagination") {
        std::string db_file = "test_api_logs.db";
        std::error_code ec;
        fs::remove(db_file, ec);

        Database db;
        CHECK(db.open(db_file).is_ok());
        OperationLogger logger(db);

        // Add 10 logs
        for (int i = 0; i < 10; ++i) {
            std::string type = (i % 2 == 0) ? "FILE_CREATE" : "BLOCK_ALLOC";
            logger.log(type, "Log test description number " + std::to_string(i), "SUCCESS", "ID_" + std::to_string(i));
        }

        // Query with pagination limit 4 offset 0
        LogQuery q1;
        q1.limit = 4;
        q1.offset = 0;
        auto r1 = logger.query_logs(q1);
        CHECK(r1.is_ok());
        CHECK(r1.value().total_count == 10);
        CHECK(r1.value().logs.size() == 4);

        // Query with event_type filter
        LogQuery q2;
        q2.event_type = "FILE_CREATE";
        q2.limit = 10;
        auto r2 = logger.query_logs(q2);
        CHECK(r2.is_ok());
        CHECK(r2.value().total_count == 5);
        CHECK(r2.value().logs.size() == 5);

        // Query with search term
        LogQuery q3;
        q3.search_term = "number 7";
        auto r3 = logger.query_logs(q3);
        CHECK(r3.is_ok());
        CHECK(r3.value().total_count == 1);
        CHECK(r3.value().logs[0].related_id == "ID_7");

        db.close();
        fs::remove(db_file, ec);
    }
}
