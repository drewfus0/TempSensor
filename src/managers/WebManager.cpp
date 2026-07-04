#include "managers/WebManager.h"

#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <stdlib.h>

#include "config/AppConfig.h"
#include "managers/LoggerManager.h"
#include "managers/TimeManager.h"
#include "web/uplot_assets.h"

namespace {
class FastLineReader {
 public:
  explicit FastLineReader(File& f) : file_(f), bufferLength_(0), bufferIndex_(0), eof_(false) {}

  bool readLine(char* lineBuf, size_t maxLen) {
    size_t lineIndex = 0;
    while (true) {
      if (bufferIndex_ >= bufferLength_) {
        if (eof_) {
          if (lineIndex == 0) return false;
          lineBuf[lineIndex] = '\0';
          return true;
        }
        int bytesRead = file_.read(reinterpret_cast<uint8_t*>(chunk_), sizeof(chunk_));
        if (bytesRead <= 0) {
          eof_ = true;
          if (lineIndex == 0) return false;
          lineBuf[lineIndex] = '\0';
          return true;
        }
        bufferLength_ = bytesRead;
        bufferIndex_ = 0;
      }

      char c = chunk_[bufferIndex_++];
      if (c == '\n') {
        lineBuf[lineIndex] = '\0';
        return true;
      }
      if (c != '\r') {
        if (lineIndex < maxLen - 1) {
          lineBuf[lineIndex++] = c;
        }
      }
    }
  }

