#pragma once

#include <Arduino.h>

#include "models/Sample.h"

class TimeManager {
 public:
  void begin();
  void update(uint32_t nowMs, uint32_t ntpRetryIntervalMs);
  void getTimestamp(char* out, size_t outSize, TimestampQuality& quality) const;
  bool isNtpSynced() const { return ntpSynced_; }
  bool consumeNtpReestablishedFlag();
  void forceNtpRetry();

 private:
  bool trySyncTime();
  void formatEpoch(time_t epochSeconds, char* out, size_t outSize) const;

  bool ntpSynced_ = false;
  bool ntpReestablished_ = false;
  uint32_t lastSyncAttemptMs_ = 0;
};
