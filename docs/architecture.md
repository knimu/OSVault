# OSVault — Architecture & System Design

## 1. Executive Summary

**OSVault** is an interactive, educational Operating Systems laboratory and crash-consistent logical block storage engine implemented in pure **C++17**, paired with a lightweight, dependency-free **HTML5/CSS3/Vanilla JavaScript** single-page web interface.

The application serves dual pedagogical roles:
1. **Interactive OS Laboratory:** Provides interactive, verifiable visual simulations of foundational Operating Systems concepts—CPU scheduling, virtual memory paging, dynamic partition allocation, multi-process synchronization, deadlock avoidance/detection via Banker's algorithm, inter-process communication (IPC), and distributed logical clocks.
2. **Crash-Consistent Block Storage Engine:** Implements a concrete user-space virtual block storage system (1024 discrete 4096-byte blocks, totaling 4 MiB) backed by an ACID-compliant, Write-Ahead Logged (WAL) SQLite persistence engine with automated orphan block leak detection and recovery.

---

## 2. High-Level Architecture Diagram

```mermaid
flowchart TD
    subgraph Frontend["Client-Side Presentation Layer (Browser)"]
        UI["Modern Web GUI (HTML5 / CSS3 / Vanilla JS)"]
        Modules["13 Interactive Modules (Dashboard, Blocks, Files, Sched, Sync, etc.)"]
        APIClient["Centralized REST Client (web/assets/js/api.js)"]
        UI --> Modules --> APIClient
    end

    subgraph Transport["Network & Transport Layer"]
        HTTP["HTTP / JSON REST API (Port 8080)"]
        CrowServer["Crow C++ Embedded Web Server (v1.3.4)"]
        Asio["Standalone Asio (v1.30.2) Socket Reactor"]
        APIClient <-->|Fetch API (HTTP)| HTTP
        HTTP <--> CrowServer
        CrowServer <--> Asio
    end

    subgraph Backend["C++17 Core Engine Layer"]
        ServerCtx["ServerContext (Dependency Injection Container)"]
        VFS["StorageEngine (Virtual File System)"]
        Alloc["BlockAllocator (First-Fit, Best-Fit, Worst-Fit)"]
        Cache["CacheManager (FIFO & LRU Page Replacement)"]
        Sched["ProcessScheduler (FCFS, SJF, SRTF, RR, Priority)"]
        Mem["MemoryManager (Variable Partitioning & Fragmentation)"]
        Sync["SynchronizationSimulator (Peterson, Bounded Buffer, RW, Dining Philosophers)"]
        Deadlock["DeadlockDetector (Banker's Algorithm & Recovery)"]
        IPC["IpcSimulator (Pipes, Message Queues, Shared Memory)"]
        Clocks["LogicalClockSystem (Lamport & Vector Clocks)"]
        Logger["OperationLogger (Persistent Audit Trail)"]
        Integrity["IntegrityChecker & RecoveryManager (Crash Replay & Orphan Detection)"]

        CrowServer --> ServerCtx
        ServerCtx --> VFS
        ServerCtx --> Alloc
        ServerCtx --> Cache
        ServerCtx --> Sched
        ServerCtx --> Mem
        ServerCtx --> Sync
        ServerCtx --> Deadlock
        ServerCtx --> IPC
        ServerCtx --> Clocks
        ServerCtx --> Logger
        ServerCtx --> Integrity
    end

    subgraph Persistence["Storage & Persistence Layer"]
        DB["Database RAII Wrapper (osvault::Database)"]
        SQLite["SQLite 3.47.0 Amalgamation Engine (Threadsafe, WAL Mode)"]
        Disk[("Physical Disk Storage (data/osvault.db + WAL/SHM)")]

        VFS --> DB
        Alloc --> DB
        Logger --> DB
        Integrity --> DB
        DB --> SQLite --> Disk
    end
```

---

## 3. Subsystem Breakdown

