# OSVault — Testing & Quality Assurance Strategy

## 1. Testing Philosophy & Framework

To guarantee pedagogical accuracy, algorithmic correctness, and crash resilience, OSVault implements a multi-tier automated testing strategy powered by:
- **Test Framework:** `doctest` (v2.4.11 pinned in `third_party/doctest/doctest.h`), an ultra-fast, lightweight C++17 unit-testing library.
- **Test Runner:** CMake `CTest` integrated with CTest test discovery.
- **Coverage Domains:**
  1. Unit tests for mathematical & OS algorithm correctness.
  2. Integration tests for SQLite WAL transactions, multi-block file I/O, and crash consistency.
  3. In-process HTTP route contract validation without external network dependencies.

---

## 2. Test Suite Breakdown (10 Test Suites)

| Suite # | Test Target | Focus & Invariants Verified |
|---|---|---|
| **1** | `test_database` | SQLite RAII lifecycle, WAL pragma verification, table migrations, multi-threaded connection handling, atomic transaction rollback on failure. |
| **2** | `test_storage` | Virtual file creation, multi-block spanning ($> 4096$ B), indexed inode mapping, FIRST_FIT, BEST_FIT, and WORST_FIT allocation heuristics, block freeing on deletion. |
| **3** | `test_cache` | Buffer pool hits and misses, FIFO eviction queue, LRU access recency updates, page fault counter correctness. |
| **4** | `test_scheduler` | FCFS, SJF, SRTF (preemptive), Round Robin with quantum slicing, Priority scheduling; mathematical validation of $\text{TAT} = \text{CT} - \text{AT}$ and $\text{WT} = \text{TAT} - \text{BT}$. |
| **5** | `test_memory` | 1024 KB dynamic partition simulation, free-list coalescence, First/Best/Worst Fit block placement, external fragmentation calculation. |
| **6** | `test_deadlock` | Banker's Algorithm safety check, correct safe sequence deduction, unsafe state rejection, deadlock set identification, simulated process termination recovery. |
| **7** | `test_synchronization` | Peterson's mutual exclusion invariant ($P_0$ and $P_1$ never simultaneously in CS), Bounded Buffer semaphore capacity bounds, Readers-Writers read concurrency & writer exclusivity, Dining Philosophers freedom from deadlock. |
| **8** | `test_clocks` | Lamport timestamp monotonicity ($L_{\text{recv}} = \max(L_{\text{recv}}, L_{\text{msg}}) + 1$), Vector clock causal tracking, vector concurrency check ($V_1 \parallel V_2$). |
| **9** | `test_recovery` | Storage engine integrity checker, orphan block detection after uncommitted writes, automated orphan reclamation, restart persistence across DB re-opens. |
| **10** | `test_api` | In-process Crow HTTP routing verification, JSON request parsing, response serialization, HTTP 200/400/404 status codes. |

---

## 3. Actual Execution Results

Running the automated test suite via `ctest` yields 100% pass rate:

```
==========================================================
 Running OSVault CTest Automated Test Suite (Release)
==========================================================
Test project C:/Users/khush/Downloads/OSVault_reconstructed/build
      Start  1: test_database
 1/10 Test  #1: test_database ....................   Passed    0.10 sec
      Start  2: test_storage
 2/10 Test  #2: test_storage .....................   Passed    0.02 sec
      Start  3: test_cache
 3/10 Test  #3: test_cache .......................   Passed    0.02 sec
      Start  4: test_scheduler
 4/10 Test  #4: test_scheduler ...................   Passed    0.02 sec
      Start  5: test_memory
 5/10 Test  #5: test_memory ......................   Passed    0.02 sec
      Start  6: test_deadlock
 6/10 Test  #6: test_deadlock ....................   Passed    0.02 sec
      Start  7: test_synchronization
 7/10 Test  #7: test_synchronization .............   Passed    0.02 sec
      Start  8: test_clocks
 8/10 Test  #8: test_clocks ......................   Passed    0.03 sec
      Start  9: test_recovery
 9/10 Test  #9: test_recovery ....................   Passed    0.03 sec
      Start 10: test_api
10/10 Test #10: test_api .........................   Passed    0.02 sec

100% tests passed out of 10

Total Test time (real) =   0.33 sec
==========================================================
 ALL TEST SUITES PASSED (10/10)
```

---

## 4. How to Run Tests

### Via PowerShell Automation Script:
```powershell
.\scripts\test.ps1
```

### Via CTest Directly:
```powershell
ctest --test-dir build -C Release --output-on-failure
```

### Running Specific Test Cases:
```powershell
.\build\tests\Release\osvault_tests.exe -tc="*Scheduler*"
```
