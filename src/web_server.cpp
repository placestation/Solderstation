#include "web_server.h"
#include "config.h"
#include "reflow_logic.h"
#include <ESPmDNS.h>
#include <Update.h>
static WiFiManager   wifiManager;
static WebServer     server(80);
static WifiState     wifiState     = WIFI_STATE_CONNECTING;
static bool          serverStarted = false;
static const char*   AP_NAME       = "Solderstation-Setup";

const char ROOT_HTML[] PROGMEM = R"=====(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
  <title>Solderstation</title>
  <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
  <style>
    :root {
      --bg: #f7f7f7; --card: #ffffff; --text: #1c1c1e; --text-secondary: #8e8e93;
      --primary: #FF5500; --destructive: #ff3b30; --success: #34c759;
      --warning: #ff9500;
      --chart-ideal: #007aff; --chart-actual: #ff3b30; --border: #e5e5ea; --highlight: #f2f2f7;
    }
    * { box-sizing: border-box; margin: 0; padding: 0; -webkit-tap-highlight-color: transparent; }
    body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; background-color: var(--bg); color: var(--text); line-height: 1.4; }
    button:disabled { opacity: 0.5; cursor: not-allowed; }
    .container { padding: 12px; max-width: 500px; margin: 0 auto; }
    .main-header { text-align: center; font-size: 24px; font-weight: 700; margin: 16px 0 20px 0; }
    .card { background-color: var(--card); border-radius: 16px; padding: 16px; margin-bottom: 12px; box-shadow: 0 2px 8px rgba(0,0,0,0.05); }
    .card-header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 12px; }
    .card-title { font-size: 16px; font-weight: 600; }
    .temperature-display { display: flex; justify-content: space-between; align-items: flex-start; }
    .temp-info { flex: 1; }
    .temperature-value { font-size: 36px; font-weight: 500; margin-bottom: 4px; }
    .temperature-target, .time-elapsed { font-size: 14px; color: var(--text-secondary); margin-bottom: 8px; }
    .state-badge { display: inline-block; padding: 4px 10px; border-radius: 12px; font-size: 12px; font-weight: 600; background-color: var(--highlight); color: var(--text-secondary); }
    .state-badge.active { background-color: rgba(52,199,89,0.2); color: var(--success); }
    #graph-container { height: 200px; margin-top: 8px; }
    .segmented-control { display: flex; background-color: var(--highlight); border-radius: 10px; padding: 2px; }
    .segment-btn { flex: 1; text-align: center; padding: 8px 0; font-size: 13px; font-weight: 500; border-radius: 8px; border: none; background: transparent; color: var(--text-secondary); transition: all 0.2s; }
    .segment-btn.active { background-color: var(--card); color: var(--text); box-shadow: 0 1px 2px rgba(0,0,0,0.1); }
    .action-buttons { display: flex; gap: 10px; }
    .btn { flex: 1; padding: 14px 20px; border-radius: 12px; font-size: 16px; font-weight: 600; border: none; cursor: pointer; transition: all 0.2s; text-align: center; }
    .btn-primary     { background-color: var(--primary);     color: white; }
    .btn-destructive { background-color: var(--destructive); color: white; }
    .btn-success     { background-color: var(--success);     color: white; }
    .btn-warning     { background-color: var(--warning);     color: white; }
    .btn-sm { padding: 10px 16px; font-size: 14px; border-radius: 10px; }
    .form-group { margin-bottom: 16px; }
    .form-label { display: block; margin-bottom: 6px; font-size: 13px; font-weight: 500; color: var(--text-secondary); }
    .form-control { width: 100%; padding: 12px; font-size: 16px; border-radius: 10px; border: 1px solid var(--border); background-color: var(--card); color: var(--text); outline: none; }
    .form-control:focus { border-color: var(--primary); }
    .form-grid { display: grid; grid-template-columns: 1fr 1fr; gap: 12px; }
    .form-submit { width: 100%; padding: 14px; background-color: var(--primary); color: white; border: none; border-radius: 12px; font-size: 16px; font-weight: 600; margin-top: 8px; }
    .indicator { width: 8px; height: 8px; border-radius: 50%; display: inline-block; margin-right: 6px; }
    .indicator.ideal  { background-color: var(--chart-ideal); }
    .indicator.actual { background-color: var(--chart-actual); }
    .chart-legend { display: flex; justify-content: center; font-size: 12px; margin-top: 8px; }
    .legend-item { display: flex; align-items: center; margin: 0 8px; color: var(--text-secondary); }
    div.disabled { opacity: 0.5; pointer-events: none; }
    .pid-bar-wrap { margin-top: 10px; }
    .pid-bar-label { font-size: 12px; color: var(--text-secondary); margin-bottom: 4px; display: flex; justify-content: space-between; }
    .pid-bar-track { height: 6px; background: var(--highlight); border-radius: 99px; overflow: hidden; }
    .pid-bar-fill { height: 100%; background: var(--primary); border-radius: 99px; transition: width 0.5s; width: 0%; }
    #custom-profile-editor { max-height: 500px; overflow: hidden; transition: max-height 0.4s ease-out, margin-top 0.4s ease-out; margin-top: 16px; }
    #custom-profile-editor.hidden { max-height: 0; margin-top: 0; }
    /* Autotune card styles */
    .at-desc { font-size: 13px; color: var(--text-secondary); margin-bottom: 12px; }
    .at-status-row { font-size: 13px; color: var(--text-secondary); margin-bottom: 12px; min-height: 18px; }
    .at-pid-values { background: var(--highlight); border-radius: 10px; padding: 12px; margin: 12px 0; display: none; }
    .at-pid-values.visible { display: block; }
    .at-pid-row { display: flex; justify-content: space-between; align-items: center; padding: 4px 0; border-bottom: 1px solid var(--border); font-size: 14px; }
    .at-pid-row:last-child { border-bottom: none; }
    .at-pid-label { font-weight: 600; color: var(--text-secondary); }
    .at-pid-val   { font-family: monospace; font-size: 15px; }
    .at-source-badge { display: inline-block; padding: 2px 8px; border-radius: 8px; font-size: 11px; font-weight: 600; margin-left: 8px; }
    .at-source-nvs     { background: rgba(52,199,89,0.15); color: var(--success); }
    .at-source-default { background: rgba(142,142,147,0.15); color: var(--text-secondary); }
    .at-save-row { display: flex; gap: 10px; margin-top: 10px; }
    .toast { position: fixed; bottom: 24px; left: 50%; transform: translateX(-50%); background: #1c1c1e; color: white; padding: 12px 24px; border-radius: 20px; font-size: 14px; font-weight: 500; opacity: 0; transition: opacity 0.3s; pointer-events: none; z-index: 999; }
    .toast.show { opacity: 1; }
    .ota-drop { border: 2px dashed var(--border); border-radius: 12px; padding: 24px; text-align: center; cursor: pointer; transition: all 0.2s; color: var(--text-secondary); font-size: 14px; }
    .ota-drop.drag-over { border-color: var(--primary); background: rgba(0,122,255,0.05); }
    .ota-drop.has-file { border-color: var(--success); background: rgba(52,199,89,0.05); color: var(--text); }
    .ota-progress { margin-top: 12px; display: none; }
    .ota-progress-track { height: 8px; background: var(--highlight); border-radius: 99px; overflow: hidden; margin-top: 6px; }
    .ota-progress-fill { height: 100%; background: var(--primary); border-radius: 99px; width: 0%; transition: width 0.3s; }
    .ota-status { font-size: 13px; color: var(--text-secondary); margin-top: 6px; }
  </style>
</head>
<body>
  <div class="container">
    <h1 class="main-header">Solderstation</h1>

    <!-- Status card -->
    <div class="card">
      <div class="temperature-display">
        <div class="temp-info">
          <div class="temperature-value" id="temp">--.- °C</div>
          <div class="temperature-target" id="target-temp">Target: --.- °C</div>
          <div class="time-elapsed" id="elapsed-time">Time: 00:00</div>
          <div class="state-badge" id="state-badge">IDLE</div>
        </div>
        <button class="btn-primary btn" id="toggle-btn" onclick="toggleReflow()">START</button>
      </div>
      <div class="pid-bar-wrap">
        <div class="pid-bar-label"><span>PID Output</span><span id="pid-val">0%</span></div>
        <div class="pid-bar-track"><div class="pid-bar-fill" id="pid-bar"></div></div>
      </div>
    </div>

    <!-- Chart card -->
    <div class="card">
      <div class="card-header"><div class="card-title">Temperature Profile</div></div>
      <div id="graph-container"><canvas id="tempChart"></canvas></div>
      <div class="chart-legend">
        <div class="legend-item"><span class="indicator ideal"></span>Target</div>
        <div class="legend-item"><span class="indicator actual"></span>Actual</div>
      </div>
    </div>

    <!-- Manual heater card -->
    <div class="card" id="manual-control-card">
      <div class="card-header"><div class="card-title">Manual Heater Control</div></div>
      <div class="action-buttons">
        <button class="btn btn-primary" id="heat-all-btn" onclick="manualHeat('all')">Heat All</button>
      </div>
      <div class="action-buttons" style="margin-top:8px">
        <button class="btn btn-primary" id="heat-top-btn"    onclick="manualHeat('top')">Heat Only Top</button>
        <button class="btn btn-primary" id="heat-bottom-btn" onclick="manualHeat('bottom')">Heat Only Bottom</button>
      </div>
    </div>

    <!-- Profile selection card -->
    <div class="card" id="profile-selection-card">
      <div class="card-header"><div class="card-title">Profile Selection</div></div>
      <div class="segmented-control" id="profile-selector">
        <button class="segment-btn active" onclick="setProfile(0)">Lead-Free</button>
        <button class="segment-btn"        onclick="setProfile(1)">Lead-Based</button>
        <button class="segment-btn"        onclick="setProfile(2)">Custom</button>
      </div>
      <div id="custom-profile-editor" class="hidden">
        <form id="profile-form" onsubmit="updateProfile(event)">
          <div class="form-grid">
            <div class="form-group">
              <label class="form-label">Soak Temp (°C)</label>
              <input type="number" id="soakTemp" class="form-control" step="1" name="soakTemp">
            </div>
            <div class="form-group">
              <label class="form-label">Soak Time (s)</label>
              <input type="number" id="soakTime" class="form-control" step="1" name="soakTime">
            </div>
            <div class="form-group">
              <label class="form-label">Reflow Temp (°C)</label>
              <input type="number" id="reflowTemp" class="form-control" step="1" name="reflowTemp">
            </div>
            <div class="form-group">
              <label class="form-label">Reflow Time (s)</label>
              <input type="number" id="reflowTime" class="form-control" step="1" name="reflowTime">
            </div>
          </div>
          <button id="update-profile-btn" type="submit" class="form-submit">Update Custom Profile</button>
        </form>
      </div>
    </div>

    <!-- PID Autotune card -->
    <div class="card" id="autotune-card">
      <div class="card-header">
        <div class="card-title">PID Autotune</div>
        <span id="pid-source-badge" class="at-source-badge at-source-default">Defaults</span>
      </div>
      <p class="at-desc">Heats oven to 150°C and oscillates to calculate ideal Kp, Ki, Kd. Takes ~5 min. Oven must be idle and cool.</p>

      <!-- Current PID values always visible -->
      <div class="at-pid-values visible" id="current-pid-box">
        <div class="at-pid-row"><span class="at-pid-label">Kp</span><span class="at-pid-val" id="cur-kp">--</span></div>
        <div class="at-pid-row"><span class="at-pid-label">Ki</span><span class="at-pid-val" id="cur-ki">--</span></div>
        <div class="at-pid-row"><span class="at-pid-label">Kd</span><span class="at-pid-val" id="cur-kd">--</span></div>
      </div>

      <div class="at-status-row" id="at-status">Status: Idle</div>

      <!-- Start / Cancel buttons -->
      <div class="action-buttons" id="at-btn-row">
        <button class="btn btn-primary"     id="at-start-btn"  onclick="startAutotune()">Start Autotune</button>
        <button class="btn btn-destructive" id="at-cancel-btn" onclick="cancelAutotune()" style="display:none">Cancel</button>
      </div>

      <!-- Save / Reset buttons — shown after autotune completes -->
      <div class="at-save-row" id="at-save-row" style="display:none">
        <button class="btn btn-success btn-sm" onclick="savePid()">💾 Save to Flash</button>
        <button class="btn btn-warning btn-sm" onclick="resetPid()">↩ Reset to Defaults</button>
      </div>
    </div>

    <!-- WiFi Reset card -->
    <div class="card" id="wifi-card">
      <div class="card-header">
        <div class="card-title">WiFi Settings</div>
        <span id="wifi-status-badge" style="font-size:12px;color:var(--text-secondary)"></span>
      </div>
      <p style="font-size:13px;color:var(--text-secondary);margin-bottom:12px">
        Reset saved WiFi credentials and restart into setup mode to connect to a different network.
      </p>
      <button class="btn btn-destructive" onclick="resetWifi()">Reset WiFi &amp; Restart</button>
    </div>

    <!-- OTA Firmware Update card -->
    <div class="card">
      <div class="card-header"><div class="card-title">Firmware Update</div></div>
      <p style="font-size:13px;color:var(--text-secondary);margin-bottom:12px">
        Upload a <code>.bin</code> built by PlatformIO
        (<code>.pio/build/esp32s3_devkit/firmware.bin</code>).
        Device restarts automatically after flashing.
      </p>
      <div class="ota-drop" id="ota-drop" onclick="document.getElementById('ota-file').click()">
        <div id="ota-drop-text">📁 Tap to select firmware.bin or drag &amp; drop here</div>
      </div>
      <input type="file" id="ota-file" accept=".bin" style="display:none" onchange="otaFileSelected(this)">
      <div class="ota-progress" id="ota-progress">
        <div class="ota-progress-track"><div class="ota-progress-fill" id="ota-bar"></div></div>
        <div class="ota-status" id="ota-status">Uploading...</div>
      </div>
      <div class="action-buttons" style="margin-top:12px">
        <button class="btn btn-primary" id="ota-btn" onclick="otaUpload()" disabled>Flash Firmware</button>
      </div>
    </div>

  </div><!-- /container -->

  <!-- Toast notification -->
  <div class="toast" id="toast"></div>

  <script>
    let tempChart, isReflowing = false, isProfileFormEdited = false, pollDataIntervalId;
    const profileSelectionCard = document.getElementById('profile-selection-card');
    const customProfileForm    = document.getElementById('profile-form');
    const updateProfileBtn     = document.getElementById('update-profile-btn');
    updateProfileBtn.disabled  = true;

    customProfileForm.addEventListener('click', (e) => {
      if (e.target.tagName.toUpperCase() === 'INPUT') {
        isProfileFormEdited = true;
        updateProfileBtn.disabled = false;
      }
    });

    function formatTime(s) {
      return `${Math.floor(s/60).toString().padStart(2,'0')}:${(s%60).toString().padStart(2,'0')}`;
    }

    function showToast(msg, duration = 2500) {
      const t = document.getElementById('toast');
      t.textContent = msg;
      t.classList.add('show');
      setTimeout(() => t.classList.remove('show'), duration);
    }

    function createChart() {
      const ctx = document.getElementById('tempChart').getContext('2d');
      tempChart = new Chart(ctx, {
        type: 'line',
        data: { datasets: [
          { label: 'Ideal',  data: [], borderColor: '#007aff', borderWidth: 2, pointRadius: 0, tension: 0.2, order: 1 },
          { label: 'Actual', data: [], borderColor: '#ff3b30', backgroundColor: 'rgba(255,59,48,0.1)', borderWidth: 2, pointRadius: 0, fill: false, order: 2 }
        ]},
        options: {
          responsive: true, maintainAspectRatio: false,
          interaction: { mode: 'index', intersect: false },
          scales: {
            x: { type: 'linear', grid: { color: 'rgba(0,0,0,0.05)' }, title: { display: true, text: 'Time (s)' } },
            y: { grid: { color: 'rgba(0,0,0,0.05)' }, title: { display: true, text: 'Temperature (°C)' } }
          },
          plugins: { legend: { display: false } }, animation: { duration: 250 }
        }
      });
    }

    function updateUI(data) {
      if (data.isOverheat) {
        document.body.innerHTML = '<div class="container"><h1 class="main-header">Solderstation</h1><div class="card"><h2 style="color:var(--destructive)">OVERHEAT</h2><p>Restart to clear.</p></div></div>';
        clearInterval(pollDataIntervalId); return;
      }

      document.getElementById('temp').textContent         = data.temp.toFixed(1) + ' °C';
      document.getElementById('target-temp').textContent  = 'Target: ' + data.targetTemp.toFixed(1) + ' °C';
      document.getElementById('elapsed-time').textContent = 'Time: ' + formatTime(data.elapsedTime);
      const pidPct = Math.round((data.pidOutput / 255.0) * 100);
      document.getElementById('pid-val').textContent = pidPct + '%';
      document.getElementById('pid-bar').style.width = pidPct + '%';

      const badge = document.getElementById('state-badge');
      badge.textContent = data.state;
      const manualCard = document.getElementById('manual-control-card');

      if (data.state === 'Idle') {
        badge.className = 'state-badge';
        document.getElementById('toggle-btn').textContent = 'START';
        document.getElementById('toggle-btn').className   = 'btn btn-primary';
        document.getElementById('toggle-btn').disabled    = false;
        isReflowing = false;
        manualCard.classList.remove('disabled');
        profileSelectionCard.classList.remove('disabled');
      } else {
        badge.className = 'state-badge active';
        document.getElementById('toggle-btn').textContent = 'STOP';
        document.getElementById('toggle-btn').className   = 'btn btn-destructive';
        isReflowing = true;
        manualCard.classList.add('disabled');
        profileSelectionCard.classList.add('disabled');
      }

      const btns = document.getElementById('profile-selector').children;
      for (let i = 0; i < btns.length; i++)
        btns[i].className = 'segment-btn' + (i === data.currentProfile ? ' active' : '');
      document.getElementById('custom-profile-editor').classList.toggle('hidden', data.currentProfile !== 2);
      if (!isProfileFormEdited) {
        const cp = data.profiles[2];
        document.getElementById('soakTemp').value   = cp.soakTemp;
        document.getElementById('soakTime').value   = cp.soakTime;
        document.getElementById('reflowTemp').value = cp.reflowTemp;
        document.getElementById('reflowTime').value = cp.reflowTime;
      }
      updateProfileBtn.disabled = !isProfileFormEdited;

      const on = 'btn-destructive', off = 'btn-primary';
      document.getElementById('heat-top-btn').className    = 'btn ' + (data.heater1_on ? on : off);
      document.getElementById('heat-bottom-btn').className = 'btn ' + (data.heater2_on ? on : off);
      document.getElementById('heat-all-btn').className    = 'btn ' + (data.heater1_on && data.heater2_on ? on : off);

      // --- Autotune UI ---
      // Current PID values row
      document.getElementById('cur-kp').textContent = data.pidKp.toFixed(4);
      document.getElementById('cur-ki').textContent = data.pidKi.toFixed(6);
      document.getElementById('cur-kd').textContent = data.pidKd.toFixed(4);

      // Source badge
      const srcBadge = document.getElementById('pid-source-badge');
      if (data.pidFromNvs) {
        srcBadge.textContent  = '💾 Saved';
        srcBadge.className    = 'at-source-badge at-source-nvs';
      } else {
        srcBadge.textContent  = 'Defaults';
        srcBadge.className    = 'at-source-badge at-source-default';
      }

      document.getElementById('at-status').textContent = 'Status: ' + data.atStatus;

      const startBtn  = document.getElementById('at-start-btn');
      const cancelBtn = document.getElementById('at-cancel-btn');
      const saveRow   = document.getElementById('at-save-row');

      if (data.atRunning) {
        startBtn.style.display  = 'none';
        cancelBtn.style.display = '';
        saveRow.style.display   = 'none';
        document.getElementById('toggle-btn').disabled = true;
      } else {
        startBtn.style.display  = '';
        cancelBtn.style.display = 'none';
        document.getElementById('toggle-btn').disabled = false;
        // Show save/reset row if autotune has been run (atKp > 0) OR values already in NVS
        saveRow.style.display = (data.atKp > 0 || data.pidFromNvs) ? '' : 'none';
      }
    }

    function updateChart(data) {
      if (!tempChart) return;
      if (data.state === 'Idle' || data.state === 'Preparing') tempChart.data.datasets[1].data = [];
      else {
        tempChart.data.datasets[1].data.push({ x: data.elapsedTime, y: data.temp });
        if (tempChart.data.datasets[1].data.length > 300) tempChart.data.datasets[1].data.shift();
      }
      if (JSON.stringify(tempChart.data.datasets[0].data) !== JSON.stringify(data.idealProfile))
        tempChart.data.datasets[0].data = data.idealProfile;
      tempChart.update('none');
    }

    async function fetchData() {
      try {
        const r = await fetch('/data');
        const d = await r.json();
        updateUI(d); updateChart(d);
      } catch(e) { console.error(e); }
    }

    function toggleReflow() {
      if (isReflowing) fetch('/stop');
      else fetch('/start').then(() => { if (tempChart) tempChart.data.datasets[1].data = []; });
    }
    function setProfile(p) { if (isReflowing) return; fetch('/setProfile?index=' + p).then(fetchData); }
    function manualHeat(m) { fetch('/manualHeat?mode=' + m).then(fetchData); }

    function startAutotune()  { fetch('/autotune/start').then(fetchData); }
    function cancelAutotune() { fetch('/autotune/cancel').then(fetchData); }

    function savePid() {
      fetch('/pid/save').then(r => r.text()).then(() => {
        showToast('✅ PID values saved to flash!');
        fetchData();
      });
    }
    function resetPid() {
      if (!confirm('Reset PID to hardcoded defaults and erase saved values?')) return;
      fetch('/pid/reset').then(r => r.text()).then(() => {
        showToast('↩ PID reset to defaults');
        fetchData();
      });
    }

    function updateProfile(e) {
      e.preventDefault();
      fetch('/updateProfile', { method: 'POST', body: new URLSearchParams(new FormData(e.target)) }).then(() => {
        const btn = e.target.querySelector('button[type="submit"]');
        btn.textContent = 'Updated!';
        isProfileFormEdited = false;
        updateProfileBtn.disabled = true;
        setTimeout(() => { btn.textContent = 'Update Custom Profile'; }, 1500);
      });
    }

    function resetWifi() {
      if (!confirm('This will erase saved WiFi credentials and restart into setup mode. Continue?')) return;
      fetch('/wifi/reset').then(() => {
        document.body.innerHTML = '<div class="container"><h1 class="main-header">Solderstation</h1><div class="card" style="text-align:center;padding:32px"><p style="font-size:18px;font-weight:600;margin-bottom:8px">Restarting...</p><p style="color:var(--text-secondary)">Connect to <b>Solderstation-Setup</b> WiFi to reconfigure.</p></div></div>';
      });
    }

    // --- OTA ---
    let otaFile = null;
    const otaDrop = document.getElementById('ota-drop');
    otaDrop.addEventListener('dragover',  (e) => { e.preventDefault(); otaDrop.classList.add('drag-over'); });
    otaDrop.addEventListener('dragleave', ()  => otaDrop.classList.remove('drag-over'));
    otaDrop.addEventListener('drop', (e) => {
      e.preventDefault(); otaDrop.classList.remove('drag-over');
      const f = e.dataTransfer.files[0];
      if (f && f.name.endsWith('.bin')) otaSetFile(f);
      else showToast('❌ Please select a .bin file');
    });
    function otaFileSelected(input) { if (input.files[0]) otaSetFile(input.files[0]); }
    function otaSetFile(f) {
      otaFile = f;
      otaDrop.classList.add('has-file');
      document.getElementById('ota-drop-text').textContent = '✅ ' + f.name + ' (' + (f.size/1024).toFixed(1) + ' KB)';
      document.getElementById('ota-btn').disabled = false;
    }
    function otaUpload() {
      if (!otaFile) return;
      if (!confirm('Flash ' + otaFile.name + '?\nDevice will restart after upload.')) return;
      const progressBox = document.getElementById('ota-progress');
      const bar    = document.getElementById('ota-bar');
      const status = document.getElementById('ota-status');
      const btn    = document.getElementById('ota-btn');
      progressBox.style.display = 'block';
      btn.disabled = true;
      clearInterval(pollDataIntervalId);
      const xhr = new XMLHttpRequest();
      xhr.open('POST', '/ota/update', true);
      xhr.upload.onprogress = (e) => {
        if (e.lengthComputable) {
          const pct = Math.round((e.loaded / e.total) * 100);
          bar.style.width = pct + '%';
          status.textContent = 'Uploading... ' + pct + '%';
        }
      };
      xhr.onload = () => {
        if (xhr.status === 200) {
          bar.style.width = '100%'; bar.style.background = 'var(--success)';
          status.textContent = '✅ Flash complete! Rebooting...';
          document.body.innerHTML = '<div class="container"><h1 class="main-header">Solderstation</h1><div class="card" style="text-align:center;padding:32px"><p style="font-size:18px;font-weight:600;margin-bottom:8px">✅ Firmware Updated!</p><p style="color:var(--text-secondary)">Device is restarting...<br><br>Page will reload in 10 seconds.</p></div></div>';
          setTimeout(() => location.reload(), 10000);
        } else {
          bar.style.background = 'var(--destructive)';
          status.textContent = '❌ Flash failed: ' + xhr.responseText;
          btn.disabled = false;
          pollDataIntervalId = setInterval(fetchData, 1000);
        }
      };
      xhr.onerror = () => {
        status.textContent = '❌ Upload error — check connection';
        btn.disabled = false;
        pollDataIntervalId = setInterval(fetchData, 1000);
      };
      const fd = new FormData(); fd.append('firmware', otaFile);
      xhr.send(fd);
    }

    window.onload = () => { createChart(); fetchData(); pollDataIntervalId = setInterval(fetchData, 1000); };
  </script>
</body>
</html>
)=====";


