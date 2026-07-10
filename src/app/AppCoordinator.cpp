#include "app/AppCoordinator.h"

#include <ESP8266WiFi.h>
#include <SPI.h>
#include <ArduinoOTA.h>

void AppCoordinator::begin() {
  Serial.println("\n[App] Booting TempSensor milestone-1 firmware...");

  // Drive CS pin HIGH immediately on boot to prevent the SD card from interpreting startup noise
  pinMode(AppConfig::SD_CS_PIN, OUTPUT);
  digitalWrite(AppConfig::SD_CS_PIN, HIGH);
  delay(50); // Let power stabilize

  displayManager_.begin(AppConfig::I2C_SDA_PIN, AppConfig::I2C_SCL_PIN);
  displayManager_.showStartupStatus("Boot", "Initializing...");

  // Initialize shared SPI bus for ESP8266 hardware SPI pins.
  SPI.begin();

  batteryManager_.begin();

  const bool sensorOk =
      sensorManager_.begin(AppConfig::I2C_SDA_PIN, AppConfig::I2C_SCL_PIN, AppConfig::BME280_I2C_ADDR);
  displayManager_.showStartupStatus("Sensor", sensorManager_.getStatus(), nullptr, !sensorOk);

  const bool sdOk = loggerManager_.begin(
      AppConfig::SD_CS_PIN, AppConfig::SD_SCK_PIN, AppConfig::SD_MISO_PIN, AppConfig::SD_MOSI_PIN);
  displayManager_.showStartupStatus("SD", sdOk ? "Ready" : "Init failed", loggerManager_.getSdDiagDetail(), !sdOk);

  webManager_.begin(AppConfig::WIFI_SSID, AppConfig::WIFI_PASSWORD, AppConfig::HOSTNAME, &loggerManager_, &timeManager_, &displayManager_);
  webManager_.registerYieldCallback([](void* arg) {
    auto* self = static_cast<AppCoordinator*>(arg);
    self->handleSampling(millis());
  }, this);

  const bool wifiConnected = (WiFi.status() == WL_CONNECTED);
  String wifiDetail = wifiConnected ? WiFi.localIP().toString() : String("Offline mode");
  displayManager_.showStartupStatus("WiFi", wifiConnected ? "Connected" : "Not connected", wifiDetail.c_str());

  timeManager_.begin();
  displayManager_.showStartupStatus("NTP", timeManager_.isNtpSynced() ? "Synced" : "Estimated clock",
                                    nullptr, !timeManager_.isNtpSynced());

  webManager_.setLatestSample(&latestSample_);
  webManager_.setHealth(&health_);

  if (!sensorOk || !sdOk) {
    displayManager_.showStartupStatus("Startup", "Completed with errors", nullptr, true);
  } else {
    displayManager_.showStartupStatus("Startup", "All systems ready");
  }

  refreshHealth(millis());
  Serial.println("[App] Startup complete");
}

void AppCoordinator::loop() {
  const uint32_t nowMs = millis();

  timeManager_.update(nowMs, AppConfig::NTP_RETRY_INTERVAL_MS);
  batteryManager_.update(nowMs);
  if (timeManager_.consumeNtpReestablishedFlag()) {
    char ts[32]{};
    TimestampQuality quality = TimestampQuality::Estimated;
    timeManager_.getTimestamp(ts, sizeof(ts), quality);
    loggerManager_.logEvent("ntp_reestablished", ts, quality);
    Serial.println("[Time] NTP re-established");
  }

  handleSampling(nowMs);
  loggerManager_.flushIfDue(nowMs, AppConfig::LOG_FLUSH_INTERVAL_MS);
  handleDisplayRefresh(nowMs);
  handleDiagnostics(nowMs);
  refreshHealth(nowMs);

  // Log battery statistics to /logs/battery.csv every 60 seconds
  if (nowMs - lastBatteryLogMs_ >= 60000) {
    lastBatteryLogMs_ = nowMs;
    char ts[32]{};
    TimestampQuality quality = TimestampQuality::Estimated;
    timeManager_.getTimestamp(ts, sizeof(ts), quality);
    loggerManager_.logBattery(ts, batteryManager_.getVoltage(), batteryManager_.getPercent(), batteryManager_.getStatus());
  }

  if (WiFi.status() == WL_CONNECTED) {
    if (!otaInitialized_) {
      setupOta();
    }
    ArduinoOTA.handle();
  }

  webManager_.loop();
}

