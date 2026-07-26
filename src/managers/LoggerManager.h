#pragma once

#include <Arduino.h>
#include <FS.h>
#include <SD.h>
#include <SPI.h>

#include "config/AppConfig.h"
#include "models/Sample.h"
#include "models/DeviceConfig.h"
#include "utils/RingBuffer.h"

class LoggerManager {
 public:
  bool begin(int sdCsPin, int sckPin, int misoPin, int mosiPin);
  bool enqueueSample(const Sample& sample);
  bool flush();
  bool flushIfDue(uint32_t nowMs, uint32_t flushIntervalMs);
  bool logEvent(const char* eventName, const char* timestamp, TimestampQuality quality, uint8_t category = 0);
  bool logEvent(const char* eventName, time_t epochTime, TimestampQuality quality, uint8_t category = 0);
  bool updateEventRecord(const char* filepath, size_t slotIndex, const EventRecord& record);
  bool logBattery(time_t epochTime, float voltage, int percent, const char* status, int32_t timeRemainingS);
  bool calibrateEstimatedLogs(time_t bootEpoch);
  bool saveDeviceConfig(const DeviceConfig& config);
  bool loadDeviceConfig(DeviceConfig& config);

  bool forceRetry();
  bool attemptRecovery();

  size_t queueDepth() const { return queue_.size(); }
  size_t queueCapacity() const { return queue_.capacity(); }
  uint32_t droppedSamples() const { return droppedSamples_; }
  bool isSdHealthy() const { return sdHealthy_; }
  const char* getSdDiagDetail() const { return sdDiagDetail_; }
  bool checkAndClearSdJustRecovered() {
    bool rec = sdJustRecovered_;
    sdJustRecovered_ = false;
    return rec;
  }

 private:
  bool initSdWithRetries(int sdCsPin);
  bool ensurePathsAndHeaders();
  bool ensureDir(const char* path);
  bool writeCsvHeaderIfMissing();
  bool writeEventHeaderIfMissing();
  void calibrateCsvFile(const char* filepath, time_t bootEpoch);
  void initBatteryLogFile();
  void initEventStorage();
  bool getActiveEventChunkPath(char* outPath, size_t maxPathLen, size_t& outSlotIndex, time_t currentEpoch);
  bool preallocateEventChunk(const char* filepath);
  void migrateLegacyEventsCsv();
  size_t batteryWriteIndex_ = 0;

  RingBuffer<Sample, AppConfig::MAX_LOG_QUEUE_SIZE> queue_;
  bool sdHealthy_ = false;
  bool sdJustRecovered_ = false;
  char sdDiagDetail_[96] = "Not initialized";
  uint32_t droppedSamples_ = 0;
  uint32_t lastFlushMs_ = 0;

  int sdCsPin_ = -1;
  int sckPin_ = -1;
  int misoPin_ = -1;
  int mosiPin_ = -1;
  uint32_t lastSdRetryMs_ = 0;
  uint32_t sdRetryIntervalMs_ = 15000; // Start with 15s retry interval
};
