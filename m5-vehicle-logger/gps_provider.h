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

// --- TinyGPS++ implementation for AT6668 (M5 GPS Module v2.1) ---
#include <TinyGPS++.h>

class TinyGPSProvider : public GPSProvider {
public:
  TinyGPSProvider(HardwareSerial& serial, int txPin, int rxPin, unsigned long baud)
    : _serial(serial), _txPin(txPin), _rxPin(rxPin), _baud(baud) {}

  void begin() override {
    _serial.end();
    delay(100);
    Serial.printf("[GPS] init: baud=%lu rx=%d tx=%d\n", _baud, _rxPin, _txPin);
    _serial.begin(_baud, SERIAL_8N1, _rxPin, _txPin);
    while (_serial.available()) _serial.read();
    delay(1000);

    // Verify NMEA reception
    bool ok = false;
    unsigned long start = millis();
    while (millis() - start < 3000) {
      if (_serial.available()) {
        if (_serial.read() == '$') {
          ok = true;
          break;
        }
      }
    }
    Serial.printf("[GPS] NMEA: %s\n", ok ? "OK" : "not detected");
    // Flush remaining
    while (_serial.available()) _serial.read();
  }

  void update() override {
    while (_serial.available() > 0) {
      _gps.encode(_serial.read());
    }

    // Periodic diagnostic (every 5 seconds)
    unsigned long now = millis();
    if (now - _lastDiagMs >= 5000) {
      _lastDiagMs = now;
      Serial.printf("[GPS] chars=%lu sentences=%lu failed=%lu sat=%d fix=%d\n",
                    _gps.charsProcessed(), _gps.sentencesWithFix(),
                    _gps.failedChecksum(),
                    _gps.satellites.isValid() ? _gps.satellites.value() : -1,
                    _gps.location.isValid() ? 1 : 0);
    }
  }

  bool read(GPSData& out) override {
    update();

    // Always populate what we have, even without a position fix.
    // This lets the display show satellite count during acquisition.
    out.satellites = _gps.satellites.isValid() ? _gps.satellites.value() : 0;
    out.valid = _gps.location.isValid();
    out.latitude = _gps.location.isValid() ? _gps.location.lat() : 0.0;
    out.longitude = _gps.location.isValid() ? _gps.location.lng() : 0.0;
    out.altitude = _gps.altitude.isValid() ? _gps.altitude.meters() : 0.0;
    out.speed = _gps.speed.isValid() ? _gps.speed.kmph() : 0.0;
    out.course = _gps.course.isValid() ? _gps.course.deg() : 0.0;

    if (_gps.date.isValid() && _gps.time.isValid()) {
      snprintf(out.timestamp, sizeof(out.timestamp),
               "%04d-%02d-%02dT%02d:%02d:%02dZ",
               _gps.date.year(), _gps.date.month(), _gps.date.day(),
               _gps.time.hour(), _gps.time.minute(), _gps.time.second());
    } else {
      snprintf(out.timestamp, sizeof(out.timestamp), "0000-00-00T00:00:00Z");
    }

    // Return true if any NMEA data was processed (not just on fix)
    return _gps.charsProcessed() > 0;
  }

private:
  HardwareSerial& _serial;
  int _txPin, _rxPin;
  unsigned long _baud;
  TinyGPSPlus _gps;
  unsigned long _lastDiagMs = 0;
};

// --- Mock implementation ---
class MockGPSProvider : public GPSProvider {
public:
  void begin() override {
    Serial.println("[MockGPS] initialized");
  }

  void update() override {}

  bool read(GPSData& out) override {
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
