import { API } from './api.js';

export const Deadlock = {
  currentState: null,

  async init() {
    this.bindEvents();
    await this.loadPreset('safe');
  },

  bindEvents() {
    document.getElementById('btn-load-safe-preset')?.addEventListener('click', () => this.loadPreset('safe'));
    document.getElementById('btn-load-deadlock-preset')?.addEventListener('click', () => this.loadPreset('deadlock'));
    document.getElementById('btn-analyze-deadlock')?.addEventListener('click', () => this.analyzeCurrent());
    document.getElementById('btn-recover-deadlock')?.addEventListener('click', () => this.recover());
  },

  async loadPreset(type) {
    try {
      const data = await API.getDeadlockPreset(type);
      this.currentState = data.result;
      this.renderState(data.result);
      window.App.toast(`Loaded ${type} state preset.`, 'success');
    } catch (err) {
      window.App.toast(`Failed to load preset: ${err.message}`, 'error');
    }
  },

  renderState(res) {
    this.currentState = res;

    // Status banner
    const statusEl = document.getElementById('deadlock-status-indicator');
    if (statusEl) {
      if (res.is_deadlock) {
        statusEl.className = 'status-badge error';
        statusEl.innerHTML = `<span class="status-dot"></span> DEADLOCK DETECTED (${res.deadlocked_processes.join(', ')})`;
      } else {
        statusEl.className = 'status-badge';
        statusEl.innerHTML = `<span class="status-dot"></span> SAFE STATE (Sequence: ${res.safe_sequence.join(' &rarr; ')})`;
      }
    }

    // Available vector
    document.getElementById('deadlock-avail-vec').textContent = `[ ${res.available.join(', ')} ]`;

    // Matrices
    const allocTbody = document.getElementById('deadlock-alloc-tbody');
    if (allocTbody) {
      allocTbody.innerHTML = res.processes.map((p, idx) => `
        <tr>
          <td><strong>${p}</strong></td>
          ${res.allocation[idx].map(v => `<td>${v}</td>`).join('')}
        </tr>
      `).join('');
    }

    const reqTbody = document.getElementById('deadlock-req-tbody');
    if (reqTbody) {
      reqTbody.innerHTML = res.processes.map((p, idx) => `
        <tr>
          <td><strong>${p}</strong></td>
          ${res.request[idx].map(v => `<td>${v}</td>`).join('')}
        </tr>
      `).join('');
    }

    // Execution logs
    const logEl = document.getElementById('deadlock-steps-log');
    if (logEl) {
      logEl.innerHTML = res.log_steps.map(s => `<div style="padding: 2px 0;">&bull; ${s}</div>`).join('');
    }

    // Populate recovery dropdown with deadlocked processes
    const selectProc = document.getElementById('deadlock-kill-proc-select');
    if (selectProc) {
      selectProc.innerHTML = (res.deadlocked_processes || []).map(p => `
        <option value="${p}">${p}</option>
      `).join('');
    }
  },

  async analyzeCurrent() {
    if (!this.currentState) return;
    try {
      const data = await API.analyzeDeadlock(
        this.currentState.processes,
        this.currentState.resource_types,
        this.currentState.available,
        this.currentState.allocation,
        this.currentState.request
      );
      this.renderState(data.result);
      window.App.toast('Resource-allocation analysis finished.', 'success');
    } catch (err) {
      window.App.toast(`Analysis error: ${err.message}`, 'error');
    }
  },

  async recover() {
    const selectProc = document.getElementById('deadlock-kill-proc-select');
    const procToKill = selectProc ? selectProc.value : null;

    if (!procToKill) {
      window.App.toast('No deadlocked process selected for recovery.', 'warning');
      return;
    }

    try {
      const data = await API.recoverDeadlock(this.currentState, procToKill);
      this.renderState(data.result);
      window.App.toast(`Simulated recovery executed for process '${procToKill}'.`, 'success');
    } catch (err) {
      window.App.toast(`Recovery failed: ${err.message}`, 'error');
    }
  }
};
