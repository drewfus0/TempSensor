#pragma once

#include <ESP8266WebServer.h>

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

  ESP8266WebServer server_{80};
  const Sample* latestSample_ = nullptr;
  const SystemHealth* health_ = nullptr;
};
