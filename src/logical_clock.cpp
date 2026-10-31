#include "osvault/logical_clock.hpp"
#include <algorithm>

namespace osvault {

LogicalClockSystem::LogicalClockSystem(int node_count) {
    reset(node_count);
}

void LogicalClockSystem::reset(int node_count) {
    std::lock_guard<std::mutex> lock(clock_mutex_);
    node_count_ = std::max(2, std::min(node_count, 10));
    nodes_.clear();
    timeline_.clear();
    next_event_id_ = 1;

    for (int i = 0; i < node_count_; ++i) {
        NodeClockState n;
        n.node_id = "Node_" + std::to_string(i);
        n.lamport_clock = 0;
        n.vector_clock = std::vector<int>(node_count_, 0);
        nodes_.push_back(n);
    }
}

int LogicalClockSystem::get_node_index(const std::string& node_id) const {
    for (size_t i = 0; i < nodes_.size(); ++i) {
        if (nodes_[i].node_id == node_id) return static_cast<int>(i);
    }
    return -1;
}

std::vector<NodeClockState> LogicalClockSystem::get_nodes() {
    std::lock_guard<std::mutex> lock(clock_mutex_);
    return nodes_;
}

std::vector<LamportEvent> LogicalClockSystem::get_timeline() {
    std::lock_guard<std::mutex> lock(clock_mutex_);
    return timeline_;
}

Result<LamportEvent> LogicalClockSystem::trigger_local_event(
    const std::string& node_id, const std::string& description) {
    std::lock_guard<std::mutex> lock(clock_mutex_);
    int idx = get_node_index(node_id);
    if (idx < 0) {
        return Result<LamportEvent>::failure("Node not found: " + node_id, 404);
    }

    // Rule 1: increment clock
    nodes_[idx].lamport_clock += 1;
    nodes_[idx].vector_clock[idx] += 1;

    LamportEvent ev;
    ev.event_id = next_event_id_++;
    ev.node_id = node_id;
    ev.event_type = "local";
    ev.description = description.empty() ? ("Local computation on " + node_id) : description;
    ev.lamport_clock = nodes_[idx].lamport_clock;
    ev.vector_clock = nodes_[idx].vector_clock;

    timeline_.push_back(ev);
    return Result<LamportEvent>::ok(ev);
}

Result<std::pair<LamportEvent, LamportEvent>> LogicalClockSystem::send_message(
    const std::string& from_node, const std::string& to_node, const std::string& msg) {
    std::lock_guard<std::mutex> lock(clock_mutex_);
    int src_idx = get_node_index(from_node);
    int dst_idx = get_node_index(to_node);

    if (src_idx < 0) return Result<std::pair<LamportEvent, LamportEvent>>::failure("Sender not found: " + from_node, 404);
    if (dst_idx < 0) return Result<std::pair<LamportEvent, LamportEvent>>::failure("Receiver not found: " + to_node, 404);
    if (src_idx == dst_idx) return Result<std::pair<LamportEvent, LamportEvent>>::failure("Sender and receiver cannot be the same node.", 400);

    // 1. Send event at sender
    nodes_[src_idx].lamport_clock += 1;
    nodes_[src_idx].vector_clock[src_idx] += 1;

    LamportEvent send_ev;
    send_ev.event_id = next_event_id_++;
    send_ev.node_id = from_node;
    send_ev.event_type = "send";
    send_ev.description = "Sent to " + to_node + ": '" + msg + "'";
    send_ev.lamport_clock = nodes_[src_idx].lamport_clock;
    send_ev.vector_clock = nodes_[src_idx].vector_clock;
    timeline_.push_back(send_ev);

    // 2. Receive event at receiver: C_j = max(C_j, C_send) + 1
    nodes_[dst_idx].lamport_clock = std::max(nodes_[dst_idx].lamport_clock, send_ev.lamport_clock) + 1;
    for (int i = 0; i < node_count_; ++i) {
        nodes_[dst_idx].vector_clock[i] = std::max(nodes_[dst_idx].vector_clock[i], send_ev.vector_clock[i]);
    }
    nodes_[dst_idx].vector_clock[dst_idx] += 1;

    LamportEvent recv_ev;
    recv_ev.event_id = next_event_id_++;
    recv_ev.node_id = to_node;
    recv_ev.event_type = "receive";
    recv_ev.description = "Received from " + from_node + ": '" + msg + "'";
    recv_ev.lamport_clock = nodes_[dst_idx].lamport_clock;
    recv_ev.vector_clock = nodes_[dst_idx].vector_clock;
    timeline_.push_back(recv_ev);

    return Result<std::pair<LamportEvent, LamportEvent>>::ok({send_ev, recv_ev});
}

bool LogicalClockSystem::is_causally_dependent(const std::vector<int>& v1, const std::vector<int>& v2) {
    if (v1.size() != v2.size() || v1.empty()) return false;
    bool strictly_less = false;
    for (size_t i = 0; i < v1.size(); ++i) {
        if (v1[i] > v2[i]) return false;
        if (v1[i] < v2[i]) strictly_less = true;
    }
    return strictly_less;
}

bool LogicalClockSystem::is_concurrent(const std::vector<int>& v1, const std::vector<int>& v2) {
    return !is_causally_dependent(v1, v2) && !is_causally_dependent(v2, v1);
}

} // namespace osvault
