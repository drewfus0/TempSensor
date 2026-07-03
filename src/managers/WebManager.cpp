#include "managers/WebManager.h"

#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <stdlib.h>

#include "config/AppConfig.h"

bool WebManager::begin(const char* ssid, const char* password, const char* hostname) {
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
}

void WebManager::handleRoot() {
  static const char html[] PROGMEM = R"HTML(
<!doctype html>
<html>
<head>
  <meta charset='utf-8'>
  <meta name='viewport' content='width=device-width,initial-scale=1'>
  <title>TempSensor Dashboard</title>
  <style>
    :root {
      --bg: #f2f4f7;
      --ink: #1e2530;
      --sub: #4a5668;
      --card: #ffffff;
      --line: #d2d9e3;
      --accent: #0d7a8a;
      --ok: #207542;
      --bad: #8f1f28;
      --warn: #9f6a00;
    }
    * { box-sizing: border-box; }
    body {
      margin: 0;
      padding: 16px;
      font-family: "Consolas", "Liberation Mono", "DejaVu Sans Mono", monospace;
      background: linear-gradient(180deg, #eef2f8 0%, var(--bg) 100%);
      color: var(--ink);
    }
    .wrap { max-width: 1200px; margin: 0 auto; }
    .head {
      display: flex;
      gap: 12px;
      justify-content: space-between;
      align-items: center;
      margin-bottom: 12px;
      flex-wrap: wrap;
    }
    h1 {
      margin: 0;
      font-size: 1.2rem;
      letter-spacing: 0.02em;
    }
    .pills { display: flex; gap: 8px; flex-wrap: wrap; }
    .pill {
      border: 1px solid var(--line);
      background: #fff;
      border-radius: 999px;
      padding: 4px 10px;
      font-size: 0.8rem;
    }
    .grid {
      display: grid;
      grid-template-columns: 1fr 1.2fr;
      gap: 12px;
    }
    .col { display: grid; gap: 12px; }
    .card {
      background: var(--card);
      border: 1px solid var(--line);
      border-radius: 10px;
      padding: 12px;
    }
    .card h2 {
      margin: 0 0 8px 0;
      font-size: 0.95rem;
      color: var(--sub);
    }
    .kv {
      display: grid;
      grid-template-columns: auto 1fr;
      gap: 4px 8px;
      font-size: 0.88rem;
    }
    .k { color: var(--sub); }
    .v { font-weight: 600; }
    .ok { color: var(--ok); }
    .bad { color: var(--bad); }
    .warn { color: var(--warn); }
    .controls {
      display: grid;
      grid-template-columns: repeat(2, minmax(0, 1fr));
      gap: 8px;
      margin-bottom: 8px;
    }
    label {
      display: grid;
      gap: 4px;
      font-size: 0.8rem;
      color: var(--sub);
    }
    input, select, button {
      width: 100%;
      border: 1px solid var(--line);
      border-radius: 8px;
      padding: 6px 8px;
      font: inherit;
      background: #fff;
      color: var(--ink);
    }
    button {
      background: var(--accent);
      color: #fff;
      border-color: var(--accent);
      cursor: pointer;
      font-weight: 700;
    }
    canvas {
      width: 100%;
      height: 240px;
      border: 1px solid var(--line);
      border-radius: 8px;
      background: #fff;
    }
    .meta { margin-top: 8px; font-size: 0.78rem; color: var(--sub); }
    pre {
      margin: 0;
      white-space: pre-wrap;
      overflow: auto;
      max-height: 220px;
      font-size: 0.8rem;
    }
    .logs a {
      color: var(--accent);
      text-decoration: none;
      font-weight: 700;
    }
    .logs a:hover { text-decoration: underline; }
    @media (max-width: 900px) {
      .grid { grid-template-columns: 1fr; }
      .controls { grid-template-columns: 1fr; }
    }
  </style>
</head>
<body>
  <div class='wrap'>
    <div class='head'>
      <h1>TempSensor Local Dashboard</h1>
      <div class='pills'>
        <span class='pill' id='pillWifi'>WiFi: ?</span>
        <span class='pill' id='pillSd'>SD: ?</span>
        <span class='pill' id='pillNtp'>NTP: ?</span>
        <span class='pill' id='pillIp'>IP: ?</span>
        <span class='pill' id='pillRef'>Updated: -</span>
      </div>
    </div>

    <div class='grid'>
      <div class='col'>
        <section class='card'>
          <h2>Live Snapshot</h2>
          <div class='kv' id='liveKv'></div>
        </section>
        <section class='card'>
          <h2>Health Snapshot</h2>
          <div class='kv' id='healthKv'></div>
        </section>
        <section class='card'>
          <h2>Config (Phase 1 API)</h2>
          <pre id='cfg'>loading...</pre>
        </section>
      </div>

      <div class='col'>
        <section class='card'>
          <h2>Historical Chart</h2>
          <div class='controls'>
            <label>Metric
              <select id='metric'>
                <option value='temp_c'>Temperature (C)</option>
                <option value='humidity_pct'>Humidity (%)</option>
                <option value='pressure_hpa'>Pressure (hPa)</option>
              </select>
            </label>
            <label>Range
              <select id='range'>
                <option value='15m'>Last 15 min</option>
                <option value='1h' selected>Last 1 hour</option>
                <option value='6h'>Last 6 hours</option>
                <option value='24h'>Last 24 hours</option>
                <option value='custom'>Custom</option>
              </select>
            </label>
            <label>Start (local)
              <input id='start' type='datetime-local'>
            </label>
            <label>End (local)
              <input id='end' type='datetime-local'>
            </label>
            <label>Max points
              <input id='maxPoints' type='number' min='20' max='1000' value='300'>
            </label>
            <label>Load
              <button id='btnLoad' type='button'>Refresh History</button>
            </label>
          </div>
          <canvas id='chart' width='720' height='240'></canvas>
          <div class='meta' id='historyMeta'>No data loaded yet.</div>
        </section>

        <section class='card'>
          <h2>Logs</h2>
          <div class='logs' id='logs'>loading...</div>
        </section>

        <section class='card'>
          <h2>Events (latest)</h2>
          <pre id='events'>loading...</pre>
        </section>

        <section class='card'>
          <h2>SD Card Tree</h2>
          <pre id='sdtree'>loading...</pre>
        </section>
      </div>
    </div>
  </div>

  <script>
    const $ = (id) => document.getElementById(id);

    function setText(id, text) {
      $(id).textContent = text;
    }

    function setHtml(id, html) {
      $(id).innerHTML = html;
    }

    async function fetchJson(url) {
      const res = await fetch(url, { cache: 'no-store' });
      if (!res.ok) {
        throw new Error(url + ' -> HTTP ' + res.status);
      }
      return await res.json();
    }

    async function fetchText(url) {
      const res = await fetch(url, { cache: 'no-store' });
      if (!res.ok) {
        throw new Error(url + ' -> HTTP ' + res.status);
      }
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

    function kvHtml(obj, order) {
      let out = '';
      for (const key of order) {
        const val = Object.prototype.hasOwnProperty.call(obj, key) ? obj[key] : '-';
        out += '<div class="k">' + key + '</div><div class="v">' + val + '</div>';
      }
      return out;
    }

    function updatePills(live, health) {
      const wifi = health.wifi_connected ? 'UP' : 'DOWN';
      const sd = health.sd_healthy ? 'OK' : 'BAD';
      const ntp = health.ntp_synced ? 'SYNC' : 'EST';

      setText('pillWifi', 'WiFi: ' + wifi);
      setText('pillSd', 'SD: ' + sd);
      setText('pillNtp', 'NTP: ' + ntp);

      let ip = '-';
      if (live && live.has_sample && typeof live.ip === 'string') {
        ip = live.ip;
      }
      setText('pillIp', 'IP: ' + ip);
      setText('pillRef', 'Updated: ' + stampNow());
    }

    function renderChart(points) {
      const c = $('chart');
      const ctx = c.getContext('2d');
      const w = c.width;
      const h = c.height;
      const padL = 42;
      const padR = 12;
      const padT = 12;
      const padB = 24;

      ctx.clearRect(0, 0, w, h);
      ctx.fillStyle = '#ffffff';
      ctx.fillRect(0, 0, w, h);

      ctx.strokeStyle = '#d2d9e3';
      ctx.lineWidth = 1;
      ctx.strokeRect(0.5, 0.5, w - 1, h - 1);

      const gx = padL;
      const gy = padT;
      const gw = w - padL - padR;
      const gh = h - padT - padB;

      ctx.strokeStyle = '#c8d0dd';
      for (let i = 0; i <= 4; i++) {
        const y = gy + (gh * i / 4);
        ctx.beginPath();
        ctx.moveTo(gx, y);
        ctx.lineTo(gx + gw, y);
        ctx.stroke();
      }

      if (!points || points.length === 0) {
        ctx.fillStyle = '#8b96a8';
        ctx.font = '14px monospace';
        ctx.fillText('No data in selected range.', gx + 8, gy + gh / 2);
        return;
      }

      let minV = Number.POSITIVE_INFINITY;
      let maxV = Number.NEGATIVE_INFINITY;
      for (const p of points) {
        const v = Number(p.v);
        if (Number.isFinite(v)) {
          if (v < minV) minV = v;
          if (v > maxV) maxV = v;
        }
      }

      if (!Number.isFinite(minV) || !Number.isFinite(maxV)) {
        minV = 0;
        maxV = 1;
      }
      if (maxV <= minV) {
        maxV = minV + 1;
      }

      ctx.fillStyle = '#4f5c70';
      ctx.font = '11px monospace';
      ctx.fillText(maxV.toFixed(2), 4, gy + 8);
      ctx.fillText(minV.toFixed(2), 4, gy + gh);

      ctx.strokeStyle = '#0d7a8a';
      ctx.lineWidth = 2;
      ctx.beginPath();

      for (let i = 0; i < points.length; i++) {
        const p = points[i];
        const v = Number(p.v);
        const nx = (points.length <= 1) ? 0 : (i / (points.length - 1));
        const ny = (v - minV) / (maxV - minV);
        const x = gx + nx * gw;
        const y = gy + (1 - ny) * gh;
        if (i === 0) {
          ctx.moveTo(x, y);
        } else {
          ctx.lineTo(x, y);
        }
      }
      ctx.stroke();

      ctx.fillStyle = '#4f5c70';
      ctx.fillText(points[0].ts || '-', gx, h - 8);
      const endTs = points[points.length - 1].ts || '-';
      const txtWidth = ctx.measureText(endTs).width;
      ctx.fillText(endTs, gx + gw - txtWidth, h - 8);
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
      try {
        setText('historyMeta', 'Loading history...');
        const q = buildHistoryQuery();
        const data = await fetchJson('/api/history?' + q);
        const points = Array.isArray(data.points) ? data.points : [];
        renderChart(points);
        setText('historyMeta',
          'metric=' + (data.metric || '?') +
          ' matched=' + (data.matched ?? 0) +
          ' plotted=' + points.length);
      } catch (err) {
        renderChart([]);
        setText('historyMeta', 'History error: ' + err.message);
      }
    }

    async function loadLogs() {
      try {
        const data = await fetchJson('/api/logs');
        const files = Array.isArray(data.files) ? data.files : [];
        if (files.length === 0) {
          setHtml('logs', '<div>No log files.</div>');
          return;
        }

        let html = '<ul>';
        for (const f of files) {
          const n = f.name || '?';
          const s = Number(f.size || 0);
          const p = '/api/logs/download?file=' + encodeURIComponent('/logs/' + n);
          html += '<li><a href="' + p + '">' + n + '</a> (' + s + ' bytes)</li>';
        }
        html += '</ul>';
        setHtml('logs', html);
      } catch (err) {
        setHtml('logs', '<div class="bad">' + err.message + '</div>');
      }
    }

    async function loadEvents() {
      try {
        const data = await fetchJson('/api/events?limit=20');
        setText('events', JSON.stringify(data, null, 2));
      } catch (err) {
        setText('events', 'Error: ' + err.message);
      }
    }

    async function loadSnapshot() {
      try {
        const [live, health] = await Promise.all([
          fetchJson('/api/live'),
          fetchJson('/api/health')
        ]);

        setHtml('liveKv', kvHtml(live, [
          'timestamp',
          'timestamp_quality',
          'temp_c',
          'humidity_pct',
          'pressure_hpa',
          'uptime_s'
        ]));

        setHtml('healthKv', kvHtml(health, [
          'uptime_s',
          'free_heap_bytes',
          'largest_free_block_bytes',
          'log_queue_depth',
          'log_queue_capacity',
          'dropped_log_samples',
          'wifi_connected',
          'sd_healthy',
          'ntp_synced'
        ]));

        updatePills(live, health);
      } catch (err) {
        setText('pillRef', 'Updated: error');
      }
    }

    async function loadStaticPanels() {
      try {
        const cfg = await fetchJson('/api/config');
        setText('cfg', JSON.stringify(cfg, null, 2));
      } catch (err) {
        setText('cfg', 'Error: ' + err.message);
      }

      try {
        const tree = await fetchText('/api/sd-tree');
        setText('sdtree', tree);
      } catch (err) {
        setText('sdtree', 'Error: ' + err.message);
      }
    }

    function onRangeChanged() {
      const custom = $('range').value === 'custom';
      $('start').disabled = !custom;
      $('end').disabled = !custom;
    }

    $('btnLoad').addEventListener('click', loadHistory);
    $('range').addEventListener('change', () => {
      onRangeChanged();
      loadHistory();
    });
    $('metric').addEventListener('change', loadHistory);

    (async function boot() {
      onRangeChanged();
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
