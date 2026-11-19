/**
 * OSVault - Operation Logger & Audit Trail
 * Displays WAL persistent event stream, filtering, searching, and audit history.
 */

window.logsModule = {
  currentFilter: '',
  searchQuery: '',
  limit: 50,
  offset: 0,
  autoRefreshInterval: null,

  init() {
    this.bindEvents();
    this.refresh();
  },

  bindEvents() {
    const filterSelect = document.getElementById('log-filter-type');
    if (filterSelect) {
      filterSelect.addEventListener('change', (e) => {
        this.currentFilter = e.target.value;
        this.offset = 0;
        this.refresh();
      });
    }

    const searchInput = document.getElementById('log-search-input');
    if (searchInput) {
      let debounceTimeout = null;
      searchInput.addEventListener('input', (e) => {
        clearTimeout(debounceTimeout);
        debounceTimeout = setTimeout(() => {
          this.searchQuery = e.target.value.trim();
          this.offset = 0;
          this.refresh();
        }, 300);
      });
    }

    const btnRefresh = document.getElementById('btn-refresh-logs');
    if (btnRefresh) {
      btnRefresh.addEventListener('click', () => this.refresh());
    }

    const btnClear = document.getElementById('btn-clear-logs');
    if (btnClear) {
      btnClear.addEventListener('click', () => this.clearLogs());
    }

    const autoRefreshToggle = document.getElementById('toggle-auto-refresh-logs');
    if (autoRefreshToggle) {
      autoRefreshToggle.addEventListener('change', (e) => {
        if (e.target.checked) {
          this.autoRefreshInterval = setInterval(() => this.refresh(), 3000);
          app.showToast('Log auto-refresh enabled (3s)', 'info');
        } else {
          clearInterval(this.autoRefreshInterval);
          this.autoRefreshInterval = null;
          app.showToast('Log auto-refresh paused', 'info');
        }
      });
    }

    // Pagination
    const btnPrev = document.getElementById('btn-log-prev');
    if (btnPrev) {
      btnPrev.addEventListener('click', () => {
        if (this.offset >= this.limit) {
          this.offset -= this.limit;
          this.refresh();
        }
      });
    }
    const btnNext = document.getElementById('btn-log-next');
    if (btnNext) {
      btnNext.addEventListener('click', () => {
        this.offset += this.limit;
        this.refresh();
      });
    }
  },

  async refresh() {
    try {
      const res = await api.getLogs(this.currentFilter, this.searchQuery, this.limit, this.offset);
      if (!res.success) return;

      const data = res.data;
      const logs = data.logs || [];
      const total = data.total || logs.length;

      // Update total badge
      const countBadge = document.getElementById('log-count-badge');
      if (countBadge) {
        countBadge.textContent = `${total} events recorded`;
      }

      // Update pagination indicators
      const pageInfo = document.getElementById('log-page-info');
      if (pageInfo) {
        const start = total === 0 ? 0 : this.offset + 1;
        const end = Math.min(this.offset + logs.length, total);
        pageInfo.textContent = `Showing ${start}-${end} of ${total}`;
      }

      const tbody = document.getElementById('logs-table-body');
      if (!tbody) return;

      if (logs.length === 0) {
        tbody.innerHTML = '<tr><td colspan="5" class="text-center text-muted p-4">No audit log records match the current filter.</td></tr>';
        return;
      }

      tbody.innerHTML = logs.map(l => {
        const timeStr = l.timestamp ? new Date(l.timestamp * 1000).toLocaleString() : '-';
        let badgeColor = 'badge-secondary';
        const type = l.event_type || 'SYSTEM';

        if (type.includes('CREATE') || type.includes('ALLOC')) badgeColor = 'badge-success';
        else if (type.includes('DELETE') || type.includes('FREE')) badgeColor = 'badge-danger';
        else if (type.includes('DEADLOCK') || type.includes('FAIL') || type.includes('ERROR')) badgeColor = 'badge-danger';
        else if (type.includes('RECOVERY') || type.includes('INTEGRITY')) badgeColor = 'badge-warning';
        else if (type.includes('HIT')) badgeColor = 'badge-teal';
        else if (type.includes('MISS')) badgeColor = 'badge-warning';
        else if (type.includes('SYNC') || type.includes('IPC')) badgeColor = 'badge-info';

        return `
          <tr>
            <td class="font-mono text-muted small">#${l.id}</td>
            <td><span class="badge ${badgeColor}">${this.escape(type)}</span></td>
            <td>${this.escape(l.description)}</td>
            <td class="font-mono small text-muted">${this.escape(l.metadata || '-')}</td>
            <td class="text-muted small">${timeStr}</td>
          </tr>
        `;
      }).join('');
    } catch (e) {
      console.error('Error fetching logs:', e);
    }
  },

  async clearLogs() {
    if (!confirm('Are you sure you want to clear the audit logs?')) return;
    try {
      const res = await api.clearLogs();
      if (res.success) {
        app.showToast('Audit log stream cleared', 'info');
        this.offset = 0;
        this.refresh();
      } else {
        app.showToast(res.error || 'Failed to clear logs', 'error');
      }
    } catch (e) {
      app.showToast(e.message, 'error');
    }
  },

  escape(s) {
    if (!s) return '';
    return String(s).replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;');
  }
};

window.Logs = window.logsModule;
