#include "osvault/server.hpp"

namespace osvault {

static crow::json::wvalue format_memory_state(const MemoryAllocationResult& r) {
    crow::json::wvalue out;
    out["strategy"] = r.strategy;
    out["total_memory"] = r.total_memory;
    out["allocated_memory"] = r.allocated_memory;
    out["free_memory"] = r.free_memory;
    out["external_fragmentation"] = r.external_fragmentation;
    out["total_internal_fragmentation"] = r.total_internal_fragmentation;
    out["success"] = r.success;
    out["message"] = r.message;

    std::vector<crow::json::wvalue> part_list;
    for (const auto& p : r.partitions) {
        crow::json::wvalue part;
        part["id"] = p.id;
        part["start_address"] = p.start_address;
        part["size"] = p.size;
        part["is_allocated"] = p.is_allocated;
        part["process_name"] = p.process_name;
        part["internal_fragmentation"] = p.internal_fragmentation;
        part_list.push_back(std::move(part));
    }
    out["partitions"] = std::move(part_list);
    return out;
}

void register_memory_routes(crow::SimpleApp& app, ServerContext& ctx) {
    // Current memory layout
    CROW_ROUTE(app, "/api/memory/state")
    ([&ctx]() {
        crow::json::wvalue res;
        res["ok"] = true;
        res["state"] = format_memory_state(ctx.memory.get_state());
        return crow::response(200, res);
    });

    // Allocate memory
    CROW_ROUTE(app, "/api/memory/allocate").methods(crow::HTTPMethod::Post)
    ([&ctx](const crow::request& req) {
        auto json = crow::json::load(req.body);
        if (!json || !json.has("process_name") || !json.has("size")) {
            crow::json::wvalue err;
            err["ok"] = false;
            err["error"] = "Required: 'process_name' and 'size' (KB).";
            return crow::response(400, err);
        }

        std::string proc = json["process_name"].s();
        size_t size = static_cast<size_t>(json["size"].i());
        std::string strat = json.has("strategy") ? std::string(json["strategy"].s()) : "FIRST_FIT";

        Result<MemoryAllocationResult> alloc_res = (strat == "BEST_FIT")
            ? ctx.memory.allocate_best_fit(proc, size)
            : (strat == "WORST_FIT")
                ? ctx.memory.allocate_worst_fit(proc, size)
                : ctx.memory.allocate_first_fit(proc, size);

        if (alloc_res.is_error()) {
            crow::json::wvalue err;
            err["ok"] = false;
            err["error"] = alloc_res.error();
            err["state"] = format_memory_state(ctx.memory.get_state(strat));
            return crow::response(alloc_res.code(), err);
        }

        crow::json::wvalue res;
        res["ok"] = true;
        res["state"] = format_memory_state(alloc_res.value());
        ctx.logger.log("MEMORY_ALLOC", "Allocated " + std::to_string(size) + " KB for " + proc + " (" + strat + ")");
        return crow::response(200, res);
    });

    // Deallocate memory
    CROW_ROUTE(app, "/api/memory/deallocate").methods(crow::HTTPMethod::Post)
    ([&ctx](const crow::request& req) {
        auto json = crow::json::load(req.body);
        if (!json) return crow::response(400, "Invalid JSON");

        Result<MemoryAllocationResult> dealloc_res = Result<MemoryAllocationResult>::failure("Provide partition_id or process_name");
        if (json.has("partition_id")) {
            dealloc_res = ctx.memory.deallocate(json["partition_id"].i());
        } else if (json.has("process_name")) {
            dealloc_res = ctx.memory.deallocate_by_process(json["process_name"].s());
        }

        if (dealloc_res.is_error()) {
            crow::json::wvalue err;
            err["ok"] = false;
            err["error"] = dealloc_res.error();
            return crow::response(dealloc_res.code(), err);
        }

        crow::json::wvalue res;
        res["ok"] = true;
        res["state"] = format_memory_state(dealloc_res.value());
        ctx.logger.log("MEMORY_DEALLOC", "Deallocated memory partition.");
        return crow::response(200, res);
    });

    // Reset memory
    CROW_ROUTE(app, "/api/memory/reset").methods(crow::HTTPMethod::Post)
    ([&ctx](const crow::request& req) {
        auto json = crow::json::load(req.body);
        size_t total = (json && json.has("total_size")) ? static_cast<size_t>(json["total_size"].i()) : 1024;
        ctx.memory.reset(total);

        crow::json::wvalue res;
        res["ok"] = true;
        res["message"] = "Memory reset to " + std::to_string(total) + " KB contiguous space.";
        res["state"] = format_memory_state(ctx.memory.get_state());
        return crow::response(200, res);
    });
}

} // namespace osvault
