#pragma once

#include <Adafruit_BME280.h>
#include <cstdint>

#include "models/Sample.h"

enum class SensorFaultType : uint8_t {
  None = 0,
  I2cNoAck = 1,
  InvalidReadingNaN = 2,
  ReadingOutOfBounds = 3,
  StuckFrozenValue = 4
};

class SensorManager {
 public:
  bool begin(int sdaPin, int sclPin, uint8_t i2cAddress);
  bool read(float& temperatureC, float& humidityPct, float& pressureHpa);
  bool recoverBusAndSensor();
  bool isReady() const { return ready_; }
  bool isSimulated() const { return simulated_; }
  bool hasFault() const { return faultActive_; }
  SensorFaultType getLastErrorType() const { return lastFault_; }
  const char* getLastErrorMsg() const { return lastFaultMsg_; }
  const char* getStatus() const { return status_msg_; }

  bool checkAndClearFaultEvent(char* outMsgBuf, size_t maxLen);
  bool checkAndClearRecoveryEvent(char* outMsgBuf, size_t maxLen);

 private:
  Adafruit_BME280 bme_;
  bool ready_ = false;
  bool simulated_ = false;
  uint32_t sim_sample_count_ = 0;
  uint8_t consecutiveFailures_ = 0;
  uint32_t lastRecoveryAttemptMs_ = 0;
  int sdaPin_ = 4;
  int sclPin_ = 5;
  uint8_t i2cAddress_ = 0x76;
  const char* status_msg_ = "Not initialized";

  SensorFaultType lastFault_ = SensorFaultType::None;
  char lastFaultMsg_[96] = "OK";
  bool faultActive_ = false;
  uint32_t faultStartMs_ = 0;
  bool pendingFaultLog_ = false;
  bool pendingRecoveryLog_ = false;

  float lastTempC_ = -999.0f;
  float lastHumPct_ = -999.0f;
  float lastPresHpa_ = -999.0f;
  uint16_t stuckSampleCount_ = 0;
};
