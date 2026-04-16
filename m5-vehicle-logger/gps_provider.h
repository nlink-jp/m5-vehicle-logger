// gps_provider.h — GPS provider interface and implementations
#pragma once

#include "types.h"
#include "config.h"

// --- Satellite info for skyplot ---
#define MAX_SATS 40

struct SatInfo {
  uint8_t prn;        // satellite PRN number
  uint8_t elevation;  // degrees (0-90)
  uint16_t azimuth;   // degrees (0-359)
  uint8_t snr;        // signal-to-noise (dB-Hz), 0 = not tracking
};

struct SatData {
  SatInfo sats[MAX_SATS];
  uint8_t count;
};

// --- Interface ---
class GPSProvider {
public:
  virtual ~GPSProvider() {}
  virtual void begin() = 0;
  virtual void update() = 0;
  virtual bool read(GPSData& out) = 0;
  virtual const SatData& satellites() const = 0;
};

// --- GSV sentence parser (manual NMEA parsing) ---
// Parses $GxGSV sentences to extract satellite positions.
// Works with all constellations: GP(GPS), GL(GLONASS), GA(Galileo), BD/GB(BeiDou)
class GSVParser {
public:
  // Feed one character at a time. Call from the main NMEA stream.
  void feed(char c) {
    if (c == '$') {
      _pos = 0;
      _buf[_pos++] = c;
      return;
    }
    if (_pos == 0) return;  // waiting for $

    if (c == '\r' || c == '\n') {
      if (_pos > 10) {
        _buf[_pos] = '\0';
        _parseLine();
      }
      _pos = 0;
      return;
    }

    if (_pos < (int)sizeof(_buf) - 1) {
      _buf[_pos++] = c;
    } else {
      _pos = 0;  // overflow
    }
  }

  const SatData& data() const { return _committed; }

private:
  char _buf[100];
  int _pos = 0;

  // Double-buffer: build into _working, commit to _committed when last GSV msg
  SatData _working;
  SatData _committed;
  bool _newSequence = true;

  void _parseLine() {
    // Check it's a GSV sentence: $xxGSV
    if (_buf[0] != '$') return;
    if (_buf[3] != 'G' || _buf[4] != 'S' || _buf[5] != 'V') return;

    // Verify checksum
    if (!_verifyChecksum()) return;

    // Parse fields (comma-separated)
    // $xxGSV,totalMsgs,msgNum,totalSats[,prn,elev,azim,snr]*checksum
    char* fields[24];
    int nf = _split(fields, 24);
    if (nf < 4) return;

    int totalMsgs = atoi(fields[1]);
    int msgNum = atoi(fields[2]);
    // int totalSats = atoi(fields[3]);  // not used directly

    // First message of a new constellation sequence
    if (msgNum == 1 && _newSequence) {
      _working.count = 0;
      _newSequence = false;
    }

    // Parse satellite blocks (4 fields each, starting at field 4)
    for (int i = 4; i + 3 < nf && _working.count < MAX_SATS; i += 4) {
      if (strlen(fields[i]) == 0) continue;  // empty PRN = padding
      SatInfo& s = _working.sats[_working.count];
      s.prn = atoi(fields[i]);
      s.elevation = atoi(fields[i + 1]);
      s.azimuth = atoi(fields[i + 2]);
      s.snr = (strlen(fields[i + 3]) > 0) ? atoi(fields[i + 3]) : 0;
      _working.count++;
    }

    // Last message of this constellation — check if this is the final one
    if (msgNum == totalMsgs) {
      // Commit: copy working to committed
      // For multi-constellation, we accumulate across GP/GL/GA/BD sequences
      // Commit after a brief period (handled by checking if msgNum==totalMsgs
      // for the last constellation we see)
      _committed = _working;
      _newSequence = true;  // next GSV starts fresh
    }
  }

  bool _verifyChecksum() {
    // Find * and compute XOR checksum
    char* star = strchr(_buf, '*');
    if (!star || (star - _buf) < 2) return false;

    uint8_t calc = 0;
    for (char* p = _buf + 1; p < star; p++) {
      calc ^= *p;
    }

    uint8_t expect = (uint8_t)strtol(star + 1, NULL, 16);
    return (calc == expect);
  }

  int _split(char** fields, int maxFields) {
    int count = 0;
    // Skip $ and start from first field
    char* p = _buf + 1;  // skip $
    // Find first comma (skip sentence ID)
    // Actually, split all fields including sentence ID
    p = _buf;
    while (*p == '$') p++;  // skip $

    // Split by comma, also stop at *
    fields[count++] = p;
    while (*p && *p != '*' && count < maxFields) {
      if (*p == ',') {
        *p = '\0';
        fields[count++] = p + 1;
      }
      p++;
    }
    if (*p == '*') *p = '\0';

    return count;
  }
};

// --- TinyGPS++ implementation for NEO-M9N (GNSS Module M135) ---
// Also compatible with AT6668 (GPS Module v2.1) — just change GPS_BAUD
#include <TinyGPS++.h>

class TinyGPSProvider : public GPSProvider {
public:
  TinyGPSProvider(HardwareSerial& serial, int txPin, int rxPin, unsigned long baud)
    : _serial(serial), _txPin(txPin), _rxPin(rxPin), _baud(baud) {
    _satData.count = 0;
  }

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
    while (_serial.available()) _serial.read();
  }

  void update() override {
    while (_serial.available() > 0) {
      char c = _serial.read();
      _gps.encode(c);
      _gsv.feed(c);  // also feed GSV parser
    }

    // Periodic diagnostic
    unsigned long now = millis();
    if (now - _lastDiagMs >= 5000) {
      _lastDiagMs = now;
      Serial.printf("[GPS] chars=%lu ok=%lu fail=%lu fixSat=%d sats=%d fix=%d\n",
                    _gps.charsProcessed(), _gps.sentencesWithFix(),
                    _gps.failedChecksum(),
                    _gps.satellites.isValid() ? _gps.satellites.value() : -1,
                    _gsv.data().count,
                    _gps.location.isValid() ? 1 : 0);
    }
  }

  bool read(GPSData& out) override {
    update();

    uint8_t fixSats = _gps.satellites.isValid() ? _gps.satellites.value() : 0;
    uint8_t inView = _gsv.data().count;
    out.satellites = fixSats > 0 ? fixSats : inView;
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

    _satData = _gsv.data();
    return _gps.charsProcessed() > 0;
  }

  const SatData& satellites() const override { return _satData; }

private:
  HardwareSerial& _serial;
  int _txPin, _rxPin;
  unsigned long _baud;
  TinyGPSPlus _gps;
  GSVParser _gsv;
  SatData _satData;
  unsigned long _lastDiagMs = 0;
};

// --- Mock implementation ---
class MockGPSProvider : public GPSProvider {
public:
  MockGPSProvider() { _satData.count = 0; }

  void begin() override {
    Serial.println("[MockGPS] initialized");
    // Create some fake satellites for skyplot testing
    _satData.count = 6;
    uint8_t prns[]  = {1,  3,  7, 14, 20, 28};
    uint8_t elevs[] = {45, 70, 20, 55, 10, 80};
    uint16_t azs[]  = {30, 120, 210, 300, 75, 350};
    uint8_t snrs[]  = {42, 35, 28, 40, 15, 38};
    for (int i = 0; i < 6; i++) {
      _satData.sats[i] = {prns[i], elevs[i], azs[i], snrs[i]};
    }
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

  const SatData& satellites() const override { return _satData; }

private:
  SatData _satData;
};
