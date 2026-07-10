#pragma once

#include <ESP8266WebServer.h>
#include <SD.h>

#include "models/Sample.h"
#include "models/SystemHealth.h"

class LoggerManager;
class TimeManager;
class DisplayManager;

class WebManager {
 public:
  bool begin(const char* ssid, const char* password, const char* hostname, LoggerManager* logger = nullptr, TimeManager* time = nullptr, DisplayManager* display = nullptr);
  void loop();

  void setLatestSample(const Sample* sample) { latestSample_ = sample; }
  void setHealth(const SystemHealth* health) { health_ = health; }

  using YieldCallback = void (*)(void* arg);
  void registerYieldCallback(YieldCallback cb, void* arg) {
    yieldCallback_ = cb;
    yieldCallbackArg_ = arg;
  }

 private:
  void registerRoutes();
  void handleRoot();
  void handleLocalUPlotJs();
  void handleLocalUPlotCss();
  void handleLiveJson();
  void handleHealthJson();
  void handleConfigGet();
  void handleConfigPost();
  void handleHistoryJson();
  void handleEventsJson();
  void handleLogsJson();
  void handleLogDownload();
  void handleSdTreeText();
  void handleFlushNow();
  void handleNtpRetry();
  void handleOtaUpdatePost();
  void handleOtaUpdateUpload();
  void logRequest();
  void appendSdTree(File entry, String& out, uint8_t depth);
  void appendIndent(String& out, uint8_t depth);

  bool ensureSdReady();
  void sendJsonError(int code, const char* message);
  bool parsePositiveUIntArg(const String& key, uint32_t& out) const;
  bool isTimestampInRange(const String& ts, const String& startTs, const String& endTs) const;
  bool parseHistoryValue(const String& line, const String& metric, String& outTs, String& outQuality, float& outValue) const;
  bool parseEventRow(const String& line, String& outTs, String& outQuality, String& outEvent) const;
  void streamEventsJson(File& file, const String& startTs, const String& endTs, uint32_t limit);

  ESP8266WebServer server_{80};
  const Sample* latestSample_ = nullptr;
  const SystemHealth* health_ = nullptr;
  LoggerManager* loggerManager_ = nullptr;
  TimeManager* timeManager_ = nullptr;
  DisplayManager* displayManager_ = nullptr;
  YieldCallback yieldCallback_ = nullptr;
  void* yieldCallbackArg_ = nullptr;
};
