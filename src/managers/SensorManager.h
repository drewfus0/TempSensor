#pragma once

#include <Adafruit_BME280.h>

#include "models/Sample.h"

class SensorManager {
 public:
  bool begin(int sdaPin, int sclPin, uint8_t i2cAddress);
  bool read(float& temperatureC, float& humidityPct, float& pressureHpa);
  bool isReady() const { return ready_; }

 private:
  Adafruit_BME280 bme_;
  bool ready_ = false;
};