// ---------------------------------------------------------------------------
// Route Handlers
// ---------------------------------------------------------------------------
void handleRoot() { server.send(200, "text/html", ROOT_HTML); }

void handleData() {
    String json;
    json.reserve(1024);   // avoid repeated reallocations while building the response
    json = "{";
    json += "\"temp\":"           + String(reflow_get_current_temp())                      + ",";
    json += "\"targetTemp\":"     + String(reflow_get_target_temp())                       + ",";
    json += "\"elapsedTime\":"    + String(reflow_get_elapsed_time())                      + ",";
    json += "\"pidOutput\":"      + String(reflow_get_pid_output())                        + ",";
    json += "\"isOverheat\":"     + String(isOverheat() ? "true" : "false")                + ",";
    json += "\"tcFault\":"        + String(reflow_is_tc_fault() ? "true" : "false")        + ",";
    json += String("\"state\":\"") + reflow_get_state_string()                             + "\",";
    json += "\"currentProfile\":" + String(reflow_get_current_profile_index())             + ",";
    json += "\"idealProfile\":"   + reflow_get_ideal_profile_points()                      + ",";
    json += "\"heater1_on\":"     + String(reflow_is_manual_heater_on(1) ? "true":"false") + ",";
    json += "\"heater2_on\":"     + String(reflow_is_manual_heater_on(2) ? "true":"false") + ",";
    // current PID values
    json += "\"pidKp\":"          + String(reflow_get_pid_kp(), 4)                         + ",";
    json += "\"pidKi\":"          + String(reflow_get_pid_ki(), 6)                         + ",";
    json += "\"pidKd\":"          + String(reflow_get_pid_kd(), 4)                         + ",";
    json += "\"pidFromNvs\":"     + String(reflow_pid_is_from_nvs() ? "true" : "false")    + ",";
    // autotune
    json += "\"atRunning\":"      + String(reflow_is_autotuning() ? "true" : "false")      + ",";
    json += "\"atStatus\":\""     + reflow_get_autotune_status()                           + "\",";
    json += "\"atKp\":"           + String(reflow_get_autotune_kp(), 4)                    + ",";
    json += "\"atKi\":"           + String(reflow_get_autotune_ki(), 6)                    + ",";
    json += "\"atKd\":"           + String(reflow_get_autotune_kd(), 4)                    + ",";
    json += "\"profiles\":[";
    for (int i = 0; i < reflow_get_profile_count(); i++) {
        ReflowProfile* p = reflow_get_profile(i);
        json += "{\"name\":\""    + p->name               + "\","
             +  "\"soakTemp\":"   + String(p->soakTemp)   + ","
             +  "\"soakTime\":"   + String(p->soakTime)   + ","
             +  "\"reflowTemp\":" + String(p->reflowTemp) + ","
             +  "\"reflowTime\":" + String(p->reflowTime) + "}";
        if (i < reflow_get_profile_count() - 1) json += ",";
    }
    json += "]}";
    server.send(200, "application/json", json);
}

