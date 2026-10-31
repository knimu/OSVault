#pragma once
#include <string>
#include <vector>
#include "osvault/database.hpp"
#include "osvault/integrity_checker.hpp"
#include "osvault/models.hpp"
#include "osvault/result.hpp"
#include "osvault/storage_engine.hpp"

namespace osvault {

class RecoveryManager {
public:
    RecoveryManager(Database& db, StorageEngine& storage, IntegrityChecker& integrity);

    Result<RecoveryExperimentReport> run_interrupted_transaction_demo(const std::string& filename);
    Result<RecoveryExperimentReport> run_restart_persistence_demo();

    Result<std::vector<RecoveryExperimentReport>> get_recovery_history();

private:
    Database& db_;
    StorageEngine& storage_;
    IntegrityChecker& integrity_;
};

} // namespace osvault
