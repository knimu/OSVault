#include "osvault/server.hpp"

namespace osvault {

static crow::json::wvalue format_lamport_event(const LamportEvent& e) {
    crow::json::wvalue out;
    out["event_id"] = e.event_id;
    out["node_id"] = e.node_id;
    out["event_type"] = e.event_type;
    out["description"] = e.description;
    out["lamport_clock"] = e.lamport_clock;
    out["vector_clock"] = e.vector_clock;
    return out;
}

void register_clock_routes(crow::SimpleApp& app, ServerContext& ctx) {
    // Get nodes
    CROW_ROUTE(app, "/api/clocks/nodes")
    ([&ctx]() {
        auto nodes = ctx.clocks.get_nodes();
        std::vector<crow::json::wvalue> list;
        for (const auto& n : nodes) {
            crow::json::wvalue item;
            item["node_id"] = n.node_id;
            item["lamport_clock"] = n.lamport_clock;
            item["vector_clock"] = n.vector_clock;
            list.push_back(std::move(item));
        }
        crow::json::wvalue res;
        res["ok"] = true;
        res["nodes"] = std::move(list);
        return crow::response(200, res);
    });

    // Get event timeline
    CROW_ROUTE(app, "/api/clocks/timeline")
    ([&ctx]() {
        auto timeline = ctx.clocks.get_timeline();
        std::vector<crow::json::wvalue> list;
        for (const auto& e : timeline) {
            list.push_back(format_lamport_event(e));
        }
        crow::json::wvalue res;
        res["ok"] = true;
        res["timeline"] = std::move(list);
        return crow::response(200, res);
    });

    // Trigger local event
    CROW_ROUTE(app, "/api/clocks/local-event").methods(crow::HTTPMethod::Post)
    ([&ctx](const crow::request& req) {
        auto json = crow::json::load(req.body);
        if (!json || !json.has("node_id")) return crow::response(400, "Required: 'node_id'");
        std::string desc = json.has("description") ? std::string(json["description"].s()) : "";
        auto r = ctx.clocks.trigger_local_event(json["node_id"].s(), desc);
        if (r.is_error()) return crow::response(r.code(), r.error());
        crow::json::wvalue res;
        res["ok"] = true;
        res["event"] = format_lamport_event(r.value());
        return crow::response(200, res);
    });

    // Send message between nodes
    CROW_ROUTE(app, "/api/clocks/send-message").methods(crow::HTTPMethod::Post)
    ([&ctx](const crow::request& req) {
        auto json = crow::json::load(req.body);
        if (!json || !json.has("from_node") || !json.has("to_node")) {
            return crow::response(400, "Required: 'from_node' and 'to_node'");
        }
        std::string msg = json.has("message") ? std::string(json["message"].s()) : "Message Payload";
        auto r = ctx.clocks.send_message(json["from_node"].s(), json["to_node"].s(), msg);
        if (r.is_error()) return crow::response(r.code(), r.error());
        crow::json::wvalue res;
        res["ok"] = true;
        res["send_event"] = format_lamport_event(r.value().first);
        res["receive_event"] = format_lamport_event(r.value().second);
        return crow::response(200, res);
    });

    // Reset logical clocks
    CROW_ROUTE(app, "/api/clocks/reset").methods(crow::HTTPMethod::Post)
    ([&ctx](const crow::request& req) {
        auto json = crow::json::load(req.body);
        int node_count = (json && json.has("node_count")) ? json["node_count"].i() : 3;
        ctx.clocks.reset(node_count);
        crow::json::wvalue res;
        res["ok"] = true;
        res["message"] = "Logical clock simulation reset with " + std::to_string(node_count) + " nodes.";
        return crow::response(200, res);
    });

    // Check concurrency between two vector clocks
    CROW_ROUTE(app, "/api/clocks/concurrency-check").methods(crow::HTTPMethod::Post)
    ([&ctx](const crow::request& req) {
        auto json = crow::json::load(req.body);
        if (!json || !json.has("v1") || !json.has("v2")) return crow::response(400, "Required: 'v1' and 'v2'");
        std::vector<int> v1, v2;
        for (const auto& val : json["v1"]) v1.push_back(val.i());
        for (const auto& val : json["v2"]) v2.push_back(val.i());

        bool v1_causal_v2 = LogicalClockSystem::is_causally_dependent(v1, v2);
        bool v2_causal_v1 = LogicalClockSystem::is_causally_dependent(v2, v1);
        bool concurrent = LogicalClockSystem::is_concurrent(v1, v2);

        crow::json::wvalue res;
        res["ok"] = true;
        res["v1_happened_before_v2"] = v1_causal_v2;
        res["v2_happened_before_v1"] = v2_causal_v1;
        res["is_concurrent"] = concurrent;
        return crow::response(200, res);
    });
}

} // namespace osvault
