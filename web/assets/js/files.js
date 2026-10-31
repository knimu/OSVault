import { API } from './api.js';
import { Dashboard } from './dashboard.js';
import { Blocks } from './blocks.js';

export const Files = {
  activeFile: null,

  async init() {
    this.bindEvents();
    await this.refresh();
  },

  bindEvents() {
    const searchInput = document.getElementById('files-search-input');
    if (searchInput) {
      searchInput.addEventListener('input', () => this.refresh(searchInput.value));
    }

    const btnNew = document.getElementById('btn-new-file');
    if (btnNew) {
      btnNew.addEventListener('click', () => this.openCreateModal());
    }
  },

  async refresh(query = '') {
    const tbody = document.getElementById('files-table-body');
    if (!tbody) return;

    try {
      const data = await API.listFiles(query);
      if (data.files.length === 0) {
        tbody.innerHTML = `<tr><td colspan="6" style="text-align: center; color: var(--text-dim); padding: 2rem;">No files found in logical block storage.</td></tr>`;
        return;
      }

      tbody.innerHTML = data.files.map(f => `
        <tr>
          <td><strong style="color: var(--accent-blue);">${this.escape(f.name)}</strong></td>
          <td>${f.size.toLocaleString()} B</td>
          <td><span class="status-badge" style="background:#1e3a5f; color:#93c5fd; border-color:#3b82f6;">${f.block_count} blk</span></td>
          <td style="font-family: var(--font-mono); font-size: 0.75rem; color: var(--text-muted);">${f.block_ids.slice(0, 8).join(', ')}${f.block_ids.length > 8 ? '...' : ''}</td>
          <td style="color: var(--text-dim); font-size: 0.75rem;">${f.updated_at}</td>
          <td>
            <div style="display: flex; gap: 4px;">
              <button class="btn btn-secondary btn-sm" onclick="window.Files.viewFile('${this.escape(f.name)}')">View</button>
              <button class="btn btn-secondary btn-sm" onclick="window.Files.editFile('${this.escape(f.name)}')">Edit</button>
              <button class="btn btn-secondary btn-sm" onclick="window.Files.renameFile('${this.escape(f.name)}')">Rename</button>
              <button class="btn btn-danger btn-sm" onclick="window.Files.deleteFile('${this.escape(f.name)}')">Delete</button>
            </div>
          </td>
        </tr>
      `).join('');
    } catch (err) {
      tbody.innerHTML = `<tr><td colspan="6" style="color: var(--status-danger); text-align: center;">Error loading files: ${err.message}</td></tr>`;
    }
  },

  openCreateModal() {
    const name = prompt('Enter new filename (e.g., sample.txt):');
    if (!name) return;
    const content = prompt('Enter initial file content (optional):') || '';

    API.createFile(name, content)
      .then(() => {
        window.App.toast(`File '${name}' created successfully.`, 'success');
        this.refresh();
        Dashboard.refresh();
        Blocks.refresh();
      })
      .catch(err => window.App.toast(`Failed to create file: ${err.message}`, 'error'));
  },

  async viewFile(name) {
    try {
      const f = await API.readFile(name);
      alert(`[FILE: ${f.name}]\nSize: ${f.size} bytes (${f.block_count} blocks)\nBlock IDs: [${f.block_ids.join(', ')}]\n\n--- Content ---\n${f.content}`);
    } catch (err) {
      window.App.toast(`Read failed: ${err.message}`, 'error');
    }
  },

  async editFile(name) {
    try {
      const f = await API.readFile(name);
      const newContent = prompt(`Edit content for '${f.name}':`, f.content);
      if (newContent === null) return;

      await API.writeFile(name, newContent);
      window.App.toast(`File '${name}' updated successfully.`, 'success');
      this.refresh();
      Dashboard.refresh();
      Blocks.refresh();
    } catch (err) {
      window.App.toast(`Write failed: ${err.message}`, 'error');
    }
  },

  async renameFile(oldName) {
    const newName = prompt(`Enter new name for '${oldName}':`, oldName);
    if (!newName || newName === oldName) return;

    try {
      await API.renameFile(oldName, newName);
      window.App.toast(`Renamed '${oldName}' to '${newName}'.`, 'success');
      this.refresh();
      Dashboard.refresh();
      Blocks.refresh();
    } catch (err) {
      window.App.toast(`Rename failed: ${err.message}`, 'error');
    }
  },

  async deleteFile(name) {
    if (!confirm(`Are you sure you want to delete file '${name}'? All logical blocks will be freed.`)) return;

    try {
      await API.deleteFile(name);
      window.App.toast(`File '${name}' deleted.`, 'success');
      this.refresh();
      Dashboard.refresh();
      Blocks.refresh();
    } catch (err) {
      window.App.toast(`Delete failed: ${err.message}`, 'error');
    }
  },

  escape(str) {
    return (str || '').replace(/[&<>'"]/g, tag => ({
      '&': '&amp;', '<': '&lt;', '>': '&gt;', "'": '&#39;', '"': '&quot;'
    }[tag] || tag));
  }
};
