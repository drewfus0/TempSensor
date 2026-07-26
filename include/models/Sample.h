#pragma once

#include <Arduino.h>

enum class TimestampQuality : uint8_t {
  Ntp = 0,
  Estimated = 1,
};

struct Sample {
  char timestamp[32];
  TimestampQuality quality;
  float temperatureC;
  float humidityPct;
  float pressureHpa;
  uint32_t uptimeSeconds;
};

inline const char* TimestampQualityToString(TimestampQuality quality) {
  return (quality == TimestampQuality::Ntp) ? "ntp" : "estimated";
}

struct __attribute__((packed)) LogRecord {
  uint32_t uptimeSeconds;
  float temperatureC;
  float humidityPct;
  float pressureHpa;
  uint8_t quality; // 0 = NTP, 1 = Estimated, 2 = Empty/Invalid
};

struct __attribute__((packed)) BatteryRecord {
  uint32_t epochTime;
  float voltage;
  uint8_t percent;
  uint8_t chargingState; // 0 = Unknown, 1 = Discharging, 2 = Charging / USB, 3 = Full
  int32_t timeRemainingS;
};

struct __attribute__((packed)) EventRecord {
  uint32_t epochTime;   // Unix timestamp (seconds)
  uint8_t quality;      // 0 = NTP, 1 = Estimated, 2 = Empty/Invalid
  uint8_t category;     // 0 = System, 1 = Custom Web, 2 = Button A, 3 = Button B
  char message[128];    // Null-terminated UTF-8 text (128 chars)
  uint8_t reserved[2];  // Alignment padding to 136 bytes total
};

inline uint8_t BatteryStatusToState(const char* status) {
  if (strcmp(status, "Discharging") == 0) return 1;
  if (strcmp(status, "Charging / USB") == 0) return 2;
  if (strcmp(status, "Full") == 0) return 3;
  return 0; // Unknown
}

inline const char* BatteryStateToStatus(uint8_t state) {
  if (state == 1) return "Discharging";
  if (state == 2) return "Charging / USB";
  if (state == 3) return "Full";
  return "Unknown";
}
