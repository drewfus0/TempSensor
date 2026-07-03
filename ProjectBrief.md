# TempSensor Project Brief (D1 Mini Refresh)

Source notes: Breif.md
Date: 2026-07-03
Project type: Personal side project

## 1) Project Summary
Build a home indoor environmental monitor using a LOLIN D1 mini Pro, BME280 sensor, OLED Shield, and microSD shield.

The device will:
- sample temperature, humidity, and pressure
- log data to SD card in CSV format
- show current readings and a small temperature trend on OLED
- host a local web dashboard for live readings and historical graphing

Primary concern: memory limits and long-run stability on ESP8266.

## 2) Hardware
- Board: LOLIN D1 mini Pro v2.0.0 (ESP8266)
- Sensor: BME280 (I2C on D1/D2)
- Display: LOLIN OLED Shield v2.0.0
- Storage: LOLIN microSD Card Shield v1.2.0
- Power: Plugged in (no battery requirement)
- Environment: Indoor use

## 3) Goals
### Main goal
Monitor indoor temperature while also recording humidity and pressure.

### First milestone target
- sensor wired and read successfully
- best-effort 1 second sampling loop
- periodic SD CSV logging from RAM buffer
- OLED shows current readings and a simple trend display
- Wi-Fi connection and basic local web page

### Full project target
All planned features run together with stable memory behavior on ESP8266.

## 4) Functional Requirements
1. Data collection
- Read BME280 temperature, humidity, pressure.
- Target sample interval: 1 second.
- Acceptable missed/jitter windows when busy: up to about 5 seconds.

2. Data logging
- Log to CSV for spreadsheet analysis.
- Buffer in RAM and flush to SD every 1 to 5 minutes (tunable).
- Data loss on sudden power loss is acceptable for unflushed RAM samples.
- Retention target: until SD card is full.

3. Time handling
- Use NTP time when available.
- If NTP is unavailable, continue with estimated timestamps.
- Mark each CSV row with timestamp quality (`ntp` or `estimated`).
- Record a timeline event when NTP is re-established.

4. Display behavior (OLED)
- Show current readings (temperature, humidity, pressure).
- Show compact recent temperature trend.
- Refresh often enough for readability while controlling RAM/CPU use.

5. Web dashboard (local network only)
- Live values for all sensor fields.
- Historical graphs for each sensor value.
- Date-time range filter (start/end).
- Runtime controls (sampling and related settings).
- Log download support.
- No authentication for now.

## 5) Technical Constraints
- Framework: PlatformIO.
- Prioritize memory-safe, bounded data structures.
- Keep web and display rendering conservative for ESP8266 capacity.
- OTA is not required in current phase.

## 6) Acceptance Criteria
### Milestone 1 accepted when
- BME280 values update continuously
- buffered samples are persisted to SD CSV
- OLED displays live values and trend output
- Wi-Fi connects and local page responds

### Full project accepted when
- live and historical data are usable from the web UI
- runtime controls and log download work
- system passes basic soak runs without memory-related crashes

## 7) Deferred Considerations
1. SD full-card behavior policy.
2. Explicit measurable tolerance targets.
3. NTP outage and recovery detail.
4. Local dashboard authentication.
5. Historical graph depth defaults and decimation strategy.
