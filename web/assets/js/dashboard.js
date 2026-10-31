import { API } from './api.js';

export const Dashboard = {
  async init() {
    await this.refresh();
  },

  async refresh() {
    try {
      const data = await API.getMetrics();
      document.getElementById('metric-total-cap').textContent = (data.total_capacity_bytes / 1024 / 1024).toFixed(1) + ' MiB';
      document.getElementById('metric-allocated-blocks').textContent = `${data.allocated_blocks} / ${data.total_blocks}`;
      document.getElementById('metric-free-blocks').textContent = data.free_blocks;
      document.getElementById('metric-utilization').textContent = data.storage_utilization_pct.toFixed(1) + '%';
      document.getElementById('metric-files-count').textContent = data.file_count;
      document.getElementById('metric-total-bytes').textContent = data.total_bytes.toLocaleString() + ' B';
      document.getElementById('metric-cache-hits').textContent = `${data.cache_hits} / ${data.cache_hits + data.cache_misses}`;
      document.getElementById('metric-cache-hit-ratio').textContent = data.cache_hit_ratio.toFixed(1) + '%';
      document.getElementById('metric-operations-count').textContent = data.recent_operations_count;

      const healthBadge = document.getElementById('global-health-badge');
      if (data.integrity_healthy) {
        healthBadge.className = 'status-badge';
        healthBadge.innerHTML = '<span class="status-dot"></span> System Healthy';
      } else {
        healthBadge.className = 'status-badge error';
        healthBadge.innerHTML = '<span class="status-dot"></span> Inconsistency Detected';
      }
    } catch (err) {
      console.error('Failed to load dashboard metrics:', err);
    }
  }
};
