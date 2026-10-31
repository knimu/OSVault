#include "osvault/server.hpp"

namespace osvault {

static crow::json::wvalue format_cs(const CriticalSectionState& s) {
    crow::json::wvalue out;
    out["thread_count"] = s.thread_count;
    out["thread_states"] = s.thread_states;
    out["current_in_cs"] = s.current_in_cs;
    out["turn"] = s.turn;
    std::vector<int> flags;
    for (bool b : s.flag) flags.push_back(b ? 1 : 0);
    out["flag"] = std::move(flags);
    out["log_history"] = s.log_history;
    return out;
}

static crow::json::wvalue format_pc(const ProducerConsumerState& s) {
    crow::json::wvalue out;
    out["buffer_capacity"] = s.buffer_capacity;
    out["buffer"] = s.buffer;
    out["in_ptr"] = s.in_ptr;
    out["out_ptr"] = s.out_ptr;
    out["empty_slots"] = s.empty_slots;
    out["full_slots"] = s.full_slots;
    out["producer_state"] = s.producer_state;
    out["consumer_state"] = s.consumer_state;
    out["log_history"] = s.log_history;
    return out;
}

static crow::json::wvalue format_rw(const ReadersWritersState& s) {
    crow::json::wvalue out;
    out["reader_count"] = s.reader_count;
    out["writer_active"] = s.writer_active;
    out["waiting_readers"] = s.waiting_readers;
    out["waiting_writers"] = s.waiting_writers;
    out["active_readers"] = s.active_readers;
    out["active_writer"] = s.active_writer;
    out["shared_resource_val"] = s.shared_resource_val;
    out["log_history"] = s.log_history;
    return out;
}

static crow::json::wvalue format_dp(const DiningPhilosophersState& s) {
    crow::json::wvalue out;
    out["states"] = s.states;
    std::vector<int> forks;
    for (bool f : s.forks) forks.push_back(f ? 1 : 0);
    out["forks"] = std::move(forks);
    out["meals_eaten"] = s.meals_eaten;
    out["log_history"] = s.log_history;
    return out;
}

void register_synchronization_routes(crow::SimpleApp& app, ServerContext& ctx) {
    // Critical Section
    CROW_ROUTE(app, "/api/sync/cs")
    ([&ctx]() {
        crow::json::wvalue res;
        res["ok"] = true;
        res["state"] = format_cs(ctx.sync.get_cs_state());
        return crow::response(200, res);
    });

    CROW_ROUTE(app, "/api/sync/cs/step").methods(crow::HTTPMethod::Post)
    ([&ctx](const crow::request& req) {
        auto json = crow::json::load(req.body);
        int tid = (json && json.has("thread_id")) ? json["thread_id"].i() : 0;
        auto step_res = ctx.sync.step_cs(tid);
        if (step_res.is_error()) return crow::response(step_res.code(), step_res.error());
        crow::json::wvalue res;
        res["ok"] = true;
        res["state"] = format_cs(step_res.value());
        return crow::response(200, res);
    });

    CROW_ROUTE(app, "/api/sync/cs/reset").methods(crow::HTTPMethod::Post)
    ([&ctx]() {
        ctx.sync.reset_cs();
        crow::json::wvalue res;
        res["ok"] = true;
        res["state"] = format_cs(ctx.sync.get_cs_state());
        return crow::response(200, res);
    });

    // Producer Consumer
    CROW_ROUTE(app, "/api/sync/pc")
    ([&ctx]() {
        crow::json::wvalue res;
        res["ok"] = true;
        res["state"] = format_pc(ctx.sync.get_pc_state());
        return crow::response(200, res);
    });

    CROW_ROUTE(app, "/api/sync/pc/produce").methods(crow::HTTPMethod::Post)
    ([&ctx](const crow::request& req) {
        auto json = crow::json::load(req.body);
        static int default_item = 100;
        int item = (json && json.has("item")) ? json["item"].i() : ++default_item;
        auto p_res = ctx.sync.step_produce(item);
        crow::json::wvalue res;
        res["ok"] = true;
        res["state"] = format_pc(p_res.value());
        return crow::response(200, res);
    });

    CROW_ROUTE(app, "/api/sync/pc/consume").methods(crow::HTTPMethod::Post)
    ([&ctx]() {
        auto c_res = ctx.sync.step_consume();
        crow::json::wvalue res;
        res["ok"] = true;
        res["state"] = format_pc(c_res.value());
        return crow::response(200, res);
    });

    CROW_ROUTE(app, "/api/sync/pc/reset").methods(crow::HTTPMethod::Post)
    ([&ctx](const crow::request& req) {
        auto json = crow::json::load(req.body);
        int cap = (json && json.has("capacity")) ? json["capacity"].i() : 5;
        ctx.sync.reset_pc(cap);
        crow::json::wvalue res;
        res["ok"] = true;
        res["state"] = format_pc(ctx.sync.get_pc_state());
        return crow::response(200, res);
    });

    // Readers Writers
    CROW_ROUTE(app, "/api/sync/rw")
    ([&ctx]() {
        crow::json::wvalue res;
        res["ok"] = true;
        res["state"] = format_rw(ctx.sync.get_rw_state());
        return crow::response(200, res);
    });

    CROW_ROUTE(app, "/api/sync/rw/read/start").methods(crow::HTTPMethod::Post)
    ([&ctx](const crow::request& req) {
        auto json = crow::json::load(req.body);
        std::string rid = (json && json.has("reader_id")) ? std::string(json["reader_id"].s()) : "R1";
        auto r = ctx.sync.start_read(rid);
        crow::json::wvalue res;
        res["ok"] = true;
        res["state"] = format_rw(r.value());
        return crow::response(200, res);
    });

    CROW_ROUTE(app, "/api/sync/rw/read/stop").methods(crow::HTTPMethod::Post)
    ([&ctx](const crow::request& req) {
        auto json = crow::json::load(req.body);
        std::string rid = (json && json.has("reader_id")) ? std::string(json["reader_id"].s()) : "R1";
        auto r = ctx.sync.stop_read(rid);
        crow::json::wvalue res;
        res["ok"] = true;
        res["state"] = format_rw(r.value());
        return crow::response(200, res);
    });

    CROW_ROUTE(app, "/api/sync/rw/write/start").methods(crow::HTTPMethod::Post)
    ([&ctx](const crow::request& req) {
        auto json = crow::json::load(req.body);
        std::string wid = (json && json.has("writer_id")) ? std::string(json["writer_id"].s()) : "W1";
        auto r = ctx.sync.start_write(wid);
        crow::json::wvalue res;
        res["ok"] = true;
        res["state"] = format_rw(r.value());
        return crow::response(200, res);
    });

    CROW_ROUTE(app, "/api/sync/rw/write/stop").methods(crow::HTTPMethod::Post)
    ([&ctx](const crow::request& req) {
        auto json = crow::json::load(req.body);
        std::string wid = (json && json.has("writer_id")) ? std::string(json["writer_id"].s()) : "W1";
        int val = (json && json.has("value")) ? json["value"].i() : 200;
        auto r = ctx.sync.stop_write(wid, val);
        crow::json::wvalue res;
        res["ok"] = true;
        res["state"] = format_rw(r.value());
        return crow::response(200, res);
    });

    CROW_ROUTE(app, "/api/sync/rw/reset").methods(crow::HTTPMethod::Post)
    ([&ctx]() {
        ctx.sync.reset_rw();
        crow::json::wvalue res;
        res["ok"] = true;
        res["state"] = format_rw(ctx.sync.get_rw_state());
        return crow::response(200, res);
    });

    // Dining Philosophers
    CROW_ROUTE(app, "/api/sync/dp")
    ([&ctx]() {
        crow::json::wvalue res;
        res["ok"] = true;
        res["state"] = format_dp(ctx.sync.get_dp_state());
        return crow::response(200, res);
    });

    CROW_ROUTE(app, "/api/sync/dp/step").methods(crow::HTTPMethod::Post)
    ([&ctx](const crow::request& req) {
        auto json = crow::json::load(req.body);
        int pid = (json && json.has("philosopher_id")) ? json["philosopher_id"].i() : 0;
        auto r = ctx.sync.step_philosopher(pid);
        if (r.is_error()) return crow::response(r.code(), r.error());
        crow::json::wvalue res;
        res["ok"] = true;
        res["state"] = format_dp(r.value());
        return crow::response(200, res);
    });

    CROW_ROUTE(app, "/api/sync/dp/reset").methods(crow::HTTPMethod::Post)
    ([&ctx]() {
        ctx.sync.reset_dp();
        crow::json::wvalue res;
        res["ok"] = true;
        res["state"] = format_dp(ctx.sync.get_dp_state());
        return crow::response(200, res);
    });
}

} // namespace osvault
