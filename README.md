# TempSensor Firmware (D1 Mini Pro Migration)

Firmware project for a LOLIN D1 mini Pro v2.0.0 (ESP8266) with:
- BME280 sensor on I2C
- OLED Shield v2.0.0 on I2C
- microSD Shield v1.2.0 on SPI

This repository was originally scaffolded for a different board and is now being migrated to D1 mini hardware.

## Hardware Profile

- MCU board: LOLIN D1 mini Pro v2.0.0
- Sensor: BME280
- Display: LOLIN OLED Shield v2.0.0
- Storage: LOLIN microSD Card Shield v1.2.0

## Wiring Assumptions

I2C shared bus:
- SCL: D1 (GPIO5)
- SDA: D2 (GPIO4)

SPI for SD:
- CS: D8 (GPIO15), with shield CS rerouted from default D4 pad
- SCK: D5 (GPIO14)
- MISO: D6 (GPIO12)
- MOSI: D7 (GPIO13)

BME280 address:
- default: `0x76` (SDO to GND)
- alternate: `0x77`

Full wiring details are in WIRING_GUIDE.md.

## Build, Upload, Monitor

Prereqs:
- PlatformIO Core or VS Code PlatformIO extension
- USB serial permissions on Linux

Commands:
- Build: `pio run`
- Upload: `pio run -t upload`
- Monitor: `pio device monitor -b 115200`

## Current Migration Status

- PlatformIO environment now targets `d1_mini_pro`.
- Core pin config in include/config/AppConfig.h has been switched to D1 mini mapping.
- Wiring guide and project briefs have been updated for the new stack.
- Manager implementation still needs a follow-up migration pass where e-ink specific display logic is replaced with OLED logic.

## Next Development Steps

1. Replace existing display manager behavior with SSD1306 OLED rendering.
2. Verify SD shield initialization on D8/GPIO15 CS (D4 pad rerouted by jumper).
3. Run sensor + logging + web endpoint smoke test on ESP8266 target.
4. Tune memory usage for ESP8266 constraints.
