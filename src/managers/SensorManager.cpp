#include "managers/SensorManager.h"

#include <Arduino.h>
#include <Wire.h>

bool SensorManager::begin(int sdaPin, int sclPin, uint8_t i2cAddress) {
  Wire.begin(sdaPin, sclPin);
  ready_ = bme_.begin(i2cAddress, &Wire);
  if (!ready_) {
    Serial.println("[Sensor] BME280 init failed");
    return false;
  }

  bme_.setSampling(Adafruit_BME280::MODE_NORMAL,
                   Adafruit_BME280::SAMPLING_X2,
                   Adafruit_BME280::SAMPLING_X16,
                   Adafruit_BME280::SAMPLING_X16,
                   Adafruit_BME280::FILTER_X16,
                   Adafruit_BME280::STANDBY_MS_500);

  Serial.println("[Sensor] BME280 ready");
  return true;
}

bool SensorManager::read(float& temperatureC, float& humidityPct, float& pressureHpa) {
  if (!ready_) {
    return false;
  }

  temperatureC = bme_.readTemperature();
  humidityPct = bme_.readHumidity();
  pressureHpa = bme_.readPressure() / 100.0f;

  if (isnan(temperatureC) || isnan(humidityPct) || isnan(pressureHpa)) {
    Serial.println("[Sensor] Invalid sensor reading");
    return false;
  }

  return true;
}
