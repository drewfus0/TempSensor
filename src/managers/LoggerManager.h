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

  size_t queueDepth() const { return queue_.size(); }
  size_t queueCapacity() const { return queue_.capacity(); }
  uint32_t droppedSamples() const { return droppedSamples_; }
  bool isSdHealthy() const { return sdHealthy_; }

 private:
  bool ensurePathsAndHeaders();
  bool ensureDir(const char* path);
  bool writeCsvHeaderIfMissing();
  bool writeEventHeaderIfMissing();

  RingBuffer<Sample, AppConfig::MAX_LOG_QUEUE_SIZE> queue_;
  bool sdHealthy_ = false;
  uint32_t droppedSamples_ = 0;
  uint32_t lastFlushMs_ = 0;
};
