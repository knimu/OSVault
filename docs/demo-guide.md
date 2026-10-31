# OSVault — Academic Demonstration & Viva Defense Guide

This guide is structured as a step-by-step demonstration walkthrough for an academic project evaluation, lab viva, or software architecture review.

---

## 1. Quick Start for Evaluators

1. **Verify Environment:**
   ```powershell
   .\scripts\setup.ps1
   ```
2. **Execute Automated Verification Suite:**
   ```powershell
   .\scripts\verify.ps1
   ```
   *(Executes all 10 CTest suites, validates binaries, verifies asset deployment, and checks API contracts).*
3. **Launch Server:**
   ```powershell
   .\scripts\run.ps1
   ```
4. **Open Browser:**
   Navigate to `http://localhost:8080/`

---

## 2. 10-Minute Live Demonstration Flow

### Step 1: Architecture & Technology Stack (1 Minute)
- **What to show:** Open the browser to the **Dashboard** (`#dashboard`).
- **Talking points:**
  - *"OSVault is built using modern C++17 with an embedded Crow HTTP server and standalone Asio. It requires zero Python, Node.js, or external runtimes."*
  - *"All persistence is backed by SQLite 3 in Write-Ahead Logging (WAL) mode, guaranteeing ACID transactions and crash durability."*
  - *"The frontend is pure Vanilla JavaScript, HTML5, and CSS3, served directly by the C++ backend on port 8080."*

---

### Step 2: Virtual Block Storage Engine (2 Minutes)
- **What to show:** Navigate to **Block Storage (1024)** (`#blocks`) and **Virtual Filesystem** (`#files`).
- **Demonstration:**
  1. Click **"+ Create File"** in the Virtual Filesystem.
  2. Name it `kernel_manifest.dat` and input ~5000 characters (exceeding 4096 bytes).
  3. Observe that the file is dynamically partitioned into 2 discrete 4096-byte blocks.
  4. Return to the **1024-Block Grid**:
     - See the newly allocated blocks lit up in blue.
     - Click on the allocated block to inspect its sector index, file ID, chunk index, and SHA-256 checksum.
  5. Demonstrate **Allocation Strategies**:
     - Toggle between `FIRST_FIT`, `BEST_FIT`, and `WORST_FIT` to explain how free list searching differs.

---

### Step 3: Page Replacement & Paging Simulation (1.5 Minutes)
- **What to show:** Navigate to **Page Cache & Paging** (`#cache`).
- **Demonstration:**
  1. Use the reference string: `7, 0, 1, 2, 0, 3, 0, 4, 2, 3, 0, 3, 2, 1, 2, 0, 1, 7, 0, 1` with 3 frames.
  2. Run **FIFO**: Note the total page faults (typically 15 faults).
  3. Run **LRU**: Note the lower page fault count (typically 12 faults).
  4. Explain how LRU leverages the principle of temporal locality by evicting the least recently referenced frame.

---

### Step 4: CPU Scheduling & Gantt Chart (2 Minutes)
- **What to show:** Navigate to **CPU Scheduler** (`#scheduler`).
- **Demonstration:**
  1. Review the default queue of processes ($P_1, P_2, P_3, P_4$) with varied arrival times and burst times.
  2. Select **Round Robin (RR)** with time quantum $q = 2$. Click **"Run Scheduler"**.
  3. Point to the generated **Gantt Chart**: explain how time-slicing and context switching operate when quantum expires.
  4. Point to the **Metrics Table**: explain that $\text{Turnaround Time} = \text{Completion Time} - \text{Arrival Time}$ and $\text{Waiting Time} = \text{Turnaround Time} - \text{Burst Time}$.
  5. Click **"Compare All"**: demonstrate how Shortest Job First (SJF/SRTF) minimizes average waiting time compared to FCFS.

---

### Step 5: Dynamic Memory Allocation & Fragmentation (1.5 Minutes)
- **What to show:** Navigate to **Memory Allocation** (`#memory`).
- **Demonstration:**
  1. Allocate `Proc_A` (256 KB), `Proc_B` (128 KB), and `Proc_C` (256 KB).
  2. Deallocate `Proc_B` (middle block).
  3. Show the **Physical Memory Map**: point to the 128 KB hole trapped between `Proc_A` and `Proc_C`.
  4. Show the **External Fragmentation Metric**: explain why an incoming 200 KB process cannot be allocated despite 384 KB of total free space, illustrating the necessity of paging or compaction.