void handleStart()  { reflow_start_process(); server.send(200, "text/plain", "OK"); }
void handleStop()   { reflow_stop_process();  server.send(200, "text/plain", "OK"); }

void handleSetProfile() {
    if (server.hasArg("index")) reflow_set_profile_index(server.arg("index").toInt());
    server.send(200, "text/plain", "OK");
}

void handleManualHeat() {
    if (server.hasArg("mode")) {
        String m = server.arg("mode");
        if      (m == "top")    reflow_toggle_manual_heat(1);
        else if (m == "bottom") reflow_toggle_manual_heat(2);
        else if (m == "all")    reflow_toggle_manual_heat(0);
    }
    server.send(200, "text/plain", "OK");
}

void handleUpdateProfile() {
    if (server.hasArg("soakTemp") && server.hasArg("soakTime") &&
        server.hasArg("reflowTemp") && server.hasArg("reflowTime")) {
        float soakT   = constrain(server.arg("soakTemp").toFloat(),   100.0f, 200.0f);
        int   soakS   = constrain(server.arg("soakTime").toInt(),      30,     240);
        float reflowT = constrain(server.arg("reflowTemp").toFloat(), 150.0f, 260.0f);
        int   reflowS = constrain(server.arg("reflowTime").toInt(),    10,     120);
        reflow_update_custom_profile(soakT, soakS, reflowT, reflowS);
    }
    server.send(200, "text/plain", "OK");
}

