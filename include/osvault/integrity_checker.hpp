#pragma once
#include <vector>
#include "osvault/database.hpp"
#include "osvault/models.hpp"
#include "osvault/result.hpp"

namespace osvault {

class IntegrityChecker {
public:
    explicit IntegrityChecker(Database& db, size_t total_blocks = 1024);

    Result<IntegrityReport> run_check();
    Result<void> fix_orphaned_blocks();

private:
    Database& db_;
    size_t total_blocks_;
};

} // namespace osvault
