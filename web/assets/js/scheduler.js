/**
 * OSVault - CPU Scheduling Simulator Module
 */

window.schedulerModule = {
  result: null,
  stepIndex: 0,
  playbackTimer: null,

  processes: [
    { pid: 'P1', arrival_time: 0, burst_time: 5, priority: 2 },
    { pid: 'P2', arrival_time: 1, burst_time: 3, priority: 1 },
    { pid: 'P3', arrival_time: 2, burst_time: 8, priority: 4 },
    { pid: 'P4', arrival_time: 3, burst_time: 6, priority: 3 }
  ],

  init() {
    this.bindEvents();
    this.renderProcessQueue();
  },

  bindEvents() {
    const btnAdd = document.getElementById('btn-add-proc');
    if (btnAdd) {
      btnAdd.onclick = () => this.addProcess();
    }

    const algoSelect = document.getElementById('sched-algo');
    if (algoSelect) {
      algoSelect.onchange = () => {
        const qGroup = document.getElementById('quantum-group');
        if (qGroup) {
          qGroup.style.display = (algoSelect.value === 'RR') ? 'block' : 'none';
        }
      };
    }

    const btnRun = document.getElementById('btn-run-scheduler');
    if (btnRun) {
      btnRun.onclick = () => this.runSimulation();
    }

    const btnCompare = document.getElementById('btn-compare-all-sched');
    if (btnCompare) {
      btnCompare.onclick = () => this.compareAll();
    }

    const prev = document.getElementById('btn-sched-prev');
    if (prev) prev.onclick = () => this.stopPlayback(() => this.moveStep(-1));

    const next = document.getElementById('btn-sched-next');
    if (next) next.onclick = () => this.stopPlayback(() => this.moveStep(1));

    const play = document.getElementById('btn-sched-play');
    if (play) play.onclick = () => this.togglePlayback();
  },

  renderProcessQueue() {
    const tbody = document.getElementById('proc-queue-body');
    if (!tbody) return;

    if (this.processes.length === 0) {
      tbody.innerHTML = '<tr><td colspan="5" class="text-center text-muted p-2">Queue is empty. Add a process.</td></tr>';
      return;
    }

    tbody.innerHTML = this.processes.map((p, idx) => `
      <tr>
        <td class="font-mono"><strong>${this.escape(p.pid)}</strong></td>
        <td>${p.arrival_time}</td>
        <td>${p.burst_time}</td>
        <td>${p.priority}</td>
        <td>
          <button class="btn btn-danger btn-sm" onclick="window.schedulerModule.removeProcess(${idx})">&times;</button>
        </td>
      </tr>
    `).join('');
  },

  addProcess() {
    const pidInput = document.getElementById('proc-pid');
    const arrInput = document.getElementById('proc-arrival');
    const burstInput = document.getElementById('proc-burst');
    const prioInput = document.getElementById('proc-priority');

    const pid = pidInput ? pidInput.value.trim() : `P${this.processes.length + 1}`;
    const arrival = arrInput ? parseInt(arrInput.value, 10) || 0 : 0;
    const burst = burstInput ? parseInt(burstInput.value, 10) || 1 : 1;
    const priority = prioInput ? parseInt(prioInput.value, 10) || 1 : 1;

    if (!pid) {
      window.app.showToast('Please specify a PID', 'warning');
      return;
    }
    if (!/^[A-Za-z0-9_-]{1,32}$/.test(pid)) {
      window.app.showToast('PID may contain 1-32 letters, digits, underscores or dashes.', 'warning');
      return;
    }
    if (arrival < 0) {
      window.app.showToast('Arrival time cannot be negative.', 'warning');
      return;
    }
    if (burst <= 0) {
      window.app.showToast('Burst time must be greater than zero.', 'warning');
      return;
    }
    if (priority < 1) {
      window.app.showToast('Priority must be at least 1; lower values run first.', 'warning');
      return;
    }
    if (this.processes.some(p => p.pid === pid)) {
      window.app.showToast(`PID '${pid}' already exists.`, 'warning');
      return;
    }

    this.processes.push({ pid, arrival_time: arrival, burst_time: burst, priority });
    this.renderProcessQueue();

    if (pidInput) pidInput.value = `P${this.processes.length + 1}`;
    window.app.showToast(`Process ${pid} added to queue.`, 'info');
  },

  removeProcess(idx) {
    this.processes.splice(idx, 1);
    this.renderProcessQueue();
  },

  async runSimulation() {
    if (this.processes.length === 0) {
      window.app.showToast('Please add at least one process to the queue', 'warning');
      return;
    }

    const algoSelect = document.getElementById('sched-algo');
    const quantumInput = document.getElementById('sched-quantum');

    const algo = algoSelect ? algoSelect.value : 'FCFS';
    const quantum = quantumInput ? parseInt(quantumInput.value, 10) || 2 : 2;
    if (algo === 'RR' && (quantum < 1 || quantum > 20)) {
      window.app.showToast('Round Robin quantum must be between 1 and 20.', 'warning');
      return;
    }

    try {
      const res = await window.api.request('/api/scheduler/simulate', {
        method: 'POST',
        body: JSON.stringify({
          processes: this.processes,
          algorithm: algo,
          time_quantum: quantum
        })
      });

      const r = res.result;
      if (!r) throw new Error('No simulation result returned');

      this.displayResults(r);
      const compareBox = document.getElementById('sched-compare-container');
      if (compareBox) compareBox.style.display = 'none';
      window.app.showToast(`${r.algorithm} simulation finished!`, 'success');
    } catch (err) {
      window.app.showToast('Scheduler simulation failed: ' + err.message, 'error');
    }
  },

  async compareAll() {
    if (this.processes.length === 0) {
      window.app.showToast('Add processes to compare algorithms', 'warning');
      return;
    }

    const quantumInput = document.getElementById('sched-quantum');
    const quantum = quantumInput ? parseInt(quantumInput.value, 10) || 2 : 2;

    try {
      const res = await window.api.request('/api/scheduler/simulate', {
        method: 'POST',
        body: JSON.stringify({
          processes: this.processes,
          algorithm: 'ALL',
          time_quantum: quantum
        })
      });

      const list = res.comparison || [];
      const compareBox = document.getElementById('sched-compare-container');
      const compareContent = document.getElementById('sched-compare-content');

      if (compareBox && compareContent) {
        compareBox.style.display = 'block';
        compareContent.innerHTML = `
          <table class="data-table">
            <thead>
              <tr>
                <th>Algorithm</th>
                <th>Avg Turnaround Time</th>
                <th>Avg Waiting Time</th>
                <th>CPU Utilization</th>
                <th>Total Time</th>
              </tr>
            </thead>
            <tbody>
              ${list.map(c => `
                <tr>
                  <td><strong class="highlight-blue">${c.algorithm}</strong></td>
                  <td class="font-mono">${(c.avg_turnaround_time || 0).toFixed(2)}</td>
                  <td class="font-mono highlight-teal"><strong>${(c.avg_waiting_time || 0).toFixed(2)}</strong></td>
                  <td>${this.calculateCpuUtilization(c).toFixed(1)}%</td>
                  <td>${c.total_time || 0}</td>
                </tr>
              `).join('')}
            </tbody>
          </table>
        `;
      }

      if (list.length > 0) {
        this.displayResults(list[0]);
      }
      window.app.showToast('Algorithm comparison complete!', 'success');
    } catch (err) {
      window.app.showToast('Comparison failed: ' + err.message, 'error');
    }
  },

  displayResults(r) {
    this.stopPlayback();
    this.result = r;
    this.stepIndex = 0;

    const avgTAT = document.getElementById('sched-avg-turnaround');
    const avgWT = document.getElementById('sched-avg-waiting');
    const cpuUtil = document.getElementById('sched-cpu-util');

    if (avgTAT) avgTAT.textContent = `${(r.avg_turnaround_time || 0).toFixed(2)} units`;
    if (avgWT) avgWT.textContent = `${(r.avg_waiting_time || 0).toFixed(2)} units`;
    if (cpuUtil) cpuUtil.textContent = `${this.calculateCpuUtilization(r).toFixed(1)}%`;

    // Render Gantt chart
    const ganttContainer = document.getElementById('gantt-chart-container');
    const labelsContainer = document.getElementById('sched-timeline-labels');

    if (ganttContainer) {
      ganttContainer.innerHTML = '';
      const totalTime = r.total_time || 1;

      (r.gantt_chart || []).forEach((bar, index) => {
        const duration = bar.end_time - bar.start_time;
        const widthPct = Math.max(1, (duration / totalTime) * 100);
        const el = document.createElement('div');
        el.className = `gantt-bar ${bar.is_idle ? 'idle' : ''} ${index === this.stepIndex ? 'scheduler-current' : ''}`;
        el.style.width = `${widthPct}%`;
        el.style.backgroundColor = bar.is_idle ? '#334155' : this.getPidColor(bar.pid);
        el.textContent = bar.is_idle ? 'IDLE' : bar.pid;
        el.title = `${bar.pid}: [${bar.start_time} -> ${bar.end_time}]`;
        ganttContainer.appendChild(el);
      });
    }

    if (labelsContainer) {
      labelsContainer.innerHTML = `<span>0</span><span>Total Execution Time: ${r.total_time || 0} units</span>`;
    }

    // Render process metrics table
    const tbody = document.getElementById('sched-metrics-body');
    if (tbody) {
      tbody.innerHTML = (r.processes || []).map(m => `
        <tr>
          <td class="font-mono"><strong>${this.escape(m.pid)}</strong></td>
          <td>${m.arrival_time || 0}</td>
          <td>${m.burst_time || 0}</td>
          <td class="font-mono">${m.completion_time || 0}</td>
          <td class="font-mono highlight-blue">${m.turnaround_time || 0}</td>
          <td class="font-mono highlight-teal"><strong>${m.waiting_time || 0}</strong></td>
          <td class="font-mono">${m.response_time || 0}</td>
        </tr>
      `).join('');
    }

    this.renderStep();
  },

  calculateCpuUtilization(result) {
    if (!result || !result.total_time || result.total_time <= 0) return 0;
    const idle = result.idle_time || 0;
    return Math.max(0, Math.min(100, ((result.total_time - idle) / result.total_time) * 100));
  },

  renderStep() {
    if (!this.result || !(this.result.gantt_chart || []).length) return;
    const ganttContainer = document.getElementById('gantt-chart-container');
    const status = document.getElementById('sched-step-status');
    const bars = ganttContainer ? ganttContainer.querySelectorAll('.gantt-bar') : [];
    bars.forEach((bar, index) => bar.classList.toggle('scheduler-current', index === this.stepIndex));

    const interval = this.result.gantt_chart[this.stepIndex];
    if (!interval) return;
    if (status) {
      status.textContent = interval.is_idle
        ? `Step ${this.stepIndex + 1}: CPU idle from ${interval.start_time} to ${interval.end_time}`
        : `Step ${this.stepIndex + 1}: ${interval.pid} runs from ${interval.start_time} to ${interval.end_time}`;
    }
  },

  moveStep(delta) {
    if (!this.result) return;
    const total = (this.result.gantt_chart || []).length;
    if (total === 0) return;
    this.stepIndex = Math.max(0, Math.min(total - 1, this.stepIndex + delta));
    this.renderStep();
  },

  togglePlayback() {
    if (!this.result) {
      window.app.showToast('Run a scheduling simulation first.', 'warning');
      return;
    }
    if (this.playbackTimer) {
      this.stopPlayback();
      return;
    }
    const button = document.getElementById('btn-sched-play');
    if (button) button.textContent = 'Pause';
    this.playbackTimer = setInterval(() => {
      if (this.stepIndex >= this.result.gantt_chart.length - 1) {
        this.stopPlayback();
        return;
      }
      this.moveStep(1);
    }, 900);
  },

  stopPlayback(callback) {
    if (this.playbackTimer) {
      clearInterval(this.playbackTimer);
      this.playbackTimer = null;
    }
    const button = document.getElementById('btn-sched-play');
    if (button) button.textContent = 'Play';
    if (callback) callback();
  },

  getPidColor(pid) {
    const palette = ['#3b82f6', '#14b8a6', '#8b5cf6', '#ec4899', '#f59e0b', '#10b981', '#06b6d4'];
    let hash = 0;
    for (let i = 0; i < pid.length; i++) {
      hash = pid.charCodeAt(i) + ((hash << 5) - hash);
    }
    return palette[Math.abs(hash) % palette.length];
  },

  escape(s) {
    if (!s) return '';
    return String(s).replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;');
  }
};

window.Scheduler = window.schedulerModule;
