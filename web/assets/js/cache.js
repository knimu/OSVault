/**
 * OSVault - Page Cache & Virtual Memory Paging Module
 */

window.cacheModule = {
  init() {
    this.bindEvents();
    this.refresh();
  },

  bindEvents() {
    const btnSim = document.getElementById('btn-run-page-sim');
    if (btnSim) {
      btnSim.onclick = () => this.runSimulation();
    }

    const btnClear = document.getElementById('btn-clear-cache');
    if (btnClear) {
      btnClear.onclick = () => this.clearCache();
    }
  },

  async refresh() {
    try {
      const stats = await window.api.getCacheStats();
      const hitsEl = document.getElementById('cache-hits');
      if (hitsEl) hitsEl.textContent = stats.hits || 0;

      const missEl = document.getElementById('cache-misses');
      if (missEl) missEl.textContent = stats.misses || 0;

      const ratioEl = document.getElementById('cache-ratio');
      if (ratioEl) ratioEl.textContent = `${(stats.hit_ratio || 0).toFixed(1)}%`;

      const cachedEl = document.getElementById('cache-cached-blocks');
      if (cachedEl) cachedEl.textContent = `${stats.entries || 0} / ${stats.capacity || 64}`;
    } catch (err) {
      console.error('Failed to load cache stats:', err);
    }
  },

  async clearCache() {
    try {
      await window.api.clearCache();
      window.app.showToast('In-memory page cache cleared.', 'info');
      this.refresh();
    } catch (err) {
      window.app.showToast('Failed to clear cache: ' + err.message, 'error');
    }
  },

  async runSimulation() {
    const refInput = document.getElementById('page-ref-string');
    const framesInput = document.getElementById('page-frame-count');
    const algoInput = document.getElementById('page-algo-select');

    const seqStr = refInput ? refInput.value.trim() : '7,0,1,2,0,3,0,4,2,3,0,3,2,1,2,0,1,7,0,1';
    const frames = framesInput ? parseInt(framesInput.value, 10) || 3 : 3;
    const algo = algoInput ? algoInput.value : 'FIFO';

    const sequence = seqStr.split(/[,\s]+/).map(x => parseInt(x, 10)).filter(x => !isNaN(x));
    if (sequence.length === 0) {
      window.app.showToast('Please provide a valid comma-separated reference string (e.g., 7, 0, 1, 2)', 'warning');
      return;
    }

    const statsBox = document.getElementById('page-sim-stats');
    const container = document.getElementById('page-sim-container');

    try {
      const res = await window.api.request('/api/cache/simulate', {
        method: 'POST',
        body: JSON.stringify({ sequence, frames, algorithm: algo })
      });

      const r = (algo === 'LRU') ? (res.lru || res.fifo) : (res.fifo || res.lru);
      if (!r) throw new Error('No simulation results returned');

      if (statsBox) {
        statsBox.innerHTML = `
          <div class="stat-card p-3 mb-2">
            <div class="d-flex justify-between align-center">
              <div>
                <strong>Algorithm:</strong> <span class="badge badge-primary">${r.algorithm}</span> &bull; 
                <strong>Page Frames:</strong> ${r.frame_count} &bull; 
                <strong>Total References:</strong> ${sequence.length}
              </div>
              <div>
                <span class="badge badge-success">Hits: ${r.total_hits} (${(r.hit_ratio * 100).toFixed(1)}%)</span>
                <span class="badge badge-danger">Page Faults: ${r.total_misses} (${(r.miss_ratio * 100).toFixed(1)}%)</span>
              </div>
            </div>
          </div>
        `;
      }

      if (container) {
        let frameHeaders = '';
        for (let f = 0; f < frames; ++f) {
          frameHeaders += `<th>Frame ${f}</th>`;
        }

        container.innerHTML = `
          <table class="data-table">
            <thead>
              <tr>
                <th>Step</th>
                <th>Page Ref</th>
                ${frameHeaders}
                <th>Result</th>
                <th>Evicted</th>
              </tr>
            </thead>
            <tbody>
              ${(r.steps || []).map((s, idx) => {
                let frameCols = '';
                for (let f = 0; f < frames; ++f) {
                  const val = (s.frames && s.frames[f] !== undefined) ? s.frames[f] : '-';
                  frameCols += `<td class="font-mono text-center">${val}</td>`;
                }

                return `
                  <tr>
                    <td class="text-muted small">#${idx + 1}</td>
                    <td class="font-mono"><strong>${s.page}</strong></td>
                    ${frameCols}
                    <td>
                      ${s.hit ? '<span class="badge badge-success">HIT</span>' : '<span class="badge badge-danger">FAULT</span>'}
                    </td>
                    <td class="font-mono text-muted small">
                      ${s.evicted_page !== undefined && s.evicted_page !== -1 ? 'Evicted ' + s.evicted_page : '-'}
                    </td>
                  </tr>
                `;
              }).join('')}
            </tbody>
          </table>
        `;
      }

      window.app.showToast(`Paging simulation complete: ${r.total_misses} page faults.`, 'info');
    } catch (err) {
      window.app.showToast('Simulation failed: ' + err.message, 'error');
    }
  }
};

window.Cache = window.cacheModule;
