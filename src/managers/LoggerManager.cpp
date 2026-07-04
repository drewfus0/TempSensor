#include "managers/LoggerManager.h"

#include "config/AppConfig.h"

bool LoggerManager::begin(int sdCsPin, int sckPin, int misoPin, int mosiPin) {
  (void)sckPin;
  (void)misoPin;
  (void)mosiPin;

  sdHealthy_ = initSdWithRetries(sdCsPin);

  Serial.printf("[Logger] SD init: %s (%s)\n", sdHealthy_ ? "ok" : "failed", sdDiagDetail_);

  if (!sdHealthy_) {
    return false;
  }

  if (!ensurePathsAndHeaders()) {
    snprintf(sdDiagDetail_, sizeof(sdDiagDetail_), "FS/header setup failed (format?)");
    Serial.printf("[Logger] SD post-init error: %s\n", sdDiagDetail_);
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

  bool allWritten = true;
  while (!queue_.empty()) {
    Sample sample;
    if (!queue_.peek(sample)) {
      break;
    }

    bool valid = (strlen(sample.timestamp) >= 19 &&
                  sample.timestamp[4] == '-' &&
                  sample.timestamp[7] == '-' &&
                  sample.timestamp[10] == ' ' &&
                  sample.timestamp[13] == ':' &&
                  sample.timestamp[16] == ':');

    if (valid) {
      for (int i = 0; i < 19; ++i) {
        if (i == 4 || i == 7 || i == 10 || i == 13 || i == 16) continue;
        if (sample.timestamp[i] < '0' || sample.timestamp[i] > '9') {
          valid = false;
          break;
        }
      }
    }

    if (!valid) {
      // Estimated / offline log format: append sequentially to estimated.bin
      File estFile = SD.open("/logs/estimated.bin", "a");
      if (estFile) {
        LogRecord record;
        record.uptimeSeconds = sample.uptimeSeconds;
        record.temperatureC = sample.temperatureC;
        record.humidityPct = sample.humidityPct;
        record.pressureHpa = sample.pressureHpa;
        record.quality = static_cast<uint8_t>(sample.quality);
        estFile.write(reinterpret_cast<const uint8_t*>(&record), sizeof(LogRecord));
        estFile.close();
      }
      Sample unused;
      queue_.pop(unused);
      continue;
    }

    char dateStr[11];
    strncpy(dateStr, sample.timestamp, 10);
    dateStr[10] = '\0';

    int hour = (sample.timestamp[11] - '0') * 10 + (sample.timestamp[12] - '0');
    int minute = (sample.timestamp[14] - '0') * 10 + (sample.timestamp[15] - '0');
    int second = (sample.timestamp[17] - '0') * 10 + (sample.timestamp[18] - '0');
    uint32_t slotIndex = hour * 3600 + minute * 60 + second;

    if (slotIndex >= 86400) {
      Sample unused;
      queue_.pop(unused);
      continue;
    }

    String filepath = "/logs/" + String(dateStr) + ".bin";
    File file;
    if (!SD.exists(filepath)) {
      file = SD.open(filepath, "w"); // Create file
    } else {
      file = SD.open(filepath, "r+"); // Open for random write
    }

    if (!file) {
      allWritten = false;
      sdHealthy_ = false;
      Serial.printf("[Logger] Failed to open binary file %s\n", filepath.c_str());
      break;
    }

    LogRecord record;
    record.uptimeSeconds = sample.uptimeSeconds;
    record.temperatureC = sample.temperatureC;
    record.humidityPct = sample.humidityPct;
    record.pressureHpa = sample.pressureHpa;
    record.quality = static_cast<uint8_t>(sample.quality);

    uint32_t byteOffset = slotIndex * sizeof(LogRecord);
    file.seek(byteOffset);
    size_t written = file.write(reinterpret_cast<const uint8_t*>(&record), sizeof(LogRecord));

    if (written < sizeof(LogRecord)) {
      allWritten = false;
      sdHealthy_ = false;
      Serial.println("[Logger] Binary write failed, retry on next flush");
      file.close();
      break;
    }

    file.close();
    Sample unused;
    queue_.pop(unused);
  }

  return allWritten;
}

bool LoggerManager::logEvent(const char* eventName, const char* timestamp, TimestampQuality quality) {
  if (!sdHealthy_) {
    Serial.printf("[Event] %s at %s (%s)\n", eventName, timestamp, TimestampQualityToString(quality));
    return false;
  }

  File file = SD.open(AppConfig::EVENT_FILE_PATH, "a");
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

bool LoggerManager::initSdWithRetries(int sdCsPin) {
  if (SD.begin(sdCsPin)) {
    snprintf(sdDiagDetail_, sizeof(sdDiagDetail_), "Init @ default speed");
    return true;
  }

  delay(100);
  if (SD.begin(sdCsPin, SPI_HALF_SPEED)) {
    snprintf(sdDiagDetail_, sizeof(sdDiagDetail_), "Init @ half speed");
    return true;
  }

  delay(100);
  if (SD.begin(sdCsPin, SPI_QUARTER_SPEED)) {
    snprintf(sdDiagDetail_, sizeof(sdDiagDetail_), "Init @ quarter speed");
    return true;
  }

  snprintf(sdDiagDetail_, sizeof(sdDiagDetail_), "No card / wiring / format issue");
  return false;
}

bool LoggerManager::ensurePathsAndHeaders() {
  if (!ensureDir("/logs")) {
    Serial.println("[Logger] Failed to create /logs directory");
    return false;
  }
  return writeEventHeaderIfMissing();
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
