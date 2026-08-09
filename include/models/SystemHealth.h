#pragma once

#include <Arduino.h>

struct SystemHealth {
  uint32_t uptimeSeconds = 0;
  uint32_t freeHeapBytes = 0;
  uint32_t largestFreeBlockBytes = 0;
  size_t logQueueDepth = 0;
  size_t logQueueCapacity = 0;
  uint32_t droppedLogSamples = 0;
  bool wifiConnected = false;
  bool sdHealthy = false;
  bool ntpSynced = false;
  bool sensorHealthy = false;
  bool sensorSimulated = false;
  const char* sensorStatus = "Not initialized";

  float batteryVoltage = 0.0f;
  int batteryPercent = 0;
  const char* batteryStatus = "Unknown";
  int32_t batteryTimeRemainingSeconds = -1;
  float batterySlope = 0.0f;
};