void AppCoordinator::handleSampling(uint32_t nowMs) {
  if ((nowMs - lastSampleMs_) < AppConfig::SAMPLE_INTERVAL_MS) {
    return;
  }
  lastSampleMs_ = nowMs;

  float tempC = 0.0f;
  float humidityPct = 0.0f;
  float pressureHpa = 0.0f;

  if (!sensorManager_.read(tempC, humidityPct, pressureHpa)) {
    return;
  }

  TimestampQuality quality = TimestampQuality::Estimated;
  Sample sample{};
  timeManager_.getTimestamp(sample.timestamp, sizeof(sample.timestamp), quality);
  sample.quality = quality;
  sample.temperatureC = tempC;
  sample.humidityPct = humidityPct;
  sample.pressureHpa = pressureHpa;
  sample.uptimeSeconds = nowMs / 1000;

  loggerManager_.enqueueSample(sample);

  latestSample_ = sample;
  hasSample_ = true;
}

void AppCoordinator::handleDisplayRefresh(uint32_t nowMs) {
  if (!hasSample_) {
    return;
  }

  if (lastDisplayMs_ != 0 &&
      (nowMs - lastDisplayMs_) < AppConfig::DISPLAY_REFRESH_INTERVAL_MS) {
    return;
  }
  lastDisplayMs_ = nowMs;

  char ipBuf[20]{};
  if (WiFi.status() == WL_CONNECTED) {
    WiFi.localIP().toString().toCharArray(ipBuf, sizeof(ipBuf));
  } else {
    strncpy(ipBuf, "0.0.0.0", sizeof(ipBuf) - 1);
  }

  displayManager_.renderLatest(latestSample_, (WiFi.status() == WL_CONNECTED), timeManager_.isNtpSynced(),
                               loggerManager_.isSdHealthy(), ipBuf, batteryManager_.getPercent(), batteryManager_.getStatus(),
                               loggerManager_.queueDepth(), loggerManager_.queueCapacity());
}

void AppCoordinator::handleDiagnostics(uint32_t nowMs) {
  if ((nowMs - lastDiagMs_) < AppConfig::DIAGNOSTICS_INTERVAL_MS) {
    return;
  }
  lastDiagMs_ = nowMs;

  refreshHealth(nowMs);
  diagnosticsManager_.printPeriodic(health_);
}

void AppCoordinator::refreshHealth(uint32_t nowMs) {
  health_.uptimeSeconds = nowMs / 1000;
  health_.freeHeapBytes = ESP.getFreeHeap();
  health_.largestFreeBlockBytes = ESP.getMaxFreeBlockSize();
  health_.logQueueDepth = loggerManager_.queueDepth();
  health_.logQueueCapacity = loggerManager_.queueCapacity();
  health_.droppedLogSamples = loggerManager_.droppedSamples();
  health_.wifiConnected = (WiFi.status() == WL_CONNECTED);
  health_.sdHealthy = loggerManager_.isSdHealthy();
  health_.ntpSynced = timeManager_.isNtpSynced();

  health_.batteryVoltage = batteryManager_.getVoltage();
  health_.batteryPercent = batteryManager_.getPercent();
  health_.batteryStatus = batteryManager_.getStatus();
  health_.batteryTimeRemainingSeconds = batteryManager_.getTimeRemainingSeconds();
}

void AppCoordinator::setupOta() {
  ArduinoOTA.setHostname(AppConfig::HOSTNAME);

  ArduinoOTA.onStart([this]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_FS
      type = "filesystem";
    }
    Serial.println("\n[OTA] ArduinoOTA Start: " + type);
    displayManager_.showStartupStatus("OTA UDP", "Flashing...", type.c_str());
  });

  ArduinoOTA.onEnd([this]() {
    Serial.println("\n[OTA] ArduinoOTA Success");
    displayManager_.showStartupStatus("OTA UDP", "Success", "Rebooting...");
  });

  ArduinoOTA.onProgress([this](unsigned int progress, unsigned int total) {
    displayManager_.showOtaProgress(progress, total);
    Serial.print(".");
  });

  ArduinoOTA.onError([this](ota_error_t error) {
    Serial.printf("\n[OTA] ArduinoOTA Error[%u]: ", error);
    const char* errStr = "Unknown";
    if (error == OTA_AUTH_ERROR) errStr = "Auth Failed";
    else if (error == OTA_BEGIN_ERROR) errStr = "Begin Failed";
    else if (error == OTA_CONNECT_ERROR) errStr = "Connect Failed";
    else if (error == OTA_RECEIVE_ERROR) errStr = "Receive Failed";
    else if (error == OTA_END_ERROR) errStr = "End Failed";

    Serial.println(errStr);
    displayManager_.showStartupStatus("OTA UDP", "Failed", errStr, true);
  });

  ArduinoOTA.begin();
  otaInitialized_ = true;
  Serial.println("[OTA] ArduinoOTA service initialized");
}
