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
  smoothedSecondsPerPercent_ = 1000.0f; // Default for new battery (approx. 27.8 hours total runtime, matching ~31h profile)
  lastStatus_ = "Unknown";
  validDischargeStartMs_ = 0;
  validDischargeStartPercent_ = -1;
  stateConfirmCount_ = 0;
  lastVoltage_ = voltage_;
  baselineChanged_ = false;

  // Initialize history buffer
  voltageHistory_[0] = voltage_;
  historyIndex_ = 1;
  historyCount_ = 1;
  lastHistoryWriteMs_ = millis();
  slope_ = 0.0f;
  
  updateStatusAndPredictions(millis());
}

void BatteryManager::update(uint32_t nowMs) {
  if (nowMs - lastReadMs_ < AppConfig::BATTERY_READ_INTERVAL_MS) {
    return;
  }
  lastReadMs_ = nowMs;
  
  readBattery();

  // Record rolling history every 60 seconds
  if (nowMs - lastHistoryWriteMs_ >= 60000) {
    lastHistoryWriteMs_ = nowMs;
    voltageHistory_[historyIndex_] = voltage_;
    historyIndex_ = (historyIndex_ + 1) % HISTORY_SIZE;
    if (historyCount_ < HISTORY_SIZE) {
      historyCount_++;
    }
    slope_ = calculateSlope();
  }
  
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

float BatteryManager::calculateSlope() const {
  if (historyCount_ < 2) {
    return 0.0f;
  }

  float sumX = 0.0f;
  float sumY = 0.0f;
  float sumXY = 0.0f;
  float sumX2 = 0.0f;

  for (size_t i = 0; i < historyCount_; ++i) {
    float xi = static_cast<float>(i);
    size_t idx = (historyIndex_ - historyCount_ + i + HISTORY_SIZE) % HISTORY_SIZE;
    float yi = voltageHistory_[idx];

    sumX += xi;
    sumY += yi;
    sumXY += xi * yi;
    sumX2 += xi * xi;
  }

  float num = (historyCount_ * sumXY) - (sumX * sumY);
  float den = (historyCount_ * sumX2) - (sumX * sumX);

  if (den == 0.0f) {
    return 0.0f;
  }

  return num / den; // Volts per minute
}

void BatteryManager::updateStatusAndPredictions(uint32_t nowMs) {
  // 1. Calculate instant voltage jump/drop
  float deltaV = (lastVoltage_ > 0.0f) ? (voltage_ - lastVoltage_) : 0.0f;
  lastVoltage_ = voltage_;

  const char* nextStatus = status_;

  // 2. Instant state transitions on high-rate charge/discharge events (plug/unplug)
  if (deltaV >= 0.030f) {
    nextStatus = "Charging / USB";
    stateConfirmCount_ = 0;
  } else if (deltaV <= -0.030f) {
    nextStatus = "Discharging";
    stateConfirmCount_ = 0;
  } else {
    // 3. Debounced transitions using regression slope (requires 3 minutes of history)
    if (historyCount_ >= 3) {
      if (slope_ >= 0.0015f) { // Charging (>1.5mV/min)
        if (strcmp(status_, "Discharging") == 0 || strcmp(status_, "Unknown") == 0) {
          stateConfirmCount_++;
          if (stateConfirmCount_ >= 3) {
            nextStatus = "Charging / USB";
            stateConfirmCount_ = 0;
          }
        } else {
          stateConfirmCount_ = 0;
        }
      } else if (slope_ <= -0.0010f) { // Discharging (<-1.0mV/min)
        if (strcmp(status_, "Charging / USB") == 0 || strcmp(status_, "Full") == 0 || strcmp(status_, "Unknown") == 0) {
          stateConfirmCount_++;
          if (stateConfirmCount_ >= 3) {
            nextStatus = "Discharging";
            stateConfirmCount_ = 0;
          }
        } else {
          stateConfirmCount_ = 0;
        }
      } else {
        // Flat slope: slowly decay confirmation count to reduce hysteresis lag
        if (stateConfirmCount_ > 0) stateConfirmCount_--;
      }
    }
  }

  // 4. Absolute voltage-level state overrides
  if (strcmp(nextStatus, "Charging / USB") == 0) {
    if (percent_ >= 99 && voltage_ >= 4.12f) {
      nextStatus = "Full";
    }
  } else if (strcmp(nextStatus, "Full") == 0) {
    if (percent_ < 95 && voltage_ < 4.05f) {
      nextStatus = "Discharging";
    }
  } else if (strcmp(nextStatus, "Unknown") == 0) {
    nextStatus = "Discharging"; // Default boot fallback
  }

  status_ = nextStatus;

  // 5. Update predictions and evaluate rate on completed discharge window
  if (strcmp(status_, "Discharging") == 0) {
    // Only mark the start of the valid discharge window once battery drops to <= 90%
    // (ignores top 90-100% region where charger re-engagements and surface charge occur)
    if (percent_ <= 90) {
      if (validDischargeStartMs_ == 0) {
        validDischargeStartMs_ = nowMs;
        validDischargeStartPercent_ = percent_;
      }
    }

    // Display predictions using the rock-solid saved baseline rate
    timeRemainingS_ = static_cast<int32_t>(percent_ * smoothedSecondsPerPercent_);
  }
  else if (strcmp(status_, "Charging / USB") == 0 || strcmp(status_, "Full") == 0) {
    // When transitioning from Discharging to Charging/Full, evaluate the completed discharge run
    if (validDischargeStartMs_ > 0 && validDischargeStartPercent_ > 0) {
      uint32_t elapsedS = (nowMs - validDischargeStartMs_) / 1000;
      int drop = validDischargeStartPercent_ - percent_;

      // Require at least a 10% drop and 1 hour of data to consider the discharge run valid
      if (drop >= 10 && elapsedS >= 3600) {
        float runRate = static_cast<float>(elapsedS) / drop;

        // Clamp to realistic bounds (22 to 41 hours total runtime)
        if (runRate < 800.0f) runRate = 800.0f;
        if (runRate > 1500.0f) runRate = 1500.0f;

        // Blend 70% historical baseline + 30% new evaluated run rate
        smoothedSecondsPerPercent_ = (smoothedSecondsPerPercent_ * 0.7f) + (runRate * 0.3f);
        baselineChanged_ = true;

        Serial.printf("[Battery] Validated discharge run: %d%% drop over %u s. New Baseline Rate: %.1f s/%%\n",
                      drop, elapsedS, smoothedSecondsPerPercent_);
      }

      // Reset tracking window
      validDischargeStartMs_ = 0;
      validDischargeStartPercent_ = -1;
    }

    if (strcmp(status_, "Full") == 0) {
      timeRemainingS_ = 0;
    } else if (slope_ > 0.0001f) {
      float remainingVolts = 4.15f - voltage_;
      if (remainingVolts <= 0.0f) {
        timeRemainingS_ = 0;
      } else {
        float minutesToFull = remainingVolts / slope_;
        timeRemainingS_ = static_cast<int32_t>(minutesToFull * 60.0f);
        if (timeRemainingS_ > 86400) {
          timeRemainingS_ = -1;
        }
      }
    } else {
      timeRemainingS_ = -1;
    }
  }

  lastStatus_ = status_;
}
