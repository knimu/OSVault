#include "osvault/server.hpp"

namespace osvault {

static crow::json::wvalue format_deadlock_state(const DeadlockState& s) {
    crow::json::wvalue out;
    out["processes"] = s.processes;
    out["resource_types"] = s.resource_types;
    out["available"] = s.available;
    out["is_deadlock"] = s.is_deadlock;
    out["is_safe"] = s.is_safe;
    out["safe_sequence"] = s.safe_sequence;
    out["deadlocked_processes"] = s.deadlocked_processes;
    out["log_steps"] = s.log_steps;

    std::vector<crow::json::wvalue> alloc_matrix;
    for (const auto& row : s.allocation) {
        std::vector<crow::json::wvalue> r_vec;
        for (int val : row) r_vec.push_back(val);
        alloc_matrix.push_back(crow::json::wvalue(std::move(r_vec)));
    }
    out["allocation"] = std::move(alloc_matrix);

    std::vector<crow::json::wvalue> req_matrix;
    for (const auto& row : s.request) {
        std::vector<crow::json::wvalue> r_vec;
        for (int val : row) r_vec.push_back(val);
        req_matrix.push_back(crow::json::wvalue(std::move(r_vec)));
    }
    out["request"] = std::move(req_matrix);

    return out;
}

void register_deadlock_routes(crow::SimpleApp& app, ServerContext& ctx) {
    // Analyze custom state
    CROW_ROUTE(app, "/api/deadlock/analyze").methods(crow::HTTPMethod::Post)
    ([&ctx](const crow::request& req) {
        auto json = crow::json::load(req.body);
        if (!json || !json.has("processes") || !json.has("resources") ||
            !json.has("available") || !json.has("allocation") || !json.has("request")) {
            crow::json::wvalue err;
            err["ok"] = false;
            err["error"] = "Required: processes, resources, available, allocation, request.";
            return crow::response(400, err);
        }

        std::vector<std::string> procs;
        for (const auto& p : json["processes"]) procs.push_back(p.s());

        std::vector<std::string> res;
        for (const auto& r : json["resources"]) res.push_back(r.s());

        std::vector<int> avail;
        for (const auto& a : json["available"]) avail.push_back(a.i());

        std::vector<std::vector<int>> alloc;
        for (const auto& row : json["allocation"]) {
            std::vector<int> r;
            for (const auto& val : row) r.push_back(val.i());
            alloc.push_back(r);
        }

        std::vector<std::vector<int>> req_matrix;
        for (const auto& row : json["request"]) {
            std::vector<int> r;
            for (const auto& val : row) r.push_back(val.i());
            req_matrix.push_back(r);
        }

        auto result = DeadlockDetector::analyze(procs, res, avail, alloc, req_matrix);
        if (result.is_error()) {
            crow::json::wvalue err;
            err["ok"] = false;
            err["error"] = result.error();
            return crow::response(result.code(), err);
        }

        crow::json::wvalue resp;
        resp["ok"] = true;
        resp["result"] = format_deadlock_state(result.value());
        ctx.logger.log("DEADLOCK_CHECK", "Analyzed resource allocation state: " +
                                       std::string(result.value().is_deadlock ? "DEADLOCK" : "SAFE"));
        return crow::response(200, resp);
    });

    // Preset configurations
    CROW_ROUTE(app, "/api/deadlock/preset/<string>")
    ([&ctx](const std::string& type) {
        crow::json::wvalue resp;
        resp["ok"] = true;
        if (type == "safe") {
            resp["result"] = format_deadlock_state(DeadlockDetector::create_preset_safe());
        } else {
            resp["result"] = format_deadlock_state(DeadlockDetector::create_preset_deadlock());
        }
        return crow::response(200, resp);
    });

    // Simulated recovery
    CROW_ROUTE(app, "/api/deadlock/recover").methods(crow::HTTPMethod::Post)
    ([&ctx](const crow::request& req) {
        auto json = crow::json::load(req.body);
        if (!json || !json.has("state") || !json.has("terminate_process")) {
            crow::json::wvalue err;
            err["ok"] = false;
            err["error"] = "Required: 'state' object and 'terminate_process' string.";
            return crow::response(400, err);
        }

        auto s_json = json["state"];
        std::vector<std::string> procs;
        for (const auto& p : s_json["processes"]) procs.push_back(p.s());

        std::vector<std::string> res;
        for (const auto& r : s_json["resources"]) res.push_back(r.s());

        std::vector<int> avail;
        for (const auto& a : s_json["available"]) avail.push_back(a.i());

        std::vector<std::vector<int>> alloc;
        for (const auto& row : s_json["allocation"]) {
            std::vector<int> r;
            for (const auto& val : row) r.push_back(val.i());
            alloc.push_back(r);
        }

        std::vector<std::vector<int>> req_matrix;
        for (const auto& row : s_json["request"]) {
            std::vector<int> r;
            for (const auto& val : row) r.push_back(val.i());
            req_matrix.push_back(r);
        }

        auto initial_state = DeadlockDetector::analyze(procs, res, avail, alloc, req_matrix);
        if (initial_state.is_error()) return crow::response(initial_state.code(), initial_state.error());

        std::string proc_to_kill = json["terminate_process"].s();
        auto recovered = DeadlockDetector::simulate_recovery(initial_state.value(), proc_to_kill);
        if (recovered.is_error()) return crow::response(recovered.code(), recovered.error());

        crow::json::wvalue resp;
        resp["ok"] = true;
        resp["result"] = format_deadlock_state(recovered.value());
        ctx.logger.log("DEADLOCK_RECOVERY", "Simulated recovery: terminated " + proc_to_kill + " and released resources.");
        return crow::response(200, resp);
    });
}

} // namespace osvault
