# TempSensor Firmware (ESP8266 / LOLIN D1 mini Pro v2.0.0)

Firmware project for an autonomous, continuous environmental monitoring station powered by a **LOLIN D1 mini Pro v2.0.0** (ESP8266) equipped with:
- **Bosch BME280 Sensor** (Temperature, Relative Humidity, Barometric Pressure via I2C)
- **LOLIN OLED Shield v2.0.0** (64x48 SSD1306 display via I2C with hardware buttons A & B)
- **LOLIN microSD Card Shield v1.2.0** (High-speed SPI binary logging & storage)
- **LiPo Battery Monitor & Machine Learning Estimator** (ADC A0 sensing with adaptive baseline rate calibration)
- **Embedded Web Administration Dashboard** (Dark-mode responsive Single-Page App with Dygraphs historical charts, event management, live telemetry, SD card explorer, and OTA updates)

---

## Hardware Profile & Pinout

- **MCU**: LOLIN D1 mini Pro v2.0.0 (ESP8266EX @ 80MHz, 80KB SRAM, 16MB Flash)
- **Sensor**: Bosch BME280 (I2C address `0x76` default with automatic fallback to `0x77`)
- **Display**: LOLIN OLED Shield v2.0.0 (64x48 pixels, I2C address `0x3C`)
- **Buttons**:
  - Button A: `D3` (GPIO0) — triggers hardware custom event logging (Category 2)
  - Button B: `D4` (GPIO2) — triggers hardware custom event logging (Category 3)
- **Storage**: LOLIN microSD Card Shield v1.2.0 (SPI)
- **Battery**: Single-cell 3.7V LiPo connected to onboard charger and ADC `A0`

### Wiring & Pin Summary

| Bus / Function | D1 Mini Pin | ESP8266 GPIO | Connected Hardware / Function |
| :--- | :--- | :--- | :--- |
| **I2C SDA** | `D2` | GPIO4 | BME280 + OLED Shield (shared I2C data) |
| **I2C SCL** | `D1` | GPIO5 | BME280 + OLED Shield (shared I2C clock) |
| **SPI SCK** | `D5` | GPIO14 | microSD Card Shield clock |
| **SPI MISO** | `D6` | GPIO12 | microSD Card Shield data in |
| **SPI MOSI** | `D7` | GPIO13 | microSD Card Shield data out |
| **SPI CS** | `D0` | GPIO16 | microSD Card Shield Chip Select (*rerouted from D4 pad*) |
| **Button A** | `D3` | GPIO0 | OLED Shield Button A (`INPUT_PULLUP`, active LOW) |
| **Button B** | `D4` | GPIO2 | OLED Shield Button B (`INPUT_PULLUP`, active LOW) |
| **Battery ADC** | `A0` | ADC0 | Resistor-divided LiPo battery voltage sensing |
| **Power** | `3V3` / `GND` | — | 3.3V power rails for sensor, display, and SD shield |

> [!NOTE]
> The LOLIN microSD Card Shield v1.2.0 default CS pad (`D4` / GPIO2) is rerouted to **`D0` (GPIO16)** via a solder jumper to prevent boot conflicts with the ESP8266 boot-strap pins and onboard LED.

---

## Core System Architecture

1. **Preallocated Binary Daily Logging ($O(1)$ Direct Access)**:
   - Records are organized into fixed-size daily files (`/logs/YYYY-MM-DD.bin`) containing 86,400 packed 17-byte `LogRecord` slots (1 second per slot = 1,468,800 bytes per day).
   - Writes and historical range queries seek directly to `slotIndex * 17` without parsing CSV text or causing filesystem fragmentation.
   - Offline records taken before NTP time sync are buffered in `/logs/estimated.bin` and retroactively calibrated once NTP time is established.

2. **Battery Monitoring & Adaptive Baseline Learning**:
   - 8-sample burst ADC averaging coupled with an Exponential Moving Average (EMA, 85/15 filter) to suppress Wi-Fi transmission noise.
   - 10-point rolling regression slope calculation (volts per minute) for robust charge vs. discharge state detection.
   - Self-learning runtime estimator that analyzes full discharge runs ($\ge 10\%$ drop over $\ge 1\text{h}$) to calibrate the discharge baseline rate ($s / 1\%$), storing the updated baseline to SD `/config.json`.
   - Continuous battery telemetry logged to `/logs/battery.bin` every 60 seconds.

3. **Autonomous Sensor Fault Detection & Hardware Auto-Recovery**:
   - Active I2C ACK checks prior to register reads.
   - Probes both `0x76` and `0x77` BME280 addresses dynamically.
   - Detects NACKs, `NaN` readings, out-of-bounds values, and frozen sensor outputs.
   - Automatic 4-step hardware bus recovery routine (bit-banging SCL 16× to unstick SDA, STOP condition generation, Wire re-init, BME280 Power-On Reset `0xB6` to register `0xE0`, and driver re-init).
   - Automatic graceful fallback to internal simulation mode when no physical sensor hardware is attached.
   - Logs fault events and recovery milestones with outage durations to the event ledger.

4. **Event Tracking & Timeline System**:
   - Dedicated preallocated binary chunked storage (`/logs/events/ev_YYYYMMDD_XXX.bin`, 1,000 slots × 136-byte `EventRecord` per file).
   - Categorized event logging: *System*, *Custom Web*, *Button A*, *Button B*, *Sensor Fault*, *Network Sync*, *Power / Battery*.
   - Web interface support for creating, backdating, editing, and deleting events in-place.
   - Visual vertical event markers and labels overlaid directly onto Dygraphs historical sensor charts.

