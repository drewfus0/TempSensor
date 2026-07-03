#include "managers/DisplayManager.h"

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Wire.h>
#include <cstring>

#include "config/AppConfig.h"

namespace {
Adafruit_SSD1306 display(AppConfig::DISPLAY_WIDTH, AppConfig::DISPLAY_HEIGHT, &Wire, -1);

constexpr size_t MAX_LINE_CHARS = 21;  // 128px / ~6px per character at text size 1

void copyClippedLine(char* out, size_t outSize, const char* in) {
  if (outSize == 0) {
    return;
  }
  if (!in) {
    out[0] = '\0';
    return;
  }

  strncpy(out, in, outSize - 1);
  out[outSize - 1] = '\0';

  const size_t len = strlen(out);
  if (len > MAX_LINE_CHARS) {
    out[MAX_LINE_CHARS] = '\0';
    if (MAX_LINE_CHARS >= 3) {
      out[MAX_LINE_CHARS - 3] = '.';
      out[MAX_LINE_CHARS - 2] = '.';
      out[MAX_LINE_CHARS - 1] = '.';
    }
  }
}

void makeShortTimestamp(const char* in, char* out, size_t outSize) {
  if (outSize == 0) {
    return;
  }

  if (!in || in[0] == '\0') {
    out[0] = '\0';
    return;
  }

  // Convert YYYY-MM-DDTHH:MM:SSZ to HH:MM:SS for compact 128x32 display.
  if (strlen(in) >= 19 && in[4] == '-' && in[7] == '-' && in[10] == 'T') {
    snprintf(out, outSize, "%c%c:%c%c:%c%c", in[11], in[12], in[14], in[15], in[17], in[18]);
    return;
  }

  copyClippedLine(out, outSize, in);
}

void drawDimensionProbe() {
  const int16_t w = display.width();
  const int16_t h = display.height();
  const int16_t maxX = (w > 0) ? (w - 1) : 0;
  const int16_t maxY = (h > 0) ? (h - 1) : 0;

  display.clearDisplay();
  display.setTextWrap(false);
  display.setTextColor(SSD1306_WHITE);

  // Outer border should touch all physical edges if width/height are correct.
  display.drawRect(0, 0, w, h, SSD1306_WHITE);

  // Tick marks every 8 px make clipping and offsets obvious.
  for (int16_t x = 0; x < w; x += 8) {
    display.drawFastVLine(x, 0, 3, SSD1306_WHITE);
    display.drawFastVLine(x, maxY - 2, 3, SSD1306_WHITE);
  }
  for (int16_t y = 0; y < h; y += 8) {
    display.drawFastHLine(0, y, 3, SSD1306_WHITE);
    display.drawFastHLine(maxX - 2, y, 3, SSD1306_WHITE);
  }

  display.setCursor(2, 2);
  display.setTextSize(1);
  display.print("Probe");

  char wh[20]{};
  snprintf(wh, sizeof(wh), "%dx%d", w, h);
  display.setCursor(2, 12);
  display.print(wh);

  // Corner coordinate markers.
  display.setCursor(2, h > 10 ? (h - 10) : 0);
  display.print("0,");
  display.print(maxY);

  char xy[24]{};
  snprintf(xy, sizeof(xy), "%d,%d", maxX, maxY);
  const int16_t labelX = (int16_t)(w - ((int16_t)strlen(xy) * 6) - 2);
  display.setCursor(labelX > 0 ? labelX : 0, h > 10 ? (h - 10) : 0);
  display.print(xy);

  // Bright points at the 4 corners and center.
  display.drawPixel(0, 0, SSD1306_WHITE);
  display.drawPixel(maxX, 0, SSD1306_WHITE);
  display.drawPixel(0, maxY, SSD1306_WHITE);
  display.drawPixel(maxX, maxY, SSD1306_WHITE);
  display.fillCircle(maxX / 2, maxY / 2, 1, SSD1306_WHITE);

  display.display();
}
}

bool DisplayManager::begin(int sdaPin, int sclPin) {
  Wire.begin(sdaPin, sclPin);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("[Display] SSD1306 init failed");
    return false;
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setFont();
  display.setTextColor(SSD1306_WHITE);
  display.setTextWrap(false);
  display.setCursor(0, 0);
  display.println("TempSensor");
  display.println("OLED ready");
  display.display();

  delay(500);

  if (AppConfig::DISPLAY_SHOW_DIMENSION_PROBE_ON_BOOT) {
    drawDimensionProbe();
    delay(AppConfig::DISPLAY_DIMENSION_PROBE_MS);
  }

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
  display.setFont();
  display.setTextColor(SSD1306_WHITE);
  display.setTextWrap(false);
  display.setCursor(0, 0);

  char stageBuf[24]{};
  char detailBuf[24]{};
  char extraBuf[24]{};
  copyClippedLine(stageBuf, sizeof(stageBuf), stage);
  copyClippedLine(detailBuf, sizeof(detailBuf), detail);
  copyClippedLine(extraBuf, sizeof(extraBuf), extra);

  display.println("TempSensor Boot");
  display.printf("S:%s\n", stageBuf[0] ? stageBuf : "Stage");
  display.printf("D:%s\n", detailBuf[0] ? detailBuf : "-");
  if (extraBuf[0] != '\0') {
    display.printf("X:%s\n", extraBuf);
  } else {
    display.println("X:-");
  }
  display.println(isError ? "STAT:ERR" : "STAT:OK");

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
  display.setFont();
  display.setTextColor(SSD1306_WHITE);
  display.setTextWrap(false);
  display.setCursor(0, 0);

  char tsBuf[24]{};
  makeShortTimestamp(sample.timestamp, tsBuf, sizeof(tsBuf));

  display.printf("T:%.1f C\n", sample.temperatureC);
  display.printf("H:%.1f %%\n", sample.humidityPct);
  display.printf("P:%.0f hPa\n", sample.pressureHpa);
  display.printf("WiFi:%s SD:%s\n", wifiConnected ? "UP" : "DOWN", sdHealthy ? "OK" : "BAD");
  display.printf("NTP:%s\n", ntpSynced ? "SYNC" : "EST");
  display.println(tsBuf[0] ? tsBuf : "time?");

  display.display();
}
