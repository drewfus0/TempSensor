# TempSensor Project Technical Reference Guide (`gemini.md`)

This document serves as the primary technical context and architecture manual for the **TempSensor** firmware codebase. It is designed for LLM developers and engineers requiring deep familiarity with the hardware topology, data formats, execution flows, and API contracts.

---

## 1. Hardware Architecture & Pin Map

The system is deployed on an **ESP8266** microcontroller (**LOLIN D1 mini Pro v2.0.0**, 80MHz, 80KB SRAM, 16MB Flash) interfacing three peripheral shields and analog battery telemetry.

### 1.1 Pinout & Bus Mapping

```
                         +------------------------+
                         |   LOLIN D1 mini Pro    |
                         +------------------------+
                           |          |         |
                     (I2C) |          | (SPI)   | (Analog / GPIO)
         +-----------------+          |         +-------------------+
         |                            |                             |
    SDA = D2 (GPIO4)             SCK  = D5 (GPIO14)            ADC  = A0 (Battery Sense)
    SCL = D1 (GPIO5)             MISO = D6 (GPIO12)            BtnA = D3 (GPIO0)
                                 MOSI = D7 (GPIO13)            BtnB = D4 (GPIO2)
                                 CS   = D0 (GPIO16) *
```

*\* Note: The LOLIN microSD Card Shield v1.2.0 CS pin is physically rerouted from its default `D4` pad to `D0` (GPIO16) via a solder jumper to prevent boot-strap state collisions and onboard LED interference.*

### 1.2 Hardware Table

| Component | Interface | Pins / Addresses | Notes |
| :--- | :--- | :--- | :--- |
| **MCU** | Core | LOLIN D1 mini Pro v2.0.0 | ESP8266EX, 80MHz, 80KB SRAM, 16MB Flash |
| **Sensor** | I2C | `SDA`=D2 (GPIO4), `SCL`=D1 (GPIO5)<br>Address: `0x76` (auto-fallback `0x77`) | Bosch BME280 (Temperature, Relative Humidity, Pressure) |
| **Display** | I2C | `SDA`=D2 (GPIO4), `SCL`=D1 (GPIO5)<br>Address: `0x3C` | LOLIN OLED Shield v2.0.0 (64x48 SSD1306) |
| **Button A** | Digital In | `D3` (GPIO0), `INPUT_PULLUP` | OLED Shield Button A (active LOW) |
| **Button B** | Digital In | `D4` (GPIO2), `INPUT_PULLUP` | OLED Shield Button B (active LOW) |
| **Storage** | SPI | `CS`=D0 (GPIO16), `SCK`=D5 (GPIO14),<br>`MISO`=D6 (GPIO12), `MOSI`=D7 (GPIO13) | LOLIN microSD Card Shield v1.2.0 (FAT32) |
| **Battery** | Analog | `A0` (ADC0), factor = `0.00418f` | Single-cell 3.7V LiPo via resistor divider |

---

## 2. Codebase Organization

```
TempSensor/
├── src/
│   ├── main.cpp                    # Firmware setup() and loop() entry point
│   ├── app/
│   │   ├── AppCoordinator.cpp       # Subsystem coordination, event routing & task timers
│   │   └── AppCoordinator.h
│   └── managers/
│       ├── SensorManager.cpp        # BME280 driver, auto-probe, fault handling & recovery
│       ├── SensorManager.h
│       ├── BatteryManager.cpp       # LiPo ADC burst sampling, slope regression, baseline ML
│       ├── BatteryManager.h
│       ├── DisplayManager.cpp       # SSD1306 64x48 OLED layout, tiny font, progress meters
│       ├── DisplayManager.h
│       ├── LoggerManager.cpp        # Preallocated binary storage, RAM ring buffer, SD recovery
│       ├── LoggerManager.h
│       ├── TimeManager.cpp          # Dual NTP sync, timezone offset, estimated uptime clock
│       ├── TimeManager.h
│       ├── WebManager.cpp           # HTTP REST server, chunked PROGMEM streaming, SoftAP
│       ├── WebManager.h
│       ├── DiagnosticsManager.cpp   # Periodic Serial console health logging
│       └── DiagnosticsManager.h
├── include/
│   ├── config/
│   │   └── AppConfig.h              # Unified hardware constants, default intervals & pin maps
│   ├── models/
│   │   ├── Sample.h                 # Sample, LogRecord, BatteryRecord, EventRecord structs
│   │   ├── SystemHealth.h           # System health & telemetry model
│   │   └── DeviceConfig.h           # JSON persistent configuration model (/config.json)
│   ├── utils/
│   │   └── RingBuffer.h             # Fixed-capacity lock-free circular buffer template
│   └── web/
│       ├── DashboardPage.h          # Single-Page Web Dashboard (HTML/CSS/JS in PROGMEM)
│       └── uplot_assets.h           # Compressed chart assets
├── old_docs/                        # Historical milestone briefs, wiring guides, and study notes
├── platformio.ini                   # PlatformIO project build configuration
├── README.md                        # Project high-level documentation
└── gemini.md                        # Technical reference guide (this file)
```

