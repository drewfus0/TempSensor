# Feature Plan: Custom Sensor Events & Preallocated Rolling Event Storage

> Saved for future milestone / implementation.

Implement a complete custom event tracking system for sensor data with high-performance preallocated binary event storage. This includes hardware button triggers (Buttons A & B on the LOLIN OLED Shield v2.0.0), web interface controls to log custom events (with backdating support), live editing/renaming of existing events, and vertical line annotations with labels overlaid directly on the sensor history graph.

---

## Preallocated Binary Event Storage Architecture

- **Fixed Packed Record (`EventRecord` - 136 bytes):**
  ```cpp
  struct __attribute__((packed)) EventRecord {
    uint32_t epochTime;     // 4 bytes: Unix timestamp (seconds since 1970)
    uint8_t  quality;       // 1 byte: 0 = NTP, 1 = Estimated
    uint8_t  category;      // 1 byte: 0 = System, 1 = Custom Web, 2 = Button A, 3 = Button B
    char     message[128];  // 128 bytes: Null-terminated UTF-8 event text
    uint8_t  reserved[2];   // 2 bytes: Alignment padding to 136 bytes (multiple of 4)
  };
  ```

- **File Naming & Rollover Scheme (`YYYYMMDD` formatted creation date):**
  - **Path Format:** `/logs/events/ev_YYYYMMDD_XXX.bin` (e.g., `/logs/events/ev_20260726_001.bin`)
  - **Capacity:** Each chunk file is preallocated for **1,000 events** ($1,000 \times 136\text{ B} = \mathbf{136\text{ KB}}$ per file).
  - **Creation & Rollover:** When a file reaches 1,000 events, a new preallocated chunk file is generated using the current creation date `YYYYMMDD` and incremental sequence counter (e.g., `/logs/events/ev_20261015_002.bin`).
  - **Indefinite Retention:** Event files remain stored on the SD card indefinitely across chunks.

- **In-Place Editing / Renaming ($O(1)$ Direct Overwrite):**
  - Renaming or editing an event does not require full file rewrites. The system seeks directly to `slotIndex * sizeof(EventRecord)` and overwrites the 136-byte record in-place.

- **Legacy Migration:**
  - On first boot with new binary logger, existing `/logs/events.csv` lines are migrated into `/logs/events/ev_YYYYMMDD_001.bin` and the old CSV is renamed to `events.csv.bak`.

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
  - Button A press logs event `"A"` (or `"Button A"`) category 2.
  - Button B press logs event `"B"` (or `"Button B"`) category 3.
  - Brief OLED toast feedback when pressed (`Event 'A' Logged!`).

---

## Backend APIs & Data Management

- **[LoggerManager.cpp](file:///home/drewfus/TempSensor/src/managers/LoggerManager.cpp):**
  - Implement binary event chunk manager (`logEvent`, `updateEventRecord`, `initEventStorage`).

- **[WebManager.cpp](file:///home/drewfus/TempSensor/src/managers/WebManager.cpp):**
  - `POST /api/events/create`: Accepts `{ "event": "Custom Event Name", "ts": "YYYY-MM-DD HH:MM:SS", "q": "ntp" }`. Supports backdated timestamps.
  - `POST /api/events/update`: Accepts `{ "file": "ev_20260726_001.bin", "index": 42, "event": "Updated Name" }`.
  - `GET /api/events`: Efficient binary streaming of requested event ranges or latest $N$ events across active chunk files.

---

## Web Dashboard & Dygraphs History Chart Overlay

- **Event Creation Form:**
  - Add a form to the Event Timeline card with Event Name input (max 128 chars), Datetime-Local picker (defaulting to current time), and "Add Event" button.

- **Timeline Item Editing:**
  - Add an inline **Edit / Rename** button next to items in `#eventsTimeline` to edit event names or timestamps live.

- **Dygraphs Vertical Lines & Labels:**
  - In `loadHistory()`: Fetch matching event range from `/api/events?start=...&end=...`.
  - Use Dygraph's `underlayCallback` to draw vertical dashed lines (`#f59e0b` amber / `#3b82f6` blue) on the canvas at `g.toDomXCoord(eventTimeMs)`.
  - Render event name badge text near top of chart canvas.
  - Add `[x] Show Events on Graph` toggle checkbox in history chart header.

---

## Verification Steps
1. **OLED Buttons:** Press A & B on shield; confirm Serial log, OLED feedback toast, and binary event record write.
2. **Web Creation & Backdating:** Create current and backdated events via web form; verify timeline and binary log file.
3. **Chart Annotations:** Load history graph; confirm vertical dashed lines and labels render over sensor data.
4. **Edit & Rename:** Click edit on an event, rename it, and verify in-place binary record overwrite and live chart label refresh.
