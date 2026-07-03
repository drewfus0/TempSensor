#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_BME280.h>

Adafruit_BME280 bme;
bool sensor_ok = false;

void setup() {
  Serial.begin(115200);
  delay(1500);
  
  Serial.println("\n=== BME280 Quick Test ===");
  Wire.begin(4, 5);  // SDA=D2, SCL=D1
  delay(10);
  
  // Try 0x76 first (most common BME280 address with SDO to GND)
  Serial.print("Testing address 0x76... ");
  if (bme.begin(0x76, &Wire)) {
    Serial.println("✓ SUCCESS!");
    sensor_ok = true;
    return;
  }
  
  // Try 0x77 (SDO tied HIGH)
  Serial.print("Testing address 0x77... ");
  if (bme.begin(0x77, &Wire)) {
    Serial.println("✓ SUCCESS!");
    sensor_ok = true;
    return;
  }
  
  // Try 0x51 (was found before)
  Serial.print("Testing address 0x51... ");
  if (bme.begin(0x51, &Wire)) {
    Serial.println("✓ SUCCESS!");
    sensor_ok = true;
    return;
  }
  
  Serial.println("✗ FAILED");
  Serial.println("\nQuick scan (checking 0x20-0x7F with 100ms timeout):");
  
  int found = 0;
  for (uint8_t addr = 0x20; addr <= 0x7F; addr++) {
    Wire.beginTransmission(addr);
    uint8_t error = Wire.endTransmission();
    if (error == 0) {
      Serial.printf("  Found: 0x%02X\n", addr);
      found++;
    }
    delayMicroseconds(100);
  }
  
  if (found == 0) {
    Serial.println("\n⚠️  No I2C devices found!");
    Serial.println("Verify: 1) VCC=3.3V  2) GND connected  3) SDA->D2/GPIO4  4) SCL->D1/GPIO5");
  } else {
    Serial.printf("\nFound %d device(s). Check if any are your BME280.\n", found);
  }
}

void loop() {
  if (sensor_ok) {
    float temp = bme.readTemperature();
    float hum = bme.readHumidity();
    float pres = bme.readPressure() / 100.0F;
    Serial.printf("T=%.1f°C  H=%.1f%%  P=%.1f hPa\n", temp, hum, pres);
  }
  delay(2000);
}
