#include "managers/DisplayManager.h"

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Wire.h>
#include <cstring>

#include "config/AppConfig.h"

namespace {
Adafruit_SSD1306 display(-1);

constexpr size_t MAX_LINE_CHARS = 10;  // 64px / ~6px per character at text size 1

constexpr uint8_t TINY_CHAR_WIDTH = 4;
constexpr uint8_t TINY_CHAR_HEIGHT = 5;

constexpr uint8_t GLYPH_0[TINY_CHAR_HEIGHT] = {0x07, 0x05, 0x05, 0x05, 0x07};
constexpr uint8_t GLYPH_1[TINY_CHAR_HEIGHT] = {0x02, 0x06, 0x02, 0x02, 0x07};
constexpr uint8_t GLYPH_2[TINY_CHAR_HEIGHT] = {0x07, 0x01, 0x07, 0x04, 0x07};
constexpr uint8_t GLYPH_3[TINY_CHAR_HEIGHT] = {0x07, 0x01, 0x07, 0x01, 0x07};
constexpr uint8_t GLYPH_4[TINY_CHAR_HEIGHT] = {0x05, 0x05, 0x07, 0x01, 0x01};
constexpr uint8_t GLYPH_5[TINY_CHAR_HEIGHT] = {0x07, 0x04, 0x07, 0x01, 0x07};
constexpr uint8_t GLYPH_6[TINY_CHAR_HEIGHT] = {0x07, 0x04, 0x07, 0x05, 0x07};
constexpr uint8_t GLYPH_7[TINY_CHAR_HEIGHT] = {0x07, 0x01, 0x02, 0x02, 0x02};
constexpr uint8_t GLYPH_8[TINY_CHAR_HEIGHT] = {0x07, 0x05, 0x07, 0x05, 0x07};
constexpr uint8_t GLYPH_9[TINY_CHAR_HEIGHT] = {0x07, 0x05, 0x07, 0x01, 0x07};
constexpr uint8_t GLYPH_DOT[TINY_CHAR_HEIGHT] = {0x00, 0x00, 0x00, 0x00, 0x02};

const uint8_t* tinyGlyph(char c) {
  switch (c) {
    case '0':
      return GLYPH_0;
    case '1':
      return GLYPH_1;
    case '2':
      return GLYPH_2;
    case '3':
      return GLYPH_3;
    case '4':
      return GLYPH_4;
    case '5':
      return GLYPH_5;
    case '6':
      return GLYPH_6;
    case '7':
      return GLYPH_7;
    case '8':
      return GLYPH_8;
    case '9':
      return GLYPH_9;
    case '.':
      return GLYPH_DOT;
    default:
      return nullptr;
  }
}

void drawTinyChar(char c, int16_t x, int16_t y) {
  const uint8_t* glyph = tinyGlyph(c);
  if (!glyph) {
    return;
  }

  for (uint8_t row = 0; row < TINY_CHAR_HEIGHT; ++row) {
    for (uint8_t col = 0; col < 3; ++col) {
      if (glyph[row] & (1 << (2 - col))) {
        display.drawPixel(x + col, y + row, WHITE);
      }
    }
  }
}

void drawTinyText(const char* text, int16_t x, int16_t y) {
  if (!text) {
    return;
  }

  int16_t cursorX = x;
  for (size_t i = 0; text[i] != '\0'; ++i) {
    drawTinyChar(text[i], cursorX, y);
    cursorX += TINY_CHAR_WIDTH;
  }
}

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

  // Convert local YYYY-MM-DD HH:MM:SS or UTC YYYY-MM-DDTHH:MM:SSZ to HH:MM:SS.
  if (strlen(in) >= 19 && in[4] == '-' && in[7] == '-' && (in[10] == 'T' || in[10] == ' ')) {
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
  display.setTextColor(WHITE);

  // Outer border should touch all physical edges if width/height are correct.
  display.drawRect(0, 0, w, h, WHITE);

  // Tick marks every 8 px make clipping and offsets obvious.
  for (int16_t x = 0; x < w; x += 8) {
    display.drawFastVLine(x, 0, 3, WHITE);
    display.drawFastVLine(x, maxY - 2, 3, WHITE);
  }
  for (int16_t y = 0; y < h; y += 8) {
    display.drawFastHLine(0, y, 3, WHITE);
    display.drawFastHLine(maxX - 2, y, 3, WHITE);
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
  display.drawPixel(0, 0, WHITE);
  display.drawPixel(maxX, 0, WHITE);
  display.drawPixel(0, maxY, WHITE);
  display.drawPixel(maxX, maxY, WHITE);
  display.fillCircle(maxX / 2, maxY / 2, 1, WHITE);

  display.display();
}
}

bool DisplayManager::begin(int sdaPin, int sclPin) {
  Wire.begin(sdaPin, sclPin);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);

  display.clearDisplay();
  display.setTextSize(1);
  display.setFont();
  display.setTextColor(WHITE);
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
  display.setTextColor(WHITE);
  display.setTextWrap(false);
  display.setCursor(0, 0);

  char stageBuf[24]{};
  char detailBuf[24]{};
  char extraBuf[24]{};
  copyClippedLine(stageBuf, sizeof(stageBuf), stage);
  copyClippedLine(detailBuf, sizeof(detailBuf), detail);
  copyClippedLine(extraBuf, sizeof(extraBuf), extra);

  // 64x48 startup layout: compact labels to keep all lines readable.
  display.println("Boot");
  display.printf("B:%s\n", stageBuf[0] ? stageBuf : "-");
  display.printf("D:%s\n", detailBuf[0] ? detailBuf : "-");
  display.printf("X:%s\n", extraBuf[0] ? extraBuf : "-");
  display.printf("S:%s\n", isError ? "ERR" : "OK");

  display.display();
  delay(isError ? 1300 : 700);
}

void DisplayManager::renderLatest(const Sample& sample,
                                  bool wifiConnected,
                                  bool ntpSynced,
                                  bool sdHealthy,
                                  const char* ipAddress) {
  if (!ready_) {
    return;
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setFont();
  display.setTextColor(WHITE);
  display.setTextWrap(false);
  display.setCursor(0, 0);

  char tsBuf[24]{};
  makeShortTimestamp(sample.timestamp, tsBuf, sizeof(tsBuf));

  char statusBuf[16]{};
  snprintf(statusBuf, sizeof(statusBuf), "%s|%s|%s", wifiConnected ? "W+" : "W-", sdHealthy ? "S+" : "S-",
           ntpSynced ? "N+" : "N-");

  // 64x48 screen: keep each line compact (about 10 chars at text size 1).
  display.printf("T:%2.1fC\n", sample.temperatureC);
  display.printf("H:%2.1f%%\n", sample.humidityPct);
  display.printf("P:%4.0fhPa\n", sample.pressureHpa);
  display.println(statusBuf);
  display.println(ntpSynced ? tsBuf : "syncing");

  const char* ipText = (ipAddress && ipAddress[0] != '\0') ? ipAddress : "0.0.0.0";
  drawTinyText(ipText, 0, display.height() - TINY_CHAR_HEIGHT);

  display.display();
}
