# Hardware Change Brief (2026-07-03)

## Change Summary
Project hardware stack has been changed from the previous T5 e-ink platform to the following:

- LOLIN D1 mini Pro v2.0.0 (ESP8266)
- LOLIN OLED Shield v2.0.0 (I2C)
- LOLIN microSD Card Shield v1.2.0 (SPI)
- BME280 sensor on I2C (D1/D2)

## Why This Change
- Simplify assembly using stacked shields and breadboard wiring.
- Reduce hardware complexity for early milestones.
- Keep memory and feature tuning focused on practical milestone progress.

## New Pin Mapping
I2C (shared by OLED + BME280):
- SDA: D2 / GPIO4
- SCL: D1 / GPIO5

SPI (microSD shield):
- CS: D8 / GPIO15 (shield default D4 CS rerouted by jumper)
- SCK: D5 / GPIO14
- MISO: D6 / GPIO12
- MOSI: D7 / GPIO13

## Repository Updates Completed
- Updated wiring documentation to D1 mini stack.
- Updated PlatformIO target environment to ESP8266 D1 mini Pro.
- Updated app pin config defaults in include/config/AppConfig.h.
- Updated project README and project brief to new hardware.
- Updated quick I2C/BME test utilities to D1 mini pin mapping.
- Updated handoff notes to remove old e-ink blocker context.

## Known Follow-Up Work
- Migrate display manager implementation from e-ink behavior to OLED behavior.
- Re-run full build and smoke tests after display migration.
- Tune memory behavior for ESP8266 constraints.

## Success Criteria for This Migration Step
- All docs and configuration references point to D1 mini/OLED/microSD/BME280 hardware.
- Team handoff notes and brief no longer assume LILYGO T5 hardware.
