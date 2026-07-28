#pragma once

#include <Arduino.h>

namespace AppConfig {

constexpr uint32_t SAMPLE_INTERVAL_MS = 1000;
constexpr uint32_t LOG_FLUSH_INTERVAL_MS = 60000;
constexpr uint32_t DISPLAY_REFRESH_INTERVAL_MS = SAMPLE_INTERVAL_MS;
constexpr uint32_t DIAGNOSTICS_INTERVAL_MS = 30000;
constexpr uint32_t NTP_RETRY_INTERVAL_MS = 30000;

constexpr int BATTERY_ADC_PIN = A0;
constexpr float BATTERY_CALIBRATION_FACTOR = 0.00418f;
constexpr uint32_t BATTERY_READ_INTERVAL_MS = 5000;

constexpr size_t MAX_LOG_QUEUE_SIZE = 128;

constexpr char WIFI_SSID[] = "ImWifiRick";
constexpr char WIFI_PASSWORD[] = "1234567890";
constexpr char HOSTNAME[] = "tempsensor-d1mini";

constexpr char NTP_SERVER_1[] = "pool.ntp.org";
constexpr char NTP_SERVER_2[] = "time.nist.gov";
constexpr char TIMEZONE_MELBOURNE[] = "AEST-10AEDT,M10.1.0,M4.1.0/3";

constexpr uint8_t BME280_I2C_ADDR = 0x76;
constexpr int I2C_SDA_PIN = 4;   // D2
constexpr int I2C_SCL_PIN = 5;   // D1

constexpr int SD_CS_PIN = 16;    // D0 (GPIO16 - shield CS rerouted from D4 -> D0)
constexpr int SD_SCK_PIN = 14;   // D5
constexpr int SD_MOSI_PIN = 13;  // D7
constexpr int SD_MISO_PIN = 12;  // D6

constexpr int BUTTON_A_PIN = 0; // D3 / GPIO0 (OLED Shield Button A)
constexpr int BUTTON_B_PIN = 2; // D4 / GPIO2 (OLED Shield Button B)
constexpr uint32_t BUTTON_DEBOUNCE_MS = 150;

constexpr uint16_t DISPLAY_WIDTH = 64;
constexpr uint16_t DISPLAY_HEIGHT = 48;
constexpr bool DISPLAY_SHOW_DIMENSION_PROBE_ON_BOOT = false;
constexpr uint16_t DISPLAY_DIMENSION_PROBE_MS = 2500;

constexpr char LOG_FILE_PATH[] = "/logs/data.csv";
constexpr char EVENT_FILE_PATH[] = "/logs/events.csv";
constexpr char EVENT_DIR_PATH[] = "/logs/events";
constexpr size_t EVENT_RECORDS_PER_FILE = 1000;

}  // namespace AppConfig
