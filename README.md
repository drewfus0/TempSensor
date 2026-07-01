# TempSensor Firmware (Milestone 1)

ESP32-S3 firmware scaffold for a LILYGO T5 4.7 inch e-ink board with BME280 sensing, SD CSV logging, periodic e-ink refresh, and a lightweight local web status server.

## Architecture Summary

Modules are split by concern and coordinated by a single non-blocking app loop:

- app/AppCoordinator: schedules periodic tasks (sampling, flush, display, diagnostics)
- managers/SensorManager: BME280 initialization and reads
- managers/TimeManager: NTP sync + estimated timestamp fallback
- managers/LoggerManager: fixed-size RAM queue and CSV/event logging to SD
- managers/DisplayManager: full e-ink refresh with current readings + mini temp trend graph
- managers/WebManager: local status page and JSON health/live endpoints
- managers/DiagnosticsManager: serial memory and buffer telemetry
- utils/RingBuffer: bounded fixed-size storage for recent values and log queue

Design intent is memory-first: fixed capacities, no unbounded containers, and periodic health logging for heap and queue depth.

## Wiring Assumptions (Adjust in include/config/AppConfig.h)

The exact pin map can vary by LILYGO T5 revision. This scaffold assumes external BME280 on I2C and SPI shared by SD/e-ink.

- BME280 SDA: GPIO18
- BME280 SCL: GPIO17
- BME280 I2C addr: 0x76
- SD CS: GPIO10
- SD SCK/MOSI/MISO: GPIO12/GPIO11/GPIO13
- E-ink CS/DC/RST/BUSY: GPIO7/GPIO6/GPIO5/GPIO4
- E-ink SCK/MOSI: GPIO12/GPIO11

If your board revision differs, update only include/config/AppConfig.h first.

## Build, Upload, Monitor

Prereqs:
- PlatformIO Core or VS Code PlatformIO extension
- USB serial permissions on Linux

Commands:
- Build: pio run
- Upload: pio run -t upload
- Monitor: pio device monitor -b 115200

Optional combined flow:
- pio run -t upload && pio device monitor -b 115200

## Runtime Behavior (Milestone 1)

- Sensor sampling target: every 1s (best effort, non-blocking scheduler)
- Logging queue flush: every 60s default
- E-ink full refresh: every 30min default
- NTP: attempts sync at boot and retries periodically
- If NTP unavailable: timestamp stored as estimated using uptime format
- On NTP restore: event row written to /logs/events.csv

CSV schema:
- /logs/data.csv: timestamp,timestamp_quality,temp_c,humidity_pct,pressure_hpa,uptime_s
- /logs/events.csv: timestamp,timestamp_quality,event

## Dependency List and Rationale

- Adafruit BME280 Library: stable BME280 driver for Arduino ecosystem
- Adafruit Unified Sensor: dependency for Adafruit BME280
- GxEPD2: mature e-ink drawing and panel support
- ArduinoJson: low-overhead JSON for local API endpoints

## Milestone 1 Test Procedure

1. Configure include/config/AppConfig.h:
   - Wi-Fi SSID/password
   - Verify board pin mapping
2. Flash firmware and open serial monitor.
3. Confirm startup logs:
   - sensor init status
   - SD init status
   - Wi-Fi status/IP
   - NTP status
4. Let device run at least 5 minutes:
   - verify repeated sensor readings (indirect via /api/live)
   - verify /logs/data.csv is created and appended
   - confirm timestamp_quality is ntp or estimated
5. Wait for display refresh interval (or temporarily reduce it in config):
   - verify full e-ink redraw with current values and graph
6. Open local web page at device IP:
   - / loads status page
   - /api/live returns latest values JSON
   - /api/health returns memory and queue diagnostics JSON
7. Soak test 30+ minutes:
   - check serial diagnostics trend for free heap stability
   - watch queue depth and dropped samples

## Risks and Next Steps

Current known risks:
- LILYGO T5 4.7 S3 pin map and panel variant may differ; verify against your exact board revision.
- First milestone uses a simple in-memory graph window and minimal web UI to protect RAM.
- Wi-Fi connect path includes a short blocking window during boot only.

Planned phase 2:
- Add button-triggered immediate display refresh
- Add SD rotation/full-card policy
- Add historical range queries (chunked) for web graphs
- Add runtime config endpoints (sample and flush tuning)
- Add optional LAN auth and hardened web controls
