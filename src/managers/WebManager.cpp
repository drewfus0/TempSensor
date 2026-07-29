#include "managers/WebManager.h"

#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <stdlib.h>
#include <vector>
#include <algorithm>

#include "config/AppConfig.h"
#include "managers/LoggerManager.h"
#include "managers/TimeManager.h"
#include "managers/DisplayManager.h"
#include "web/DashboardPage.h"


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

  server_.on("/favicon.ico", [this]() { server_.send(204, "image/x-icon", ""); });
  server_.on("/api/live", [this]() { logRequest(); handleLiveJson(); });
  server_.on("/api/health", [this]() { logRequest(); handleHealthJson(); });
  server_.on("/api/config", HTTP_GET, [this]() { logRequest(); handleConfigGet(); });
  server_.on("/api/config", HTTP_POST, [this]() { logRequest(); handleConfigPost(); });
  server_.on("/api/history", HTTP_GET, [this]() { logRequest(); handleHistoryJson(); });
  server_.on("/api/events", HTTP_GET, [this]() { logRequest(); handleEventsJson(); });
  server_.on("/api/events/create", HTTP_POST, [this]() { logRequest(); handleCreateEvent(); });
  server_.on("/api/events/update", HTTP_POST, [this]() { logRequest(); handleUpdateEvent(); });
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
  server_.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server_.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server_.send(200, "text/html", "");

  const char* ptr = web::DASHBOARD_HTML;
  size_t remaining = sizeof(web::DASHBOARD_HTML) - 1;
  char chunkBuf[1024];

  while (remaining > 0) {
    size_t chunkSize = (remaining > sizeof(chunkBuf)) ? sizeof(chunkBuf) : remaining;
    memcpy_P(chunkBuf, ptr, chunkSize);
    server_.sendContent(chunkBuf, chunkSize);
    ptr += chunkSize;
    remaining -= chunkSize;
    yield();
  }
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
  doc["latitude"] = config_->latitude;
  doc["longitude"] = config_->longitude;
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
  if (requested.containsKey("latitude")) {
    config_->latitude = requested["latitude"];
  }
  if (requested.containsKey("longitude")) {
    config_->longitude = requested["longitude"];
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



void WebManager::streamEventsBinary(uint32_t limit) {
  if (!SD.exists(AppConfig::EVENT_DIR_PATH)) {
    sendJsonError(404, "event directory not found");
    return;
  }

  File dir = SD.open(AppConfig::EVENT_DIR_PATH);
  if (!dir || !dir.isDirectory()) {
    sendJsonError(404, "event directory unavailable");
    return;
  }

  std::vector<String> binFiles;
  File entry = dir.openNextFile();
  while (entry) {
    if (!entry.isDirectory()) {
      String name = String(entry.name());
      if (name.startsWith("ev_") && name.endsWith(".bin")) {
        binFiles.push_back(name);
      }
    }
    entry.close();
    entry = dir.openNextFile();
  }
  dir.close();

  std::sort(binFiles.begin(), binFiles.end());

  std::vector<EventRecord> records;
  for (int f = (int)binFiles.size() - 1; f >= 0 && records.size() < limit; f--) {
    String filepath = String(AppConfig::EVENT_DIR_PATH) + "/" + binFiles[f];
    File file = SD.open(filepath, "r");
    if (!file) continue;

    size_t totalRecords = file.size() / sizeof(EventRecord);
    for (int r = (int)totalRecords - 1; r >= 0 && records.size() < limit; r--) {
      file.seek(r * sizeof(EventRecord));
      EventRecord rec;
      if (file.read(reinterpret_cast<uint8_t*>(&rec), sizeof(EventRecord)) == sizeof(EventRecord)) {
        if (rec.quality == 2 || rec.epochTime == 0) continue;
        records.push_back(rec);
      }
      if (yieldCallback_) yieldCallback_(yieldCallbackArg_);
    }
    file.close();
  }

  std::reverse(records.begin(), records.end());

  server_.sendHeader("X-Record-Size", String(sizeof(EventRecord)));
  server_.setContentLength(records.size() * sizeof(EventRecord));
  server_.send(200, "application/octet-stream", "");

  for (size_t i = 0; i < records.size(); ++i) {
    server_.client().write(reinterpret_cast<const uint8_t*>(&records[i]), sizeof(EventRecord));
    if (i % 10 == 0 && yieldCallback_) {
      yieldCallback_(yieldCallbackArg_);
    }
  }
}

void WebManager::streamEventsJson(const String& startTs, const String& endTs, uint32_t limit) {
  server_.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server_.send(200, "application/json", "");
  server_.sendContent("{\"events\":[");

  uint32_t emitted = 0;

  if (SD.exists(AppConfig::EVENT_DIR_PATH)) {
    File dir = SD.open(AppConfig::EVENT_DIR_PATH);
    if (dir && dir.isDirectory()) {
      std::vector<String> binFiles;
      File entry = dir.openNextFile();
      while (entry) {
        if (!entry.isDirectory()) {
          String name = String(entry.name());
          if (name.startsWith("ev_") && name.endsWith(".bin")) {
            binFiles.push_back(name);
          }
        }
        entry.close();
        entry = dir.openNextFile();
      }
      dir.close();

      std::sort(binFiles.begin(), binFiles.end());

      std::vector<String> items;
      for (int f = (int)binFiles.size() - 1; f >= 0 && items.size() < limit; f--) {
        String filepath = String(AppConfig::EVENT_DIR_PATH) + "/" + binFiles[f];
        File file = SD.open(filepath, "r");
        if (!file) continue;

        size_t totalRecords = file.size() / sizeof(EventRecord);
        for (int r = (int)totalRecords - 1; r >= 0 && items.size() < limit; r--) {
          file.seek(r * sizeof(EventRecord));
          EventRecord rec;
          if (file.read(reinterpret_cast<uint8_t*>(&rec), sizeof(EventRecord)) == sizeof(EventRecord)) {
            if (rec.quality == 2 || rec.epochTime == 0) continue;

            char tsBuf[64]{};
            time_t epoch = rec.epochTime;
            struct tm* timeinfo = localtime(&epoch);
            if (timeinfo && timeinfo->tm_year > 70) {
              snprintf(tsBuf, sizeof(tsBuf), "%04u-%02u-%02u %02u:%02u:%02u",
                       (unsigned)(timeinfo->tm_year + 1900), (unsigned)(timeinfo->tm_mon + 1), (unsigned)timeinfo->tm_mday,
                       (unsigned)timeinfo->tm_hour, (unsigned)timeinfo->tm_min, (unsigned)timeinfo->tm_sec);
            } else {
              snprintf(tsBuf, sizeof(tsBuf), "2026-01-01 00:00:00");
            }

            String ts(tsBuf);
            if (!isTimestampInRange(ts, startTs, endTs)) continue;

            const char* qStr = (rec.quality == 0) ? "ntp" : "estimated";
            String item = "{\"ts\":\"" + ts + "\",\"q\":\"" + String(qStr) + "\",\"cat\":" + String(rec.category) + ",\"event\":\"" + String(rec.message) + "\"}";
            items.push_back(item);
          }
          if (yieldCallback_) yieldCallback_(yieldCallbackArg_);
        }
        file.close();
      }

      for (int i = (int)items.size() - 1; i >= 0; i--) {
        if (emitted > 0) server_.sendContent(",");
        server_.sendContent(items[i]);
        emitted++;
      }
    }
  } else if (SD.exists(AppConfig::EVENT_FILE_PATH)) {
    File file = SD.open(AppConfig::EVENT_FILE_PATH, "r");
    if (file) {
      while (file.available()) {
        const String line = file.readStringUntil('\n');
        String ts, quality, eventName;
        if (!parseEventRow(line, ts, quality, eventName)) continue;
        if (!isTimestampInRange(ts, startTs, endTs)) continue;
        if (emitted > 0) server_.sendContent(",");
        String item = "{\"ts\":\"" + ts + "\",\"q\":\"" + quality + "\",\"event\":\"" + eventName + "\"}";
        server_.sendContent(item);
        emitted++;
      }
      file.close();
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

  const String format = server_.arg("format");
  if (format == "json" || !SD.exists(AppConfig::EVENT_DIR_PATH)) {
    const String startTs = server_.arg("start");
    const String endTs = server_.arg("end");
    streamEventsJson(startTs, endTs, limit);
  } else {
    streamEventsBinary(limit);
  }
}

void WebManager::handleCreateEvent() {
  if (!ensureSdReady()) {
    sendJsonError(503, "SD card unavailable");
    return;
  }

  String msg = "";
  String ts = "";
  uint8_t category = 1;

  if (server_.hasArg("plain")) {
    StaticJsonDocument<256> doc;
    DeserializationError err = deserializeJson(doc, server_.arg("plain"));
    if (!err) {
      if (doc.containsKey("event")) msg = doc["event"].as<String>();
      if (doc.containsKey("ts")) ts = doc["ts"].as<String>();
      if (doc.containsKey("category")) category = doc["category"].as<uint8_t>();
    }
  }

  if (msg.length() == 0 && server_.hasArg("event")) msg = server_.arg("event");
  if (ts.length() == 0 && server_.hasArg("ts")) ts = server_.arg("ts");

  msg.trim();
  if (msg.length() == 0) {
    sendJsonError(400, "Event message cannot be empty");
    return;
  }

  if (msg.length() > 128) {
    msg = msg.substring(0, 128);
  }

  TimestampQuality quality = TimestampQuality::Estimated;
  if (ts.length() == 0) {
    char tsBuf[32]{};
    if (timeManager_) {
      timeManager_->getTimestamp(tsBuf, sizeof(tsBuf), quality);
      ts = String(tsBuf);
    } else {
      ts = "2026-01-01 00:00:00";
    }
  } else {
    if (timeManager_ && timeManager_->isNtpSynced()) {
      quality = TimestampQuality::Ntp;
    }
  }

  bool ok = loggerManager_->logEvent(msg.c_str(), ts.c_str(), quality, category);
  if (ok) {
    server_.send(200, "application/json", "{\"success\":true}");
  } else {
    sendJsonError(500, "Failed to log event");
  }
}

void WebManager::handleUpdateEvent() {
  if (!ensureSdReady()) {
    sendJsonError(503, "SD card unavailable");
    return;
  }

  String oldTs = "";
  String oldEvent = "";
  String newEvent = "";

  if (server_.hasArg("plain")) {
    StaticJsonDocument<256> doc;
    DeserializationError err = deserializeJson(doc, server_.arg("plain"));
    if (!err) {
      if (doc.containsKey("oldTs")) oldTs = doc["oldTs"].as<String>();
      else if (doc.containsKey("ts")) oldTs = doc["ts"].as<String>();

      if (doc.containsKey("oldEvent")) oldEvent = doc["oldEvent"].as<String>();
      else if (doc.containsKey("oldName")) oldEvent = doc["oldName"].as<String>();

      if (doc.containsKey("event")) newEvent = doc["event"].as<String>();
      else if (doc.containsKey("newEvent")) newEvent = doc["newEvent"].as<String>();
    }
  }

  if (oldTs.length() == 0 && server_.hasArg("oldTs")) oldTs = server_.arg("oldTs");
  if (oldEvent.length() == 0 && server_.hasArg("oldEvent")) oldEvent = server_.arg("oldEvent");
  if (newEvent.length() == 0 && server_.hasArg("event")) newEvent = server_.arg("event");

  newEvent.trim();
  if (newEvent.length() == 0) {
    sendJsonError(400, "New event name cannot be empty");
    return;
  }
  if (newEvent.length() > 128) {
    newEvent = newEvent.substring(0, 128);
  }

  if (!SD.exists(AppConfig::EVENT_DIR_PATH)) {
    sendJsonError(404, "Event directory not found");
    return;
  }

  File dir = SD.open(AppConfig::EVENT_DIR_PATH);
  if (!dir || !dir.isDirectory()) {
    sendJsonError(404, "Event directory unavailable");
    return;
  }

  std::vector<String> binFiles;
  File entry = dir.openNextFile();
  while (entry) {
    if (!entry.isDirectory()) {
      String name = String(entry.name());
      if (name.startsWith("ev_") && name.endsWith(".bin")) {
        binFiles.push_back(name);
      }
    }
    entry.close();
    entry = dir.openNextFile();
  }
  dir.close();

  bool updated = false;
  for (const auto& fname : binFiles) {
    String path = String(AppConfig::EVENT_DIR_PATH) + "/" + fname;
    File file = SD.open(path, "r+");
    if (!file) continue;

    size_t totalRecords = file.size() / sizeof(EventRecord);
    for (size_t r = 0; r < totalRecords; r++) {
      file.seek(r * sizeof(EventRecord));
      EventRecord rec;
      if (file.read(reinterpret_cast<uint8_t*>(&rec), sizeof(EventRecord)) == sizeof(EventRecord)) {
        if (rec.quality == 2 || rec.epochTime == 0) continue;

        char tsBuf[64]{};
        time_t epoch = rec.epochTime;
        struct tm* timeinfo = localtime(&epoch);
        if (timeinfo && timeinfo->tm_year > 70) {
          snprintf(tsBuf, sizeof(tsBuf), "%04u-%02u-%02u %02u:%02u:%02u",
                   (unsigned)(timeinfo->tm_year + 1900), (unsigned)(timeinfo->tm_mon + 1), (unsigned)timeinfo->tm_mday,
                   (unsigned)timeinfo->tm_hour, (unsigned)timeinfo->tm_min, (unsigned)timeinfo->tm_sec);
        } else {
          snprintf(tsBuf, sizeof(tsBuf), "2026-01-01 00:00:00");
        }

        if ((oldTs.length() == 0 || String(tsBuf) == oldTs) &&
            (oldEvent.length() == 0 || strcmp(rec.message, oldEvent.c_str()) == 0)) {
          
          strncpy(rec.message, newEvent.c_str(), sizeof(rec.message) - 1);
          rec.message[sizeof(rec.message) - 1] = '\0';

          file.seek(r * sizeof(EventRecord));
          file.write(reinterpret_cast<const uint8_t*>(&rec), sizeof(EventRecord));
          file.flush();
          file.close();
          updated = true;
          break;
        }
      }
    }
    if (file) file.close();
    if (updated) break;
  }

  if (updated) {
    server_.send(200, "application/json", "{\"success\":true}");
  } else {
    sendJsonError(404, "Matching event record not found");
  }
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

  const size_t fileSize = file.size();
  server_.setContentLength(fileSize);
  server_.sendHeader("Content-Type", contentType);
  server_.sendHeader("Content-Disposition", "attachment; filename=\"" + filename + "\"");
  server_.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server_.send(200, contentType, "");

  WiFiClient client = server_.client();
  uint8_t buf[256];
  size_t bytesSent = 0;
  while (file.available() && bytesSent < fileSize) {
    size_t toRead = sizeof(buf);
    if (fileSize - bytesSent < toRead) {
      toRead = fileSize - bytesSent;
    }
    int n = file.read(buf, toRead);
    if (n <= 0) break;
    client.write(buf, n);
    bytesSent += n;
    if (yieldCallback_) {
      yieldCallback_(yieldCallbackArg_);
    } else {
      yield();
    }
  }
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

