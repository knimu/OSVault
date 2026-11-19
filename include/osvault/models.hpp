#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace osvault {

struct FileRecord {
    int64_t id = 0;
    std::string name;
    int64_t directory_id = 0;
    size_t size = 0;
    size_t block_count = 0;
    std::vector<int> block_ids;
    std::string created_at;
    std::string updated_at;
};

struct DirectoryRecord {
    int64_t id = 0;
    std::string name;
    int64_t parent_id = 0;
    std::string created_at;
};

struct BlockInfo {
    int block_id = 0;
    bool is_allocated = false;
    int64_t owner_file_id = 0;
    std::string owner_file_name;
    int block_index_in_file = -1;
    std::string payload;
};

struct SystemMetrics {
    size_t total_capacity_bytes = 0;
    size_t block_size = 0;
    size_t total_blocks = 0;
    size_t allocated_blocks = 0;
    size_t free_blocks = 0;
    double storage_utilization_pct = 0.0;
    size_t file_count = 0;
    size_t total_bytes = 0;
    size_t cache_hits = 0;
    size_t cache_misses = 0;
    double cache_hit_ratio = 0.0;
    size_t recent_operations_count = 0;
    bool integrity_healthy = true;
};

struct LogRecord {
    int64_t id = 0;
    std::string timestamp;
    std::string event_type;
    std::string description;
    std::string result;
    std::string related_id;
};

struct PageTableEntry {
    int page = 0;
    int frame = -1;
    bool present = false;
};

struct PageStep {
    int page = 0;
    std::vector<int> frames;
    bool hit = false;
    int evicted_page = -1;
    int frame = -1;
    int offset = 0;
    int logical_address = 0;
    int physical_address = -1;
    std::vector<PageTableEntry> page_table;
};

struct PageReplacementResult {
    std::string algorithm;
    std::vector<int> sequence;
    int frame_count = 0;
    int page_size = 4096;
    std::vector<PageStep> steps;
    int total_hits = 0;
    int total_misses = 0;
    double hit_ratio = 0.0;
    double miss_ratio = 0.0;
    std::vector<int> final_frames;
};

struct ProcessTask {
    std::string pid;
    int arrival_time = 0;
    int burst_time = 0;
    int priority = 0;
};

struct ProcessResult {
    std::string pid;
    int arrival_time = 0;
    int burst_time = 0;
    int priority = 0;
    int completion_time = 0;
    int turnaround_time = 0;
    int waiting_time = 0;
    int response_time = 0;
};

struct GanttInterval {
    std::string pid;
    int start_time = 0;
    int end_time = 0;
    bool is_idle = false;
};

struct SchedulingResult {
    std::string algorithm;
    int time_quantum = 0;
    std::vector<ProcessResult> processes;
    std::vector<GanttInterval> gantt_chart;
    double avg_turnaround_time = 0.0;
    double avg_waiting_time = 0.0;
    double avg_response_time = 0.0;
    int total_time = 0;
    int idle_time = 0;
};

struct MemoryPartition {
    int id = 0;
    size_t start_address = 0;
    size_t size = 0;
    bool is_allocated = false;
    std::string process_name;
    size_t internal_fragmentation = 0;
};

struct MemoryAllocationResult {
    std::string strategy;
    std::vector<MemoryPartition> partitions;
    size_t total_memory = 0;
    size_t allocated_memory = 0;
    size_t free_memory = 0;
    size_t external_fragmentation = 0;
    size_t total_internal_fragmentation = 0;
    bool success = true;
    std::string message;
};

struct DeadlockState {
    std::vector<std::string> processes;
    std::vector<std::string> resource_types;
    std::vector<int> available;
    std::vector<std::vector<int>> allocation;
    std::vector<std::vector<int>> request;
    bool is_deadlock = false;
    bool is_safe = false;
    std::vector<std::string> safe_sequence;
    std::vector<std::string> deadlocked_processes;
    std::vector<std::string> log_steps;
};

struct IpcMessage {
    int id = 0;
    std::string timestamp;
    std::string channel_type; // "pipe", "queue", "shared_memory", "socket"
    std::string sender;
    std::string receiver;
    std::string message;
    bool delivered = true;
};

struct LamportEvent {
    int event_id = 0;
    std::string node_id;
    std::string event_type; // "local", "send", "receive"
    std::string description;
    int lamport_clock = 0;
    std::vector<int> vector_clock;
};

struct IntegrityReport {
    bool is_healthy = true;
    size_t files_checked = 0;
    size_t blocks_referenced = 0;
    size_t duplicate_references = 0;
    size_t invalid_block_ids = 0;
    size_t orphaned_blocks = 0;
    size_t block_count_mismatches = 0;
    bool capacity_exceeded = false;
    std::vector<std::string> errors;
};

struct RecoveryExperimentReport {
    std::string experiment_name;
    bool success = true;
    std::string description;
    std::string before_state;
    std::string injection_event;
    std::string after_state;
    bool recovered_cleanly = true;
    std::string recovery_details;
};

} // namespace osvault