---

### Step 6: Concurrency & Synchronization (2 Minutes)
- **What to show:** Navigate to **Synchronization** (`#sync`).
- **Demonstration:**
  1. **Peterson's Algorithm:** Step $P_0$ (sets flag, yields turn). Step $P_1$. Show that mutual exclusion is strictly maintained and only one process enters the Critical Section at any time.
  2. **Bounded Buffer:** Produce 5 items until the buffer is full. Show that the `empty` semaphore decrements to 0 and subsequent produce attempts block until `Consume` is pressed.
  3. **Dining Philosophers:** Step philosophers through Thinking $\to$ Hungry $\to$ Eating $\to$ Thinking. Explain how Dijkstra's resource hierarchy eliminates circular wait deadlock.

---

### Step 7: Deadlock & Banker's Algorithm (1.5 Minutes)
- **What to show:** Navigate to **Deadlock & Banker's** (`#deadlock`).
- **Demonstration:**
  1. Click **"Load Safe Preset"** and run **"Analyze Deadlock / Safety"**:
     - System outputs: *"System is in a SAFE state. Safe sequence: $\langle P_1, P_3, P_4, P_0, P_2 \rangle$"*.
  2. Click **"Load Deadlock Preset"**:
     - System detects circular wait and highlights the deadlocked processes.
  3. Click **"Recover (Terminate Deadlocked)"**:
     - Engine preempts the culprit process, releases its allocated resources to the available vector, and restores system safety.

---

### Step 8: Distributed Logical Clocks (1 Minute)
- **What to show:** Navigate to **Logical Clocks** (`#clocks`).
- **Demonstration:**
  1. Trigger local events on Node 0 and Node 1.
  2. Send a message from Node 0 to Node 1. Observe receiver vector update: $V_{\text{recv}}[i] = \max(V_{\text{recv}}[i], V_{\text{msg}}[i])$.
  3. In the **Causal Concurrency Checker**, evaluate $V_1 = [1, 0, 0]$ and $V_2 = [0, 1, 0]$:
     - Result: `CONCURRENT (V1 || V2)`. Explain that neither event causally influenced the other in distributed time.

---

### Step 9: Crash Consistency & Orphan Block Recovery (1.5 Minutes)
- **What to show:** Navigate to **Crash Recovery** (`#recovery`).
- **Demonstration:**
  1. Click **"Run Integrity Check"**: status reports `HEALTHY (0 orphan blocks)`.
  2. Click **"Simulate Interrupted Write (Power Loss Crash)"**:
     - The engine allocates a physical sector on disk, but intentionally aborts the transaction before committing the inode metadata to SQLite.
  3. Click **"Run Integrity Check"**:
     - Status changes to `FAIL (Inconsistencies Detected)`.
     - Orphan block is highlighted.
  4. Click **"Fix Orphan Blocks"**:
     - The recovery engine scans unreferenced allocations, frees the leaked block, and returns the storage engine to a 100% consistent state.

---

## 3. Frequently Asked Viva / Defense Questions

### Q1: Why use SQLite in an Operating Systems project rather than writing a raw disk driver?
**Answer:** Real-world operating systems use layered abstractions. SQLite acts as our crash-consistent WAL storage substrate, allowing us to accurately model indexed inodes, sector bitmaps, and crash consistency without requiring elevated kernel privileges or risking the host filesystem.

### Q2: How does Peterson's algorithm guarantee mutual exclusion?
**Answer:** Mutual exclusion relies on two variables: `flag[i] = true` (signaling intent) and `turn = j` (yielding priority). For both processes to be in the critical section simultaneously, `flag[0] == true`, `flag[1] == true`, `turn == 0`, and `turn == 1` would have to be true at the same time, which is impossible on sequential memory.

### Q3: What is the difference between Lamport Clocks and Vector Clocks?
**Answer:** Lamport timestamps provide a partial order satisfying the weak clock condition ($A \to B \implies L(A) < L(B)$). However, $L(A) < L(B)$ does *not* imply $A \to B$. Vector clocks provide the strong clock condition ($A \to B \iff V(A) < V(B)$), allowing distributed systems to detect whether two events are casually ordered or concurrent.
