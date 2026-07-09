# TempSensor Project Brief (D1 Mini Refresh)

Source notes: Breif.md
Date: 2026-07-03
Project type: Personal side project

## 1) Project Summary
Build a home indoor environmental monitor using a LOLIN D1 mini Pro, BME280 sensor, OLED Shield, and microSD shield.

The device will:
- sample temperature, humidity, and pressure
- log data to SD card in CSV format
- show current readings and status on OLED
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
- OLED shows current readings and status
- Wi-Fi connection and basic local web page

### Milestone 1 status
Complete.

### Second milestone target
- web dashboard shows live values for temperature, humidity, and pressure
- web dashboard shows historical data and graphs sourced from SD CSV logs
- date-time range filter for historical queries
- runtime controls for sample rate and related settings
- log download support from the local web UI
- OLED remains a compact status/readout screen rather than a history display

### Milestone 2 status
Complete.

### Third milestone target (Milestone 3)
- Over-The-Air (OTA) firmware update support
- Physical signal and connection stability (soldering parts into a stack)
- Battery diagnostics: use history logging to predict/identify charging vs. discharging state
- Solar charging feasibility evaluation (assess if 4-6 hours of solar charging is sufficient)
- System event logging to Serial and timestamp correction post-NTP sync
- Continuously run long-term stability and soak testing

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
- OLED displays live values and status output
- Wi-Fi connects and local page responds

### Milestone 2 accepted when
- web dashboard renders historical data from SD logs
- web dashboard supports date range filtering
- web dashboard supports runtime controls and log download
- web dashboard remains usable on ESP8266 memory limits
- OLED remains readable with compact live status only

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

## 8) Milestone 2 Web Design And Structure

### 8.1 Design Objectives
- Keep the UI fast and readable on desktop and mobile browsers.
- Keep firmware memory usage bounded on ESP8266.
- Avoid loading full CSV files into RAM.
- Provide practical local-network tooling first (read, filter, download, tune).

### 8.2 Information Architecture (Pages And Panels)
- Route: `/`
- Panel: Live Snapshot
- Panel: Health Snapshot
- Panel: Historical Chart
- Panel: Date-Time Filter
- Panel: Runtime Controls
- Panel: SD Files And Tree
- Panel: Event Timeline

### 8.3 Single-Page Layout Structure
- Header row: device name, Wi-Fi state, NTP state, current IP, last refresh time.
- Left column: Live Snapshot, Health Snapshot, Runtime Controls.
- Right column: Historical Chart with metric selector and range filter.
- Bottom row: SD file tree, downloadable log list, event timeline.
- Mobile behavior: stack all panels vertically in the same logical order.

### 8.4 Functional Scope Per Panel
- Live Snapshot
- Show temperature, humidity, pressure, timestamp, timestamp quality.
- Refresh interval target: 1 second.

- Health Snapshot
- Show heap free, largest free block, queue depth/capacity, dropped samples, SD health.
- Refresh interval target: 3 to 5 seconds.

- Historical Chart
- Plot one metric at a time: `temp_c`, `humidity_pct`, `pressure_hpa`.
- Allow selecting multiple preset windows: 15 min, 1 hr, 6 hr, 24 hr, custom.
- Render downsampled points returned by firmware (not raw full-resolution for long windows).

- Date-Time Filter
- Inputs: start local datetime, end local datetime.
- Validation: end must be greater than start, and range capped by configured max.

- Runtime Controls
- Editable settings: sample interval, SD flush interval, display refresh interval.
- Optional controls: force flush now, reconnect Wi-Fi, retry NTP sync.
- Apply flow: preview values, apply, show success or rollback message.

- SD Files And Tree
- Show recursive tree and file sizes.
- Allow selecting known log files for download.

- Event Timeline
- Show parsed entries from events CSV in reverse chronological order.
- Include NTP re-established and any future warning events.

### 8.5 HTTP API Contract (Milestone 2)
- Keep existing
- `GET /api/live`
- `GET /api/health`
- `GET /api/sd-tree`

- Add
- `GET /api/config`
- `POST /api/config`
- `GET /api/history?metric=temp_c&start=...&end=...&max_points=300`
- `GET /api/events?start=...&end=...&limit=200`
- `GET /api/logs`
- `GET /api/logs/download?file=/logs/data.csv`
- `POST /api/action/flush-now`
- `POST /api/action/ntp-retry`

### 8.6 Data And Processing Rules
- History queries must stream-read CSV line by line.
- Never hold full file contents in RAM.
- Response point cap defaults to 300 per request.
- If requested range exceeds cap, perform decimation during stream parse.
- Reject invalid requests with clear `400` JSON error payload.
- Return `503` when SD is unavailable.

### 8.7 Frontend Rendering Rules
- No heavy frontend frameworks for milestone 2.
- Use plain HTML/CSS/JS and lightweight canvas-based chart rendering.
- Polling
- Live: 1 second.
- Health: 5 seconds.
- History: on-demand and when filter changes.
- Show loading and error states per panel, not full-page blocking errors.

### 8.8 Runtime Configuration Model
- Configuration source of truth remains firmware.
- Runtime changes apply immediately and also persist to a small settings file on SD.
- On boot, load settings from SD with fallback to compile-time defaults.
- If settings file is invalid, log event and continue with defaults.

### 8.9 Milestone 2 Acceptance Test Matrix
- Live panel updates continuously while sampling remains active.
- History endpoint returns filtered data for all three metrics.
- Chart renders 24-hour range without browser freeze.
- Runtime control updates sample interval and takes effect without reboot.
- Log list and downloads work for at least data and event CSV files.
- SD unavailable state is surfaced clearly in UI and API responses.
- Device remains stable during 12-hour run with periodic web queries.

### 8.10 Delivery Phases
- Phase 1: API foundation for config, history, events, logs.
- Phase 2: Frontend layout and live/health/history views.
- Phase 3: Runtime controls and settings persistence.
- Phase 4: Robustness pass (errors, limits, soak test, memory profiling).

## 9) Milestone 3 Design And Acceptance Criteria

### 9.1 Design Objectives
- Support remote Over-the-Air (OTA) firmware compilation/updates to avoid USB reconnection.
- Solder parts into a clean stack rather than using temporary jumpers to resolve signal/power instability.
- Predict and detect battery charge/discharge states by analyzing voltage characteristics over time.
- Perform a technical evaluation of solar charging capability (checking if 4-6 hours of daily sunlight is sufficient to sustain operation).
- Improve system diagnostics by redirecting key system transition events (API calls, NTP status changes, SD card flushes, and Wi-Fi transitions) to the Serial output.
- Support NTP correction for logs saved during NTP-outage boot phases.

### 9.2 Acceptance Criteria
- **OTA Updates:** Able to flash new firmware remotely over Wi-Fi.
- **Signal Stability:** Visual signal drops and I2C/SPI bus drops are eliminated by permanent soldering.
- **Battery Prediction:** The web dashboard and prediction algorithms identify charging vs. discharging state based on voltage slope trends.
- **Solar Feasibility Study:** Document actual battery current draw and solar panel output to confirm charging viability.
- **System Event Tracing:** High-level events appear on Serial with readable timestamps.
- **Timestamp Retroactive Correction:** When NTP sync completes after a disconnected boot, the firmware corrects the estimated timestamps of already-written RAM/SD buffer logs.
