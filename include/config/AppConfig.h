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

constexpr char WIFI_SSID[] = "YOUR_WIFI_SSID";
constexpr char WIFI_PASSWORD[] = "YOUR_WIFI_PASSWORD";
constexpr char HOSTNAME[] = "tempsensor-t5";

constexpr char NTP_SERVER_1[] = "pool.ntp.org";
constexpr char NTP_SERVER_2[] = "time.nist.gov";
constexpr long GMT_OFFSET_SECONDS = 0;
constexpr int DST_OFFSET_SECONDS = 0;

constexpr uint8_t BME280_I2C_ADDR = 0x76;
constexpr int I2C_SDA_PIN = 18;
constexpr int I2C_SCL_PIN = 17;

constexpr int SD_CS_PIN = 10;
constexpr int SD_SCK_PIN = 12;
constexpr int SD_MOSI_PIN = 11;
constexpr int SD_MISO_PIN = 13;

constexpr int EINK_CS_PIN = 7;
constexpr int EINK_DC_PIN = 6;
constexpr int EINK_RST_PIN = 5;
constexpr int EINK_BUSY_PIN = 4;
constexpr int EINK_SCK_PIN = 12;
constexpr int EINK_MOSI_PIN = 11;

constexpr uint16_t DISPLAY_WIDTH = 960;
constexpr uint16_t DISPLAY_HEIGHT = 540;

constexpr char LOG_FILE_PATH[] = "/logs/data.csv";
constexpr char EVENT_FILE_PATH[] = "/logs/events.csv";

}  // namespace AppConfig
