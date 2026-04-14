// config.h — Build-time configuration constants
#pragma once

// --- Pin assignments (M5Stack Basic v2.7 + GPS Module v2.1) ---
#define GPS_TX_PIN 17
#define GPS_RX_PIN 16
#define GPS_PPS_PIN 36
#define GPS_BAUD 9600

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
