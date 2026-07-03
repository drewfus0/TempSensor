#include "app/AppCoordinator.h"

#include <SPI.h>
#include <WiFi.h>
#include <esp_heap_caps.h>

void AppCoordinator::begin() {
  Serial.println("\n[App] Booting TempSensor milestone-1 firmware...");

  // Initialise the shared SPI bus once with all four pins before any manager uses it.
  SPI.begin(AppConfig::SD_SCK_PIN, AppConfig::SD_MISO_PIN, AppConfig::SD_MOSI_PIN);

  sensorManager_.begin(AppConfig::I2C_SDA_PIN, AppConfig::I2C_SCL_PIN, AppConfig::BME280_I2C_ADDR);
  loggerManager_.begin(
      AppConfig::SD_CS_PIN, AppConfig::SD_SCK_PIN, AppConfig::SD_MISO_PIN, AppConfig::SD_MOSI_PIN);
  displayManager_.begin();
  webManager_.begin(AppConfig::WIFI_SSID, AppConfig::WIFI_PASSWORD, AppConfig::HOSTNAME);
  timeManager_.begin();

  webManager_.setLatestSample(&latestSample_);
  webManager_.setHealth(&health_);

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
  tempHistory_.push(tempC);

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

  static float graphData[AppConfig::MAX_RING_BUFFER_SIZE]{};
  const size_t copied = tempHistory_.copyTo(graphData, AppConfig::MAX_RING_BUFFER_SIZE);

  displayManager_.render(latestSample_, graphData, copied, copied * (AppConfig::SAMPLE_INTERVAL_MS / 1000));
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
  health_.largestFreeBlockBytes = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
  health_.graphBufferUsage = tempHistory_.size();
  health_.graphBufferCapacity = tempHistory_.capacity();
  health_.logQueueDepth = loggerManager_.queueDepth();
  health_.logQueueCapacity = loggerManager_.queueCapacity();
  health_.droppedLogSamples = loggerManager_.droppedSamples();
  health_.wifiConnected = (WiFi.status() == WL_CONNECTED);
  health_.sdHealthy = loggerManager_.isSdHealthy();
  health_.ntpSynced = timeManager_.isNtpSynced();
}
