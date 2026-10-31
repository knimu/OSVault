#pragma once
#include <deque>
#include <mutex>
#include <string>
#include <vector>
#include "osvault/result.hpp"

namespace osvault {

struct CriticalSectionState {
    int thread_count = 2;
    std::vector<std::string> thread_states; // "REMAINDER", "WAITING", "CRITICAL"
    int current_in_cs = -1;
    int turn = 0;
    std::vector<bool> flag;
    std::vector<std::string> log_history;
};

struct ProducerConsumerState {
    int buffer_capacity = 5;
    std::vector<int> buffer;
    int in_ptr = 0;
    int out_ptr = 0;
    int empty_slots = 5;
    int full_slots = 0;
    bool mutex_locked = false;
    std::string producer_state = "IDLE";
    std::string consumer_state = "IDLE";
    std::vector<std::string> log_history;
};

struct ReadersWritersState {
    int reader_count = 0;
    bool writer_active = false;
    int waiting_readers = 0;
    int waiting_writers = 0;
    std::vector<std::string> active_readers;
    std::string active_writer;
    int shared_resource_val = 100;
    std::vector<std::string> log_history;
};

struct DiningPhilosophersState {
    static constexpr int NUM_PHILOSOPHERS = 5;
    std::vector<std::string> states; // "THINKING", "HUNGRY", "EATING"
    std::vector<bool> forks; // true = fork available, false = fork in use
    std::vector<int> meals_eaten;
    std::vector<std::string> log_history;
};

class SynchronizationSimulator {
public:
    SynchronizationSimulator();

    // Critical Section
    CriticalSectionState get_cs_state();
    void reset_cs();
    Result<CriticalSectionState> step_cs(int thread_id);

    // Producer Consumer
    ProducerConsumerState get_pc_state();
    void reset_pc(int capacity = 5);
    Result<ProducerConsumerState> step_produce(int item);
    Result<ProducerConsumerState> step_consume();

    // Readers Writers
    ReadersWritersState get_rw_state();
    void reset_rw();
    Result<ReadersWritersState> start_read(const std::string& reader_id);
    Result<ReadersWritersState> stop_read(const std::string& reader_id);
    Result<ReadersWritersState> start_write(const std::string& writer_id);
    Result<ReadersWritersState> stop_write(const std::string& writer_id, int new_val);

    // Dining Philosophers
    DiningPhilosophersState get_dp_state();
    void reset_dp();
    Result<DiningPhilosophersState> step_philosopher(int phil_id);

private:
    std::mutex sim_mutex_;
    CriticalSectionState cs_state_;
    ProducerConsumerState pc_state_;
    ReadersWritersState rw_state_;
    DiningPhilosophersState dp_state_;
};

} // namespace osvault
