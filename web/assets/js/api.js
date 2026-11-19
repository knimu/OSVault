/**
 * OSVault Centralized API Client (Plain JS / No ES6 module exports for maximum browser compatibility)
 * Wraps all C++ Crow backend REST endpoints.
 */

window.api = {
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

      if (!response.ok || data.ok === false) {
        const errorMsg = data.error || `HTTP ${response.status}: ${response.statusText}`;
        throw new Error(errorMsg);
      }

      data.success = true;
      return data;
    } catch (err) {
      console.error(`[API Error] ${options.method || 'GET'} ${endpoint}:`, err);
      throw err;
    }
  },

  // System & Overview
  getHealth() { return this.request('/api/health'); },
  getMetrics() { return this.request('/api/metrics'); },
  resetDemoData(confirmToken = 'RESET_CONFIRM') {
    return this.request('/api/system/reset-demo-data', {
      method: 'POST',
      body: JSON.stringify({ confirm: confirmToken })
    });
  },

  // Files
  listFiles(query = '', directoryId = 0) {
    const params = new URLSearchParams();
    if (query) params.append('q', query);
    params.append('directory_id', directoryId);
    return this.request(`/api/files?${params.toString()}`);
  },
  readFile(name, directoryId = 0) {
    return this.request(`/api/files/${encodeURIComponent(name)}?directory_id=${directoryId}`);
  },
  createFile(name, content = '', directoryId = 0) {
    return this.request('/api/files', {
      method: 'POST',
      body: JSON.stringify({ name, content, directory_id: directoryId })
    });
  },
  writeFile(name, content, directoryId = 0) {
    return this.request(`/api/files/${encodeURIComponent(name)}`, {
      method: 'PUT',
      body: JSON.stringify({ content, directory_id: directoryId })
    });
  },
  renameFile(oldName, newName, directoryId = 0) {
    return this.request(`/api/files/${encodeURIComponent(oldName)}/rename`, {
      method: 'POST',
      body: JSON.stringify({ new_name: newName, directory_id: directoryId })
    });
  },
  deleteFile(name, directoryId = 0) {
    return this.request(`/api/files/${encodeURIComponent(name)}?directory_id=${directoryId}`, {
      method: 'DELETE'
    });
  },
  listDirectories(parentId = 0) {
    return this.request(`/api/directories?directory_id=${parentId}`);
  },
  createDirectory(name, parentId = 0) {
    return this.request('/api/directories', {
      method: 'POST',
      body: JSON.stringify({ name, parent_id: parentId })
    });
  },
  deleteDirectory(directoryId) {
    return this.request(`/api/directories/${directoryId}`, { method: 'DELETE' });
  },

  // Blocks
  getBlocks() { return this.request('/api/blocks'); },
  getBlock(id) { return this.request(`/api/blocks/${id}`); },
  setBlockStrategy(strategy) {
    return this.request('/api/blocks/strategy', {
      method: 'POST',
      body: JSON.stringify({ strategy })
    });
  },

  // Cache & Paging
  getCacheStats() { return this.request('/api/cache/stats'); },
  setCacheConfig(capacity, policy) {
    return this.request('/api/cache/config', {
      method: 'POST',
      body: JSON.stringify({ capacity, policy })
    });
  },
  clearCache() {
    return this.request('/api/cache/clear', { method: 'POST' });
  },
  simulatePaging(sequence, frames = 3, policy = 'FIFO') {
    return this.request('/api/cache/simulate', {
      method: 'POST',
      body: JSON.stringify({ sequence, frames, policy })
    });
  },

  // CPU Scheduler
  simulateScheduler(processes, algorithm = 'FCFS', quantum = 2) {
    return this.request('/api/scheduler/simulate', {
      method: 'POST',
      body: JSON.stringify({ processes, algorithm, quantum })
    });
  },

  // Memory Management
  getMemoryState() { return this.request('/api/memory/state'); },
  allocateMemory(processName, sizeKb, strategy = 'FIRST_FIT') {
    return this.request('/api/memory/allocate', {
      method: 'POST',
      body: JSON.stringify({ process_name: processName, size: sizeKb, strategy })
    });
  },
  deallocateMemory(partitionId) {
    return this.request('/api/memory/deallocate', {
      method: 'POST',
      body: JSON.stringify({ partition_id: partitionId })
    });
  },
  resetMemory() {
    return this.request('/api/memory/reset', { method: 'POST' });
  },

  // Synchronization
  getCsState() { return this.request('/api/sync/cs'); },
  stepCs(processId) {
    return this.request('/api/sync/cs/step', {
      method: 'POST',
      body: JSON.stringify({ process_id: processId })
    });
  },
  resetCs() {
    return this.request('/api/sync/cs/reset', { method: 'POST' });
  },

  getPcState() { return this.request('/api/sync/pc'); },
  producePc(item = 'Item') {
    return this.request('/api/sync/pc/produce', {
      method: 'POST',
      body: JSON.stringify({ item })
    });
  },
  consumePc() {
    return this.request('/api/sync/pc/consume', { method: 'POST' });
  },
  resetPc() {
    return this.request('/api/sync/pc/reset', { method: 'POST' });
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
      body: JSON.stringify({ writer_id: writerId, value: parseInt(value, 10) })
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

  // Deadlock Detection & Banker's Algorithm
  analyzeDeadlock(payload) {
    return this.request('/api/deadlock/analyze', {
      method: 'POST',
      body: JSON.stringify(payload)
    });
  },
  getDeadlockPreset(name) {
    return this.request(`/api/deadlock/preset/${encodeURIComponent(name)}`);
  },
  recoverDeadlock(processId) {
    return this.request('/api/deadlock/recover', {
      method: 'POST',
      body: JSON.stringify({ terminate_process_id: processId })
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
  readPipe(bytes = 64) {
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
  readShm() {
    return this.request('/api/ipc/shm/read', { method: 'POST' });
  },
  writeShm(data) {
    return this.request('/api/ipc/shm/write', {
      method: 'POST',
      body: JSON.stringify({ data })
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
  exchangeSocket(message) {
    return this.request('/api/ipc/exchange', {
      method: 'POST',
      body: JSON.stringify({ message })
    });
  },
  getIpcHistory() { return this.request('/api/ipc/history'); },

  // Distributed Logical Clocks
  getClockNodes() { return this.request('/api/clocks/nodes'); },
  getClockTimeline() { return this.request('/api/clocks/timeline'); },
  triggerLocalEvent(nodeId, desc) {
    return this.request('/api/clocks/local-event', {
      method: 'POST',
      body: JSON.stringify({ node_id: nodeId, description: desc })
    });
  },
  sendClockMessage(fromNode, toNode, msg) {
    return this.request('/api/clocks/send-message', {
      method: 'POST',
      body: JSON.stringify({ from_node: fromNode, to_node: toNode, message: msg })
    });
  },
  resetClocks(count = 3) {
    return this.request('/api/clocks/reset', {
      method: 'POST',
      body: JSON.stringify({ node_count: count })
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

  // Crash Consistency & Recovery
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

window.API = window.api;
