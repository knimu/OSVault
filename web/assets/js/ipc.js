/**
 * OSVault - Inter-Process Communication (IPC) Simulator
 * Demonstrates Pipes, Message Queues, Shared Memory with Mutex, and Client-Server IPC.
 */

const ipcModule = {
  currentTab: 'pipe',

  init() {
    this.bindEvents();
    this.refreshCurrentTab();
  },

  bindEvents() {
    // Sub-tab switching
    document.querySelectorAll('.ipc-subtab-btn').forEach(btn => {
      btn.addEventListener('click', (e) => {
        document.querySelectorAll('.ipc-subtab-btn').forEach(b => b.classList.remove('active'));
        document.querySelectorAll('.ipc-subtab-content').forEach(c => c.classList.remove('active'));
        
        btn.classList.add('active');
        const tabId = btn.dataset.tab;
        this.currentTab = tabId;
        const target = document.getElementById(`ipc-${tabId}`);
        if (target) target.classList.add('active');
        this.refreshCurrentTab();
      });
    });

    // Pipe controls
    const btnPipeWrite = document.getElementById('btn-pipe-write');
    if (btnPipeWrite) {
      btnPipeWrite.addEventListener('click', () => this.writePipe());
    }
    const btnPipeRead = document.getElementById('btn-pipe-read');
    if (btnPipeRead) {
      btnPipeRead.addEventListener('click', () => this.readPipe());
    }
    const btnPipeReset = document.getElementById('btn-pipe-reset');
    if (btnPipeReset) {
      btnPipeReset.addEventListener('click', () => this.resetPipe());
    }

    // Message Queue controls
    const btnMqSend = document.getElementById('btn-mq-send');
    if (btnMqSend) {
      btnMqSend.addEventListener('click', () => this.sendMq());
    }
    const btnMqRecv = document.getElementById('btn-mq-receive');
    if (btnMqRecv) {
      btnMqRecv.addEventListener('click', () => this.receiveMq());
    }
    const btnMqClear = document.getElementById('btn-mq-clear');
    if (btnMqClear) {
      btnMqClear.addEventListener('click', () => this.clearMq());
    }

    // Shared Memory controls
    const btnShmLock = document.getElementById('btn-shm-lock');
    if (btnShmLock) {
      btnShmLock.addEventListener('click', () => this.lockShm());
    }
    const btnShmUnlock = document.getElementById('btn-shm-unlock');
    if (btnShmUnlock) {
      btnShmUnlock.addEventListener('click', () => this.unlockShm());
    }
    const btnShmWrite = document.getElementById('btn-shm-write');
    if (btnShmWrite) {
      btnShmWrite.addEventListener('click', () => this.writeShm());
    }
    const btnShmRead = document.getElementById('btn-shm-read');
    if (btnShmRead) {
      btnShmRead.addEventListener('click', () => this.readShm());
    }

    // Client-Server Socket
    const btnSocketSend = document.getElementById('btn-socket-send');
    if (btnSocketSend) {
      btnSocketSend.addEventListener('click', () => this.exchangeSocket());
    }
  },

  refreshCurrentTab() {
    switch (this.currentTab) {
      case 'pipe': this.refreshPipe(); break;
      case 'mq': this.refreshMq(); break;
      case 'shm': this.refreshShm(); break;
      case 'socket': this.refreshSocketHistory(); break;
    }
  },

  // --- PIPE ---
  async refreshPipe() {
    try {
      const res = await api.getPipe();
      if (!res.success) return;
      const data = res.data;

      const bufferEl = document.getElementById('pipe-buffer-display');
      if (bufferEl) {
        bufferEl.textContent = data.buffer.length > 0 ? data.buffer : '(Pipe empty)';
      }

      const sizeEl = document.getElementById('pipe-size-badge');
      if (sizeEl) sizeEl.textContent = `${data.bytes_used} / ${data.capacity} bytes (${data.bytes_available} free)`;

      const fillBar = document.getElementById('pipe-fill-bar');
      if (fillBar) {
        const pct = Math.min(100, Math.round((data.bytes_used / data.capacity) * 100));
        fillBar.style.width = `${pct}%`;
      }
    } catch (e) {
      console.error('Error refreshing pipe:', e);
    }
  },

  async writePipe() {
    const input = document.getElementById('pipe-write-input');
    if (!input || !input.value) return;
    try {
      const res = await api.writePipe(input.value);
      if (res.success) {
        app.showToast(`Wrote ${res.data.written_bytes} bytes to pipe`, 'success');
        input.value = '';
        this.refreshPipe();
      } else {
        app.showToast(res.error || 'Write pipe failed', 'error');
      }
    } catch (e) {
      app.showToast(e.message, 'error');
    }
  },

  async readPipe() {
    const bytesInput = document.getElementById('pipe-read-bytes');
    const bytes = bytesInput ? parseInt(bytesInput.value, 10) || 64 : 64;
    try {
      const res = await api.readPipe(bytes);
      if (res.success) {
        const out = document.getElementById('pipe-read-output');
        if (out) out.textContent = res.data.read_data || '(EOF / empty)';
        app.showToast(`Read ${res.data.bytes_read} bytes from pipe`, 'info');
        this.refreshPipe();
      } else {
        app.showToast(res.error || 'Read pipe failed', 'error');
      }
    } catch (e) {
      app.showToast(e.message, 'error');
    }
  },

  async resetPipe() {
    try {
      const res = await api.resetPipe();
      if (res.success) {
        app.showToast('Pipe cleared', 'info');
        const out = document.getElementById('pipe-read-output');
        if (out) out.textContent = '';
        this.refreshPipe();
      }
    } catch (e) {
      app.showToast(e.message, 'error');
    }
  },

  // --- MESSAGE QUEUE ---
  async refreshMq() {
    try {
      const res = await api.getMq();
      if (!res.success) return;
      const data = res.data;

      const countBadge = document.getElementById('mq-count-badge');
      if (countBadge) countBadge.textContent = `${data.count} / ${data.capacity} messages`;

      const listEl = document.getElementById('mq-messages-list');
      if (!listEl) return;

      if (!data.messages || data.messages.length === 0) {
        listEl.innerHTML = '<div class="text-muted p-3 text-center">Queue is currently empty.</div>';
        return;
      }

      listEl.innerHTML = data.messages.map((m, idx) => `
        <div class="mq-card">
          <div class="mq-header">
            <span class="badge badge-primary">#${idx + 1}</span>
            <span class="badge badge-info">${this.escape(m.sender)} &rarr; ${this.escape(m.receiver)}</span>
            <span class="text-muted small">${new Date(m.timestamp * 1000).toLocaleTimeString()}</span>
          </div>
          <div class="mq-body">${this.escape(m.content)}</div>
        </div>
      `).join('');
    } catch (e) {
      console.error('Error refreshing MQ:', e);
    }
  },

  async sendMq() {
    const sender = document.getElementById('mq-sender').value;
    const receiver = document.getElementById('mq-receiver').value;
    const messageInput = document.getElementById('mq-message');
    if (!messageInput || !messageInput.value) return;

    try {
      const res = await api.sendMq(sender, receiver, messageInput.value);
      if (res.success) {
        app.showToast('Message sent to queue', 'success');
        messageInput.value = '';
        this.refreshMq();
      } else {
        app.showToast(res.error || 'MQ send failed', 'error');
      }
    } catch (e) {
      app.showToast(e.message, 'error');
    }
  },

  async receiveMq() {
    const receiver = document.getElementById('mq-receive-proc').value;
    try {
      const res = await api.receiveMq(receiver);
      if (res.success) {
        const out = document.getElementById('mq-receive-output');
        if (out) {
          out.textContent = `[From: ${res.data.sender}] ${res.data.content}`;
        }
        app.showToast('Message received by ' + receiver, 'info');
        this.refreshMq();
      } else {
        app.showToast(res.error || 'No message available', 'warning');
      }
    } catch (e) {
      app.showToast(e.message, 'error');
    }
  },

  async clearMq() {
    try {
      const res = await api.clearMq();
      if (res.success) {
        app.showToast('Message queue cleared', 'info');
        const out = document.getElementById('mq-receive-output');
        if (out) out.textContent = '';
        this.refreshMq();
      }
    } catch (e) {
      app.showToast(e.message, 'error');
    }
  },

  // --- SHARED MEMORY & MUTEX ---
  async refreshShm() {
    try {
      const res = await api.getShm();
      if (!res.success) return;
      const data = res.data;

      const lockStatus = document.getElementById('shm-lock-status');
      if (lockStatus) {
        if (data.is_locked) {
          lockStatus.className = 'badge badge-warning';
          lockStatus.textContent = `LOCKED by ${data.locked_by}`;
        } else {
          lockStatus.className = 'badge badge-success';
          lockStatus.textContent = 'UNLOCKED';
        }
      }

      const memContent = document.getElementById('shm-content');
      if (memContent) memContent.textContent = data.data || '(Empty Shared Memory)';

      const verEl = document.getElementById('shm-version');
      if (verEl) verEl.textContent = `Version / Seq: ${data.version || 0}`;
    } catch (e) {
      console.error('Error refreshing SHM:', e);
    }
  },

  async lockShm() {
    const proc = document.getElementById('shm-process').value;
    try {
      const res = await api.lockShm(proc);
      if (res.success) {
        app.showToast(`Mutex locked by ${proc}`, 'success');
        this.refreshShm();
      } else {
        app.showToast(res.error || 'Lock failed (Mutex contention)', 'error');
      }
    } catch (e) {
      app.showToast(e.message, 'error');
    }
  },

  async unlockShm() {
    const proc = document.getElementById('shm-process').value;
    try {
      const res = await api.unlockShm(proc);
      if (res.success) {
        app.showToast(`Mutex released by ${proc}`, 'info');
        this.refreshShm();
      } else {
        app.showToast(res.error || 'Unlock failed (Not owner)', 'error');
      }
    } catch (e) {
      app.showToast(e.message, 'error');
    }
  },

  async writeShm() {
    const proc = document.getElementById('shm-process').value;
    const dataInput = document.getElementById('shm-write-data');
    if (!dataInput) return;

    try {
      const res = await api.writeShm(proc, dataInput.value);
      if (res.success) {
        app.showToast(`Written to shared memory by ${proc}`, 'success');
        this.refreshShm();
      } else {
        app.showToast(res.error || 'Write rejected: Race condition / Mutex required', 'error');
      }
    } catch (e) {
      app.showToast(e.message, 'error');
    }
  },

  async readShm() {
    const proc = document.getElementById('shm-process').value;
    try {
      const res = await api.readShm(proc);
      if (res.success) {
        const out = document.getElementById('shm-read-output');
        if (out) out.textContent = `[Reader: ${proc}] "${res.data.data}"`;
        app.showToast('Read shared memory', 'info');
      } else {
        app.showToast(res.error || 'Read failed', 'error');
      }
    } catch (e) {
      app.showToast(e.message, 'error');
    }
  },

  // --- SOCKET EXCHANGE ---
  async exchangeSocket() {
    const client = document.getElementById('socket-client-name').value;
    const reqStr = document.getElementById('socket-payload').value;
    if (!reqStr) return;

    try {
      const res = await api.exchangeSocket(client, reqStr);
      if (res.success) {
        app.showToast('IPC socket request responded', 'success');
        this.refreshSocketHistory();
      } else {
        app.showToast(res.error || 'Socket exchange failed', 'error');
      }
    } catch (e) {
      app.showToast(e.message, 'error');
    }
  },

  async refreshSocketHistory() {
    try {
      const res = await api.getIpcHistory();
      if (!res.success) return;
      const history = res.data.history || [];

      const listEl = document.getElementById('socket-history-list');
      if (!listEl) return;

      if (history.length === 0) {
        listEl.innerHTML = '<div class="text-muted p-3 text-center">No socket IPC transactions recorded yet.</div>';
        return;
      }

      listEl.innerHTML = history.slice(-10).reverse().map(h => `
        <div class="ipc-history-item">
          <div class="history-meta">
            <span class="badge badge-primary">${this.escape(h.channel || 'socket')}</span>
            <span class="text-muted small">${new Date(h.timestamp * 1000).toLocaleTimeString()}</span>
          </div>
          <div class="history-details">
            <strong>Req:</strong> <code>${this.escape(h.request || '')}</code> &rarr; 
            <strong>Resp:</strong> <code>${this.escape(h.response || '')}</code>
          </div>
        </div>
      `).join('');
    } catch (e) {
      console.error('Error refreshing socket history:', e);
    }
  },

  escape(s) {
    if (!s) return '';
    return String(s).replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;');
  }
};
