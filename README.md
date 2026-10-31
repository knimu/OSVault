# OSVault — Interactive Operating Systems Laboratory & Crash-Consistent Block Storage Engine

[![C++17](https://img.shields.io/badge/Language-C%2B%2B17-00599C?logo=c%2B%2B)](https://en.cppreference.com/w/cpp/17)
[![Build System](https://img.shields.io/badge/Build-CMake%203.16%2B-064F8C?logo=cmake)](https://cmake.org/)
[![Database](https://img.shields.io/badge/Database-SQLite%203.47%20(WAL)-003B57?logo=sqlite)](https://www.sqlite.org/)
[![Web Framework](https://img.shields.io/badge/Backend-Crow%20C%2B%2B%20v1.3.4-3b82f6)](https://crowcpp.org/)
[![Tests](https://img.shields.io/badge/Tests-10%2F10%20Passing%20(100%25)-10b981)](docs/testing.md)
[![License](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)

---

## 1. Project Overview

**OSVault** is a comprehensive, production-grade academic software project designed to bridge the gap between theoretical Operating Systems principles and practical systems programming. It implements an interactive, visual Operating Systems laboratory alongside a verifiable, crash-consistent logical block storage engine.

Built from scratch in pure **C++17**, OSVault embeds the lightweight, header-only **Crow C++** microframework and standalone **Asio** to serve an interactive, modern **HTML5/CSS3/Vanilla JavaScript** web application directly on port **8080**.

### Key Architectural Highlights:
- **Zero External Language Runtimes:** No Python, Flask, Node.js, PHP, or Java. The entire application backend, routing layer, and algorithm implementations run from a single compiled native C++ binary.
- **ACID Persistence:** Persistent data, inode hierarchies, allocation bitmaps, and audit logs are managed by an embedded, threadsafe **SQLite 3 (v3.47.0)** engine configured with Write-Ahead Logging (WAL).
- **100% Offline & Reproducible:** All dependencies (`crow_all.h`, `asio/`, `sqlite3.c`, `doctest.h`) are pinned directly inside the repository under `third_party/`.

---

## 2. Core Operating Systems Modules

OSVault provides 13 fully integrated, interactive modules:

1. **Virtual Filesystem (VFS):** Inode-based file creation, reading, updating, and deletion with automatic multi-block spanning and SHA-256 checksums.
2. **1024-Block Logical Storage Engine:** Visualizes 1024 discrete 4096-byte sectors (4 MiB total storage). Features live cell inspection, block allocation bitmaps, and dynamic switching between **First Fit**, **Best Fit**, and **Worst Fit** allocation strategies.
3. **Page Cache & Virtual Memory Paging:** In-memory 64-frame buffer pool with step-by-step trace simulation comparing **FIFO** (First-In, First-Out) and **LRU** (Least Recently Used) page replacement.
4. **CPU Scheduling Simulator:** Implements **FCFS**, **SJF** (Non-Preemptive), **SRTF** (Preemptive Shortest Remaining Time First), **Round Robin (RR)** with adjustable time quantum, and **Priority Scheduling**. Outputs interactive SVG/HTML Gantt charts and calculates Turnaround Time (TAT) and Waiting Time (WT).
5. **Dynamic Memory Manager:** 1024 KB physical memory partition manager. Simulates contiguous variable partition allocation, free-list coalescence, and dynamic external fragmentation measurement.
6. **Concurrency & Synchronization:**
   - **Peterson's Algorithm:** Verifiable software mutual exclusion for two processes ($P_0, P_1$) without hardware atomic locks.
   - **Bounded Buffer (Producer-Consumer):** Circular buffer synchronized via counting semaphores (`empty`, `full`) and a mutex lock.
   - **Readers-Writers Problem:** Read-preferring synchronization guaranteeing multiple concurrent readers while enforcing exclusive writer access.
   - **Dining Philosophers:** Five philosophers around a circular table synchronized using Dijkstra's resource hierarchy to mathematically prevent deadlock and circular wait.
7. **Deadlock & Banker's Algorithm:** Evaluates system safety via Dijkstra's Banker's Algorithm. Computes safe sequences (e.g. $\langle P_1, P_3, P_4, P_0, P_2 \rangle$), identifies deadlocked processes, and demonstrates recovery via simulated process termination.
8. **Inter-Process Communication (IPC):**
   - **Anonymous Pipes:** FIFO unidirectional byte streams with capacity limits.
   - **Message Queues:** POSIX-style structured message passing between named processes.
   - **Shared Memory with Mutex Locks:** Shared buffer region guarded by a mutual exclusion lock to demonstrate race condition prevention.
   - **Socket Exchange:** Simulated client-server request-reply exchange over loopback IPC.
9. **Distributed Logical Clocks:** Implements **Lamport Timestamps** and multi-node **Vector Clocks**, featuring an interactive causal concurrency evaluator ($V_1 \parallel V_2$).
10. **Persistent Operation Logger:** SQLite WAL audit stream with full-text search, event type filtering, and pagination.
11. **Crash Consistency & Integrity Recovery:** Simulates uncommitted disk writes (power failure before commit), scans for leaked orphan blocks, and executes 1-click automated storage repair.
12. **System Health & Overview Dashboard:** Live KPI cards for block usage, cache hit ratio, inode counts, and real-time backend health monitoring.
13. **Engine Settings & Environment Specs:** Comprehensive runtime specifications, compiler flags, and database configuration details.

---

## 3. Directory Layout

```
OSVault/
├── CMakeLists.txt              # Root build configuration
├── CMakePresets.json           # Standard CMake presets
├── CMakeUserPresets.json.example
├── README.md                   # Master project documentation
├── LICENSE                     # MIT License
├── third_party/                # Pinned offline dependencies
│   ├── crow/crow_all.h         # Crow C++ v1.3.4 (Header-Only)
│   ├── asio/                   # Standalone Asio v1.30.2
│   ├── sqlite/                 # SQLite 3.47.0 amalgamation (sqlite3.c, sqlite3.h)
│   └── doctest/doctest.h       # doctest v2.4.11
├── include/osvault/            # C++17 Header Files
│   ├── config.hpp
│   ├── database.hpp
│   ├── models.hpp
│   ├── result.hpp
│   ├── block_allocator.hpp
│   ├── storage_engine.hpp
│   ├── cache_manager.hpp
│   ├── scheduler.hpp
│   ├── memory_manager.hpp
│   ├── synchronization.hpp
│   ├── deadlock_detector.hpp
│   ├── ipc_simulator.hpp
│   ├── logical_clock.hpp
│   ├── operation_logger.hpp
│   ├── integrity_checker.hpp
│   ├── recovery_manager.hpp
│   └── server.hpp
├── src/                        # C++17 Implementations
│   ├── main.cpp                # Application entry point
│   ├── config.cpp
│   ├── database.cpp
│   ├── block_allocator.cpp
│   ├── storage_engine.cpp
│   ├── cache_manager.cpp
│   ├── scheduler.cpp
│   ├── memory_manager.cpp
│   ├── synchronization.cpp
│   ├── deadlock_detector.cpp
│   ├── ipc_simulator.cpp
│   ├── logical_clock.cpp
│   ├── operation_logger.cpp
│   ├── integrity_checker.cpp
│   ├── recovery_manager.cpp
│   └── routes/                 # Crow REST API Endpoints (12 route files)
├── tests/                      # Automated CTest Test Suite (10 Suites)
│   ├── CMakeLists.txt
│   ├── main_test.cpp
│   ├── test_database.cpp
│   ├── test_storage.cpp
│   ├── test_cache.cpp
│   ├── test_scheduler.cpp
│   ├── test_memory.cpp
│   ├── test_deadlock.cpp
│   ├── test_synchronization.cpp
│   ├── test_clocks.cpp
│   ├── test_recovery.cpp
│   └── test_api.cpp
├── web/                        # Frontend Web Assets (Served directly by C++)
│   ├── index.html              # Master SPA layout (All 13 modules)
│   └── assets/
│       ├── css/styles.css      # Dark navy / slate responsive UI design
│       └── js/                 # Modular Vanilla JS (14 files)
├── scripts/                    # Automation Scripts
│   ├── setup.ps1               # Dependency & environment verification
│   ├── build.ps1               # CMake build automation
│   ├── test.ps1                # CTest automated test execution
│   ├── run.ps1                 # Launch OSVault server
│   └── verify.ps1              # End-to-end system & API verification
└── docs/                       # Comprehensive Technical Documentation
    ├── architecture.md         # System design & Mermaid diagrams
    ├── database.md             # SQLite schema, WAL mode, & durability
    ├── api.md                  # REST API contract & JSON payloads
    ├── testing.md              # Test strategy & CTest execution logs
    ├── demo-guide.md           # Step-by-step academic presentation guide
    ├── limitations.md          # Simulation vs real kernel trade-offs
    └── progress.md             # Implementation milestone checklist
```

---

## 4. Quick Start Guide

### Prerequisites
- Windows 10/11, Linux, or macOS.
- C++17 compatible compiler:
  - Windows: Visual Studio 2019/2022/2026 or Visual Studio Build Tools with C++ workload.
  - Linux: GCC 9+ or Clang 10+.
- CMake 3.16 or higher.

### Step 1: Verify Dependencies
```powershell
.\scripts\setup.ps1
```

### Step 2: Build the Application
```powershell
.\scripts\build.ps1 -Config Release
```
*(Or via CMake directly: `cmake -B build -S . && cmake --build build --config Release`)*

### Step 3: Run Automated Tests
```powershell
.\scripts\test.ps1
```
*(Or via CTest: `ctest --test-dir build -C Release --output-on-failure`)*

### Step 4: Run System Verification
```powershell
.\scripts\verify.ps1
```

### Step 5: Start the OSVault Server
```powershell
.\scripts\run.ps1
```
Open your browser and navigate to: **`http://localhost:8080/`**

---

## 5. Documentation Links

- [System Architecture & Subsystem Breakdown](docs/architecture.md)
- [Database Schema & SQLite WAL Durability](docs/database.md)
- [REST API Specification](docs/api.md)
- [Testing Strategy & Test Suite Details](docs/testing.md)
- [Academic Defense & Demonstration Guide](docs/demo-guide.md)
- [Technical Limitations & Kernel Comparison](docs/limitations.md)

---

## 6. License & Academic Integrity

This project is licensed under the MIT License. Developed for educational purposes and Operating Systems laboratory curriculum study.
