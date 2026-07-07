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

 private:
  void readBattery();
  int calculatePercent(float voltage) const;
  void updateStatusAndPredictions(uint32_t nowMs);

  float voltage_ = 0.0f;
  int percent_ = 0;
  const char* status_ = "Unknown";
  int32_t timeRemainingS_ = -1;

  uint32_t lastReadMs_ = 0;
  float emaVoltage_ = -1.0f;
  uint32_t lastPercentChangeMs_ = 0;
  int lastRecordedPercent_ = -1;
  float smoothedSecondsPerPercent_ = 540.0f;
  const char* lastStatus_ = "Unknown";
};