void handleAutotuneStart()  { reflow_start_autotune();  server.send(200, "text/plain", "OK"); }
void handleAutotuneCancel() { reflow_cancel_autotune(); server.send(200, "text/plain", "OK"); }

void handlePidSave() {
    // Save whatever the current in-RAM PID values are (could be from autotune or manual)
    reflow_save_pid_to_nvs(reflow_get_pid_kp(), reflow_get_pid_ki(), reflow_get_pid_kd());
    server.send(200, "text/plain", "OK");
}

void handlePidReset() {
    reflow_reset_pid_to_defaults();
    server.send(200, "text/plain", "OK");
}

void handleWifiReset() {
    server.send(200, "text/plain", "OK");
    delay(500);
    wifiManager.resetSettings();
    ESP.restart();
}

void web_server_reset_wifi() {
    wifiManager.resetSettings();
    ESP.restart();
}

void handleOtaUpdate() {
    server.send(200, "text/plain", Update.hasError() ? "FAIL" : "OK");
    delay(500);
    ESP.restart();
}

void handleOtaUpload() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
        Serial.printf("OTA: receiving %s\n", upload.filename.c_str());
        reflow_stop_process(); // safety — stop oven before flashing
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) Update.printError(Serial);
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize)
            Update.printError(Serial);
    } else if (upload.status == UPLOAD_FILE_END) {
        if (Update.end(true)) Serial.printf("OTA: done (%u bytes). Rebooting.\n", upload.totalSize);
        else Update.printError(Serial);
    }
}

