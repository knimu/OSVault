/**
 * OSVault - Dashboard Module
 */

window.dashboardModule = {
  refreshTimer: null,

  async init() {
    this.bindEvents();
    await this.refresh();
    if (!this.refreshTimer) {
      this.refreshTimer = setInterval(() => this.refresh(), 15000);
    }
  },

  bindEvents() {
    const btnSeed = document.getElementById('btn-dash-seed');
    if (btnSeed) {
      btnSeed.onclick = () => this.seedDemoFiles();
    }

    document.querySelectorAll('[data-dashboard-view]').forEach(control => {
      control.onclick = event => {
        event.preventDefault();
        const view = control.dataset.dashboardView;
        if (view && window.app) {
          window.app.switchView(view);
          window.location.hash = view;
        }
      };
    });
  },

  async refresh() {
    try {
      const [health, metrics, filesData, logsData, cacheStats, integrity] = await Promise.all([
        window.api.getHealth().catch(() => null),
        window.api.getMetrics().catch(() => null),
        window.api.listFiles().catch(() => ({ files: [] })),
        window.api.getLogs('', '', 5).catch(() => ({ logs: [] })),
        window.api.getCacheStats().catch(() => null),
        window.api.checkIntegrity().catch(() => null)
      ]);

      const backendValue = document.getElementById('dash-backend-value');
      if (backendValue) {
        const online = Boolean(health && (health.status === 'healthy' || health.ok));
        backendValue.textContent = online ? 'Online' : 'Unavailable';
        backendValue.className = `stat-value ${online ? 'highlight-teal' : 'highlight-red'}`;
      }

      const lastUpdate = document.getElementById('dash-last-update');
      if (lastUpdate) lastUpdate.textContent = `Updated ${new Date().toLocaleTimeString()}`;

      if (metrics) {
        const elTotalFiles = document.getElementById('dash-total-files');
        if (elTotalFiles) elTotalFiles.textContent = metrics.file_count || 0;

        const elAlloc = document.getElementById('dash-allocated-blocks');
        if (elAlloc) elAlloc.textContent = `${metrics.allocated_blocks || 0} / ${metrics.total_blocks || 1024}`;

        const elPct = document.getElementById('dash-storage-usage-pct');
        if (elPct) elPct.textContent = `${(metrics.storage_utilization_pct || 0).toFixed(1)}% Allocated`;

        const totalBytes = document.getElementById('dash-total-bytes');
        if (totalBytes) totalBytes.textContent = this.formatBytes(metrics.total_bytes || 0);

        const operationCount = document.getElementById('dash-operation-count');
        if (operationCount) operationCount.textContent = `${metrics.recent_operations_count || 0} operations logged`;

        this.setStorageVisual(metrics);
      }

      if (cacheStats) {
        const hitRate = Number(cacheStats.hit_ratio || 0);
        const hits = Number(cacheStats.hits || 0);
        const misses = Number(cacheStats.misses || 0);
        const elHitRate = document.getElementById('dash-cache-hit-rate');
        if (elHitRate) elHitRate.textContent = `${hitRate.toFixed(1)}%`;
        const elRatio = document.getElementById('dash-cache-ratio');
        if (elRatio) elRatio.textContent = `${hits} hits / ${misses} misses`;
        const elCachePercent = document.getElementById('dash-cache-percent');
        if (elCachePercent) elCachePercent.textContent = `${hitRate.toFixed(1)}%`;
        const elCacheProgress = document.getElementById('dash-cache-progress');
        if (elCacheProgress) elCacheProgress.style.width = `${Math.max(0, Math.min(100, hitRate))}%`;
        const elCacheHits = document.getElementById('dash-cache-hits-detail');
        if (elCacheHits) elCacheHits.textContent = hits;
        const elCacheMisses = document.getElementById('dash-cache-misses-detail');
        if (elCacheMisses) elCacheMisses.textContent = misses;
      }

      if (integrity) {
        const integrityValue = document.getElementById('dash-integrity-value');
        const integrityDetail = document.getElementById('dash-integrity-detail');
        if (integrityValue) {
          integrityValue.textContent = integrity.is_healthy ? 'Healthy' : 'Issues';
          integrityValue.className = `stat-value ${integrity.is_healthy ? 'highlight-teal' : 'highlight-red'}`;
        }
        if (integrityDetail) {
          const issueCount = (integrity.orphaned_blocks || 0) +
            (integrity.duplicate_references || 0) +
            (integrity.invalid_block_ids || 0) +
            (integrity.block_count_mismatches || 0);
          integrityDetail.textContent = integrity.is_healthy
            ? `${integrity.files_checked || 0} files checked`
            : `${issueCount} integrity issues found`;
        }
      }

      this.checkModuleStatuses();

      // Recent Files
      const recentFilesBody = document.getElementById('dash-recent-files-body');
      if (recentFilesBody && filesData && filesData.files) {
        if (filesData.files.length === 0) {
          recentFilesBody.innerHTML = '<tr><td colspan="4" class="text-muted text-center p-3">No files stored yet. Seed demo files or create one in Virtual Filesystem.</td></tr>';
        } else {
          recentFilesBody.innerHTML = filesData.files.slice(0, 5).map(f => `
            <tr>
              <td><strong class="highlight-blue">${this.escape(f.name)}</strong></td>
              <td>${(f.size || 0).toLocaleString()} B</td>
              <td><span class="badge badge-info">${f.block_count || 1} blk</span></td>
              <td class="text-muted small">${this.formatTimestamp(f.created_at)}</td>
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
              <td class="text-muted small">${this.formatTimestamp(l.timestamp)}</td>
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
      const demoFiles = [
        ['kernel_boot.cfg', 'BOOT_MODE=ACPI\nCORES=4\nMEMORY=1024MB\nJOURNAL=WAL\nSTORAGE_ENGINE=OSVAULT'],
        ['process_table.dat', 'PID 1: /sbin/init [RUNNING]\nPID 2: /bin/kthreadd [SLEEP]\nPID 3: /usr/bin/crond [WAIT]'],
        ['memory_map.sys', '0x00000000 - 0x000FFFFF: Reserved BIOS\n0x00100000 - 0x003FFFFF: OS Kernel Core\n0x00400000 - 0x00FFFFFF: Dynamic Allocation Pool']
      ];
      const existing = await window.api.listFiles().catch(() => ({ files: [] }));
      const existingNames = new Set((existing.files || []).map(file => file.name));
      let added = 0;
      for (const [name, content] of demoFiles) {
        if (!existingNames.has(name)) {
          await window.api.createFile(name, content);
          added += 1;
        }
      }
      if (added === 0) {
        window.app.showToast('Demo files are already present.', 'info');
      } else {
      window.app.showToast('Demo files successfully seeded!', 'success');
      }
      await this.refresh();
      if (window.filesModule) window.filesModule.refresh();
      if (window.blocksModule) window.blocksModule.refresh();
    } catch (err) {
      window.app.showToast('Seed failed: ' + err.message, 'error');
    }
  },

  setStorageVisual(metrics) {
    const total = Number(metrics.total_blocks || 1024);
    const allocated = Number(metrics.allocated_blocks || 0);
    const free = Math.max(0, total - allocated);
    const percentage = total > 0 ? (allocated / total) * 100 : 0;

    const blockPercent = document.getElementById('dash-block-percent');
    if (blockPercent) blockPercent.textContent = `${percentage.toFixed(1)}% used`;
    const blockProgress = document.getElementById('dash-block-progress');
    if (blockProgress) blockProgress.style.width = `${percentage}%`;
    const blocksUsed = document.getElementById('dash-blocks-used');
    if (blocksUsed) blocksUsed.textContent = `${allocated} blocks`;
    const blocksFree = document.getElementById('dash-blocks-free');
    if (blocksFree) blocksFree.textContent = `${free} blocks`;
  },

  async checkModuleStatuses() {
    const checks = [
      ['files', window.api.listFiles()],
      ['blocks', window.api.getBlocks()],
      ['cache', window.api.getCacheStats()],
      ['scheduler', window.api.request('/api/scheduler/simulate', {
        method: 'POST',
        body: JSON.stringify({ processes: [], algorithm: 'FCFS' })
      }).catch(error => {
        if (String(error.message).includes('Process list')) return Promise.resolve({});
        throw error;
      })],
      ['memory', window.api.getMemoryState()],
      ['sync', window.api.getCsState()],
      ['deadlock', window.api.getDeadlockPreset('safe')],
      ['ipc', window.api.getPipe()],
      ['clocks', window.api.getClockNodes()],
      ['logs', window.api.getLogs('', '', 1)],
      ['recovery', window.api.getRecoveryHistory()]
    ];

    const results = await Promise.allSettled(checks.map(([, request]) => request));
    let onlineCount = 0;
    checks.forEach(([view], index) => {
      const online = results[index].status === 'fulfilled';
      if (online) onlineCount += 1;
      const card = document.querySelector(`[data-dashboard-view="${view}"].dashboard-module-card`);
      if (card) {
        const status = card.querySelector('em');
        if (status) {
          status.textContent = online ? 'Available' : 'Unavailable';
          status.className = online ? 'available' : 'unavailable';
        }
      }
    });

    const modulesOnline = document.getElementById('dash-modules-online');
    if (modulesOnline) {
      modulesOnline.textContent = `${onlineCount} / ${checks.length} available`;
      modulesOnline.className = `badge ${onlineCount === checks.length ? 'badge-success' : 'badge-warning'}`;
    }
  },

  formatBytes(bytes) {
    if (bytes < 1024) return `${bytes} B`;
    const units = ['KB', 'MB', 'GB', 'TB'];
    let value = bytes / 1024;
    let unitIndex = 0;
    while (value >= 1024 && unitIndex < units.length - 1) {
      value /= 1024;
      unitIndex += 1;
    }
    return `${value.toFixed(value >= 10 ? 1 : 2)} ${units[unitIndex]}`;
  },

  formatTimestamp(timestamp) {
    if (!timestamp) return '-';
    const date = typeof timestamp === 'number'
      ? new Date(timestamp < 1e12 ? timestamp * 1000 : timestamp)
      : new Date(timestamp);
    return Number.isNaN(date.getTime()) ? '-' : date.toLocaleString();
  },

  escape(s) {
    if (!s) return '';
    return String(s).replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;');
  }
};

window.Dashboard = window.dashboardModule;
