// config_manager.h — Read runtime config from TF card (JSON)
#pragma once

#include <Arduino.h>
#include <SD.h>
#include <ArduinoJson.h>
#include "config.h"

struct RuntimeConfig {
  char wifiSSID[64];
  char wifiPassword[64];
  char endpoint[256];     // data upload URL
  char apiKey[128];       // API key for endpoint authentication
  bool useMockGPS;
  bool useMockIMU;
  bool useMockSender;
};

class ConfigManager {
public:
  static bool load(RuntimeConfig& cfg) {
    // Set defaults
    memset(&cfg, 0, sizeof(cfg));
    cfg.useMockGPS = false;
    cfg.useMockIMU = true;     // IMU not yet available
    cfg.useMockSender = true;  // cloud not yet ready
    strncpy(cfg.endpoint, "http://example.com/api/data", sizeof(cfg.endpoint) - 1);
    cfg.apiKey[0] = '\0';

    // SD is already initialized by M5.begin() in M5Unified
    File file = SD.open(CONFIG_FILE_PATH, FILE_READ);
    if (!file) {
      Serial.printf("[Config] %s not found, using defaults\n", CONFIG_FILE_PATH);
      return false;
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, file);
    file.close();

    if (err) {
      Serial.printf("[Config] JSON parse error: %s\n", err.c_str());
      return false;
    }

    // Wi-Fi
    if (doc["wifi"]["ssid"].is<const char*>()) {
      strncpy(cfg.wifiSSID, doc["wifi"]["ssid"], sizeof(cfg.wifiSSID) - 1);
    }
    if (doc["wifi"]["password"].is<const char*>()) {
      strncpy(cfg.wifiPassword, doc["wifi"]["password"], sizeof(cfg.wifiPassword) - 1);
    }

    // Endpoint & API key
    if (doc["endpoint"].is<const char*>()) {
      strncpy(cfg.endpoint, doc["endpoint"], sizeof(cfg.endpoint) - 1);
    }
    if (doc["api_key"].is<const char*>()) {
      strncpy(cfg.apiKey, doc["api_key"], sizeof(cfg.apiKey) - 1);
    }

    // Mock flags
    if (doc["mock"]["gps"].is<bool>()) cfg.useMockGPS = doc["mock"]["gps"];
    if (doc["mock"]["imu"].is<bool>()) cfg.useMockIMU = doc["mock"]["imu"];
    if (doc["mock"]["sender"].is<bool>()) cfg.useMockSender = doc["mock"]["sender"];

    Serial.printf("[Config] loaded: SSID=%s mock_gps=%d mock_imu=%d mock_sender=%d\n",
                  cfg.wifiSSID, cfg.useMockGPS, cfg.useMockIMU, cfg.useMockSender);
    return true;
  }
};
