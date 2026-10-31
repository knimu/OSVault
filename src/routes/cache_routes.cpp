#include "osvault/server.hpp"

namespace osvault {

static crow::json::wvalue format_page_result(const PageReplacementResult& r) {
    crow::json::wvalue out;
    out["algorithm"] = r.algorithm;
    out["frame_count"] = r.frame_count;
    out["sequence"] = r.sequence;
    out["total_hits"] = r.total_hits;
    out["total_misses"] = r.total_misses;
    out["hit_ratio"] = r.hit_ratio;
    out["miss_ratio"] = r.miss_ratio;
    out["final_frames"] = r.final_frames;

    std::vector<crow::json::wvalue> step_list;
    for (const auto& s : r.steps) {
        crow::json::wvalue step;
        step["page"] = s.page;
        step["hit"] = s.hit;
        step["evicted_page"] = s.evicted_page;
        step["frames"] = s.frames;
        step_list.push_back(std::move(step));
    }
    out["steps"] = std::move(step_list);
    return out;
}

void register_cache_routes(crow::SimpleApp& app, ServerContext& ctx) {
    // Current cache statistics
    CROW_ROUTE(app, "/api/cache/stats")
    ([&ctx]() {
        crow::json::wvalue res;
        res["ok"] = true;
        res["policy"] = ctx.cache.policy();
        res["capacity"] = ctx.cache.capacity();
        res["entries"] = ctx.cache.size();
        res["hits"] = ctx.cache.hits();
        res["misses"] = ctx.cache.misses();
        res["evictions"] = ctx.cache.evictions();
        res["hit_ratio"] = ctx.cache.hit_ratio();
        return crow::response(200, res);
    });

    // Configure cache
    CROW_ROUTE(app, "/api/cache/config").methods(crow::HTTPMethod::Post)
    ([&ctx](const crow::request& req) {
        auto json = crow::json::load(req.body);
        if (!json) return crow::response(400, "Invalid JSON");

        if (json.has("policy")) {
            ctx.cache.set_policy(json["policy"].s());
        }
        if (json.has("capacity")) {
            ctx.cache.set_capacity(static_cast<size_t>(json["capacity"].i()));
        }

        crow::json::wvalue res;
        res["ok"] = true;
        res["policy"] = ctx.cache.policy();
        res["capacity"] = ctx.cache.capacity();
        return crow::response(200, res);
    });

    // Clear cache
    CROW_ROUTE(app, "/api/cache/clear").methods(crow::HTTPMethod::Post)
    ([&ctx]() {
        ctx.cache.clear();
        crow::json::wvalue res;
        res["ok"] = true;
        res["message"] = "In-memory file cache cleared.";
        return crow::response(200, res);
    });

    // Educational simulation for FIFO & LRU
    CROW_ROUTE(app, "/api/cache/simulate").methods(crow::HTTPMethod::Post)
    ([&ctx](const crow::request& req) {
        auto json = crow::json::load(req.body);
        if (!json || !json.has("sequence") || !json.has("frames")) {
            crow::json::wvalue err;
            err["ok"] = false;
            err["error"] = "Required: 'sequence' (array of integers) and 'frames' (integer).";
            return crow::response(400, err);
        }

        std::vector<int> seq;
        for (const auto& item : json["sequence"]) {
            seq.push_back(item.i());
        }
        int frames = json["frames"].i();
        std::string algo = json.has("algorithm") ? std::string(json["algorithm"].s()) : "BOTH";

        crow::json::wvalue res;
        res["ok"] = true;

        if (algo == "FIFO") {
            auto sim = CacheManager::simulate_fifo(seq, frames);
            if (sim.is_error()) return crow::response(sim.code(), sim.error());
            res["fifo"] = format_page_result(sim.value());
        } else if (algo == "LRU") {
            auto sim = CacheManager::simulate_lru(seq, frames);
            if (sim.is_error()) return crow::response(sim.code(), sim.error());
            res["lru"] = format_page_result(sim.value());
        } else {
            auto cmp = CacheManager::simulate_comparison(seq, frames);
            if (cmp.is_error()) return crow::response(cmp.code(), cmp.error());
            res["fifo"] = format_page_result(cmp.value().first);
            res["lru"] = format_page_result(cmp.value().second);
        }

        ctx.logger.log("CACHE_SIM", "Simulated page replacement for sequence of " +
                                  std::to_string(seq.size()) + " pages with " + std::to_string(frames) + " frames.");
        return crow::response(200, res);
    });
}

} // namespace osvault
