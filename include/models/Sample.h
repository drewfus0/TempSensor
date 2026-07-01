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
