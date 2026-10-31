# OSVault — Technical Limitations & Architectural Trade-offs

## 1. Educational Simulation vs. Real Kernel Architecture

OSVault is designed as an educational laboratory and user-space storage engine. To run safely across all developer environments without requiring root/Administrator privileges, kernel modules, or virtual machine hypervisors, certain architectural trade-offs were made.

---

## 2. Comparison Table: OSVault vs. Real OS Kernel

| Dimension | Real OS Kernel (Linux / Windows NT) | OSVault Implementation |
|---|---|---|
| **Execution Privilege** | Ring 0 (Kernel Supervisor Mode) | Ring 3 (User-Space Application) |
| **Hardware Access** | Direct MMIO, DMA controllers, and PCI bus drivers | User-space POSIX / Win32 I/O APIs and SQLite persistence |
| **CPU Preemption** | Hardware APIC timer interrupts every 1-10 ms | Algorithmic discrete-time simulation slices |
| **Memory Management** | Hardware MMU, Page Tables (CR3 register), TLB caches | Simulated 1024 KB partition model with doubly linked free-lists |
| **Storage Substrate** | Block device drivers (`/dev/nvme0n1`, ATA/SATA controller) | 1024 logical sectors of 4096 bytes indexed in SQLite WAL database |
| **Crash Consistency** | Journaling file systems (ext4, XFS) with hardware write barriers (`fsync`, `fdatasync`) | SQLite Write-Ahead Logging (WAL) and orphan block verification engine |
| **Process Model** | Full PCB with registers, stack frames, and virtual address spaces | Process descriptor structs with arrival, burst, and priority attributes |

---

## 3. Storage & Crash Consistency Realities

### 3.1 Simulated Power-Loss Crash vs. Physical Power Cut
- In OSVault, an interrupted write is simulated by executing physical block allocations in the block table while intentionally omitting the inode metadata transaction commit.
- In a physical computer, sudden power loss also involves:
  - Hardware volatile DRAM write caches inside the SSD/HDD controller losing uncommitted sectors unless battery-backed.
  - Potential partial sector writes (torn writes), where less than 4096 bytes are persisted during a voltage drop.
  - SQLite's WAL mode protects against torn pages via checksummed write-ahead log frames, providing an accurate model of how modern journaling file systems recover after ungraceful restarts.

### 3.2 In-Memory Page Cache
- OSVault's `CacheManager` models 64 cache frames using FIFO and LRU replacement policies in RAM.
- In a production operating system kernel:
  - The OS unified page cache dynamically scales to consume all available unallocated physical RAM.
  - Dirty page flushing is performed asynchronously by kernel background threads (e.g., Linux `pdflush` / `kswapd`).

---

## 4. Concurrency & Synchronization Abstractions

- **Peterson's Algorithm:**
  - Modern superscalar CPUs feature out-of-order execution and store buffering, requiring hardware memory fences (`mfence` or `std::atomic_thread_fence`) to guarantee Peterson's algorithm works correctly on physical hardware.
  - OSVault models Peterson's algorithm sequentially at the algorithmic state-machine level, demonstrating its mathematical correctness as originally conceived by Gary Peterson in 1981.
- **Dining Philosophers & Banker's Algorithm:**
  - Real OS kernels rarely employ Banker's algorithm for general process memory allocation because processes cannot predict their maximum resource demand in advance.
  - However, Banker's algorithm remains the foundational academic benchmark for demonstrating safe states, need matrices, and circular wait avoidance.

---

## 5. Pedagogical Strengths of the OSVault Approach

Despite operating in user-space, OSVault provides distinct educational advantages:
1. **Determinism:** Algorithms can be stepped forward, paused, and inspected cell-by-cell without timing jitter.
2. **Safety:** Evaluators and students can simulate storage corruption, power failures, and deadlocks without risking the host operating system or filesystem.
3. **Transparency:** Every block, inode, mutex, and vector timestamp is fully visible and inspectable in real-time through the web interface.
