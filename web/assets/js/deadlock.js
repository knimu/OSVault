/**
 * OSVault - Banker's Algorithm & Deadlock Detection Module
 */

window.deadlockModule = {
  lastDeadlockedProcesses: [],

  init() {
    this.bindEvents();
  },

  bindEvents() {
    const btnSafe = document.getElementById('btn-load-preset-safe');
    if (btnSafe) {
      btnSafe.onclick = () => this.loadPreset('safe');
    }

    const btnDeadlock = document.getElementById('btn-load-preset-deadlock');
    if (btnDeadlock) {
      btnDeadlock.onclick = () => this.loadPreset('deadlock');
    }

    const btnRun = document.getElementById('btn-run-banker');
    if (btnRun) {
      btnRun.onclick = () => this.analyze();
    }

    const btnRecover = document.getElementById('btn-recover-deadlock');
    if (btnRecover) {
      btnRecover.onclick = () => this.recover();
    }
  },

  async loadPreset(type) {
    try {
      const res = await window.api.getDeadlockPreset(type);
      const state = res.preset || res.state || res;

      const availInput = document.getElementById('banker-avail-vector');
      const allocInput = document.getElementById('banker-alloc-matrix');
      const reqInput = document.getElementById('banker-request-matrix');

      if (availInput && state.available) {
        availInput.value = state.available.join(', ');
      }

      if (allocInput && state.allocation) {
        allocInput.value = state.allocation.map(row => row.join(', ')).join('\n');
      }

      if (reqInput && state.request) {
        reqInput.value = state.request.map(row => row.join(', ')).join('\n');
      }

      window.app.showToast(`Loaded ${type.toUpperCase()} preset into matrices.`, 'info');
      this.analyze();
    } catch (err) {
      window.app.showToast('Failed to load preset: ' + err.message, 'error');
    }
  },

  async analyze() {
    const availInput = document.getElementById('banker-avail-vector');
    const allocInput = document.getElementById('banker-alloc-matrix');
    const reqInput = document.getElementById('banker-request-matrix');
    const resultBox = document.getElementById('deadlock-result-box');

    const avail = (availInput ? availInput.value : '3, 3, 2')
      .split(/[,\s]+/)
      .map(x => parseInt(x, 10))
      .filter(x => !isNaN(x));

    const parseMatrix = (text) => {
      return (text || '').trim().split('\n').map(line => {
        return line.split(/[,\s]+/).map(x => parseInt(x, 10)).filter(x => !isNaN(x));
      }).filter(row => row.length > 0);
    };

    const alloc = parseMatrix(allocInput ? allocInput.value : '');
    const request = parseMatrix(reqInput ? reqInput.value : '');

    const numProcs = alloc.length;
    const processes = [];
    for (let i = 0; i < numProcs; ++i) {
      processes.push(`P${i}`);
    }

    const resources = ['A', 'B', 'C'].slice(0, avail.length);

    try {
      const res = await window.api.analyzeDeadlock({
        processes,
        resources,
        available: avail,
        allocation: alloc,
        request: request
      });

      const r = res.result || res;
      this.lastDeadlockedProcesses = r.deadlocked_processes || [];

      if (resultBox) {
        if (r.is_safe) {
          resultBox.className = 'p-3 rounded border border-success bg-dark';
          resultBox.innerHTML = `
            <div class="d-flex justify-between align-center mb-2">
              <h4 style="color: #10b981; margin: 0;">✓ System is in a SAFE State (Deadlock-Free)</h4>
              <span class="badge badge-success">SAFE</span>
            </div>
            <p class="mb-2">Dijkstra's Banker's Algorithm discovered a valid execution order where all processes can acquire required resources and finish:</p>
            <div class="font-mono p-2 rounded bg-black" style="font-size: 1.1rem; color: #60a5fa;">
              Safe Sequence: &lang; ${(r.safe_sequence || []).join(' &rarr; ')} &rang;
            </div>
          `;
        } else {
          resultBox.className = 'p-3 rounded border border-danger bg-dark';
          resultBox.innerHTML = `
            <div class="d-flex justify-between align-center mb-2">
              <h4 style="color: #ef4444; margin: 0;">✕ DEADLOCK DETECTED! (Unsafe State)</h4>
              <span class="badge badge-danger">DEADLOCKED</span>
            </div>
            <p class="mb-2">Circular wait condition detected. No processes in the following set can complete with currently available resources:</p>
            <div class="font-mono p-2 rounded bg-black highlight-red" style="font-size: 1.1rem;">
              Deadlocked Processes: { ${(this.lastDeadlockedProcesses || []).join(', ')} }
            </div>
            <p class="small text-muted mt-2 mb-0">Click "Recover (Terminate Deadlocked)" to terminate a culprit process and release its held resources.</p>
          `;
        }
      }

      window.app.showToast(r.is_safe ? 'Safe sequence verified!' : 'Deadlock condition detected!', r.is_safe ? 'success' : 'warning');
    } catch (err) {
      if (resultBox) {
        resultBox.innerHTML = `<div class="text-danger p-2">Analysis Error: ${this.escape(err.message)}</div>`;
      }
      window.app.showToast('Banker analysis failed: ' + err.message, 'error');
    }
  },

  async recover() {
    if (this.lastDeadlockedProcesses.length === 0) {
      window.app.showToast('No active deadlocked processes to recover from', 'info');
      return;
    }

    const victim = this.lastDeadlockedProcesses[0];
    try {
      await window.api.recoverDeadlock(victim);
      window.app.showToast(`Simulated recovery: Terminated ${victim} and reclaimed its resources.`, 'success');
      this.loadPreset('safe');
    } catch (err) {
      window.app.showToast('Recovery failed: ' + err.message, 'error');
    }
  },

  escape(s) {
    if (!s) return '';
    return String(s).replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;');
  }
};

window.Deadlock = window.deadlockModule;
