import { API } from './api.js';

export const Blocks = {
  blocksData: [],
  selectedBlockId: null,
  currentFilter: 'all', // 'all', 'allocated', 'free'

  async init() {
    this.bindEvents();
    await this.refresh();
  },

  bindEvents() {
    const filterSelect = document.getElementById('block-filter-select');
    if (filterSelect) {
      filterSelect.addEventListener('change', (e) => {
        this.currentFilter = e.target.value;
        this.renderGrid();
      });
    }

    const btnRefresh = document.getElementById('btn-refresh-blocks');
    if (btnRefresh) {
      btnRefresh.addEventListener('click', () => this.refresh());
    }
  },

  async refresh() {
    try {
      const data = await API.getBlocks();
      this.blocksData = data.blocks || [];
      this.renderGrid();
      this.updateSummary();
    } catch (err) {
      console.error('Failed to load blocks:', err);
    }
  },

  updateSummary() {
    const total = this.blocksData.length;
    const allocated = this.blocksData.filter(b => b.is_allocated).length;
    const free = total - allocated;

    const summaryEl = document.getElementById('blocks-summary-text');
    if (summaryEl) {
      summaryEl.textContent = `Showing 1,024 blocks (4 KiB per block = 4 MiB logical capacity). Allocated: ${allocated}, Free: ${free}`;
    }
  },

  renderGrid() {
    const gridContainer = document.getElementById('block-grid');
    if (!gridContainer) return;

    gridContainer.innerHTML = '';

    for (const b of this.blocksData) {
      if (this.currentFilter === 'allocated' && !b.is_allocated) continue;
      if (this.currentFilter === 'free' && b.is_allocated) continue;

      const cell = document.createElement('div');
      cell.className = `block-cell ${b.is_allocated ? 'allocated' : 'free'}`;
      if (this.selectedBlockId === b.block_id) cell.classList.add('selected');

      cell.title = `Block #${b.block_id} | ${b.is_allocated ? 'Allocated to ' + b.owner_file_name : 'Free'}`;
      cell.addEventListener('click', () => this.inspectBlock(b));
      gridContainer.appendChild(cell);
    }
  },

  inspectBlock(b) {
    this.selectedBlockId = b.block_id;
    this.renderGrid();

    const infoPanel = document.getElementById('block-inspector-details');
    if (!infoPanel) return;

    infoPanel.innerHTML = `
      <div style="font-size: 0.85rem; line-height: 1.6;">
        <p><strong>Logical Block ID:</strong> <span style="font-family: var(--font-mono); color: var(--accent-blue);">#${b.block_id}</span></p>
        <p><strong>Physical State:</strong> <span class="status-badge ${b.is_allocated ? '' : 'error'}">${b.is_allocated ? 'ALLOCATED' : 'FREE'}</span></p>
        <p><strong>Owner File Name:</strong> ${b.owner_file_name ? '<span style="color:var(--accent-teal);">' + b.owner_file_name + '</span>' : '<em>None</em>'}</p>
        <p><strong>Owner File ID:</strong> ${b.owner_file_id || '<em>None</em>'}</p>
        <p><strong>Index Within File:</strong> ${b.block_index_in_file >= 0 ? b.block_index_in_file : '<em>None</em>'}</p>
        <p><strong>Block Size:</strong> 4,096 Bytes</p>
      </div>
    `;
  }
};
