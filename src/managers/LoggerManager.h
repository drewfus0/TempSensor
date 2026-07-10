#pragma once

#include <Arduino.h>
#include <FS.h>
#include <SD.h>
#include <SPI.h>

#include "config/AppConfig.h"
#include "models/Sample.h"
#include "utils/RingBuffer.h"

class LoggerManager {
 public:
  bool begin(int sdCsPin, int sckPin, int misoPin, int mosiPin);
  bool enqueueSample(const Sample& sample);
  bool flush();
  bool flushIfDue(uint32_t nowMs, uint32_t flushIntervalMs);
  bool logEvent(const char* eventName, const char* timestamp, TimestampQuality quality);
  bool logBattery(const char* timestamp, float voltage, int percent, const char* status);
  bool calibrateEstimatedLogs(time_t bootEpoch);

  bool forceRetry();
  bool attemptRecovery();

  size_t queueDepth() const { return queue_.size(); }
  size_t queueCapacity() const { return queue_.capacity(); }
  uint32_t droppedSamples() const { return droppedSamples_; }
  bool isSdHealthy() const { return sdHealthy_; }
  const char* getSdDiagDetail() const { return sdDiagDetail_; }

 private:
  bool initSdWithRetries(int sdCsPin);
  bool ensurePathsAndHeaders();
  bool ensureDir(const char* path);
  bool writeCsvHeaderIfMissing();
  bool writeEventHeaderIfMissing();
  void calibrateCsvFile(const char* filepath, time_t bootEpoch);

  RingBuffer<Sample, AppConfig::MAX_LOG_QUEUE_SIZE> queue_;
  bool sdHealthy_ = false;
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
