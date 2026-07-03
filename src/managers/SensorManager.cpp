#include "managers/SensorManager.h"

#include <Arduino.h>
#include <cmath>
#include <Wire.h>

bool SensorManager::begin(int sdaPin, int sclPin, uint8_t i2cAddress) {
  Wire.begin(sdaPin, sclPin);
  delay(10);  // Give I2C time to settle
  
  // Try to detect I2C devices for debugging
  Wire.beginTransmission(i2cAddress);
  uint8_t error = Wire.endTransmission();
  
  if (error != 0) {
    Serial.printf("[Sensor] No I2C ACK at 0x%02X (error=%d)\n", i2cAddress, error);
    Serial.println("[Sensor] Scanning I2C bus for any devices...");
    
    int found = 0;
    for (uint8_t addr = 1; addr < 127; addr++) {
      Wire.beginTransmission(addr);
      if (Wire.endTransmission() == 0) {
        Serial.printf("[Sensor]   → Found device at 0x%02X\n", addr);
        found++;
      }
    }
    if (found == 0) {
      Serial.println("[Sensor]   → No I2C devices found!");
    }
  }
  
  // Attempt BME280 initialization
  ready_ = bme_.begin(i2cAddress, &Wire);
  if (!ready_) {
    Serial.println("[Sensor] BME280 initialization failed (address mismatch or not BME280)");
    Serial.println("[Sensor] Enabling SIMULATION mode with dummy sensor data");
    simulated_ = true;
    status_msg_ = "SIMULATED (BME280 not found)";
    return true;  // Allow operation in simulated mode
  }

  bme_.setSampling(Adafruit_BME280::MODE_NORMAL,
                   Adafruit_BME280::SAMPLING_X2,
                   Adafruit_BME280::SAMPLING_X16,
                   Adafruit_BME280::SAMPLING_X16,
                   Adafruit_BME280::FILTER_X16,
                   Adafruit_BME280::STANDBY_MS_500);

  Serial.println("[Sensor] BME280 ready (REAL sensor)");
  status_msg_ = "REAL sensor active";
  return true;
}


bool SensorManager::read(float& temperatureC, float& humidityPct, float& pressureHpa) {
  if (!ready_) {
    return false;
  }

  if (simulated_) {
    // Provide dummy data that looks realistic
    sim_sample_count_++;
    temperatureC = 22.0f + 2.0f * sinf(sim_sample_count_ * 0.001f);  // Oscillate 20-24°C
    humidityPct = 45.0f + 15.0f * cosf(sim_sample_count_ * 0.002f);   // Oscillate 30-60%
    pressureHpa = 1013.25f + 1.0f * sinf(sim_sample_count_ * 0.0005f); // Slight variation
    return true;
  }

  temperatureC = bme_.readTemperature();
  humidityPct = bme_.readHumidity();
  pressureHpa = bme_.readPressure() / 100.0f;

  if (isnan(temperatureC) || isnan(humidityPct) || isnan(pressureHpa)) {
    Serial.println("[Sensor] Invalid sensor reading (NaN)");
    return false;
  }

  return true;
}
