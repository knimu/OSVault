#include "osvault/server.hpp"

namespace osvault {

static crow::json::wvalue format_scheduling_result(const SchedulingResult& r) {
    crow::json::wvalue out;
    out["algorithm"] = r.algorithm;
    out["time_quantum"] = r.time_quantum;
    out["avg_turnaround_time"] = r.avg_turnaround_time;
    out["avg_waiting_time"] = r.avg_waiting_time;
    out["avg_response_time"] = r.avg_response_time;
    out["total_time"] = r.total_time;
    out["idle_time"] = r.idle_time;

    std::vector<crow::json::wvalue> proc_list;
    for (const auto& p : r.processes) {
        crow::json::wvalue item;
        item["pid"] = p.pid;
        item["arrival_time"] = p.arrival_time;
        item["burst_time"] = p.burst_time;
        item["priority"] = p.priority;
        item["completion_time"] = p.completion_time;
        item["turnaround_time"] = p.turnaround_time;
        item["waiting_time"] = p.waiting_time;
        item["response_time"] = p.response_time;
        proc_list.push_back(std::move(item));
    }
    out["processes"] = std::move(proc_list);

    std::vector<crow::json::wvalue> gantt_list;
    for (const auto& g : r.gantt_chart) {
        crow::json::wvalue item;
        item["pid"] = g.pid;
        item["start_time"] = g.start_time;
        item["end_time"] = g.end_time;
        item["is_idle"] = g.is_idle;
        gantt_list.push_back(std::move(item));
    }
    out["gantt_chart"] = std::move(gantt_list);
    return out;
}

void register_scheduler_routes(crow::SimpleApp& app, ServerContext& ctx) {
    CROW_ROUTE(app, "/api/scheduler/simulate").methods(crow::HTTPMethod::Post)
    ([&ctx](const crow::request& req) {
        auto json = crow::json::load(req.body);
        if (!json || !json.has("processes")) {
            crow::json::wvalue err;
            err["ok"] = false;
            err["error"] = "Required JSON field: 'processes' (array of objects).";
            return crow::response(400, err);
        }

        std::vector<ProcessTask> tasks;
        for (const auto& p : json["processes"]) {
            ProcessTask t;
            t.pid = p.has("pid") ? p["pid"].s() : ("P" + std::to_string(tasks.size() + 1));
            t.arrival_time = p.has("arrival_time") ? p["arrival_time"].i() : 0;
            t.burst_time = p.has("burst_time") ? p["burst_time"].i() : 1;
            t.priority = p.has("priority") ? p["priority"].i() : 0;
            tasks.push_back(t);
        }

        int quantum = json.has("time_quantum") ? json["time_quantum"].i() : 2;
        std::string algo = json.has("algorithm") ? std::string(json["algorithm"].s()) : "ALL";

        crow::json::wvalue res;
        res["ok"] = true;

        if (algo == "FCFS") {
            auto r = ProcessScheduler::run_fcfs(tasks);
            if (r.is_error()) return crow::response(r.code(), r.error());
            res["result"] = format_scheduling_result(r.value());
        } else if (algo == "SJF") {
            auto r = ProcessScheduler::run_sjf_non_preemptive(tasks);
            if (r.is_error()) return crow::response(r.code(), r.error());
            res["result"] = format_scheduling_result(r.value());
        } else if (algo == "RR") {
            auto r = ProcessScheduler::run_round_robin(tasks, quantum);
            if (r.is_error()) return crow::response(r.code(), r.error());
            res["result"] = format_scheduling_result(r.value());
        } else if (algo == "PRIORITY") {
            auto r = ProcessScheduler::run_priority_non_preemptive(tasks);
            if (r.is_error()) return crow::response(r.code(), r.error());
            res["result"] = format_scheduling_result(r.value());
        } else {
            // Run all comparison
            auto cmp = ProcessScheduler::run_comparison(tasks, quantum);
            if (cmp.is_error()) return crow::response(cmp.code(), cmp.error());
            std::vector<crow::json::wvalue> cmp_list;
            for (const auto& r : cmp.value()) {
                cmp_list.push_back(format_scheduling_result(r));
            }
            res["comparison"] = std::move(cmp_list);
        }

        ctx.logger.log("SCHEDULER_SIM", "Simulated scheduling algorithm '" + algo + "' for " +
                                      std::to_string(tasks.size()) + " processes.");
        return crow::response(200, res);
    });
}

} // namespace osvault