---

## 3. Execution Lifecycle & Coordination Flow

The project follows a centralized coordination architecture managed by `AppCoordinator`.

```
                    +-------------------+
                    |     main.cpp      |
                    +-------------------+
                              | (setup / loop)
                              v
                    +-------------------+
                    |  AppCoordinator   |
                    +-------------------+
           ___________/  |    |    |   \__________
          /             /     |     \             \
         v             v      v      v             v
    [Sensor]        [Time] [Logger] [Display]    [Web]
    Manager        Manager Manager  Manager     Manager
         \                                         /
          \______________ [Battery] ______________/
                          Manager
```

### 3.1 Boot Sequence (`AppCoordinator::begin()`)

1. **CS Pin Assertion**: Immediately sets `SD_CS_PIN` (`D0`) `HIGH` to silence SPI noise during board boot.
2. **Button Inputs**: Configures `BUTTON_A_PIN` (`D3`) and `BUTTON_B_PIN` (`D4`) as `INPUT_PULLUP`.
3. **Display Subsystem**: Initializes `DisplayManager` over I2C and draws boot splash screen.
4. **SPI & Battery**: Starts `SPI.begin()` and calls `BatteryManager::begin()`, performing ADC warm-up.
5. **Sensor Initialization**: Calls `SensorManager::begin()`. Probes `0x76`, falls back to `0x77`, and engages simulation mode if hardware is missing.
6. **SD Storage**: Mounts microSD filesystem in `LoggerManager::begin()`, initializes binary file structures (`/logs/battery.bin`, `/logs/events/`).
7. **Configuration Load**: Reads `/config.json` from SD (or populates compile-time defaults if missing).
8. **Web Server & Yields**: Boots `WebManager` HTTP server on port 80; registers yield callback so long HTTP queries still service sensor sampling.
9. **Time & Network**: Connects to Wi-Fi station; triggers initial NTP synchronization in `TimeManager`.
10. **Boot Event Logging**: Enqueues `boot`, `wifi_connected`/`wifi_disconnected`, and `ntp_synced`/`ntp_failed` events.

### 3.2 Loop Coordination (`AppCoordinator::loop()`)

- **NTP & Network Updates**:
  - Ticks `TimeManager::update()`. If NTP sync is newly established, calls `LoggerManager::calibrateEstimatedLogs()` to convert offline logs to UTC timestamps.
  - Monitors Wi-Fi status transitions. If disconnected for $>30\text{s}$, commands `WebManager::startAPFallback()` to activate `tempsensor-d1mini` SoftAP.
- **Sensor Sampling (`handleSampling`)**:
  - Runs every `config_.sampleIntervalMs` (default: 1000ms).
  - Reads BME280 values and checks for fault / recovery events.
  - Packages telemetry into a `Sample` struct and enqueues it to `LoggerManager`'s RAM `RingBuffer`.
- **Hardware Button Handling (`handleButtons`)**:
  - Checks falling edge of Button A (`D3`) and Button B (`D4`) with a 150ms debounce window.
  - Logs Category 2 (*Button A*) and Category 3 (*Button B*) binary event records.
