# OSVault — REST API Specification

## 1. Global Conventions

- **Base URL:** `http://127.0.0.1:8080`
- **Content-Type:** `application/json; charset=utf-8`
- **Standard Envelope:**
  ```json
  {
    "success": true,
    "data": { ... }
  }
  ```
- **Error Response Format:**
  ```json
  {
    "success": false,
    "error": "Error description message"
  }
  ```

---

## 2. System & Health Endpoints

### `GET /api/health`
Health check endpoint used by orchestrators, automated verifiers, and UI ping indicators.
- **Response (200 OK):**
  ```json
  {
    "status": "ok",
    "timestamp": 1730370000,
    "uptime_seconds": 120
  }
  ```

### `GET /api/system/status`
Returns high-level runtime metadata, block allocation stats, and memory utilization.
- **Response (200 OK):**
  ```json
  {
    "success": true,
    "data": {
      "version": "1.0.0",
      "server": "Crow C++ (Release)",
      "total_blocks": 1024,
      "block_size": 4096,
      "allocated_blocks": 48,
      "free_blocks": 976,
      "total_files": 3,
      "cache_hits": 12,
      "cache_misses": 4,
      "allocation_strategy": "FIRST_FIT"
    }
  }
  ```

### `POST /api/system/seed`
Seeds sample files into the virtual file system for demonstration.
- **Response (200 OK):**
  ```json
  {
    "success": true,
    "data": {
      "seeded_files": ["kernel_boot.cfg", "process_table.dat", "memory_map.sys"],
      "message": "Demo files successfully seeded."
    }
  }
  ```

---

## 3. Virtual File System Endpoints

### `GET /api/files`
List all virtual files.
- **Response (200 OK):**
  ```json
  {
    "success": true,
    "data": {
      "files": [
        {
          "id": 1,
          "name": "sample.txt",
          "size_bytes": 1024,
          "block_count": 1,
          "checksum": "a4f89b...",
          "created_at": 1730370000,
          "updated_at": 1730370000
        }
      ]
    }
  }
  ```

### `POST /api/files`
Create a new virtual file.
- **Request:**
  ```json
  {
    "name": "demo.dat",
    "content": "Hello OSVault File System!"
  }
  ```
- **Response (200 OK):**
  ```json
  {
    "success": true,
    "data": {
      "id": 2,
      "name": "demo.dat",
      "size_bytes": 26,
      "blocks_allocated": [14]
    }
  }
  ```

### `DELETE /api/files/<id>`
Delete a virtual file and reclaim all its allocated storage blocks.
- **Response (200 OK):**
  ```json
  {
    "success": true,
    "data": {
      "deleted_id": 2,
      "freed_blocks": [14]
    }
  }
  ```

---

## 4. Block Storage Visualizer Endpoints

### `GET /api/blocks/map`
Returns the complete 1024-block allocation bitmap and metadata.
- **Response (200 OK):**
  ```json
  {
    "success": true,
    "data": {
      "total_blocks": 1024,
      "allocated_count": 1,
      "free_count": 1023,
      "strategy": "FIRST_FIT",
      "blocks": [
        { "index": 0, "is_allocated": true, "file_id": 1 },
        { "index": 1, "is_allocated": false, "file_id": null }
      ]
    }
  }
  ```

### `POST /api/blocks/strategy`
Changes active block allocation strategy (`FIRST_FIT`, `BEST_FIT`, `WORST_FIT`).
- **Request:** `{ "strategy": "BEST_FIT" }`
- **Response (200 OK):** `{ "success": true, "data": { "strategy": "BEST_FIT" } }`

---

## 5. CPU Scheduler Endpoints

### `POST /api/scheduler/simulate`
Simulates a set of process workloads under a chosen scheduling algorithm.
- **Request:**
  ```json
  {
    "algorithm": "RR",
    "quantum": 2,
    "processes": [
      { "pid": "P1", "arrival": 0, "burst": 5, "priority": 1 },
      { "pid": "P2", "arrival": 1, "burst": 3, "priority": 2 }
    ]
  }
  ```
- **Response (200 OK):**
  ```json
  {
    "success": true,
    "data": {
      "algorithm": "RR",
      "quantum": 2,
      "gantt_chart": [
        { "pid": "P1", "start": 0, "end": 2 },
        { "pid": "P2", "start": 2, "end": 4 },
        { "pid": "P1", "start": 4, "end": 6 },
        { "pid": "P2", "start": 6, "end": 7 },
        { "pid": "P1", "start": 7, "end": 8 }
      ],
      "metrics": [
        { "pid": "P1", "completion": 8, "turnaround": 8, "waiting": 3 },
        { "pid": "P2", "completion": 7, "turnaround": 6, "waiting": 3 }
      ],
      "avg_turnaround": 7.0,
      "avg_waiting": 3.0,
      "cpu_utilization": 100.0
    }
  }
  ```

---

## 6. Deadlock & Banker's Algorithm Endpoints

### `POST /api/deadlock/analyze`
Executes Banker's safety algorithm on provided resource vectors and matrices.
- **Request:**
  ```json
  {
    "available": [3, 3, 2],
    "allocation": [
      [0, 1, 0],
      [2, 0, 0],
      [3, 0, 2],
      [2, 1, 1],
      [0, 0, 2]
    ],
    "request": [
      [7, 5, 3],
      [3, 2, 2],
      [9, 0, 2],
      [2, 2, 2],
      [4, 3, 3]
    ]
  }
  ```
- **Response (200 OK):**
  ```json
  {
    "success": true,
    "data": {
      "is_safe": true,
      "safe_sequence": ["P1", "P3", "P4", "P0", "P2"],
      "deadlocked_processes": []
    }
  }
  ```

---

## 7. Distributed Logical Clocks Endpoints

### `POST /api/clocks/send-message`
Simulates message passing from one node to another, updating Lamport and Vector clocks.
- **Request:**
  ```json
  {
    "from_node": 0,
    "to_node": 1,
    "message": "State Sync"
  }
  ```
- **Response (200 OK):**
  ```json
  {
    "success": true,
    "data": {
      "from_node": 0,
      "to_node": 1,
      "sender_vector": [1, 0, 0],
      "receiver_vector": [1, 1, 0],
      "sender_lamport": 1,
      "receiver_lamport": 2
    }
  }
  ```

### `POST /api/clocks/concurrency-check`
Compares two vector clocks for causal precedence or concurrency.
- **Request:**
  ```json
  {
    "v1": [1, 0, 0],
    "v2": [0, 1, 0]
  }
  ```
- **Response (200 OK):**
  ```json
  {
    "success": true,
    "data": {
      "relation": "CONCURRENT (V1 || V2)",
      "is_concurrent": true
    }
  }
  ```

---

## 8. Crash Recovery Endpoints

### `GET /api/integrity/check`
Executes an audit scan across all 1024 blocks and file inodes.
- **Response (200 OK):**
  ```json
  {
    "success": true,
    "data": {
      "is_consistent": true,
      "total_files": 3,
      "allocated_blocks": 6,
      "orphan_blocks": 0,
      "corrupted_blocks": 0,
      "details": []
    }
  }
  ```

### `POST /api/integrity/fix-orphans`
Scans for unreferenced allocated blocks and reclaims them back to the free block pool.
- **Response (200 OK):**
  ```json
  {
    "success": true,
    "data": {
      "repaired_count": 2,
      "reclaimed_block_indices": [45, 46],
      "message": "Orphan blocks repaired."
    }
  }
  ```
