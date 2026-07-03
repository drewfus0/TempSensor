#pragma once

#include <Arduino.h>

namespace AppConfig {

constexpr uint32_t SAMPLE_INTERVAL_MS = 1000;
constexpr uint32_t LOG_FLUSH_INTERVAL_MS = 60000;
constexpr uint32_t DISPLAY_REFRESH_INTERVAL_MS = 30UL * 60UL * 1000UL;
constexpr uint32_t DIAGNOSTICS_INTERVAL_MS = 30000;
constexpr uint32_t NTP_RETRY_INTERVAL_MS = 30000;

constexpr size_t MAX_RING_BUFFER_SIZE = 1800;
constexpr size_t MAX_LOG_QUEUE_SIZE = 512;

constexpr char WIFI_SSID[] = "ImWifiRick";
constexpr char WIFI_PASSWORD[] = "1234567890";
constexpr char HOSTNAME[] = "tempsensor-d1mini";

constexpr char NTP_SERVER_1[] = "pool.ntp.org";
constexpr char NTP_SERVER_2[] = "time.nist.gov";
constexpr long GMT_OFFSET_SECONDS = 0;
constexpr int DST_OFFSET_SECONDS = 0;

constexpr uint8_t BME280_I2C_ADDR = 0x76;
constexpr int I2C_SDA_PIN = 4;   // D2
constexpr int I2C_SCL_PIN = 5;   // D1

constexpr int SD_CS_PIN = 15;    // D8
constexpr int SD_SCK_PIN = 14;   // D5
constexpr int SD_MOSI_PIN = 13;  // D7
constexpr int SD_MISO_PIN = 12;  // D6

constexpr int BUTTON_PIN = 21;

constexpr uint16_t DISPLAY_WIDTH = 128;
constexpr uint16_t DISPLAY_HEIGHT = 64;

constexpr char LOG_FILE_PATH[] = "/logs/data.csv";
constexpr char EVENT_FILE_PATH[] = "/logs/events.csv";

}  // namespace AppConfig
