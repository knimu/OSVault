#pragma once
#include <string>
#include <vector>
#include "osvault/database.hpp"
#include "osvault/models.hpp"
#include "osvault/result.hpp"

namespace osvault {

struct LogQuery {
    std::string event_type;
    std::string search_term;
    int limit = 50;
    int offset = 0;
};

struct LogQueryResult {
    std::vector<LogRecord> logs;
    int total_count = 0;
    int limit = 50;
    int offset = 0;
};

class OperationLogger {
public:
    explicit OperationLogger(Database& db);

    Result<void> log(const std::string& event_type, const std::string& description,
                     const std::string& result = "SUCCESS", const std::string& related_id = "");

    Result<LogQueryResult> query_logs(const LogQuery& query);
    Result<void> clear_logs();

private:
    Database& db_;
};

} // namespace osvault