5. **Local Web Dashboard & REST API**:
   - Single-Page Application served in chunks from flash `PROGMEM` via port 80.
   - Real-time telemetry cards (Temperature, Humidity, Pressure, Battery level & status, Time Remaining, Heap health, Queue depth).
   - Interactive Dygraphs historical graphs with zoom, pan, synchronized crosshairs, custom time ranges (1h, 6h, 24h, 7d, all, custom), and dynamic data binning.
   - SD Card file browser with direct download, file deletion, and file renaming.
   - Web-based runtime configuration editing (`/config.json`) and firmware OTA upload.
   - Automatic SoftAP failover mode (`tempsensor-d1mini`) if station Wi-Fi disconnects for >30 seconds.

---

## Directory Hierarchy

```
TempSensor/
├── src/
│   ├── main.cpp                    # Minimal firmware entry point
│   ├── app/
│   │   ├── AppCoordinator.cpp       # System lifecycle, timers & task coordination
│   │   └── AppCoordinator.h
│   └── managers/
│       ├── SensorManager.cpp        # BME280 driver, auto-probe, fault & bus recovery
│       ├── SensorManager.h
│       ├── BatteryManager.cpp       # LiPo ADC sampling, regression slope, baseline learning
│       ├── BatteryManager.h
│       ├── DisplayManager.cpp       # SSD1306 OLED layout, tiny fonts, queue/OTA meters
│       ├── DisplayManager.h
│       ├── LoggerManager.cpp        # Preallocated binary logs, RAM queue, SD recovery
│       ├── LoggerManager.h
│       ├── TimeManager.cpp          # NTP sync, timezone handling, estimated clock
│       ├── TimeManager.h
│       ├── WebManager.cpp           # HTTP REST endpoints, chunked HTML server, SoftAP
│       ├── WebManager.h
│       ├── DiagnosticsManager.cpp   # Periodic Serial console health reporting
│       └── DiagnosticsManager.h
├── include/
│   ├── config/
│   │   └── AppConfig.h              # Unified hardware pinout, intervals & compile defaults
│   ├── models/
│   │   ├── Sample.h                 # Sample, LogRecord, BatteryRecord, EventRecord
│   │   ├── SystemHealth.h           # Heap, queue, sensor & battery health metrics
│   │   └── DeviceConfig.h           # Runtime settings schema saved to /config.json
│   ├── utils/
│   │   └── RingBuffer.h             # Fixed-capacity lock-free in-memory circular buffer
│   └── web/
│       ├── DashboardPage.h          # Single-Page Web Dashboard (HTML/CSS/JS in PROGMEM)
│       └── uplot_assets.h           # Standalone bundled charting assets
├── old_docs/                        # Historical briefs, specifications, and study notes
├── platformio.ini                   # PlatformIO project configuration (d1_mini_pro)
├── README.md                        # Project overview & documentation (this file)
└── gemini.md                        # LLM developer reference & technical architecture guide
```

---

## Build, Flash, and Monitor

### Prerequisites
- [PlatformIO Core](https://platformio.org/) (CLI) or the VS Code PlatformIO extension.
- Serial USB driver (CH340 / CP210x) and appropriate USB dialout permissions on Linux.

### Commands

```bash
# Compile firmware
pio run

# Flash over USB Serial
pio run -t upload

# Open Serial Monitor (115200 baud)
pio device monitor -b 115200

# Over-The-Air (OTA) Network Flash (ArduinoOTA)
pio run -t upload --upload-port tempsensor-d1mini.local
```

---

## Web REST API Summary

| Method | Endpoint | Description |
| :--- | :--- | :--- |
| `GET` | `/` | Serves the Single-Page Web Dashboard |
| `GET` | `/api/live` | Returns latest sensor telemetry (T, H, P, timestamp, quality, uptime) |
| `GET` | `/api/health` | Returns system telemetry (heap, block size, queue, Wi-Fi, SD, battery metrics) |
| `GET` | `/api/config` | Returns active runtime device configuration |
| `POST` | `/api/config` | Updates configuration, saves to SD `/config.json`, triggers restart if Wi-Fi changes |
| `GET` | `/api/history` | Fetches historical log data with binning (`metric`, `max_points`, `start`, `end`) |
| `GET` | `/api/events` | Fetches event timeline records (`start`, `end`, `limit`) |
| `POST` | `/api/events/create` | Creates a new event record with optional backdating |
| `POST` | `/api/events/update` | Updates an existing event record in-place |
| `POST` | `/api/events/delete` | Marks an event record as deleted/invalid |
| `GET` | `/api/logs` | Lists all log files and sizes stored on the SD card |
| `GET` | `/api/logs/download` | Streams a specific file download from the SD card (`?file=<path>`) |
| `POST` | `/api/logs/delete` | Deletes a file on the SD card (`file=<path>`) |
| `POST` | `/api/logs/rename` | Renames a file on the SD card (`old=<path>&new=<path>`) |
| `GET` | `/api/sd-tree` | Returns hierarchical directory tree of the SD card |
| `POST` | `/api/action/flush-now` | Flushes the RAM sample queue immediately to the SD card |
| `POST` | `/api/action/ntp-retry` | Resets NTP backoff and triggers an immediate time synchronization attempt |
| `POST` | `/api/update` | Web OTA firmware binary upload endpoint |