- **Log Flushing (`LoggerManager::flushIfDue`)**:
  - Checks if `config_.logFlushIntervalMs` (default: 60,000ms) has elapsed and flushes RAM buffer to `/logs/YYYY-MM-DD.bin`.
- **SD Runtime Hot-Plug Recovery**:
  - When a recovered SD card is detected, automatically reloads settings from `/config.json` and updates `BatteryManager` baseline and `TimeManager` timezone.
- **Battery Logging**:
  - Every 60s, logs voltage, percent, state, and remaining seconds to `/logs/battery.bin`.
  - Checks if a valid discharge cycle completed; if so, writes updated `batteryRateBaseline` to `/config.json` and logs a calibration event.
- **Display & Diagnostics**:
  - Updates SSD1306 screen at `config_.displayRefreshIntervalMs`.
  - Emits Serial health diagnostics every 30 seconds via `DiagnosticsManager`.
- **OTA & Web Loop**:
  - Handles `ArduinoOTA.handle()` and `WebManager::loop()` (`server.handleClient()`).

---

## 4. Manager Subsystems Deep Dive

### 4.1 SensorManager

- **I2C Address Auto-Probe**: Probes primary `0x76`, automatically switches to `0x77` if primary fails.
- **Simulation Fallback**: If no I2C device responds on either address, activates built-in math simulation (`simulated_ = true`) so firmware remains operational for UI/web development without attached sensors.
- **Active ACK Check**: Sends an empty I2C transmission before register reads to immediately detect loose wiring or bus lockups.
- **Fault Detection**: Detects I2C NACKs (`I2cNoAck`), `NaN` float returns (`InvalidReadingNaN`), out-of-bounds metrics ($P < 300$ or $> 1200\text{ hPa}$, $T < -40$ or $> 85^\circ\text{C}$, $H < 0$ or $> 100\%$), and 3 consecutive frozen identical readings (`StuckFrozenValue`).
- **4-Step Hardware Bus Auto-Recovery**:
  1. Configures `SCL` as output and bit-bangs 16 clock cycles to release stuck `SDA` lines.
  2. Generates an explicit I2C STOP condition.
  3. Re-initializes `Wire.begin(sda, scl)`.
  4. Writes Power-On Reset command `0xB6` to BME280 register `0xE0`, delays 100ms, and re-initializes `Adafruit_BME280`.
- **Outage Logging**: Captures fault start timestamps and emits recovery log records stating the exact outage duration in seconds.

### 4.2 BatteryManager

- **ADC Burst Averaging**: Averages 8 consecutive ADC readings with 2ms delays to filter RF transmission spikes from the ESP8266 Wi-Fi radio.
- **Exponential Moving Average (EMA)**: Applies $V_{ema} = 0.85 \times V_{ema} + 0.15 \times V_{raw}$ filtering.
- **Piecewise Linear LiPo Model**:
  - $V \ge 4.15\text{V} \rightarrow 100\%$
  - $4.00\text{V} \le V < 4.15\text{V} \rightarrow 80\% - 100\%$
  - $3.82\text{V} \le V < 4.00\text{V} \rightarrow 50\% - 80\%$
  - $3.70\text{V} \le V < 3.82\text{V} \rightarrow 15\% - 50\%$
  - $3.40\text{V} \le V < 3.70\text{V} \rightarrow 0\% - 15\%$
  - $V < 3.40\text{V} \rightarrow 0\%$
- **Rolling Linear Regression Slope**: Maintains a 10-minute historical circular buffer to compute slope $\frac{dV}{dt}$ in volts/minute:
  $$\text{Slope} = \frac{N \sum (xy) - \sum x \sum y}{N \sum (x^2) - (\sum x)^2}$$
- **State Machine Transitions**:
  - Fast step transition for instant $\pm 30\text{mV}$ step changes (charger plugged / unplugged).
  - Debounced slope transition: Slope $\ge +1.5\text{mV/min}$ confirms `Charging / USB`; Slope $\le -1.0\text{mV/min}$ confirms `Discharging`.
  - Voltage threshold overrides: $V \ge 4.12\text{V} \ \& \ \ge 99\%$ triggers `Full`; $V < 4.05\text{V} \ \& \ < 95\%$ drops to `Discharging`.
