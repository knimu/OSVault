cmake_minimum_required(VERSION 3.16)
project(OSVault VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable CTest
include(CTest)
enable_testing()

# Compiler options and definitions
if(MSVC)
    add_compile_definitions(_WIN32_WINNT=0x0601 ASIO_STANDALONE _CRT_SECURE_NO_WARNINGS)
    add_compile_options(/W3 /EHsc /bigobj)
else()
    add_compile_definitions(ASIO_STANDALONE)
    add_compile_options(-Wall -Wextra -pedantic -Wno-unused-parameter)
endif()

# Third-party: SQLite 3 amalgamation static library
add_library(sqlite3 STATIC third_party/sqlite/sqlite3.c)
target_include_directories(sqlite3 PUBLIC third_party/sqlite)
target_compile_definitions(sqlite3 PRIVATE 
    SQLITE_THREADSAFE=1 
    SQLITE_ENABLE_WAL=1
    SQLITE_ENABLE_COLUMN_METADATA=1
)

# Core engine and route implementation files
set(OSVAULT_CORE_SOURCES
    src/config.cpp
    src/database.cpp
    src/block_allocator.cpp
    src/storage_engine.cpp
    src/cache_manager.cpp
    src/scheduler.cpp
    src/memory_manager.cpp
    src/synchronization.cpp
    src/deadlock_detector.cpp
    src/ipc_simulator.cpp
    src/logical_clock.cpp
    src/operation_logger.cpp
    src/integrity_checker.cpp
    src/recovery_manager.cpp

    src/routes/system_routes.cpp
    src/routes/file_routes.cpp
    src/routes/block_routes.cpp
    src/routes/cache_routes.cpp
    src/routes/scheduler_routes.cpp
    src/routes/memory_routes.cpp
    src/routes/synchronization_routes.cpp
    src/routes/deadlock_routes.cpp
    src/routes/ipc_routes.cpp
    src/routes/clock_routes.cpp
    src/routes/log_routes.cpp
    src/routes/recovery_routes.cpp
)

add_library(osvault_core STATIC ${OSVAULT_CORE_SOURCES})
target_include_directories(osvault_core PUBLIC
    include
    third_party/crow
    third_party/asio/include
    third_party/sqlite
)
target_link_libraries(osvault_core PUBLIC sqlite3)
if(WIN32)
    target_link_libraries(osvault_core PUBLIC ws2_32 wsock32)
endif()

# Main executable
add_executable(osvault src/main.cpp)
target_link_libraries(osvault PRIVATE osvault_core)
target_include_directories(osvault PRIVATE include third_party/crow)

# Copy web directory to build directory so executable finds assets reliably
add_custom_command(
    TARGET osvault POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/web
        $<TARGET_FILE_DIR:osvault>/web
    COMMENT "Deploying frontend web assets to output directory"
)

# Subdirectory for automated tests
add_subdirectory(tests)
