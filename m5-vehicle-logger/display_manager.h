// display_manager.h — M5Stack LCD status display (double-buffered via M5Canvas)
#pragma once

#include <M5Unified.h>
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
  DisplayManager() : _canvas(&M5.Display), _lastUpdateMs(0) {
    memset(&stats, 0, sizeof(stats));
  }

  SensorStats stats;

  void begin() {
    _canvas.createSprite(320, 240);
    _canvas.setTextSize(2);
    _canvas.fillSprite(BLACK);
    _drawHeader();
    _canvas.pushSprite(0, 0);
  }

  void update(const GPSData& gps, const DataBuffer& buffer,
              const WifiManager& network) {
    unsigned long now = millis();
    if (now - _lastUpdateMs < DISPLAY_UPDATE_INTERVAL_MS) return;
    _lastUpdateMs = now;

    // Draw everything to off-screen canvas
    _canvas.fillSprite(BLACK);
    _drawHeader();

    int y = 28;
    int lineH = 18;
    _canvas.setTextSize(2);

    // ── GPS ──
    _sectionLabel(y, "GPS");
    y += lineH;
    if (gps.valid) {
      _statusDot(y, GREEN);
      _canvas.setCursor(20, y);
      _canvas.setTextColor(WHITE, BLACK);
      _canvas.printf("%d sat  %.1f km/h", gps.satellites, gps.speed);
      y += lineH;
      _canvas.setCursor(20, y);
      _canvas.setTextSize(1);
      _canvas.printf("%.6f, %.6f  alt:%.0fm", gps.latitude, gps.longitude, gps.altitude);
      _canvas.setTextSize(2);
    } else {
      _statusDot(y, RED);
      _canvas.setCursor(20, y);
      _canvas.setTextColor(DARKGREY, BLACK);
      _canvas.print("no fix");
    }

    // ── IMU ──
    y += lineH + 4;
    _sectionLabel(y, "IMU");
    y += lineH;
    if (stats.imuReadCount > 0) {
      _statusDot(y, GREEN);
      _canvas.setCursor(20, y);
      _canvas.setTextColor(WHITE, BLACK);
      _canvas.printf("%d/s  total:%lu", stats.lastIMUSamples, stats.imuReadCount);
    } else {
      _statusDot(y, DARKGREY);
      _canvas.setCursor(20, y);
      _canvas.setTextColor(DARKGREY, BLACK);
      _canvas.print("waiting...");
    }

    // ── Network ──
    y += lineH + 4;
    _sectionLabel(y, "NET");
    y += lineH;
    if (network.isConnected()) {
      _statusDot(y, GREEN);
      _canvas.setCursor(20, y);
      _canvas.setTextColor(WHITE, BLACK);
      _canvas.printf("%s", network.ssid());
    } else {
      _statusDot(y, RED);
      _canvas.setCursor(20, y);
      _canvas.setTextColor(DARKGREY, BLACK);
      _canvas.print("disconnected");
    }

    // Send stats
    y += lineH;
    _canvas.setCursor(20, y);
    _canvas.setTextColor(WHITE, BLACK);
    _canvas.printf("TX ok:%lu fail:%lu", stats.sendSuccessCount, stats.sendFailCount);

    // ── Buffer ──
    y += lineH + 4;
    _sectionLabel(y, "BUF");
    y += lineH;
    _canvas.setCursor(5, y);
    _canvas.setTextColor(WHITE, BLACK);
    _canvas.printf("%3d/%d ", buffer.count(), buffer.capacity());
    // Bar graph
    int barX = 100;
    int barW = 210;
    int barH = 14;
    float fillRatio = (float)buffer.count() / buffer.capacity();
    uint16_t barColor = fillRatio > 0.9 ? RED : fillRatio > 0.7 ? YELLOW : GREEN;
    _canvas.drawRect(barX, y, barW, barH, WHITE);
    int fillW = (int)((barW - 2) * fillRatio);
    if (fillW > 0) {
      _canvas.fillRect(barX + 1, y + 1, fillW, barH - 2, barColor);
    }

    // ── Footer: uptime ──
    y = 240 - 14;
    _canvas.setCursor(5, y);
    unsigned long sec = now / 1000;
    _canvas.setTextSize(1);
    _canvas.setTextColor(DARKGREY, BLACK);
    _canvas.printf("uptime %02lu:%02lu:%02lu  gps_rx:%lu",
                   sec / 3600, (sec / 60) % 60, sec % 60, stats.gpsReadCount);

    // Push entire frame to LCD at once — no flicker
    _canvas.pushSprite(0, 0);
  }

private:
  M5Canvas _canvas;
  unsigned long _lastUpdateMs;

  void _drawHeader() {
    _canvas.fillRect(0, 0, 320, 24, NAVY);
    _canvas.setTextSize(2);
    _canvas.setCursor(5, 4);
    _canvas.setTextColor(WHITE, NAVY);
    _canvas.print("Vehicle Logger");
  }

  void _sectionLabel(int y, const char* label) {
    _canvas.setTextSize(1);
    _canvas.setCursor(5, y);
    _canvas.setTextColor(CYAN, BLACK);
    _canvas.printf("--- %s ", label);
    int cursorX = _canvas.getCursorX();
    _canvas.drawFastHLine(cursorX, y + 4, 320 - cursorX - 5, CYAN);
    _canvas.setTextSize(2);
  }

  void _statusDot(int y, uint16_t color) {
    _canvas.fillCircle(10, y + 7, 5, color);
  }
};
