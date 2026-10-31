#include "osvault/server.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace osvault {

static std::string get_mime_type(const std::string& path) {
    if (path.rfind(".html") != std::string::npos) return "text/html; charset=utf-8";
    if (path.rfind(".css") != std::string::npos) return "text/css; charset=utf-8";
    if (path.rfind(".js") != std::string::npos) return "application/javascript; charset=utf-8";
    if (path.rfind(".json") != std::string::npos) return "application/json; charset=utf-8";
    if (path.rfind(".svg") != std::string::npos) return "image/svg+xml";
    if (path.rfind(".png") != std::string::npos) return "image/png";
    if (path.rfind(".ico") != std::string::npos) return "image/x-icon";
    return "text/plain; charset=utf-8";
}

static crow::response serve_static_file(const fs::path& file_path) {
    if (!fs::exists(file_path) || !fs::is_regular_file(file_path)) {
        crow::json::wvalue err;
        err["ok"] = false;
        err["error"] = "File not found";
        return crow::response(404, err);
    }

    std::ifstream file(file_path, std::ios::binary);
    if (!file) {
        crow::json::wvalue err;
        err["ok"] = false;
        err["error"] = "Unable to read file";
        return crow::response(500, err);
    }

    std::ostringstream ss;
    ss << file.rdbuf();
    crow::response res(ss.str());
    res.set_header("Content-Type", get_mime_type(file_path.string()));
    return res;
}

void register_system_routes(crow::SimpleApp& app, ServerContext& ctx) {
    // Health check
    CROW_ROUTE(app, "/api/health")
    ([]() {
        crow::json::wvalue res;
        res["ok"] = true;
        res["status"] = "healthy";
        res["service"] = "OSVault Storage Engine & OS Lab";
        res["version"] = "1.0.0";
        return crow::response(200, res);
    });

    // System metrics
    CROW_ROUTE(app, "/api/metrics")
    ([&ctx]() {
        auto metrics_res = ctx.storage.get_system_metrics();
        if (metrics_res.is_error()) {
            crow::json::wvalue err;
            err["ok"] = false;
            err["error"] = metrics_res.error();
            return crow::response(metrics_res.code(), err);
        }

        const auto& m = metrics_res.value();
        crow::json::wvalue res;
        res["ok"] = true;
        res["total_capacity_bytes"] = m.total_capacity_bytes;
        res["block_size"] = m.block_size;
        res["total_blocks"] = m.total_blocks;
        res["allocated_blocks"] = m.allocated_blocks;
        res["free_blocks"] = m.free_blocks;
        res["storage_utilization_pct"] = m.storage_utilization_pct;
        res["file_count"] = m.file_count;
        res["total_bytes"] = m.total_bytes;
        res["cache_hits"] = ctx.cache.hits();
        res["cache_misses"] = ctx.cache.misses();
        res["cache_hit_ratio"] = ctx.cache.hit_ratio();
        res["recent_operations_count"] = m.recent_operations_count;

        auto check_res = ctx.integrity.run_check();
        res["integrity_healthy"] = check_res.is_ok() && check_res.value().is_healthy;

        return crow::response(200, res);
    });

    // Reset demo data
    CROW_ROUTE(app, "/api/system/reset-demo-data").methods(crow::HTTPMethod::Post)
    ([&ctx](const crow::request& req) {
        auto json = crow::json::load(req.body);
        if (!json || !json.has("confirm") || json["confirm"].s() != "RESET_CONFIRM") {
            crow::json::wvalue err;
            err["ok"] = false;
            err["error"] = "Confirmation token required: {\"confirm\": \"RESET_CONFIRM\"}";
            return crow::response(400, err);
        }

        ctx.cache.clear();
        auto res = ctx.db.reset_demo_data();
        if (res.is_error()) {
            crow::json::wvalue err;
            err["ok"] = false;
            err["error"] = res.error();
            return crow::response(500, err);
        }

        ctx.logger.log("SYSTEM_RESET", "Demo data reset confirmed and executed successfully.");

        crow::json::wvalue out;
        out["ok"] = true;
        out["message"] = "Demo data cleared and storage reset to 1024 free blocks.";
        return crow::response(200, out);
    });

    // Root page
    CROW_ROUTE(app, "/")
    ([&ctx]() {
        fs::path index_path = fs::path(ctx.config.web_dir) / "index.html";
        return serve_static_file(index_path);
    });

    // Static asset serving: CSS, JS, etc.
    CROW_ROUTE(app, "/assets/<path>")
    ([&ctx](const std::string& subpath) {
        fs::path asset_path = fs::path(ctx.config.web_dir) / "assets" / subpath;
        return serve_static_file(asset_path);
    });
}

} // namespace osvault
