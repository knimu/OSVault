#include "osvault/server.hpp"

namespace osvault {

void register_log_routes(crow::SimpleApp& app, ServerContext& ctx) {
    CROW_ROUTE(app, "/api/logs")
    ([&ctx](const crow::request& req) {
        LogQuery q;
        if (req.url_params.get("event_type")) q.event_type = req.url_params.get("event_type");
        if (req.url_params.get("q")) q.search_term = req.url_params.get("q");
        if (req.url_params.get("limit")) {
            try { q.limit = std::stoi(req.url_params.get("limit")); } catch (...) {}
        }
        if (req.url_params.get("offset")) {
            try { q.offset = std::stoi(req.url_params.get("offset")); } catch (...) {}
        }

        auto res_query = ctx.logger.query_logs(q);
        if (res_query.is_error()) {
            crow::json::wvalue err;
            err["ok"] = false;
            err["error"] = res_query.error();
            return crow::response(500, err);
        }

        const auto& q_res = res_query.value();
        std::vector<crow::json::wvalue> log_items;
        for (const auto& l : q_res.logs) {
            crow::json::wvalue item;
            item["id"] = l.id;
            item["timestamp"] = l.timestamp;
            item["event_type"] = l.event_type;
            item["description"] = l.description;
            item["result"] = l.result;
            item["related_id"] = l.related_id;
            log_items.push_back(std::move(item));
        }

        crow::json::wvalue res;
        res["ok"] = true;
        res["total_count"] = q_res.total_count;
        res["limit"] = q_res.limit;
        res["offset"] = q_res.offset;
        res["logs"] = std::move(log_items);
        return crow::response(200, res);
    });

    CROW_ROUTE(app, "/api/logs/clear").methods(crow::HTTPMethod::Post)
    ([&ctx]() {
        auto r = ctx.logger.clear_logs();
        if (r.is_error()) return crow::response(500, r.error());
        crow::json::wvalue res;
        res["ok"] = true;
        res["message"] = "Operation logs cleared.";
        return crow::response(200, res);
    });
}

} // namespace osvault