### 3.1 Web Routing & API Dispatcher (`src/routes/`)
- Powered by the **Crow C++ framework**, configured with standalone Asio (zero external Boost dependencies).
- Direct route registration organized cleanly by functional domain:
  - `system_routes.cpp`: System health (`/api/health`), status, seed demo data, database reset.
  - `file_routes.cpp`: Virtual file CRUD operations, payload streaming, metadata indexing.
  - `block_routes.cpp`: 1024-block allocation bitmap, block inspector, strategy switcher.
  - `cache_routes.cpp`: In-memory page cache state, FIFO/LRU stepping and comparison.
  - `scheduler_routes.cpp`: CPU scheduling algorithms, timeline generation, metrics calculation.
  - `memory_routes.cpp`: Dynamic memory partitioning, allocation, free list, fragmentation metrics.
  - `synchronization_routes.cpp`: Peterson's algorithm, Producer-Consumer, Readers-Writers, Dining Philosophers.
  - `deadlock_routes.cpp`: Banker's safety algorithm, deadlock detection, process termination.
  - `ipc_routes.cpp`: Pipes, POSIX-style message queues, shared memory with mutex locks, socket request-reply.
  - `clock_routes.cpp`: Lamport timestamps, Vector clocks, causal concurrency checks.
  - `log_routes.cpp`: Filtered, paginated SQLite event log inspection.
  - `recovery_routes.cpp`: Block integrity audits, orphan block reclamation, power-cut crash simulation.

### 3.2 Block Storage Engine & Inode Indexing (`src/storage_engine.cpp`, `src/block_allocator.cpp`)
- **Block Geometry:** 1024 logical blocks numbered 0 through 1023, each with a fixed size of 4096 bytes (4 KiB sector). Total storage capacity is exactly 4,194,304 bytes (4 MiB).
- **Indexed Allocation:** Each virtual file is assigned a unique inode entry in the database. Inodes map to an ordered array of block indices (`block_index`).
- **Allocation Strategies:**
  - `FIRST_FIT`: Scans the block index sequentially and picks the first contiguous free range of sufficient size.
  - `BEST_FIT`: Scans the entire block map and allocates from the free block run whose size is closest to the request, minimizing internal fragment size.
  - `WORST_FIT`: Scans for the largest available contiguous hole, leaving the largest remaining fragment.
- **Data Integrity:** Each block stores an SHA-256 / Adler-32 content checksum to detect corruption or incomplete writes.

### 3.3 Page Cache & Virtual Memory Paging (`src/cache_manager.cpp`)
- Simulates an in-memory page buffer pool (64 memory frames).
- Implements two classical page replacement policies:
  - **FIFO (First-In, First-Out):** Evicts the oldest loaded block frame regardless of recent accesses.
  - **LRU (Least Recently Used):** Tracks access timestamps/counters and evicts the frame that has not been accessed for the longest duration.
- Exposes step-by-step paging trace simulations with hit/miss tracking and page fault counting across customizable reference strings.

### 3.4 Process Scheduler (`src/scheduler.cpp`)
- Supports five core operating system CPU scheduling algorithms:
  1. **FCFS (First-Come, First-Served):** Non-preemptive scheduling in arrival order.
  2. **SJF (Shortest Job First):** Non-preemptive selection of shortest remaining burst time.
  3. **SRTF (Shortest Remaining Time First):** Preemptive SJF where incoming shorter bursts preempt executing processes.
  4. **Round Robin (RR):** Time-sliced preemptive execution governed by an adjustable time quantum.
  5. **Priority Scheduling:** Non-preemptive selection based on static integer priority levels.
- Generates precise Gantt chart execution slices and computes standard OS academic metrics: **Completion Time (CT)**, **Turnaround Time (TAT)**, and **Waiting Time (WT)**.

### 3.5 Dynamic Memory Manager (`src/memory_manager.cpp`)
- Simulates contiguous physical memory allocation across a 1024 KB address space.
- Tracks variable partitions via an internal doubly linked free-list.
- Dynamically measures **external fragmentation** (sum of unallocated memory trapped in blocks too small to fulfill allocations) and **internal fragmentation**.

