# TempSensor Project Reference Guide (gemini.md)

This document provides a comprehensive overview of the **TempSensor** project architecture, hardware mapping, codebase structure, and API endpoints. It serves as the primary context reference for LLM developers and future prompt iterations.

---

## 1. Project & Hardware Overview

The project is an ESP8266-based temperature, humidity, and atmospheric pressure logging system deployed on a **LOLIN D1 mini Pro v2.0.0** MCU. It features persistent storage, an OLED visual display, and a local web-based administration dashboard.

### 1.1 Hardware Components
- **MCU**: LOLIN D1 mini Pro v2.0.0 (ESP8266, 80MHz, 80KB RAM, 16MB Flash).
- **Sensor**: BME280 (Temperature, Humidity, Pressure) connected via I2C.
- **Display**: LOLIN OLED Shield v2.0.0 (64x48 pixels, SSD1306 driver) connected via I2C.
- **Storage**: LOLIN microSD Card Shield v1.2.0 connected via SPI.

### 1.2 Shared Bus Pin Mapping

```
                 +----------------------+
                 |  LOLIN D1 mini Pro   |
                 +----------------------+
                    |                |   
             (I2C)  |                | (SPI)
           +--------+                +-------------+
           |                                       |
     SDA = D2 (GPIO4)                       SCK  = D5 (GPIO14)
     SCL = D1 (GPIO5)                       MISO = D6 (GPIO12)
                                            MOSI = D7 (GPIO13)
                                            CS   = D8 (GPIO15) *
```
*\* Note: The microSD Card Shield CS pin is rerouted from its default D4 pad to D8 via solder jumper to avoid conflicting with the built-in LED/Boot pins.*

### 1.3 Hardware Config Parameters
- **BME280 I2C Address**: `0x76` (SDO pulled Low to GND).
- **OLED Dimensions**: 64x48 pixels.
- **Serial Baud Rate**: 115200.

---

## 2. Directory Hierarchy

```
TempSensor/
├── src/
│   ├── main.cpp                    # Firmware Entry point (setup/loop)
│   ├── app/
│   │   ├── AppCoordinator.cpp       # Core loop routines coordinator
│   │   └── AppCoordinator.h
│   └── managers/
│       ├── SensorManager.cpp        # BME280 sensor driver abstraction
│       ├── SensorManager.h
│       ├── DisplayManager.cpp       # SSD1306 OLED shield rendering
│       ├── DisplayManager.h
│       ├── LoggerManager.cpp        # RAM queue + SD persistence (CSV format)
│       ├── LoggerManager.h
│       ├── TimeManager.cpp          # NTP sync timer & estimated backup clock
│       ├── TimeManager.h
│       ├── WebManager.cpp           # HTTP REST endpoints & HTML Dashboard page
│       ├── WebManager.h
│       ├── DiagnosticsManager.cpp   # Periodic debug logger to Serial
│       └── DiagnosticsManager.h
├── include/
│   ├── config/
│   │   └── AppConfig.h              # Unified hardware pin & interval settings
│   ├── models/
│   │   ├── Sample.h                 # Sensor reading data structure
│   │   └── SystemHealth.h           # Heap/uptime status data structure
│   └── utils/
│       └── RingBuffer.h             # Fixed-capacity lock-free queue in RAM
├── ProjectBrief.md                  # Milestone specifications & guidelines
└── gemini.md                        # Project reference (this file)
```

---

## 3. Architecture & Coordination Flow

The project follows a centralized coordination architecture. All system lifecycle tasks are initiated inside `src/main.cpp` and orchestrated by `AppCoordinator`.

```
                    +-------------------+
                    |     main.cpp      |
                    +-------------------+
                              | (setup/loop)
                              v
                    +-------------------+
                    |  AppCoordinator   |
                    +-------------------+
          ___________/  |    |    |   \__________
         /             /     |     \             \
        v             v      v      v             v
   [Sensor]        [Time] [Logger] [Display]    [Web]
   Manager        Manager Manager  Manager     Manager
```

