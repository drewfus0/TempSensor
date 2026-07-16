#include "managers/WebManager.h"

#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <stdlib.h>

#include "config/AppConfig.h"
#include "managers/LoggerManager.h"
#include "managers/TimeManager.h"
#include "managers/DisplayManager.h"
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

bool WebManager::begin(DeviceConfig* config, LoggerManager* logger, TimeManager* time, DisplayManager* display) {
  config_ = config;
  loggerManager_ = logger;
  timeManager_ = time;
  displayManager_ = display;

  // Collect headers for Content-Length to display OTA progress
  server_.collectHeaders("Content-Length");

  WiFi.mode(WIFI_STA);
  WiFi.setHostname(config_->hostname);
  WiFi.begin(config_->wifiSsid, config_->wifiPassword);

  Serial.printf("[WiFi] Connecting to %s", config_->wifiSsid);
  const uint32_t startMs = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - startMs) < 15000) {
    delay(250);
    Serial.print('.');
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("[WiFi] Connected, IP: %s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("[WiFi] Connection timeout. Starting Fallback AP...");
    startAPFallback();
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
  server_.on("/", [this]() { logRequest(); handleRoot(); });
  server_.on("/sys/uplot.js", HTTP_GET, [this]() { logRequest(); handleLocalUPlotJs(); });
  server_.on("/sys/uplot.css", HTTP_GET, [this]() { logRequest(); handleLocalUPlotCss(); });
  server_.on("/api/live", [this]() { logRequest(); handleLiveJson(); });
  server_.on("/api/health", [this]() { logRequest(); handleHealthJson(); });
  server_.on("/api/config", HTTP_GET, [this]() { logRequest(); handleConfigGet(); });
  server_.on("/api/config", HTTP_POST, [this]() { logRequest(); handleConfigPost(); });
  server_.on("/api/history", HTTP_GET, [this]() { logRequest(); handleHistoryJson(); });
  server_.on("/api/events", HTTP_GET, [this]() { logRequest(); handleEventsJson(); });
  server_.on("/api/logs", HTTP_GET, [this]() { logRequest(); handleLogsJson(); });
  server_.on("/api/logs/download", HTTP_GET, [this]() { logRequest(); handleLogDownload(); });
  server_.on("/api/logs/delete", HTTP_POST, [this]() { logRequest(); handleLogDelete(); });
  server_.on("/api/logs/rename", HTTP_POST, [this]() { logRequest(); handleLogRename(); });
  server_.on("/api/sd-tree", [this]() { logRequest(); handleSdTreeJson(); });
  server_.on("/api/action/flush-now", HTTP_POST, [this]() { logRequest(); handleFlushNow(); });
  server_.on("/api/action/ntp-retry", HTTP_POST, [this]() { logRequest(); handleNtpRetry(); });
  server_.on("/api/update", HTTP_POST, [this]() { logRequest(); handleOtaUpdatePost(); }, [this]() { handleOtaUpdateUpload(); });
}

void WebManager::logRequest() {
  Serial.printf("[Web] %s %s from %s\n",
                (server_.method() == HTTP_GET) ? "GET" : "POST",
                server_.uri().c_str(),
                server_.client().remoteIP().toString().c_str());
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
      padding: 0 20px 20px 20px;
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
      margin-bottom: 12px;
      padding-bottom: 8px;
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
    
    .header-container {
      position: sticky;
      top: 0;
      z-index: 100;
      background: rgba(9, 13, 22, 0.95);
      backdrop-filter: blur(12px);
      -webkit-backdrop-filter: blur(12px);
      border-bottom: 1px solid var(--card-border);
      padding: 20px 20px 0 20px;
      margin-bottom: 24px;
    }
    .countdown-bar {
      display: flex;
      justify-content: flex-start;
      gap: 16px;
      padding: 8px 16px;
      background: rgba(255, 255, 255, 0.02);
      border-radius: 6px;
      margin-bottom: 16px;
      font-size: 0.75rem;
      color: var(--text-sub);
      flex-wrap: wrap;
      border: 1px solid var(--card-border);
    }
    .countdown-item {
      display: flex;
      align-items: center;
      gap: 6px;
    }
    .countdown-val {
      font-weight: 600;
      color: var(--accent);
      font-family: var(--font-mono);
    }

    /* Tab System Styles */
    .tab-bar {
      display: flex;
      gap: 12px;
      margin-bottom: 0px;
      border-bottom: none;
      padding-bottom: 8px;
    }
    .tab-btn {
      background: none;
      border: none;
      color: var(--text-sub);
      font-family: var(--font-main);
      font-size: 1rem;
      font-weight: 500;
      cursor: pointer;
      padding: 6px 12px;
      border-bottom: 2px solid transparent;
      transition: color 0.2s ease, border-color 0.2s ease;
    }
    .tab-btn:hover {
      color: var(--text);
    }
    .tab-btn.active {
      color: var(--accent);
      border-bottom: 2px solid var(--accent);
    }
    .tab-content {
      display: none;
      animation: fadeIn 0.3s ease;
    }
    .tab-content.active {
      display: block;
    }
    @keyframes fadeIn {
      from { opacity: 0; transform: translateY(4px); }
      to { opacity: 1; transform: translateY(0); }
    }
    
    /* Config Form Styles */
    .form-group {
      display: flex;
      flex-direction: column;
      gap: 6px;
      margin-bottom: 16px;
    }
    .form-group label {
      font-size: 0.85rem;
      color: var(--text-sub);
      font-weight: 500;
    }
    .form-control {
      background: rgba(0, 0, 0, 0.3);
      border: 1px solid var(--card-border);
      border-radius: 6px;
      padding: 10px;
      color: var(--text);
      font-family: var(--font-main);
      font-size: 0.9rem;
      width: 100%;
      transition: border-color 0.2s;
    }
    .form-control:focus {
      outline: none;
      border-color: var(--accent);
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

  <div class='header-container'>
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

      <div class='countdown-bar'>
        <div class='countdown-item'><span>Next Live:</span><span class='countdown-val' id='cntLive'>--s</span></div>
        <div class='countdown-item'><span>Next Health:</span><span class='countdown-val' id='cntHealth'>--s</span></div>
        <div class='countdown-item'><span>Next Events:</span><span class='countdown-val' id='cntEvents'>--s</span></div>
        <div class='countdown-item'><span>Next SD Tree:</span><span class='countdown-val' id='cntSdTree'>--s</span></div>
        <div class='countdown-item'><span>Next Battery:</span><span class='countdown-val' id='cntBattery'>--s</span></div>
      </div>

      <div class='tab-bar'>
        <button class='tab-btn active' onclick="showTab('dashboard')" id='tab-dashboard'>Dashboard</button>
        <button class='tab-btn' onclick="showTab('files')" id='tab-files'>File Manager</button>
        <button class='tab-btn' onclick="showTab('settings')" id='tab-settings'>Settings</button>
      </div>
    </div>
  </div>

  <div class='wrap'>

    <div id='content-dashboard' class='tab-content active'>
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
          <h2>Manual Diagnostics</h2>
          <div class='btn-group' style='display: flex; gap: 10px; margin-top: 10px;'>
            <button class='btn-secondary' id='btnFlush' title='Flush RAM buffer to SD card' style='flex: 1; padding: 10px; background: rgba(255,255,255,0.05); color: var(--text); border: 1px solid var(--card-border); border-radius: 6px; cursor: pointer; font-weight: 500;'>Force Flush</button>
            <button class='btn-secondary' id='btnNtp' title='Force immediate NTP time sync attempt' style='flex: 1; padding: 10px; background: rgba(255,255,255,0.05); color: var(--text); border: 1px solid var(--card-border); border-radius: 6px; cursor: pointer; font-weight: 500;'>Sync NTP</button>
          </div>
          <div class='alert-banner' id='actionAlert' style='margin-top: 10px; display: none;'></div>
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
            <div class='form-group'>
              <label for='binSize'>Bin Size <span id='binSizeMeta' style='font-size: 11px; color: var(--text-sub); margin-left: 4px;'></span></label>
              <select id='binSize'>
                <option value='auto' selected>Auto</option>
                <option value='10'>10s</option>
                <option value='30'>30s</option>
                <option value='60'>1m</option>
                <option value='300'>5m</option>
                <option value='600'>10m</option>
                <option value='1800'>30m</option>
                <option value='3600'>1h</option>
                <option value='custom'>Custom</option>
              </select>
            </div>
            <div class='form-group' id='customBinGroup' style='display: none;'>
              <label for='customBinVal'>Seconds</label>
              <input id='customBinVal' type='number' min='1' max='86400' value='60' style='width: 100%;'>
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

        <section class='card' style='margin-top: 20px;'>
          <div style='display: flex; justify-content: space-between; align-items: center; margin-bottom: 12px;'>
            <h2 style='margin: 0;'>Battery Voltage History (48h)</h2>
            <button class='btn' id='btnDownloadBatCsv' style='padding: 6px 12px; font-size: 12px; margin: 0;'>Download CSV</button>
          </div>
          <div class='chart-container' id='batChartParent' style='height: 250px;'>
            <div id='batChart'></div>
          </div>
          <div class='live-meta' id='batChartMeta' style='margin-top: 12px; text-align: left;'>
            No battery history loaded.
          </div>
        </section>
      </div> <!-- End right column -->
    </div> <!-- End grid-main -->
  </div> <!-- End content-dashboard -->

    <div id='content-files' class='tab-content'>
      <section class='card'>
        <h2>SD File Explorer</h2>
        <div id='sdtree' style='background: rgba(0, 0, 0, 0.25); border: 1px solid var(--card-border); border-radius: 8px; padding: 10px; font-family: var(--font-mono); font-size: 0.78rem; line-height: 1.4; color: #a5f3fc; white-space: normal;'>loading...</div>
      </section>
    </div> <!-- End content-files -->

    <div id='content-settings' class='tab-content'>
      <section class='card' style='max-width: 600px; margin: 0 auto;'>
        <h2>System Configuration</h2>
        <form id='settingsForm' onsubmit='saveSettings(event)' style='display: flex; flex-direction: column; gap: 16px;'>
          
          <div class='form-group'>
            <label for='cfgWifiSsid'>Wi-Fi SSID</label>
            <input type='text' id='cfgWifiSsid' class='form-control' required>
          </div>
          
          <div class='form-group'>
            <label for='cfgWifiPassword'>Wi-Fi Password</label>
            <input type='password' id='cfgWifiPassword' class='form-control' placeholder='••••••••'>
            <span style='font-size: 0.75rem; color: var(--text-sub);'>Leave blank to keep current password</span>
          </div>
          
          <div class='form-group'>
            <label for='cfgHostname'>Hostname</label>
            <input type='text' id='cfgHostname' class='form-control' required>
          </div>
          
          <div class='form-group'>
            <label for='cfgTimezone'>Timezone Configuration</label>
            <input type='text' id='cfgTimezone' class='form-control' required>
            <span style='font-size: 0.75rem; color: var(--text-sub);'>e.g., AEST-10AEDT,M10.1.0,M4.1.0/3 (Melbourne)</span>
          </div>
          
          <div style='display: grid; grid-template-columns: 1fr 1fr; gap: 16px;'>
            <div class='form-group'>
              <label for='cfgSampleInterval'>Sample Interval (ms)</label>
              <input type='number' id='cfgSampleInterval' class='form-control' min='500' max='60000' required>
            </div>
            <div class='form-group'>
              <label for='cfgLogFlushInterval'>Log Flush Interval (ms)</label>
              <input type='number' id='cfgLogFlushInterval' class='form-control' min='5000' max='3600000' required>
            </div>
          </div>

          <div class='form-group'>
            <label for='cfgDisplayRefreshInterval'>Display Refresh Interval (ms)</label>
            <input type='number' id='cfgDisplayRefreshInterval' class='form-control' min='500' max='60000' required>
          </div>

          <button type='submit' class='btn' style='margin-top: 12px; padding: 12px; background: var(--accent); color: var(--text); border: none; border-radius: 6px; font-weight: 600; cursor: pointer; transition: background 0.2s;'>Save & Apply Settings</button>
        </form>
      </section>

      <section class='card' style='max-width: 600px; margin: 20px auto 0 auto;'>
        <h2>Firmware Update (OTA)</h2>
        <div class='form-group'>
          <label for='otaFile'>Select Firmware Binary (.bin)</label>
          <input type='file' id='otaFile' accept='.bin' style='display: none;'>
          <div id='otaDragDrop' style='border: 2px dashed var(--card-border); padding: 20px; text-align: center; border-radius: 6px; cursor: pointer; background: rgba(255,255,255,0.02); transition: all 0.2s; margin-top: 8px;'>
            <span id='otaDragText'>Drag & drop or click to select file</span>
          </div>
        </div>
        <div id='otaProgressContainer' style='display: none; margin-top: 15px;'>
          <div style='display: flex; justify-content: space-between; margin-bottom: 5px; font-size: 13px;'>
            <span id='otaStatus'>Uploading...</span>
            <span id='otaPercent'>0%</span>
          </div>
          <div style='background: rgba(255,255,255,0.1); height: 10px; border-radius: 5px; overflow: hidden;'>
            <div id='otaProgressBar' style='background: var(--accent); width: 0%; height: 100%; transition: width 0.1s; border-radius: 5px;'></div>
          </div>
        </div>
        <button id='btnStartOta' class='btn' style='margin-top: 15px; width: 100%; padding: 12px; background: var(--accent); color: var(--text); border: none; border-radius: 6px; font-weight: 600; cursor: pointer;' disabled>Flash Firmware</button>
        <div class='alert-banner' id='otaAlert' style='margin-top: 10px; display: none;'></div>
      </section>

      <section class='card' style='max-width: 600px; margin: 20px auto 0 auto;'>
        <h2>Web Telemetry Intervals</h2>
        <div style='display: flex; flex-direction: column; gap: 16px;'>
          <div class='form-group'>
            <label for='uiIntervalLive'>Live Telemetry Interval (seconds)</label>
            <input type='number' id='uiIntervalLive' class='form-control' min='1' max='300' value='15'>
          </div>
          <div class='form-group'>
            <label for='uiIntervalHealth'>System Health Interval (seconds)</label>
            <input type='number' id='uiIntervalHealth' class='form-control' min='5' max='600' value='20'>
          </div>
          <div class='form-group'>
            <label for='uiIntervalEvents'>Events List Interval (seconds)</label>
            <input type='number' id='uiIntervalEvents' class='form-control' min='5' max='3600' value='60'>
          </div>
          <div class='form-group'>
            <label for='uiIntervalSdTree'>SD Explorer Interval (seconds)</label>
            <input type='number' id='uiIntervalSdTree' class='form-control' min='10' max='3600' value='60'>
          </div>
          <div class='form-group'>
            <label for='uiIntervalBattery'>Battery History Interval (seconds)</label>
            <input type='number' id='uiIntervalBattery' class='form-control' min='10' max='3600' value='60'>
          </div>
          <button id='btnSaveUiIntervals' class='btn' style='margin-top: 12px; padding: 12px; background: var(--accent); color: var(--text); border: none; border-radius: 6px; font-weight: 600; cursor: pointer; transition: background 0.2s;'>Update Fetch Rates</button>
        </div>
      </section>
    </div> <!-- End content-settings -->
  </div> <!-- End wrap -->

  <script>
    const $ = (id) => document.getElementById(id);
    
    let uplotInstance = null;
    let batChartInstance = null;
    let loadedBatteryData = [];
    let tasks = [];
    let schedulerTimer = null;

    function initTasks() {
      const now = Date.now();
      tasks = [
        { name: 'sdtree', intervalKey: 'uiIntervalSdTree', action: loadSdTree, lastRun: now, priority: 5 },
        { name: 'battery', intervalKey: 'uiIntervalBattery', action: loadBatteryHistory, lastRun: now, priority: 4 },
        { name: 'events', intervalKey: 'uiIntervalEvents', action: loadEvents, lastRun: now, priority: 3 },
        { name: 'health', intervalKey: 'uiIntervalHealth', action: loadHealth, lastRun: now, priority: 2 },
        { name: 'live', intervalKey: 'uiIntervalLive', action: loadLive, lastRun: now, priority: 1 }
      ];
    }

    function getTaskDefault(key) {
      if (key === 'uiIntervalLive') return 10;
      if (key === 'uiIntervalHealth') return 20;
      if (key === 'uiIntervalEvents') return 30;
      if (key === 'uiIntervalSdTree') return 120;
      if (key === 'uiIntervalBattery') return 50;
      return 60;
    }

    function updateCountdowns() {
      const now = Date.now();
      if (tasks.length === 0) return;
      
      for (const t of tasks) {
        const intervalSec = parseInt($(t.intervalKey).value) || getTaskDefault(t.intervalKey);
        const intervalMs = intervalSec * 1000;
        const elapsed = now - t.lastRun;
        const diffMs = intervalMs - elapsed;
        const rem = diffMs >= 0 ? Math.ceil(diffMs / 1000) : Math.floor(diffMs / 1000);
        
        let displayId = '';
        if (t.name === 'live') displayId = 'cntLive';
        else if (t.name === 'health') displayId = 'cntHealth';
        else if (t.name === 'events') displayId = 'cntEvents';
        else if (t.name === 'sdtree') displayId = 'cntSdTree';
        else if (t.name === 'battery') displayId = 'cntBattery';
        
        if (displayId) {
          $(displayId).textContent = rem + 's';
        }
      }
    }

    setInterval(updateCountdowns, 1000);

    function loadUiIntervals() {
      $('uiIntervalLive').value = localStorage.getItem('uiIntervalLive') || 10;
      $('uiIntervalHealth').value = localStorage.getItem('uiIntervalHealth') || 20;
      $('uiIntervalEvents').value = localStorage.getItem('uiIntervalEvents') || 30;
      $('uiIntervalSdTree').value = localStorage.getItem('uiIntervalSdTree') || 120;
      $('uiIntervalBattery').value = localStorage.getItem('uiIntervalBattery') || 50;
    }

    function saveUiIntervals() {
      localStorage.setItem('uiIntervalLive', $('uiIntervalLive').value);
      localStorage.setItem('uiIntervalHealth', $('uiIntervalHealth').value);
      localStorage.setItem('uiIntervalEvents', $('uiIntervalEvents').value);
      localStorage.setItem('uiIntervalSdTree', $('uiIntervalSdTree').value);
      localStorage.setItem('uiIntervalBattery', $('uiIntervalBattery').value);
      
      const now = Date.now();
      for (const t of tasks) {
        t.lastRun = now;
      }
      
      startScheduler();
      alert("Telemetry fetch rates updated successfully!");
    }

    function startScheduler(staggered = false) {
      if (schedulerTimer) clearInterval(schedulerTimer);
      
      const now = Date.now();
      if (staggered && tasks.length > 0) {
        for (const t of tasks) {
          const intervalSec = parseInt($(t.intervalKey).value) || getTaskDefault(t.intervalKey);
          let offsetSec = 0;
          if (t.name === 'live') offsetSec = 5;
          else if (t.name === 'health') offsetSec = 10;
          else if (t.name === 'events') offsetSec = 20;
          else if (t.name === 'sdtree') offsetSec = 40;
          else if (t.name === 'battery') offsetSec = 45;
          t.lastRun = now - (intervalSec - offsetSec) * 1000;
        }
      }
      
      schedulerTimer = setInterval(schedulerTick, 10000);
    }

    async function schedulerTick() {
      if (isLoadingHistory) return;
      
      const now = Date.now();
      const dueTasks = [];

      for (const t of tasks) {
        const intervalSec = parseInt($(t.intervalKey).value) || getTaskDefault(t.intervalKey);
        const intervalMs = intervalSec * 1000;
        if (now - t.lastRun >= intervalMs) {
          dueTasks.push(t);
        }
      }

      if (dueTasks.length === 0) return;

      dueTasks.sort((a, b) => b.priority - a.priority);

      const taskToRun = dueTasks[0];
      taskToRun.lastRun = now;
      
      try {
        await taskToRun.action();
      } catch (err) {
        console.error(`Scheduler failed to run ${taskToRun.name}:`, err);
      }
    }

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

    function formatAvgMinMax(self, val, seriesIdx, dataIdx, suffix, decimals) {
      if (val == null) return '--';
      const idx = dataIdx !== null ? dataIdx : self.data[0].length - 1;
      const avgVal = self.data[seriesIdx][idx];
      const minVal = self.data[seriesIdx - 2][idx];
      const maxVal = self.data[seriesIdx - 1][idx];
      return `Avg: ${avgVal != null ? avgVal.toFixed(decimals) : '--'}${suffix} | Min: ${minVal != null ? minVal.toFixed(decimals) : '--'}${suffix} | Max: ${maxVal != null ? maxVal.toFixed(decimals) : '--'}${suffix}`;
    }

    function aggregateData(dataset, binSizeSeconds) {
      if (!dataset || dataset.length === 0) return [];

      const parsed = [];
      for (let i = 0; i < dataset.length; i++) {
        const pt = dataset[i];
        const epochSec = Date.parse(pt.ts.replace(' ', 'T')) / 1000;
        if (!isNaN(epochSec)) {
          parsed.push({
            ts: epochSec,
            temp: parseFloat(pt.temp),
            hum: parseFloat(pt.hum),
            pres: parseFloat(pt.pres)
          });
        }
      }

      if (parsed.length === 0) return [];
      parsed.sort((a, b) => a.ts - b.ts);

      const startTs = parsed[0].ts;
      const endTs = parsed[parsed.length - 1].ts;
      
      const bins = [];
      let currentBinStart = startTs;
      let currentBinData = [];

      for (let i = 0; i < parsed.length; i++) {
        const pt = parsed[i];
        while (pt.ts >= currentBinStart + binSizeSeconds) {
          if (currentBinData.length > 0) {
            bins.push({
              ts: currentBinStart + binSizeSeconds / 2, // middle of the bin
              data: currentBinData
            });
            currentBinData = [];
          }
          currentBinStart += binSizeSeconds;
        }
        currentBinData.push(pt);
      }

      if (currentBinData.length > 0) {
        bins.push({
          ts: currentBinStart + binSizeSeconds / 2,
          data: currentBinData
        });
      }

      const result = [];
      for (let i = 0; i < bins.length; i++) {
        const bin = bins[i];
        const d = bin.data;
        let tSum = 0, hSum = 0, pSum = 0;
        let tMin = d[0].temp, tMax = d[0].temp;
        let hMin = d[0].hum, hMax = d[0].hum;
        let pMin = d[0].pres, pMax = d[0].pres;

        for (let j = 0; j < d.length; j++) {
          const val = d[j];
          tSum += val.temp;
          hSum += val.hum;
          pSum += val.pres;

          if (val.temp < tMin) tMin = val.temp;
          if (val.temp > tMax) tMax = val.temp;
          if (val.hum < hMin) hMin = val.hum;
          if (val.hum > hMax) hMax = val.hum;
          if (val.pres < pMin) pMin = val.pres;
          if (val.pres > pMax) pMax = val.pres;
        }

        result.push({
          ts: bin.ts,
          temp: { min: tMin, max: tMax, avg: tSum / d.length },
          hum: { min: hMin, max: hMax, avg: hSum / d.length },
          pres: { min: pMin, max: pMax, avg: pSum / d.length }
        });
      }

      return result;
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

      // 1. Calculate bin size
      let binSizeSeconds = 10;
      const binSel = $('binSize').value;
      if (binSel === 'auto') {
        const range = $('range').value;
        if (range === '15m') binSizeSeconds = 10;
        else if (range === '1h') binSizeSeconds = 30;
        else if (range === '6h') binSizeSeconds = 180;
        else if (range === '24h') binSizeSeconds = 600;
        else if (range === 'custom') {
          const startVal = $('start').value;
          const endVal = $('end').value;
          if (startVal && endVal) {
            const diffMs = new Date(endVal) - new Date(startVal);
            binSizeSeconds = Math.max(10, Math.floor(diffMs / (150 * 1000)));
          }
        }
        
        let autoStr = binSizeSeconds + 's';
        if (binSizeSeconds >= 3600) {
          autoStr = (binSizeSeconds / 3600).toFixed(1) + 'h';
        } else if (binSizeSeconds >= 60) {
          autoStr = (binSizeSeconds / 60).toFixed(1) + 'm';
        }
        $('binSizeMeta').textContent = `(Auto: ${autoStr})`;
      } else if (binSel === 'custom') {
        const customVal = parseInt($('customBinVal').value);
        binSizeSeconds = !isNaN(customVal) && customVal > 0 ? customVal : 60;
        $('binSizeMeta').textContent = '(Custom)';
      } else {
        binSizeSeconds = parseInt(binSel);
        $('binSizeMeta').textContent = '';
      }

      // 2. Perform aggregation
      const aggregated = aggregateData(historyDataset, binSizeSeconds);

      if (aggregated.length === 0) {
        target.innerHTML = '<div style="color: var(--text-sub); text-align: center; line-height: 500px; font-size: 13px;">No parseable history data.</div>';
        return;
      }

      const xData = [];
      const tempMin = [];
      const tempMax = [];
      const tempAvg = [];
      const humMin = [];
      const humMax = [];
      const humAvg = [];
      const presMin = [];
      const presMax = [];
      const presAvg = [];

      for (let i = 0; i < aggregated.length; i++) {
        const pt = aggregated[i];
        xData.push(pt.ts);
        tempMin.push(pt.temp.min);
        tempMax.push(pt.temp.max);
        tempAvg.push(pt.temp.avg);
        humMin.push(pt.hum.min);
        humMax.push(pt.hum.max);
        humAvg.push(pt.hum.avg);
        presMin.push(pt.pres.min);
        presMax.push(pt.pres.max);
        presAvg.push(pt.pres.avg);
      }

      const data = [
        xData,
        tempMin, tempMax, tempAvg,
        humMin, humMax, humAvg,
        presMin, presMax, presAvg
      ];

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
          {}, // x-axis
          // Temperature
          {
            show: true,
            scale: 'temp',
            stroke: 'rgba(244, 63, 94, 0.5)',
            width: 1,
            points: { show: false },
            label: 'Temp Min',
            value: (self, rawValue) => rawValue != null ? rawValue.toFixed(2) + ' °C' : '--',
          },
          {
            show: true,
            scale: 'temp',
            stroke: 'rgba(244, 63, 94, 0.5)',
            width: 1,
            points: { show: false },
            label: 'Temp Max',
            value: (self, rawValue) => rawValue != null ? rawValue.toFixed(2) + ' °C' : '--',
          },
          {
            show: true,
            scale: 'temp',
            label: 'Temperature (Min/Max/Avg)',
            value: (self, val, sIdx, dIdx) => formatAvgMinMax(self, val, sIdx, dIdx, '°C', 2),
            stroke: '#f43f5e',
            width: 2,
          },
          // Humidity
          {
            show: true,
            scale: 'humidity',
            stroke: 'rgba(6, 182, 212, 0.5)',
            width: 1,
            points: { show: false },
            label: 'Hum Min',
            value: (self, rawValue) => rawValue != null ? rawValue.toFixed(2) + ' %' : '--',
          },
          {
            show: true,
            scale: 'humidity',
            stroke: 'rgba(6, 182, 212, 0.5)',
            width: 1,
            points: { show: false },
            label: 'Hum Max',
            value: (self, rawValue) => rawValue != null ? rawValue.toFixed(2) + ' %' : '--',
          },
          {
            show: true,
            scale: 'humidity',
            label: 'Humidity (Min/Max/Avg)',
            value: (self, val, sIdx, dIdx) => formatAvgMinMax(self, val, sIdx, dIdx, '%', 2),
            stroke: '#06b6d4',
            width: 2,
          },
          // Pressure
          {
            show: true,
            scale: 'pressure',
            stroke: 'rgba(16, 185, 129, 0.5)',
            width: 1,
            points: { show: false },
            label: 'Pres Min',
            value: (self, rawValue) => rawValue != null ? rawValue.toFixed(1) + ' hPa' : '--',
          },
          {
            show: true,
            scale: 'pressure',
            stroke: 'rgba(16, 185, 129, 0.5)',
            width: 1,
            points: { show: false },
            label: 'Pres Max',
            value: (self, rawValue) => rawValue != null ? rawValue.toFixed(1) + ' hPa' : '--',
          },
          {
            show: true,
            scale: 'pressure',
            label: 'Pressure (Min/Max/Avg)',
            value: (self, val, sIdx, dIdx) => formatAvgMinMax(self, val, sIdx, dIdx, 'hPa', 1),
            stroke: '#10b981',
            width: 2,
          }
        ],
        bands: [
          {
            series: [2, 1],
            fill: 'rgba(244, 63, 94, 0.15)'
          },
          {
            series: [5, 4],
            fill: 'rgba(6, 182, 212, 0.15)'
          },
          {
            series: [8, 7],
            fill: 'rgba(16, 185, 129, 0.15)'
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

      // Hide Min and Max series from the default legend DOM
      const legendRows = target.querySelectorAll('.u-legend tr');
      legendRows.forEach((row, idx) => {
        if (idx === 0 || idx === 1 || idx === 3 || idx === 4 || idx === 6 || idx === 7) {
          row.style.display = 'none';
        }
      });
    }

    function renderBatteryChart(dataset) {
      const container = $('batChartParent');
      const target = $('batChart');
      const rect = container.getBoundingClientRect();

      if (batChartInstance) {
        batChartInstance.destroy();
        batChartInstance = null;
      }

      if (!dataset || dataset.length === 0) {
        target.innerHTML = `<div style="color: var(--text-sub); text-align: center; line-height: 250px; font-size: 13px;">No battery history data.</div>`;
        return;
      }

      const xData = [];
      const vData = [];

      for (let i = 0; i < dataset.length; i++) {
        xData.push(dataset[i].ts);
        vData.push(dataset[i].v);
      }

      const data = [xData, vData];

      const opts = {
        width: rect.width,
        height: 250,
        title: "",
        class: "uplot-theme",
        cursor: {
          show: true
        },
        select: {
          show: false
        },
        scales: {
          x: {
            time: true,
          },
          v: {
            auto: true,
            range: [3.0, 4.3],
          }
        },
        series: [
          {},
          {
            show: true,
            scale: 'v',
            label: 'Voltage',
            value: (self, rawValue) => rawValue != null ? rawValue.toFixed(2) + ' V' : '--',
            stroke: '#fbbf24',
            width: 2,
            fill: 'rgba(251, 191, 36, 0.04)',
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
            scale: 'v',
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
          }
        ]
      };

      target.innerHTML = '';
      batChartInstance = new uPlot(opts, data, target);
    }

    async function loadBatteryHistory() {
      try {
        // Load all records in the log file (up to 2 days capped by backend pruning)
        
        const response = await fetch('/api/logs/download?file=/logs/battery.bin');
        if (!response.ok) {
          if (response.status === 404) {
            setText('batChartMeta', 'No battery log file found yet (waiting for first 60s sample).');
            renderBatteryChart([]);
            loadedBatteryData = [];
            return;
          }
          throw new Error(`HTTP ${response.status}`);
        }

        const arrayBuffer = await response.arrayBuffer();
        if (arrayBuffer.byteLength === 0) {
          setText('batChartMeta', 'No battery data file found or file empty.');
          renderBatteryChart([]);
          loadedBatteryData = [];
          return;
        }

        const recordSize = 14;
        const view = new DataView(arrayBuffer);
        const numRecords = Math.floor(arrayBuffer.byteLength / recordSize);
        const dataset = [];

        for (let i = 0; i < numRecords; i++) {
          const offset = i * recordSize;
          const epochTime = view.getUint32(offset, true);
          if (epochTime === 0) continue;
          const voltage = view.getFloat32(offset + 4, true);
          const percent = view.getUint8(offset + 8);
          const chargingState = view.getUint8(offset + 9);
          const timeRemaining = view.getInt32(offset + 10, true);

          let status = 'Unknown';
          if (chargingState === 1) status = 'Discharging';
          else if (chargingState === 2) status = 'Charging / USB';
          else if (chargingState === 3) status = 'Full';

          let tsStr = '';
          if (epochTime < 1000000) {
            tsStr = `uptime+${epochTime}s`;
          } else {
            const d = new Date(epochTime * 1000);
            const pad = (n) => String(n).padStart(2, '0');
            tsStr = `${d.getFullYear()}-${pad(d.getMonth()+1)}-${pad(d.getDate())} ${pad(d.getHours())}:${pad(d.getMinutes())}:${pad(d.getSeconds())}`;
          }

          dataset.push({ ts: epochTime, tsStr: tsStr, v: voltage, p: percent, s: status, tr: timeRemaining });
        }

        dataset.sort((a, b) => a.ts - b.ts);
        loadedBatteryData = dataset;
        renderBatteryChart(dataset);

        if (dataset.length > 0) {
          const lastPoint = dataset[dataset.length - 1];
          let metaText = `Loaded ${dataset.length} points. Latest: ${lastPoint.v.toFixed(2)}V (${lastPoint.p}%) - ${lastPoint.s}`;
          if (lastPoint.tr && lastPoint.tr > 0) {
            const hours = Math.floor(lastPoint.tr / 3600);
            const mins = Math.floor((lastPoint.tr % 3600) / 60);
            if (lastPoint.s === 'Charging / USB') {
              metaText += ` (${hours}h ${mins}m to full)`;
            } else {
              metaText += ` (${hours}h ${mins}m remaining)`;
            }
          }
          setText('batChartMeta', metaText);
        } else {
          setText('batChartMeta', 'No battery records found in the log file.');
        }
        lastBatteryFetchTime = Date.now();
      } catch (err) {
        console.error("Battery history load error", err);
        setText('batChartMeta', 'Failed to load battery history: ' + err.message);
        renderBatteryChart([]);
        loadedBatteryData = [];
      }
    }

    window.addEventListener('resize', () => {
      if (uplotInstance) {
        const container = $('chart-parent');
        const rect = container.getBoundingClientRect();
        uplotInstance.setSize({ width: rect.width, height: 500 });
      }
      if (batChartInstance) {
        const container = $('batChartParent');
        const rect = container.getBoundingClientRect();
        batChartInstance.setSize({ width: rect.width, height: 250 });
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

    function downloadBatteryCSV() {
      if (!loadedBatteryData || loadedBatteryData.length === 0) {
        alert("No battery records loaded yet.");
        return;
      }
      let csvContent = "timestamp,voltage,percent,status,time_remaining_s\n";
      for (const pt of loadedBatteryData) {
        csvContent += pt.tsStr + "," + pt.v.toFixed(2) + "," + pt.p + "," + pt.s + "," + pt.tr + "\n";
      }
      const blob = new Blob([csvContent], { type: 'text/csv;charset=utf-8;' });
      const link = document.createElement("a");
      const url = URL.createObjectURL(blob);
      link.setAttribute("href", url);
      link.setAttribute("download", "battery.csv");
      link.style.visibility = 'hidden';
      document.body.appendChild(link);
      link.click();
      document.body.removeChild(link);
    }

    async function loadSdTree() {
      if (isLoadingHistory) return;
      try {
        const data = await fetchJson('/api/sd-tree');
        const files = Array.isArray(data.files) ? data.files : [];
        const container = $('sdtree');
        if (files.length === 0) {
          container.innerHTML = '<div style="padding: 10px; color: var(--text-sub);">SD card is empty.</div>';
          return;
        }

        let html = '';
        for (const f of files) {
          const path = f.path;
          const name = f.name;
          const isDir = f.is_dir;
          
          let cleanPath = path;
          if (cleanPath.endsWith('/')) cleanPath = cleanPath.slice(0, -1);
          const slashCount = (cleanPath.match(/\//g) || []).length;
          const indent = (slashCount - 1) * 20;

          const sizeText = isDir ? '' : ` (${(f.size / 1024).toFixed(2)} KB)`;
          const icon = isDir ? '📁' : '📄';
          const downloadUrl = '/api/logs/download?file=' + encodeURIComponent(path);

          html += `<div style="display: flex; justify-content: space-between; align-items: center; padding: 6px 8px; margin-left: ${indent}px; border-bottom: 1px solid rgba(255,255,255,0.03);">
                     <div style="display: flex; align-items: center; gap: 8px;">
                       <span>${icon}</span>
                       ${isDir ? `<span style="font-weight: 500;">${name}</span>` : `<a href="${downloadUrl}" style="color: var(--accent); text-decoration: none;">${name}</a>`}
                       <span style="color: var(--text-sub); font-size: 0.75rem;">${sizeText}</span>
                     </div>
                     <div style="display: flex; gap: 8px;">
                       <button onclick="renameFile('${path}', '${name}')" style="padding: 2px 6px; font-size: 0.75rem; background: rgba(255,255,255,0.08); color: var(--text); border: 1px solid var(--card-border); border-radius: 4px; cursor: pointer;">Rename</button>
                       <button onclick="deleteFile('${path}', '${name}')" style="padding: 2px 6px; font-size: 0.75rem; background: rgba(239,68,68,0.2); border: 1px solid var(--error); color: var(--error); border-radius: 4px; cursor: pointer;">Delete</button>
                     </div>
                   </div>`;
        }
        container.innerHTML = html;
        lastSdTreeFetchTime = Date.now();
      } catch (err) {
        $('sdtree').innerHTML = '<div style="padding: 10px; color: var(--error);">' + err.message + '</div>';
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
          } else if (e.event === 'ntp_reestablished' || e.event === 'ntp_synced') {
            severity = 'ntp_reestablished';
          }
          
          const timePart = e.ts;
          html += '<div class="timeline-item ' + severity + '">';
          html += '  <div class="timeline-time">' + timePart + '</div>';
          html += '  <div class="timeline-content">' + e.event + '</div>';
          html += '</div>';
        }
        timeline.innerHTML = html;
        lastEventsFetchTime = Date.now();
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
        lastLiveFetchTime = Date.now();
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
              const tRemaining = bat.time_remaining;
              if (tRemaining > 0) {
                const tHrs = Math.floor(tRemaining / 3600);
                const tMins = Math.floor((tRemaining % 3600) / 60);
                timeText = `${tHrs}h ${tMins}m to full (Charging)`;
              } else {
                timeText = "Charging via USB";
              }
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
        lastHealthFetchTime = Date.now();
      } catch (err) {
        console.error("Health fetch error", err);
      }
    }

    function showTab(tabId) {
      document.querySelectorAll('.tab-btn').forEach(btn => btn.classList.remove('active'));
      document.querySelectorAll('.tab-content').forEach(content => content.classList.remove('active'));
      
      const targetBtn = $('tab-' + tabId);
      const targetContent = $('content-' + tabId);
      if (targetBtn && targetContent) {
        targetBtn.classList.add('active');
        targetContent.classList.add('active');
      }
    }

    async function loadConfig() {
      if (isLoadingHistory) return;
      try {
        const cfg = await fetchJson('/api/config');
        $('cfgWifiSsid').value = cfg.wifi_ssid || '';
        $('cfgWifiPassword').value = '';
        $('cfgHostname').value = cfg.hostname || '';
        $('cfgTimezone').value = cfg.timezone || '';
        $('cfgSampleInterval').value = cfg.sample_interval_ms;
        $('cfgLogFlushInterval').value = cfg.log_flush_interval_ms;
        $('cfgDisplayRefreshInterval').value = cfg.display_refresh_interval_ms;
      } catch (err) {
        console.error("Config fetch error", err);
      }
    }

    async function saveSettings(e) {
      e.preventDefault();
      if (isLoadingHistory) return;
      
      const payload = {
        wifi_ssid: $('cfgWifiSsid').value,
        hostname: $('cfgHostname').value,
        timezone: $('cfgTimezone').value,
        sample_interval_ms: parseInt($('cfgSampleInterval').value),
        log_flush_interval_ms: parseInt($('cfgLogFlushInterval').value),
        display_refresh_interval_ms: parseInt($('cfgDisplayRefreshInterval').value)
      };

      const pwd = $('cfgWifiPassword').value;
      if (pwd.length > 0) {
        payload.wifi_password = pwd;
      }

      try {
        const res = await fetch('/api/config', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify(payload)
        });
        const data = await res.json();
        if (data.accepted) {
          alert(data.message || 'Settings applied successfully.');
          if (data.reboot) {
            setTimeout(() => { window.location.reload(); }, 5000);
          } else {
            await loadConfig();
          }
        } else {
          alert('Failed to save configuration: ' + data.message);
        }
      } catch (err) {
        alert('Error saving configuration: ' + err.message);
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

    async function deleteFile(path, name) {
      if (!confirm('Are you sure you want to delete ' + name + '?')) return;
      try {
        const res = await fetch('/api/logs/delete?file=' + encodeURIComponent(path), { method: 'POST' });
        const data = await res.json();
        if (data.success) {
          alert(data.message || 'Deleted successfully.');
          await loadSdTree();
        } else {
          alert('Error: ' + data.message);
        }
      } catch (err) {
        alert('Failed to delete: ' + err.message);
      }
    }

    async function renameFile(path, name) {
      const newName = prompt('Enter new name for ' + name + ':', name);
      if (!newName || newName === name) return;
      try {
        const res = await fetch('/api/logs/rename?file=' + encodeURIComponent(path) + '&new_name=' + encodeURIComponent(newName), { method: 'POST' });
        const data = await res.json();
        if (data.success) {
          alert(data.message || 'Renamed successfully.');
          await loadSdTree();
        } else {
          alert('Error: ' + data.message);
        }
      } catch (err) {
        alert('Failed to rename: ' + err.message);
      }
    }

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
    $('btnDownloadBatCsv').addEventListener('click', downloadBatteryCSV);
    $('btnSaveUiIntervals').addEventListener('click', saveUiIntervals);
    $('range').addEventListener('change', onRangeChanged);
    $('binSize').addEventListener('change', () => {
      const isCustom = $('binSize').value === 'custom';
      $('customBinGroup').style.display = isCustom ? 'block' : 'none';
      if (historyLoaded && historyDataset.length > 0) {
        renderChart();
      }
    });
    $('customBinVal').addEventListener('input', () => {
      if (historyLoaded && historyDataset.length > 0) {
        renderChart();
      }
    });

    // OTA File Upload Handler
    const otaFile = $('otaFile');
    const otaDragDrop = $('otaDragDrop');
    const otaDragText = $('otaDragText');
    const btnStartOta = $('btnStartOta');
    const otaProgressContainer = $('otaProgressContainer');
    const otaProgressBar = $('otaProgressBar');
    const otaPercent = $('otaPercent');
    const otaStatus = $('otaStatus');
    const otaAlert = $('otaAlert');
    let selectedOtaFile = null;

    otaDragDrop.addEventListener('click', () => otaFile.click());

    otaDragDrop.addEventListener('dragover', (e) => {
      e.preventDefault();
      otaDragDrop.style.borderColor = 'var(--accent)';
      otaDragDrop.style.background = 'rgba(6, 182, 212, 0.05)';
    });

    otaDragDrop.addEventListener('dragleave', () => {
      otaDragDrop.style.borderColor = 'var(--card-border)';
      otaDragDrop.style.background = 'rgba(255, 255, 255, 0.02)';
    });

    otaDragDrop.addEventListener('drop', (e) => {
      e.preventDefault();
      otaDragDrop.style.borderColor = 'var(--card-border)';
      otaDragDrop.style.background = 'rgba(255, 255, 255, 0.02)';
      if (e.dataTransfer.files.length > 0) {
        handleOtaFileSelect(e.dataTransfer.files[0]);
      }
    });

    otaFile.addEventListener('change', (e) => {
      if (e.target.files.length > 0) {
        handleOtaFileSelect(e.target.files[0]);
      }
    });

    function handleOtaFileSelect(file) {
      if (!file.name.endsWith('.bin')) {
        showOtaAlert('Only .bin firmware files are supported.', 'error');
        selectedOtaFile = null;
        btnStartOta.disabled = true;
        otaDragText.textContent = 'Drag & drop or click to select file';
        return;
      }
      selectedOtaFile = file;
      otaDragText.innerHTML = `<strong>Selected:</strong> ${file.name} (${(file.size / 1024).toFixed(1)} KB)`;
      btnStartOta.disabled = false;
      otaAlert.style.display = 'none';
    }

    function showOtaAlert(msg, type) {
      otaAlert.style.display = 'block';
      otaAlert.textContent = msg;
      otaAlert.className = 'alert-banner ' + type;
    }

    function resetOtaUI() {
      btnStartOta.disabled = false;
      otaDragDrop.style.pointerEvents = 'auto';
      selectedOtaFile = null;
      otaDragText.textContent = 'Drag & drop or click to select file';
      otaFile.value = '';
    }

    function startRebootCountdown() {
      let count = 10;
      otaDragText.textContent = 'Device is rebooting. Reconnecting...';
      const timer = setInterval(() => {
        count--;
        if (count <= 0) {
          clearInterval(timer);
          window.location.reload();
        } else {
          otaStatus.textContent = `Reconnecting in ${count}s...`;
        }
      }, 1000);
    }

    btnStartOta.addEventListener('click', () => {
      if (!selectedOtaFile) return;

      btnStartOta.disabled = true;
      otaDragDrop.style.pointerEvents = 'none';
      otaProgressContainer.style.display = 'block';
      otaStatus.textContent = 'Uploading...';
      otaPercent.textContent = '0%';
      otaProgressBar.style.width = '0%';
      otaAlert.style.display = 'none';

      const formData = new FormData();
      formData.append('update', selectedOtaFile);

      const xhr = new XMLHttpRequest();
      xhr.open('POST', '/api/update', true);

      xhr.upload.addEventListener('progress', (e) => {
        if (e.lengthComputable) {
          const percent = Math.round((e.loaded / e.total) * 100);
          otaProgressBar.style.width = percent + '%';
          otaPercent.textContent = percent + '%';
          if (percent === 100) {
            otaStatus.textContent = 'Flashing...';
          }
        }
      });

      xhr.onload = () => {
        if (xhr.status === 200) {
          try {
            const res = JSON.parse(xhr.responseText);
            if (res.success) {
              showOtaAlert(res.message, 'success');
              otaStatus.textContent = 'Rebooting...';
              startRebootCountdown();
            } else {
              showOtaAlert(res.message || 'OTA update failed', 'error');
              resetOtaUI();
            }
          } catch (e) {
            showOtaAlert('Firmware flashed. Rebooting...', 'success');
            otaStatus.textContent = 'Rebooting...';
            startRebootCountdown();
          }
        } else {
          showOtaAlert('Upload failed. Server status: ' + xhr.status, 'error');
          resetOtaUI();
        }
      };

      xhr.onerror = () => {
        showOtaAlert('Network error occurred during update.', 'error');
        resetOtaUI();
      };

      xhr.send(formData);
    });

    window.addEventListener('load', async () => {
      onRangeChanged();
      await loadConfig();
      await loadSdTree();
      await loadLive();
      await loadHealth();
      await loadEvents();
      await loadBatteryHistory();
      renderChart();

      initTasks();
      loadUiIntervals();
      startScheduler(true);
    });
  </script>
</body>
</html>
)HTML";

  server_.send_P(200, "text/html", html, sizeof(html) - 1);
}

void WebManager::handleLocalUPlotJs() {
  server_.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server_.sendHeader("Content-Encoding", "gzip");
  server_.sendHeader("Cache-Control", "public, max-age=86400");
  server_.send(200, "application/javascript", "");
  server_.sendContent_P((const char*)UPLOT_JS_GZ, UPLOT_JS_GZ_LEN);
}

void WebManager::handleLocalUPlotCss() {
  server_.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server_.sendHeader("Content-Encoding", "gzip");
  server_.sendHeader("Cache-Control", "public, max-age=86400");
  server_.send(200, "text/css", "");
  server_.sendContent_P((const char*)UPLOT_CSS_GZ, UPLOT_CSS_GZ_LEN);
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
    battery["slope"] = health_->batterySlope;
    
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

  String cleanLine = line;
  if (cleanLine.endsWith("\r")) {
    cleanLine.remove(cleanLine.length() - 1);
  }

  char buf[160]{};
  cleanLine.toCharArray(buf, sizeof(buf));

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
  StaticJsonDocument<512> doc;
  doc["wifi_ssid"] = config_->wifiSsid;
  // For security, do not return the actual password
  doc["wifi_password"] = "";
  doc["hostname"] = config_->hostname;
  doc["timezone"] = config_->timezone;
  doc["sample_interval_ms"] = config_->sampleIntervalMs;
  doc["log_flush_interval_ms"] = config_->logFlushIntervalMs;
  doc["display_refresh_interval_ms"] = config_->displayRefreshIntervalMs;
  doc["phase"] = "milestone4";

  String response;
  serializeJson(doc, response);
  server_.send(200, "application/json", response);
}

void WebManager::handleConfigPost() {
  StaticJsonDocument<512> requested;
  if (server_.hasArg("plain") && server_.arg("plain").length() > 0) {
    const DeserializationError err = deserializeJson(requested, server_.arg("plain"));
    if (err) {
      sendJsonError(400, "Invalid JSON body");
      return;
    }
  } else {
    sendJsonError(400, "Missing JSON body");
    return;
  }

  bool rebootNeeded = false;

  if (requested.containsKey("wifi_ssid")) {
    const char* val = requested["wifi_ssid"];
    if (strcmp(config_->wifiSsid, val) != 0) {
      strncpy(config_->wifiSsid, val, sizeof(config_->wifiSsid));
      rebootNeeded = true;
    }
  }

  if (requested.containsKey("wifi_password")) {
    const char* val = requested["wifi_password"];
    if (strcmp(config_->wifiPassword, val) != 0) {
      strncpy(config_->wifiPassword, val, sizeof(config_->wifiPassword));
      rebootNeeded = true;
    }
  }

  if (requested.containsKey("hostname")) {
    const char* val = requested["hostname"];
    if (strcmp(config_->hostname, val) != 0) {
      strncpy(config_->hostname, val, sizeof(config_->hostname));
      rebootNeeded = true;
    }
  }

  if (requested.containsKey("timezone")) {
    const char* val = requested["timezone"];
    if (strcmp(config_->timezone, val) != 0) {
      strncpy(config_->timezone, val, sizeof(config_->timezone));
      if (timeManager_) {
        timeManager_->setTimezone(config_->timezone);
      }
    }
  }

  if (requested.containsKey("sample_interval_ms")) {
    config_->sampleIntervalMs = requested["sample_interval_ms"];
  }
  if (requested.containsKey("log_flush_interval_ms")) {
    config_->logFlushIntervalMs = requested["log_flush_interval_ms"];
  }
  if (requested.containsKey("display_refresh_interval_ms")) {
    config_->displayRefreshIntervalMs = requested["display_refresh_interval_ms"];
  }

  // Save the configuration to the SD card
  bool saveOk = false;
  if (loggerManager_) {
    saveOk = loggerManager_->saveDeviceConfig(*config_);
  }

  StaticJsonDocument<256> response;
  response["accepted"] = saveOk;
  response["reboot"] = rebootNeeded;
  if (saveOk) {
    response["message"] = rebootNeeded ? "Configuration saved. Rebooting..." : "Configuration applied dynamically.";
  } else {
    response["message"] = "Failed to save configuration to SD card.";
  }

  String out;
  serializeJson(response, out);
  server_.send(200, "application/json", out);

  if (saveOk && rebootNeeded) {
    Serial.println("[Config] Rebooting to apply network configuration changes...");
    delay(1000);
    ESP.restart();
  }
}

void WebManager::handleLogDelete() {
  if (!ensureSdReady()) {
    sendJsonError(503, "SD card unavailable");
    return;
  }
  if (!server_.hasArg("file")) {
    sendJsonError(400, "Missing file parameter");
    return;
  }
  String path = server_.arg("file");
  if (!path.startsWith("/") || path.indexOf("..") >= 0) {
    sendJsonError(400, "Invalid file path");
    return;
  }
  if (path == "/" || path.length() <= 1) {
    sendJsonError(400, "Cannot delete root directory");
    return;
  }
  if (!SD.exists(path)) {
    sendJsonError(404, "File or directory not found");
    return;
  }

  bool success = false;
  File f = SD.open(path, "r");
  if (f) {
    bool isDir = f.isDirectory();
    f.close();
    if (isDir) {
      success = SD.rmdir(path);
    } else {
      success = SD.remove(path);
    }
  }

  if (success) {
    StaticJsonDocument<128> doc;
    doc["success"] = true;
    doc["message"] = "Deleted successfully";
    String out;
    serializeJson(doc, out);
    server_.send(200, "application/json", out);
  } else {
    sendJsonError(500, "Failed to delete item. Ensure folders are empty.");
  }
}

void WebManager::handleLogRename() {
  if (!ensureSdReady()) {
    sendJsonError(503, "SD card unavailable");
    return;
  }
  if (!server_.hasArg("file") || !server_.hasArg("new_name")) {
    sendJsonError(400, "Missing parameters");
    return;
  }
  String oldPath = server_.arg("file");
  String newName = server_.arg("new_name");

  if (!oldPath.startsWith("/") || oldPath.indexOf("..") >= 0) {
    sendJsonError(400, "Invalid source path");
    return;
  }
  if (oldPath == "/" || oldPath.length() <= 1) {
    sendJsonError(400, "Cannot rename root directory");
    return;
  }
  if (newName.indexOf('/') >= 0 || newName.indexOf('\\') >= 0 || newName.indexOf("..") >= 0) {
    sendJsonError(400, "Invalid target filename");
    return;
  }

  int lastSlash = oldPath.lastIndexOf('/');
  String parentPath = oldPath.substring(0, lastSlash);
  String newPath = parentPath + "/" + newName;

  if (!SD.exists(oldPath)) {
    sendJsonError(404, "Source item not found");
    return;
  }
  if (SD.exists(newPath)) {
    sendJsonError(409, "Target item already exists");
    return;
  }

  if (SD.rename(oldPath, newPath)) {
    StaticJsonDocument<128> doc;
    doc["success"] = true;
    doc["message"] = "Renamed successfully";
    String out;
    serializeJson(doc, out);
    server_.send(200, "application/json", out);
  } else {
    sendJsonError(500, "Failed to rename item");
  }
}



void WebManager::streamEventsJson(File& file, const String& startTs, const String& endTs, uint32_t limit) {
  server_.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server_.send(200, "application/json", "");

  server_.sendContent("{\"events\":[");

  uint32_t fileSize = file.size();
  if (fileSize > 0) {
    uint32_t pos = fileSize;
    uint32_t newlineCount = 0;
    while (pos > 0) {
      pos--;
      file.seek(pos);
      char c = file.read();
      if (c == '\n') {
        newlineCount++;
        if (newlineCount > limit) {
          break;
        }
      }
    }
    if (pos == 0) {
      file.seek(0);
    }
  }

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
  if (!path.startsWith("/") || path.indexOf("..") >= 0) {
    sendJsonError(400, "Invalid file path");
    return;
  }



  File file = SD.open(path, "r");
  if (!file || file.isDirectory()) {
    if (file) file.close();
    sendJsonError(404, "Requested file not found");
    return;
  }

  const char* contentType = "text/plain";
  if (path.endsWith(".csv")) {
    contentType = "text/csv";
  } else if (path.endsWith(".json")) {
    contentType = "application/json";
  } else if (path.endsWith(".bin")) {
    contentType = "application/octet-stream";
  }

  int lastSlash = path.lastIndexOf('/');
  String filename = (lastSlash >= 0) ? path.substring(lastSlash + 1) : path;

  server_.sendHeader("Content-Disposition", "attachment; filename=\"" + filename + "\"");
  server_.sendHeader("Connection", "close");

  server_.streamFile(file, contentType);
  file.close();
}

void WebManager::handleSdTreeJson() {
  if (!ensureSdReady()) {
    sendJsonError(503, "SD card unavailable");
    return;
  }

  File root = SD.open("/");
  if (!root || !root.isDirectory()) {
    sendJsonError(404, "SD root unavailable");
    return;
  }

  server_.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server_.send(200, "application/json", "");
  server_.sendContent("{\"files\":[");

  bool first = true;
  streamSdTree(root, "", first);

  root.close();
  server_.sendContent("]}");
}

void WebManager::streamSdTree(File dir, const String& parentPath, bool& first) {
  while (true) {
    File entry = dir.openNextFile();
    if (!entry) {
      break;
    }

    if (!first) {
      server_.sendContent(",");
    }
    first = false;

    String name = entry.name();
    String path = parentPath + "/" + name;
    if (path.startsWith("//")) {
      path.remove(0, 1);
    }

    unsigned long size = entry.isDirectory() ? 0 : entry.size();
    bool isDir = entry.isDirectory();



    String item = "{\"path\":\"" + path + "\",\"name\":\"" + name + "\",\"size\":" + String(size) + ",\"is_dir\":" + (isDir ? "true" : "false") + "}";
    server_.sendContent(item);

    if (isDir) {
      File subDir = SD.open(path);
      if (subDir) {
        streamSdTree(subDir, path, first);
        subDir.close();
      }
    }
    entry.close();
  }
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

void WebManager::handleOtaUpdatePost() {
  server_.sendHeader("Connection", "close");
  StaticJsonDocument<128> doc;
  if (Update.hasError()) {
    doc["success"] = false;
    doc["message"] = "Firmware update failed";
  } else {
    doc["success"] = true;
    doc["message"] = "Firmware update successful! Rebooting...";
  }
  String response;
  serializeJson(doc, response);
  server_.send(200, "application/json", response);

  delay(100);
  ESP.restart();
}

void WebManager::handleOtaUpdateUpload() {
  HTTPUpload& upload = server_.upload();
  static uint32_t totalLength = 0;

  if (upload.status == UPLOAD_FILE_START) {
    totalLength = server_.header("Content-Length").toInt();
    Serial.printf("\n[OTA] Web Update Start: %s (Expected Total: %u bytes)\n", upload.filename.c_str(), totalLength);
    if (displayManager_) {
      displayManager_->showStartupStatus("OTA Web", "Flashing...");
    }
    
    // Use maximum available sketch space
    uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
    if (!Update.begin(maxSketchSpace)) {
      Update.printError(Serial);
    }
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
      Update.printError(Serial);
    } else {
      if (displayManager_ && totalLength > 0) {
        displayManager_->showOtaProgress(upload.totalSize, totalLength);
      }
      Serial.print(".");
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (Update.end(true)) {
      Serial.printf("\n[OTA] Web Update Success: %u bytes\n", upload.totalSize);
      if (displayManager_) {
        displayManager_->showStartupStatus("OTA Web", "Success", "Rebooting...");
      }
    } else {
      Update.printError(Serial);
      if (displayManager_) {
        displayManager_->showStartupStatus("OTA Web", "Failed", "Check logs", true);
      }
    }
  }
}

void WebManager::startAPFallback() {
  apFallbackActive_ = true;
  WiFi.mode(WIFI_AP_STA);
  
  String apSsid = String(config_->hostname) + "-AP";
  WiFi.softAP(apSsid.c_str());
  
  Serial.printf("[WiFi] Fallback AP Started: %s\n", apSsid.c_str());
  Serial.printf("[WiFi] AP IP Address: %s\n", WiFi.softAPIP().toString().c_str());
  
  if (displayManager_) {
    displayManager_->showStartupStatus("WiFi AP", "AP Started", apSsid.c_str());
  }
}

void WebManager::handleWifiReconnectedSTA() {
  if (apFallbackActive_) {
    Serial.println("[WiFi] Station connected. Disabling Fallback AP.");
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
    apFallbackActive_ = false;
  }
}