 private:
  File& file_;
  char chunk_[512];
  size_t bufferLength_;
  size_t bufferIndex_;
  bool eof_;
};


}  // namespace

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
  server_.on("/sys/uplot.js", HTTP_GET, [this]() { handleLocalUPlotJs(); });
  server_.on("/sys/uplot.css", HTTP_GET, [this]() { handleLocalUPlotCss(); });
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
  <link rel="stylesheet" href="/sys/uplot.css">
  <script src="/sys/uplot.js"></script>
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
    
    .battery-icon-container {
      position: relative;
      width: 28px;
      height: 14px;
      display: flex;
      align-items: center;
    }
    .battery-body {
      width: 25px;
      height: 14px;
      border: 1.5px solid var(--text-sub);
      border-radius: 3px;
      padding: 1px;
      display: flex;
    }
    .battery-fill {
      height: 100%;
      width: 0%;
      background: var(--success);
      border-radius: 1px;
      transition: width 0.3s ease, background-color 0.3s ease;
    }
    .battery-tip {
      width: 3px;
      height: 6px;
      background: var(--text-sub);
      border-radius: 0 1px 1px 0;
    }
    .charging-bolt {
      position: absolute;
      top: -2px;
      left: 10px;
      color: #fbbf24;
      font-size: 10px;
      font-weight: bold;
      text-shadow: 0 0 2px rgba(0,0,0,0.8);
    }
    
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
      height: 500px;
      margin-top: 12px;
      border-radius: 6px;
      background: rgba(0, 0, 0, 0.2);
      overflow: hidden;
    }
    .uplot {
      position: relative;
      font-family: var(--font-main);
      width: 100% !important;
      height: 100% !important;
    }
    .u-legend {
      position: absolute;
      top: 10px;
      right: 10px;
      z-index: 10;
      padding: 6px 12px !important;
      font-size: 0.72rem !important;
      color: var(--text-sub) !important;
      background: rgba(21, 27, 43, 0.85) !important;
      border: 1px solid rgba(255, 255, 255, 0.08) !important;
      border-radius: 6px;
      box-shadow: 0 4px 12px rgba(0, 0, 0, 0.4);
    }
    .u-legend.u-inline tr {
      margin-left: 15px;
      margin-right: 0;
    }
    .u-legend .u-label {
      color: var(--text-sub) !important;
      font-weight: 600;
    }
    .u-legend .u-value {
      font-family: var(--font-mono);
      font-weight: 600;
      color: var(--text) !important;
    }
    .u-tooltip {
      background: rgba(9, 13, 22, 0.95) !important;
      border: 1px solid var(--card-border) !important;
      border-radius: 6px !important;
      color: var(--text) !important;
      font-family: var(--font-main) !important;
      font-size: 0.72rem !important;
    }
    
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
    
    /* Modal / Progress Overlay */
    .modal-overlay {
      position: fixed;
      top: 0;
      left: 0;
      width: 100%;
      height: 100%;
      background: rgba(9, 13, 22, 0.85);
      backdrop-filter: blur(8px);
      -webkit-backdrop-filter: blur(8px);
      display: flex;
      justify-content: center;
      align-items: center;
      z-index: 9999;
      opacity: 0;
      pointer-events: none;
      transition: opacity 0.3s ease;
    }
    .modal-overlay.active {
      opacity: 1;
      pointer-events: auto;
    }
    .modal-content {
      background: var(--card-bg);
      border: 1px solid var(--card-border);
      border-radius: 16px;
      padding: 28px;
      width: 90%;
      max-width: 420px;
      box-shadow: 0 20px 25px -5px rgba(0, 0, 0, 0.5), 0 10px 10px -5px rgba(0, 0, 0, 0.5);
      text-align: center;
      transform: scale(0.9);
      transition: transform 0.3s ease;
    }
    .modal-overlay.active .modal-content {
      transform: scale(1);
    }
    .modal-title {
      font-size: 1.15rem;
      font-weight: 600;
      margin-bottom: 8px;
      letter-spacing: 0.02em;
      color: #fff;
    }
    .modal-subtitle {
      font-size: 0.85rem;
      color: var(--text-sub);
      margin-bottom: 20px;
    }
    .modal-progress-container {
      width: 100%;
      height: 8px;
      background: rgba(255, 255, 255, 0.05);
      border-radius: 4px;
      overflow: hidden;
      margin-bottom: 12px;
    }
    .modal-progress-bar {
      height: 100%;
      background: linear-gradient(90deg, var(--accent) 0%, #3b82f6 100%);
      width: 0%;
      border-radius: 4px;
      transition: width 0.1s linear;
    }
    .modal-percent {
      font-family: var(--font-mono);
      font-size: 0.85rem;
      font-weight: 600;
      color: var(--accent);
    }
  </style>
</head>
<body>
  <div class='modal-overlay' id='progressModal'>
    <div class='modal-content'>
      <div class='modal-title'>Retrieving History Data</div>
      <div class='modal-subtitle' id='modalStatus'>Scanning SD card logs...</div>
      <div class='modal-progress-container'>
        <div class='modal-progress-bar' id='modalProgressBar'></div>
      </div>
      <div class='modal-percent' id='modalProgressPercent'>0%</div>
    </div>
  </div>

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
            <div class='health-item full-width' style='border-top: 1px solid rgba(255,255,255,0.06); padding-top: 12px; margin-top: 4px; background: transparent; border-left: none; border-right: none; border-bottom: none; border-radius: 0;'>
              <div class='health-lbl' style='display:flex; align-items:center; justify-content:space-between;'>
                <span>Battery Status</span>
                <span id='healthBatStatus' style='font-size:0.65rem; color:var(--text-sub);'>-</span>
              </div>
              <div style='display:flex; align-items:center; gap:10px; margin-top:6px;'>
                <div id='batteryIcon' class='battery-icon-container'>
                  <div class='battery-body'>
                    <div id='batteryLevelBar' class='battery-fill'></div>
                  </div>
                  <div class='battery-tip'></div>
                </div>
                <div class='health-val' id='healthBatText' style='margin:0;'>-</div>
              </div>
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
              <label for='range'>Range</label>
              <select id='range'>
                <option value='15m'>Last 15m</option>
                <option value='1h' selected>Last 1h</option>
                <option value='6h'>Last 6h</option>
                <option value='24h'>Last 24h</option>
                <option value='custom'>Custom</option>
              </select>
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

          <div style='display: flex; justify-content: flex-end; gap: 10px; margin-bottom: 8px;'>
            <button id='btnDownloadCsv' style='width: auto; padding: 10px 20px; background-color: var(--bg-card); border: 1px solid var(--border); color: var(--text-main); cursor: pointer;'>Download Filtered CSV</button>
            <button id='btnLoad' style='width: auto; padding: 10px 20px; cursor: pointer;'>Load Graph Data</button>
          </div>
          
          <div class='alert-banner' id='historyAlert' style='margin-bottom: 10px;'></div>

          <div class='chart-container' id='chart-parent'>
            <div id='chart'></div>
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
    
    let uplotInstance = null;
    let historyLoaded = false;
    let historyDataset = [];
    let lastLive = null;
    let lastHealth = null;

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
      const wifi = (health && health.wifi_connected) ? 'UP' : 'DOWN';
      const sd = (health && health.sd_healthy) ? 'OK' : 'BAD';
      const ntp = (health && health.ntp_synced) ? 'SYNC' : 'EST';

      $('pillWifi').textContent = wifi;
      $('pillWifi').style.color = (health && health.wifi_connected) ? 'var(--success)' : 'var(--error)';
      
      $('pillSd').textContent = sd;
      $('pillSd').style.color = (health && health.sd_healthy) ? 'var(--success)' : 'var(--error)';
      
      $('pillNtp').textContent = ntp;
      $('pillNtp').style.color = (health && health.ntp_synced) ? 'var(--success)' : 'var(--warning)';

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
      const container = $('chart-parent');
      const target = $('chart');
      const rect = container.getBoundingClientRect();

      if (uplotInstance) {
        uplotInstance.destroy();
        uplotInstance = null;
      }

      if (!historyDataset || historyDataset.length === 0) {
        target.innerHTML = `<div style="color: var(--text-sub); text-align: center; line-height: 500px; font-size: 13px;">${historyLoaded ? 'No history data in this range.' : 'Select a range and load history to display chart.'}</div>`;
        return;
      }

      const xData = [];
      const tempYData = [];
      const humYData = [];
      const presYData = [];

      for (let i = 0; i < historyDataset.length; i++) {
        const pt = historyDataset[i];
        const epochSec = Date.parse(pt.ts.replace(' ', 'T')) / 1000;
        if (!isNaN(epochSec)) {
          xData.push(epochSec);
          tempYData.push(parseFloat(pt.temp));
          humYData.push(parseFloat(pt.hum));
          presYData.push(parseFloat(pt.pres));
        }
      }

      if (xData.length === 0) {
        target.innerHTML = '<div style="color: var(--text-sub); text-align: center; line-height: 500px; font-size: 13px;">No parseable history data.</div>';
        return;
      }

      const data = [xData, tempYData, humYData, presYData];

      const opts = {
        width: rect.width,
        height: 500,
        title: "",
        class: "uplot-theme",
        cursor: {
          show: true
        },
        select: {
          show: true,
          over: true,
        },
        scales: {
          x: {
            time: true,
          },
          temp: {
            auto: true,
          },
          humidity: {
            auto: true,
            range: [0, 100],
          },
          pressure: {
            auto: true,
          }
        },
        series: [
          {},
          {
            show: true,
            scale: 'temp',
            spanGaps: false,
            label: 'Temperature',
            value: (self, rawValue) => rawValue != null ? rawValue.toFixed(2) + ' °C' : '--',
            stroke: '#f43f5e',
            width: 2,
            fill: 'rgba(244, 63, 94, 0.04)',
          },
          {
            show: true,
            scale: 'humidity',
            spanGaps: false,
            label: 'Humidity',
            value: (self, rawValue) => rawValue != null ? rawValue.toFixed(2) + ' %' : '--',
            stroke: '#06b6d4',
            width: 2,
            fill: 'rgba(6, 182, 212, 0.04)',
          },
          {
            show: true,
            scale: 'pressure',
            spanGaps: false,
            label: 'Pressure',
            value: (self, rawValue) => rawValue != null ? rawValue.toFixed(1) + ' hPa' : '--',
            stroke: '#10b981',
            width: 2,
            fill: 'rgba(16, 185, 129, 0.04)',
          }
        ],
        axes: [
          {
            stroke: "rgba(255, 255, 255, 0.5)",
            grid: {
              show: true,
              stroke: "rgba(255, 255, 255, 0.05)",
              width: 1,
            },
            ticks: {
              show: true,
              stroke: "rgba(255, 255, 255, 0.1)",
              width: 1,
            },
            space: 60,
          },
          {
            scale: 'temp',
            side: 3,
            stroke: "rgba(255, 255, 255, 0.5)",
            grid: {
              show: true,
              stroke: "rgba(255, 255, 255, 0.05)",
              width: 1,
            },
            ticks: {
              show: true,
              stroke: "rgba(255, 255, 255, 0.1)",
              width: 1,
            },
            space: 30,
          },
          {
            scale: 'humidity',
            side: 1,
            stroke: "rgba(255, 255, 255, 0.5)",
            grid: {
              show: false,
            },
            ticks: {
              show: true,
              stroke: "rgba(255, 255, 255, 0.1)",
              width: 1,
            },
            space: 30,
          }
        ]
      };

      target.innerHTML = '';
      uplotInstance = new uPlot(opts, data, target);
    }

    window.addEventListener('resize', () => {
      if (uplotInstance) {
        const container = $('chart-parent');
        const rect = container.getBoundingClientRect();
        uplotInstance.setSize({ width: rect.width, height: 500 });
      }
    });

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

    let isLoadingHistory = false;

    function updateModalProgress(percent, status) {
      $('modalProgressBar').style.width = percent + '%';
      $('modalProgressPercent').textContent = Math.round(percent) + '%';
      if (status) {
        $('modalStatus').textContent = status;
      }
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

      isLoadingHistory = true;
      const modal = $('progressModal');
      modal.classList.add('active');
      updateModalProgress(0, 'Initializing data request...');

      let startDate = null;
      let endDate = null;
      const range = $('range').value;
      const now = new Date();
      if (range === 'custom') {
        const s = $('start').value;
        const e = $('end').value;
        if (s) startDate = new Date(s.replace('T', ' ') + ':00');
        if (e) endDate = new Date(e.replace('T', ' ') + ':00');
      } else {
        const mins = {
          '15m': 15,
          '1h': 60,
          '6h': 360,
          '24h': 1440
        }[range] || 60;
        startDate = new Date(now.getTime() - mins * 60000);
        endDate = now;
      }

      if (!startDate || !endDate || isNaN(startDate.getTime()) || isNaN(endDate.getTime())) {
        alert.textContent = "Invalid date range selected.";
        alert.className = 'alert-banner error';
        alert.style.display = 'block';
        modal.classList.remove('active');
        isLoadingHistory = false;
        return;
      }

      const chunkMs = 1 * 3600 * 1000; // 1 hour in milliseconds (smaller chunk to prevent gaps in data logging)
      const totalMs = endDate.getTime() - startDate.getTime();
      const numChunks = Math.ceil(totalMs / chunkMs);
      
      let allPoints = [];

      try {
        setText('historyMeta', 'Initializing chunked download...');
        
        for (let i = 0; i < numChunks; i++) {
          const chunkStart = new Date(startDate.getTime() + i * chunkMs);
          const chunkEnd = new Date(Math.min(startDate.getTime() + (i + 1) * chunkMs - 1000, endDate.getTime()));
          
          const startStr = fmtLocalTs(chunkStart);
          const endStr = fmtLocalTs(chunkEnd);
          
          updateModalProgress(
            (i / numChunks) * 80 + 10,
            `Downloading chunk ${i + 1}/${numChunks} (${Math.round((i / numChunks) * 100)}%)...`
          );
          
          const url = '/api/history?start=' + encodeURIComponent(startStr) + '&end=' + encodeURIComponent(endStr);
          const res = await fetch(url, { cache: 'no-store' });
          if (!res.ok) {
            throw new Error(`HTTP status ${res.status} on chunk ${i + 1}`);
          }
          
          const startTimestampStr = res.headers.get('X-Start-Timestamp') || startStr;
          const intervalMs = parseInt(res.headers.get('X-Sample-Interval-Ms') || '1000');
          const recordSize = parseInt(res.headers.get('X-Record-Size') || '17');

          const arrayBuffer = await res.arrayBuffer();
          const view = new DataView(arrayBuffer);
          const totalRecords = arrayBuffer.byteLength / recordSize;
          const baseTime = new Date(startTimestampStr.replace(' ', 'T')).getTime();
          
          for (let r = 0; r < totalRecords; r++) {
            const offset = r * recordSize;
            
            const uptime = view.getUint32(offset + 0, true);
            const temp = view.getFloat32(offset + 4, true);
            const hum = view.getFloat32(offset + 8, true);
            const pres = view.getFloat32(offset + 12, true);
            const quality = view.getUint8(offset + 16);

            if (quality === 2) {
              continue; // Empty/unwritten slot
            }

            const recTime = new Date(baseTime + r * intervalMs);
            const recTs = fmtLocalTs(recTime);

            allPoints.push({
              ts: recTs,
              q: quality === 0 ? 'ntp' : 'estimated',
              temp: temp,
              hum: hum,
              pres: pres,
              uptime: uptime
            });
          }
          
          // Yield to give ESP8266 CPU time for background tasks
          await new Promise(resolve => setTimeout(resolve, 50));
        }

        updateModalProgress(90, 'Preparing chart dataset...');

        historyLoaded = true;
        historyDataset = allPoints;
        renderChart();

        setText('historyMeta',
          'Total Logged: ' + allPoints.length +
          ' | Rendered: ' + historyDataset.length);

        updateModalProgress(100, 'Done!');
        await new Promise(resolve => setTimeout(resolve, 250));
      } catch (err) {
        historyDataset = [];
        historyLoaded = true;
        renderChart();
        setText('historyMeta', 'History error: ' + err.message);
        alert.textContent = 'History load failed: ' + err.message;
        alert.className = 'alert-banner error';
        alert.style.display = 'block';
      } finally {
        modal.classList.remove('active');
        setTimeout(() => {
          isLoadingHistory = false;
        }, 100);
      }
    }

    function downloadFilteredCSV() {
      if (!historyDataset || historyDataset.length === 0) {
        alert("No history data loaded to download. Click Load Graph Data first.");
        return;
      }
      
      let csv = 'timestamp,timestamp_quality,temp_c,humidity_pct,pressure_hpa,uptime_s\n';
      for (const p of historyDataset) {
        csv += p.ts + ',' + p.q + ',' + p.temp.toFixed(2) + ',' + p.hum.toFixed(2) + ',' + p.pres.toFixed(2) + ',' + p.uptime + '\n';
      }
      
      const blob = new Blob([csv], { type: 'text/csv;charset=utf-8;' });
      const link = document.createElement('a');
      const url = URL.createObjectURL(blob);
      link.setAttribute('href', url);
      link.setAttribute('download', 'filtered_log.csv');
      link.style.visibility = 'hidden';
      document.body.appendChild(link);
      link.click();
      document.body.removeChild(link);
    }

    async function loadLogs() {
      if (isLoadingHistory) return;
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
      if (isLoadingHistory) return;
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

    async function loadLive() {
      if (isLoadingHistory) return;
      try {
        const live = await fetchJson('/api/live');
        if (live && live.has_sample) {
          $('valTemp').textContent = parseFloat(live.temp_c).toFixed(1) + ' °C';
          $('valHum').textContent = parseFloat(live.humidity_pct).toFixed(1) + ' %';
          $('valPres').textContent = parseFloat(live.pressure_hpa).toFixed(1) + ' hPa';
          $('liveTime').textContent = 'Timestamp: ' + live.timestamp;
          $('liveQuality').textContent = live.timestamp_quality;
          $('liveQuality').style.color = live.timestamp_quality === 'ntp' ? 'var(--success)' : 'var(--warning)';
        }
        lastLive = live;
        updatePills(lastLive, lastHealth);
      } catch (err) {
        console.error("Live fetch error", err);
      }
    }

    async function loadHealth() {
      if (isLoadingHistory) return;
      try {
        const health = await fetchJson('/api/health');
        if (health && health.has_health) {
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

          if (health.battery) {
            const bat = health.battery;
            const percent = bat.percent;
            const voltage = bat.voltage.toFixed(2);
            
            let timeText = "";
            if (bat.status === "Full") {
              timeText = "Full (External Power)";
            } else if (bat.status === "Charging / USB") {
              timeText = "Charging via USB";
            } else {
              const tRemaining = bat.time_remaining;
              if (tRemaining > 0) {
                const tHrs = Math.floor(tRemaining / 3600);
                const tMins = Math.floor((tRemaining % 3600) / 60);
                timeText = `${tHrs}h ${tMins}m remaining (Discharging)`;
              } else {
                timeText = "Discharging";
              }
            }
            
            $('healthBatStatus').textContent = timeText;
            $('healthBatText').textContent = percent + '% (' + voltage + ' V)';
            
            const fill = $('batteryLevelBar');
            fill.style.width = percent + '%';
            
            if (percent > 50) {
              fill.style.backgroundColor = 'var(--success)';
            } else if (percent > 20) {
              fill.style.backgroundColor = '#fbbf24';
            } else {
              fill.style.backgroundColor = 'var(--error)';
            }
            
            const container = $('batteryIcon');
            let bolt = container.querySelector('.charging-bolt');
            if (bat.status === "Charging / USB") {
              if (!bolt) {
                bolt = document.createElement('div');
                bolt.className = 'charging-bolt';
                bolt.innerHTML = '⚡';
                container.appendChild(bolt);
              }
            } else {
              if (bolt) {
                bolt.remove();
              }
            }
          }
        }
        lastHealth = health;
        updatePills(lastLive, lastHealth);
      } catch (err) {
        console.error("Health fetch error", err);
      }
    }

    async function loadConfig() {
      if (isLoadingHistory) return;
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
      if (isLoadingHistory) return;
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
      if (isLoadingHistory) return;
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
      if (isLoadingHistory) return;
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
    $('btnDownloadCsv').addEventListener('click', downloadFilteredCSV);
    $('range').addEventListener('change', onRangeChanged);

    (async function boot() {
      onRangeChanged();
      await loadConfig();
      await loadStaticPanels();
      await loadLive();
      await loadHealth();
      await loadLogs();
      await loadEvents();
      renderChart();

      // Staggered polling intervals to spread the load on ESP8266
      setTimeout(() => {
        setInterval(loadLive, 30000);
      }, 30000); // Live: Slot 0 (starts at 30s)

      setTimeout(() => {
        setInterval(loadHealth, 60000);
      }, 10000); // Health: Slot 10 (starts at 70s)

      setTimeout(() => {
        setInterval(loadEvents, 60000);
      }, 20000); // Events: Slot 20 (starts at 80s)

      setTimeout(() => {
        setInterval(loadLogs, 60000);
      }, 40000); // Logs: Slot 40 (starts at 100s)

      setTimeout(() => {
        setInterval(loadStaticPanels, 60000);
      }, 50000); // Static panels: Slot 50 (starts at 110s)
    })();
  </script>
</body>
</html>
)HTML";

  server_.send_P(200, "text/html", html);
}

