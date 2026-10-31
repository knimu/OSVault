#pragma once
#include "crow_all.h"
#include "osvault/block_allocator.hpp"
#include "osvault/cache_manager.hpp"
#include "osvault/config.hpp"
#include "osvault/database.hpp"
#include "osvault/deadlock_detector.hpp"
#include "osvault/integrity_checker.hpp"
#include "osvault/ipc_simulator.hpp"
#include "osvault/logical_clock.hpp"
#include "osvault/memory_manager.hpp"
#include "osvault/operation_logger.hpp"
#include "osvault/recovery_manager.hpp"
#include "osvault/scheduler.hpp"
#include "osvault/storage_engine.hpp"
#include "osvault/synchronization.hpp"

namespace osvault {

struct ServerContext {
    Database& db;
    BlockAllocator& allocator;
    StorageEngine& storage;
    CacheManager& cache;
    MemoryManager& memory;
    SynchronizationSimulator& sync;
    IpcSimulator& ipc;
    LogicalClockSystem& clocks;
    OperationLogger& logger;
    IntegrityChecker& integrity;
    RecoveryManager& recovery;
    AppConfig& config;
};

void register_system_routes(crow::SimpleApp& app, ServerContext& ctx);
void register_file_routes(crow::SimpleApp& app, ServerContext& ctx);
void register_block_routes(crow::SimpleApp& app, ServerContext& ctx);
void register_cache_routes(crow::SimpleApp& app, ServerContext& ctx);
void register_scheduler_routes(crow::SimpleApp& app, ServerContext& ctx);
void register_memory_routes(crow::SimpleApp& app, ServerContext& ctx);
void register_synchronization_routes(crow::SimpleApp& app, ServerContext& ctx);
void register_deadlock_routes(crow::SimpleApp& app, ServerContext& ctx);
void register_ipc_routes(crow::SimpleApp& app, ServerContext& ctx);
void register_clock_routes(crow::SimpleApp& app, ServerContext& ctx);
void register_log_routes(crow::SimpleApp& app, ServerContext& ctx);
void register_recovery_routes(crow::SimpleApp& app, ServerContext& ctx);

void register_all_routes(crow::SimpleApp& app, ServerContext& ctx);

} // namespace osvault
