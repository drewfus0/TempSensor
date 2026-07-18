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
*   **Battery Prediction Refinement & Noise Filtering:** Validated the prediction algorithm using a 31-hour full discharge cycle from the new battery. Found the EMA filter successfully adapted the estimation rate to `1045` seconds per percent (within 6% of the true `1116` seconds/percent rate). Tuned the default baseline from `600s` to `1000s` (approx. 27.8h run time) to match the new battery profile immediately on boot. Optimized charging state detection by implementing a dynamic slope threshold ($5\text{mV/min}$ when voltage $< 4.0\text{V}$) to ignore transient ADC noise fluctuations while maintaining a $0.6\text{mV/min}$ threshold for CV charging mode.

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
