import { API } from './api.js';

export const Scheduler = {
  processes: [
    { pid: 'P1', arrival_time: 0, burst_time: 5, priority: 2 },
    { pid: 'P2', arrival_time: 1, burst_time: 3, priority: 1 },
    { pid: 'P3', arrival_time: 2, burst_time: 8, priority: 4 },
    { pid: 'P4', arrival_time: 3, burst_time: 6, priority: 3 }
  ],

  init() {
    this.bindEvents();
    this.renderProcessTable();
  },

  bindEvents() {
    const btnAdd = document.getElementById('btn-add-process');
    if (btnAdd) {
      btnAdd.addEventListener('click', () => {
        const nextId = 'P' + (this.processes.length + 1);
        this.processes.push({ pid: nextId, arrival_time: 0, burst_time: 4, priority: 1 });
        this.renderProcessTable();
      });
    }

    const btnSim = document.getElementById('btn-run-scheduler');
    if (btnSim) {
      btnSim.addEventListener('click', () => this.runSimulation());
    }
  },

  renderProcessTable() {
    const tbody = document.getElementById('scheduler-input-table-body');
    if (!tbody) return;

    tbody.innerHTML = this.processes.map((p, idx) => `
      <tr>
        <td><input type="text" class="input" style="width:70px;" value="${p.pid}" onchange="window.Scheduler.updateProc(${idx}, 'pid', this.value)"></td>
        <td><input type="number" class="input" style="width:70px;" min="0" value="${p.arrival_time}" onchange="window.Scheduler.updateProc(${idx}, 'arrival_time', parseInt(this.value, 10))"></td>
        <td><input type="number" class="input" style="width:70px;" min="1" value="${p.burst_time}" onchange="window.Scheduler.updateProc(${idx}, 'burst_time', parseInt(this.value, 10))"></td>
        <td><input type="number" class="input" style="width:70px;" value="${p.priority}" onchange="window.Scheduler.updateProc(${idx}, 'priority', parseInt(this.value, 10))"></td>
        <td>
          <button class="btn btn-danger btn-sm" onclick="window.Scheduler.removeProc(${idx})">&times;</button>
        </td>
      </tr>
    `).join('');
  },

  updateProc(idx, field, val) {
    if (this.processes[idx]) {
      this.processes[idx][field] = val;
    }
  },

  removeProc(idx) {
    if (this.processes.length <= 1) {
      window.App.toast('Must have at least one process.', 'warning');
      return;
    }
    this.processes.splice(idx, 1);
    this.renderProcessTable();
  },

  async runSimulation() {
    const algo = document.getElementById('scheduler-algo-select').value;
    const quantum = parseInt(document.getElementById('scheduler-quantum-input').value, 10) || 2;

    try {
      const data = await API.simulateScheduler(this.processes, quantum, algo);
      const outContainer = document.getElementById('scheduler-results-container');

      if (algo === 'ALL' && data.comparison) {
        outContainer.innerHTML = data.comparison.map(r => this.renderSingleResult(r)).join('<hr style="border:0; border-top:1px solid var(--border-color); margin: 1.5rem 0;">');
      } else if (data.result) {
        outContainer.innerHTML = this.renderSingleResult(data.result);
      }
      window.App.toast('Scheduling calculation completed.', 'success');
    } catch (err) {
      window.App.toast(`Scheduler error: ${err.message}`, 'error');
    }
  },

  renderSingleResult(r) {
    const colors = ['#3b82f6', '#10b981', '#f59e0b', '#8b5cf6', '#ec4899', '#06b6d4'];

    const totalTime = r.total_time || (r.gantt_chart[r.gantt_chart.length - 1]?.end_time) || 1;

    let ganttBars = r.gantt_chart.map((g, idx) => {
      const duration = g.end_time - g.start_time;
      const pct = (duration / totalTime) * 100;
      const color = g.is_idle ? '#334155' : colors[idx % colors.length];

      return `
        <div class="gantt-bar ${g.is_idle ? 'idle' : ''}" style="width: ${pct}%; background-color: ${color};" title="${g.pid}: [${g.start_time} - ${g.end_time}]">
          ${g.pid}
        </div>
      `;
    }).join('');

    let procRows = r.processes.map(p => `
      <tr>
        <td><strong>${p.pid}</strong></td>
        <td>${p.arrival_time}</td>
        <td>${p.burst_time}</td>
        <td>${p.priority}</td>
        <td>${p.completion_time}</td>
        <td>${p.turnaround_time}</td>
        <td>${p.waiting_time}</td>
        <td>${p.response_time}</td>
      </tr>
    `).join('');

    return `
      <div style="margin-bottom: 1rem;">
        <div style="display:flex; justify-content:space-between; align-items:center;">
          <h4 style="color: var(--text-main);">${r.algorithm} ${r.time_quantum ? '(Quantum: ' + r.time_quantum + ')' : ''}</h4>
          <span style="font-size:0.8rem; color:var(--text-muted);">
            Avg Turnaround: <strong style="color:var(--accent-blue);">${r.avg_turnaround_time.toFixed(2)}</strong> | 
            Avg Waiting: <strong style="color:var(--status-success);">${r.avg_waiting_time.toFixed(2)}</strong> | 
            Idle CPU: <strong>${r.idle_time}</strong>
          </span>
        </div>

        <div class="gantt-chart">
          ${ganttBars}
        </div>

        <div class="table-wrap">
          <table class="data-table" style="font-size:0.75rem;">
            <thead>
              <tr>
                <th>PID</th>
                <th>Arrival</th>
                <th>Burst</th>
                <th>Priority</th>
                <th>Completion</th>
                <th>Turnaround</th>
                <th>Waiting</th>
                <th>Response</th>
              </tr>
            </thead>
            <tbody>
              ${procRows}
            </tbody>
          </table>
        </div>
      </div>
    `;
  }
};
