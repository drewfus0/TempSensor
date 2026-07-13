#include "managers/TimeManager.h"

#include <time.h>

#include "config/AppConfig.h"

void TimeManager::begin() {
  configTzTime(AppConfig::TIMEZONE_MELBOURNE, AppConfig::NTP_SERVER_1, AppConfig::NTP_SERVER_2);

  ntpSynced_ = trySyncTime();
  Serial.printf("[Time] NTP initial sync: %s\n", ntpSynced_ ? "ok" : "not available");
}

void TimeManager::update(uint32_t nowMs, uint32_t ntpRetryIntervalMs) {
  if ((nowMs - lastSyncAttemptMs_) < ntpRetryIntervalMs) {
    return;
  }
  lastSyncAttemptMs_ = nowMs;

  const bool hadSync = ntpSynced_;
  Serial.println("[Time] Attempting NTP time synchronization...");
  ntpSynced_ = trySyncTime();
  if (ntpSynced_) {
    if (!hadSync) {
      ntpReestablished_ = true;
      Serial.println("[Time] NTP sync SUCCESSFUL");
    }
  } else {
    Serial.println("[Time] NTP sync FAILED, will retry later");
  }
}

void TimeManager::getTimestamp(char* out, size_t outSize, TimestampQuality& quality) const {
  time_t nowEpoch = 0;
  time(&nowEpoch);

  if (ntpSynced_ && nowEpoch > 1700000000) {
    quality = TimestampQuality::Ntp;
    formatEpoch(nowEpoch, out, outSize);
    return;
  }

  quality = TimestampQuality::Estimated;
  const uint32_t uptime = millis() / 1000;
  snprintf(out, outSize, "uptime+%lus", static_cast<unsigned long>(uptime));
}

bool TimeManager::consumeNtpReestablishedFlag() {
  const bool flag = ntpReestablished_;
  ntpReestablished_ = false;
  return flag;
}

void TimeManager::forceNtpRetry() {
  lastSyncAttemptMs_ = 0;
}

bool TimeManager::trySyncTime() {
  struct tm timeInfo;
  const bool ok = getLocalTime(&timeInfo, 1000);
  return ok;
}

void TimeManager::formatEpoch(time_t epochSeconds, char* out, size_t outSize) const {
  struct tm timeInfo;
  localtime_r(&epochSeconds, &timeInfo);
  strftime(out, outSize, "%Y-%m-%d %H:%M:%S", &timeInfo);
}

time_t TimeManager::getBootEpoch() const {
  if (!ntpSynced_) {
    return 0;
  }
  time_t nowEpoch = 0;
  time(&nowEpoch);
  return nowEpoch - (millis() / 1000);
}

void TimeManager::setTimezone(const char* tz) {
  configTzTime(tz, AppConfig::NTP_SERVER_1, AppConfig::NTP_SERVER_2);
}
