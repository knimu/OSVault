#include "osvault/synchronization.hpp"
#include <algorithm>

namespace osvault {

SynchronizationSimulator::SynchronizationSimulator() {
    reset_cs();
    reset_pc(5);
    reset_rw();
    reset_dp();
}

// Critical Section
void SynchronizationSimulator::reset_cs() {
    std::lock_guard<std::mutex> lock(sim_mutex_);
    cs_state_.thread_count = 2;
    cs_state_.thread_states = {"REMAINDER", "REMAINDER"};
    cs_state_.current_in_cs = -1;
    cs_state_.turn = 0;
    cs_state_.flag = {false, false};
    cs_state_.log_history = {"Critical section simulation initialized with Peterson's algorithm."};
}

CriticalSectionState SynchronizationSimulator::get_cs_state() {
    std::lock_guard<std::mutex> lock(sim_mutex_);
    return cs_state_;
}

Result<CriticalSectionState> SynchronizationSimulator::step_cs(int thread_id) {
    std::lock_guard<std::mutex> lock(sim_mutex_);
    if (thread_id < 0 || thread_id >= cs_state_.thread_count) {
        return Result<CriticalSectionState>::failure("Invalid thread ID: " + std::to_string(thread_id), 400);
    }

    int other = 1 - thread_id;
    std::string& state = cs_state_.thread_states[thread_id];

    if (state == "REMAINDER") {
        // Entry Section: set flag and yield turn
        cs_state_.flag[thread_id] = true;
        cs_state_.turn = other;
        state = "WAITING";
        cs_state_.log_history.push_back("Thread " + std::to_string(thread_id) + " entered WAITING (flag set, turn given to " + std::to_string(other) + ")");

        // Check if can enter critical section immediately
        if (!cs_state_.flag[other] || cs_state_.turn == thread_id) {
            state = "CRITICAL";
            cs_state_.current_in_cs = thread_id;
            cs_state_.log_history.push_back("Thread " + std::to_string(thread_id) + " acquired lock and entered CRITICAL SECTION.");
        }
    } else if (state == "WAITING") {
        if (!cs_state_.flag[other] || cs_state_.turn == thread_id) {
            state = "CRITICAL";
            cs_state_.current_in_cs = thread_id;
            cs_state_.log_history.push_back("Thread " + std::to_string(thread_id) + " acquired lock and entered CRITICAL SECTION.");
        } else {
            cs_state_.log_history.push_back("Thread " + std::to_string(thread_id) + " still waiting: Thread " + std::to_string(other) + " holds lock/turn.");
        }
    } else if (state == "CRITICAL") {
        // Exit Section
        cs_state_.flag[thread_id] = false;
        state = "REMAINDER";
        cs_state_.current_in_cs = -1;
        cs_state_.log_history.push_back("Thread " + std::to_string(thread_id) + " exited CRITICAL SECTION (flag cleared).");

        // If other thread is waiting, let it enter
        if (cs_state_.thread_states[other] == "WAITING") {
            cs_state_.thread_states[other] = "CRITICAL";
            cs_state_.current_in_cs = other;
            cs_state_.log_history.push_back("Thread " + std::to_string(other) + " proceeded from WAITING into CRITICAL SECTION.");
        }
    }

    return Result<CriticalSectionState>::ok(cs_state_);
}

// Producer Consumer
void SynchronizationSimulator::reset_pc(int capacity) {
    std::lock_guard<std::mutex> lock(sim_mutex_);
    pc_state_.buffer_capacity = capacity > 0 ? capacity : 5;
    pc_state_.buffer.clear();
    pc_state_.in_ptr = 0;
    pc_state_.out_ptr = 0;
    pc_state_.empty_slots = pc_state_.buffer_capacity;
    pc_state_.full_slots = 0;
    pc_state_.mutex_locked = false;
    pc_state_.producer_state = "IDLE";
    pc_state_.consumer_state = "IDLE";
    pc_state_.log_history = {"Bounded buffer initialized (Capacity: " + std::to_string(pc_state_.buffer_capacity) + ")."};
}

ProducerConsumerState SynchronizationSimulator::get_pc_state() {
    std::lock_guard<std::mutex> lock(sim_mutex_);
    return pc_state_;
}

Result<ProducerConsumerState> SynchronizationSimulator::step_produce(int item) {
    std::lock_guard<std::mutex> lock(sim_mutex_);
    if (pc_state_.empty_slots <= 0) {
        pc_state_.producer_state = "BLOCKED (Buffer Full)";
        pc_state_.log_history.push_back("Producer attempted produce(" + std::to_string(item) + "), but buffer is FULL. Producer BLOCKED.");
        return Result<ProducerConsumerState>::ok(pc_state_);
    }

    // Produce item
    --pc_state_.empty_slots;
    pc_state_.buffer.push_back(item);
    pc_state_.in_ptr = (pc_state_.in_ptr + 1) % pc_state_.buffer_capacity;
    ++pc_state_.full_slots;
    pc_state_.producer_state = "PRODUCED " + std::to_string(item);
    pc_state_.log_history.push_back("Producer added item " + std::to_string(item) + " to buffer (Full: " +
                                    std::to_string(pc_state_.full_slots) + "/" + std::to_string(pc_state_.buffer_capacity) + ")");

    if (pc_state_.consumer_state.find("BLOCKED") != std::string::npos) {
        pc_state_.consumer_state = "READY";
    }

    return Result<ProducerConsumerState>::ok(pc_state_);
}

Result<ProducerConsumerState> SynchronizationSimulator::step_consume() {
    std::lock_guard<std::mutex> lock(sim_mutex_);
    if (pc_state_.full_slots <= 0 || pc_state_.buffer.empty()) {
        pc_state_.consumer_state = "BLOCKED (Buffer Empty)";
        pc_state_.log_history.push_back("Consumer attempted consume(), but buffer is EMPTY. Consumer BLOCKED.");
        return Result<ProducerConsumerState>::ok(pc_state_);
    }

    int item = pc_state_.buffer.front();
    pc_state_.buffer.erase(pc_state_.buffer.begin());
    --pc_state_.full_slots;
    ++pc_state_.empty_slots;
    pc_state_.out_ptr = (pc_state_.out_ptr + 1) % pc_state_.buffer_capacity;
    pc_state_.consumer_state = "CONSUMED " + std::to_string(item);
    pc_state_.log_history.push_back("Consumer retrieved item " + std::to_string(item) + " from buffer (Remaining: " +
                                    std::to_string(pc_state_.full_slots) + "/" + std::to_string(pc_state_.buffer_capacity) + ")");

    if (pc_state_.producer_state.find("BLOCKED") != std::string::npos) {
        pc_state_.producer_state = "READY";
    }

    return Result<ProducerConsumerState>::ok(pc_state_);
}

// Readers Writers
void SynchronizationSimulator::reset_rw() {
    std::lock_guard<std::mutex> lock(sim_mutex_);
    rw_state_.reader_count = 0;
    rw_state_.writer_active = false;
    rw_state_.waiting_readers = 0;
    rw_state_.waiting_writers = 0;
    rw_state_.active_readers.clear();
    rw_state_.active_writer = "";
    rw_state_.shared_resource_val = 100;
    rw_state_.log_history = {"Readers-Writers simulation initialized."};
}

ReadersWritersState SynchronizationSimulator::get_rw_state() {
    std::lock_guard<std::mutex> lock(sim_mutex_);
    return rw_state_;
}

Result<ReadersWritersState> SynchronizationSimulator::start_read(const std::string& reader_id) {
    std::lock_guard<std::mutex> lock(sim_mutex_);
    if (rw_state_.writer_active) {
        ++rw_state_.waiting_readers;
        rw_state_.log_history.push_back("Reader " + reader_id + " wants to read, but Writer is active. Reader WAITING.");
        return Result<ReadersWritersState>::ok(rw_state_);
    }

    rw_state_.active_readers.push_back(reader_id);
    rw_state_.reader_count = static_cast<int>(rw_state_.active_readers.size());
    rw_state_.log_history.push_back("Reader " + reader_id + " started reading (Value: " + std::to_string(rw_state_.shared_resource_val) +
                                    ", Concurrent readers: " + std::to_string(rw_state_.reader_count) + ")");
    return Result<ReadersWritersState>::ok(rw_state_);
}

Result<ReadersWritersState> SynchronizationSimulator::stop_read(const std::string& reader_id) {
    std::lock_guard<std::mutex> lock(sim_mutex_);
    auto it = std::find(rw_state_.active_readers.begin(), rw_state_.active_readers.end(), reader_id);
    if (it != rw_state_.active_readers.end()) {
        rw_state_.active_readers.erase(it);
        rw_state_.reader_count = static_cast<int>(rw_state_.active_readers.size());
        rw_state_.log_history.push_back("Reader " + reader_id + " finished reading.");
    }
    return Result<ReadersWritersState>::ok(rw_state_);
}

Result<ReadersWritersState> SynchronizationSimulator::start_write(const std::string& writer_id) {
    std::lock_guard<std::mutex> lock(sim_mutex_);
    if (rw_state_.writer_active || rw_state_.reader_count > 0) {
        ++rw_state_.waiting_writers;
        rw_state_.log_history.push_back("Writer " + writer_id + " wants to write, but resource is busy (" +
                                        std::to_string(rw_state_.reader_count) + " readers active, writer=" +
                                        (rw_state_.writer_active ? "yes" : "no") + "). Writer WAITING.");
        return Result<ReadersWritersState>::ok(rw_state_);
    }

    rw_state_.writer_active = true;
    rw_state_.active_writer = writer_id;
    rw_state_.log_history.push_back("Writer " + writer_id + " acquired exclusive write lock.");
    return Result<ReadersWritersState>::ok(rw_state_);
}

Result<ReadersWritersState> SynchronizationSimulator::stop_write(const std::string& writer_id, int new_val) {
    std::lock_guard<std::mutex> lock(sim_mutex_);
    if (rw_state_.writer_active && rw_state_.active_writer == writer_id) {
        rw_state_.shared_resource_val = new_val;
        rw_state_.writer_active = false;
        rw_state_.active_writer = "";
        rw_state_.log_history.push_back("Writer " + writer_id + " updated resource value to " + std::to_string(new_val) + " and released write lock.");
    }
    return Result<ReadersWritersState>::ok(rw_state_);
}

// Dining Philosophers
void SynchronizationSimulator::reset_dp() {
    std::lock_guard<std::mutex> lock(sim_mutex_);
    dp_state_.states = std::vector<std::string>(DiningPhilosophersState::NUM_PHILOSOPHERS, "THINKING");
    dp_state_.forks = std::vector<bool>(DiningPhilosophersState::NUM_PHILOSOPHERS, true); // true = available
    dp_state_.meals_eaten = std::vector<int>(DiningPhilosophersState::NUM_PHILOSOPHERS, 0);
    dp_state_.log_history = {"Dining philosophers simulation initialized (5 philosophers, 5 forks, asymmetric pickup)."};
}

DiningPhilosophersState SynchronizationSimulator::get_dp_state() {
    std::lock_guard<std::mutex> lock(sim_mutex_);
    return dp_state_;
}

Result<DiningPhilosophersState> SynchronizationSimulator::step_philosopher(int phil_id) {
    std::lock_guard<std::mutex> lock(sim_mutex_);
    if (phil_id < 0 || phil_id >= DiningPhilosophersState::NUM_PHILOSOPHERS) {
        return Result<DiningPhilosophersState>::failure("Invalid philosopher ID.", 400);
    }

    int left_fork = phil_id;
    int right_fork = (phil_id + 1) % DiningPhilosophersState::NUM_PHILOSOPHERS;

    // Asymmetric pickup order to prevent circular wait deadlock:
    // If phil_id is last (4), pick right first, then left. Otherwise left then right.
    int first_fork = (phil_id == 4) ? right_fork : left_fork;
    int second_fork = (phil_id == 4) ? left_fork : right_fork;

    std::string& cur_state = dp_state_.states[phil_id];

    if (cur_state == "THINKING") {
        cur_state = "HUNGRY";
        dp_state_.log_history.push_back("Philosopher " + std::to_string(phil_id) + " became HUNGRY.");

        // Attempt to pick up forks
        if (dp_state_.forks[first_fork] && dp_state_.forks[second_fork]) {
            dp_state_.forks[first_fork] = false;
            dp_state_.forks[second_fork] = false;
            cur_state = "EATING";
            dp_state_.meals_eaten[phil_id]++;
            dp_state_.log_history.push_back("Philosopher " + std::to_string(phil_id) + " acquired forks (" +
                                            std::to_string(first_fork) + ", " + std::to_string(second_fork) + ") and is now EATING.");
        } else {
            dp_state_.log_history.push_back("Philosopher " + std::to_string(phil_id) + " waiting for forks to become available.");
        }
    } else if (cur_state == "HUNGRY") {
        if (dp_state_.forks[first_fork] && dp_state_.forks[second_fork]) {
            dp_state_.forks[first_fork] = false;
            dp_state_.forks[second_fork] = false;
            cur_state = "EATING";
            dp_state_.meals_eaten[phil_id]++;
            dp_state_.log_history.push_back("Philosopher " + std::to_string(phil_id) + " acquired forks (" +
                                            std::to_string(first_fork) + ", " + std::to_string(second_fork) + ") and is now EATING.");
        } else {
            dp_state_.log_history.push_back("Philosopher " + std::to_string(phil_id) + " still waiting for forks.");
        }
    } else if (cur_state == "EATING") {
        // Finished eating, put down forks, return to thinking
        dp_state_.forks[first_fork] = true;
        dp_state_.forks[second_fork] = true;
        cur_state = "THINKING";
        dp_state_.log_history.push_back("Philosopher " + std::to_string(phil_id) + " put down forks and returned to THINKING.");
    }

    return Result<DiningPhilosophersState>::ok(dp_state_);
}

} // namespace osvault
