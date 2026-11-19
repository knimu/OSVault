/**
 * OSVault - 1024-Block Storage Visualizer Module
 */

window.blocksModule = {
  blocksData: [],
  selectedBlockId: null,
  demoFile: null,

  init() {
    this.bindEvents();
    this.refresh();
  },

  bindEvents() {
    const btnStrategy = document.getElementById('btn-apply-strategy');
    if (btnStrategy) {
      btnStrategy.onclick = () => this.applyStrategy();
    }
    const btnDemo = document.getElementById('btn-block-demo');
    if (btnDemo) {
      btnDemo.onclick = () => this.createDemoFile();
    }
    const btnDeleteDemo = document.getElementById('btn-block-demo-delete');
    if (btnDeleteDemo) {
      btnDeleteDemo.onclick = () => this.deleteDemoFile();
    }
  },

  async refresh() {
    try {
      const res = await window.api.getBlocks();
      this.blocksData = res.blocks || [];
      const sel = document.getElementById('block-strategy-select');
      if (sel && res.strategy) sel.value = res.strategy;
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
        checksum.textContent = b.is_allocated ? `${b.payload_size || 0} bytes in this block` : 'N/A';
      }
      if (dataPreview) {
        dataPreview.textContent = b.is_allocated ? (b.payload || '(Empty payload)') : '(Free sector - no payload stored)';
      }
    } catch (err) {
      window.app.showToast('Could not load block details: ' + err.message, 'error');
    }
  },

  async applyStrategy() {
    const sel = document.getElementById('block-strategy-select');
    const strategy = sel ? sel.value : 'FIRST_FIT';
    try {
      await window.api.setBlockStrategy(strategy);
      window.app.showToast(`Allocation strategy switched to ${strategy}`, 'success');
    } catch (err) {
      window.app.showToast('Could not set strategy: ' + err.message, 'error');
    }
  },

  async createDemoFile() {
    try {
      const name = `block_demo_${Date.now()}.txt`;
      const content = 'OSVault 4096-byte logical block demonstration. '.repeat(95);
      const created = await window.api.createFile(name, content);
      this.demoFile = {
        name,
        size: created.size,
        blockCount: created.block_count,
        blockIds: created.block_ids || []
      };
      this.renderDemoDetails();
      await this.refresh();
      if (window.filesModule) window.filesModule.refresh();
      if (window.dashboardModule) window.dashboardModule.refresh();
      const blockIds = created.block_ids || [];
      if (blockIds.length > 0) await this.inspectBlock(blockIds[0]);
      window.app.showToast(`Created ${name} and mapped ${blockIds.length} logical blocks.`, 'success');
    } catch (err) {
      window.app.showToast('Demo file creation failed: ' + err.message, 'error');
    }
  },

  renderDemoDetails() {
    const details = document.getElementById('block-demo-details');
    const deleteButton = document.getElementById('btn-block-demo-delete');
    if (!this.demoFile) {
      if (details) details.style.display = 'none';
      if (deleteButton) deleteButton.disabled = true;
      return;
    }

    if (details) details.style.display = 'block';
    if (deleteButton) deleteButton.disabled = false;

    const name = document.getElementById('block-demo-name');
    const size = document.getElementById('block-demo-size');
    const count = document.getElementById('block-demo-count');
    const ids = document.getElementById('block-demo-ids');
    const explanation = document.getElementById('block-demo-explanation');

    if (name) name.textContent = this.demoFile.name;
    if (size) size.textContent = `${this.demoFile.size} bytes`;
    if (count) count.textContent = `${this.demoFile.blockCount}`;
    if (ids) ids.textContent = this.demoFile.blockIds.length ? this.demoFile.blockIds.join(', ') : 'None';
    if (explanation) {
      explanation.textContent =
        `The file uses ${this.demoFile.blockCount} block${this.demoFile.blockCount === 1 ? '' : 's'} because OSVault allocates logical storage in 4096-byte chunks. ` +
        `ceil(${this.demoFile.size} bytes / 4096 bytes per block) = ${this.demoFile.blockCount} block${this.demoFile.blockCount === 1 ? '' : 's'}.`;
    }
  },

  async deleteDemoFile() {
    if (!this.demoFile) return;
    const name = this.demoFile.name;
    const oldBlockIds = [...this.demoFile.blockIds];
    try {
      await window.api.deleteFile(name, 0);
      this.demoFile = null;
      this.selectedBlockId = null;
      this.renderDemoDetails();
      await this.refresh();
      if (window.filesModule) window.filesModule.refresh();
      if (window.dashboardModule) window.dashboardModule.refresh();

      const emptyMsg = document.getElementById('inspector-empty');
      const content = document.getElementById('inspector-content');
      if (emptyMsg) emptyMsg.style.display = 'block';
      if (content) content.style.display = 'none';

      window.app.showToast(
        `Deleted ${name}; freed ${oldBlockIds.length} block${oldBlockIds.length === 1 ? '' : 's'}: ${oldBlockIds.join(', ') || 'none'}.`,
        'success'
      );
    } catch (err) {
      window.app.showToast('Demo file deletion failed: ' + err.message, 'error');
    }
  }
};

window.Blocks = window.blocksModule;
