#pragma once

#include <Arduino.h>
#include <GxEPD2_BW.h>

#include "config/AppConfig.h"
#include "models/Sample.h"

class DisplayManager {
 public:
  bool begin(int csPin, int dcPin, int rstPin, int busyPin, int sckPin, int mosiPin);
  void render(const Sample& sample, const float* tempValues, size_t tempCount, uint32_t windowSeconds);
  bool isReady() const { return ready_; }

 private:
  bool ready_ = false;
  GxEPD2_BW<GxEPD2_1160_T91, GxEPD2_1160_T91::HEIGHT> display_ =
      GxEPD2_BW<GxEPD2_1160_T91, GxEPD2_1160_T91::HEIGHT>(
          GxEPD2_1160_T91(AppConfig::EINK_CS_PIN,
                          AppConfig::EINK_DC_PIN,
                          AppConfig::EINK_RST_PIN,
                          AppConfig::EINK_BUSY_PIN));
};
