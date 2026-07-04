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
