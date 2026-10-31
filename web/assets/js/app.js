/**
 * OSVault - Main Application Orchestrator & View Switcher
 */

window.app = {
  currentView: 'dashboard',
  healthTimer: null,

  init() {
    this.setupNavigation();
    this.setupToastContainer();
    this.setupHealthMonitoring();
    this.initCurrentView();
  },

  setupNavigation() {
    // Nav items
    document.querySelectorAll('.nav-link').forEach(link => {
      link.addEventListener('click', (e) => {
        e.preventDefault();
        const view = link.dataset.view;
        if (view) {
          this.switchView(view);
          window.location.hash = view;
        }
      });
    });

    // Handle hash change / initial load
    window.addEventListener('hashchange', () => {
      const hash = window.location.hash.replace('#', '');
      if (hash && hash !== this.currentView) {
        this.switchView(hash);
      }
    });

    const initialHash = window.location.hash.replace('#', '');
    if (initialHash) {
      this.switchView(initialHash);
    } else {
      this.switchView('dashboard');
    }
  },

  switchView(viewName) {
    // Hide all view sections
    document.querySelectorAll('.view-section').forEach(sec => {
      sec.classList.remove('active');
      sec.style.display = 'none';
    });

    // Deactivate all nav links
    document.querySelectorAll('.nav-link').forEach(link => {
      link.classList.remove('active');
      if (link.dataset.view === viewName) {
        link.classList.add('active');
      }
    });

    // Activate selected view
    const target = document.getElementById(`view-${viewName}`);
    if (target) {
      target.classList.add('active');
      target.style.display = 'block';
      this.currentView = viewName;
    } else {
      // Fallback
      const dash = document.getElementById('view-dashboard');
      if (dash) {
        dash.classList.add('active');
        dash.style.display = 'block';
      }
      this.currentView = 'dashboard';
    }

    // Call sub-module refresh/init on view switch
    this.triggerViewInit(this.currentView);
  },

  triggerViewInit(view) {
    switch (view) {
      case 'dashboard':
        if (window.dashboardModule) window.dashboardModule.init();
        break;
      case 'files':
        if (window.filesModule) window.filesModule.init();
        break;
      case 'blocks':
        if (window.blocksModule) window.blocksModule.init();
        break;
      case 'cache':
        if (window.cacheModule) window.cacheModule.init();
        break;
      case 'scheduler':
        if (window.schedulerModule) window.schedulerModule.init();
        break;
      case 'memory':
        if (window.memoryModule) window.memoryModule.init();
        break;
      case 'sync':
        if (window.syncModule) window.syncModule.init();
        break;
      case 'deadlock':
        if (window.deadlockModule) window.deadlockModule.init();
        break;
      case 'ipc':
        if (window.ipcModule) window.ipcModule.init();
        break;
      case 'clocks':
        if (window.clocksModule) window.clocksModule.init();
        break;
      case 'logs':
        if (window.logsModule) window.logsModule.init();
        break;
      case 'recovery':
        if (window.recoveryModule) window.recoveryModule.init();
        break;
    }
  },

  initCurrentView() {
    this.triggerViewInit(this.currentView);
  },

  setupHealthMonitoring() {
    const checkHealth = async () => {
      try {
        const res = await window.api.getHealth();
        const badge = document.getElementById('status-indicator');
        if (badge) {
          if (res.ok || res.status === 'healthy' || res.status === 'ok') {
            badge.className = 'status-badge online';
            badge.innerHTML = '<span class="status-dot"></span> Online';
          } else {
            badge.className = 'status-badge degraded';
            badge.innerHTML = '<span class="status-dot"></span> Degraded';
          }
        }
      } catch (e) {
        const badge = document.getElementById('status-indicator');
        if (badge) {
          badge.className = 'status-badge offline';
          badge.innerHTML = '<span class="status-dot"></span> Offline';
        }
      }
    };

    checkHealth();
    this.healthTimer = setInterval(checkHealth, 5000);
  },

  setupToastContainer() {
    let container = document.getElementById('toast-container');
    if (!container) {
      container = document.createElement('div');
      container.id = 'toast-container';
      container.className = 'toast-container';
      document.body.appendChild(container);
    }
  },

  showToast(message, type = 'info', duration = 3500) {
    const container = document.getElementById('toast-container');
    if (!container) return;

    const toast = document.createElement('div');
    toast.className = `toast toast-${type} ${type}`;
    
    let icon = 'ℹ️';
    if (type === 'success') icon = '✓';
    else if (type === 'error') icon = '✕';
    else if (type === 'warning') icon = '⚠';

    toast.innerHTML = `
      <span class="toast-icon">${icon}</span>
      <span class="toast-message" style="margin-left: 6px;">${this.escape(message)}</span>
    `;

    container.appendChild(toast);

    setTimeout(() => {
      toast.style.opacity = '0';
      toast.style.transition = 'opacity 0.3s ease';
      setTimeout(() => {
        if (toast.parentNode) toast.parentNode.removeChild(toast);
      }, 300);
    }, duration);
  },

  toast(msg, type) {
    this.showToast(msg, type);
  },

  escape(s) {
    if (!s) return '';
    return String(s).replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;');
  }
};

window.App = window.app;

// Auto bootstrap when DOM ready
document.addEventListener('DOMContentLoaded', () => {
  window.app.init();
});
