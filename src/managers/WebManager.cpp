#include "managers/WebManager.h"

#include <ArduinoJson.h>
#include <ESP8266WiFi.h>

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
      "<div class='card'><h2>SD Card Tree</h2><pre id='sdtree'>loading...</pre></div>"
      "<script>async function pull(){const a=await fetch('/api/live').then(r=>r.json());"
      "const b=await fetch('/api/health').then(r=>r.json());"
      "const c=await fetch('/api/sd-tree').then(r=>r.text());"
      "document.getElementById('live').textContent=JSON.stringify(a,null,2);"
      "document.getElementById('health').textContent=JSON.stringify(b,null,2);"
      "document.getElementById('sdtree').textContent=c;}"
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
  if (health_ != nullptr && !health_->sdHealthy) {
    server_.send(200, "text/plain", "SD card unavailable\n");
    return;
  }

  if (!SD.begin(AppConfig::SD_CS_PIN)) {
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
