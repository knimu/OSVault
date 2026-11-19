/**
 * OSVault - Page Cache & Virtual Memory Paging Module
 */

window.cacheModule = {
  simulation: null,
  translation: null,
  stepIndex: 0,
  playbackTimer: null,

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

    const prev = document.getElementById('btn-page-prev');
    if (prev) prev.onclick = () => this.stopPlayback(() => this.moveStep(-1));

    const next = document.getElementById('btn-page-next');
    if (next) next.onclick = () => this.stopPlayback(() => this.moveStep(1));

    const play = document.getElementById('btn-page-play');
    if (play) play.onclick = () => this.togglePlayback();
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
    const pageSizeInput = document.getElementById('page-size');
    const logicalAddressInput = document.getElementById('logical-address');

    const seqStr = refInput ? refInput.value.trim() : '7,0,1,2,0,3,0,4,2,3,0,3,2,1,2,0,1,7,0,1';
    const frames = framesInput ? parseInt(framesInput.value, 10) || 3 : 3;
    const algo = algoInput ? algoInput.value : 'FIFO';
    const pageSize = pageSizeInput ? parseInt(pageSizeInput.value, 10) || 4096 : 4096;
    const logicalAddress = logicalAddressInput ? parseInt(logicalAddressInput.value, 10) : 8192;

    const sequence = seqStr.split(/[,\s]+/).map(x => parseInt(x, 10)).filter(x => !isNaN(x));
    if (sequence.length === 0) {
      window.app.showToast('Please provide a valid comma-separated reference string (e.g., 7, 0, 1, 2)', 'warning');
      return;
    }
    if (sequence.some(page => page < 0)) {
      window.app.showToast('Virtual page numbers cannot be negative.', 'warning');
      return;
    }
    if (frames < 1 || frames > 10 || pageSize < 1) {
      window.app.showToast('Frame count must be 1-10 and page size must be positive.', 'warning');
      return;
    }

    const statsBox = document.getElementById('page-sim-stats');
    const container = document.getElementById('page-sim-container');

    try {
      const res = await window.api.request('/api/cache/simulate', {
        method: 'POST',
        body: JSON.stringify({ sequence, frames, page_size: pageSize, algorithm: algo, logical_address: logicalAddress })
      });

      const r = (algo === 'LRU') ? (res.lru || res.fifo) : (res.fifo || res.lru);
      if (!r) throw new Error('No simulation results returned');
      this.stopPlayback();
      this.simulation = r;
      this.translation = res.translation || null;
      this.stepIndex = 0;

      if (statsBox) {
        statsBox.innerHTML = `
          <div class="stat-card p-3 mb-2">
            <div class="d-flex justify-between align-center">
              <div>
                <strong>Algorithm:</strong> <span class="badge badge-primary">${r.algorithm}</span> &bull; 
                <strong>Page Frames:</strong> ${r.frame_count} &bull; 
                <strong>Page Size:</strong> ${r.page_size} bytes &bull;
                <strong>Total References:</strong> ${sequence.length}
              </div>
              <div>
                <span class="badge badge-success">Hits: ${r.total_hits} (${r.hit_ratio.toFixed(1)}%)</span>
                <span class="badge badge-danger">Page Faults: ${r.total_misses} (${r.miss_ratio.toFixed(1)}%)</span>
              </div>
            </div>
          </div>
        `;
      }

      this.renderStep(this.translation);

      window.app.showToast(`Paging simulation complete: ${r.total_misses} page faults.`, 'info');
    } catch (err) {
      window.app.showToast('Simulation failed: ' + err.message, 'error');
    }
  },

  renderStep(translation) {
    const container = document.getElementById('page-sim-container');
    if (!container || !this.simulation) return;

    const r = this.simulation;
    const step = r.steps[this.stepIndex];
    if (!step) return;

    const frameCards = Array.from({ length: r.frame_count }, (_, frame) => {
      const page = step.frames[frame];
      return `
        <div class="paging-frame ${page !== undefined ? 'occupied' : 'free'}">
          <div class="paging-frame-label">RAM Frame ${frame}</div>
          <div class="paging-frame-page">${page !== undefined ? 'Page ' + page : 'Free'}</div>
        </div>`;
    }).join('');

    const virtualPages = r.sequence.filter((page, index, list) => list.indexOf(page) === index)
      .sort((a, b) => a - b)
      .map(page => {
        const entry = (step.page_table || []).find(item => item.page === page);
        return `
          <div class="paging-page ${entry && entry.present ? 'mapped' : 'unmapped'}">
            <strong>Virtual Page ${page}</strong>
            <span>${entry && entry.present ? 'Frame ' + entry.frame : 'Not resident'}</span>
          </div>`;
      }).join('');

    const pageTableRows = (step.page_table || []).map(entry => `
      <tr>
        <td>Page ${entry.page}</td>
        <td>${entry.present ? 'Frame ' + entry.frame : '-'}</td>
        <td><span class="badge ${entry.present ? 'badge-success' : 'badge-secondary'}">${entry.present ? 'VALID' : 'INVALID'}</span></td>
      </tr>`).join('');

    const addressText = translation && translation.logical_address !== undefined
      ? `
        <div class="paging-translation">
          <strong>Logical-to-Physical Address Translation</strong>
          <div>Logical ${translation.logical_address} = Page ${translation.page} + Offset ${translation.offset}</div>
          <div>${translation.fault
            ? 'Page fault: address has no resident mapping'
            : `Physical ${translation.physical_address} = Frame ${translation.frame} × ${r.page_size} + Offset ${translation.offset}`}</div>
        </div>`
      : '';

    container.innerHTML = `
      <div class="paging-step-header">
        <div>
          <strong>Step ${this.stepIndex + 1} / ${r.steps.length}</strong>
          <span class="font-mono ms-2">Reference: Page ${step.page}</span>
        </div>
        <span class="badge ${step.hit ? 'badge-success' : 'badge-danger'}">${step.hit ? 'PAGE HIT' : 'PAGE FAULT'}${step.evicted_page >= 0 ? ' - Evicted Page ' + step.evicted_page : ''}</span>
      </div>
      <div class="grid-3 mb-3">
        <div class="paging-panel">
          <div class="paging-panel-title">Physical RAM Frames</div>
          <div class="paging-frames">${frameCards}</div>
        </div>
        <div class="paging-panel">
          <div class="paging-panel-title">Virtual Address Space</div>
          <div class="paging-pages">${virtualPages}</div>
        </div>
        <div class="paging-panel">
          <div class="paging-panel-title">Live Page Table</div>
          <table class="data-table paging-table">
            <thead><tr><th>Page</th><th>Frame</th><th>Present</th></tr></thead>
            <tbody>${pageTableRows}</tbody>
          </table>
        </div>
      </div>
      ${addressText}
      <div class="table-wrap mt-3">
        <table class="data-table">
          <thead><tr><th>Step</th><th>Reference</th><th>Result</th><th>Frame</th><th>Evicted</th></tr></thead>
          <tbody>${r.steps.map((item, index) => `
            <tr class="${index === this.stepIndex ? 'paging-current-row' : ''}">
              <td>${index + 1}</td>
              <td class="font-mono">Page ${item.page}</td>
              <td><span class="badge ${item.hit ? 'badge-success' : 'badge-danger'}">${item.hit ? 'HIT' : 'FAULT'}</span></td>
              <td>${item.frame >= 0 ? item.frame : '-'}</td>
              <td>${item.evicted_page >= 0 ? item.evicted_page : '-'}</td>
            </tr>`).join('')}
          </tbody>
        </table>
      </div>`;
  },

  moveStep(delta) {
    if (!this.simulation) return;
    this.stepIndex = Math.max(0, Math.min(this.simulation.steps.length - 1, this.stepIndex + delta));
    this.renderStep(this.translation);
  },

  togglePlayback() {
    if (!this.simulation) {
      window.app.showToast('Run a paging simulation first.', 'warning');
      return;
    }
    if (this.playbackTimer) {
      this.stopPlayback();
      return;
    }
    const button = document.getElementById('btn-page-play');
    if (button) button.textContent = 'Pause';
    this.playbackTimer = setInterval(() => {
      if (this.stepIndex >= this.simulation.steps.length - 1) {
        this.stopPlayback();
        return;
      }
      this.moveStep(1);
    }, 800);
  },

  stopPlayback(callback) {
    if (this.playbackTimer) {
      clearInterval(this.playbackTimer);
      this.playbackTimer = null;
    }
    const button = document.getElementById('btn-page-play');
    if (button) button.textContent = 'Play';
    if (callback) callback();
  }
};

window.Cache = window.cacheModule;
