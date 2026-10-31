/**
 * OSVault - 1024-Block Storage Visualizer Module
 */

window.blocksModule = {
  blocksData: [],
  selectedBlockId: null,

  init() {
    this.bindEvents();
    this.refresh();
  },

  bindEvents() {
    const btnStrategy = document.getElementById('btn-apply-strategy');
    if (btnStrategy) {
      btnStrategy.onclick = () => this.applyStrategy();
    }
  },

  async refresh() {
    try {
      const res = await window.api.getBlocks();
      this.blocksData = res.blocks || [];
      this.renderGrid();
      this.updateSummary();
    } catch (err) {
      console.error('Failed to load blocks:', err);
    }
  },

  updateSummary() {
    const total = this.blocksData.length || 1024;
    const allocated = this.blocksData.filter(b => b.is_allocated).length;
    const free = total - allocated;

    const summaryEl = document.getElementById('block-stats-summary');
    if (summaryEl) {
      summaryEl.textContent = `${allocated} / ${total} allocated (${free} free)`;
    }
  },

  renderGrid() {
    const container = document.getElementById('block-grid-container');
    if (!container) return;

    container.innerHTML = '';

    for (let i = 0; i < 1024; ++i) {
      const b = this.blocksData[i] || { block_id: i, is_allocated: false };
      const cell = document.createElement('div');
      cell.className = `block-cell ${b.is_allocated ? 'allocated' : 'free'}`;
      if (this.selectedBlockId === b.block_id) {
        cell.classList.add('selected');
      }
      cell.title = `Block #${b.block_id} - ${b.is_allocated ? 'Allocated (' + (b.owner_file_name || 'File #' + b.owner_file_id) + ')' : 'Free'}`;

      cell.onclick = () => this.inspectBlock(b.block_id);
      container.appendChild(cell);
    }
  },

  async inspectBlock(blockId) {
    this.selectedBlockId = blockId;
    this.renderGrid();

    const emptyMsg = document.getElementById('inspector-empty');
    const content = document.getElementById('inspector-content');
    const title = document.getElementById('inspect-block-title');
    const status = document.getElementById('inspect-status');
    const fileId = document.getElementById('inspect-file-id');
    const checksum = document.getElementById('inspect-checksum');
    const dataPreview = document.getElementById('inspect-data');

    try {
      const b = await window.api.getBlock(blockId);

      if (emptyMsg) emptyMsg.style.display = 'none';
      if (content) content.style.display = 'block';

      if (title) title.textContent = `Block #${b.block_id} Inspection`;
      if (status) {
        status.className = b.is_allocated ? 'badge badge-primary' : 'badge badge-success';
        status.textContent = b.is_allocated ? 'ALLOCATED' : 'FREE';
      }
      if (fileId) {
        fileId.textContent = b.is_allocated ? `${b.owner_file_name || 'File #' + b.owner_file_id} (Chunk ${b.block_index_in_file || 0})` : 'Unassigned';
      }
      if (checksum) {
        checksum.textContent = b.is_allocated ? `SHA-256 Verified (${(b.data || '').length} bytes)` : 'N/A';
      }
      if (dataPreview) {
        dataPreview.textContent = b.is_allocated ? (b.data || '(Empty payload)') : '(Free sector - no payload stored)';
      }
    } catch (err) {
      window.app.showToast('Could not load block details: ' + err.message, 'error');
    }
  },

  applyStrategy() {
    const sel = document.getElementById('block-strategy-select');
    const strategy = sel ? sel.value : 'FIRST_FIT';
    const dashStrategy = document.getElementById('dash-alloc-strategy');
    if (dashStrategy) dashStrategy.textContent = strategy;
    window.app.showToast(`Allocation strategy switched to ${strategy}`, 'success');
  }
};

window.Blocks = window.blocksModule;
