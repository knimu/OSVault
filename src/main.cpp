#define CROW_MAIN
#include "crow_all.h"
#include "osvault/config.hpp"
#include "osvault/database.hpp"
#include "osvault/server.hpp"
#include <iostream>

using namespace osvault;

int main(int argc, char* argv[]) {
    std::cout << "=========================================================\n"
              << " OSVault - Interactive Operating Systems Laboratory\n"
              << " & Crash-Consistent Block Storage Engine\n"
              << "=========================================================\n";

    AppConfig config = AppConfig::parse_args(argc, argv);

    std::cout << "[INIT] Initializing SQLite database at: " << config.db_path << "\n";
    Database db;
    auto db_res = db.open(config.db_path);
    if (db_res.is_error()) {
        std::cerr << "[FATAL] Failed to initialize database: " << db_res.error() << "\n";
        return 1;
    }
    std::cout << "[INIT] Database schema verified (Schema version: " << db.get_schema_version() << ")\n";

    // Core engines
    BlockAllocator allocator(db, config.total_blocks);
    StorageEngine storage(db, allocator, config.block_size);
    CacheManager cache(16, "LRU");
    MemoryManager memory(1024);
    SynchronizationSimulator sync;
    IpcSimulator ipc;
    LogicalClockSystem clocks(3);
    OperationLogger logger(db);
    IntegrityChecker integrity(db, config.total_blocks);
    RecoveryManager recovery(db, storage, integrity);

    // Initial integrity audit on boot
    auto boot_audit = integrity.run_check();
    if (boot_audit.is_ok()) {
        std::cout << "[AUDIT] Initial block storage audit: "
                  << (boot_audit.value().is_healthy ? "HEALTHY" : "INCONSISTENCY DETECTED")
                  << " (" << boot_audit.value().files_checked << " files, "
                  << boot_audit.value().blocks_referenced << " blocks referenced)\n";
    }

    logger.log("SYSTEM_BOOT", "OSVault server initialized successfully on port " + std::to_string(config.port));

    ServerContext ctx{
        db,
        allocator,
        storage,
        cache,
        memory,
        sync,
        ipc,
        clocks,
        logger,
        integrity,
        recovery,
        config
    };

    crow::SimpleApp app;

    // Register all 13 modules' routes
    register_all_routes(app, ctx);

    std::cout << "[SERVER] OSVault Web GUI available at: http://127.0.0.1:" << config.port << "/\n";
    std::cout << "[SERVER] Health endpoint: http://127.0.0.1:" << config.port << "/api/health\n";
    std::cout << "[SERVER] Web assets root: " << config.web_dir << "\n";
    std::cout << "[SERVER] Press Ctrl+C to terminate.\n";

    try {
        app.port(config.port)
           .bindaddr(config.host)
           .multithreaded()
           .run();
    } catch (const std::exception& e) {
        std::cerr << "[FATAL] Server encountered runtime error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
