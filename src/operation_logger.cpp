#include "osvault/operation_logger.hpp"
#include <chrono>
#include <iomanip>
#include <sstream>

namespace osvault {

static std::string current_iso_time() {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&in_time_t), "%Y-%m-%dT%H:%M:%SZ");
    return ss.str();
}

OperationLogger::OperationLogger(Database& db) : db_(db) {}

Result<void> OperationLogger::log(
    const std::string& event_type, const std::string& description,
    const std::string& result, const std::string& related_id) {

    std::string now = current_iso_time();
    std::string sql = "INSERT INTO operation_logs (timestamp, event_type, description, result, related_id) VALUES (?, ?, ?, ?, ?);";

    std::lock_guard<std::mutex> lock(db_.mutex());
    auto stmt_res = db_.prepare(sql);
    if (stmt_res.is_error()) return Result<void>::failure(stmt_res.error());
    auto stmt = std::move(stmt_res.value());
    stmt.bind_text(1, now);
    stmt.bind_text(2, event_type);
    stmt.bind_text(3, description);
    stmt.bind_text(4, result);
    stmt.bind_text(5, related_id);
    stmt.execute();

    return Result<void>::ok();
}

Result<LogQueryResult> OperationLogger::query_logs(const LogQuery& query) {
    std::lock_guard<std::mutex> lock(db_.mutex());

    std::string count_sql = "SELECT COUNT(*) FROM operation_logs WHERE 1=1";
    std::string select_sql = "SELECT id, timestamp, event_type, description, result, related_id FROM operation_logs WHERE 1=1";
    std::vector<std::string> params;

    if (!query.event_type.empty() && query.event_type != "ALL") {
        count_sql += " AND event_type = ?";
        select_sql += " AND event_type = ?";
        params.push_back(query.event_type);
    }
    if (!query.search_term.empty()) {
        count_sql += " AND (description LIKE ? OR related_id LIKE ?)";
        select_sql += " AND (description LIKE ? OR related_id LIKE ?)";
        params.push_back("%" + query.search_term + "%");
        params.push_back("%" + query.search_term + "%");
    }

    select_sql += " ORDER BY id DESC LIMIT ? OFFSET ?;";

    // Get total count
    auto c_stmt_res = db_.prepare(count_sql);
    if (c_stmt_res.is_error()) return Result<LogQueryResult>::failure(c_stmt_res.error());
    auto c_stmt = std::move(c_stmt_res.value());
    for (size_t i = 0; i < params.size(); ++i) {
        c_stmt.bind_text(static_cast<int>(i + 1), params[i]);
    }
    int total = 0;
    if (c_stmt.step()) {
        total = c_stmt.column_int(0);
    }

    // Get page of rows
    auto s_stmt_res = db_.prepare(select_sql);
    if (s_stmt_res.is_error()) return Result<LogQueryResult>::failure(s_stmt_res.error());
    auto s_stmt = std::move(s_stmt_res.value());
    int param_idx = 1;
    for (const auto& p : params) {
        s_stmt.bind_text(param_idx++, p);
    }
    s_stmt.bind_int(param_idx++, query.limit > 0 ? query.limit : 50);
    s_stmt.bind_int(param_idx++, query.offset >= 0 ? query.offset : 0);

    LogQueryResult res;
    res.total_count = total;
    res.limit = query.limit;
    res.offset = query.offset;

    while (s_stmt.step()) {
        LogRecord r;
        r.id = s_stmt.column_int64(0);
        r.timestamp = s_stmt.column_text(1);
        r.event_type = s_stmt.column_text(2);
        r.description = s_stmt.column_text(3);
        r.result = s_stmt.column_text(4);
        r.related_id = s_stmt.column_text(5);
        res.logs.push_back(std::move(r));
    }

    return Result<LogQueryResult>::ok(std::move(res));
}

Result<void> OperationLogger::clear_logs() {
    std::lock_guard<std::mutex> lock(db_.mutex());
    return db_.execute("DELETE FROM operation_logs;");
}

} // namespace osvault