void WebManager::handleLocalUPlotJs() {
  server_.sendHeader("Content-Encoding", "gzip");
  server_.send_P(200, "application/javascript", (const char*)UPLOT_JS_GZ, UPLOT_JS_GZ_LEN);
}

void WebManager::handleLocalUPlotCss() {
  server_.sendHeader("Content-Encoding", "gzip");
  server_.send_P(200, "text/css", (const char*)UPLOT_CSS_GZ, UPLOT_CSS_GZ_LEN);
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
  StaticJsonDocument<768> doc;

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
    
    JsonObject battery = doc.createNestedObject("battery");
    battery["voltage"] = health_->batteryVoltage;
    battery["percent"] = health_->batteryPercent;
    battery["status"] = health_->batteryStatus;
    battery["time_remaining"] = health_->batteryTimeRemainingSeconds;
    
    doc["has_health"] = true;
  } else {
    doc["has_health"] = false;
  }

  String response;
  serializeJson(doc, response);
  server_.send(200, "application/json", response);
}

bool WebManager::ensureSdReady() {
  if (loggerManager_ != nullptr) {
    if (loggerManager_->isSdHealthy()) {
      return true;
    }
    // Try a manual forced recovery retry immediately
    return loggerManager_->forceRetry();
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
    
    if (yieldCallback_) {
      yieldCallback_(yieldCallbackArg_);
    }

    if (emitted >= limit) {
      break;
    }
  }

  server_.sendContent("],\"count\":");
  server_.sendContent(String(emitted));
  server_.sendContent("}");
}

