/**
 * OSVault - Distributed Logical Clocks (Lamport & Vector Clocks)
 * Visualizes distributed timestamps, message passing causal dependencies, and concurrency checks.
 */

const clocksModule = {
  init() {
    this.bindEvents();
    this.refresh();
  },

  bindEvents() {
    // Local event trigger
    const btnLocal = document.getElementById('btn-clock-local-event');
    if (btnLocal) {
      btnLocal.addEventListener('click', () => this.triggerLocalEvent());
    }

    // Message send
    const btnSend = document.getElementById('btn-clock-send');
    if (btnSend) {
      btnSend.addEventListener('click', () => this.sendMessage());
    }

    // Reset clocks
    const btnReset = document.getElementById('btn-clock-reset');
    if (btnReset) {
      btnReset.addEventListener('click', () => this.resetClocks());
    }

    // Concurrency check
    const btnCheck = document.getElementById('btn-concurrency-check');
    if (btnCheck) {
      btnCheck.addEventListener('click', () => this.checkConcurrency());
    }
  },

  async refresh() {
    try {
      const [nodesRes, timelineRes] = await Promise.all([
        api.getClockNodes(),
        api.getClockTimeline()
      ]);

      if (nodesRes.success) {
        this.renderNodes(nodesRes.data.nodes || []);
      }

      if (timelineRes.success) {
        this.renderTimeline(timelineRes.data.events || []);
      }
    } catch (e) {
      console.error('Error refreshing clocks:', e);
    }
  },

  renderNodes(nodes) {
    const container = document.getElementById('clock-nodes-container');
    if (!container) return;

    if (nodes.length === 0) {
      container.innerHTML = '<div class="text-muted p-3">No active nodes. Click Reset Clocks to initialize.</div>';
      return;
    }

    // Update select dropdowns
    const selLocal = document.getElementById('clock-local-node');
    const selFrom = document.getElementById('clock-from-node');
    const selTo = document.getElementById('clock-to-node');

    if (selLocal) {
      const cur = selLocal.value;
      selLocal.innerHTML = nodes.map(n => `<option value="${n.node_id}">Node ${n.node_id}</option>`).join('');
      if (cur) selLocal.value = cur;
    }
    if (selFrom) {
      const cur = selFrom.value;
      selFrom.innerHTML = nodes.map(n => `<option value="${n.node_id}">Node ${n.node_id}</option>`).join('');
      if (cur) selFrom.value = cur;
    }
    if (selTo) {
      const cur = selTo.value;
      selTo.innerHTML = nodes.map(n => `<option value="${n.node_id}">Node ${n.node_id}</option>`).join('');
      if (cur) selTo.value = cur;
      else if (nodes.length > 1) selTo.value = nodes[1].node_id;
    }

    // Render node cards
    container.innerHTML = nodes.map(n => {
      const vectorStr = Array.isArray(n.vector_clock) 
        ? `[ ${n.vector_clock.join(', ')} ]` 
        : JSON.stringify(n.vector_clock || []);

      return `
        <div class="clock-node-card">
          <div class="node-header">
            <h4>Node ${n.node_id}</h4>
            <span class="badge badge-primary">Active</span>
          </div>
          <div class="node-clock-metrics">
            <div class="clock-stat">
              <span class="stat-label">Lamport Clock:</span>
              <span class="stat-value font-mono highlight-blue">${n.lamport_clock}</span>
            </div>
            <div class="clock-stat">
              <span class="stat-label">Vector Clock:</span>
              <span class="stat-value font-mono highlight-teal">${vectorStr}</span>
            </div>
            <div class="clock-stat">
              <span class="stat-label">Total Events:</span>
              <span class="stat-value">${n.event_count || 0}</span>
            </div>
          </div>
        </div>
      `;
    }).join('');
  },

  renderTimeline(events) {
    const list = document.getElementById('clock-timeline-list');
    if (!list) return;

    if (events.length === 0) {
      list.innerHTML = '<div class="text-muted p-3 text-center">No clock events recorded yet. Trigger an event or send a message.</div>';
      return;
    }

    list.innerHTML = events.slice(-20).reverse().map(ev => {
      const vecStr = Array.isArray(ev.vector_clock) ? `[${ev.vector_clock.join(', ')}]` : '';
      let badgeClass = 'badge-primary';
      if (ev.type === 'LOCAL') badgeClass = 'badge-secondary';
      else if (ev.type === 'SEND') badgeClass = 'badge-warning';
      else if (ev.type === 'RECEIVE') badgeClass = 'badge-success';

      return `
        <div class="clock-event-item">
          <div class="event-meta">
            <span class="badge ${badgeClass}">${ev.type}</span>
            <span class="font-mono">Node ${ev.node_id}</span>
            <span class="text-muted small">${new Date(ev.timestamp * 1000).toLocaleTimeString()}</span>
          </div>
          <div class="event-desc">
            ${this.escape(ev.description)}
          </div>
          <div class="event-timestamps">
            <span class="font-mono small badge badge-dark">L=${ev.lamport_clock}</span>
            <span class="font-mono small badge badge-dark">V=${vecStr}</span>
          </div>
        </div>
      `;
    }).join('');
  },

  async triggerLocalEvent() {
    const nodeSelect = document.getElementById('clock-local-node');
    const descInput = document.getElementById('clock-local-desc');
    const nodeId = parseInt(nodeSelect ? nodeSelect.value : 0, 10);
    const desc = (descInput && descInput.value) ? descInput.value : `Internal Computation on N${nodeId}`;

    try {
      const res = await api.triggerLocalEvent(nodeId, desc);
      if (res.success) {
        app.showToast(`Node ${nodeId} local event triggered (L=${res.data.lamport_clock})`, 'success');
        if (descInput) descInput.value = '';
        this.refresh();
      } else {
        app.showToast(res.error || 'Failed to trigger event', 'error');
      }
    } catch (e) {
      app.showToast(e.message, 'error');
    }
  },

  async sendMessage() {
    const fromSel = document.getElementById('clock-from-node');
    const toSel = document.getElementById('clock-to-node');
    const msgInput = document.getElementById('clock-send-msg');

    const fromNode = parseInt(fromSel ? fromSel.value : 0, 10);
    const toNode = parseInt(toSel ? toSel.value : 1, 10);
    const msg = (msgInput && msgInput.value) ? msgInput.value : `Hello from Node ${fromNode}`;

    if (fromNode === toNode) {
      app.showToast('Sender and Receiver nodes must be different', 'warning');
      return;
    }

    try {
      const res = await api.sendClockMessage(fromNode, toNode, msg);
      if (res.success) {
        app.showToast(`Message sent from N${fromNode} to N${toNode}`, 'success');
        if (msgInput) msgInput.value = '';
        this.refresh();
      } else {
        app.showToast(res.error || 'Failed to send message', 'error');
      }
    } catch (e) {
      app.showToast(e.message, 'error');
    }
  },

  async resetClocks() {
    try {
      const res = await api.resetClocks(3);
      if (res.success) {
        app.showToast('Logical Clocks reset to initial state', 'info');
        this.refresh();
      }
    } catch (e) {
      app.showToast(e.message, 'error');
    }
  },

  async checkConcurrency() {
    const v1Input = document.getElementById('concurrency-v1');
    const v2Input = document.getElementById('concurrency-v2');
    const resultBox = document.getElementById('concurrency-result');

    if (!v1Input || !v2Input || !resultBox) return;

    try {
      const v1 = JSON.parse(v1Input.value);
      const v2 = JSON.parse(v2Input.value);

      const res = await api.checkConcurrency(v1, v2);
      if (res.success) {
        const relation = res.data.relation;
        let color = '#3b82f6';
        let explanation = '';

        if (relation === 'HAPPENS_BEFORE (V1 -> V2)') {
          color = '#10b981';
          explanation = 'V1 causally preceded V2 (all elements <= and at least one strictly <).';
        } else if (relation === 'HAPPENS_AFTER (V2 -> V1)') {
          color = '#f59e0b';
          explanation = 'V2 causally preceded V1.';
        } else if (relation === 'CONCURRENT (V1 || V2)') {
          color = '#ef4444';
          explanation = 'V1 and V2 are concurrent! Neither causally happened before the other in distributed time.';
        } else {
          color = '#94a3b8';
          explanation = 'Identical clocks.';
        }

        resultBox.innerHTML = `
          <div class="p-3 rounded" style="background: rgba(15, 23, 42, 0.6); border-left: 4px solid ${color};">
            <h4 style="color: ${color}; margin: 0 0 0.5rem 0;">Result: ${relation}</h4>
            <p class="small text-muted" style="margin: 0;">${explanation}</p>
          </div>
        `;
      } else {
        resultBox.textContent = res.error || 'Check failed';
      }
    } catch (e) {
      resultBox.textContent = 'Invalid JSON vectors! Format must be: [1, 2, 0]';
    }
  },

  escape(s) {
    if (!s) return '';
    return String(s).replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;');
  }
};
