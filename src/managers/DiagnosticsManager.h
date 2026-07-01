#pragma once

#include "models/SystemHealth.h"

class DiagnosticsManager {
 public:
  void printPeriodic(const SystemHealth& health);
};
