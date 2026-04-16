// network_manager.h — Wi-Fi connection + data sender interface
#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include "types.h"
#include "config.h"

// --- DataSender interface ---
class DataSender {
public:
  virtual ~DataSender() {}
  virtual bool send(const SensorRecord* records, int count) = 0;
};

// --- Mock sender: prints JSON to Serial ---
class MockSender : public DataSender {
public:
  MockSender(const char* endpoint = "", const char* apiKey = "")
    : _endpoint(endpoint), _apiKey(apiKey) {}

  bool send(const SensorRecord* records, int count) override {
    Serial.printf("[MockSender] POST %s (key=%s) %d records:\n",
                  _endpoint, strlen(_apiKey) > 0 ? "***" : "none", count);
    for (int i = 0; i < count; i++) {
      const SensorRecord& r = records[i];
      Serial.printf("  [%s] lat=%.6f lng=%.6f spd=%.1f sat=%d imu_n=%d %.1fC %.1fhPa\n",
                     r.gps.timestamp, r.gps.latitude, r.gps.longitude,
                     r.gps.speed, r.gps.satellites, r.imuCount,
                     r.env.temperature, r.env.pressure);
    }
    return true;
  }

private:
  const char* _endpoint;
  const char* _apiKey;
};

// --- HTTP sender (placeholder for cloud integration) ---
// #include <HTTPClient.h>
// class HTTPSender : public DataSender {
// public:
//   HTTPSender(const char* endpoint, const char* apiKey)
//     : _endpoint(endpoint), _apiKey(apiKey) {}
//   bool send(const SensorRecord* records, int count) override {
//     HTTPClient http;
//     http.begin(_endpoint);
//     http.addHeader("Content-Type", "application/json");
//     if (strlen(_apiKey) > 0) {
//       http.addHeader("X-API-Key", _apiKey);
//     }
//     String json = serializeRecords(records, count);
//     int code = http.POST(json);
//     http.end();
//     return (code >= 200 && code < 300);
//   }
// private:
//   const char* _endpoint;
//   const char* _apiKey;
// };

// --- Wi-Fi connection manager with exponential backoff ---
class WifiManager {
public:
  WifiManager() : _sender(nullptr), _connected(false),
                     _retryMs(WIFI_RETRY_BASE_MS), _lastAttemptMs(0) {}

  void setSender(DataSender* sender) { _sender = sender; }

  void begin(const char* ssid, const char* password) {
    strncpy(_ssid, ssid, sizeof(_ssid) - 1);
    strncpy(_password, password, sizeof(_password) - 1);

    if (strlen(_ssid) == 0) {
      Serial.println("[Network] no SSID configured, skipping Wi-Fi");
      return;
    }

    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    _tryConnect();
  }

  void update() {
    if (strlen(_ssid) == 0) return;

    if (WiFi.status() == WL_CONNECTED) {
      if (!_connected) {
        _connected = true;
        _retryMs = WIFI_RETRY_BASE_MS;
        Serial.printf("[Network] connected: %s\n", WiFi.localIP().toString().c_str());
      }
    } else {
      if (_connected) {
        _connected = false;
        Serial.println("[Network] disconnected");
      }
      // Retry with backoff
      unsigned long now = millis();
      if (now - _lastAttemptMs >= _retryMs) {
        _tryConnect();
        // Exponential backoff with cap
        _retryMs = min(_retryMs * 2, (unsigned long)WIFI_RETRY_MAX_MS);
      }
    }
  }

  bool isConnected() const { return _connected; }

  bool sendData(const SensorRecord* records, int count) {
    if (!_sender) return false;
    if (!_connected) return false;
    return _sender->send(records, count);
  }

  const char* ssid() const { return _ssid; }

private:
  void _tryConnect() {
    Serial.printf("[Network] connecting to %s ...\n", _ssid);
    WiFi.begin(_ssid, _password);
    _lastAttemptMs = millis();
  }

  DataSender* _sender;
  bool _connected;
  char _ssid[64];
  char _password[64];
  unsigned long _retryMs;
  unsigned long _lastAttemptMs;
};
