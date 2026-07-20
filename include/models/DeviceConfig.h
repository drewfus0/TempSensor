#pragma once

#include <Arduino.h>

struct DeviceConfig {
  char wifiSsid[32] = "";
  char wifiPassword[64] = "";
  char hostname[32] = "";
  char timezone[64] = "";
  uint32_t sampleIntervalMs = 1000;
  uint32_t logFlushIntervalMs = 60000;
  uint32_t displayRefreshIntervalMs = 1000;
  float latitude = -37.8136f;
  float longitude = 144.9631f;
  float batteryRateBaseline = 1000.0f;
};
