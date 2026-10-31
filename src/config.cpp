#include "osvault/config.hpp"
#include <cstdlib>
#include <iostream>
#include <string>

namespace osvault {

AppConfig AppConfig::parse_args(int argc, char* argv[]) {
    AppConfig config;

    // Check environment variables first
    if (const char* env_port = std::getenv("OSVAULT_PORT")) {
        try { config.port = std::stoi(env_port); } catch (...) {}
    }
    if (const char* env_host = std::getenv("OSVAULT_HOST")) {
        config.host = env_host;
    }
    if (const char* env_db = std::getenv("OSVAULT_DB_PATH")) {
        config.db_path = env_db;
    }
    if (const char* env_web = std::getenv("OSVAULT_WEB_DIR")) {
        config.web_dir = env_web;
    }

    // Command-line arguments take precedence
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "--port" || arg == "-p") && i + 1 < argc) {
            try { config.port = std::stoi(argv[++i]); } catch (...) {}
        } else if ((arg == "--host" || arg == "-b") && i + 1 < argc) {
            config.host = argv[++i];
        } else if ((arg == "--db" || arg == "-d") && i + 1 < argc) {
            config.db_path = argv[++i];
        } else if ((arg == "--web" || arg == "-w") && i + 1 < argc) {
            config.web_dir = argv[++i];
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "OSVault - Interactive Operating Systems Lab & Storage Engine\n"
                      << "Usage: osvault [options]\n"
                      << "Options:\n"
                      << "  -p, --port <number>    Set HTTP listen port (default: 8080)\n"
                      << "  -d, --db <path>        Set SQLite database path (default: data/osvault.db)\n"
                      << "  -w, --web <path>       Set web assets directory (default: web)\n"
                      << "  -h, --help             Show this help message\n";
            std::exit(0);
        }
    }

    return config;
}

} // namespace osvault
