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
  static const char html[] =
      "<!doctype html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>"
      "<title>TempSensor Status</title><style>body{font-family:monospace;margin:20px;background:#f2f4f7;color:#20252d}"
      "h1{margin-bottom:8px}.card{background:#fff;padding:16px;border:1px solid #d3d7de;border-radius:8px;margin-bottom:12px}"
      "pre{white-space:pre-wrap;overflow:auto;max-height:320px}</style></head><body><h1>TempSensor Local Status</h1>"
      "<div class='card'><h2>Live Sensor</h2><pre id='live'>loading...</pre></div>"
      "<div class='card'><h2>Health</h2><pre id='health'>loading...</pre></div>"
      "<div class='card'><h2>Config</h2><pre id='cfg'>loading...</pre></div>"
      "<div class='card'><h2>Logs</h2><pre id='logs'>loading...</pre></div>"
      "<div class='card'><h2>Events (latest)</h2><pre id='events'>loading...</pre></div>"
      "<div class='card'><h2>History (temp_c)</h2><pre id='history'>loading...</pre></div>"
      "<div class='card'><h2>SD Card Tree</h2><pre id='sdtree'>loading...</pre></div>"
      "<script>async function pull(){const a=await fetch('/api/live').then(r=>r.json());"
      "const b=await fetch('/api/health').then(r=>r.json());"
      "const c=await fetch('/api/config').then(r=>r.json());"
      "const d=await fetch('/api/logs').then(r=>r.json());"
      "const e=await fetch('/api/events?limit=10').then(r=>r.json());"
      "const f=await fetch('/api/history?metric=temp_c&max_points=20').then(r=>r.json());"
      "const g=await fetch('/api/sd-tree').then(r=>r.text());"
      "document.getElementById('live').textContent=JSON.stringify(a,null,2);"
      "document.getElementById('health').textContent=JSON.stringify(b,null,2);"
      "document.getElementById('cfg').textContent=JSON.stringify(c,null,2);"
      "document.getElementById('logs').textContent=JSON.stringify(d,null,2);"
      "document.getElementById('events').textContent=JSON.stringify(e,null,2);"
      "document.getElementById('history').textContent=JSON.stringify(f,null,2);"
      "document.getElementById('sdtree').textContent=g;}"
      "pull();setInterval(pull,5000);</script></body></html>";

  server_.send(200, "text/html", html);
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
