#include "osvault/server.hpp"

namespace osvault {

static crow::json::wvalue format_recovery_report(const RecoveryExperimentReport& r) {
    crow::json::wvalue out;
    out["experiment_name"] = r.experiment_name;
    out["success"] = r.success;
    out["description"] = r.description;
    out["before_state"] = r.before_state;
    out["injection_event"] = r.injection_event;
    out["after_state"] = r.after_state;
    out["recovered_cleanly"] = r.recovered_cleanly;
    out["recovery_details"] = r.recovery_details;
    return out;
}

void register_recovery_routes(crow::SimpleApp& app, ServerContext& ctx) {
    // Run integrity check
    CROW_ROUTE(app, "/api/integrity/check")
    ([&ctx]() {
        auto chk_res = ctx.integrity.run_check();
        if (chk_res.is_error()) {
            crow::json::wvalue err;
            err["ok"] = false;
            err["error"] = chk_res.error();
            return crow::response(500, err);
        }

        const auto& r = chk_res.value();
        crow::json::wvalue res;
        res["ok"] = true;
        res["is_healthy"] = r.is_healthy;
        res["files_checked"] = r.files_checked;
        res["blocks_referenced"] = r.blocks_referenced;
        res["duplicate_references"] = r.duplicate_references;
        res["invalid_block_ids"] = r.invalid_block_ids;
        res["orphaned_blocks"] = r.orphaned_blocks;
        res["block_count_mismatches"] = r.block_count_mismatches;
        res["capacity_exceeded"] = r.capacity_exceeded;
        res["errors"] = r.errors;
        return crow::response(200, res);
    });

    // Fix orphaned blocks
    CROW_ROUTE(app, "/api/integrity/fix-orphans").methods(crow::HTTPMethod::Post)
    ([&ctx]() {
        auto fix_res = ctx.integrity.fix_orphaned_blocks();
        if (fix_res.is_error()) return crow::response(500, fix_res.error());
        crow::json::wvalue res;
        res["ok"] = true;
        res["message"] = "Orphaned allocations scanned and reconciled.";
        return crow::response(200, res);
    });

    // Interrupted transaction simulation
    CROW_ROUTE(app, "/api/recovery/interrupted-write").methods(crow::HTTPMethod::Post)
    ([&ctx](const crow::request& req) {
        auto json = crow::json::load(req.body);
        std::string fname = (json && json.has("filename")) ? std::string(json["filename"].s()) : "__interrupted_test.dat";
        auto rep = ctx.recovery.run_interrupted_transaction_demo(fname);
        if (rep.is_error()) return crow::response(500, rep.error());
        crow::json::wvalue res;
        res["ok"] = true;
        res["report"] = format_recovery_report(rep.value());
        return crow::response(200, res);
    });

    // Restart persistence verification
    CROW_ROUTE(app, "/api/recovery/restart-persistence").methods(crow::HTTPMethod::Post)
    ([&ctx]() {
        auto rep = ctx.recovery.run_restart_persistence_demo();
        if (rep.is_error()) return crow::response(500, rep.error());
        crow::json::wvalue res;
        res["ok"] = true;
        res["report"] = format_recovery_report(rep.value());
        return crow::response(200, res);
    });

    // Recovery records history
    CROW_ROUTE(app, "/api/recovery/history")
    ([&ctx]() {
        auto hist = ctx.recovery.get_recovery_history();
        crow::json::wvalue res;
        res["ok"] = true;
        std::vector<crow::json::wvalue> list;
        if (hist.is_ok()) {
            for (const auto& r : hist.value()) {
                list.push_back(format_recovery_report(r));
            }
        }
        res["history"] = std::move(list);
        return crow::response(200, res);
    });
}

void register_all_routes(crow::SimpleApp& app, ServerContext& ctx) {
    register_system_routes(app, ctx);
    register_file_routes(app, ctx);
    register_block_routes(app, ctx);
    register_cache_routes(app, ctx);
    register_scheduler_routes(app, ctx);
    register_memory_routes(app, ctx);
    register_synchronization_routes(app, ctx);
    register_deadlock_routes(app, ctx);
    register_ipc_routes(app, ctx);
    register_clock_routes(app, ctx);
    register_log_routes(app, ctx);
    register_recovery_routes(app, ctx);
}

} // namespace osvault