- **Adaptive Discharge Rate Learning**:
  - Tracks valid discharge windows between $\le 90\%$ and recharge.
  - Requires at least a $10\%$ drop over $\ge 1\text{ hour}$ ($3600\text{s}$) to qualify as a valid run.
  - Updates smoothed rate: $\text{Rate}_{new} = 0.70 \times \text{Rate}_{old} + 0.30 \times \text{Rate}_{measured}$, clamped between $800\text{ s/\%}$ (22h) and $1500\text{ s/\%}$ (41.6h).
  - Triggers automatic persistence to `/config.json` and logs a calibration event.

### 4.3 LoggerManager

- **In-Memory Queue**: `RingBuffer<Sample, 128>` buffers readings in RAM to isolate high-frequency sampling from SPI write latency.
- **Preallocated Daily Binary Storage (`/logs/YYYY-MM-DD.bin`)**:
  - File size: Exactly $86,400 \times 17\text{ bytes} = 1,468,800\text{ bytes}$.
  - Slot Index: $\text{slot} = \text{hour} \times 3600 + \text{min} \times 60 + \text{sec}$.
  - Direct seek write: `file.seek(slotIndex * sizeof(LogRecord))`.
  - Direct range reading: Reads arbitrary time slices in constant $O(1)$ seek time without reading or parsing unused data.
- **Offline Buffer (`/logs/estimated.bin`)**:
  - Stores samples collected prior to NTP sync with uptime seconds.
  - When NTP synchronizes, `calibrateEstimatedLogs()` reads `estimated.bin`, computes UTC timestamps using the calculated boot epoch, writes records into corresponding daily `.bin` files, and removes `estimated.bin`.
- **Battery Binary Ledger (`/logs/battery.bin`)**:
  - Stores packed 13-byte `BatteryRecord` entries appended every 60 seconds.
- **Event Storage (`/logs/events/ev_YYYYMMDD_XXX.bin`)**:
  - Preallocated 1,000 slots ($1,000 \times 136\text{ bytes} = 136\text{ KB}$ per chunk file).
  - Direct in-place updates and deletions by seeking to `slotIndex * sizeof(EventRecord)`.
  - Automatic migration from legacy `events.csv` on startup.
- **SD Hot-Plug Recovery**: Periodically tests SD presence with exponential backoff ($15\text{s} \to 300\text{s}$), re-initializing SPI and recovering filesystem upon card re-insertion.

### 4.4 DisplayManager

- **OLED Driver**: Uses `Adafruit_SSD1306` mapped to 64x48 resolution.
- **Startup Screens**: Sequential stage indicators (`Boot`, `Sensor`, `SD`, `WiFi`, `NTP`, `Startup`) with error flags.
- **Live Screen Layout**:
  - Line 1: `T: 23.5C`
  - Line 2: `H: 48.2%`
  - Line 3: `P:1013hPa`
  - Line 4: `B:85% Dis` (or `Chg`, `Ful`, `Unk`)
  - Line 5: 3-second alternating toggle between Network/SD status (`W+|S+|N+`) and live time (`14:23:05` / `syncing`).
  - Bottom row: Tiny 4x5 custom bitmapped font rendering the local IP address (`192.168.1.50`).
  - Right edge ($X=63$): 1-pixel vertical bar visually indicating RAM queue fill level ($0 - 48\text{px}$).
- **OTA Progress Screen**: Live percentage readout and graphical progress bar during firmware flashing.

### 4.5 WebManager & Dashboard

- **Chunked Flash Streaming**: Serves the single-page HTML dashboard (`DashboardPage.h`) in 1024-byte chunks from `PROGMEM` to eliminate SRAM heap allocations.
- **Dygraphs Frontend**: High-performance canvas charting with multi-axis support, synchronized crosshairs, event overlay markers, and dynamic data binning.
- **Downsampling & Range Filtering**: Binary reader extracts requested time slices and aggregates samples into uniform bins to conserve network bandwidth and ESP8266 memory.
- **SoftAP Failover**: Automatically boots an access point named `tempsensor-d1mini` (IP `192.168.4.1`) if station Wi-Fi fails to connect for $>30$ seconds.

