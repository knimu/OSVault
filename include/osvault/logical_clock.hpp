#pragma once
#include <mutex>
#include <string>
#include <vector>
#include "osvault/models.hpp"
#include "osvault/result.hpp"

namespace osvault {

struct NodeClockState {
    std::string node_id;
    int lamport_clock = 0;
    std::vector<int> vector_clock;
};

class LogicalClockSystem {
public:
    explicit LogicalClockSystem(int node_count = 3);

    void reset(int node_count = 3);
    std::vector<NodeClockState> get_nodes();

    Result<LamportEvent> trigger_local_event(const std::string& node_id, const std::string& description);
    Result<std::pair<LamportEvent, LamportEvent>> send_message(const std::string& from_node, const std::string& to_node, const std::string& msg);

    std::vector<LamportEvent> get_timeline();

    static bool is_causally_dependent(const std::vector<int>& v1, const std::vector<int>& v2);
    static bool is_concurrent(const std::vector<int>& v1, const std::vector<int>& v2);

private:
    int get_node_index(const std::string& node_id) const;

    mutable std::mutex clock_mutex_;
    int node_count_{3};
    std::vector<NodeClockState> nodes_;
    std::vector<LamportEvent> timeline_;
    int next_event_id_{1};
};

} // namespace osvault
