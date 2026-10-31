/**
 * OSVault - Crash Consistency & Integrity Recovery
 * Verifies block allocations, repairs orphan blocks, simulates interrupted writes,
 * and tests WAL crash-recovery and restart persistence.
 */

const recoveryModule = {
  init() {
    this.bindEvents();
    this.checkIntegrity();
    this.refreshHistory();
  },

  bindEvents() {
    const btnCheck = document.getElementById('btn-run-integrity-check');
    if (btnCheck) {
      btnCheck.addEventListener('click', () => this.checkIntegrity());
    }

    const btnFix = document.getElementById('btn-fix-orphans');
    if (btnFix) {
      btnFix.addEventListener('click', () => this.fixOrphans());
    }

    const btnCrashSim = document.getElementById('btn-sim-interrupted-write');
    if (btnCrashSim) {
      btnCrashSim.addEventListener('click', () => this.simulateCrash());
    }

    const btnRestartSim = document.getElementById('btn-sim-restart');
    if (btnRestartSim) {
      btnRestartSim.addEventListener('click', () => this.simulateRestart());
    }
  },

  async checkIntegrity() {
    const statusBox = document.getElementById('integrity-status-card');
    if (statusBox) statusBox.innerHTML = '<div class="text-center p-3 text-muted">Running integrity scans...</div>';

    try {
      const res = await api.checkIntegrity();
      if (!res.success) {
        if (statusBox) statusBox.innerHTML = `<div class="p-3 text-danger">Integrity check failed: ${res.error}</div>`;
        return;
      }

      const d = res.data;
      const isConsistent = d.is_consistent;

      if (statusBox) {
        statusBox.className = `card ${isConsistent ? 'border-success' : 'border-danger'}`;
        statusBox.innerHTML = `
          <div class="card-body">
            <div class="d-flex justify-between align-center mb-3">
              <h3 style="margin: 0; color: ${isConsistent ? '#10b981' : '#ef4444'};">
                ${isConsistent ? 'System Consistent (PASS)' : 'Inconsistencies Detected (FAIL)'}
              </h3>
              <span class="badge ${isConsistent ? 'badge-success' : 'badge-danger'}">
                ${isConsistent ? 'Healthy' : 'Repairs Required'}
              </span>
            </div>
            <div class="row g-3">
              <div class="col-md-3">
                <div class="stat-card">
                  <span class="stat-label">Total Files:</span>
                  <span class="stat-value">${d.total_files}</span>
                </div>
              </div>
              <div class="col-md-3">
                <div class="stat-card">
                  <span class="stat-label">Allocated Blocks:</span>
                  <span class="stat-value">${d.allocated_blocks}</span>
                </div>
              </div>
              <div class="col-md-3">
                <div class="stat-card">
                  <span class="stat-label">Orphan Blocks:</span>
                  <span class="stat-value highlight-red">${d.orphan_blocks || 0}</span>
                </div>
              </div>
              <div class="col-md-3">
                <div class="stat-card">
                  <span class="stat-label">Checksum Errors:</span>
                  <span class="stat-value">${d.corrupted_blocks || 0}</span>
                </div>
              </div>
            </div>
            ${d.details && d.details.length > 0 ? `
              <div class="mt-3 p-3 bg-dark rounded">
                <strong>Findings:</strong>
                <ul class="mb-0 mt-2 small text-muted">
                  ${d.details.map(det => `<li>${this.escape(det)}</li>`).join('')}
                </ul>
              </div>
            ` : ''}
          </div>
        `;
      }

      const btnFix = document.getElementById('btn-fix-orphans');
      if (btnFix) {
        btnFix.disabled = (d.orphan_blocks || 0) === 0;
      }
    } catch (e) {
      if (statusBox) statusBox.innerHTML = `<div class="p-3 text-danger">Error: ${e.message}</div>`;
    }
  },

  async fixOrphans() {
    try {
      const res = await api.fixOrphanBlocks();
      if (res.success) {
        app.showToast(`Orphan blocks reclaimed: ${res.data.repaired_count}`, 'success');
        this.checkIntegrity();
        this.refreshHistory();
        if (window.blocksModule) blocksModule.refresh();
      } else {
        app.showToast(res.error || 'Fix orphans failed', 'error');
      }
    } catch (e) {
      app.showToast(e.message, 'error');
    }
  },

  async simulateCrash() {
    const nameInput = document.getElementById('sim-crash-filename');
    const filename = (nameInput && nameInput.value) ? nameInput.value : `crashed_file_${Date.now()}.bin`;

    try {
      const res = await api.simulateInterruptedWrite(filename);
      if (res.success) {
        const out = document.getElementById('recovery-sim-result');
        if (out) {
          out.innerHTML = `
            <div class="alert alert-warning">
              <strong>Crash Simulated:</strong> Interrupted write for <code>${this.escape(filename)}</code>.<br>
              <strong>Status:</strong> ${this.escape(res.data.message || 'Block allocated but metadata uncommitted before simulated power failure.')}<br>
              <em>Click "Run Integrity Check" to observe orphan block detection and repair.</em>
            </div>
          `;
        }
        app.showToast('Simulated crash triggered (power loss before commit)', 'warning');
        this.checkIntegrity();
        this.refreshHistory();
        if (window.blocksModule) blocksModule.refresh();
      } else {
        app.showToast(res.error || 'Simulation failed', 'error');
      }
    } catch (e) {
      app.showToast(e.message, 'error');
    }
  },

  async simulateRestart() {
    try {
      const res = await api.simulateRestartPersistence();
      if (res.success) {
        const out = document.getElementById('recovery-sim-result');
        if (out) {
          out.innerHTML = `
            <div class="alert alert-success">
              <strong>Restart Persistence Verified:</strong><br>
              WAL Replay Check: ${res.data.wal_clean ? 'Clean (All committed transactions replayed)' : 'Recovered'}<br>
              Blocks verified: ${res.data.verified_blocks}<br>
              Database Integrity: ${res.data.database_ok ? 'OK' : 'Error'}
            </div>
          `;
        }
        app.showToast('System restart persistence verified', 'success');
        this.checkIntegrity();
        this.refreshHistory();
      } else {
        app.showToast(res.error || 'Restart simulation failed', 'error');
      }
    } catch (e) {
      app.showToast(e.message, 'error');
    }
  },

  async refreshHistory() {
    try {
      const res = await api.getRecoveryHistory();
      if (!res.success) return;

      const history = res.data.history || [];
      const tbody = document.getElementById('recovery-history-body');
      if (!tbody) return;

      if (history.length === 0) {
        tbody.innerHTML = '<tr><td colspan="4" class="text-center text-muted p-3">No recovery events recorded yet.</td></tr>';
        return;
      }

      tbody.innerHTML = history.slice(-10).reverse().map(h => `
        <tr>
          <td><span class="badge badge-info">${this.escape(h.action || 'CHECK')}</span></td>
          <td>${this.escape(h.description || '-')}</td>
          <td class="font-mono small">${this.escape(h.status || 'OK')}</td>
          <td class="text-muted small">${new Date(h.timestamp * 1000).toLocaleString()}</td>
        </tr>
      `).join('');
    } catch (e) {
      console.error('Error fetching recovery history:', e);
    }
  },

  escape(s) {
    if (!s) return '';
    return String(s).replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;');
  }
};
