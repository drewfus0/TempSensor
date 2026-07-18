#pragma once
#include <Arduino.h>

class BatteryManager {
 public:
  void begin();
  void update(uint32_t nowMs);

  float getVoltage() const { return voltage_; }
  int getPercent() const { return percent_; }
  const char* getStatus() const { return status_; }
  int32_t getTimeRemainingSeconds() const { return timeRemainingS_; }
  float getSlope() const { return slope_; }

 private:
  void readBattery();
  int calculatePercent(float voltage) const;
  void updateStatusAndPredictions(uint32_t nowMs);
  float calculateSlope() const;

  float voltage_ = 0.0f;
  int percent_ = 0;
  const char* status_ = "Unknown";
  int32_t timeRemainingS_ = -1;

  uint32_t lastReadMs_ = 0;
  float emaVoltage_ = -1.0f;
  uint32_t lastPercentChangeMs_ = 0;
  int lastRecordedPercent_ = -1;
  float smoothedSecondsPerPercent_ = 1000.0f;
  const char* lastStatus_ = "Unknown";

  static constexpr size_t HISTORY_SIZE = 10;
  float voltageHistory_[HISTORY_SIZE];
  size_t historyCount_ = 0;
  size_t historyIndex_ = 0;
  uint32_t lastHistoryWriteMs_ = 0;
  float slope_ = 0.0f;
};
