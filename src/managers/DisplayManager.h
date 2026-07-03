#pragma once

#include <Arduino.h>

#include "models/Sample.h"

class DisplayManager {
 public:
  bool begin();
  void render(const Sample& sample, const float* tempValues, size_t tempCount, uint32_t windowSeconds);
  bool isReady() const { return ready_; }

 private:
  bool ready_ = false;
  uint8_t* framebuffer_ = nullptr;
};
