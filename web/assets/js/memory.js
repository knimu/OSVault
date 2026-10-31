import { API } from './api.js';

export const Memory = {
  async init() {
    this.bindEvents();
    await this.refresh();
  },

  bindEvents() {
    const btnAlloc = document.getElementById('btn-alloc-memory');
    if (btnAlloc) {
      btnAlloc.addEventListener('click', () => this.allocate());
    }

    const btnReset = document.getElementById('btn-reset-memory');
    if (btnReset) {
      btnReset.addEventListener('click', () => this.reset());
    }
  },

  async refresh() {
    try {
      const data = await API.getMemoryState();
      this.renderState(data.state);
    } catch (err) {
      console.error('Failed to load memory state:', err);
    }
  },

  renderState(state) {
    document.getElementById('mem-total').textContent = `${state.total_memory} KB`;
    document.getElementById('mem-allocated').textContent = `${state.allocated_memory} KB`;
    document.getElementById('mem-free').textContent = `${state.free_memory} KB`;
    document.getElementById('mem-ext-frag').textContent = `${state.external_fragmentation} KB`;

    const bar = document.getElementById('memory-map-bar');
    if (!bar) return;

    bar.innerHTML = '';
    state.partitions.forEach(p => {
      const pct = (p.size / state.total_memory) * 100;
      const seg = document.createElement('div');
      seg.className = `memory-partition-segment ${p.is_allocated ? 'allocated' : 'free'}`;
      seg.style.width = `${pct}%`;
      seg.title = `ID: ${p.id} | ${p.is_allocated ? p.process_name + ' (' + p.size + ' KB)' : 'FREE (' + p.size + ' KB)'} [Start: ${p.start_address}]`;
      seg.textContent = p.is_allocated ? `${p.process_name} (${p.size}K)` : `Free (${p.size}K)`;
      bar.appendChild(seg);
    });

    const tbody = document.getElementById('memory-partitions-table-body');
    if (!tbody) return;

    tbody.innerHTML = state.partitions.map(p => `
      <tr>
        <td>#${p.id}</td>
        <td>${p.start_address}</td>
        <td>${p.size} KB</td>
        <td><span class="status-badge ${p.is_allocated ? '' : 'error'}">${p.is_allocated ? 'ALLOCATED' : 'FREE'}</span></td>
        <td>${p.is_allocated ? '<strong>' + p.process_name + '</strong>' : '<em>None</em>'}</td>
        <td>
          ${p.is_allocated ? `<button class="btn btn-danger btn-sm" onclick="window.Memory.deallocate(${p.id})">Free</button>` : '-'}
        </td>
      </tr>
    `).join('');
  },

  async allocate() {
    const proc = document.getElementById('mem-proc-name').value.trim() || 'ProcA';
    const size = parseInt(document.getElementById('mem-proc-size').value, 10);
    const strategy = document.getElementById('mem-strategy-select').value;

    if (isNaN(size) || size <= 0) {
      window.App.toast('Please enter a valid positive size in KB.', 'error');
      return;
    }

    try {
      const data = await API.allocateMemory(proc, size, strategy);
      this.renderState(data.state);
      window.App.toast(data.state.message, 'success');
    } catch (err) {
      window.App.toast(`Allocation failed: ${err.message}`, 'error');
    }
  },

  async deallocate(id) {
    try {
      const data = await API.deallocateMemory(id);
      this.renderState(data.state);
      window.App.toast(data.state.message, 'success');
    } catch (err) {
      window.App.toast(`Deallocation error: ${err.message}`, 'error');
    }
  },

  async reset() {
    const total = parseInt(prompt('Enter total memory size in KB (default: 1024):', '1024'), 10) || 1024;
    try {
      const data = await API.resetMemory(total);
      this.renderState(data.state);
      window.App.toast(data.message, 'success');
    } catch (err) {
      window.App.toast(`Reset error: ${err.message}`, 'error');
    }
  }
};
