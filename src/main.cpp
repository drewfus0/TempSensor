#include <Arduino.h>

#include "app/AppCoordinator.h"

AppCoordinator app;

void setup() {
  Serial.begin(115200);
  delay(300);
  app.begin();
}

void loop() {
  app.loop();
}
