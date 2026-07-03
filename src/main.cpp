#include <Arduino.h>

#include "app/AppCoordinator.h"

AppCoordinator app;

void setup() {
  Serial.begin(115200);
  delay(1500);  // allow USB-CDC to enumerate
  app.begin();
}

void loop() {
  app.loop();
}
