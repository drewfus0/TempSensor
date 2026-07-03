#pragma once

#include <ESP8266WebServer.h>
#include <SD.h>

#include "models/Sample.h"
#include "models/SystemHealth.h"

class LoggerManager;
class TimeManager;

class WebManager {
 public:
  bool begin(const char* ssid, const char* password, const char* hostname, LoggerManager* logger = nullptr, TimeManager* time = nullptr);
  void loop();

  void setLatestSample(const Sample* sample) { latestSample_ = sample; }
  void setHealth(const SystemHealth* health) { health_ = health; }

 private:
  void registerRoutes();
  void handleRoot();
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
  void appendSdTree(File entry, String& out, uint8_t depth);
  void appendIndent(String& out, uint8_t depth);

  bool ensureSdReady();
  void sendJsonError(int code, const char* message);
  bool parsePositiveUIntArg(const String& key, uint32_t& out) const;
  bool isTimestampInRange(const String& ts, const String& startTs, const String& endTs) const;
  bool parseHistoryValue(const String& line, const String& metric, String& outTs, String& outQuality, float& outValue) const;
  bool parseEventRow(const String& line, String& outTs, String& outQuality, String& outEvent) const;
  void streamHistoryJson(File& file, const String& metric, const String& startTs, const String& endTs, uint32_t maxPoints, uint32_t offset = 0, uint32_t limit = 1000);
  void streamEventsJson(File& file, const String& startTs, const String& endTs, uint32_t limit);

  ESP8266WebServer server_{80};
  const Sample* latestSample_ = nullptr;
  const SystemHealth* health_ = nullptr;
  LoggerManager* loggerManager_ = nullptr;
  TimeManager* timeManager_ = nullptr;
};
