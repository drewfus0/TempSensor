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

  // Initialize tracking baseline
  lastRecordedPercent_ = percent_;
  lastPercentChangeMs_ = millis();
  smoothedSecondsPerPercent_ = 540.0f; // Default (15 hours)
  lastStatus_ = "Unknown";
  
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
  // Take 8 readings and average them to filter out raw ADC measurement noise (e.g. from WiFi activity)
  int sum = 0;
  for (int i = 0; i < 8; ++i) {
    sum += analogRead(AppConfig::BATTERY_ADC_PIN);
    delay(2);
  }
  float rawVoltage = (sum / 8.0f) * AppConfig::BATTERY_CALIBRATION_FACTOR;
  
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
  // If the voltage is high, it is connected to USB power (and charging / full)
  if (voltage_ >= 4.15f) {
    if (percent_ >= 99) {
      status_ = "Full";
    } else {
      status_ = "Charging / USB";
    }
    timeRemainingS_ = -1; // Charging / external power has infinite remaining time
    
    // Reset baseline tracking while charging
    lastRecordedPercent_ = percent_;
    lastPercentChangeMs_ = nowMs;
  } else {
    status_ = "Discharging";

    // Reset baseline tracking if we just transitioned to discharging
    if (strcmp(lastStatus_, "Discharging") != 0) {
      lastRecordedPercent_ = percent_;
      lastPercentChangeMs_ = nowMs;
    } 
    // If the percentage dropped, update the dynamic rate
    else if (percent_ < lastRecordedPercent_) {
      uint32_t elapsedMs = nowMs - lastPercentChangeMs_;
      int drop = lastRecordedPercent_ - percent_;

      if (elapsedMs > 0 && drop > 0) {
        float secondsPerPercent = (elapsedMs / 1000.0f) / drop;

        // Bound rate to sanity-check ranges (100 seconds to 3600 seconds per 1%)
        // representing realistic 2.7 to 100 hours of discharge time
        if (secondsPerPercent >= 100.0f && secondsPerPercent <= 3600.0f) {
          // EMA filter: 80% weight to historical smoothed rate, 20% to new observation
          smoothedSecondsPerPercent_ = (smoothedSecondsPerPercent_ * 0.8f) + (secondsPerPercent * 0.2f);
        }
      }

      lastRecordedPercent_ = percent_;
      lastPercentChangeMs_ = nowMs;
    }
    // If the percentage rose (due to noise/voltage recovery), reset baseline
    else if (percent_ > lastRecordedPercent_) {
      lastRecordedPercent_ = percent_;
      lastPercentChangeMs_ = nowMs;
    }

    // Dynamic estimation: remaining capacity (%) * smoothed seconds per 1%
    timeRemainingS_ = static_cast<int32_t>(percent_ * smoothedSecondsPerPercent_);
  }

  lastStatus_ = status_;
}
