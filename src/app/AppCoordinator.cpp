#include "app/AppCoordinator.h"

#include <ESP8266WiFi.h>
#include <SPI.h>

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

  const bool sensorOk =
      sensorManager_.begin(AppConfig::I2C_SDA_PIN, AppConfig::I2C_SCL_PIN, AppConfig::BME280_I2C_ADDR);
  displayManager_.showStartupStatus("Sensor", sensorManager_.getStatus(), nullptr, !sensorOk);

  const bool sdOk = loggerManager_.begin(
      AppConfig::SD_CS_PIN, AppConfig::SD_SCK_PIN, AppConfig::SD_MISO_PIN, AppConfig::SD_MOSI_PIN);
  displayManager_.showStartupStatus("SD", sdOk ? "Ready" : "Init failed", loggerManager_.getSdDiagDetail(), !sdOk);

  webManager_.begin(AppConfig::WIFI_SSID, AppConfig::WIFI_PASSWORD, AppConfig::HOSTNAME, &loggerManager_, &timeManager_);
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
                               loggerManager_.isSdHealthy(), ipBuf);
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
}
