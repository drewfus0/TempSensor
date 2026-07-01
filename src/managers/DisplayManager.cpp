#include "managers/DisplayManager.h"

#include <Adafruit_GFX.h>
#include <math.h>
#include <SPI.h>

bool DisplayManager::begin(int csPin, int dcPin, int rstPin, int busyPin, int sckPin, int mosiPin) {
  SPI.begin(sckPin, -1, mosiPin, csPin);
  display_.init(115200, true, 2, false);
  display_.setRotation(1);
  display_.setTextColor(GxEPD_BLACK);

  ready_ = true;
  Serial.println("[Display] E-ink initialized");
  return true;
}

void DisplayManager::render(const Sample& sample, const float* tempValues, size_t tempCount, uint32_t windowSeconds) {
  if (!ready_) {
    return;
  }

  display_.setFullWindow();
  display_.firstPage();
  do {
    display_.fillScreen(GxEPD_WHITE);

    display_.setCursor(20, 40);
    display_.setTextSize(2);
    display_.printf("Temp: %.2f C", sample.temperatureC);

    display_.setCursor(20, 80);
    display_.printf("Humidity: %.2f %%", sample.humidityPct);

    display_.setCursor(20, 120);
    display_.printf("Pressure: %.2f hPa", sample.pressureHpa);

    display_.setTextSize(1);
    display_.setCursor(20, 155);
    display_.printf("Timestamp: %s (%s)", sample.timestamp, TimestampQualityToString(sample.quality));

    const int graphX = 20;
    const int graphY = 190;
    const int graphW = 900;
    const int graphH = 300;

    display_.drawRect(graphX, graphY, graphW, graphH, GxEPD_BLACK);
    display_.setCursor(graphX, graphY - 10);
    display_.printf("Temperature trend (%lus window)", static_cast<unsigned long>(windowSeconds));

    if (tempCount > 1) {
      float minV = tempValues[0];
      float maxV = tempValues[0];
      for (size_t i = 1; i < tempCount; ++i) {
        if (tempValues[i] < minV) minV = tempValues[i];
        if (tempValues[i] > maxV) maxV = tempValues[i];
      }

      if (fabs(maxV - minV) < 0.01f) {
        maxV = minV + 0.01f;
      }

      for (size_t i = 1; i < tempCount; ++i) {
        const int x1 = graphX + static_cast<int>((i - 1) * (graphW - 2) / (tempCount - 1)) + 1;
        const int x2 = graphX + static_cast<int>(i * (graphW - 2) / (tempCount - 1)) + 1;

        const float norm1 = (tempValues[i - 1] - minV) / (maxV - minV);
        const float norm2 = (tempValues[i] - minV) / (maxV - minV);

        const int y1 = graphY + graphH - 1 - static_cast<int>(norm1 * (graphH - 2));
        const int y2 = graphY + graphH - 1 - static_cast<int>(norm2 * (graphH - 2));

        display_.drawLine(x1, y1, x2, y2, GxEPD_BLACK);
      }

      display_.setCursor(graphX + 4, graphY + 14);
      display_.printf("max %.2f C", maxV);
      display_.setCursor(graphX + 4, graphY + graphH - 6);
      display_.printf("min %.2f C", minV);
    } else {
      display_.setCursor(graphX + 10, graphY + 20);
      display_.print("Collecting data for graph...");
    }
  } while (display_.nextPage());

  Serial.println("[Display] Full refresh complete");
}
