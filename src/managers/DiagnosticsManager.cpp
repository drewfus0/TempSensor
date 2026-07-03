#include "managers/DiagnosticsManager.h"

#include <Arduino.h>

void DiagnosticsManager::printPeriodic(const SystemHealth& health) {
  Serial.printf(
  "[Diag] uptime=%lus free_heap=%lu largest_block=%lu logq=%u/%u dropped=%lu wifi=%s sd=%s ntp=%s\n",
      static_cast<unsigned long>(health.uptimeSeconds),
      static_cast<unsigned long>(health.freeHeapBytes),
      static_cast<unsigned long>(health.largestFreeBlockBytes),
      static_cast<unsigned int>(health.logQueueDepth),
      static_cast<unsigned int>(health.logQueueCapacity),
      static_cast<unsigned long>(health.droppedLogSamples),
      health.wifiConnected ? "up" : "down",
      health.sdHealthy ? "ok" : "bad",
      health.ntpSynced ? "yes" : "no");
}
