#include "managers/LoggerManager.h"

#include "config/AppConfig.h"

bool LoggerManager::begin(int sdCsPin, int sckPin, int misoPin, int mosiPin) {
  sdCsPin_ = sdCsPin;
  sckPin_ = sckPin;
  misoPin_ = misoPin;
  mosiPin_ = mosiPin;

  (void)sckPin;
  (void)misoPin;
  (void)mosiPin;

  sdHealthy_ = initSdWithRetries(sdCsPin);

  Serial.printf("[Logger] SD init: %s (%s)\n", sdHealthy_ ? "ok" : "failed", sdDiagDetail_);

  if (!sdHealthy_) {
    return false;
  }

  if (!ensurePathsAndHeaders()) {
    snprintf(sdDiagDetail_, sizeof(sdDiagDetail_), "FS write failure (read-only/corrupt?)");
    Serial.printf("[Logger] SD post-init error: %s (Check write-protect switch, card format, or capacity)\n", sdDiagDetail_);
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
  if (!sdHealthy_ && sdCsPin_ != -1) {
    if (nowMs - lastSdRetryMs_ >= sdRetryIntervalMs_) {
      lastSdRetryMs_ = nowMs;
      Serial.printf("[Logger] SD card unhealthy. Periodic retry attempt... (interval: %ds, queue: %d/%d)\n",
                    sdRetryIntervalMs_ / 1000, queue_.size(), queue_.capacity());

      if (attemptRecovery()) {
        sdRetryIntervalMs_ = 15000; // Reset retry interval on success
      } else {
        // Exponential backoff capped at 5 minutes
        sdRetryIntervalMs_ = min(sdRetryIntervalMs_ * 2, (uint32_t)300000);
      }
    }
  }

  if ((nowMs - lastFlushMs_) < flushIntervalMs) {
    return true;
  }
  lastFlushMs_ = nowMs;
  return flush();
}

static bool preAllocateDailyFile(File& file) {
  // 86400 records * 17 bytes = 1,468,800 bytes.
  // Chunk size: 16 records * 17 bytes = 272 bytes (very safe for ESP8266 stack)
  LogRecord emptyRecords[16];
  for (int i = 0; i < 16; ++i) {
    memset(&emptyRecords[i], 0, sizeof(LogRecord));
    emptyRecords[i].quality = 2; // Empty/Invalid
  }

  for (int chunk = 0; chunk < 5400; ++chunk) {
    size_t written = file.write(reinterpret_cast<const uint8_t*>(emptyRecords), 16 * sizeof(LogRecord));
    if (written < 16 * sizeof(LogRecord)) {
      return false;
    }
    if (chunk % 32 == 0) {
      yield(); // Yield periodically to prevent hardware watchdog timeouts and keep WiFi alive
    }
  }

  file.flush();
  return true;
}

bool LoggerManager::flush() {
  if (queue_.empty()) {
    return true;
  }

  if (!sdHealthy_) {
    Serial.println("[Logger] SD unhealthy, keeping queue in RAM");
    return false;
  }

  const size_t initialSize = queue_.size();
  Serial.printf("[Logger] Flushing %u samples to SD card...\n", initialSize);

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
    bool exists = SD.exists(filepath);
    if (exists) {
      File checkFile = SD.open(filepath, "r");
      if (checkFile) {
        size_t foundSize = checkFile.size();
        if (foundSize != 86400 * sizeof(LogRecord)) {
          exists = false;
        }
        checkFile.close();
      } else {
        exists = false;
      }
    }

    if (!exists) {
      if (!SD.exists("/logs")) {
        SD.mkdir("/logs");
      }
      if (SD.exists(filepath)) {
        SD.remove(filepath);
      }
      file = SD.open(filepath, "w+"); // Open in read/write/create mode
      if (file) {
        bool success = preAllocateDailyFile(file);
        if (!success) {
          Serial.printf("[Logger] Failed to pre-allocate binary file %s\n", filepath.c_str());
          file.close();
          SD.remove(filepath); // Clean up incomplete file
          allWritten = false;
          sdHealthy_ = false;
          break;
        }
      }
    } else {
      file = SD.open(filepath, "r+"); // Open for random write
    }

    if (!file) {
      allWritten = false;
      sdHealthy_ = false;
      Serial.printf("[Logger] Failed to open binary file %s (mode=%s)\n", filepath.c_str(), exists ? "r+" : "w+");
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

  if (allWritten) {
    Serial.printf("[Logger] Flush successful: %u samples written to SD card\n", initialSize);
  } else {
    Serial.println("[Logger] Flush failed or incomplete");
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
  // Explicitly configure CS pin as output and hold HIGH to deselect card
  pinMode(sdCsPin, OUTPUT);
  digitalWrite(sdCsPin, HIGH);
  delay(50);

  // Cycle CS pin to reset the SD card's internal SPI controller
  digitalWrite(sdCsPin, LOW);
  delay(10);
  digitalWrite(sdCsPin, HIGH);
  delay(50);

  Serial.printf("[Logger] Testing SD.begin on CS pin %d...\n", sdCsPin);

  if (SD.begin(sdCsPin)) {
    snprintf(sdDiagDetail_, sizeof(sdDiagDetail_), "Init @ default speed");
    Serial.println("[Logger] SD card mounted successfully at default speed.");
    return true;
  }

  Serial.println("[Logger] SD.begin failed at default speed. Trying half speed...");
  delay(100);
  if (SD.begin(sdCsPin, SPI_HALF_SPEED)) {
    snprintf(sdDiagDetail_, sizeof(sdDiagDetail_), "Init @ half speed");
    Serial.println("[Logger] SD card mounted successfully at half speed.");
    return true;
  }

  Serial.println("[Logger] SD.begin failed at half speed. Trying quarter speed...");
  delay(100);
  if (SD.begin(sdCsPin, SPI_QUARTER_SPEED)) {
    snprintf(sdDiagDetail_, sizeof(sdDiagDetail_), "Init @ quarter speed");
    Serial.println("[Logger] SD card mounted successfully at quarter speed.");
    return true;
  }

  Serial.println("[Logger] SD.begin failed at all speeds. Verify wiring (CS pad to GPIO15 jumper), card insertion, and FAT32 format.");
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

bool LoggerManager::forceRetry() {
  if (sdHealthy_) {
    return true;
  }

  Serial.println("[Logger] Forced SD card retry requested...");
  return attemptRecovery();
}

bool LoggerManager::attemptRecovery() {
  if (sdCsPin_ == -1) {
    return false;
  }

  // Release any lockups/half-states in filesystem
  SD.end();
  delay(50);

  if (begin(sdCsPin_, sckPin_, misoPin_, mosiPin_)) {
    Serial.println("[Logger] SD card recovered and mounted!");
    // Flush pending queue immediately on recovery
    flush();
    return true;
  }

  return false;
}

bool LoggerManager::logBattery(const char* timestamp, float voltage, int percent, const char* status) {
  if (!sdHealthy_) {
    return false;
  }

  const char* filepath = "/logs/battery.csv";
  bool exists = SD.exists(filepath);

  File file = SD.open(filepath, "a");
  if (!file) {
    sdHealthy_ = false;
    Serial.println("[Logger] Failed to open /logs/battery.csv for writing");
    return false;
  }

  if (!exists || file.size() == 0) {
    file.println("timestamp,voltage,percent,status");
  }

  const int written = file.printf("%s,%.2f,%d,%s\n", timestamp, voltage, percent, status);
  file.flush();
  file.close();

  if (written <= 0) {
    sdHealthy_ = false;
    Serial.println("[Logger] Battery log write failed");
    return false;
  }

  return true;
}

bool LoggerManager::calibrateEstimatedLogs(time_t bootEpoch) {
  if (bootEpoch == 0) {
    return false;
  }

  Serial.printf("[Logger] Calibrating estimated logs with boot epoch %lld\n", (long long)bootEpoch);

  // 1. Calibrate in-memory queue
  size_t qSize = queue_.size();
  for (size_t i = 0; i < qSize; ++i) {
    Sample& sample = queue_[i];
    if (sample.quality == TimestampQuality::Estimated) {
      time_t sampleEpoch = bootEpoch + sample.uptimeSeconds;
      struct tm timeInfo;
      localtime_r(&sampleEpoch, &timeInfo);
      strftime(sample.timestamp, sizeof(sample.timestamp), "%Y-%m-%d %H:%M:%S", &timeInfo);
      sample.quality = TimestampQuality::Ntp;
    }
  }
  Serial.println("[Logger] Calibrated in-memory queue");

  if (!sdHealthy_) {
    return false;
  }

  // 2. Calibrate /logs/estimated.bin and write to daily files
  if (SD.exists("/logs/estimated.bin")) {
    File estFile = SD.open("/logs/estimated.bin", "r");
    if (estFile) {
      Serial.println("[Logger] Found /logs/estimated.bin, moving records...");
      uint32_t movedCount = 0;
      while (estFile.available() >= (int)sizeof(LogRecord)) {
        LogRecord record;
        if (estFile.read(reinterpret_cast<uint8_t*>(&record), sizeof(LogRecord)) != sizeof(LogRecord)) {
          break;
        }

        time_t recordEpoch = bootEpoch + record.uptimeSeconds;
        struct tm timeInfo;
        localtime_r(&recordEpoch, &timeInfo);

        char dateStr[16];
        strftime(dateStr, sizeof(dateStr), "%Y-%m-%d", &timeInfo);

        uint32_t slotIndex = timeInfo.tm_hour * 3600 + timeInfo.tm_min * 60 + timeInfo.tm_sec;
        if (slotIndex >= 86400) {
          continue;
        }

        String filepath = "/logs/" + String(dateStr) + ".bin";
        bool exists = SD.exists(filepath);
        if (exists) {
          File checkFile = SD.open(filepath, "r");
          if (checkFile) {
            if (checkFile.size() != 86400 * sizeof(LogRecord)) {
              exists = false;
            }
            checkFile.close();
          } else {
            exists = false;
          }
        }

        File dailyFile;
        if (!exists) {
          if (SD.exists(filepath)) {
            SD.remove(filepath);
          }
          dailyFile = SD.open(filepath, "w+");
          if (dailyFile) {
            preAllocateDailyFile(dailyFile);
          }
        } else {
          dailyFile = SD.open(filepath, "r+");
        }

        if (dailyFile) {
          record.quality = 0; // Set to Ntp (0)
          uint32_t byteOffset = slotIndex * sizeof(LogRecord);
          dailyFile.seek(byteOffset);
          dailyFile.write(reinterpret_cast<const uint8_t*>(&record), sizeof(LogRecord));
          dailyFile.close();
          movedCount++;
        }
      }
      estFile.close();
      SD.remove("/logs/estimated.bin");
      Serial.printf("[Logger] Calibrated and moved %u records from estimated.bin to daily files\n", movedCount);
    }
  }

  // 3. Calibrate /logs/events.csv
  calibrateCsvFile(AppConfig::EVENT_FILE_PATH, bootEpoch);

  // 4. Calibrate /logs/battery.csv
  calibrateCsvFile("/logs/battery.csv", bootEpoch);

  return true;
}

void LoggerManager::calibrateCsvFile(const char* filepath, time_t bootEpoch) {
  if (!SD.exists(filepath)) {
    return;
  }

  File inFile = SD.open(filepath, "r");
  if (!inFile) {
    return;
  }

  String tempPath = String(filepath) + ".tmp";
  if (SD.exists(tempPath)) {
    SD.remove(tempPath);
  }
  File outFile = SD.open(tempPath, "w");
  if (!outFile) {
    inFile.close();
    return;
  }

  Serial.printf("[Logger] Calibrating CSV file %s...\n", filepath);

  bool isHeader = true;
  uint32_t calibratedCount = 0;

  while (inFile.available()) {
    String line = inFile.readStringUntil('\n');
    if (line.length() == 0) {
      continue;
    }

    if (line.endsWith("\r")) {
      line.remove(line.length() - 1);
    }

    if (isHeader) {
      outFile.println(line);
      isHeader = false;
      continue;
    }

    if (line.startsWith("uptime+")) {
      int commaIdx = line.indexOf(',');
      if (commaIdx != -1) {
        String tsPart = line.substring(7, commaIdx - 1); // Extract uptime seconds part
        uint32_t uptimeSec = tsPart.toInt();
        time_t realEpoch = bootEpoch + uptimeSec;

        struct tm timeInfo;
        localtime_r(&realEpoch, &timeInfo);
        char realTs[24];
        strftime(realTs, sizeof(realTs), "%Y-%m-%d %H:%M:%S", &timeInfo);

        int secondCommaIdx = line.indexOf(',', commaIdx + 1);
        String remaining;
        if (secondCommaIdx != -1) {
          String qualityField = line.substring(commaIdx + 1, secondCommaIdx);
          if (qualityField == "estimated") {
            remaining = ",ntp" + line.substring(secondCommaIdx);
          } else {
            remaining = line.substring(commaIdx);
          }
        } else {
          remaining = line.substring(commaIdx);
        }

        outFile.print(realTs);
        outFile.println(remaining);
        calibratedCount++;
        continue;
      }
    }

    outFile.println(line);
  }

  inFile.close();
  outFile.close();

  // Copy temp file back to original file
  File tempFile = SD.open(tempPath, "r");
  if (!tempFile) {
    return;
  }

  if (SD.exists(filepath)) {
    SD.remove(filepath);
  }
  File origFile = SD.open(filepath, "w");
  if (origFile) {
    uint8_t copyBuf[256];
    while (tempFile.available()) {
      int bytesRead = tempFile.read(copyBuf, sizeof(copyBuf));
      if (bytesRead <= 0) break;
      origFile.write(copyBuf, bytesRead);
    }
    origFile.close();
  }
  tempFile.close();
  SD.remove(tempPath);

  Serial.printf("[Logger] CSV file %s calibration finished. %u lines calibrated\n", filepath, calibratedCount);
}
