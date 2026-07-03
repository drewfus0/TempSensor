#pragma once

#include <Adafruit_BME280.h>
#include <cstdint>

#include "models/Sample.h"

class SensorManager {
 public:
  bool begin(int sdaPin, int sclPin, uint8_t i2cAddress);
  bool read(float& temperatureC, float& humidityPct, float& pressureHpa);
  bool isReady() const { return ready_; }
  const char* getStatus() const { return status_msg_; }

 private:
  Adafruit_BME280 bme_;
  bool ready_ = false;
  bool simulated_ = false;
  uint32_t sim_sample_count_ = 0;
  const char* status_msg_ = "Not initialized";
};
