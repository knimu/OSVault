#pragma once
#include <deque>
#include <mutex>
#include <string>
#include <vector>
#include "osvault/models.hpp"
#include "osvault/result.hpp"

namespace osvault {

struct PipeState {
    std::string buffer;
    size_t capacity = 512;
    bool write_open = true;
    bool read_open = true;
};

struct SharedMemorySegment {
    std::string segment_id = "shm_0";
    std::string content = "Initial Shared Memory Data";
    bool locked = false;
    std::string locked_by = "";
};

class IpcSimulator {
public:
    IpcSimulator();

    // Pipe operations
    Result<void> pipe_write(const std::string& data);
    Result<std::string> pipe_read(size_t max_bytes);
    PipeState get_pipe_state();
    void reset_pipe();

    // Message Queue operations
    Result<void> mq_send(const std::string& sender, const std::string& receiver, const std::string& message);
    Result<IpcMessage> mq_receive(const std::string& receiver);
    std::vector<IpcMessage> get_mq_messages();
    void clear_mq();

    // Shared Memory operations
    Result<std::string> shm_read(const std::string& reader);
    Result<void> shm_write(const std::string& writer, const std::string& data);
    Result<void> shm_lock(const std::string& process);
    Result<void> shm_unlock(const std::string& process);
    SharedMemorySegment get_shm_state();

    // Client-Server Message Exchange
    Result<std::pair<IpcMessage, IpcMessage>> client_server_exchange(const std::string& client, const std::string& request);

    std::vector<IpcMessage> get_history() const;

private:
    void record_event(const std::string& channel, const std::string& sender, const std::string& receiver, const std::string& msg, bool delivered = true);

    mutable std::mutex ipc_mutex_;
    PipeState pipe_;
    std::deque<IpcMessage> message_queue_;
    SharedMemorySegment shm_;
    std::vector<IpcMessage> history_;
    int next_msg_id_{1};
};

} // namespace osvault
