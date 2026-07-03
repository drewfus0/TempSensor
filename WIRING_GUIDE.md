# TempSensor Wiring Guide
**Board:** LILYGO T5 Screen 4.7" S3 v2.3 (ESP32-S3)  
**Sensor:** TS1208P-BME280-3.3V (I2C)  
**Storage:** Built-in microSD slot (SPI)  
**Display:** Built-in 4.7" e-ink (SPI)

---

## Pin Summary

| Component | Signal | Pin | GPIO | Notes |
|-----------|--------|-----|------|-------|
| **BME280 (I2C Bus 1)** | VCC | 3.3V | — | Power |
| | GND | GND | — | Ground |
| | SDA | 18 | GPIO18 | I2C Data (T5 default) |
| | SCL | 17 | GPIO17 | I2C Clock (T5 default) |
| | SDO | — | — | Leave floating (optional) → GND for 0x76 addr |
| **SD Card (Built-in SPI)** | CS | 10 | GPIO10 | Chip Select |
| | MOSI | 11 | GPIO11 | SPI Data Out |
| | MISO | 13 | GPIO13 | SPI Data In |
| | SCK | 12 | GPIO12 | SPI Clock |
| | VCC | 3.3V | — | Power |
| | GND | GND | — | Ground |
| **E-ink Display (Built-in SPI)** | CS | 7 | GPIO7 | Chip Select |
| | DC | 6 | GPIO6 | Data/Command |
| | RST | 5 | GPIO5 | Reset |
| | BUSY | 4 | GPIO4 | Busy Status |
| | MOSI | 11 | GPIO11 | SPI Data Out (shared) |
| | SCK | 12 | GPIO12 | SPI Clock (shared) |

---

## Current Code Configuration

**File:** `include/config/AppConfig.h`

```cpp
// BME280 - I2C Bus 1
constexpr uint8_t BME280_I2C_ADDR = 0x76;  // Check SDO pin on your module
constexpr int I2C_SDA_PIN = 18;
constexpr int I2C_SCL_PIN = 17;

// SD Card - Built-in SPI
constexpr int SD_CS_PIN = 10;
constexpr int SD_SCK_PIN = 12;
constexpr int SD_MOSI_PIN = 11;
constexpr int SD_MISO_PIN = 13;

// E-ink Display - Built-in SPI (shares SCK/MOSI)
constexpr int EINK_CS_PIN = 7;
constexpr int EINK_DC_PIN = 6;
constexpr int EINK_RST_PIN = 5;
constexpr int EINK_BUSY_PIN = 4;
constexpr int EINK_SCK_PIN = 12;
constexpr int EINK_MOSI_PIN = 11;
```

✅ **Status:** Config matches standard LILYGO T5 4.7" S3 pinout

---

## LILYGO T5 4.7" S3 Power Pins Location

**Board Orientation:** USB port on RIGHT side

### Left Edge Connector (Most Common) ⭐
Looking at the board with USB on right:
- **Top-left corner** area has power pins
- Scan the silkscreen text for: `3V3`, `VCC`, or `GND`
- Usually in a vertical row: GND, 3V3, or similar

### Bottom Edge (Alternative)
If left edge is unclear:
- Bottom-left corner may have pads labeled `3V3` and `GND`
- Check PCB silkscreen carefully

### Quick Find Method
1. **Look for silkscreen labels** on the PCB edge:
   - `3V3` or `VCC` = **3.3V output** ✓ Use this
   - `GND` = Ground (also needed)
   
2. **Pin spacing:** Usually 2-3mm holes in a row
   - Power and GND pins are typically **adjacent** or very close
   - Look for male pins or test pads

3. **Can't find labels? Use multimeter:**
   - Set multimeter to **20V DC**
   - Probe each edge pin against a known **GND** point
   - Should read **~3.3V** on the power pin
   - Document with a photo

---

## Wiring Steps

### 1. BME280 Sensor (I2C Connection)
**Connections:**
- **VCC** → T5 3.3V pin
- **GND** → T5 GND pin
- **SDA** → T5 GPIO18 (labeled as I2C_SDA or similar)
- **SCL** → T5 GPIO17 (labeled as I2C_SCL or similar)
- **SDO** → Leave floating (default 0x76) OR tie to GND if needed

**Why I2C?** 
- Only 2 data wires needed
- Shares no pins with SD or e-ink (both on SPI)
- BME280 has built-in pullups, no extra resistors needed

### 2. microSD Card (Already Soldered)
**Status:** Built-in to T5 board
- No external wiring needed
- SPI interface uses GPIO10 (CS), GPIO11 (MOSI), GPIO12 (SCK), GPIO13 (MISO)
- Share SCK/MOSI with e-ink display

### 3. E-ink Display (Already Soldered)
**Status:** Built-in to T5 board
- No external wiring needed
- SPI interface uses GPIO7 (CS), GPIO6 (DC), GPIO5 (RST), GPIO4 (BUSY)
- Share SCK/MOSI with SD card

---

## Verification Checklist

Before uploading firmware:

- [ ] **BME280 I2C address:** Check SDO pin state
  - Floating or tied HIGH → `0x77`
  - Tied LOW → `0x76` (default in config)
  
- [ ] **Power:** All 3.3V signals verified with multimeter
  
- [ ] **SDA/SCL:** Continuity check on GPIO18/GPIO17 to BME280 pads
  
- [ ] **No shorts:** Between adjacent pads or to GND/VCC

---

## Troubleshooting

If build works but sensor doesn't respond:

1. **Serial output:** Check boot messages for `[Sensor] BME280 ready`
   - If not ready → I2C address or pin mismatch
   
2. **Check I2C address:** Modify config if `0x76` doesn't work
   ```cpp
   constexpr uint8_t BME280_I2C_ADDR = 0x77;  // Try alternate
   ```
   
3. **Verify pins:** Use serial debug to confirm GPIO state
   ```cpp
   pinMode(18, INPUT);  // Check SDA
   pinMode(17, INPUT);  // Check SCL
   ```

---

## References
- **LILYGO T5 4.7" S3:** Standard ESP32-S3 pinout
- **BME280 Datasheet:** I2C address selected by SDO pin
- **GxEPD2 Display Driver:** Supports shared SPI bus
