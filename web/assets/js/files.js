/**
 * OSVault - Virtual Filesystem Module
 */

window.filesModule = {
  isEditing: false,
  currentDirectoryId: 0,
  directoryStack: [{ id: 0, name: 'Root' }],

  init() {
    this.bindEvents();
    this.refresh();
  },

  bindEvents() {
    const btnNewFolder = document.getElementById('btn-new-folder');
    if (btnNewFolder) btnNewFolder.onclick = () => this.toggleDirectoryCreate(true);
    const btnCreateFolder = document.getElementById('btn-create-folder');
    if (btnCreateFolder) btnCreateFolder.onclick = () => this.createDirectory();
    const btnCancelFolder = document.getElementById('btn-cancel-folder');
    if (btnCancelFolder) btnCancelFolder.onclick = () => this.toggleDirectoryCreate(false);
    // Open Create Modal
    const btnCreate = document.getElementById('btn-create-file-modal');
    if (btnCreate) {
      btnCreate.onclick = () => this.openCreateModal();
    }

    // Refresh Files
    const btnRefresh = document.getElementById('btn-refresh-files');
    if (btnRefresh) {
      btnRefresh.onclick = () => this.refresh();
    }

    // Live search
    const searchInput = document.getElementById('file-search-input');
    if (searchInput) {
      let timeout = null;
      searchInput.oninput = () => {
        clearTimeout(timeout);
        timeout = setTimeout(() => this.refresh(searchInput.value), 250);
      };
    }

    // Modal buttons
    const btnSave = document.getElementById('btn-save-file');
    if (btnSave) {
      btnSave.onclick = () => this.saveFile();
    }

    const btnCancel = document.getElementById('btn-cancel-modal');
    if (btnCancel) {
      btnCancel.onclick = () => this.closeModal();
    }

    const btnClose = document.getElementById('btn-close-modal');
    if (btnClose) {
      btnClose.onclick = () => this.closeModal();
    }

    const btnCloseView = document.getElementById('btn-close-view-modal');
    if (btnCloseView) {
      btnCloseView.onclick = () => this.closeViewModal();
    }
  },

  async refresh(query = '') {
    const tbody = document.getElementById('files-table-body');
    if (!tbody) return;

    try {
      const [data, directories] = await Promise.all([
        window.api.listFiles(query, this.currentDirectoryId),
        window.api.listDirectories(this.currentDirectoryId)
      ]);
      const files = data.files || [];
      const dirs = directories.directories || [];
      this.renderBreadcrumb();

      const countBadge = document.getElementById('file-count-badge');
      if (countBadge) {
        countBadge.textContent = `${dirs.length} folder${dirs.length === 1 ? '' : 's'}, ${files.length} file${files.length === 1 ? '' : 's'}`;
      }

      if (dirs.length === 0 && files.length === 0) {
        tbody.innerHTML = `<tr><td colspan="7" class="text-center text-muted p-4">This directory is empty. Create a folder or file to begin.</td></tr>`;
        return;
      }

      const directoryRows = dirs.map(d => `
        <tr>
          <td class="font-mono text-muted small">DIR-${d.id}</td>
          <td><button class="file-folder-link" onclick="window.filesModule.openDirectory(${d.id}, '${this.escape(d.name)}')">📁 <strong>${this.escape(d.name)}</strong></button></td>
          <td class="text-muted">-</td>
          <td class="text-muted">Directory</td>
          <td class="text-muted small">${this.formatTimestamp(d.created_at)}</td>
          <td class="text-muted small">-</td>
          <td style="text-align:right;">
            <button class="btn btn-danger btn-sm" onclick="window.filesModule.deleteDirectory(${d.id}, '${this.escape(d.name)}')">Delete</button>
          </td>
        </tr>
      `).join('');

      const fileRows = files.map(f => {
        const blockIds = (f.block_ids && f.block_ids.length > 0) ? f.block_ids.join(', ') : '-';
        const createdStr = this.formatTimestamp(f.created_at);
        const updatedStr = this.formatTimestamp(f.updated_at);
        const safeName = this.escape(f.name);

        return `
          <tr>
            <td class="font-mono text-muted small">#${f.id}</td>
            <td><strong class="highlight-blue">${safeName}</strong></td>
            <td class="font-mono">${(f.size || 0).toLocaleString()} B</td>
            <td><span class="badge badge-primary">${f.block_count || 1} blk</span> <span class="text-muted small">(${blockIds})</span></td>
            <td class="text-muted small">${createdStr}</td>
            <td class="text-muted small">${updatedStr}</td>
            <td style="text-align: right;">
              <div class="d-flex justify-end gap-1">
                <button class="btn btn-secondary btn-sm" onclick="window.filesModule.viewFile('${safeName}')">View</button>
                <button class="btn btn-secondary btn-sm" onclick="window.filesModule.editFile('${safeName}')">Edit</button>
                <button class="btn btn-danger btn-sm" onclick="window.filesModule.deleteFile('${safeName}')">Delete</button>
              </div>
            </td>
          </tr>
        `;
      }).join('');
      tbody.innerHTML = directoryRows + fileRows;
    } catch (err) {
      tbody.innerHTML = `<tr><td colspan="7" class="text-center text-danger p-4">Error loading files: ${err.message}</td></tr>`;
    }
  },

  openCreateModal() {
    this.isEditing = false;
    const modal = document.getElementById('file-modal');
    const title = document.getElementById('modal-title');
    const nameInput = document.getElementById('file-name');
    const contentInput = document.getElementById('file-content');

    if (title) title.textContent = 'Create Virtual File';
    if (nameInput) {
      nameInput.value = '';
      nameInput.removeAttribute('disabled');
    }
    if (contentInput) contentInput.value = '';
    if (modal) modal.classList.add('active');
  },

  async editFile(name) {
    try {
      const f = await window.api.readFile(name, this.currentDirectoryId);
      this.isEditing = true;
      const modal = document.getElementById('file-modal');
      const title = document.getElementById('modal-title');
      const nameInput = document.getElementById('file-name');
      const contentInput = document.getElementById('file-content');

      if (title) title.textContent = `Edit File: ${f.name}`;
      if (nameInput) {
        nameInput.value = f.name;
        nameInput.setAttribute('disabled', 'true');
      }
      if (contentInput) contentInput.value = f.content || '';
      if (modal) modal.classList.add('active');
    } catch (err) {
      window.app.showToast('Could not load file for editing: ' + err.message, 'error');
    }
  },

  closeModal() {
    const modal = document.getElementById('file-modal');
    if (modal) modal.classList.remove('active');
  },

  async saveFile() {
    const nameInput = document.getElementById('file-name');
    const contentInput = document.getElementById('file-content');
    const name = nameInput ? nameInput.value.trim() : '';
    const content = contentInput ? contentInput.value : '';

    if (!name) {
      window.app.showToast('Please enter a valid filename', 'warning');
      return;
    }
    if (!this.validateName(name)) {
      window.app.showToast('Filename: use 1-128 alphanumeric characters, dots, underscores, or dashes.', 'warning');
      return;
    }

    try {
      if (this.isEditing) {
        await window.api.writeFile(name, content, this.currentDirectoryId);
        window.app.showToast(`File '${name}' updated successfully!`, 'success');
      } else {
        await window.api.createFile(name, content, this.currentDirectoryId);
        window.app.showToast(`File '${name}' created and allocated to storage!`, 'success');
      }

      this.closeModal();
      this.refresh();
      if (window.dashboardModule) window.dashboardModule.refresh();
      if (window.blocksModule) window.blocksModule.refresh();
    } catch (err) {
      window.app.showToast(err.message, 'error');
    }
  },

  async viewFile(name) {
    try {
      const f = await window.api.readFile(name, this.currentDirectoryId);
      const modal = document.getElementById('file-view-modal');
      const title = document.getElementById('view-modal-title');
      const meta = document.getElementById('view-file-meta');
      const content = document.getElementById('view-file-content');

      if (title) title.textContent = `File Inspector: ${f.name}`;
      if (meta) {
        const blks = (f.block_ids && f.block_ids.length > 0) ? f.block_ids.join(', ') : 'None';
        meta.innerHTML = `
          <strong>Size:</strong> ${(f.size || 0).toLocaleString()} bytes &bull; 
          <strong>Allocated Blocks:</strong> ${f.block_count || 1} &bull; 
          <strong>Sector IDs:</strong> <code>[ ${blks} ]</code>
        `;
      }
      if (content) {
        content.textContent = f.content || '(File is empty)';
      }
      if (modal) modal.classList.add('active');
    } catch (err) {
      window.app.showToast('Could not read file: ' + err.message, 'error');
    }
  },

  closeViewModal() {
    const modal = document.getElementById('file-view-modal');
    if (modal) modal.classList.remove('active');
  },

  async deleteFile(name) {
    if (!confirm(`Are you sure you want to delete '${name}' and reclaim its storage blocks?`)) {
      return;
    }

    try {
      await window.api.deleteFile(name, this.currentDirectoryId);
      window.app.showToast(`File '${name}' deleted and blocks reclaimed.`, 'info');
      this.refresh();
      if (window.dashboardModule) window.dashboardModule.refresh();
      if (window.blocksModule) window.blocksModule.refresh();
    } catch (err) {
      window.app.showToast(err.message, 'error');
    }
  },

  toggleDirectoryCreate(show) {
    const row = document.getElementById('directory-create-row');
    const input = document.getElementById('directory-name');
    if (row) row.style.display = show ? 'flex' : 'none';
    if (show && input) {
      input.value = '';
      input.focus();
    }
  },

  async createDirectory() {
    const input = document.getElementById('directory-name');
    const name = input ? input.value.trim() : '';
    if (!this.validateName(name)) {
      window.app.showToast('Directory name: use 1-128 alphanumeric characters, dots, underscores, or dashes.', 'warning');
      return;
    }
    try {
      await window.api.createDirectory(name, this.currentDirectoryId);
      window.app.showToast(`Folder '${name}' created.`, 'success');
      this.toggleDirectoryCreate(false);
      await this.refresh();
    } catch (err) {
      window.app.showToast(err.message, 'error');
    }
  },

  openDirectory(directoryId, name) {
    this.directoryStack.push({ id: directoryId, name });
    this.currentDirectoryId = directoryId;
    this.refresh();
  },

  navigateToDirectory(directoryId) {
    const index = this.directoryStack.findIndex(item => item.id === directoryId);
    if (index >= 0) this.directoryStack = this.directoryStack.slice(0, index + 1);
    this.currentDirectoryId = directoryId;
    this.refresh();
  },

  renderBreadcrumb() {
    const breadcrumb = document.getElementById('file-breadcrumb');
    if (!breadcrumb) return;
    breadcrumb.innerHTML = this.directoryStack.map((item, index) => `
      <button class="file-breadcrumb-link ${index === this.directoryStack.length - 1 ? 'active' : ''}"
        onclick="window.filesModule.navigateToDirectory(${item.id})">${this.escape(item.name)}</button>
      ${index < this.directoryStack.length - 1 ? '<span>/</span>' : ''}
    `).join('');
  },

  async deleteDirectory(directoryId, name) {
    if (!confirm(`Delete folder '${name}'? It must be empty.`)) return;
    try {
      await window.api.deleteDirectory(directoryId);
      window.app.showToast(`Folder '${name}' deleted.`, 'info');
      await this.refresh();
    } catch (err) {
      window.app.showToast(err.message, 'error');
    }
  },

  validateName(name) {
    return name.length > 0 && name.length <= 128 && name !== '.' && name !== '..' &&
      /^[A-Za-z0-9._-]+$/.test(name);
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

window.Files = window.filesModule;
