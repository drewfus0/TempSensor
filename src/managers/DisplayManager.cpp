#include "managers/DisplayManager.h"

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Wire.h>

#include "config/AppConfig.h"

namespace {
Adafruit_SSD1306 display(AppConfig::DISPLAY_WIDTH, AppConfig::DISPLAY_HEIGHT, &Wire, -1);
}

bool DisplayManager::begin(int sdaPin, int sclPin) {
  Wire.begin(sdaPin, sclPin);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("[Display] SSD1306 init failed");
    return false;
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("TempSensor");
  display.println("OLED ready");
  display.display();

  delay(500);

  ready_ = true;
  Serial.println("[Display] SSD1306 initialized");
  return true;
}

void DisplayManager::showStartupStatus(const char* stage,
                                       const char* detail,
                                       const char* extra,
                                       bool isError) {
  if (!ready_) {
    return;
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);

  display.println("TempSensor Boot");
  display.println("--------------");
  display.print(stage ? stage : "Stage");
  display.print(": ");
  display.println(detail ? detail : "");
  if (extra && extra[0] != '\0') {
    display.println(extra);
  }
  if (isError) {
    display.println("ERROR");
  }

  display.display();
  delay(isError ? 1300 : 700);
}

void DisplayManager::renderLatest(const Sample& sample,
                                  bool wifiConnected,
                                  bool ntpSynced,
                                  bool sdHealthy) {
  if (!ready_) {
    return;
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);

  display.printf("T: %.1f C\n", sample.temperatureC);
  display.printf("H: %.1f %%\n", sample.humidityPct);
  display.printf("P: %.1f hPa\n", sample.pressureHpa);
  display.printf("WiFi:%s SD:%s\n", wifiConnected ? "UP" : "DOWN", sdHealthy ? "OK" : "BAD");
  display.printf("NTP:%s\n", ntpSynced ? "SYNC" : "EST");
  display.println(sample.timestamp);

  display.display();
}
