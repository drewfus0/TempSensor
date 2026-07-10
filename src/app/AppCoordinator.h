#pragma once

#include "config/AppConfig.h"
#include "managers/DiagnosticsManager.h"
#include "managers/DisplayManager.h"
#include "managers/LoggerManager.h"
#include "managers/SensorManager.h"
#include "managers/TimeManager.h"
#include "managers/WebManager.h"
#include "managers/BatteryManager.h"
#include "models/Sample.h"
#include "models/SystemHealth.h"

class AppCoordinator {
 public:
  void begin();
  void loop();

 private:
  void handleSampling(uint32_t nowMs);
  void handleDisplayRefresh(uint32_t nowMs);
  void handleDiagnostics(uint32_t nowMs);
  void refreshHealth(uint32_t nowMs);
  void setupOta();

  SensorManager sensorManager_;
  TimeManager timeManager_;
  LoggerManager loggerManager_;
  DisplayManager displayManager_;
  WebManager webManager_;
  DiagnosticsManager diagnosticsManager_;
  BatteryManager batteryManager_;

  Sample latestSample_{};
  bool hasSample_ = false;
  SystemHealth health_{};

  uint32_t lastSampleMs_ = 0;
  uint32_t lastDisplayMs_ = 0;
  uint32_t lastDiagMs_ = 0;
  uint32_t lastBatteryLogMs_ = 0;
  bool otaInitialized_ = false;
  bool lastWifiConnected_ = false;
};
