#include "managers/BatteryManager.h"
#include "config/AppConfig.h"

void BatteryManager::begin() {
  pinMode(AppConfig::BATTERY_ADC_PIN, INPUT);
  
  // Perform multiple readings to warm up the ADC and initialize the EMA filter
  int sum = 0;
  for (int i = 0; i < 10; ++i) {
    sum += analogRead(AppConfig::BATTERY_ADC_PIN);
    delay(10);
  }
  float initialRaw = (sum / 10.0f) * AppConfig::BATTERY_CALIBRATION_FACTOR;
  emaVoltage_ = initialRaw;
  voltage_ = initialRaw;
  percent_ = calculatePercent(voltage_);
  updateStatusAndPredictions(millis());
}

void BatteryManager::update(uint32_t nowMs) {
  if (nowMs - lastReadMs_ < AppConfig::BATTERY_READ_INTERVAL_MS) {
    return;
  }
  lastReadMs_ = nowMs;
  
  readBattery();
  updateStatusAndPredictions(nowMs);
}

void BatteryManager::readBattery() {
  int rawAdc = analogRead(AppConfig::BATTERY_ADC_PIN);
  float rawVoltage = rawAdc * AppConfig::BATTERY_CALIBRATION_FACTOR;
  
  // Apply Exponential Moving Average (EMA) to smooth out raw ADC measurement noise
  if (emaVoltage_ < 0.0f) {
    emaVoltage_ = rawVoltage;
  } else {
    emaVoltage_ = (emaVoltage_ * 0.85f) + (rawVoltage * 0.15f);
  }
  
  voltage_ = emaVoltage_;
  percent_ = calculatePercent(voltage_);
}

int BatteryManager::calculatePercent(float voltage) const {
  // LiPo voltage lookup table/linear interpolation logic
  if (voltage >= 4.15f) return 100;
  if (voltage <= 3.40f) return 0;
  
  // Linear interpolation across segments matching LiPo discharge curves
  if (voltage >= 4.00f) {
    // 4.00V -> 80%, 4.15V -> 100%
    return 80 + static_cast<int>((voltage - 4.00f) / (4.15f - 4.00f) * 20.0f);
  } else if (voltage >= 3.82f) {
    // 3.82V -> 50%, 4.00V -> 80%
    return 50 + static_cast<int>((voltage - 3.82f) / (4.00f - 3.82f) * 30.0f);
  } else if (voltage >= 3.70f) {
    // 3.70V -> 15%, 3.82V -> 50%
    return 15 + static_cast<int>((voltage - 3.70f) / (3.82f - 3.70f) * 35.0f);
  } else {
    // 3.40V -> 0%, 3.70V -> 15%
    return 0 + static_cast<int>((voltage - 3.40f) / (3.70f - 3.40f) * 15.0f);
  }
}

void BatteryManager::updateStatusAndPredictions(uint32_t nowMs) {
  (void)nowMs;
  
  // If the voltage is high, it is connected to USB power (and charging / full)
  if (voltage_ >= 4.15f) {
    if (percent_ >= 99) {
      status_ = "Full";
    } else {
      status_ = "Charging / USB";
    }
    timeRemainingS_ = -1; // Charging / external power has infinite remaining time
  } else {
    status_ = "Discharging";
    // Proportional estimation: 100% capacity corresponds to roughly 15 hours of runtime
    // on a 1200mAh battery drawing ~80mA on average.
    timeRemainingS_ = static_cast<int32_t>(percent_ * 540); // 540 seconds per 1% SoC (15 hours total)
  }
}
