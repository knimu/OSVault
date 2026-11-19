/**
 * OSVault - Dynamic Memory Manager Module
 */

window.memoryModule = {
  currentStrategy: 'FIRST_FIT',
  actionHistory: [],

  init() {
    this.bindEvents();
    this.refresh();
  },

  bindEvents() {
    const btnAlloc = document.getElementById('btn-mem-allocate');
    if (btnAlloc) {
      btnAlloc.onclick = () => this.allocate();
    }

    const btnReset = document.getElementById('btn-mem-reset');
    if (btnReset) {
      btnReset.onclick = () => this.resetMemory();
    }

    const btnStrategy = document.getElementById('btn-apply-mem-strategy');
    if (btnStrategy) {
      btnStrategy.onclick = () => {
        const sel = document.getElementById('mem-strategy');
        this.currentStrategy = sel ? sel.value : 'FIRST_FIT';
        window.app.showToast(`Memory allocation strategy set to ${this.currentStrategy}`, 'info');
      };
    }
  },

  async refresh() {
    try {
      const res = await window.api.getMemoryState();
      const st = res.state;
      if (!st) return;

      const usedEl = document.getElementById('mem-used-stat');
      const freeEl = document.getElementById('mem-free-stat');
      const fragEl = document.getElementById('mem-ext-frag-stat');

      if (usedEl) usedEl.textContent = `${st.allocated_memory || 0} KB`;
      if (freeEl) freeEl.textContent = `${st.free_memory || st.total_memory || 1024} KB`;
      if (fragEl) fragEl.textContent = `${st.external_fragmentation || 0} KB`;
      const utilEl = document.getElementById('mem-util-stat');
      if (utilEl) {
        const utilization = st.total_memory > 0 ? (st.allocated_memory / st.total_memory) * 100 : 0;
        utilEl.textContent = `${utilization.toFixed(1)}%`;
      }
      const freeHolesEl = document.getElementById('mem-free-holes-stat');
      if (freeHolesEl) {
        freeHolesEl.textContent = (st.partitions || []).filter(p => !p.is_allocated).length;
      }
      const intFragEl = document.getElementById('mem-int-frag-stat');
      if (intFragEl) intFragEl.textContent = `${st.total_internal_fragmentation || 0} KB`;

      // Render physical memory bar
      const bar = document.getElementById('memory-map-container');
      if (bar) {
        bar.innerHTML = '';
        const total = st.total_memory || 1024;

        (st.partitions || []).forEach(p => {
          const pct = Math.max(2, (p.size / total) * 100);
          const seg = document.createElement('div');
          seg.className = `memory-partition-segment ${p.is_allocated ? 'allocated' : 'free'}`;
          seg.style.width = `${pct}%`;
          seg.title = `Partition #${p.id}: [${p.start_address}..${p.start_address + p.size - 1}] (${p.size} KB) - ${p.is_allocated ? p.process_name : 'FREE'}`;
          seg.innerHTML = `<span>${p.is_allocated ? p.process_name : 'FREE'}</span><span class="small font-mono">${p.size}K</span>`;
          bar.appendChild(seg);
        });
      }

      // Render table
      const tbody = document.getElementById('memory-blocks-body');
      if (tbody) {
        if (!st.partitions || st.partitions.length === 0) {
          tbody.innerHTML = '<tr><td colspan="6" class="text-center text-muted p-3">Memory fully free (1024 KB).</td></tr>';
          return;
        }

        tbody.innerHTML = st.partitions.map(p => `
          <tr>
            <td class="font-mono text-muted small">#${p.id}</td>
            <td><strong>${p.is_allocated ? this.escape(p.process_name) : '<span class="text-muted">(Free Hole)</span>'}</strong></td>
            <td class="font-mono">0x${(p.start_address || 0).toString(16).toUpperCase().padStart(4, '0')} (${p.start_address} KB)</td>
            <td class="font-mono">${p.size} KB</td>
            <td>
              ${p.is_allocated ? '<span class="badge badge-success">ALLOCATED</span>' : '<span class="badge badge-secondary">FREE</span>'}
            </td>
            <td>
              ${p.is_allocated ? `<button class="btn btn-danger btn-sm" onclick="window.memoryModule.deallocate(${p.id})">Deallocate</button>` : '-'}
            </td>
          </tr>
        `).join('');
      }

      const history = document.getElementById('memory-action-history');
      if (history) {
        history.innerHTML = this.actionHistory.length
          ? this.actionHistory.map(item => `<div>${this.escape(item)}</div>`).join('')
          : 'No allocation actions yet.';
      }
    } catch (err) {
      console.error('Failed to load memory state:', err);
    }
  },

  async allocate() {
    const pidInput = document.getElementById('mem-req-pid');
    const sizeInput = document.getElementById('mem-req-size');
    const selStrategy = document.getElementById('mem-strategy');

    const procName = pidInput ? pidInput.value.trim() : 'Proc_A';
    const size = sizeInput ? parseInt(sizeInput.value, 10) || 64 : 64;
    const strategy = selStrategy ? selStrategy.value : this.currentStrategy;

    if (!procName) {
      window.app.showToast('Please enter a process name or tag', 'warning');
      return;
    }
    if (!/^[A-Za-z0-9_-]{1,32}$/.test(procName)) {
      window.app.showToast('Process name must be 1-32 letters, digits, underscores or dashes.', 'warning');
      return;
    }
    if (!Number.isInteger(size) || size <= 0) {
      window.app.showToast('Allocation size must be greater than zero.', 'warning');
      return;
    }
    if (size > 1024) {
      window.app.showToast('Allocation size cannot exceed 1024 KB.', 'warning');
      return;
    }

    try {
      const result = await window.api.allocateMemory(procName, size, strategy);
      this.actionHistory.unshift(`${result.state.message || 'Allocation completed'} — ${new Date().toLocaleTimeString()}`);
      this.actionHistory = this.actionHistory.slice(0, 8);

      window.app.showToast(`Allocated ${size} KB for ${procName} (${strategy})`, 'success');
      this.refresh();
      if (pidInput) {
        const nextChar = String.fromCharCode(procName.charCodeAt(procName.length - 1) + 1);
        if (/[A-Z]/.test(nextChar)) {
          pidInput.value = procName.slice(0, -1) + nextChar;
        }
      }
    } catch (err) {
      window.app.showToast('Allocation failed: ' + err.message, 'error');
    }
  },

  async deallocate(partitionId) {
    try {
      const result = await window.api.deallocateMemory(partitionId);
      this.actionHistory.unshift(`${result.state.message || `Partition #${partitionId} deallocated`} — ${new Date().toLocaleTimeString()}`);
      this.actionHistory = this.actionHistory.slice(0, 8);
      window.app.showToast(`Partition #${partitionId} deallocated and coalesced.`, 'info');
      this.refresh();
    } catch (err) {
      window.app.showToast('Deallocation failed: ' + err.message, 'error');
    }
  },

  async resetMemory() {
    if (!confirm('Reset dynamic memory to one contiguous 1024 KB free block?')) return;
    try {
      await window.api.resetMemory();
      this.actionHistory.unshift(`Memory reset to one 1024 KB free block — ${new Date().toLocaleTimeString()}`);
      this.actionHistory = this.actionHistory.slice(0, 8);
      window.app.showToast('Physical memory reset to contiguous 1024 KB free space.', 'info');
      this.refresh();
    } catch (err) {
      window.app.showToast(err.message, 'error');
    }
  },

  escape(s) {
    if (!s) return '';
    return String(s).replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;');
  }
};

window.Memory = window.memoryModule;