---

## 5. Packed Data Structures & Memory Models

All binary storage structures use `__attribute__((packed))` to enforce strict byte alignments across builds.

### 5.1 `LogRecord` (17 Bytes)

```cpp
struct __attribute__((packed)) LogRecord {
  uint32_t uptimeSeconds;  // 4 bytes: Monotonic system uptime
  float    temperatureC;   // 4 bytes: IEEE 754 float
  float    humidityPct;    // 4 bytes: IEEE 754 float
  float    pressureHpa;    // 4 bytes: IEEE 754 float
  uint8_t  quality;        // 1 byte:  0 = NTP, 1 = Estimated, 2 = Empty/Invalid
};
```

### 5.2 `BatteryRecord` (13 Bytes)

```cpp
struct __attribute__((packed)) BatteryRecord {
  uint32_t epochTime;       // 4 bytes: Unix timestamp (seconds)
  float    voltage;         // 4 bytes: Battery voltage (e.g. 3.85V)
  uint8_t  percent;         // 1 byte:  0 - 100%
  uint8_t  chargingState;   // 1 byte:  0 = Unknown, 1 = Discharging, 2 = Charging, 3 = Full
  int32_t  timeRemainingS;  // 4 bytes: Estimated remaining seconds (-1 = unknown)
};
```

### 5.3 `EventRecord` (136 Bytes)

```cpp
enum class EventCategory : uint8_t {
  System = 0,
  CustomWeb = 1,
  ButtonA = 2,
  ButtonB = 3,
  SensorFault = 4,
  NetworkSync = 5,
  PowerBattery = 6
};

struct __attribute__((packed)) EventRecord {
  uint32_t epochTime;      // 4 bytes:   Unix timestamp (seconds)
  uint8_t  quality;        // 1 byte:    0 = NTP, 1 = Estimated, 2 = Empty/Invalid
  uint8_t  category;       // 1 byte:    EventCategory enum
  char     message[128];   // 128 bytes: Null-terminated UTF-8 event text
  uint8_t  reserved[2];    // 2 bytes:   Alignment padding (total 136 bytes, divisible by 4)
};
```

### 5.4 `DeviceConfig`

```cpp
struct DeviceConfig {
  char     wifiSsid[32];
  char     wifiPassword[64];
  char     hostname[32];
  char     timezone[64];
  uint32_t sampleIntervalMs;
  uint32_t logFlushIntervalMs;
  uint32_t displayRefreshIntervalMs;
  float    latitude;
  float    longitude;
  float    batteryRateBaseline;  // Evaluated seconds per 1% drop
};
```

---

## 6. REST API Reference

### 6.1 Telemetry & State

#### `GET /api/live`
Returns the latest instantaneous sensor reading.
```json
{
  "timestamp": "2026-08-25 03:45:00",
  "timestamp_quality": "ntp",
  "temp_c": 21.45,
  "humidity_pct": 55.20,
  "pressure_hpa": 1014.10,
  "uptime_s": 3600,
  "has_sample": true
}
```

#### `GET /api/health`
Returns system diagnostics, memory metrics, and battery state.
```json
{
  "uptime_s": 3600,
  "free_heap_bytes": 42150,
  "largest_free_block_bytes": 38400,
  "log_queue_depth": 12,
  "log_queue_capacity": 128,
  "dropped_log_samples": 0,
  "wifi_connected": true,
  "sd_healthy": true,
  "ntp_synced": true,
  "sensor_healthy": true,
  "sensor_simulated": false,
  "sensor_status": "REAL sensor active",
  "battery": {
    "voltage": 3.92,
    "percent": 68,
    "status": "Discharging",
    "time_remaining": 68000,
    "slope": -0.00085
  },
  "has_health": true
}
```

### 6.2 Configuration

#### `GET /api/config`
Returns active configuration settings (password masked).