void WebManager::handleHistoryJson() {
  if (!ensureSdReady()) {
    sendJsonError(503, "SD card unavailable");
    return;
  }

  const String startTs = server_.arg("start");
  const String endTs = server_.arg("end");

  if (startTs.length() < 19 || endTs.length() < 19) {
    sendJsonError(400, "start and end must be YYYY-MM-DD HH:MM:SS");
    return;
  }

  String startDay = startTs.substring(0, 10);
  String endDay = endTs.substring(0, 10);

  int startHour = startTs.substring(11, 13).toInt();
  int startMin = startTs.substring(14, 16).toInt();
  int startSec = startTs.substring(17, 19).toInt();
  uint32_t startSlot = startHour * 3600 + startMin * 60 + startSec;

  int endHour = endTs.substring(11, 13).toInt();
  int endMin = endTs.substring(14, 16).toInt();
  int endSec = endTs.substring(17, 19).toInt();
  uint32_t endSlot = endHour * 3600 + endMin * 60 + endSec;

  uint32_t totalRecords = 0;
  if (startDay == endDay) {
    if (endSlot >= startSlot) {
      totalRecords = endSlot - startSlot + 1;
    }
  } else {
    totalRecords = (86400 - startSlot) + (endSlot + 1);
  }

  uint32_t totalBytes = totalRecords * sizeof(LogRecord);

  server_.setContentLength(totalBytes);
  server_.sendHeader("X-Start-Timestamp", startTs);
  server_.sendHeader("X-Sample-Interval-Ms", "1000");
  server_.sendHeader("X-Record-Size", String(sizeof(LogRecord)));
  server_.send(200, "application/octet-stream", "");

  uint8_t buffer[512];
  uint32_t recordsToRead = totalRecords;
  uint32_t currentSlot = startSlot;
  String currentDay = startDay;

  while (recordsToRead > 0) {
    String filepath = "/logs/" + currentDay + ".bin";
    File file = SD.open(filepath, "r");

    uint32_t limitSlots = (currentDay == startDay && startDay != endDay) ? (86400 - startSlot) : 
                          ((currentDay == endDay) ? (endSlot - currentSlot + 1) : (endSlot - currentSlot + 1));
    if (limitSlots > recordsToRead) {
      limitSlots = recordsToRead;
    }

    if (!file) {
      LogRecord emptyRecord;
      memset(&emptyRecord, 0, sizeof(LogRecord));
      emptyRecord.quality = 2; // Empty

      uint32_t slotsLeft = limitSlots;
      while (slotsLeft > 0) {
        uint32_t chunkSlots = sizeof(buffer) / sizeof(LogRecord);
        if (chunkSlots > slotsLeft) {
          chunkSlots = slotsLeft;
        }

        for (uint32_t i = 0; i < chunkSlots; ++i) {
          memcpy(buffer + i * sizeof(LogRecord), &emptyRecord, sizeof(LogRecord));
        }

        server_.client().write(buffer, chunkSlots * sizeof(LogRecord));
        slotsLeft -= chunkSlots;
        recordsToRead -= chunkSlots;

        if (yieldCallback_) {
          yieldCallback_(yieldCallbackArg_);
        }
      }
    } else {
      file.seek(currentSlot * sizeof(LogRecord));
      uint32_t slotsLeft = limitSlots;

      while (slotsLeft > 0) {
        uint32_t chunkSlots = sizeof(buffer) / sizeof(LogRecord);
        if (chunkSlots > slotsLeft) {
          chunkSlots = slotsLeft;
        }

        size_t bytesToRead = chunkSlots * sizeof(LogRecord);
        size_t bytesRead = file.read(buffer, bytesToRead);

        if (bytesRead < bytesToRead) {
          LogRecord emptyRecord;
          memset(&emptyRecord, 0, sizeof(LogRecord));
          emptyRecord.quality = 2;

          for (size_t offset = bytesRead; offset < bytesToRead; offset += sizeof(LogRecord)) {
            memcpy(buffer + offset, &emptyRecord, sizeof(LogRecord));
          }
        }

        server_.client().write(buffer, bytesToRead);
        slotsLeft -= chunkSlots;
        recordsToRead -= chunkSlots;

        if (yieldCallback_) {
          yieldCallback_(yieldCallbackArg_);
        }
      }
      file.close();
    }

    if (currentDay == startDay && startDay != endDay) {
      currentDay = endDay;
      currentSlot = 0;
    } else {
      break;
    }
  }
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
