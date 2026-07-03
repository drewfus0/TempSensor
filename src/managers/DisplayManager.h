#pragma once

#include <Arduino.h>

#include "models/Sample.h"

class DisplayManager {
 public:
  bool begin(int sdaPin, int sclPin);
  void showStartupStatus(const char* stage, const char* detail, const char* extra = nullptr, bool isError = false);
  void renderLatest(const Sample& sample, bool wifiConnected, bool ntpSynced, bool sdHealthy, const char* ipAddress);
  bool isReady() const { return ready_; }

 private:
  bool ready_ = false;
};
