#include "managers/LoggerManager.h"

#include "config/AppConfig.h"

bool LoggerManager::begin(int sdCsPin, int sckPin, int misoPin, int mosiPin) {
  spiSd_.begin(sckPin, misoPin, mosiPin, sdCsPin);
  sdHealthy_ = SD.begin(sdCsPin, spiSd_);
  Serial.printf("[Logger] SD init: %s\n", sdHealthy_ ? "ok" : "failed");

  if (!sdHealthy_) {
    return false;
  }

  if (!ensurePathsAndHeaders()) {
    sdHealthy_ = false;
    return false;
  }

  return true;
}

bool LoggerManager::enqueueSample(const Sample& sample) {
  const bool insertedWithoutOverwrite = queue_.push(sample);
  if (!insertedWithoutOverwrite) {
    ++droppedSamples_;
  }
  return insertedWithoutOverwrite;
}

bool LoggerManager::flushIfDue(uint32_t nowMs, uint32_t flushIntervalMs) {
  if ((nowMs - lastFlushMs_) < flushIntervalMs) {
    return true;
  }
  lastFlushMs_ = nowMs;
  return flush();
}

bool LoggerManager::flush() {
  if (queue_.empty()) {
    return true;
  }

  if (!sdHealthy_) {
    Serial.println("[Logger] SD unhealthy, keeping queue in RAM");
    return false;
  }

  File file = SD.open(AppConfig::LOG_FILE_PATH, FILE_APPEND);
  if (!file) {
    sdHealthy_ = false;
    Serial.println("[Logger] Failed to open CSV for append");
    return false;
  }

  bool allWritten = true;
  while (!queue_.empty()) {
    Sample sample;
    if (!queue_.peek(sample)) {
      break;
    }

    const int written = file.printf("%s,%s,%.2f,%.2f,%.2f,%lu\n",
                                    sample.timestamp,
                                    TimestampQualityToString(sample.quality),
                                    sample.temperatureC,
                                    sample.humidityPct,
                                    sample.pressureHpa,
                                    static_cast<unsigned long>(sample.uptimeSeconds));
    if (written <= 0) {
      allWritten = false;
      sdHealthy_ = false;
      Serial.println("[Logger] CSV write failed, retry on next flush");
      break;
    }

    Sample unused;
    queue_.pop(unused);
  }

  file.flush();
  file.close();
  return allWritten;
}

bool LoggerManager::logEvent(const char* eventName, const char* timestamp, TimestampQuality quality) {
  if (!sdHealthy_) {
    Serial.printf("[Event] %s at %s (%s)\n", eventName, timestamp, TimestampQualityToString(quality));
    return false;
  }

  File file = SD.open(AppConfig::EVENT_FILE_PATH, FILE_APPEND);
  if (!file) {
    sdHealthy_ = false;
    Serial.println("[Logger] Failed to append event log");
    return false;
  }

  const int written = file.printf("%s,%s,%s\n", timestamp, TimestampQualityToString(quality), eventName);
  file.flush();
  file.close();

  if (written <= 0) {
    sdHealthy_ = false;
    Serial.println("[Logger] Event write failed");
    return false;
  }

  return true;
}

bool LoggerManager::ensurePathsAndHeaders() {
  if (!ensureDir("/logs")) {
    Serial.println("[Logger] Failed to create /logs directory");
    return false;
  }
  return writeCsvHeaderIfMissing() && writeEventHeaderIfMissing();
}

bool LoggerManager::ensureDir(const char* path) {
  if (SD.exists(path)) {
    return true;
  }
  return SD.mkdir(path);
}

bool LoggerManager::writeCsvHeaderIfMissing() {
  if (SD.exists(AppConfig::LOG_FILE_PATH)) {
    return true;
  }

  File file = SD.open(AppConfig::LOG_FILE_PATH, FILE_WRITE);
  if (!file) {
    return false;
  }

  const int written = file.println("timestamp,timestamp_quality,temp_c,humidity_pct,pressure_hpa,uptime_s");
  file.close();
  return written > 0;
}

bool LoggerManager::writeEventHeaderIfMissing() {
  if (SD.exists(AppConfig::EVENT_FILE_PATH)) {
    return true;
  }

  File file = SD.open(AppConfig::EVENT_FILE_PATH, FILE_WRITE);
  if (!file) {
    return false;
  }

  const int written = file.println("timestamp,timestamp_quality,event");
  file.close();
  return written > 0;
}
