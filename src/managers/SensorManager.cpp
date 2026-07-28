#include "managers/SensorManager.h"

#include <Arduino.h>
#include <cmath>
#include <Wire.h>

bool SensorManager::begin(int sdaPin, int sclPin, uint8_t i2cAddress) {
  sdaPin_ = sdaPin;
  sclPin_ = sclPin;
  i2cAddress_ = i2cAddress;

  Wire.begin(sdaPin_, sclPin_);
  delay(10);  // Give I2C time to settle
  
  // Try to detect I2C devices for debugging
  Wire.beginTransmission(i2cAddress_);
  uint8_t error = Wire.endTransmission();
  
  if (error != 0) {
    Serial.printf("[Sensor] No I2C ACK at 0x%02X (error=%d)\n", i2cAddress_, error);
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
  ready_ = bme_.begin(i2cAddress_, &Wire);
  if (!ready_) {
    Serial.println("[Sensor] BME280 initialization failed (address mismatch or not BME280)");
    Serial.println("[Sensor] Enabling SIMULATION mode with dummy sensor data");
    simulated_ = true;
    ready_ = true;
    status_msg_ = "SIMULATED (BME280 not found)";
    return true;  // Allow operation in simulated mode
  }

  bme_.setSampling(Adafruit_BME280::MODE_NORMAL,
                   Adafruit_BME280::SAMPLING_X2,
                   Adafruit_BME280::SAMPLING_X16,
                   Adafruit_BME280::SAMPLING_X16,
                   Adafruit_BME280::FILTER_X16,
                   Adafruit_BME280::STANDBY_MS_500);

  simulated_ = false;
  consecutiveFailures_ = 0;
  Serial.println("[Sensor] BME280 ready (REAL sensor)");
  status_msg_ = "REAL sensor active";
  return true;
}

bool SensorManager::recoverBusAndSensor() {
  Serial.println("[Sensor] Attempting I2C bus & BME280 sensor auto-recovery...");

  // Toggle SCL pin 9 times to unstick SDA line if pulled low by slave
  pinMode(sdaPin_, INPUT_PULLUP);
  pinMode(sclPin_, OUTPUT);
  for (int i = 0; i < 9; i++) {
    digitalWrite(sclPin_, LOW);
    delayMicroseconds(10);
    digitalWrite(sclPin_, HIGH);
    delayMicroseconds(10);
  }

  Wire.begin(sdaPin_, sclPin_);
  delay(20);

  bool reInitOk = bme_.begin(i2cAddress_, &Wire);
  if (reInitOk) {
    bme_.setSampling(Adafruit_BME280::MODE_NORMAL,
                     Adafruit_BME280::SAMPLING_X2,
                     Adafruit_BME280::SAMPLING_X16,
                     Adafruit_BME280::SAMPLING_X16,
                     Adafruit_BME280::FILTER_X16,
                     Adafruit_BME280::STANDBY_MS_500);
    ready_ = true;
    simulated_ = false;
    consecutiveFailures_ = 0;
    status_msg_ = "REAL sensor active (recovered)";
    Serial.println("[Sensor] BME280 auto-recovery SUCCESSFUL");
    return true;
  }

  Serial.println("[Sensor] BME280 hardware recovery failed");
  status_msg_ = "Sensor hardware error";
  return false;
}

bool SensorManager::read(float& temperatureC, float& humidityPct, float& pressureHpa) {
  if (simulated_) {
    sim_sample_count_++;
    temperatureC = 22.0f + 2.0f * sinf(sim_sample_count_ * 0.001f);
    humidityPct = 45.0f + 15.0f * cosf(sim_sample_count_ * 0.002f);
    pressureHpa = 1013.25f + 1.0f * sinf(sim_sample_count_ * 0.0005f);
    return true;
  }

  uint32_t nowMs = millis();

  if (!ready_) {
    if (nowMs - lastRecoveryAttemptMs_ >= 5000) {
      lastRecoveryAttemptMs_ = nowMs;
      if (!faultActive_) {
        faultActive_ = true;
        faultStartMs_ = nowMs;
        lastFault_ = SensorFaultType::I2cNoAck;
        snprintf(lastFaultMsg_, sizeof(lastFaultMsg_), "sensor_fault: I2C offline (0x%02X)", i2cAddress_);
        pendingFaultLog_ = true;
      }
      recoverBusAndSensor();
    }
    return false;
  }

  temperatureC = bme_.readTemperature();
  humidityPct = bme_.readHumidity();
  pressureHpa = bme_.readPressure() / 100.0f;

  bool isNan = isnan(temperatureC) || isnan(humidityPct) || isnan(pressureHpa);
  bool isOutOfBounds = (!isNan) && (pressureHpa < 300.0f || pressureHpa > 1200.0f ||
                                    temperatureC < -40.0f || temperatureC > 85.0f ||
                                    humidityPct < 0.0f || humidityPct > 100.0f);

  // Check for stuck / frozen sensor values (10 consecutive identical float readings)
  bool isStuck = false;
  if (!isNan && !isOutOfBounds) {
    if (temperatureC == lastTempC_ && humidityPct == lastHumPct_ && pressureHpa == lastPresHpa_) {
      stuckSampleCount_++;
      if (stuckSampleCount_ >= 10) {
        isStuck = true;
      }
    } else {
      lastTempC_ = temperatureC;
      lastHumPct_ = humidityPct;
      lastPresHpa_ = pressureHpa;
      stuckSampleCount_ = 0;
    }
  }

  if (isNan || isOutOfBounds || isStuck) {
    consecutiveFailures_++;

    if (!faultActive_) {
      faultActive_ = true;
      faultStartMs_ = nowMs;
      if (isNan) {
        lastFault_ = SensorFaultType::InvalidReadingNaN;
        snprintf(lastFaultMsg_, sizeof(lastFaultMsg_), "sensor_fault: Invalid NaN reading");
      } else if (isOutOfBounds) {
        lastFault_ = SensorFaultType::ReadingOutOfBounds;
        snprintf(lastFaultMsg_, sizeof(lastFaultMsg_), "sensor_fault: Out-of-bounds (T=%.1fC, P=%.1fhPa)", temperatureC, pressureHpa);
      } else {
        lastFault_ = SensorFaultType::StuckFrozenValue;
        snprintf(lastFaultMsg_, sizeof(lastFaultMsg_), "sensor_fault: Frozen readings detected (%.1fC)", temperatureC);
      }
      pendingFaultLog_ = true;
    }

    Serial.printf("[Sensor] Fault detected: %s (failure #%u)\n", lastFaultMsg_, consecutiveFailures_);

    if (consecutiveFailures_ >= 3 || isStuck) {
      if (nowMs - lastRecoveryAttemptMs_ >= 2000) {
        lastRecoveryAttemptMs_ = nowMs;
        recoverBusAndSensor();
      }
    }
    return false;
  }

  // Reading succeeded! Check if recovering from previous fault
  if (faultActive_) {
    uint32_t outageSec = (nowMs - faultStartMs_) / 1000;
    faultActive_ = false;
    pendingRecoveryLog_ = true;
    snprintf(lastFaultMsg_, sizeof(lastFaultMsg_), "sensor_recovered: BME280 active after %lus outage", (unsigned long)outageSec);
    Serial.printf("[Sensor] %s\n", lastFaultMsg_);
  }

  consecutiveFailures_ = 0;
  return true;
}

bool SensorManager::checkAndClearFaultEvent(char* outMsgBuf, size_t maxLen) {
  if (!pendingFaultLog_) return false;
  pendingFaultLog_ = false;
  snprintf(outMsgBuf, maxLen, "%s", lastFaultMsg_);
  return true;
}

bool SensorManager::checkAndClearRecoveryEvent(char* outMsgBuf, size_t maxLen) {
  if (!pendingRecoveryLog_) return false;
  pendingRecoveryLog_ = false;
  snprintf(outMsgBuf, maxLen, "%s", lastFaultMsg_);
  return true;
}
