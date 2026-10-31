#include "osvault/ipc_simulator.hpp"
#include <algorithm>
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

IpcSimulator::IpcSimulator() {
    reset_pipe();
}

void IpcSimulator::record_event(const std::string& channel, const std::string& sender,
                               const std::string& receiver, const std::string& msg, bool delivered) {
    IpcMessage record;
    record.id = next_msg_id_++;
    record.timestamp = current_iso_time();
    record.channel_type = channel;
    record.sender = sender;
    record.receiver = receiver;
    record.message = msg;
    record.delivered = delivered;
    history_.push_back(record);
    if (history_.size() > 100) {
        history_.erase(history_.begin());
    }
}

std::vector<IpcMessage> IpcSimulator::get_history() const {
    std::lock_guard<std::mutex> lock(ipc_mutex_);
    return history_;
}

// Pipe operations
void IpcSimulator::reset_pipe() {
    std::lock_guard<std::mutex> lock(ipc_mutex_);
    pipe_.buffer.clear();
    pipe_.capacity = 512;
    pipe_.write_open = true;
    pipe_.read_open = true;
}

PipeState IpcSimulator::get_pipe_state() {
    std::lock_guard<std::mutex> lock(ipc_mutex_);
    return pipe_;
}

Result<void> IpcSimulator::pipe_write(const std::string& data) {
    std::lock_guard<std::mutex> lock(ipc_mutex_);
    if (!pipe_.write_open) {
        return Result<void>::failure("Broken pipe: write end is closed.", 410);
    }
    if (pipe_.buffer.size() + data.size() > pipe_.capacity) {
        return Result<void>::failure("Pipe buffer full: cannot write " + std::to_string(data.size()) +
                                     " bytes (Capacity: " + std::to_string(pipe_.capacity) + ").", 507);
    }

    pipe_.buffer.append(data);
    record_event("pipe", "WriterProcess", "ReaderProcess", "Wrote " + std::to_string(data.size()) + " bytes to pipe: '" + data + "'");
    return Result<void>::ok();
}

Result<std::string> IpcSimulator::pipe_read(size_t max_bytes) {
    std::lock_guard<std::mutex> lock(ipc_mutex_);
    if (pipe_.buffer.empty()) {
        return Result<std::string>::ok("");
    }

    size_t to_read = std::min(max_bytes, pipe_.buffer.size());
    std::string out = pipe_.buffer.substr(0, to_read);
    pipe_.buffer.erase(0, to_read);

    record_event("pipe", "WriterProcess", "ReaderProcess", "Read " + std::to_string(to_read) + " bytes from pipe: '" + out + "'");
    return Result<std::string>::ok(std::move(out));
}

// Message Queue operations
Result<void> IpcSimulator::mq_send(const std::string& sender, const std::string& receiver, const std::string& message) {
    std::lock_guard<std::mutex> lock(ipc_mutex_);
    if (sender.empty() || receiver.empty() || message.empty()) {
        return Result<void>::failure("Sender, receiver, and message must not be empty.", 400);
    }

    IpcMessage msg;
    msg.id = next_msg_id_++;
    msg.timestamp = current_iso_time();
    msg.channel_type = "queue";
    msg.sender = sender;
    msg.receiver = receiver;
    msg.message = message;
    msg.delivered = false;

    message_queue_.push_back(msg);
    record_event("queue", sender, receiver, message, false);
    return Result<void>::ok();
}

Result<IpcMessage> IpcSimulator::mq_receive(const std::string& receiver) {
    std::lock_guard<std::mutex> lock(ipc_mutex_);
    for (auto it = message_queue_.begin(); it != message_queue_.end(); ++it) {
        if (it->receiver == receiver) {
            IpcMessage msg = *it;
            msg.delivered = true;
            message_queue_.erase(it);

            record_event("queue", msg.sender, receiver, "Received: " + msg.message, true);
            return Result<IpcMessage>::ok(std::move(msg));
        }
    }
    return Result<IpcMessage>::failure("No message available for receiver: " + receiver, 404);
}

std::vector<IpcMessage> IpcSimulator::get_mq_messages() {
    std::lock_guard<std::mutex> lock(ipc_mutex_);
    return std::vector<IpcMessage>(message_queue_.begin(), message_queue_.end());
}

void IpcSimulator::clear_mq() {
    std::lock_guard<std::mutex> lock(ipc_mutex_);
    message_queue_.clear();
}

// Shared Memory operations
Result<std::string> IpcSimulator::shm_read(const std::string& reader) {
    std::lock_guard<std::mutex> lock(ipc_mutex_);
    record_event("shared_memory", reader, "SharedSegment", "Read: '" + shm_.content + "'");
    return Result<std::string>::ok(shm_.content);
}

Result<void> IpcSimulator::shm_write(const std::string& writer, const std::string& data) {
    std::lock_guard<std::mutex> lock(ipc_mutex_);
    if (shm_.locked && shm_.locked_by != writer) {
        return Result<void>::failure("Shared memory is locked by " + shm_.locked_by, 423);
    }

    shm_.content = data;
    record_event("shared_memory", writer, "SharedSegment", "Updated to: '" + data + "'");
    return Result<void>::ok();
}

Result<void> IpcSimulator::shm_lock(const std::string& process) {
    std::lock_guard<std::mutex> lock(ipc_mutex_);
    if (shm_.locked && shm_.locked_by != process) {
        return Result<void>::failure("Already locked by " + shm_.locked_by, 423);
    }
    shm_.locked = true;
    shm_.locked_by = process;
    record_event("shared_memory", process, "SharedSegment", "Acquired mutex lock");
    return Result<void>::ok();
}

Result<void> IpcSimulator::shm_unlock(const std::string& process) {
    std::lock_guard<std::mutex> lock(ipc_mutex_);
    if (!shm_.locked) return Result<void>::ok();
    if (shm_.locked_by != process) {
        return Result<void>::failure("Cannot unlock: held by " + shm_.locked_by, 403);
    }
    shm_.locked = false;
    shm_.locked_by = "";
    record_event("shared_memory", process, "SharedSegment", "Released mutex lock");
    return Result<void>::ok();
}

SharedMemorySegment IpcSimulator::get_shm_state() {
    std::lock_guard<std::mutex> lock(ipc_mutex_);
    return shm_;
}

// Client-Server Message Exchange
Result<std::pair<IpcMessage, IpcMessage>> IpcSimulator::client_server_exchange(
    const std::string& client, const std::string& request) {
    std::lock_guard<std::mutex> lock(ipc_mutex_);

    IpcMessage req;
    req.id = next_msg_id_++;
    req.timestamp = current_iso_time();
    req.channel_type = "socket";
    req.sender = client;
    req.receiver = "ServerEngine";
    req.message = request;
    req.delivered = true;
    history_.push_back(req);

    // Simulated Server response
    IpcMessage resp;
    resp.id = next_msg_id_++;
    resp.timestamp = current_iso_time();
    resp.channel_type = "socket";
    resp.sender = "ServerEngine";
    resp.receiver = client;
    resp.message = "ACK: Processed request [" + request + "] at " + resp.timestamp;
    resp.delivered = true;
    history_.push_back(resp);

    return Result<std::pair<IpcMessage, IpcMessage>>::ok({req, resp});
}

} // namespace osvault
