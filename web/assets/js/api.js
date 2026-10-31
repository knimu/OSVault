/**
 * OSVault Centralized API Client
 * Wraps all C++ Crow backend REST endpoints.
 */
export const API = {
  baseUrl: '',

  async request(endpoint, options = {}) {
    const url = this.baseUrl + endpoint;
    const defaultHeaders = {
      'Accept': 'application/json',
      'Content-Type': 'application/json'
    };

    try {
      const response = await fetch(url, {
        ...options,
        headers: {
          ...defaultHeaders,
          ...(options.headers || {})
        }
      });

      const data = await response.json().catch(() => ({}));

      if (!response.ok) {
        const errorMsg = data.error || `HTTP ${response.status}: ${response.statusText}`;
        throw new Error(errorMsg);
      }

      return data;
    } catch (err) {
      console.error(`[API Error] ${options.method || 'GET'} ${endpoint}:`, err);
      throw err;
    }
  },

  // System & Overview
  getHealth() { return this.request('/api/health'); },
  getMetrics() { return this.request('/api/metrics'); },
  resetDemoData(confirmToken) {
    return this.request('/api/system/reset-demo-data', {
      method: 'POST',
      body: JSON.stringify({ confirm: confirmToken })
    });
  },

  // Files
  listFiles(query = '') {
    const q = query ? `?q=${encodeURIComponent(query)}` : '';
    return this.request(`/api/files${q}`);
  },
  readFile(filename) { return this.request(`/api/files/${encodeURIComponent(filename)}`); },
  createFile(name, content = '') {
    return this.request('/api/files', {
      method: 'POST',
      body: JSON.stringify({ name, content })
    });
  },
  writeFile(name, content) {
    return this.request(`/api/files/${encodeURIComponent(name)}`, {
      method: 'PUT',
      body: JSON.stringify({ content })
    });
  },
  renameFile(oldName, newName) {
    return this.request(`/api/files/${encodeURIComponent(oldName)}/rename`, {
      method: 'POST',
      body: JSON.stringify({ new_name: newName })
    });
  },
  deleteFile(name) {
    return this.request(`/api/files/${encodeURIComponent(name)}`, {
      method: 'DELETE'
    });
  },

  // Block Allocator Visualizer
  getBlocks() { return this.request('/api/blocks'); },
  getBlock(blockId) { return this.request(`/api/blocks/${blockId}`); },

  // Cache & Paging
  getCacheStats() { return this.request('/api/cache/stats'); },
  setCacheConfig(policy, capacity) {
    return this.request('/api/cache/config', {
      method: 'POST',
      body: JSON.stringify({ policy, capacity })
    });
  },
  clearCache() { return this.request('/api/cache/clear', { method: 'POST' }); },
  simulatePaging(sequence, frames, algorithm = 'BOTH') {
    return this.request('/api/cache/simulate', {
      method: 'POST',
      body: JSON.stringify({ sequence, frames, algorithm })
    });
  },

  // Process Scheduling
  simulateScheduler(processes, timeQuantum = 2, algorithm = 'ALL') {
    return this.request('/api/scheduler/simulate', {
      method: 'POST',
      body: JSON.stringify({ processes, time_quantum: timeQuantum, algorithm })
    });
  },

  // Memory Management
  getMemoryState() { return this.request('/api/memory/state'); },
  allocateMemory(processName, size, strategy = 'FIRST_FIT') {
    return this.request('/api/memory/allocate', {
      method: 'POST',
      body: JSON.stringify({ process_name: processName, size, strategy })
    });
  },
  deallocateMemory(param) {
    return this.request('/api/memory/deallocate', {
      method: 'POST',
      body: JSON.stringify(typeof param === 'number' ? { partition_id: param } : { process_name: param })
    });
  },
  resetMemory(totalSize = 1024) {
    return this.request('/api/memory/reset', {
      method: 'POST',
      body: JSON.stringify({ total_size: totalSize })
    });
  },

  // Synchronization
  getCsState() { return this.request('/api/sync/cs'); },
  stepCs(threadId) {
    return this.request('/api/sync/cs/step', {
      method: 'POST',
      body: JSON.stringify({ thread_id: threadId })
    });
  },
  resetCs() { return this.request('/api/sync/cs/reset', { method: 'POST' }); },

  getPcState() { return this.request('/api/sync/pc'); },
  producePc(item) {
    return this.request('/api/sync/pc/produce', {
      method: 'POST',
      body: JSON.stringify({ item })
    });
  },
  consumePc() { return this.request('/api/sync/pc/consume', { method: 'POST' }); },
  resetPc(capacity = 5) {
    return this.request('/api/sync/pc/reset', {
      method: 'POST',
      body: JSON.stringify({ capacity })
    });
  },

  getRwState() { return this.request('/api/sync/rw'); },
  startReadRw(readerId) {
    return this.request('/api/sync/rw/read/start', {
      method: 'POST',
      body: JSON.stringify({ reader_id: readerId })
    });
  },
  stopReadRw(readerId) {
    return this.request('/api/sync/rw/read/stop', {
      method: 'POST',
      body: JSON.stringify({ reader_id: readerId })
    });
  },
  startWriteRw(writerId) {
    return this.request('/api/sync/rw/write/start', {
      method: 'POST',
      body: JSON.stringify({ writer_id: writerId })
    });
  },
  stopWriteRw(writerId, value) {
    return this.request('/api/sync/rw/write/stop', {
      method: 'POST',
      body: JSON.stringify({ writer_id: writerId, value })
    });
  },
  resetRw() { return this.request('/api/sync/rw/reset', { method: 'POST' }); },

  getDpState() { return this.request('/api/sync/dp'); },
  stepDp(philosopherId) {
    return this.request('/api/sync/dp/step', {
      method: 'POST',
      body: JSON.stringify({ philosopher_id: philosopherId })
    });
  },
  resetDp() { return this.request('/api/sync/dp/reset', { method: 'POST' }); },

  // Deadlock Detection
  analyzeDeadlock(processes, resources, available, allocation, request) {
    return this.request('/api/deadlock/analyze', {
      method: 'POST',
      body: JSON.stringify({ processes, resources, available, allocation, request })
    });
  },
  getDeadlockPreset(type) { return this.request(`/api/deadlock/preset/${type}`); },
  recoverDeadlock(state, terminateProcess) {
    return this.request('/api/deadlock/recover', {
      method: 'POST',
      body: JSON.stringify({ state, terminate_process: terminateProcess })
    });
  },

  // IPC
  getPipe() { return this.request('/api/ipc/pipe'); },
  writePipe(data) {
    return this.request('/api/ipc/pipe/write', {
      method: 'POST',
      body: JSON.stringify({ data })
    });
  },
  readPipe(bytes = 256) {
    return this.request('/api/ipc/pipe/read', {
      method: 'POST',
      body: JSON.stringify({ bytes })
    });
  },
  resetPipe() { return this.request('/api/ipc/pipe/reset', { method: 'POST' }); },

  getMq() { return this.request('/api/ipc/mq'); },
  sendMq(sender, receiver, message) {
    return this.request('/api/ipc/mq/send', {
      method: 'POST',
      body: JSON.stringify({ sender, receiver, message })
    });
  },
  receiveMq(receiver) {
    return this.request('/api/ipc/mq/receive', {
      method: 'POST',
      body: JSON.stringify({ receiver })
    });
  },
  clearMq() { return this.request('/api/ipc/mq/clear', { method: 'POST' }); },

  getShm() { return this.request('/api/ipc/shm'); },
  readShm(reader) {
    return this.request('/api/ipc/shm/read', {
      method: 'POST',
      body: JSON.stringify({ reader })
    });
  },
  writeShm(writer, data) {
    return this.request('/api/ipc/shm/write', {
      method: 'POST',
      body: JSON.stringify({ writer, data })
    });
  },
  lockShm(process) {
    return this.request('/api/ipc/shm/lock', {
      method: 'POST',
      body: JSON.stringify({ process })
    });
  },
  unlockShm(process) {
    return this.request('/api/ipc/shm/unlock', {
      method: 'POST',
      body: JSON.stringify({ process })
    });
  },
  exchangeSocket(client, requestStr) {
    return this.request('/api/ipc/exchange', {
      method: 'POST',
      body: JSON.stringify({ client, request: requestStr })
    });
  },
  getIpcHistory() { return this.request('/api/ipc/history'); },

  // Logical Clocks
  getClockNodes() { return this.request('/api/clocks/nodes'); },
  getClockTimeline() { return this.request('/api/clocks/timeline'); },
  triggerLocalEvent(nodeId, description) {
    return this.request('/api/clocks/local-event', {
      method: 'POST',
      body: JSON.stringify({ node_id: nodeId, description })
    });
  },
  sendClockMessage(fromNode, toNode, message) {
    return this.request('/api/clocks/send-message', {
      method: 'POST',
      body: JSON.stringify({ from_node: fromNode, to_node: toNode, message })
    });
  },
  resetClocks(nodeCount = 3) {
    return this.request('/api/clocks/reset', {
      method: 'POST',
      body: JSON.stringify({ node_count: nodeCount })
    });
  },
  checkConcurrency(v1, v2) {
    return this.request('/api/clocks/concurrency-check', {
      method: 'POST',
      body: JSON.stringify({ v1, v2 })
    });
  },

  // Operation Logs
  getLogs(eventType = '', query = '', limit = 50, offset = 0) {
    const params = new URLSearchParams();
    if (eventType) params.append('event_type', eventType);
    if (query) params.append('q', query);
    params.append('limit', limit);
    params.append('offset', offset);
    return this.request(`/api/logs?${params.toString()}`);
  },
  clearLogs() { return this.request('/api/logs/clear', { method: 'POST' }); },

  // Integrity & Recovery
  checkIntegrity() { return this.request('/api/integrity/check'); },
  fixOrphanBlocks() { return this.request('/api/integrity/fix-orphans', { method: 'POST' }); },
  simulateInterruptedWrite(filename) {
    return this.request('/api/recovery/interrupted-write', {
      method: 'POST',
      body: JSON.stringify({ filename })
    });
  },
  simulateRestartPersistence() {
    return this.request('/api/recovery/restart-persistence', { method: 'POST' });
  },
  getRecoveryHistory() { return this.request('/api/recovery/history'); }
};
