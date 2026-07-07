# TempSensor Wiring Guide (LOLIN D1 Mini Pro Stack)
**Board:** LOLIN D1 mini Pro v2.0.0 (ESP8266)  
**Sensor:** BME280 (I2C)  
**Storage:** LOLIN microSD Card Shield v1.2.0 (SPI)  
**Display:** LOLIN OLED Shield v2.0.0 (I2C)

---

## Pin Summary

| Bus | Function | D1 Mini Pin | ESP8266 GPIO | Used By |
|-----|----------|-------------|--------------|---------|
| I2C | SCL | D1 | GPIO5 | OLED Shield + BME280 |
| I2C | SDA | D2 | GPIO4 | OLED Shield + BME280 |
| SPI | SCK | D5 | GPIO14 | microSD Shield |
| SPI | MISO | D6 | GPIO12 | microSD Shield |
| SPI | MOSI | D7 | GPIO13 | microSD Shield |
| SPI | CS/SS | D0 | GPIO16 | microSD Shield (D4 CS rerouted to D0) |
| Power | 3V3 | 3V3 | — | OLED + BME280 + microSD Shield |
| Power | Ground | G | — | OLED + BME280 + microSD Shield |

---

## Module Wiring

### 1. OLED Shield v2.0.0
If stacked directly on the D1 mini, no jumper wires are needed.

- **SCL** -> D1 (GPIO5)
- **SDA** -> D2 (GPIO4)
- **VCC** -> 3V3
- **GND** -> G

Default OLED I2C address is usually `0x3C`.

### 2. BME280 (still on D1/D2)
The BME280 shares the same I2C bus as the OLED.

- **VCC** -> 3V3
- **GND** -> G
- **SCL** -> D1 (GPIO5)
- **SDA** -> D2 (GPIO4)
- **SDO** -> GND for `0x76`, or leave/high for `0x77`

### 3. microSD Card Shield v1.2.0
If stacked directly on the D1 mini, SPI lines are already routed.

- **CS (shield pad)** -> D4 (GPIO0) *(shield default pad)*
- **Jumper**: Shield CS (D4 pad) -> D1 mini D0 (GPIO16)
- **SCK** -> D5 (GPIO14)
- **MISO** -> D6 (GPIO12)
- **MOSI** -> D7 (GPIO13)
- **VCC** -> 3V3
- **GND** -> G

Note: LOLIN microSD Shield v1.2.0 defaults CS to D4 (GPIO2), but this build uses a jumper reroute to D0 (GPIO16) to avoid boot issues.

---

## Breadboard Layout Notes

- Place the D1 mini so each header row is on opposite sides of the breadboard center gap.
- Run one 3.3V rail and one GND rail down the breadboard for clean power distribution.
- If the OLED and microSD shields are stacked, only the BME280 needs jumper wires.
- If not stacked, keep I2C wires short and route SPI wires away from power rails to reduce noise.

---

## Recommended Firmware Pin Config

If you are updating firmware config for this hardware:

```cpp
constexpr uint8_t BME280_I2C_ADDR = 0x76;
constexpr int I2C_SDA_PIN = 4;   // D2
constexpr int I2C_SCL_PIN = 5;   // D1

constexpr int SD_CS_PIN = 16;    // D0 (shield D4 CS rerouted to D0)
constexpr int SD_SCK_PIN = 14;   // D5
constexpr int SD_MOSI_PIN = 13;  // D7
constexpr int SD_MISO_PIN = 12;  // D6
```

---

## Verification Checklist

- [ ] Board powered from stable USB source
- [ ] 3.3V present on OLED/BME280/microSD modules
- [ ] I2C scan shows OLED (`0x3C`) and BME280 (`0x76` or `0x77`)
- [ ] SD card initializes on CS pin D8 (with D4->D8 jumper)
- [ ] No jumper shorts across breadboard rows

---

## Quick Troubleshooting

1. If OLED is blank, verify I2C address (`0x3C` vs `0x3D`) and power.
2. If BME280 is not found, swap between `0x76` and `0x77` based on SDO wiring.
3. If SD init fails, confirm shield CS (D4 pad) is actually jumpered to D0 and card is FAT32 formatted.
