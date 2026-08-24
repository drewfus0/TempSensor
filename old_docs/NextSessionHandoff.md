# TempSensor Project Handoff - Milestone 5 Kickoff

Date: 2026-07-14  
Author: Antigravity AI  

---

## 1. Current State (Milestone 4 Accomplished)

We have successfully completed all planned features for **Milestone 4**:
*   **Persistent Configuration Storage:** Created [DeviceConfig.h](file:///home/drewfus/TempSensor/include/models/DeviceConfig.h) configuration models saved persistently to `/config.json` on the SD card using `ArduinoJson`.
*   **Timezone & Network Configurations:** Added runtime timezone configurations rule reloading (`TimeManager::setTimezone()`) and dynamic adjustments of sampling, flush, and display refresh intervals without rebooting.
*   **Wi-Fi AP Fallback Recovery Mode:** If Wi-Fi fails to connect on boot (after 15s) or disconnects during loop execution (after 30s), the board starts a local fallback Access Point (`<Hostname>-AP`) allowing configuration recovery via `http://192.168.4.1`. Upon reconnecting, the fallback AP is disabled.
*   **Unified SD File Explorer:** Replaced separate tree and downloads sections with a single, auto-sizing unified file explorer displaying folders/files. Added click-to-download with correct original filenames, connection-closure headers to prevent downloads sticking at 100%, and inline **Rename** and **Delete** actions for both files and empty directories anywhere on the SD card.
*   **OLED Screen Indicators:** Mapped battery percentage to line 4, showing real-time states (`Chg`, `Dis`, `Ful`). Configured the OLED status line to show `W-/AP` and print the AP IP address `192.168.4.1` when fallback AP is active.
*   **Binary Battery Logging & Browser-side CSV Converter:** Switched battery logging from a verbose text CSV format to a highly efficient packed binary structure (`BatteryRecord`, 14 bytes) written to `/logs/battery.bin`. The browser downloads this raw binary file, decodes it using a Javascript `DataView`, and renders the history chart. Added a **Download CSV** button next to the battery graph, allowing the user to export the data to CSV on-the-fly entirely client-side, eliminating CSV generation overhead on the ESP8266.
*   **Event Timeline Chronology:** Moved the Event Timeline back to the main dashboard. Refactored the ESP8266 backend (`streamEventsJson()`) to seek backward from the end of `events.csv`, retrieving the actual latest 30 events in milliseconds, rendered with full dates and times, ordering the newest events at the very top. Added initial boot-time Wi-Fi SSID and NTP sync success logging.
*   **A11y Fixes:** Associated all 8 settings form fields and OTA file selector labels with their respective input elements using `for` attributes.
*   **Sticky Header, UI Refresh Settings, and Single-Tick Scheduler:** Nested the header logo, status pills, and tab buttons inside a sticky glassmorphism container. Replaced independent interval timers with a unified **10-second priority scheduler**. Every 10 seconds, the scheduler evaluates which tasks (Live, Health, Events, Battery, SD Tree) are due, resolves conflicts by prioritizing **how overdue they are** (Maximum Overdue First) with a static priority tie-breaker (SD Tree > Battery > Events > Health > Live) if they become due on the same tick, and executes **exactly one task** per tick. This prevents task starvation (like `live` getting backed up) while keeping the ESP8266 completely safe from concurrent network calls.
*   **Interactive Force Refresh Buttons & Fetch Lockout:** Added a small, square refresh button (`⟳`) styled with active click micro-animations and a fluid 180-degree rotation transition to the headers of the 5 cards (Live, Health, Events, Battery, and SD File Explorer). When any task is fetching (either manually or via the scheduler), the system sets a global `isFetching = true` lockout. This temporarily dims and disables all refresh buttons, updates the cursor to `wait` / `not-allowed`, and locks out the scheduler from triggering any new network requests. Once the fetch completes, all controls are immediately unlocked, guaranteeing total mutual exclusivity for ESP8266 HTTP transactions.
*   **Post-Discharge Window Evaluation, Calibration Event Logging & Single SD Save per Run:** Refined the adaptive battery rate learning mechanism to evaluate and adjust the baseline rate **only at the end of a completed discharge run** (upon confirming transition to `Charging / USB`). Discharge window tracking ignores top-of-battery fluctuations and charger re-engagements above 90% (only capturing data from $\le 90\%$ down to the end of the run). Upon charging, if a valid run is confirmed ($\ge 10\%$ drop and $\ge 1\text{h}$ duration), the new run rate is blended into the historical baseline (70% historical + 30% new run rate), saved to `/config.json` on the SD card **exactly once per discharge cycle**, and logged as a descriptive event `battery_rate_calibrated (1000s/1% -> 1045s/1%, 29.0h full runtime)` to `/logs/events.csv` (visible in the Event Timeline). This eliminates ~100 redundant SD card writes per discharge run while preserving Full status detection at $\ge 99\%$.
*   **Hysteresis Charger Detection State Machine:** Replaced the noise-prone regression slope charger check with a robust **Dual-Hysteresis State Machine**. Detects instant USB plug/unplug events using rapid delta-voltage step checks ($\pm 30\text{mV}$ in 1 minute) for maximum responsiveness, and uses debounced slope checks ($+1.5\text{mV/min}$ charging, $-1.0\text{mV/min}$ discharging) over 3 consecutive minutes to filter out all transient ADC noise wiggles from raw battery reports.
*   **Full Battery History, Time-to-Empty Predictions & uPlot Solar Layer Fix:** Updated the battery history graph (`WebManager.cpp`) to render **all available historical data** from `/logs/battery.bin` without clipping older points. Extended dashed predictions for the **FULL predicted time to empty** ($0\%$ capacity / $3.400\text{V}$) when discharging, or **time to full** ($100\%$ capacity / $4.150\text{V}$) when charging. Fixed uPlot solar rendering hooks by moving night slate shading (`rgba(2, 6, 23, 0.55)`) to the pre-render `drawClear` hook and moving vibrant Sunrise (`☀` yellow) / Sunset (`🌙` orange) dotted 1.5px vertical lines and labels to the post-render `draw` hook, guaranteeing they are rendered clearly on top of grid and series lines.
*   **Runtime SD Card Recovery & Config Auto-Reload:** Implemented automatic runtime recovery for the SD card without requiring system reboots. If the SD card is missing or fails on boot, the device operates in safe Degraded Mode with default RAM settings. As soon as the SD card recovers or is inserted, the background polling engine detects it, re-mounts the file system, flushes pending RAM sensor logs, and **automatically reloads `/config.json` from the SD card**. It updates `BatteryManager` (learned rate baseline) and `TimeManager` (timezone) live in RAM, and appends an audit event `sd_card_recovered (Restored config.json from SD)` to `/logs/events.csv`.
*   **High-Precision Battery Voltage Formatting:** Increased the formatting precision of the battery voltage throughout the web dashboard from 2 to **3 decimal places** (`toFixed(3)`). This applies to the uPlot hover tooltips (both historical and predicted series), the latest status metadata text block, the live system health card, and the generated battery history CSV export. This preserves the 32-bit floating-point voltage precision stored in `/logs/battery.bin`, eliminating discrete quantization rounding artifacts (e.g. mapping `3.815V` to `3.82V` while reporting `48%` capacity).

---

## 2. Resource & Build Status

*   **RAM:** **57.1%** (used 46,788 bytes of 81,920 bytes)
*   **Flash:** **50.7%** (used 529,528 bytes of 1,044,464 bytes)
*   **Verify Command:** `pio run` (compiles successfully with zero warnings/errors).

---

## 3. Milestone 5 Roadmap (Next Session Tasks)

Milestone 5 is dedicated to a comprehensive **Code Review, Structural Optimization, and Memory Audit**:

### Task 1: Code Comprehension & Walkthrough
*   Document the logical structure and interactions between the components:
    *   [AppCoordinator](file:///home/drewfus/TempSensor/src/app/AppCoordinator.cpp) (execution loop coordinator)
    *   [WebManager](file:///home/drewfus/TempSensor/src/managers/WebManager.cpp) (REST APIs, static PROGMEM assets, AP recovery, OTA uploader)
    *   [LoggerManager](file:///home/drewfus/TempSensor/src/managers/LoggerManager.cpp) (SD card logs queue, JSON configurations persistence)
    *   [TimeManager](file:///home/drewfus/TempSensor/src/managers/TimeManager.cpp) (NTP time synchronization, POSIX timezone parser)
    *   [DisplayManager](file:///home/drewfus/TempSensor/src/managers/DisplayManager.cpp) (SSD1306 custom graphics, status lines)

### Task 2: Structural Code Improvements
*   Investigate refactoring code patterns to improve read/write operations stability.
*   Evaluate separation of concerns, decoupling managers where appropriate (e.g. standardizing event log interfaces).
*   Assess inline styles and raw literal HTML templates storage, reviewing options to host assets or gzip templates.

### Task 3: Memory & Heap Analysis
*   Detail the device's heap allocation patterns.
*   Identify potential risk areas for memory leaks, fragmentation, or Stack Overflow on ESP8266.
*   Analyze static RAM usage breakdown:
    *   OLED screen buffer: 384 bytes.
    *   Logger Ring Buffer (`RingBuffer<Sample, 128>`): ~6.1 KB static RAM.
    *   ESP8266 web client headers buffers and JSON parser sizing constraints.

---

## 4. Suggested First Steps for Next Session

1.  Review the memory footprints under active API calls (e.g., streaming long logs).
2.  Start with a structural critique of [WebManager.cpp](file:///home/drewfus/TempSensor/src/managers/WebManager.cpp) to isolate the massive PROGMEM HTML template literal from API routing logic.