static void setupRoutes() {
    if (serverStarted) return;
    serverStarted = true;
    server.on("/",               handleRoot);
    server.on("/data",           handleData);
    server.on("/start",          handleStart);
    server.on("/stop",           handleStop);
    server.on("/setProfile",     handleSetProfile);
    server.on("/manualHeat",     handleManualHeat);
    server.on("/updateProfile",  HTTP_POST, handleUpdateProfile);
    server.on("/autotune/start", handleAutotuneStart);
    server.on("/autotune/cancel",handleAutotuneCancel);
    server.on("/pid/save",       handlePidSave);
    server.on("/pid/reset",      handlePidReset);
    server.on("/wifi/reset",     handleWifiReset);
    server.on("/ota/update",     HTTP_POST, handleOtaUpdate, handleOtaUpload);
    server.begin();
    Serial.println("Web server started.");
}


// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void web_server_init() {
    wifiManager.setConfigPortalBlocking(false);
    wifiManager.setConfigPortalTimeout(0);
    wifiManager.setSaveConfigCallback([]() {
        Serial.println("WiFiManager: new credentials saved.");
    });

    if (wifiManager.autoConnect(AP_NAME)) {
        wifiState = WIFI_STATE_CONNECTED;
        setupRoutes();
        MDNS.begin("solderstation");
        Serial.print("WiFi connected. IP: ");
        Serial.println(WiFi.localIP());
    } else {
        wifiState = WIFI_STATE_AP_MODE;
        Serial.print("No saved WiFi. Connect to AP: ");
        Serial.println(AP_NAME);
    }
}

