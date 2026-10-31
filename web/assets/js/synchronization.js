import { API } from './api.js';

export const Synchronization = {
  async init() {
    this.bindEvents();
    await this.refreshAll();
  },

  bindEvents() {
    // Peterson CS
    document.getElementById('btn-cs-step-0')?.addEventListener('click', () => this.stepCs(0));
    document.getElementById('btn-cs-step-1')?.addEventListener('click', () => this.stepCs(1));
    document.getElementById('btn-cs-reset')?.addEventListener('click', () => this.resetCs());

    // Producer Consumer
    document.getElementById('btn-pc-produce')?.addEventListener('click', () => this.producePc());
    document.getElementById('btn-pc-consume')?.addEventListener('click', () => this.consumePc());
    document.getElementById('btn-pc-reset')?.addEventListener('click', () => this.resetPc());

    // Readers Writers
    document.getElementById('btn-rw-read-start')?.addEventListener('click', () => this.startReadRw());
    document.getElementById('btn-rw-read-stop')?.addEventListener('click', () => this.stopReadRw());
    document.getElementById('btn-rw-write-start')?.addEventListener('click', () => this.startWriteRw());
    document.getElementById('btn-rw-write-stop')?.addEventListener('click', () => this.stopWriteRw());
    document.getElementById('btn-rw-reset')?.addEventListener('click', () => this.resetRw());

    // Dining Philosophers
    document.getElementById('btn-dp-step-0')?.addEventListener('click', () => this.stepDp(0));
    document.getElementById('btn-dp-step-1')?.addEventListener('click', () => this.stepDp(1));
    document.getElementById('btn-dp-step-2')?.addEventListener('click', () => this.stepDp(2));
    document.getElementById('btn-dp-step-3')?.addEventListener('click', () => this.stepDp(3));
    document.getElementById('btn-dp-step-4')?.addEventListener('click', () => this.stepDp(4));
    document.getElementById('btn-dp-reset')?.addEventListener('click', () => this.resetDp());
  },

  async refreshAll() {
    await Promise.all([
      this.refreshCs(),
      this.refreshPc(),
      this.refreshRw(),
      this.refreshDp()
    ]);
  },

  // Critical Section
  async refreshCs() {
    try {
      const data = await API.getCsState();
      this.renderCs(data.state);
    } catch (err) { console.error(err); }
  },
  renderCs(s) {
    document.getElementById('cs-t0-state').textContent = s.thread_states[0];
    document.getElementById('cs-t1-state').textContent = s.thread_states[1];
    document.getElementById('cs-turn-val').textContent = `Thread ${s.turn}`;
    document.getElementById('cs-flags').textContent = `[ ${s.flag.map(f => f ? 'true' : 'false').join(', ')} ]`;
    document.getElementById('cs-active').textContent = s.current_in_cs >= 0 ? `Thread ${s.current_in_cs}` : 'None';

    const logEl = document.getElementById('cs-log-history');
    if (logEl) logEl.innerHTML = s.log_history.slice(-5).map(l => `<div>• ${l}</div>`).join('');
  },
  async stepCs(id) {
    try {
      const data = await API.stepCs(id);
      this.renderCs(data.state);
    } catch (err) { window.App.toast(err.message, 'error'); }
  },
  async resetCs() {
    const data = await API.resetCs();
    this.renderCs(data.state);
  },

  // Producer Consumer
  async refreshPc() {
    try {
      const data = await API.getPcState();
      this.renderPc(data.state);
    } catch (err) { console.error(err); }
  },
  renderPc(s) {
    document.getElementById('pc-slots-info').textContent = `Full: ${s.full_slots} / ${s.buffer_capacity} (Empty: ${s.empty_slots})`;
    document.getElementById('pc-prod-state').textContent = s.producer_state;
    document.getElementById('pc-cons-state').textContent = s.consumer_state;

    const bufContainer = document.getElementById('pc-buffer-slots');
    if (bufContainer) {
      let html = '';
      for (let i = 0; i < s.buffer_capacity; ++i) {
        const item = s.buffer[i];
        html += `<div style="padding:6px 12px; background:${item !== undefined ? 'var(--accent-blue)' : '#1e293b'}; border:1px solid var(--border-color); border-radius:4px; font-weight:600;">${item !== undefined ? item : 'empty'}</div>`;
      }
      bufContainer.innerHTML = html;
    }

    const logEl = document.getElementById('pc-log-history');
    if (logEl) logEl.innerHTML = s.log_history.slice(-5).map(l => `<div>• ${l}</div>`).join('');
  },
  async producePc() {
    const data = await API.producePc(Math.floor(Math.random() * 900) + 100);
    this.renderPc(data.state);
  },
  async consumePc() {
    const data = await API.consumePc();
    this.renderPc(data.state);
  },
  async resetPc() {
    const data = await API.resetPc(5);
    this.renderPc(data.state);
  },

  // Readers Writers
  async refreshRw() {
    try {
      const data = await API.getRwState();
      this.renderRw(data.state);
    } catch (err) { console.error(err); }
  },
  renderRw(s) {
    document.getElementById('rw-reader-count').textContent = s.reader_count;
    document.getElementById('rw-writer-active').textContent = s.writer_active ? s.active_writer : 'None';
    document.getElementById('rw-resource-val').textContent = s.shared_resource_val;
    document.getElementById('rw-waiting-readers').textContent = s.waiting_readers;
    document.getElementById('rw-waiting-writers').textContent = s.waiting_writers;

    const logEl = document.getElementById('rw-log-history');
    if (logEl) logEl.innerHTML = s.log_history.slice(-5).map(l => `<div>• ${l}</div>`).join('');
  },
  async startReadRw() {
    const rid = 'R' + (Math.floor(Math.random() * 5) + 1);
    const data = await API.startReadRw(rid);
    this.renderRw(data.state);
  },
  async stopReadRw() {
    const cur = (await API.getRwState()).state;
    if (cur.active_readers.length > 0) {
      const data = await API.stopReadRw(cur.active_readers[0]);
      this.renderRw(data.state);
    }
  },
  async startWriteRw() {
    const wid = 'W' + (Math.floor(Math.random() * 3) + 1);
    const data = await API.startWriteRw(wid);
    this.renderRw(data.state);
  },
  async stopWriteRw() {
    const cur = (await API.getRwState()).state;
    if (cur.writer_active) {
      const newVal = Math.floor(Math.random() * 500) + 100;
      const data = await API.stopWriteRw(cur.active_writer, newVal);
      this.renderRw(data.state);
    }
  },
  async resetRw() {
    const data = await API.resetRw();
    this.renderRw(data.state);
  },

  // Dining Philosophers
  async refreshDp() {
    try {
      const data = await API.getDpState();
      this.renderDp(data.state);
    } catch (err) { console.error(err); }
  },
  renderDp(s) {
    for (let i = 0; i < 5; ++i) {
      const stEl = document.getElementById(`dp-p${i}-state`);
      const mealsEl = document.getElementById(`dp-p${i}-meals`);
      if (stEl) {
        stEl.textContent = s.states[i];
        stEl.className = s.states[i] === 'EATING' ? 'status-badge' : (s.states[i] === 'HUNGRY' ? 'status-badge error' : 'status-badge');
      }
      if (mealsEl) mealsEl.textContent = `${s.meals_eaten[i]} meals`;
    }

    const forksEl = document.getElementById('dp-forks-state');
    if (forksEl) {
      forksEl.textContent = s.forks.map((f, idx) => `Fork ${idx}: ${f ? 'Available' : 'Held'}`).join(' | ');
    }

    const logEl = document.getElementById('dp-log-history');
    if (logEl) logEl.innerHTML = s.log_history.slice(-5).map(l => `<div>• ${l}</div>`).join('');
  },
  async stepDp(id) {
    const data = await API.stepDp(id);
    this.renderDp(data.state);
  },
  async resetDp() {
    const data = await API.resetDp();
    this.renderDp(data.state);
  }
};
