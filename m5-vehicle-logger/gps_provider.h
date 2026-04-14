// gps_provider.h — GPS provider interface and implementations
#pragma once

#include "types.h"
#include "config.h"

// --- Interface ---
class GPSProvider {
public:
  virtual ~GPSProvider() {}
  virtual void begin() = 0;
  virtual void update() = 0;     // call frequently to feed parser
  virtual bool read(GPSData& out) = 0;  // true if new fix available
};

// --- TinyGPS++ implementation ---
#include <TinyGPS++.h>

class TinyGPSProvider : public GPSProvider {
public:
  TinyGPSProvider(HardwareSerial& serial, int txPin, int rxPin, unsigned long baud)
    : _serial(serial), _txPin(txPin), _rxPin(rxPin), _baud(baud), _lastReadMs(0) {}

  void begin() override {
    _serial.begin(_baud, SERIAL_8N1, _rxPin, _txPin);
  }

  void update() override {
    while (_serial.available() > 0) {
      _gps.encode(_serial.read());
    }
  }

  bool read(GPSData& out) override {
    update();

    if (!_gps.location.isUpdated()) {
      return false;
    }

    out.valid = _gps.location.isValid();
    out.latitude = _gps.location.lat();
    out.longitude = _gps.location.lng();
    out.altitude = _gps.altitude.isValid() ? _gps.altitude.meters() : 0.0;
    out.speed = _gps.speed.isValid() ? _gps.speed.kmph() : 0.0;
    out.course = _gps.course.isValid() ? _gps.course.deg() : 0.0;
    out.satellites = _gps.satellites.isValid() ? _gps.satellites.value() : 0;

    // Build ISO 8601 timestamp from GPS time
    if (_gps.date.isValid() && _gps.time.isValid()) {
      snprintf(out.timestamp, sizeof(out.timestamp),
               "%04d-%02d-%02dT%02d:%02d:%02dZ",
               _gps.date.year(), _gps.date.month(), _gps.date.day(),
               _gps.time.hour(), _gps.time.minute(), _gps.time.second());
    } else {
      snprintf(out.timestamp, sizeof(out.timestamp), "0000-00-00T00:00:00Z");
    }

    return true;
  }

private:
  HardwareSerial& _serial;
  int _txPin, _rxPin;
  unsigned long _baud;
  TinyGPSPlus _gps;
  unsigned long _lastReadMs;
};

// --- Mock implementation ---
class MockGPSProvider : public GPSProvider {
public:
  void begin() override {
    Serial.println("[MockGPS] initialized");
  }

  void update() override {}

  bool read(GPSData& out) override {
    // Simulate a position in Tokyo with slight drift
    unsigned long now = millis();
    double drift = (now % 10000) * 0.00001;

    out.valid = true;
    out.latitude = 35.6812 + drift;
    out.longitude = 139.7671 + drift;
    out.altitude = 40.0;
    out.speed = 30.0 + (now % 100) * 0.1;
    out.course = (now / 1000) % 360;
    out.satellites = 8;
    snprintf(out.timestamp, sizeof(out.timestamp),
             "2026-04-14T%02lu:%02lu:%02luZ",
             (now / 3600000) % 24, (now / 60000) % 60, (now / 1000) % 60);
    return true;
  }
};