void web_server_loop() {
    wifiManager.process();
    switch (wifiState) {
        case WIFI_STATE_AP_MODE:
            if (WiFi.status() == WL_CONNECTED) {
                wifiState = WIFI_STATE_CONNECTED;
                setupRoutes();
                MDNS.begin("solderstation");
                Serial.print("WiFi connected via portal. IP: ");
                Serial.println(WiFi.localIP());
            }
            break;
        case WIFI_STATE_CONNECTED:
            server.handleClient();
            if (WiFi.status() != WL_CONNECTED) {
                wifiState = WIFI_STATE_DISCONNECTED;
                Serial.println("WiFi disconnected.");
            }
            break;
        case WIFI_STATE_DISCONNECTED:
            if (WiFi.status() == WL_CONNECTED) {
                wifiState = WIFI_STATE_CONNECTED;
                Serial.println("WiFi reconnected.");
            } else {
                WiFi.reconnect();
                delay(100);
            }
            break;
        case WIFI_STATE_CONNECTING:
        default:
            break;
    }
}

bool      isWifiConnected()  { return (wifiState == WIFI_STATE_CONNECTED && WiFi.status() == WL_CONNECTED); }
IPAddress getWifiIPAddress() { return WiFi.localIP(); }
WifiState getWifiState()     { return wifiState; }
String    getWifiAPName()    { return String(AP_NAME); }