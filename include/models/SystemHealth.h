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
};
