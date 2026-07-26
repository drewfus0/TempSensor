# Feature Plan: Custom Sensor Events & Hardware Button Integration

> Saved for future milestone / implementation.

Implement a complete custom event tracking system for sensor data. This includes hardware button triggers (Buttons A & B on the LOLIN OLED Shield v2.0.0), web interface controls to log custom events (with backdating support), live editing/renaming of existing events, and vertical line annotations with labels overlaid directly on the sensor history graph.

---

## Hardware Configuration & Input Handling

- **Display Shield Pin Allocation:**
  - LOLIN OLED Shield v2.0.0 (64x48 I2C display for D1 Mini)
  - Button A: **D3 (GPIO0)**
  - Button B: **D4 (GPIO2)**
  - Use `INPUT_PULLUP` mode; triggers `LOW` when pressed.

- **[AppConfig.h](file:///home/drewfus/TempSensor/include/config/AppConfig.h):**
  ```cpp
  constexpr int BUTTON_A_PIN = 0; // D3 / GPIO0
  constexpr int BUTTON_B_PIN = 2; // D4 / GPIO2
  constexpr uint32_t BUTTON_DEBOUNCE_MS = 150;
  ```

- **[AppCoordinator.cpp](file:///home/drewfus/TempSensor/src/app/AppCoordinator.cpp):**
  - Implement `handleButtons(uint32_t nowMs)` with debouncing and falling edge detection.
  - Button A press logs event `"A"` (or `"Button A"`) to `/logs/events.csv`.
  - Button B press logs event `"B"` (or `"Button B"`) to `/logs/events.csv`.
  - Optional brief OLED toast feedback when pressed (`Event 'A' Logged!`).

---

## Backend Storage & Web APIs

- **Data Format:**
  - Uses existing `/logs/events.csv` structure (`timestamp,quality,eventName`). No breaking schema changes.

- **[LoggerManager.cpp](file:///home/drewfus/TempSensor/src/managers/LoggerManager.cpp):**
  - Add `updateEvent(oldTs, oldName, newTs, newName)` method to rewrite/update targeted event lines in `/logs/events.csv` safely using a temp file transaction (`/logs/events.tmp`).

- **[WebManager.cpp](file:///home/drewfus/TempSensor/src/managers/WebManager.cpp):**
  - `POST /api/events/create`: Accepts `{ "event": "Custom Event Name", "ts": "YYYY-MM-DD HH:MM:SS", "q": "ntp" }`. Supports backdated timestamps.
  - `POST /api/events/update`: Accepts `{ "oldTs": "...", "oldEvent": "...", "newTs": "...", "newEvent": "..." }`. Renames or modifies event timestamp/name.
  - `GET /api/events`: Add `start` and `end` filtering parameters.

---

## Web Dashboard & Dygraphs History Chart Overlay

- **Event Creation Form:**
  - Add a form to the Event Timeline card with Event Name input, Datetime-Local picker (defaulting to current time), and "Add Event" button.

- **Timeline Item Editing:**
  - Add an inline **Edit / Rename** button next to items in `#eventsTimeline` to edit event names or timestamps live.

- **Dygraphs Vertical Lines & Labels:**
  - In `loadHistory()`: Fetch matching event range from `/api/events?start=...&end=...`.
  - Use Dygraph's `underlayCallback` to draw vertical dashed lines (`#f59e0b` amber / `#3b82f6` blue) on the canvas at `g.toDomXCoord(eventTimeMs)`.
  - Render event name badge text near top of chart canvas.
  - Add `[x] Show Events on Graph` toggle checkbox in history chart header.

---

## Verification Steps
1. **OLED Buttons:** Press A & B on shield; confirm Serial log, OLED feedback toast, and CSV append.
2. **Web Creation & Backdating:** Create current and backdated events via web form; verify timeline and CSV log.
3. **Chart Annotations:** Load history graph; confirm vertical dashed lines and labels render over sensor data.
4. **Edit & Rename:** Click edit on an event, rename it, and verify backend CSV update and live chart label refresh.
