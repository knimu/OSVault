/**
 * OSVault - Dashboard Module
 */

window.dashboardModule = {
  async init() {
    this.bindEvents();
    await this.refresh();
  },

  bindEvents() {
    const btnSeed = document.getElementById('btn-dash-seed');
    if (btnSeed) {
      btnSeed.onclick = () => this.seedDemoFiles();
    }
  },

  async refresh() {
    try {
      const [metrics, filesData, logsData] = await Promise.all([
        window.api.getMetrics().catch(() => null),
        window.api.listFiles().catch(() => ({ files: [] })),
        window.api.getLogs('', '', 5).catch(() => ({ logs: [] }))
      ]);

      if (metrics) {
        const elTotalFiles = document.getElementById('dash-total-files');
        if (elTotalFiles) elTotalFiles.textContent = metrics.file_count || 0;

        const elAlloc = document.getElementById('dash-allocated-blocks');
        if (elAlloc) elAlloc.textContent = `${metrics.allocated_blocks || 0} / ${metrics.total_blocks || 1024}`;

        const elPct = document.getElementById('dash-storage-usage-pct');
        if (elPct) elPct.textContent = `${(metrics.storage_utilization_pct || 0).toFixed(1)}% Allocated`;

        const elHitRate = document.getElementById('dash-cache-hit-rate');
        if (elHitRate) elHitRate.textContent = `${(metrics.cache_hit_ratio || 0).toFixed(1)}%`;

        const elRatio = document.getElementById('dash-cache-ratio');
        if (elRatio) elRatio.textContent = `${metrics.cache_hits || 0} hits / ${metrics.cache_misses || 0} misses`;

        const elBadge = document.getElementById('dash-status-badge');
        if (elBadge) {
          elBadge.textContent = metrics.integrity_healthy ? 'WAL Crash-Consistent (Healthy)' : 'Inconsistency Detected';
          elBadge.className = metrics.integrity_healthy ? 'stat-subtext highlight-blue' : 'stat-subtext highlight-red';
        }
      }

      // Recent Files
      const recentFilesBody = document.getElementById('dash-recent-files-body');
      if (recentFilesBody && filesData && filesData.files) {
        if (filesData.files.length === 0) {
          recentFilesBody.innerHTML = '<tr><td colspan="4" class="text-muted text-center p-3">No files stored yet. Click "Seed Demo Files" or create one in Virtual Filesystem.</td></tr>';
        } else {
          recentFilesBody.innerHTML = filesData.files.slice(0, 5).map(f => `
            <tr>
              <td><strong class="highlight-blue">${this.escape(f.name)}</strong></td>
              <td>${(f.size || 0).toLocaleString()} B</td>
              <td><span class="badge badge-info">${f.block_count || 1} blk</span></td>
              <td class="text-muted small">${f.created_at ? new Date(f.created_at * 1000).toLocaleTimeString() : '-'}</td>
            </tr>
          `).join('');
        }
      }

      // Recent Logs
      const recentLogsBody = document.getElementById('dash-recent-logs-body');
      if (recentLogsBody && logsData && logsData.logs) {
        if (logsData.logs.length === 0) {
          recentLogsBody.innerHTML = '<tr><td colspan="4" class="text-muted text-center p-3">No activity logs recorded yet.</td></tr>';
        } else {
          recentLogsBody.innerHTML = logsData.logs.slice(0, 5).map(l => `
            <tr>
              <td class="font-mono text-muted small">#${l.id}</td>
              <td><span class="badge badge-primary">${this.escape(l.event_type)}</span></td>
              <td>${this.escape(l.description)}</td>
              <td class="text-muted small">${l.timestamp ? new Date(l.timestamp * 1000).toLocaleTimeString() : '-'}</td>
            </tr>
          `).join('');
        }
      }
    } catch (err) {
      console.error('Failed to load dashboard:', err);
    }
  },

  async seedDemoFiles() {
    try {
      window.app.showToast('Seeding demo virtual files...', 'info');
      await window.api.createFile('kernel_boot.cfg', 'BOOT_MODE=ACPI\nCORES=4\nMEMORY=1024MB\nJOURNAL=WAL\nSTORAGE_ENGINE=OSVAULT');
      await window.api.createFile('process_table.dat', 'PID 1: /sbin/init [RUNNING]\nPID 2: /bin/kthreadd [SLEEP]\nPID 3: /usr/bin/crond [WAIT]');
      await window.api.createFile('memory_map.sys', '0x00000000 - 0x000FFFFF: Reserved BIOS\n0x00100000 - 0x003FFFFF: OS Kernel Core\n0x00400000 - 0x00FFFFFF: Dynamic Allocation Pool');
      window.app.showToast('Demo files successfully seeded!', 'success');
      await this.refresh();
      if (window.filesModule) window.filesModule.refresh();
      if (window.blocksModule) window.blocksModule.refresh();
    } catch (err) {
      window.app.showToast('Seed failed: ' + err.message, 'error');
    }
  },

  escape(s) {
    if (!s) return '';
    return String(s).replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;');
  }
};

window.Dashboard = window.dashboardModule;
