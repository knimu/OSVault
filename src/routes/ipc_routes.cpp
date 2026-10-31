#include "osvault/server.hpp"

namespace osvault {

static crow::json::wvalue format_ipc_message(const IpcMessage& m) {
    crow::json::wvalue out;
    out["id"] = m.id;
    out["timestamp"] = m.timestamp;
    out["channel_type"] = m.channel_type;
    out["sender"] = m.sender;
    out["receiver"] = m.receiver;
    out["message"] = m.message;
    out["delivered"] = m.delivered;
    return out;
}

void register_ipc_routes(crow::SimpleApp& app, ServerContext& ctx) {
    // Pipe
    CROW_ROUTE(app, "/api/ipc/pipe")
    ([&ctx]() {
        auto state = ctx.ipc.get_pipe_state();
        crow::json::wvalue res;
        res["ok"] = true;
        res["buffer"] = state.buffer;
        res["bytes_used"] = state.buffer.size();
        res["capacity"] = state.capacity;
        res["write_open"] = state.write_open;
        res["read_open"] = state.read_open;
        return crow::response(200, res);
    });

    CROW_ROUTE(app, "/api/ipc/pipe/write").methods(crow::HTTPMethod::Post)
    ([&ctx](const crow::request& req) {
        auto json = crow::json::load(req.body);
        if (!json || !json.has("data")) return crow::response(400, "Required: 'data'");
        auto r = ctx.ipc.pipe_write(json["data"].s());
        if (r.is_error()) return crow::response(r.code(), r.error());
        auto state = ctx.ipc.get_pipe_state();
        crow::json::wvalue res;
        res["ok"] = true;
        res["buffer"] = state.buffer;
        res["bytes_used"] = state.buffer.size();
        return crow::response(200, res);
    });

    CROW_ROUTE(app, "/api/ipc/pipe/read").methods(crow::HTTPMethod::Post)
    ([&ctx](const crow::request& req) {
        auto json = crow::json::load(req.body);
        size_t bytes = (json && json.has("bytes")) ? static_cast<size_t>(json["bytes"].i()) : 256;
        auto r = ctx.ipc.pipe_read(bytes);
        if (r.is_error()) return crow::response(r.code(), r.error());
        auto state = ctx.ipc.get_pipe_state();
        crow::json::wvalue res;
        res["ok"] = true;
        res["data"] = r.value();
        res["remaining_buffer"] = state.buffer;
        res["bytes_used"] = state.buffer.size();
        return crow::response(200, res);
    });

    CROW_ROUTE(app, "/api/ipc/pipe/reset").methods(crow::HTTPMethod::Post)
    ([&ctx]() {
        ctx.ipc.reset_pipe();
        crow::json::wvalue res;
        res["ok"] = true;
        res["message"] = "Pipe reset.";
        return crow::response(200, res);
    });

    // Message Queue
    CROW_ROUTE(app, "/api/ipc/mq")
    ([&ctx]() {
        auto msgs = ctx.ipc.get_mq_messages();
        std::vector<crow::json::wvalue> list;
        for (const auto& m : msgs) list.push_back(format_ipc_message(m));
        crow::json::wvalue res;
        res["ok"] = true;
        res["messages"] = std::move(list);
        return crow::response(200, res);
    });

    CROW_ROUTE(app, "/api/ipc/mq/send").methods(crow::HTTPMethod::Post)
    ([&ctx](const crow::request& req) {
        auto json = crow::json::load(req.body);
        if (!json || !json.has("sender") || !json.has("receiver") || !json.has("message")) {
            return crow::response(400, "Required: sender, receiver, message");
        }
        auto r = ctx.ipc.mq_send(json["sender"].s(), json["receiver"].s(), json["message"].s());
        if (r.is_error()) return crow::response(r.code(), r.error());
        crow::json::wvalue res;
        res["ok"] = true;
        res["message"] = "Message enqueued.";
        return crow::response(200, res);
    });

    CROW_ROUTE(app, "/api/ipc/mq/receive").methods(crow::HTTPMethod::Post)
    ([&ctx](const crow::request& req) {
        auto json = crow::json::load(req.body);
        if (!json || !json.has("receiver")) return crow::response(400, "Required: receiver");
        auto r = ctx.ipc.mq_receive(json["receiver"].s());
        if (r.is_error()) return crow::response(r.code(), r.error());
        crow::json::wvalue res;
        res["ok"] = true;
        res["received"] = format_ipc_message(r.value());
        return crow::response(200, res);
    });

    CROW_ROUTE(app, "/api/ipc/mq/clear").methods(crow::HTTPMethod::Post)
    ([&ctx]() {
        ctx.ipc.clear_mq();
        crow::json::wvalue res;
        res["ok"] = true;
        return crow::response(200, res);
    });

    // Shared Memory
    CROW_ROUTE(app, "/api/ipc/shm")
    ([&ctx]() {
        auto state = ctx.ipc.get_shm_state();
        crow::json::wvalue res;
        res["ok"] = true;
        res["segment_id"] = state.segment_id;
        res["content"] = state.content;
        res["locked"] = state.locked;
        res["locked_by"] = state.locked_by;
        return crow::response(200, res);
    });

    CROW_ROUTE(app, "/api/ipc/shm/read").methods(crow::HTTPMethod::Post)
    ([&ctx](const crow::request& req) {
        auto json = crow::json::load(req.body);
        std::string reader = (json && json.has("reader")) ? std::string(json["reader"].s()) : "ReaderProcess";
        auto r = ctx.ipc.shm_read(reader);
        crow::json::wvalue res;
        res["ok"] = true;
        res["content"] = r.value();
        return crow::response(200, res);
    });

    CROW_ROUTE(app, "/api/ipc/shm/write").methods(crow::HTTPMethod::Post)
    ([&ctx](const crow::request& req) {
        auto json = crow::json::load(req.body);
        if (!json || !json.has("data")) return crow::response(400, "Required: data");
        std::string writer = json.has("writer") ? std::string(json["writer"].s()) : "WriterProcess";
        auto r = ctx.ipc.shm_write(writer, json["data"].s());
        if (r.is_error()) return crow::response(r.code(), r.error());
        auto state = ctx.ipc.get_shm_state();
        crow::json::wvalue res;
        res["ok"] = true;
        res["content"] = state.content;
        return crow::response(200, res);
    });

    CROW_ROUTE(app, "/api/ipc/shm/lock").methods(crow::HTTPMethod::Post)
    ([&ctx](const crow::request& req) {
        auto json = crow::json::load(req.body);
        std::string proc = (json && json.has("process")) ? std::string(json["process"].s()) : "ProcessA";
        auto r = ctx.ipc.shm_lock(proc);
        if (r.is_error()) return crow::response(r.code(), r.error());
        crow::json::wvalue res;
        res["ok"] = true;
        res["locked"] = true;
        res["locked_by"] = proc;
        return crow::response(200, res);
    });

    CROW_ROUTE(app, "/api/ipc/shm/unlock").methods(crow::HTTPMethod::Post)
    ([&ctx](const crow::request& req) {
        auto json = crow::json::load(req.body);
        std::string proc = (json && json.has("process")) ? std::string(json["process"].s()) : "ProcessA";
        auto r = ctx.ipc.shm_unlock(proc);
        if (r.is_error()) return crow::response(r.code(), r.error());
        crow::json::wvalue res;
        res["ok"] = true;
        res["locked"] = false;
        return crow::response(200, res);
    });

    // Client-Server Socket Exchange
    CROW_ROUTE(app, "/api/ipc/exchange").methods(crow::HTTPMethod::Post)
    ([&ctx](const crow::request& req) {
        auto json = crow::json::load(req.body);
        std::string client = (json && json.has("client")) ? std::string(json["client"].s()) : "Client_1";
        std::string request_str = (json && json.has("request")) ? std::string(json["request"].s()) : "GET /status HTTP/1.1";
        auto r = ctx.ipc.client_server_exchange(client, request_str);
        crow::json::wvalue res;
        res["ok"] = true;
        res["request"] = format_ipc_message(r.value().first);
        res["response"] = format_ipc_message(r.value().second);
        return crow::response(200, res);
    });

    // IPC event history
    CROW_ROUTE(app, "/api/ipc/history")
    ([&ctx]() {
        auto hist = ctx.ipc.get_history();
        std::vector<crow::json::wvalue> list;
        for (const auto& m : hist) list.push_back(format_ipc_message(m));
        crow::json::wvalue res;
        res["ok"] = true;
        res["history"] = std::move(list);
        return crow::response(200, res);
    });
}

} // namespace osvault
