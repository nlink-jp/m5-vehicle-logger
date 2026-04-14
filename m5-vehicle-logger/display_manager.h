// display_manager.h — M5Stack LCD status display
#pragma once

#include <M5Stack.h>
#include "types.h"
#include "data_buffer.h"
#include "network_manager.h"
#include "config.h"

// Stats tracked for display
struct SensorStats {
  uint32_t gpsReadCount;     // total GPS reads
  uint32_t imuReadCount;     // total IMU reads
  uint32_t sendSuccessCount; // successful batch sends
  uint32_t sendFailCount;    // failed batch sends
  uint8_t lastIMUSamples;    // IMU samples in last epoch
};

class DisplayManager {
public:
  DisplayManager() : _lastUpdateMs(0) {
    memset(&stats, 0, sizeof(stats));
  }

  SensorStats stats;

  void begin() {
    M5.Lcd.setTextSize(1);
    M5.Lcd.fillScreen(BLACK);
    _drawHeader();
  }

  void update(const GPSData& gps, const DataBuffer& buffer,
              const NetworkManager& network) {
    unsigned long now = millis();
    if (now - _lastUpdateMs < DISPLAY_UPDATE_INTERVAL_MS) return;
    _lastUpdateMs = now;

    int y = 26;
    int lineH = 18;
    M5.Lcd.setTextSize(2);
    M5.Lcd.fillRect(0, y, 320, 240 - y, BLACK);

    // ── GPS ──
    _sectionLabel(y, "GPS");
    y += lineH;
    M5.Lcd.setCursor(5, y);
    if (gps.valid) {
      _statusDot(y, GREEN);
      M5.Lcd.setTextColor(WHITE, BLACK);
      M5.Lcd.printf(" %d sat  %.1f km/h", gps.satellites, gps.speed);
      y += lineH;
      M5.Lcd.setCursor(5, y);
      M5.Lcd.setTextSize(1);
      M5.Lcd.printf("  %.6f, %.6f  alt:%.0fm", gps.latitude, gps.longitude, gps.altitude);
      M5.Lcd.setTextSize(2);
    } else {
      _statusDot(y, RED);
      M5.Lcd.setTextColor(DARKGREY, BLACK);
      M5.Lcd.print(" no fix");
    }

    // ── IMU ──
    y += lineH + 4;
    _sectionLabel(y, "IMU");
    y += lineH;
    M5.Lcd.setCursor(5, y);
    if (stats.imuReadCount > 0) {
      _statusDot(y, GREEN);
      M5.Lcd.setTextColor(WHITE, BLACK);
      M5.Lcd.printf(" %d/s  total:%lu", stats.lastIMUSamples, stats.imuReadCount);
    } else {
      _statusDot(y, DARKGREY);
      M5.Lcd.setTextColor(DARKGREY, BLACK);
      M5.Lcd.print(" waiting...");
    }

    // ── Network ──
    y += lineH + 4;
    _sectionLabel(y, "NET");
    y += lineH;
    M5.Lcd.setCursor(5, y);
    if (network.isConnected()) {
      _statusDot(y, GREEN);
      M5.Lcd.setTextColor(WHITE, BLACK);
      M5.Lcd.printf(" %s", network.ssid());
    } else {
      _statusDot(y, RED);
      M5.Lcd.setTextColor(DARKGREY, BLACK);
      M5.Lcd.print(" disconnected");
    }

    // Send stats
    y += lineH;
    M5.Lcd.setCursor(5, y);
    M5.Lcd.setTextColor(WHITE, BLACK);
    M5.Lcd.printf("  TX ok:%lu fail:%lu", stats.sendSuccessCount, stats.sendFailCount);

    // ── Buffer ──
    y += lineH + 4;
    _sectionLabel(y, "BUF");
    y += lineH;
    M5.Lcd.setCursor(5, y);
    // Bar graph
    int barW = 200;
    int barH = 14;
    int barX = 80;
    float fillRatio = (float)buffer.count() / buffer.capacity();
    uint16_t barColor = fillRatio > 0.9 ? RED : fillRatio > 0.7 ? YELLOW : GREEN;
    M5.Lcd.drawRect(barX, y, barW, barH, WHITE);
    M5.Lcd.fillRect(barX + 1, y + 1, (int)((barW - 2) * fillRatio), barH - 2, barColor);
    M5.Lcd.setTextColor(WHITE, BLACK);
    M5.Lcd.printf("%3d/%d", buffer.count(), buffer.capacity());

    // ── Footer: uptime ──
    y = 240 - lineH - 2;
    M5.Lcd.setCursor(5, y);
    unsigned long sec = now / 1000;
    M5.Lcd.setTextSize(1);
    M5.Lcd.setTextColor(DARKGREY, BLACK);
    M5.Lcd.printf("uptime %02lu:%02lu:%02lu  gps_rx:%lu",
                   sec / 3600, (sec / 60) % 60, sec % 60, stats.gpsReadCount);
    M5.Lcd.setTextSize(2);
  }

private:
  void _drawHeader() {
    M5.Lcd.fillRect(0, 0, 320, 24, NAVY);
    M5.Lcd.setTextSize(2);
    M5.Lcd.setCursor(5, 4);
    M5.Lcd.setTextColor(WHITE, NAVY);
    M5.Lcd.print("Vehicle Logger");
  }

  void _sectionLabel(int y, const char* label) {
    M5.Lcd.setCursor(5, y);
    M5.Lcd.setTextColor(CYAN, BLACK);
    M5.Lcd.setTextSize(1);
    M5.Lcd.printf("--- %s ", label);
    // Draw line
    int cursorX = M5.Lcd.getCursorX();
    M5.Lcd.drawFastHLine(cursorX, y + 4, 320 - cursorX - 5, CYAN);
    M5.Lcd.setTextSize(2);
  }

  void _statusDot(int y, uint16_t color) {
    M5.Lcd.fillCircle(10, y + 7, 5, color);
  }

  unsigned long _lastUpdateMs;
};
