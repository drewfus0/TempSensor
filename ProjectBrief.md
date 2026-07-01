# TempSensor Project Brief (Working Draft v1)

Source notes: Breif.md
Date: 2026-07-02
Project type: Personal side project

## 1) Project Summary
Build a home indoor environmental monitor using a LILYGO T5 4.7 inch e-ink board and a BME280 sensor.

The device will:
- sample temperature, humidity, and pressure
- log data to SD card in CSV format
- show current data and a small temperature history graph on the e-ink display
- host a local web dashboard for live readings and historical graphing

Primary concern: memory and storage limits while running all features together.

## 2) Hardware
- Board: LILYGO T5 Screen 4.7 inch S3 v2.3 (2021-6-10)
- Sensor: TS1208P-BME280-3.3V
- Storage: SanDisk Extreme 32GB microSD HC V30
- Power: Plugged in (no battery requirement)
- Environment: Indoor use

## 3) Goals
### Main goal
Monitor home indoor temperature (with humidity and pressure also captured).

### First milestone target (about 1 week, flexible)
- sensor wired and read successfully
- 1 second sampling loop running (best effort)
- periodic SD CSV logging from RAM buffer
- e-ink screen shows current readings plus simple temp graph
- Wi-Fi connection and basic local hello world page

### Full project target
All planned features running together with stable behavior under device memory limits.

## 4) Functional Requirements
1. Data collection
- Read BME280 temperature, humidity, pressure.
- Target sample interval: 1 second.
- Acceptable jitter/missed intervals: up to about 5 seconds when system is busy.

2. Data logging
- Log to CSV for spreadsheet analysis.
- Buffer data in RAM and flush to SD every 1 to 5 minutes (tunable).
- Data loss on power loss is acceptable for unflushed RAM samples.
- Retention target: keep logging until SD fills.

3. Time handling
- Use NTP time when available.
- If NTP is unavailable, continue logging with estimated/relative timestamps.
- Include a CSV field that marks timestamp quality (for example: ntp or estimated).
- Record an event when NTP is re-established so timeline trust is clear.

4. Display behavior (e-ink)
- Show current readings (temp, humidity, pressure).
- Show mini temperature graph (last hour target window).
- Use full refresh updates.
- Refresh cadence target: every 30 to 60 minutes, plus optional manual refresh via button.

5. Web dashboard (local network only)
- Live values for all sensors.
- Historical graphs for each sensor value.
- Date-time filter (start and end).
- Controls for sample rate and related runtime settings.
- Log download support.
- No authentication for now.

## 5) Technical Constraints and Implementation Direction
- Framework: PlatformIO.
- No mandatory library constraints yet.
- Web assets may be served from flash or SD, whichever is simplest and best for memory limits.
- OTA updates: not required in current phase.

## 6) Memory-Risk Plan (RAM, Flash, SD)
### Key risk
Feature set may exceed practical RAM/flash budget if buffering, graph generation, and web history are not bounded.

### Planned controls
1. Keep in-RAM structures bounded
- fixed-size ring buffer for recent samples
- avoid unbounded dynamic containers

2. Keep web history queries bounded
- load filtered windows from SD files instead of keeping long history in RAM
- paginate/chunk responses when needed

3. Reduce render costs
- precompute small graph datasets
- avoid frequent display redraws (already low refresh cadence)

4. Keep storage format lightweight
- compact CSV rows
- rotate log files by day or size if needed

5. Measure and tune early
- add periodic memory telemetry logs (free heap, largest block)
- tune sample interval, flush interval, and graph window based on observed limits

## 7) Acceptance Criteria (Current Draft)
### Milestone 1 accepted when
- sensor values update continuously
- samples are written to CSV on SD from buffered memory
- e-ink shows current values and a temperature mini graph
- device connects to Wi-Fi and serves a basic local page

### Full project accepted when
- live and historical data are available on web UI with date filtering
- runtime controls (for sampling/config) work from web UI
- log download works
- system remains stable during normal operation with no memory-related crashes in basic soak testing

## 8) Deferred / Future Considerations
These are intentionally deferred until after baseline functionality is stable:

1. SD full policy
- choose one: stop logging, overwrite oldest files, or archive externally

2. Explicit tolerance targets
- define measurable limits (max missed samples, max data-loss window, UI response targets)

3. NTP outage policy detail
- define exact fallback timestamp method and resync correction behavior

4. Dashboard security
- decide if simple LAN authentication is needed later

5. Historical depth targets
- define default graph windows (24h, 7d, 30d) and data decimation policy

6. Long-run service strategy
- maintenance and reliability expectations if deployed for longer periods

## 9) Open Notes
- This is a side project and requirements may adapt as memory/capacity constraints are discovered.
- Priority order is practicality and stability over perfect timing precision.