#### `POST /api/config`
Applies new configuration settings and saves to `/config.json`.
- **Payload**: JSON containing fields to update (`wifi_ssid`, `wifi_password`, `hostname`, `timezone`, `sample_interval_ms`, `log_flush_interval_ms`, `display_refresh_interval_ms`, `latitude`, `longitude`).
- **Response**: `{"accepted": true, "reboot": true/false, "message": "..."}`. Reboots if network credentials change.

### 6.3 Historical Sensor & Battery Data

#### `GET /api/history`
Streams historical binary data downsampled into JSON.
- **Query Parameters**:
  - `metric`: `temp_c` | `humidity_pct` | `pressure_hpa` | `battery` (default: `temp_c`)
  - `start`: Start timestamp (`YYYY-MM-DD HH:MM:SS` or `YYYY-MM-DD`)
  - `end`: End timestamp (`YYYY-MM-DD HH:MM:SS` or `YYYY-MM-DD`)
  - `max_points`: Maximum number of data points to return (default: 800)
  - `bin`: Optional explicit bin interval in seconds

### 6.4 Event Management

#### `GET /api/events`
Returns parsed event records across binary chunk files.
- **Query Parameters**: `limit` (default: 100), `start`, `end`.

#### `POST /api/events/create`
Creates a new event record.
- **Payload**: `{"event": "Custom Event", "ts": "2026-08-25 03:00:00", "category": 1}`. Supports backdating.

#### `POST /api/events/update`
Updates an existing event record in-place ($O(1)$ write).
- **Payload**: `{"file": "ev_20260825_001.bin", "index": 42, "event": "Updated Text", "category": 1}`.

#### `POST /api/events/delete`
Marks an event record slot as deleted/invalid (`quality = 2`).
- **Payload**: `{"file": "ev_20260825_001.bin", "index": 42}`.

### 6.5 File Management & Actions

#### `GET /api/logs`
Returns JSON array of all files on SD with byte sizes.

#### `GET /api/logs/download?file=<path>`
Streams a file download from SD card.

#### `POST /api/logs/delete`
Deletes a file on SD (`file=<path>`).

#### `POST /api/logs/rename`
Renames a file on SD (`old=<path>&new=<path>`).

#### `GET /api/sd-tree`
Returns complete hierarchical directory tree of the SD card.

#### `POST /api/action/flush-now`
Forces an immediate write flush of the RAM sample queue to the SD card.

#### `POST /api/action/ntp-retry`
Resets the NTP backoff timer and forces an immediate sync attempt.

#### `POST /api/update`
Receives multipart binary firmware payload for Web OTA flashing.

---

## 7. SD Card Filesystem Hierarchy

```
SD Card Root/
├── config.json                     # Persistent runtime configuration
└── logs/
    ├── YYYY-MM-DD.bin              # Daily preallocated sensor logs (86,400 * 17B = 1.46MB)
    ├── estimated.bin               # Pre-NTP offline sample buffer
    ├── battery.bin                 # Continuous battery telemetry (13B per entry)
    └── events/
        ├── ev_YYYYMMDD_001.bin     # Preallocated event chunk 1 (1,000 * 136B = 136KB)
        └── ev_YYYYMMDD_002.bin     # Preallocated event chunk 2
```

---

## 8. Development Constraints & Best Practices

1. **ESP8266 Memory Limits**: Total available SRAM is ~80KB, with ~40KB free for user code.
   - Never build large in-memory JSON structures on heap. Use fixed-size `StaticJsonDocument` or chunked streaming.
   - Large web assets must reside in Flash `PROGMEM` (`const char DASHBOARD_HTML[] PROGMEM`) and stream in $\le 1024$-byte buffers.
2. **Watchdog Timer (WDT) & Yields**:
   - Long SPI preallocation loops or network reads must call `yield()` periodically to feed the hardware and software WDTs and maintain Wi-Fi connectivity.
3. **SPI & I2C Bus Sharing**:
   - `DisplayManager` and `SensorManager` share the `Wire` I2C peripheral on `D1`/`D2`.
   - `LoggerManager` operates the hardware SPI peripheral (`D5`, `D6`, `D7`, `D0`).
   - SD CS pin must stay asserted `HIGH` during non-SD operations to prevent SPI bus contention.
