#pragma once
#include <cstddef>
#include <string>

namespace osvault {

struct AppConfig {
    static constexpr size_t DEFAULT_BLOCK_SIZE = 4096;
    static constexpr size_t DEFAULT_TOTAL_BLOCKS = 1024;
    static constexpr size_t DEFAULT_TOTAL_CAPACITY_BYTES = DEFAULT_BLOCK_SIZE * DEFAULT_TOTAL_BLOCKS; // 4 MiB

    int port = 8080;
    std::string host = "0.0.0.0";
    std::string db_path = "data/osvault.db";
    std::string web_dir = "web";
    size_t block_size = DEFAULT_BLOCK_SIZE;
    size_t total_blocks = DEFAULT_TOTAL_BLOCKS;

    static AppConfig parse_args(int argc, char* argv[]);
};

} // namespace osvault
