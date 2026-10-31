import { API } from './api.js';

export const Cache = {
  async init() {
    this.bindEvents();
    await this.refreshInMemStats();
  },

  bindEvents() {
    const btnSim = document.getElementById('btn-run-paging-sim');
    if (btnSim) {
      btnSim.addEventListener('click', () => this.runSimulation());
    }

    const btnClear = document.getElementById('btn-clear-inmem-cache');
    if (btnClear) {
      btnClear.addEventListener('click', async () => {
        await API.clearCache();
        window.App.toast('In-memory cache cleared.', 'success');
        this.refreshInMemStats();
      });
    }
  },

  async refreshInMemStats() {
    try {
      const stats = await API.getCacheStats();
      const el = document.getElementById('inmem-cache-stats-text');
      if (el) {
        el.textContent = `Policy: ${stats.policy} | Capacity: ${stats.capacity} entries | Active: ${stats.entries} | Hits: ${stats.hits} | Misses: ${stats.misses} | Evictions: ${stats.evictions} | Hit Ratio: ${stats.hit_ratio.toFixed(1)}%`;
      }
    } catch (err) {
      console.error('Failed to load cache stats:', err);
    }
  },

  async runSimulation() {
    const seqStr = document.getElementById('paging-sequence-input').value.trim();
    const frames = parseInt(document.getElementById('paging-frames-input').value, 10);

    const sequence = seqStr.split(/[,\s]+/).map(x => parseInt(x, 10)).filter(x => !isNaN(x));
    if (sequence.length === 0) {
      window.App.toast('Please provide a valid sequence of page numbers.', 'error');
      return;
    }

    try {
      const data = await API.simulatePaging(sequence, frames, 'BOTH');
      this.renderPagingResults(data.fifo, 'fifo-results-container');
      this.renderPagingResults(data.lru, 'lru-results-container');
      window.App.toast('Page replacement simulation completed.', 'success');
    } catch (err) {
      window.App.toast(`Simulation failed: ${err.message}`, 'error');
    }
  },

  renderPagingResults(res, containerId) {
    const container = document.getElementById(containerId);
    if (!container) return;

    let html = `
      <div style="margin-bottom: 0.75rem; font-size: 0.85rem;">
        <span class="status-badge" style="background:#1e3a5f; color:#93c5fd; border-color:#3b82f6;">Hits: ${res.total_hits} (${res.hit_ratio.toFixed(1)}%)</span>
        <span class="status-badge error" style="margin-left: 6px;">Faults: ${res.total_misses} (${res.miss_ratio.toFixed(1)}%)</span>
      </div>
      <div class="table-wrap">
        <table class="data-table" style="font-size: 0.75rem;">
          <thead>
            <tr>
              <th>Step</th>
              <th>Page</th>
              <th>Frames</th>
              <th>Status</th>
              <th>Evicted</th>
            </tr>
          </thead>
          <tbody>
    `;

    res.steps.forEach((s, idx) => {
      const statusBadge = s.hit
        ? '<span style="color: var(--status-success); font-weight:600;">HIT</span>'
        : '<span style="color: var(--status-danger); font-weight:600;">FAULT</span>';
      
      const frameStr = `[ ${s.frames.join(', ')} ]`;
      const evicted = s.evicted_page >= 0 ? `Page ${s.evicted_page}` : '-';

      html += `
        <tr>
          <td>${idx + 1}</td>
          <td><strong>${s.page}</strong></td>
          <td style="font-family: var(--font-mono);">${frameStr}</td>
          <td>${statusBadge}</td>
          <td style="color: var(--text-dim);">${evicted}</td>
        </tr>
      `;
    });

    html += `</tbody></table></div>`;
    container.innerHTML = html;
  }
};