### 3.6 Concurrency & Synchronization (`src/synchronization.cpp`)
- Demonstrates four classical concurrency models:
  - **Peterson's Algorithm:** Software mutual exclusion for two processes (`P0` and `P1`) using `flag` array and `turn` variable, rigorously proving mutual exclusion without hardware atomic primitives.
  - **Bounded Buffer (Producer-Consumer):** Circular buffer synchronized with counting semaphores (`empty`, `full`) and a binary mutex semaphore.
  - **Readers-Writers Problem:** Read-preferring solution with `read_count` tracking and `resource_mutex` protecting against writer starvations.
  - **Dining Philosophers Problem:** Five philosophers with Dijkstra's resource hierarchy solution (odd/even fork acquisition order) to mathematically guarantee freedom from deadlock and circular wait.

### 3.7 Deadlock Detection & Banker's Algorithm (`src/deadlock_detector.cpp`)
- Full implementation of Dijkstra's **Banker's Algorithm for Deadlock Avoidance**:
  - Main vectors and matrices: Available Vector $V$, Allocation Matrix $A$, Max Demand Matrix $M$, and Need Matrix $N = M - A$.
  - Executes the Safety Algorithm to determine whether the system is in a safe state, returning the complete safe sequence (e.g., $\langle P_1, P_3, P_4, P_0, P_2 \rangle$) or identifying the deadlock set.
  - Provides simulated recovery mechanisms via selective process termination.

### 3.8 Inter-Process Communication (IPC) Simulator (`src/ipc_simulator.cpp`)
- Models the three fundamental OS IPC archetypes:
  1. **Anonymous Pipes:** Unidirectional FIFO byte stream with buffer capacity bounds and blocking semantics.
  2. **Message Queues:** Structured message passing between named processes with priority and recipient filtering.
  3. **Shared Memory with Mutex Locks:** Shared buffer region guarded by an explicit mutex lock, demonstrating data race prevention and mutex ownership validation.
  4. **Socket Request-Reply:** Synchronous client-server exchange simulating loopback IPC transactions.

### 3.9 Distributed Logical Clocks (`src/logical_clock.cpp`)
- Simulates distributed system event ordering across independent processing nodes:
  - **Lamport Timestamps:** Scalar logical clocks satisfying the weak Clock Condition ($e \to e' \implies C(e) < C(e')$).
  - **Vector Clocks:** $N$-dimensional vector clocks satisfying the strong Clock Condition ($e \to e' \iff V(e) < V(e')$).
  - **Causal Concurrency Evaluator:** Directly evaluates any two vector timestamps to determine if they are casually ordered ($V_1 < V_2$, $V_2 < V_1$) or concurrent ($V_1 \parallel V_2$).

### 3.10 Crash Consistency & Integrity Recovery (`src/integrity_checker.cpp`, `src/recovery_manager.cpp`)
- Models crash consistency and crash recovery in file systems:
  - **Orphan Block Detection:** Cross-references physical block allocation records with active file inode maps. Any block marked as allocated in the bitmap that is unreferenced by any valid file is flagged as an orphan.
  - **Simulated Interrupted Write:** Allocates storage blocks on disk and simulates a sudden power loss or process termination *before* committing the corresponding inode metadata update to the Write-Ahead Log.
  - **Automated Repair:** Reclaims orphan blocks back to the free pool, restoring storage engine invariant consistency.

---

## 4. Concurrency & Thread-Safety Model

- The Crow web server operates on a multi-threaded thread pool (default 4 worker threads).
- All shared backend domain resources (StorageEngine, BlockAllocator, MemoryManager, CacheManager, etc.) are encapsulated and synchronized with RAII `std::mutex` and `std::lock_guard` guards.
- SQLite connections are protected by SQLite's threadsafe multi-threaded mode (`SQLITE_THREADSAFE=1`), serialized transaction blocks (`BEGIN IMMEDIATE`), and busy handlers with exponential backoff.
