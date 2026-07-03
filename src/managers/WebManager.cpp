#include "managers/WebManager.h"

#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <stdlib.h>

#include "config/AppConfig.h"
#include "managers/LoggerManager.h"
#include "managers/TimeManager.h"

bool WebManager::begin(const char* ssid, const char* password, const char* hostname, LoggerManager* logger, TimeManager* time) {
  loggerManager_ = logger;
  timeManager_ = time;

  WiFi.mode(WIFI_STA);
  WiFi.setHostname(hostname);
  WiFi.begin(ssid, password);

  Serial.printf("[WiFi] Connecting to %s", ssid);
  const uint32_t startMs = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - startMs) < 15000) {
    delay(250);
    Serial.print('.');
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("[WiFi] Connected, IP: %s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("[WiFi] Connection timeout, running offline mode");
  }

  registerRoutes();
  server_.begin();
  Serial.println("[Web] Server started on port 80");
  return true;
}

void WebManager::loop() {
  server_.handleClient();
}

void WebManager::registerRoutes() {
  server_.on("/", [this]() { handleRoot(); });
  server_.on("/api/live", [this]() { handleLiveJson(); });
  server_.on("/api/health", [this]() { handleHealthJson(); });
  server_.on("/api/config", HTTP_GET, [this]() { handleConfigGet(); });
  server_.on("/api/config", HTTP_POST, [this]() { handleConfigPost(); });
  server_.on("/api/history", HTTP_GET, [this]() { handleHistoryJson(); });
  server_.on("/api/events", HTTP_GET, [this]() { handleEventsJson(); });
  server_.on("/api/logs", HTTP_GET, [this]() { handleLogsJson(); });
  server_.on("/api/logs/download", HTTP_GET, [this]() { handleLogDownload(); });
  server_.on("/api/sd-tree", [this]() { handleSdTreeText(); });
  server_.on("/api/action/flush-now", HTTP_POST, [this]() { handleFlushNow(); });
  server_.on("/api/action/ntp-retry", HTTP_POST, [this]() { handleNtpRetry(); });
}

