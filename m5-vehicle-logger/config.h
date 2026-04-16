// config.h — Build-time configuration constants
#pragma once

// --- Pin assignments (M5Stack Basic v2.7 + GNSS Module M135) ---
// DIP switch: GPS TXD=G17, GPS RXD=G16
// ESP32 Serial2: RX pin reads GPS TXD, TX pin writes GPS RXD
#define GPS_RX_PIN 16
#define GPS_TX_PIN 17
#define GPS_PPS_PIN 36
#define GPS_BAUD 38400  // NEO-M9N default baud rate

// --- I2C addresses (GNSS Module M135) ---
#define BMI270_I2C_ADDR 0x68
#define BMM150_I2C_ADDR 0x10  // connected via BMI270 secondary I2C
#define BMP280_I2C_ADDR 0x76

// --- Sampling rates ---
#define GPS_SAMPLE_INTERVAL_MS 1000   // 1 Hz
#define IMU_SAMPLE_INTERVAL_MS 100    // 10 Hz
#define IMU_SAMPLES_PER_RECORD 10     // samples per GPS epoch

// --- Data buffer ---
#define DATA_BUFFER_SIZE 120          // ring buffer capacity (seconds of data)

// --- Network ---
#define WIFI_CONNECT_TIMEOUT_MS 10000
#define WIFI_RETRY_BASE_MS 5000
#define WIFI_RETRY_MAX_MS 60000
#define SEND_BATCH_SIZE 10            // records per HTTP POST

// --- TF Card config file ---
#define CONFIG_FILE_PATH "/config.json"

// --- Display ---
#define DISPLAY_UPDATE_INTERVAL_MS 500
