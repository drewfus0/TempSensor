# TempSensor Project Handoff - Milestone 3 Kickoff

Date: 2026-07-10  
Author: Antigravity AI  

---

## 1. Current State
* **Milestone 2 Accomplished:** The local web server, OLED shield display, SD log writer, and Web UI are fully operational and stable.
* **Sensor Logging & History:** BME280 metrics are written to pre-allocated daily binary files. The web client downloads data over `/api/history` and aggregates points on the fly.
* **Aggregated Visualizations:** 
  * The main uPlot chart uses client-side binning to display Min/Max range bands (5% opacity) and solid Average lines (100% opacity).
  * Hovering over the graph updates the legend to show combined `Avg | Min | Max` values for each metric in a single clean row, while hiding boundary rows to avoid legend clutter.
  * Added a **Bin Size** dropdown control with an interactive **Custom Seconds** field, which triggers instant, lag-free client-side re-aggregation on the cached data.
* **Battery Analytics:** 
  * Battery statistics are written to `/logs/battery.csv` every 60s.
  * A second uPlot dashboard chart parses this CSV to draw the battery voltage curve over the last 24 hours.
* **OLED Status Line:** Displays temperature, humidity, pressure, Wi-Fi status, battery percentages, and a **1-pixel wide vertical queue line** on the far right column showing RAM buffer fill levels.

---

## 2. Resource & Build Status
* **RAM:** **53.6%** (used 43,892 bytes of 81,920 bytes)
* **Flash:** **45.0%** (used 469,511 bytes of 1,044,464 bytes)
* **Verify Command:** `pio run` (compiles successfully with zero warnings/errors in program code).

---

## 3. Milestone 3 Roadmap (Next Session Tasks)

### Task 1: Over-the-Air (OTA) Updates
* Integrate `ArduinoOTA` or a simple HTTP-based web updater so the board can be flashed remotely over Wi-Fi, avoiding physical serial cable connections.

### Task 2: Signal Stability & Soldering
* Solder pins and components together in a stacked shield design to replace temporary breadboard jumper connections and eliminate signal/power drops.

### Task 3: Battery Slope Analysis (State Tracking)
* Implement algorithms that parse the slope of the battery voltage curve over time. Use this trend to dynamically determine and display whether the battery is currently `Charging` or `Discharging` (before the physical diode sensing hardware is added).

### Task 4: Solar Study
* Perform a technical feasibility study to calculate power consumption versus solar panel output. Determine if 4–6 hours of daily sunlight is enough to maintain a positive charge cycle.

### Task 5: Detailed System Logging to Serial
* Redirect important transitions to the Serial console for easier debugging:
  * Log incoming web API calls.
  * Trace NTP status (failure, retry, success).
  * Trace SD card write queue flushes (success vs. failures).
  * Trace Wi-Fi status transitions.

### Task 6: Retroactive Timestamp Calibration
* When the board boots without NTP, it logs with estimated timestamps. Implement a parser that retroactively updates the estimated timestamps in the event files and daily binary logs once NTP successfully syncs.

---

## 4. Suggested First Steps for Next Session
1. Run `pio run` to verify the build is in a clean starting state.
2. Select one of the Milestone 3 tasks (e.g. implementing **OTA Updates** or **Serial Diagnostics**).
3. Reference active managers in:
   * [WebManager.cpp](file:///home/drewfus/TempSensor/src/managers/WebManager.cpp) (web server endpoints and OTA update portal).
   * [LoggerManager.cpp](file:///home/drewfus/TempSensor/src/managers/LoggerManager.cpp) (events logging and CSV formatting).
   * [BatteryManager.cpp](file:///home/drewfus/TempSensor/src/managers/BatteryManager.cpp) (prediction values and slopes).