void WebManager::handleRoot() {
  static const char html[] PROGMEM = R"HTML(
<!doctype html>
<html>
<head>
  <meta charset='utf-8'>
  <meta name='viewport' content='width=device-width,initial-scale=1'>
  <title>TempSensor Dashboard</title>
  <link rel="preconnect" href="https://fonts.googleapis.com">
  <link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
  <link href="https://fonts.googleapis.com/css2?family=Outfit:wght@300;400;500;600;700&family=JetBrains+Mono:wght@400;500;600&display=swap" rel="stylesheet">
  <style>
    :root {
      --bg: #090d16;
      --card-bg: rgba(21, 27, 43, 0.75);
      --card-border: rgba(255, 255, 255, 0.08);
      --text: #f3f4f6;
      --text-sub: #9ca3af;
      --accent: #06b6d4;
      --accent-hover: #0891b2;
      --success: #10b981;
      --error: #ef4444;
      --warning: #f59e0b;
      --font-main: 'Outfit', sans-serif;
      --font-mono: 'JetBrains Mono', monospace;
    }
    * { box-sizing: border-box; }
    body {
      margin: 0;
      padding: 20px;
      font-family: var(--font-main);
      background: linear-gradient(135deg, #070a13 0%, #0f172a 100%);
      color: var(--text);
      min-height: 100vh;
      -webkit-font-smoothing: antialiased;
    }
    .wrap { max-width: 1280px; margin: 0 auto; }
    
    .head {
      display: flex;
      justify-content: space-between;
      align-items: center;
      margin-bottom: 24px;
      padding-bottom: 16px;
      border-bottom: 1px solid var(--card-border);
      flex-wrap: wrap;
      gap: 16px;
    }
    .title-area {
      display: flex;
      align-items: center;
      gap: 12px;
    }
    .heartbeat {
      width: 10px;
      height: 10px;
      background-color: var(--success);
      border-radius: 50%;
      box-shadow: 0 0 0 0 rgba(16, 185, 129, 0.7);
      animation: pulse 2s infinite;
    }
    @keyframes pulse {
      0% {
        transform: scale(0.95);
        box-shadow: 0 0 0 0 rgba(16, 185, 129, 0.7);
      }
      70% {
        transform: scale(1);
        box-shadow: 0 0 0 8px rgba(16, 185, 129, 0);
      }
      100% {
        transform: scale(0.95);
        box-shadow: 0 0 0 0 rgba(16, 185, 129, 0);
      }
    }
    h1 {
      margin: 0;
      font-size: 1.4rem;
      font-weight: 700;
      letter-spacing: -0.02em;
      background: linear-gradient(to right, #ffffff, #9ca3af);
      -webkit-background-clip: text;
      -webkit-text-fill-color: transparent;
    }
    .pills { display: flex; gap: 8px; flex-wrap: wrap; }
    .pill {
      background: var(--card-bg);
      border: 1px solid var(--card-border);
      border-radius: 999px;
      padding: 6px 14px;
      font-size: 0.8rem;
      font-weight: 500;
      display: flex;
      align-items: center;
      gap: 6px;
    }
    
    .grid-main {
      display: grid;
      grid-template-columns: 1fr 1.6fr;
      gap: 20px;
      margin-bottom: 20px;
    }
    .grid-bottom {
      display: grid;
      grid-template-columns: 1fr 1fr 1.2fr;
      gap: 20px;
    }
    
    .card {
      background: var(--card-bg);
      backdrop-filter: blur(12px);
      -webkit-backdrop-filter: blur(12px);
      border: 1px solid var(--card-border);
      border-radius: 12px;
      padding: 20px;
      box-shadow: 0 10px 15px -3px rgba(0, 0, 0, 0.3), 0 4px 6px -4px rgba(0, 0, 0, 0.3);
      transition: transform 0.2s ease, box-shadow 0.2s ease;
    }
    .card h2 {
      margin: 0 0 16px 0;
      font-size: 0.95rem;
      font-weight: 600;
      color: var(--text);
      text-transform: uppercase;
      letter-spacing: 0.05em;
      border-left: 3px solid var(--accent);
      padding-left: 8px;
    }
    
    .readings {
      display: grid;
      gap: 12px;
    }
    .reading-row {
      display: flex;
      justify-content: space-between;
      align-items: center;
      padding: 12px;
      background: rgba(255, 255, 255, 0.015);
      border-radius: 8px;
      border: 1px solid rgba(255, 255, 255, 0.03);
    }
    .reading-label {
      font-size: 0.85rem;
      color: var(--text-sub);
    }
    .reading-val {
      font-family: var(--font-mono);
      font-size: 1.3rem;
      font-weight: 600;
      color: #fff;
    }
    .reading-val.temp { color: #f43f5e; text-shadow: 0 0 10px rgba(244, 63, 94, 0.25); }
    .reading-val.hum { color: #06b6d4; text-shadow: 0 0 10px rgba(6, 182, 212, 0.25); }
    .reading-val.pres { color: #10b981; text-shadow: 0 0 10px rgba(16, 185, 129, 0.25); }
    
    .live-meta {
      margin-top: 12px;
      font-size: 0.75rem;
      color: var(--text-sub);
      text-align: right;
    }
    
    .health-grid {
      display: grid;
      grid-template-columns: 1fr 1fr;
      gap: 12px;
    }
    .health-item {
      background: rgba(255, 255, 255, 0.01);
      border: 1px solid rgba(255, 255, 255, 0.03);
      padding: 10px;
      border-radius: 8px;
    }
    .health-item.full-width {
      grid-column: span 2;
    }
    .health-lbl { font-size: 0.72rem; color: var(--text-sub); margin-bottom: 4px; }
    .health-val { font-family: var(--font-mono); font-size: 0.9rem; font-weight: 600; }
    
    .progress-bar-container {
      width: 100%;
      height: 4px;
      background: rgba(255, 255, 255, 0.05);
      border-radius: 2px;
      margin-top: 6px;
      overflow: hidden;
    }
    .progress-bar {
      height: 100%;
      background: var(--accent);
      border-radius: 2px;
      width: 0%;
      transition: width 0.3s ease;
    }
    
    .controls-grid {
      display: grid;
      grid-template-columns: 1fr 1fr;
      gap: 12px;
      margin-bottom: 12px;
    }
    .form-group {
      display: flex;
      flex-direction: column;
      gap: 6px;
    }
    .form-group.full-width {
      grid-column: span 2;
    }
    input, select {
      background: rgba(0, 0, 0, 0.25);
      border: 1px solid var(--card-border);
      border-radius: 6px;
      padding: 8px 10px;
      font-family: var(--font-main);
      color: #fff;
      font-size: 0.85rem;
      transition: border-color 0.2s, box-shadow 0.2s;
    }
    input:focus, select:focus {
      outline: none;
      border-color: var(--accent);
      box-shadow: 0 0 0 2px rgba(6, 182, 212, 0.15);
    }
    input:disabled {
      opacity: 0.5;
      cursor: not-allowed;
    }
    .btn-group {
      display: flex;
      gap: 8px;
      margin-top: 10px;
      flex-wrap: wrap;
    }
    button {
      width: 100%;
      background: var(--accent);
      color: #fff;
      border: none;
      border-radius: 6px;
      padding: 10px;
      font-family: var(--font-main);
      font-weight: 600;
      font-size: 0.85rem;
      cursor: pointer;
      transition: background 0.2s, transform 0.1s, box-shadow 0.2s;
    }
    button:hover {
      background: var(--accent-hover);
      box-shadow: 0 0 10px rgba(6, 182, 212, 0.3);
    }
    button:active {
      transform: scale(0.98);
    }
    button:disabled {
      opacity: 0.5;
      cursor: not-allowed;
      box-shadow: none;
    }
    button.btn-secondary {
      flex: 1;
      background: rgba(255, 255, 255, 0.04);
      border: 1px solid var(--card-border);
      color: var(--text);
    }
    button.btn-secondary:hover {
      background: rgba(255, 255, 255, 0.08);
      box-shadow: none;
    }
    
    .alert-banner {
      padding: 10px;
      border-radius: 6px;
      font-size: 0.78rem;
      margin-top: 10px;
      display: none;
      line-height: 1.3;
    }
    .alert-banner.success {
      background: rgba(16, 185, 129, 0.12);
      border: 1px solid rgba(16, 185, 129, 0.25);
      color: var(--success);
    }
    .alert-banner.warning {
      background: rgba(245, 158, 11, 0.12);
      border: 1px solid rgba(245, 158, 11, 0.25);
      color: var(--warning);
    }
    .alert-banner.error {
      background: rgba(239, 68, 68, 0.12);
      border: 1px solid rgba(239, 68, 68, 0.25);
      color: var(--error);
    }
    
    .chart-container {
      position: relative;
      width: 100%;
      height: 250px;
      margin-top: 12px;
    }
    canvas {
      width: 100%;
      height: 100%;
      display: block;
      border-radius: 6px;
      background: rgba(0, 0, 0, 0.2);
    }
    .chart-tooltip {
      position: absolute;
      background: rgba(9, 13, 22, 0.95);
      border: 1px solid var(--card-border);
      border-radius: 6px;
      padding: 8px 12px;
      font-family: var(--font-main);
      font-size: 0.75rem;
      pointer-events: none;
      opacity: 0;
      transition: opacity 0.15s ease;
      box-shadow: 0 4px 12px rgba(0, 0, 0, 0.5);
      z-index: 10;
    }
    .tooltip-date { font-weight: 600; color: #fff; margin-bottom: 2px; }
    .tooltip-val { font-family: var(--font-mono); color: var(--accent); font-weight: 600; }
    .tooltip-q { font-size: 0.65rem; color: var(--text-sub); margin-top: 2px; }
    
    .scroll-area {
      margin: 0;
      padding: 10px;
      background: rgba(0, 0, 0, 0.25);
      border: 1px solid var(--card-border);
      border-radius: 8px;
      height: 220px;
      overflow-y: auto;
      font-family: var(--font-mono);
      font-size: 0.78rem;
      line-height: 1.4;
    }
    #sdtree {
      white-space: pre;
      color: #a5f3fc;
    }
    .logs-list {
      list-style-type: none;
      padding: 0;
      margin: 0;
    }
    .logs-list li {
      display: flex;
      justify-content: space-between;
      align-items: center;
      padding: 8px;
      border-bottom: 1px solid rgba(255, 255, 255, 0.03);
    }
    .logs-list a {
      color: var(--accent);
      text-decoration: none;
      font-weight: 600;
    }
    .logs-list a:hover {
      text-decoration: underline;
    }
    
    .timeline-container {
      display: flex;
      flex-direction: column;
      gap: 10px;
    }
    .timeline-item {
      display: flex;
      gap: 10px;
      border-left: 2px solid rgba(255, 255, 255, 0.05);
      padding-left: 10px;
      position: relative;
    }
    .timeline-item::before {
      content: '';
      position: absolute;
      left: -5px;
      top: 4px;
      width: 8px;
      height: 8px;
      border-radius: 50%;
      background: var(--text-sub);
    }
    .timeline-item.ntp_reestablished::before {
      background: var(--success);
      box-shadow: 0 0 6px var(--success);
    }
    .timeline-item.warning::before {
      background: var(--warning);
    }
    .timeline-item.error::before {
      background: var(--error);
    }
    
    .timeline-time {
      font-size: 0.68rem;
      color: var(--text-sub);
      min-width: 55px;
    }
    .timeline-content {
      font-size: 0.75rem;
    }
    
    @media (max-width: 1024px) {
      .grid-main, .grid-bottom {
        grid-template-columns: 1fr;
      }
    }
  </style>
</head>
<body>
  <div class='wrap'>
    <div class='head'>
      <div class='title-area'>
        <div class='heartbeat' id='heartbeat'></div>
        <h1>TEMPSENSOR LOCAL</h1>
      </div>
      <div class='pills'>
        <div class='pill'><span style='color: var(--text-sub)'>WiFi:</span> <span id='pillWifi'>-</span></div>
        <div class='pill'><span style='color: var(--text-sub)'>SD:</span> <span id='pillSd'>-</span></div>
        <div class='pill'><span style='color: var(--text-sub)'>NTP:</span> <span id='pillNtp'>-</span></div>
        <div class='pill'><span style='color: var(--text-sub)'>IP:</span> <span id='pillIp'>-</span></div>
        <div class='pill'><span style='color: var(--text-sub)'>Updated:</span> <span id='pillRef'>-</span></div>
      </div>
    </div>

    <div class='grid-main'>
      <div style='display: flex; flex-direction: column; gap: 20px;'>
        <section class='card'>
          <h2>Live Snapshot</h2>
          <div class='readings'>
            <div class='reading-row'>
              <div class='reading-label'>Temperature</div>
              <div class='reading-val temp' id='valTemp'>--.- °C</div>
            </div>
            <div class='reading-row'>
              <div class='reading-label'>Humidity</div>
              <div class='reading-val hum' id='valHum'>--.- %</div>
            </div>
            <div class='reading-row'>
              <div class='reading-label'>Pressure</div>
              <div class='reading-val pres' id='valPres'>----.- hPa</div>
            </div>
          </div>
          <div class='live-meta'>
            <span id='liveTime'>Timestamp: --:--:--</span> Quality: <span id='liveQuality' style='font-weight:600;'>-</span>
          </div>
        </section>

        <section class='card'>
          <h2>Health Snapshot</h2>
          <div class='health-grid'>
            <div class='health-item full-width'>
              <div class='health-lbl'>Uptime</div>
              <div class='health-val' id='healthUptime'>-</div>
            </div>
            <div class='health-item'>
              <div class='health-lbl'>Free Heap</div>
              <div class='health-val' id='healthHeap'>-</div>
              <div class='progress-bar-container'>
                <div class='progress-bar' id='barHeap'></div>
              </div>
            </div>
            <div class='health-item'>
              <div class='health-lbl'>Max Block</div>
              <div class='health-val' id='healthBlock'>-</div>
            </div>
            <div class='health-item'>
              <div class='health-lbl'>Queue Depth</div>
              <div class='health-val' id='healthQueue'>-</div>
              <div class='progress-bar-container'>
                <div class='progress-bar' id='barQueue' style='background: var(--warning);'></div>
              </div>
            </div>
            <div class='health-item'>
              <div class='health-lbl'>Dropped Samples</div>
              <div class='health-val' id='healthDropped'>-</div>
            </div>
          </div>
        </section>

        <section class='card'>
          <h2>Runtime Controls</h2>
          <form id='cfgForm'>
            <div class='controls-grid'>
              <div class='form-group'>
                <label for='cfgSample'>Sample Interval (ms)</label>
                <input id='cfgSample' type='number' min='500' max='300000' value='1000' required>
              </div>
              <div class='form-group'>
                <label for='cfgFlush'>SD Flush Interval (ms)</label>
                <input id='cfgFlush' type='number' min='5000' max='3600000' value='60000' required>
              </div>
              <div class='form-group full-width'>
                <label for='cfgDisplay'>Display Refresh Interval (ms)</label>
                <input id='cfgDisplay' type='number' min='500' max='300000' value='1000' required>
              </div>
            </div>
            <button type='submit' id='btnSaveCfg'>Apply Settings</button>
            <div class='alert-banner' id='cfgAlert'></div>
          </form>
          
          <div class='btn-group'>
            <button class='btn-secondary' id='btnFlush' title='Flush RAM buffer to SD card'>Force Flush</button>
            <button class='btn-secondary' id='btnNtp' title='Force immediate NTP time sync attempt'>Sync NTP</button>
          </div>
          <div class='alert-banner' id='actionAlert'></div>
        </section>
      </div>

      <div style='display: flex; flex-direction: column; gap: 20px;'>
        <section class='card' style='flex: 1; display: flex; flex-direction: column;'>
          <h2>Historical Chart</h2>
          <div class='controls-grid' style='grid-template-columns: repeat(auto-fit, minmax(110px, 1fr)); gap: 8px; margin-bottom: 8px;'>
            <div class='form-group'>
              <label for='metric'>Metric</label>
              <select id='metric'>
                <option value='temp_c'>Temperature</option>
                <option value='humidity_pct'>Humidity</option>
                <option value='pressure_hpa'>Pressure</option>
              </select>
            </div>
            <div class='form-group'>
              <label for='range'>Range</label>
              <select id='range'>
                <option value='15m'>Last 15m</option>
                <option value='1h' selected>Last 1h</option>
                <option value='6h'>Last 6h</option>
                <option value='24h'>Last 24h</option>
                <option value='custom'>Custom</option>
              </select>
            </div>
            <div class='form-group'>
              <label for='maxPoints'>Max Points</label>
              <input id='maxPoints' type='number' min='20' max='1000' value='300'>
            </div>
          </div>
          
          <div class='controls-grid' id='customRangeGroup' style='grid-template-columns: 1fr 1fr; gap: 8px; margin-bottom: 8px; display: none;'>
            <div class='form-group'>
              <label for='start'>Start Date/Time</label>
              <input id='start' type='datetime-local'>
            </div>
            <div class='form-group'>
              <label for='end'>End Date/Time</label>
              <input id='end' type='datetime-local'>
            </div>
          </div>

          <div style='display: flex; justify-content: flex-end; margin-bottom: 8px;'>
            <button id='btnLoad' style='width: auto; padding: 10px 20px;'>Refresh History</button>
          </div>
          
          <div class='alert-banner' id='historyAlert' style='margin-bottom: 10px;'></div>

          <div class='chart-container'>
            <canvas id='chart'></canvas>
            <div class='chart-tooltip' id='tooltip'>
              <div class='tooltip-date' id='tooltipDate'></div>
              <div class='tooltip-val' id='tooltipVal'></div>
              <div class='tooltip-q' id='tooltipQ'></div>
            </div>
          </div>
          <div class='live-meta' id='historyMeta' style='margin-top: 12px; text-align: left;'>
            No history data loaded.
          </div>
        </section>
      </div>
    </div>

    <div class='grid-bottom'>
      <section class='card'>
        <h2>SD Files & Tree</h2>
        <div class='scroll-area' id='sdtree'>loading...</div>
      </section>

      <section class='card'>
        <h2>Downloads</h2>
        <div class='scroll-area'>
          <ul class='logs-list' id='logs'>
            <li style='color: var(--text-sub);'>loading...</li>
          </ul>
        </div>
      </section>

      <section class='card'>
        <h2>Event Timeline</h2>
        <div class='scroll-area'>
          <div class='timeline-container' id='eventsTimeline'>
            <div style='color: var(--text-sub);'>loading...</div>
          </div>
        </div>
      </section>
    </div>
  </div>

  <script>
    const $ = (id) => document.getElementById(id);
    
    let chartPoints = [];
    let minVal = 0;
    let maxVal = 1;
    let hoveredPoint = null;

    async function fetchJson(url) {
      const res = await fetch(url, { cache: 'no-store' });
      if (!res.ok) throw new Error(url + ' -> HTTP ' + res.status);
      return await res.json();
    }

    async function fetchText(url) {
      const res = await fetch(url, { cache: 'no-store' });
      if (!res.ok) throw new Error(url + ' -> HTTP ' + res.status);
      return await res.text();
    }

    function stampNow() {
      const d = new Date();
      const p = (n) => String(n).padStart(2, '0');
      return p(d.getHours()) + ':' + p(d.getMinutes()) + ':' + p(d.getSeconds());
    }

    function fmtLocalTs(date) {
      const p = (n) => String(n).padStart(2, '0');
      return date.getFullYear() + '-' + p(date.getMonth() + 1) + '-' + p(date.getDate()) +
        ' ' + p(date.getHours()) + ':' + p(date.getMinutes()) + ':' + p(date.getSeconds());
    }

    function updatePills(live, health) {
      const wifi = health.wifi_connected ? 'UP' : 'DOWN';
      const sd = health.sd_healthy ? 'OK' : 'BAD';
      const ntp = health.ntp_synced ? 'SYNC' : 'EST';

      $('pillWifi').textContent = wifi;
      $('pillWifi').style.color = health.wifi_connected ? 'var(--success)' : 'var(--error)';
      
      $('pillSd').textContent = sd;
      $('pillSd').style.color = health.sd_healthy ? 'var(--success)' : 'var(--error)';
      
      $('pillNtp').textContent = ntp;
      $('pillNtp').style.color = health.ntp_synced ? 'var(--success)' : 'var(--warning)';

      let ip = '-';
      if (live && live.has_sample && typeof live.ip === 'string') {
        ip = live.ip;
      } else if (health && typeof health.ip === 'string') {
        ip = health.ip;
      }
      $('pillIp').textContent = ip;
      $('pillRef').textContent = stampNow();
    }

    function renderChart() {
      const canvas = $('chart');
      const ctx = canvas.getContext('2d');
      const dpr = window.devicePixelRatio || 1;
      const rect = canvas.getBoundingClientRect();
      
      canvas.width = rect.width * dpr;
      canvas.height = rect.height * dpr;
      ctx.scale(dpr, dpr);
      
      const w = rect.width;
      const h = rect.height;

      ctx.clearRect(0, 0, w, h);

      const padL = 50;
      const padR = 15;
      const padT = 20;
      const padB = 30;
      const gw = w - padL - padR;
      const gh = h - padT - padB;

      ctx.strokeStyle = 'rgba(255, 255, 255, 0.05)';
      ctx.lineWidth = 1;
      for (let i = 0; i <= 4; i++) {
        const y = padT + (gh * i / 4);
        ctx.beginPath();
        ctx.moveTo(padL, y);
        ctx.lineTo(w - padR, y);
        ctx.stroke();
      }

      if (chartPoints.length === 0) {
        ctx.fillStyle = 'var(--text-sub)';
        ctx.font = '13px var(--font-main)';
        ctx.textAlign = 'center';
        ctx.textBaseline = 'middle';
        ctx.fillText('No history data in this range.', padL + gw / 2, padT + gh / 2);
        return;
      }

      minVal = Number.POSITIVE_INFINITY;
      maxVal = Number.NEGATIVE_INFINITY;
      for (const p of chartPoints) {
        const val = parseFloat(p.v);
        if (!isNaN(val)) {
          if (val < minVal) minVal = val;
          if (val > maxVal) maxVal = val;
        }
      }

      if (minVal === Number.POSITIVE_INFINITY || isNaN(minVal)) {
        minVal = 0; maxVal = 100;
      }
      if (maxVal === minVal) {
        maxVal = minVal + 1;
      }

      const diff = maxVal - minVal;
      minVal -= diff * 0.05;
      maxVal += diff * 0.05;

      const metric = $('metric').value;
      let strokeColor = '#06b6d4';
      let fillColor1 = 'rgba(6, 182, 212, 0.25)';
      let fillColor2 = 'rgba(6, 182, 212, 0)';
      let suffix = '';
      if (metric === 'temp_c') {
        strokeColor = '#f43f5e';
        fillColor1 = 'rgba(244, 63, 94, 0.25)';
        fillColor2 = 'rgba(244, 63, 94, 0)';
        suffix = ' °C';
      } else if (metric === 'humidity_pct') {
        strokeColor = '#06b6d4';
        fillColor1 = 'rgba(6, 182, 212, 0.25)';
        fillColor2 = 'rgba(6, 182, 212, 0)';
        suffix = ' %';
      } else if (metric === 'pressure_hpa') {
        strokeColor = '#10b981';
        fillColor1 = 'rgba(16, 185, 129, 0.25)';
        fillColor2 = 'rgba(16, 185, 129, 0)';
        suffix = ' hPa';
      }

      const getX = (index) => padL + (chartPoints.length <= 1 ? 0.5 : (index / (chartPoints.length - 1))) * gw;
      const getY = (val) => padT + gh - ((val - minVal) / (maxVal - minVal)) * gh;

      const grad = ctx.createLinearGradient(0, padT, 0, padT + gh);
      grad.addColorStop(0, fillColor1);
      grad.addColorStop(1, fillColor2);
      ctx.fillStyle = grad;
      ctx.beginPath();
      ctx.moveTo(getX(0), padT + gh);
      for (let i = 0; i < chartPoints.length; i++) {
        ctx.lineTo(getX(i), getY(chartPoints[i].v));
      }
      ctx.lineTo(getX(chartPoints.length - 1), padT + gh);
      ctx.closePath();
      ctx.fill();

      ctx.strokeStyle = strokeColor;
      ctx.lineWidth = 2.5;
      ctx.lineJoin = 'round';
      ctx.lineCap = 'round';
      ctx.beginPath();
      ctx.moveTo(getX(0), getY(chartPoints[0].v));
      for (let i = 1; i < chartPoints.length; i++) {
        ctx.lineTo(getX(i), getY(chartPoints[i].v));
      }
      ctx.stroke();

      ctx.fillStyle = 'var(--text-sub)';
      ctx.font = '10px var(--font-mono)';
      ctx.textAlign = 'right';
      ctx.textBaseline = 'middle';
      ctx.fillText(maxVal.toFixed(1) + suffix, padL - 8, padT);
      ctx.fillText(((maxVal + minVal) / 2).toFixed(1) + suffix, padL - 8, padT + gh / 2);
      ctx.fillText(minVal.toFixed(1) + suffix, padL - 8, padT + gh);

      ctx.textAlign = 'left';
      ctx.textBaseline = 'top';
      ctx.font = '10px var(--font-mono)';
      
      const startT = chartPoints[0].ts.split(' ').slice(1).join(' ') || chartPoints[0].ts;
      const endT = chartPoints[chartPoints.length - 1].ts.split(' ').slice(1).join(' ') || chartPoints[chartPoints.length - 1].ts;
      ctx.fillText(startT, padL, padT + gh + 6);
      
      ctx.textAlign = 'right';
      ctx.fillText(endT, w - padR, padT + gh + 6);

      if (hoveredPoint !== null) {
        const i = hoveredPoint.index;
        const x = getX(i);
        const y = getY(hoveredPoint.v);

        ctx.strokeStyle = 'rgba(255, 255, 255, 0.15)';
        ctx.lineWidth = 1;
        ctx.beginPath();
        ctx.moveTo(x, padT);
        ctx.lineTo(x, padT + gh);
        ctx.stroke();

        ctx.fillStyle = strokeColor;
        ctx.strokeStyle = '#fff';
        ctx.lineWidth = 2;
        ctx.beginPath();
        ctx.arc(x, y, 6, 0, Math.PI * 2);
        ctx.fill();
        ctx.stroke();
      }
    }

    $('chart').addEventListener('mousemove', (e) => {
      if (chartPoints.length === 0) return;
      const canvas = $('chart');
      const rect = canvas.getBoundingClientRect();
      const mouseX = e.clientX - rect.left;
      const mouseY = e.clientY - rect.top;

      const padL = 50;
      const padR = 15;
      const gw = rect.width - padL - padR;

      let nearestIndex = 0;
      let minDist = Number.POSITIVE_INFINITY;
      for (let i = 0; i < chartPoints.length; i++) {
        const x = padL + (chartPoints.length <= 1 ? 0.5 : (i / (chartPoints.length - 1))) * gw;
        const dist = Math.abs(x - mouseX);
        if (dist < minDist) {
          minDist = dist;
          nearestIndex = i;
        }
      }

      if (minDist < 40) {
        const pt = chartPoints[nearestIndex];
        hoveredPoint = {
          index: nearestIndex,
          v: pt.v,
          ts: pt.ts,
          q: pt.q
        };
        renderChart();

        const tooltip = $('tooltip');
        const metric = $('metric').value;
        let suffix = '';
        if (metric === 'temp_c') suffix = ' °C';
        else if (metric === 'humidity_pct') suffix = ' %';
        else if (metric === 'pressure_hpa') suffix = ' hPa';

        $('tooltipDate').textContent = pt.ts;
        $('tooltipVal').textContent = parseFloat(pt.v).toFixed(2) + suffix;
        $('tooltipQ').textContent = 'Quality: ' + (pt.q === 'n' || pt.q === 'ntp' ? 'NTP' : 'Estimated');
        
        const xPos = padL + (chartPoints.length <= 1 ? 0.5 : (nearestIndex / (chartPoints.length - 1))) * gw;
        
        tooltip.style.left = (xPos + 10) + 'px';
        tooltip.style.top = (mouseY - 60) + 'px';
        tooltip.style.opacity = 1;
      } else {
        hoveredPoint = null;
        renderChart();
        $('tooltip').style.opacity = 0;
      }
    });

    $('chart').addEventListener('mouseleave', () => {
      hoveredPoint = null;
      renderChart();
      $('tooltip').style.opacity = 0;
    });

    window.addEventListener('resize', renderChart);

    function validateDateRange(startStr, endStr) {
      if (!startStr || !endStr) return "Both start and end date-times are required.";
      const s = new Date(startStr);
      const e = new Date(endStr);
      if (isNaN(s.getTime()) || isNaN(e.getTime())) return "Invalid date format.";
      if (e < s) return "End date-time must be greater than or equal to start date-time.";
      const diffMs = e - s;
      const maxMs = 7 * 24 * 60 * 60 * 1000;
      if (diffMs > maxMs) return "Requested date range exceeds the maximum allowed range of 7 days.";
      return null;
    }

    function buildHistoryQuery() {
      const metric = $('metric').value;
      const range = $('range').value;
      let start = '';
      let end = '';
      const now = new Date();

      if (range === 'custom') {
        const s = $('start').value;
        const e = $('end').value;
        if (s) start = s.replace('T', ' ') + ':00';
        if (e) end = e.replace('T', ' ') + ':00';
      } else {
        const mins = {
          '15m': 15,
          '1h': 60,
          '6h': 360,
          '24h': 1440
        }[range] || 60;
        const startDate = new Date(now.getTime() - mins * 60000);
        start = fmtLocalTs(startDate);
        end = fmtLocalTs(now);
      }

      const maxPoints = Math.max(20, Math.min(1000, Number($('maxPoints').value || 300)));
      const q = new URLSearchParams();
      q.set('metric', metric);
      q.set('max_points', String(maxPoints));
      if (start) q.set('start', start);
      if (end) q.set('end', end);
      return q.toString();
    }

    async function loadHistory() {
      const alert = $('historyAlert');
      alert.style.display = 'none';
      
      if ($('range').value === 'custom') {
        const err = validateDateRange($('start').value, $('end').value);
        if (err) {
          alert.textContent = err;
          alert.className = 'alert-banner error';
          alert.style.display = 'block';
          return;
        }
      }

      try {
        setText('historyMeta', 'Loading history...');
        const q = buildHistoryQuery();
        const data = await fetchJson('/api/history?' + q);
        chartPoints = Array.isArray(data.points) ? data.points : [];
        renderChart();
        setText('historyMeta',
          'Metric: ' + (data.metric || '?') +
          ' | Matched Samples: ' + (data.matched ?? 0) +
          ' | Downsampled Points: ' + chartPoints.length);
      } catch (err) {
        chartPoints = [];
        renderChart();
        setText('historyMeta', 'History error: ' + err.message);
        alert.textContent = 'History load failed: ' + err.message;
        alert.className = 'alert-banner error';
        alert.style.display = 'block';
      }
    }

    async function loadLogs() {
      try {
        const data = await fetchJson('/api/logs');
        const files = Array.isArray(data.files) ? data.files : [];
        const container = $('logs');
        if (files.length === 0) {
          container.innerHTML = '<li style="color: var(--text-sub);">No log files found.</li>';
          return;
        }

        let html = '';
        for (const f of files) {
          const n = f.name || '?';
          const s = (f.size / 1024).toFixed(2);
          const p = '/api/logs/download?file=' + encodeURIComponent('/logs/' + n);
          html += '<li><a href="' + p + '">' + n + '</a> <span style="color: var(--text-sub); font-size: 0.75rem;">(' + s + ' KB)</span></li>';
        }
        container.innerHTML = html;
      } catch (err) {
        $('logs').innerHTML = '<li style="color: var(--error);">' + err.message + '</li>';
      }
    }

    async function loadEvents() {
      try {
        const data = await fetchJson('/api/events?limit=30');
        const timeline = $('eventsTimeline');
        const list = Array.isArray(data.events) ? data.events : [];
        
        if (list.length === 0) {
          timeline.innerHTML = '<div style="color: var(--text-sub); font-size:0.75rem;">No events logged.</div>';
          return;
        }

        const reversedList = [...list].reverse();
        let html = '';
        for (const e of reversedList) {
          let severity = 'info';
          const evLower = e.event.toLowerCase();
          if (evLower.indexOf('error') >= 0 || evLower.indexOf('fail') >= 0) {
            severity = 'error';
          } else if (evLower.indexOf('warn') >= 0) {
            severity = 'warning';
          } else if (e.event === 'ntp_reestablished') {
            severity = 'ntp_reestablished';
          }
          
          const timePart = e.ts.split(' ').slice(1).join(' ') || e.ts;
          html += '<div class="timeline-item ' + severity + '">';
          html += '  <div class="timeline-time">' + timePart + '</div>';
          html += '  <div class="timeline-content">' + e.event + '</div>';
          html += '</div>';
        }
        timeline.innerHTML = html;
      } catch (err) {
        $('eventsTimeline').innerHTML = '<div style="color: var(--error); font-size:0.75rem;">' + err.message + '</div>';
      }
    }

    async function loadSnapshot() {
      try {
        const [live, health] = await Promise.all([
          fetchJson('/api/live'),
          fetchJson('/api/health')
        ]);

        if (live && live.has_sample) {
          $('valTemp').textContent = parseFloat(live.temp_c).toFixed(1) + ' °C';
          $('valHum').textContent = parseFloat(live.humidity_pct).toFixed(1) + ' %';
          $('valPres').textContent = parseFloat(live.pressure_hpa).toFixed(1) + ' hPa';
          $('liveTime').textContent = 'Timestamp: ' + live.timestamp;
          $('liveQuality').textContent = live.timestamp_quality;
          $('liveQuality').style.color = live.timestamp_quality === 'ntp' ? 'var(--success)' : 'var(--warning)';
        }

        if (health && health.has_health) {
          // Format uptime
          const up = health.uptime_s;
          const hrs = Math.floor(up / 3600);
          const mins = Math.floor((up % 3600) / 60);
          const secs = up % 60;
          $('healthUptime').textContent = hrs + 'h ' + mins + 'm ' + secs + 's';
          
          $('healthHeap').textContent = (health.free_heap_bytes / 1024).toFixed(1) + ' KB';
          const heapPercent = Math.max(0, Math.min(100, (health.free_heap_bytes / 81920) * 100));
          $('barHeap').style.width = heapPercent + '%';

          $('healthBlock').textContent = (health.largest_free_block_bytes / 1024).toFixed(1) + ' KB';
          
          $('healthQueue').textContent = health.log_queue_depth + ' / ' + health.log_queue_capacity;
          const queuePercent = Math.max(0, Math.min(100, (health.log_queue_depth / health.log_queue_capacity) * 100));
          $('barQueue').style.width = queuePercent + '%';

          $('healthDropped').textContent = health.dropped_log_samples;
          if (health.dropped_log_samples > 0) {
            $('healthDropped').style.color = 'var(--error)';
          }
        }

        updatePills(live, health);
      } catch (err) {
        console.error("Poller error", err);
      }
    }

    async function loadConfig() {
      try {
        const cfg = await fetchJson('/api/config');
        $('cfgSample').value = cfg.sample_interval_ms;
        $('cfgFlush').value = cfg.log_flush_interval_ms;
        $('cfgDisplay').value = cfg.display_refresh_interval_ms;
      } catch (err) {
        console.error("Config fetch error", err);
      }
    }

    async function loadStaticPanels() {
      try {
        const tree = await fetchText('/api/sd-tree');
        $('sdtree').textContent = tree;
      } catch (err) {
        $('sdtree').textContent = 'Error: ' + err.message;
      }
    }

    function setText(id, text) {
      $(id).textContent = text;
    }

    function onRangeChanged() {
      const custom = $('range').value === 'custom';
      $('customRangeGroup').style.display = custom ? 'grid' : 'none';
      if (custom) {
        const now = new Date();
        const oneHourAgo = new Date(now.getTime() - 60 * 60 * 1000);
        if (!$('start').value) {
          $('start').value = new Date(oneHourAgo.getTime() - oneHourAgo.getTimezoneOffset() * 60000).toISOString().slice(0, 16);
        }
        if (!$('end').value) {
          $('end').value = new Date(now.getTime() - now.getTimezoneOffset() * 60000).toISOString().slice(0, 16);
        }
      }
    }

    $('cfgForm').addEventListener('submit', async (e) => {
      e.preventDefault();
      const alert = $('cfgAlert');
      alert.style.display = 'none';
      
      const payload = {
        sample_interval_ms: parseInt($('cfgSample').value),
        log_flush_interval_ms: parseInt($('cfgFlush').value),
        display_refresh_interval_ms: parseInt($('cfgDisplay').value)
      };

      try {
        const res = await fetch('/api/config', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify(payload)
        });
        
        const data = await res.json();
        alert.style.display = 'block';
        if (data.accepted) {
          alert.textContent = data.message || "Config applied successfully.";
          alert.className = 'alert-banner success';
        } else {
          alert.textContent = "Preview response: " + (data.message || "Config apply not accepted.");
          alert.className = 'alert-banner warning';
          if (data.current) {
            $('cfgSample').value = data.current.sample_interval_ms;
            $('cfgFlush').value = data.current.log_flush_interval_ms;
            $('cfgDisplay').value = data.current.display_refresh_interval_ms;
          }
        }
      } catch (err) {
        alert.textContent = "Error applying settings: " + err.message;
        alert.className = 'alert-banner error';
        alert.style.display = 'block';
      }
    });

    async function triggerAction(url, btnId) {
      const alert = $('actionAlert');
      const btn = $(btnId);
      alert.style.display = 'none';
      btn.disabled = true;
      try {
        const res = await fetch(url, { method: 'POST' });
        const data = await res.json();
        alert.style.display = 'block';
        if (data.success) {
          alert.textContent = data.message;
          alert.className = 'alert-banner success';
        } else {
          alert.textContent = "Action failed: " + data.message;
          alert.className = 'alert-banner error';
        }
      } catch (err) {
        alert.textContent = "Network error: " + err.message;
        alert.className = 'alert-banner error';
        alert.style.display = 'block';
      } finally {
        btn.disabled = false;
        setTimeout(() => { alert.style.display = 'none'; }, 6000);
      }
    }

    $('btnFlush').addEventListener('click', () => triggerAction('/api/action/flush-now', 'btnFlush'));
    $('btnNtp').addEventListener('click', () => triggerAction('/api/action/ntp-retry', 'btnNtp'));

    $('btnLoad').addEventListener('click', loadHistory);
    $('range').addEventListener('change', () => {
      onRangeChanged();
      loadHistory();
    });
    $('metric').addEventListener('change', loadHistory);

    (async function boot() {
      onRangeChanged();
      await loadConfig();
      await loadStaticPanels();
      await loadSnapshot();
      await loadLogs();
      await loadEvents();
      await loadHistory();

      setInterval(loadSnapshot, 1000);
      setInterval(loadEvents, 5000);
      setInterval(loadLogs, 15000);
      setInterval(loadStaticPanels, 15000);
    })();
  </script>
</body>
</html>
)HTML";

  server_.send_P(200, "text/html", html);
}

void WebManager::handleLiveJson() {
  StaticJsonDocument<384> doc;

  if (latestSample_ != nullptr) {
    doc["timestamp"] = latestSample_->timestamp;
    doc["timestamp_quality"] = TimestampQualityToString(latestSample_->quality);
    doc["temp_c"] = latestSample_->temperatureC;
    doc["humidity_pct"] = latestSample_->humidityPct;
    doc["pressure_hpa"] = latestSample_->pressureHpa;
    doc["uptime_s"] = latestSample_->uptimeSeconds;
    doc["has_sample"] = true;
  } else {
    doc["has_sample"] = false;
  }

  String response;
  serializeJson(doc, response);
  server_.send(200, "application/json", response);
}

void WebManager::handleHealthJson() {
  StaticJsonDocument<512> doc;

  if (health_ != nullptr) {
    doc["uptime_s"] = health_->uptimeSeconds;
    doc["free_heap_bytes"] = health_->freeHeapBytes;
    doc["largest_free_block_bytes"] = health_->largestFreeBlockBytes;
    doc["log_queue_depth"] = health_->logQueueDepth;
    doc["log_queue_capacity"] = health_->logQueueCapacity;
    doc["dropped_log_samples"] = health_->droppedLogSamples;
    doc["wifi_connected"] = health_->wifiConnected;
    doc["sd_healthy"] = health_->sdHealthy;
    doc["ntp_synced"] = health_->ntpSynced;
    doc["has_health"] = true;
  } else {
    doc["has_health"] = false;
  }

  String response;
  serializeJson(doc, response);
  server_.send(200, "application/json", response);
}

bool WebManager::ensureSdReady() {
  if (health_ != nullptr && !health_->sdHealthy) {
    return false;
  }
  return SD.begin(AppConfig::SD_CS_PIN);
}

void WebManager::sendJsonError(int code, const char* message) {
  StaticJsonDocument<160> doc;
  doc["error"] = message;
  String response;
  serializeJson(doc, response);
  server_.send(code, "application/json", response);
}

bool WebManager::parsePositiveUIntArg(const String& key, uint32_t& out) const {
  if (!server_.hasArg(key)) {
    return false;
  }

  const String raw = server_.arg(key);
  if (raw.length() == 0) {
    return false;
  }

  for (size_t i = 0; i < raw.length(); ++i) {
    if (raw[i] < '0' || raw[i] > '9') {
      return false;
    }
  }

  out = static_cast<uint32_t>(raw.toInt());
  return true;
}

bool WebManager::isTimestampInRange(const String& ts, const String& startTs, const String& endTs) const {
  if (startTs.length() > 0 && ts < startTs) {
    return false;
  }
  if (endTs.length() > 0 && ts > endTs) {
    return false;
  }
  return true;
}

bool WebManager::parseHistoryValue(const String& line,
                                   const String& metric,
                                   String& outTs,
                                   String& outQuality,
                                   float& outValue) const {
  if (line.length() == 0 || line.startsWith("timestamp,")) {
    return false;
  }

  char buf[160]{};
  line.toCharArray(buf, sizeof(buf));

  char* save = nullptr;
  char* c0 = strtok_r(buf, ",", &save);
  char* c1 = strtok_r(nullptr, ",", &save);
  char* c2 = strtok_r(nullptr, ",", &save);
  char* c3 = strtok_r(nullptr, ",", &save);
  char* c4 = strtok_r(nullptr, ",", &save);
  if (!c0 || !c1 || !c2 || !c3 || !c4) {
    return false;
  }

  outTs = c0;
  outQuality = c1;

  const char* valueTok = nullptr;
  if (metric == "temp_c") {
    valueTok = c2;
  } else if (metric == "humidity_pct") {
    valueTok = c3;
  } else if (metric == "pressure_hpa") {
    valueTok = c4;
  } else {
    return false;
  }

  outValue = atof(valueTok);
  return true;
}

bool WebManager::parseEventRow(const String& line, String& outTs, String& outQuality, String& outEvent) const {
  if (line.length() == 0 || line.startsWith("timestamp,")) {
    return false;
  }

  char buf[160]{};
  line.toCharArray(buf, sizeof(buf));

  char* save = nullptr;
  char* c0 = strtok_r(buf, ",", &save);
  char* c1 = strtok_r(nullptr, ",", &save);
  char* c2 = strtok_r(nullptr, ",", &save);
  if (!c0 || !c1 || !c2) {
    return false;
  }

  outTs = c0;
  outQuality = c1;
  outEvent = c2;
  return true;
}

void WebManager::handleConfigGet() {
  StaticJsonDocument<320> doc;
  doc["sample_interval_ms"] = AppConfig::SAMPLE_INTERVAL_MS;
  doc["log_flush_interval_ms"] = AppConfig::LOG_FLUSH_INTERVAL_MS;
  doc["display_refresh_interval_ms"] = AppConfig::DISPLAY_REFRESH_INTERVAL_MS;
  doc["diagnostics_interval_ms"] = AppConfig::DIAGNOSTICS_INTERVAL_MS;
  doc["ntp_retry_interval_ms"] = AppConfig::NTP_RETRY_INTERVAL_MS;
  doc["max_log_queue_size"] = AppConfig::MAX_LOG_QUEUE_SIZE;
  doc["timezone"] = AppConfig::TIMEZONE_MELBOURNE;
  doc["phase"] = "foundation";

  String response;
  serializeJson(doc, response);
  server_.send(200, "application/json", response);
}

void WebManager::handleConfigPost() {
  StaticJsonDocument<256> requested;
  if (server_.hasArg("plain") && server_.arg("plain").length() > 0) {
    const DeserializationError err = deserializeJson(requested, server_.arg("plain"));
    if (err) {
      sendJsonError(400, "Invalid JSON body");
      return;
    }
  }

  StaticJsonDocument<384> response;
  response["accepted"] = false;
  response["message"] = "Config apply is scheduled for phase 3; phase 1 provides API shape only.";
  response["current"]["sample_interval_ms"] = AppConfig::SAMPLE_INTERVAL_MS;
  response["current"]["log_flush_interval_ms"] = AppConfig::LOG_FLUSH_INTERVAL_MS;
  response["current"]["display_refresh_interval_ms"] = AppConfig::DISPLAY_REFRESH_INTERVAL_MS;

  if (!requested.isNull()) {
    response["requested"] = requested.as<JsonObject>();
  }

  String out;
  serializeJson(response, out);
  server_.send(200, "application/json", out);
}

void WebManager::streamHistoryJson(File& file,
                                   const String& metric,
                                   const String& startTs,
                                   const String& endTs,
                                   uint32_t maxPoints) {
  uint32_t matched = 0;

  file.seek(0);
  while (file.available()) {
    const String line = file.readStringUntil('\n');
    String ts;
    String quality;
    float value = 0.0f;
    if (!parseHistoryValue(line, metric, ts, quality, value)) {
      continue;
    }
    if (!isTimestampInRange(ts, startTs, endTs)) {
      continue;
    }
    ++matched;
  }

  const uint32_t stride = (matched > maxPoints) ? ((matched + maxPoints - 1) / maxPoints) : 1;

  server_.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server_.send(200, "application/json", "");

  server_.sendContent("{");
  server_.sendContent("\"metric\":\"");
  server_.sendContent(metric);
  server_.sendContent("\",\"matched\":");
  server_.sendContent(String(matched));
  server_.sendContent(",\"points\":[");

  uint32_t matchedIndex = 0;
  uint32_t emitted = 0;
  file.seek(0);
  while (file.available()) {
    const String line = file.readStringUntil('\n');
    String ts;
    String quality;
    float value = 0.0f;
    if (!parseHistoryValue(line, metric, ts, quality, value)) {
      continue;
    }
    if (!isTimestampInRange(ts, startTs, endTs)) {
      continue;
    }

    if ((matchedIndex % stride) != 0) {
      ++matchedIndex;
      continue;
    }
    ++matchedIndex;

    if (emitted > 0) {
      server_.sendContent(",");
    }

    String item = "{\"ts\":\"" + ts + "\",\"q\":\"" + quality + "\",\"v\":" + String(value, 2) + "}";
    server_.sendContent(item);
    ++emitted;
    if (emitted >= maxPoints) {
      break;
    }
  }

  server_.sendContent("]}");
}

void WebManager::streamEventsJson(File& file, const String& startTs, const String& endTs, uint32_t limit) {
  server_.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server_.send(200, "application/json", "");

  server_.sendContent("{\"events\":[");
  uint32_t emitted = 0;
  while (file.available()) {
    const String line = file.readStringUntil('\n');
    String ts;
    String quality;
    String eventName;
    if (!parseEventRow(line, ts, quality, eventName)) {
      continue;
    }
    if (!isTimestampInRange(ts, startTs, endTs)) {
      continue;
    }

    if (emitted > 0) {
      server_.sendContent(",");
    }
    String item = "{\"ts\":\"" + ts + "\",\"q\":\"" + quality + "\",\"event\":\"" + eventName + "\"}";
    server_.sendContent(item);
    ++emitted;
    if (emitted >= limit) {
      break;
    }
  }

  server_.sendContent("],\"count\":");
  server_.sendContent(String(emitted));
  server_.sendContent("}");
}

void WebManager::handleHistoryJson() {
  const String metric = server_.arg("metric");
  if (metric != "temp_c" && metric != "humidity_pct" && metric != "pressure_hpa") {
    sendJsonError(400, "metric must be temp_c, humidity_pct, or pressure_hpa");
    return;
  }

  uint32_t maxPoints = 300;
  if (server_.hasArg("max_points")) {
    if (!parsePositiveUIntArg("max_points", maxPoints) || maxPoints == 0) {
      sendJsonError(400, "max_points must be a positive integer");
      return;
    }
    if (maxPoints > 1000) {
      maxPoints = 1000;
    }
  }

  if (!ensureSdReady()) {
    sendJsonError(503, "SD card unavailable");
    return;
  }

  File file = SD.open(AppConfig::LOG_FILE_PATH, "r");
  if (!file) {
    sendJsonError(404, "log file not found");
    return;
  }

  const String startTs = server_.arg("start");
  const String endTs = server_.arg("end");
  streamHistoryJson(file, metric, startTs, endTs, maxPoints);
  file.close();
}

void WebManager::handleEventsJson() {
  uint32_t limit = 200;
  if (server_.hasArg("limit")) {
    if (!parsePositiveUIntArg("limit", limit) || limit == 0) {
      sendJsonError(400, "limit must be a positive integer");
      return;
    }
    if (limit > 1000) {
      limit = 1000;
    }
  }

  if (!ensureSdReady()) {
    sendJsonError(503, "SD card unavailable");
    return;
  }

  File file = SD.open(AppConfig::EVENT_FILE_PATH, "r");
  if (!file) {
    sendJsonError(404, "event file not found");
    return;
  }

  const String startTs = server_.arg("start");
  const String endTs = server_.arg("end");
  streamEventsJson(file, startTs, endTs, limit);
  file.close();
}

void WebManager::handleLogsJson() {
  if (!ensureSdReady()) {
    sendJsonError(503, "SD card unavailable");
    return;
  }

  File logsDir = SD.open("/logs");
  if (!logsDir || !logsDir.isDirectory()) {
    sendJsonError(404, "/logs directory not found");
    return;
  }

  server_.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server_.send(200, "application/json", "");
  server_.sendContent("{\"files\":[");

  bool first = true;
  while (true) {
    File f = logsDir.openNextFile();
    if (!f) {
      break;
    }
    if (f.isDirectory()) {
      f.close();
      continue;
    }

    if (!first) {
      server_.sendContent(",");
    }
    first = false;

    String item = "{\"name\":\"" + String(f.name()) + "\",\"size\":" + String(static_cast<unsigned long>(f.size())) +
                  "}";
    server_.sendContent(item);
    f.close();
  }

  logsDir.close();
  server_.sendContent("]}");
}

void WebManager::handleLogDownload() {
  if (!ensureSdReady()) {
    sendJsonError(503, "SD card unavailable");
    return;
  }

  if (!server_.hasArg("file")) {
    sendJsonError(400, "Missing file query parameter");
    return;
  }

  const String path = server_.arg("file");
  if (!path.startsWith("/logs/") || path.indexOf("..") >= 0) {
    sendJsonError(400, "Only /logs/* files are allowed");
    return;
  }

  File file = SD.open(path, "r");
  if (!file) {
    sendJsonError(404, "Requested file not found");
    return;
  }

  const char* contentType = path.endsWith(".csv") ? "text/csv" : "application/octet-stream";
  server_.streamFile(file, contentType);
  file.close();
}

void WebManager::appendIndent(String& out, uint8_t depth) {
  for (uint8_t i = 0; i < depth; ++i) {
    out += "  ";
  }
}

void WebManager::appendSdTree(File entry, String& out, uint8_t depth) {
  while (entry) {
    File child = entry.openNextFile();
    if (!child) {
      break;
    }

    appendIndent(out, depth);
    out += child.isDirectory() ? "[D] " : "[F] ";
    out += child.name();
    if (!child.isDirectory()) {
      out += " (";
      out += String(static_cast<unsigned long>(child.size()));
      out += " bytes)";
    }
    out += "\n";

    if (child.isDirectory()) {
      appendSdTree(child, out, depth + 1);
    }
    child.close();
  }
}

void WebManager::handleSdTreeText() {
  if (!ensureSdReady()) {
    server_.send(200, "text/plain", "SD card unavailable\n");
    return;
  }

  File root = SD.open("/");
  if (!root) {
    server_.send(200, "text/plain", "SD root unavailable\n");
    return;
  }

  String tree;
  tree.reserve(1024);
  tree += "/\n";
  appendSdTree(root, tree, 1);
  root.close();

  server_.send(200, "text/plain", tree);
}

void WebManager::handleFlushNow() {
  if (loggerManager_ == nullptr) {
    sendJsonError(500, "LoggerManager not initialized");
    return;
  }

  const bool success = loggerManager_->flush();
  StaticJsonDocument<128> doc;
  doc["success"] = success;
  doc["message"] = success ? "RAM buffer flushed to SD card" : "Flush failed or queue empty";

  String response;
  serializeJson(doc, response);
  server_.send(200, "application/json", response);
}

void WebManager::handleNtpRetry() {
  if (timeManager_ == nullptr) {
    sendJsonError(500, "TimeManager not initialized");
    return;
  }

  timeManager_->forceNtpRetry();
  StaticJsonDocument<128> doc;
  doc["success"] = true;
  doc["message"] = "NTP sync retry triggered";

  String response;
  serializeJson(doc, response);
  server_.send(200, "application/json", response);
}