- **Boot Sequence (`AppCoordinator::begin`)**:
  1. Initializes the `DisplayManager` and prints status.
  2. Starts hardware `SPI` for the SD card.
  3. Initializes the `SensorManager` (BME280) and checks its signature.
  4. Mounts the SD Card filesystem through the `LoggerManager`.
  5. Launches the Wi-Fi client and boots the `WebManager` HTTP server.
  6. Synchronizes NTP time using the `TimeManager`.
- **System Loop (`AppCoordinator::loop`)**:
  - Updates the NTP timer state and writes `"ntp_reestablished"` event logs upon reconnects.
  - Samples the BME280 at `SAMPLE_INTERVAL_MS` (e.g. 1000ms), packages metrics into a `Sample` struct, and enqueues it to the RAM `RingBuffer`.
  - Checks if `LOG_FLUSH_INTERVAL_MS` (e.g. 60000ms) has elapsed and flushes the queue to the SD card under `/logs/data.csv`.
  - Refreshes the 64x48 OLED screen with live readings, connection status, and IP.
  - Gathers memory usage metadata and prints health diagnostics to the console.

---

## 4. Managers & Components

### 4.1 SensorManager
- Reads pressure, humidity, and temperature.
- Handles BME280 connection failures and returns fallback flags.

### 4.2 DisplayManager
- Uses the `Adafruit_SSD1306` library mapped to 64x48 dimensions.
- Renders system boot indicators on startup.
- Displays a cyclical view of current temperature, humidity, pressure, Wi-Fi status, and the assigned DHCP local IP.

### 4.3 LoggerManager
- Stores unsaved readings in a volatile RAM buffer (`RingBuffer<Sample, 512>`) to reduce SD card wear and prevent slow write blockages from stalling the main cycle.
- Persists data to `/logs/data.csv` using streams. Format:
  `YYYY-MM-DD HH:MM:SS,Quality,TempC,HumPct,PresHpa,UptimeS`
- Maintains an event log under `/logs/events.csv` for tracking critical milestones (e.g., NTP re-established, diagnostics alerts).

### 4.4 TimeManager
- Synchronizes with `pool.ntp.org` and `time.nist.gov`.
- Retries NTP sync with an exponential backoff-based retry interval starting at 30 seconds.
- Provides fallback estimated clock calculations when NTP is unavailable (`TimestampQuality::Estimated`).

### 4.5 WebManager
- Hosts a local HTTP Web Server (`ESP8266WebServer`) listening on port 80.
- Serves the Single-Page Dashboard HTML page using `PROGMEM` to avoid exhausting the limited ESP8266 SRAM.
- Exposes JSON REST API endpoints to feed telemetry, charts, configuration changes, and custom runtime commands.

---

## 5. Web REST API Endpoints

### 5.1 Telemetry & Status
- **`GET /api/live`**: Returns the latest temperature, humidity, pressure, timestamp, quality, and IP address.
- **`GET /api/health`**: Returns system telemetry (free heap, max free block size, queue depth, queue capacity, dropped logs, and network connection flags).

### 5.2 Configurations & Runtime Controls
- **`GET /api/config`**: Returns the current intervals (Sample Interval, Flush Interval, Display Refresh).
- **`POST /api/config`**: Submits new intervals. Operates with a preview/rollback logic.
- **`POST /api/action/flush-now`**: Triggers an immediate write flush of the RAM buffer to `/logs/data.csv`.
- **`POST /api/action/ntp-retry`**: Resets the NTP retry backoff timer to force a manual synchronization attempt.

### 5.3 Files & Events
- **`GET /api/history`**: Retrieves historical log data. Accepts query parameters (`metric`, `max_points`, `start`, `end`). Automatically downsamples large datasets to keep payload sizes small.
- **`GET /api/logs`**: Lists all log files stored on the SD card in JSON format.
- **`GET /api/logs/download?file=<path>`**: Serves a file download stream for a specific log file (e.g., `/logs/data.csv`).
- **`GET /api/events`**: Returns a JSON array of parsed event records from `/logs/events.csv` (e.g. `?limit=30`).
- **`GET /api/sd-tree`**: Serves a flat text representation of the SD card file tree layout.
