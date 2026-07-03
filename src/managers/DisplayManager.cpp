#include "managers/DisplayManager.h"

#include "epd_driver.h"
#include "firasans.h"
#include <esp_heap_caps.h>

bool DisplayManager::begin() {
  framebuffer_ = (uint8_t*)ps_calloc(EPD_WIDTH * EPD_HEIGHT / 2, 1);
  if (!framebuffer_) {
    Serial.println("[Display] PSRAM framebuffer alloc failed");
    return false;
  }
  memset(framebuffer_, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2);

  epd_init();
  epd_poweron();
  epd_clear();
  epd_poweroff();

  ready_ = true;
  Serial.println("[Display] EPD47 initialized");
  return true;
}

void DisplayManager::render(const Sample& sample, const float* tempValues, size_t tempCount, uint32_t windowSeconds) {
  if (!ready_) {
    return;
  }

  memset(framebuffer_, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2);

  char buf[96];
  int32_t cx, cy;

  cx = 20; cy = 50;
  snprintf(buf, sizeof(buf), "Temp: %.1f C", sample.temperatureC);
  writeln((GFXfont*)&FiraSans, buf, &cx, &cy, framebuffer_);

  cx = 20; cy = 100;
  snprintf(buf, sizeof(buf), "Humidity: %.1f %%", sample.humidityPct);
  writeln((GFXfont*)&FiraSans, buf, &cx, &cy, framebuffer_);

  cx = 20; cy = 150;
  snprintf(buf, sizeof(buf), "Pressure: %.1f hPa", sample.pressureHpa);
  writeln((GFXfont*)&FiraSans, buf, &cx, &cy, framebuffer_);

  cx = 20; cy = 200;
  snprintf(buf, sizeof(buf), "%s", sample.timestamp);
  writeln((GFXfont*)&FiraSans, buf, &cx, &cy, framebuffer_);

  // Temperature bar graph
  const int32_t GX = 20;
  const int32_t GY = 215;
  const int32_t GW = EPD_WIDTH - 2 * GX;
  const int32_t GH = EPD_HEIGHT - GY - 10;

  epd_draw_rect(GX, GY, GW, GH, 0, framebuffer_);

  if (tempCount > 1) {
    float minT = tempValues[0], maxT = tempValues[0];
    for (size_t i = 1; i < tempCount; i++) {
      if (tempValues[i] < minT) minT = tempValues[i];
      if (tempValues[i] > maxT) maxT = tempValues[i];
    }
    float range = maxT - minT;
    if (range < 0.5f) range = 0.5f;

    const int32_t INNER_W = GW - 2;
    const int32_t INNER_H = GH - 2;

    for (int32_t col = 0; col < INNER_W; col++) {
      size_t idx = ((size_t)col * tempCount) / (size_t)INNER_W;
      if (idx >= tempCount) idx = tempCount - 1;
      int32_t barH = (int32_t)((tempValues[idx] - minT) / range * (float)INNER_H);
      if (barH > INNER_H) barH = INNER_H;
      if (barH > 0) {
        epd_draw_vline(GX + 1 + col, GY + GH - 1 - barH, barH, 0, framebuffer_);
      }
    }

    cx = GX + 4; cy = GY + GH - 4;
    snprintf(buf, sizeof(buf), "%.1fC", minT);
    writeln((GFXfont*)&FiraSans, buf, &cx, &cy, framebuffer_);

    cx = GX + GW - 100; cy = GY + 36;
    snprintf(buf, sizeof(buf), "%.1fC", maxT);
    writeln((GFXfont*)&FiraSans, buf, &cx, &cy, framebuffer_);
  } else {
    cx = GX + 10; cy = GY + 50;
    writeln((GFXfont*)&FiraSans, "Collecting data...", &cx, &cy, framebuffer_);
  }

  epd_poweron();
  epd_draw_grayscale_image(epd_full_screen(), framebuffer_);
  epd_poweroff();

  Serial.println("[Display] Full refresh complete");
}
