#pragma once

#if defined(ESP8266)
#include <ESP8266WebServer.h>
using TempWebServer = ESP8266WebServer;
#else
#include <WebServer.h>
using TempWebServer = WebServer;
#endif

#include "models/Sample.h"
#include "models/SystemHealth.h"

class WebManager {
 public:
  bool begin(const char* ssid, const char* password, const char* hostname);
  void loop();

  void setLatestSample(const Sample* sample) { latestSample_ = sample; }
  void setHealth(const SystemHealth* health) { health_ = health; }

 private:
  void registerRoutes();
  void handleRoot();
  void handleLiveJson();
  void handleHealthJson();

  TempWebServer server_{80};
  const Sample* latestSample_ = nullptr;
  const SystemHealth* health_ = nullptr;
};
