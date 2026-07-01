1) Architecture Summary
Assumptions used for milestone-1 scaffold:

Board environment uses Arduino framework on ESP32-S3 and can start from a generic S3 PlatformIO board profile, then tune pin mapping.
Exact LILYGO T5 4.7 panel variant and pin map may differ by hardware revision, so all tunables and pin assumptions are centralized.
Milestone 1 web scope is lightweight local status + JSON APIs only, with no auth and no heavy frontend.

2) Architecture:

App coordinator schedules periodic jobs with millis-based non-blocking timing (sampling, flush, display refresh, diagnostics).
Sensor manager handles BME280 init/read over I2C.
Time manager handles NTP sync and estimated fallback timestamps.
Logger manager uses fixed-size RAM queue and periodic CSV flush to SD, plus event logging.
Display manager performs full-refresh e-ink updates and draws a mini temperature trend graph from fixed-size history.
Web manager serves local status page and two low-overhead JSON endpoints.
Diagnostics manager emits periodic memory/buffer telemetry to serial.
Fixed-size ring buffers are used for graph history and log queue to avoid unbounded growth.

2) Full File Tree
Firmware tree (excluding .git internals):
Firmware tree (excluding .git internals):

Breif.md
ProjectBrief.md
platformio.ini
README.md
AppConfig.h
Sample.h
SystemHealth.h
RingBuffer.h
main.cpp
AppCoordinator.h
AppCoordinator.cpp
SensorManager.h
SensorManager.cpp
TimeManager.h
TimeManager.cpp
LoggerManager.h
LoggerManager.cpp
DisplayManager.h
DisplayManager.cpp
WebManager.h
WebManager.cpp
DiagnosticsManager.h
DiagnosticsManager.cpp
3) Full File Contents
## omited see files / git history.
4) Test Plan And Verification Commands
Milestone verification sequence:

Set Wi-Fi and pin mapping in AppConfig.h.
Build firmware:
pio run
Upload firmware:
pio run -t upload
Open serial monitor:
pio device monitor -b 115200
Confirm startup logs: sensor, SD, Wi-Fi/IP, NTP sync state.
Verify local endpoints in browser using device IP:
http://DEVICE_IP/
http://DEVICE_IP/api/live
http://DEVICE_IP/api/health
Validate SD logs on card:
/logs/data.csv exists and appends rows.
/logs/events.csv exists and captures ntp_reestablished events.
Soak for 30+ minutes and observe diagnostics lines for stable heap and bounded queue behavior.
Temporarily reduce display interval in AppConfig.h to quickly validate full-refresh graph rendering.
Acceptance checklist mapping:

Boot/status over serial: implemented.
Repeated BME280 sampling: implemented with 1s scheduler target.
CSV create/append with timestamp_quality: implemented.
NTP fallback + re-established event logging: implemented.
E-ink current values + mini temp graph + full refresh path: implemented.
Wi-Fi local status page + live/health JSON: implemented.
Memory telemetry for 30+ min leak checks: implemented.
5) Known Limitations And Phase-2 Plan
Known limitations:

Board profile uses generic ESP32-S3 and assumes pin mapping in config; real T5 4.7 hardware may need pin and panel driver adjustments.
E-ink driver class targets one 4.7-inch panel variant and may need change for your exact revision.
Wi-Fi connect has a short boot-time blocking window (15s max); runtime loop remains non-blocking.
SD failure handling is graceful (queue retained in RAM, health degrades), but no auto-remount/backoff strategy yet.
Historical web graphing/filtering and log download are intentionally deferred for memory safety in milestone 1.
No OTA and no auth, by design for local-only milestone scope.
Phase-2 plan:

Confirm exact T5 panel and pin map, then finalize board environment and display driver mapping.
Add runtime config endpoints for sample/flush/display intervals with persistence.
Add chunked historical API and lightweight web plotting with bounded memory.
Add SD full policy and file rotation strategy.
Add optional button-triggered immediate display refresh.
Add optional local auth layer once baseline performance is stable.
Natural next steps:

I can now do a board-specific pass for your exact T5 revision (pin map + panel class) if you share your silkscreen or a product link/photo.
I can add a quick compile target check and adjust anything needed to make platformio build cleanly on your machine.