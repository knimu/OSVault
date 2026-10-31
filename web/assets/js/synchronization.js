/**
 * OSVault - Concurrency & Synchronization Module
 */

window.syncModule = {
  currentTab: 'peterson',

  init() {
    this.bindEvents();
    this.refreshAll();
  },

  bindEvents() {
    // Sub-tab switcher
    document.querySelectorAll('.sync-tab-btn').forEach(btn => {
      btn.onclick = () => {
        document.querySelectorAll('.sync-tab-btn').forEach(b => b.classList.remove('active'));
        document.querySelectorAll('.sync-tab-content').forEach(c => {
          c.classList.remove('active');
          c.style.display = 'none';
        });

        btn.classList.add('active');
        const tab = btn.dataset.tab;
        this.currentTab = tab;
        const target = document.getElementById(`sync-${tab}`);
        if (target) {
          target.classList.add('active');
          target.style.display = 'block';
        }
      };
    });

    // Peterson controls
    const btnP0 = document.getElementById('btn-peterson-p0');
    if (btnP0) btnP0.onclick = () => this.stepPeterson(0);

    const btnP1 = document.getElementById('btn-peterson-p1');
    if (btnP1) btnP1.onclick = () => this.stepPeterson(1);

    const btnPetersonReset = document.getElementById('btn-peterson-reset');
    if (btnPetersonReset) btnPetersonReset.onclick = () => this.resetPeterson();

    // Bounded Buffer controls
    const btnProduce = document.getElementById('btn-bb-produce');
    if (btnProduce) btnProduce.onclick = () => this.produceItem();

    const btnConsume = document.getElementById('btn-bb-consume');
    if (btnConsume) btnConsume.onclick = () => this.consumeItem();

    const btnBbReset = document.getElementById('btn-bb-reset');
    if (btnBbReset) btnBbReset.onclick = () => this.resetBoundedBuffer();

    // Readers-Writers controls
    const btnStartRead = document.getElementById('btn-rw-start-read');
    if (btnStartRead) btnStartRead.onclick = () => this.startRead();

    const btnStopRead = document.getElementById('btn-rw-stop-read');
    if (btnStopRead) btnStopRead.onclick = () => this.stopRead();

    const btnStartWrite = document.getElementById('btn-rw-start-write');
    if (btnStartWrite) btnStartWrite.onclick = () => this.startWrite();

    const btnStopWrite = document.getElementById('btn-rw-stop-write');
    if (btnStopWrite) btnStopWrite.onclick = () => this.stopWrite();

    const btnRwReset = document.getElementById('btn-rw-reset');
    if (btnRwReset) btnRwReset.onclick = () => this.resetRw();

    // Dining Philosophers controls
    const btnDpReset = document.getElementById('btn-dp-reset');
    if (btnDpReset) btnDpReset.onclick = () => this.resetDp();
  },

  async refreshAll() {
    await Promise.all([
      this.refreshPeterson(),
      this.refreshBoundedBuffer(),
      this.refreshReadersWriters(),
      this.refreshDiningPhilosophers()
    ]);
  },

  // --- Peterson's Algorithm ---
  async refreshPeterson() {
    try {
      const res = await window.api.getCsState();
      const p0El = document.getElementById('sync-p0-flag');
      const p1El = document.getElementById('sync-p1-flag');
      const turnEl = document.getElementById('sync-turn');
      const csStatus = document.getElementById('sync-cs-status');

      if (p0El) p0El.textContent = res.flag_0 ? 'TRUE (Wants CS)' : 'false';
      if (p1El) p1El.textContent = res.flag_1 ? 'TRUE (Wants CS)' : 'false';
      if (turnEl) turnEl.textContent = res.turn !== undefined ? res.turn : 0;

      if (csStatus) {
        if (res.in_critical_section === 0) {
          csStatus.className = 'badge badge-primary ml-2';
          csStatus.textContent = 'Process P0 in Critical Section';
        } else if (res.in_critical_section === 1) {
          csStatus.className = 'badge badge-teal ml-2';
          csStatus.textContent = 'Process P1 in Critical Section';
        } else {
          csStatus.className = 'badge badge-success ml-2';
          csStatus.textContent = 'IDLE (No collision)';
        }
      }
    } catch (err) {
      console.error('Error refreshing Peterson:', err);
    }
  },

  async stepPeterson(procId) {
    try {
      const res = await window.api.stepCs(procId);
      window.app.showToast(`P${procId} stepped: ${res.state_desc || 'State updated'}`, 'info');
      this.refreshPeterson();
    } catch (err) {
      window.app.showToast('Peterson step failed: ' + err.message, 'error');
    }
  },

  async resetPeterson() {
    try {
      await window.api.resetCs();
      window.app.showToast('Peterson state reset', 'info');
      this.refreshPeterson();
    } catch (err) {
      window.app.showToast(err.message, 'error');
    }
  },

  // --- Bounded Buffer ---
  async refreshBoundedBuffer() {
    try {
      const res = await window.api.getPcState();
      const capEl = document.getElementById('bb-capacity');
      const countEl = document.getElementById('bb-count');
      const semEmpty = document.getElementById('bb-sem-empty');
      const semFull = document.getElementById('bb-sem-full');
      const slotsContainer = document.getElementById('bb-slots-container');

      const cap = res.capacity || 5;
      const count = res.count || 0;

      if (capEl) capEl.textContent = cap;
      if (countEl) countEl.textContent = count;
      if (semEmpty) semEmpty.textContent = cap - count;
      if (semFull) semFull.textContent = count;

      if (slotsContainer) {
        slotsContainer.innerHTML = '';
        for (let i = 0; i < cap; ++i) {
          const item = (res.buffer && res.buffer[i]) ? res.buffer[i] : null;
          const slot = document.createElement('div');
          slot.className = `p-3 rounded text-center font-mono ${item ? 'bg-primary text-white' : 'bg-dark border border-secondary text-muted'}`;
          slot.style.minWidth = '90px';
          slot.innerHTML = `<div>Slot ${i}</div><strong>${item ? this.escape(item) : 'EMPTY'}</strong>`;
          slotsContainer.appendChild(slot);
        }
      }
    } catch (err) {
      console.error('Error refreshing Bounded Buffer:', err);
    }
  },

  async produceItem() {
    const valInput = document.getElementById('bb-produce-val');
    const val = valInput && valInput.value ? valInput.value.trim() : `Item_${Date.now() % 1000}`;

    try {
      await window.api.producePc(val);
      window.app.showToast(`Produced item '${val}' into bounded buffer`, 'success');
      this.refreshBoundedBuffer();
    } catch (err) {
      window.app.showToast('Buffer full (Semaphore empty == 0): ' + err.message, 'warning');
    }
  },

  async consumeItem() {
    try {
      const res = await window.api.consumePc();
      window.app.showToast(`Consumed item '${res.consumed_item || 'Item'}' from buffer`, 'info');
      this.refreshBoundedBuffer();
    } catch (err) {
      window.app.showToast('Buffer empty (Semaphore full == 0): ' + err.message, 'warning');
    }
  },

  async resetBoundedBuffer() {
    try {
      await window.api.resetPc();
      window.app.showToast('Bounded buffer reset to empty', 'info');
      this.refreshBoundedBuffer();
    } catch (err) {
      window.app.showToast(err.message, 'error');
    }
  },

  // --- Readers-Writers ---
  async refreshReadersWriters() {
    try {
      const res = await window.api.getRwState();
      const readersEl = document.getElementById('rw-active-readers');
      const writerEl = document.getElementById('rw-writer-active');
      const valEl = document.getElementById('rw-resource-val');
      const msgEl = document.getElementById('rw-status-msg');

      if (readersEl) readersEl.textContent = res.active_readers || 0;
      if (writerEl) {
        writerEl.textContent = res.writer_active ? `Writing (${res.active_writer_id || 'Writer'})` : 'IDLE';
        writerEl.className = res.writer_active ? 'stat-value highlight-red' : 'stat-value text-muted';
      }
      if (valEl) valEl.textContent = res.shared_value !== undefined ? res.shared_value : 100;

      if (msgEl) {
        if (res.writer_active) {
          msgEl.className = 'alert alert-warning mb-3';
          msgEl.textContent = `Exclusive Lock held by ${res.active_writer_id}. Readers and other writers are locked out.`;
        } else if (res.active_readers > 0) {
          msgEl.className = 'alert alert-info mb-3';
          msgEl.textContent = `${res.active_readers} reader(s) actively reading resource concurrently. Writers must wait.`;
        } else {
          msgEl.className = 'alert alert-info mb-3';
          msgEl.textContent = 'System idle. Readers can read concurrently; Writers require exclusive lock.';
        }
      }
    } catch (err) {
      console.error('Error refreshing RW:', err);
    }
  },

  async startRead() {
    const readerInput = document.getElementById('rw-reader-id');
    const readerId = readerInput ? readerInput.value.trim() : 'Reader_1';
    try {
      await window.api.startReadRw(readerId);
      window.app.showToast(`${readerId} acquired read access`, 'success');
      this.refreshReadersWriters();
    } catch (err) {
      window.app.showToast(err.message, 'error');
    }
  },

  async stopRead() {
    const readerInput = document.getElementById('rw-reader-id');
    const readerId = readerInput ? readerInput.value.trim() : 'Reader_1';
    try {
      await window.api.stopReadRw(readerId);
      window.app.showToast(`${readerId} released read access`, 'info');
      this.refreshReadersWriters();
    } catch (err) {
      window.app.showToast(err.message, 'error');
    }
  },

  async startWrite() {
    const writerInput = document.getElementById('rw-writer-id');
    const writerId = writerInput ? writerInput.value.trim() : 'Writer_1';
    try {
      await window.api.startWriteRw(writerId);
      window.app.showToast(`${writerId} acquired exclusive write lock`, 'warning');
      this.refreshReadersWriters();
    } catch (err) {
      window.app.showToast('Write lock blocked: ' + err.message, 'error');
    }
  },

  async stopWrite() {
    const writerInput = document.getElementById('rw-writer-id');
    const valInput = document.getElementById('rw-writer-val');
    const writerId = writerInput ? writerInput.value.trim() : 'Writer_1';
    const val = valInput ? parseInt(valInput.value, 10) || 200 : 200;

    try {
      await window.api.stopWriteRw(writerId, val);
      window.app.showToast(`${writerId} wrote value ${val} and released lock`, 'success');
      this.refreshReadersWriters();
    } catch (err) {
      window.app.showToast('Write release failed: ' + err.message, 'error');
    }
  },

  async resetRw() {
    try {
      await window.api.resetRw();
      window.app.showToast('Readers-Writers state reset', 'info');
      this.refreshReadersWriters();
    } catch (err) {
      window.app.showToast(err.message, 'error');
    }
  },

  // --- Dining Philosophers ---
  async refreshDiningPhilosophers() {
    try {
      const res = await window.api.getDpState();
      const container = document.getElementById('dp-table-container');
      const msgEl = document.getElementById('dp-status-msg');

      if (msgEl) {
        msgEl.textContent = res.deadlock_free ? 'Dijkstra resource hierarchy active: No circular wait or deadlock possible.' : 'Philosophers synchronized.';
      }

      if (container) {
        container.innerHTML = '';
        const philosophers = res.philosophers || [
          { id: 0, state: 'THINKING' },
          { id: 1, state: 'THINKING' },
          { id: 2, state: 'THINKING' },
          { id: 3, state: 'THINKING' },
          { id: 4, state: 'THINKING' }
        ];

        philosophers.forEach(p => {
          let badge = 'badge-secondary';
          let statusColor = '#64748b';
          if (p.state === 'HUNGRY') {
            badge = 'badge-warning';
            statusColor = '#f59e0b';
          } else if (p.state === 'EATING') {
            badge = 'badge-success';
            statusColor = '#10b981';
          }

          const card = document.createElement('div');
          card.className = 'panel mb-0 p-3 text-center';
          card.style.background = 'var(--bg-secondary)';
          card.style.borderTop = `4px solid ${statusColor}`;
          card.innerHTML = `
            <h4>Philosopher ${p.id}</h4>
            <div class="my-2"><span class="badge ${badge}">${p.state}</span></div>
            <button class="btn btn-primary btn-sm" style="width: 100%;" onclick="window.syncModule.stepDp(${p.id})">Step Action</button>
          `;
          container.appendChild(card);
        });
      }
    } catch (err) {
      console.error('Error refreshing DP:', err);
    }
  },

  async stepDp(philosopherId) {
    try {
      const res = await window.api.stepDp(philosopherId);
      window.app.showToast(`Philosopher ${philosopherId} transitioned: ${res.state || 'Next state'}`, 'info');
      this.refreshDiningPhilosophers();
    } catch (err) {
      window.app.showToast('Step failed (Chopstick in use): ' + err.message, 'warning');
    }
  },

  async resetDp() {
    try {
      await window.api.resetDp();
      window.app.showToast('All philosophers reset to Thinking', 'info');
      this.refreshDiningPhilosophers();
    } catch (err) {
      window.app.showToast(err.message, 'error');
    }
  },

  escape(s) {
    if (!s) return '';
    return String(s).replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;');
  }
};

window.Synchronization = window.syncModule;
