// display_manager.h — M5Stack LCD status display
// Zero-flicker: text with background color overwrites previous content.
// No fillRect in update loop — only padded fixed-width printf.
#pragma once

#include <M5Unified.h>
#include "types.h"
#include "data_buffer.h"
#include "network_manager.h"
#include "config.h"

// Stats tracked for display
struct SensorStats {
  uint32_t gpsReadCount;
  uint32_t imuReadCount;
  uint32_t sendSuccessCount;
  uint32_t sendFailCount;
  uint8_t lastIMUSamples;
};

class DisplayManager {
public:
  DisplayManager() : _lastUpdateMs(0), _initialized(false) {
    memset(&stats, 0, sizeof(stats));
  }

  SensorStats stats;

  void begin() {
    M5.Display.fillScreen(BLACK);
    _drawHeader();
    // Draw static section labels once
    _sectionLabel(28,  "GPS");
    _sectionLabel(98,  "IMU");
    _sectionLabel(134, "NET");
    _sectionLabel(188, "BUF");
    _initialized = true;
  }

  void update(const GPSData& gps, const DataBuffer& buffer,
              const WifiManager& network) {
    unsigned long now = millis();
    if (now - _lastUpdateMs < DISPLAY_UPDATE_INTERVAL_MS) return;
    _lastUpdateMs = now;
    if (!_initialized) return;

    char line[54]; // 320px / 6px per char at size 1 = 53 chars max
    int y;

    // ── GPS data (y=46..92) ──
    y = 46;
    _drawDot(y, gps.valid ? GREEN : RED);
    M5.Display.setTextSize(2);
    M5.Display.setTextColor(gps.valid ? WHITE : DARKGREY, BLACK);
    M5.Display.setCursor(20, y);
    if (gps.valid) {
      snprintf(line, sizeof(line), "%d sat  %5.1f km/h    ", gps.satellites, gps.speed);
    } else {
      // Show satellite count even without fix
      if (gps.satellites > 0) {
        snprintf(line, sizeof(line), "no fix  %d sat       ", gps.satellites);
      } else {
        snprintf(line, sizeof(line), "no fix  searching... ");
      }
    }
    M5.Display.print(line);

    y += 20;
    M5.Display.setTextSize(1);
    M5.Display.setTextColor(WHITE, BLACK);
    M5.Display.setCursor(20, y);
    if (gps.valid) {
      snprintf(line, sizeof(line), "%.6f, %.6f  alt:%.0fm       ", gps.latitude, gps.longitude, gps.altitude);
    } else {
      snprintf(line, sizeof(line), "acquiring signal...                    ");
    }
    M5.Display.print(line);

    y += 12;
    M5.Display.setCursor(20, y);
    if (gps.valid) {
      snprintf(line, sizeof(line), "course:%.0f deg  %s       ", gps.course, gps.timestamp);
    } else {
      snprintf(line, sizeof(line), "                                      ");
    }
    M5.Display.print(line);

    // ── IMU data (y=116..130) ──
    y = 116;
    _drawDot(y, stats.imuReadCount > 0 ? GREEN : DARKGREY);
    M5.Display.setTextSize(2);
    M5.Display.setCursor(20, y);
    M5.Display.setTextColor(stats.imuReadCount > 0 ? WHITE : DARKGREY, BLACK);
    snprintf(line, sizeof(line), "%2d/s  total:%-8lu", stats.lastIMUSamples, stats.imuReadCount);
    M5.Display.print(line);

    // ── Network (y=152..180) ──
    y = 152;
    _drawDot(y, network.isConnected() ? GREEN : RED);
    M5.Display.setTextSize(2);
    M5.Display.setCursor(20, y);
    M5.Display.setTextColor(network.isConnected() ? WHITE : DARKGREY, BLACK);
    if (network.isConnected()) {
      snprintf(line, sizeof(line), "%-20s", network.ssid());
    } else {
      snprintf(line, sizeof(line), "disconnected        ");
    }
    M5.Display.print(line);

    y += 18;
    M5.Display.setCursor(20, y);
    M5.Display.setTextColor(WHITE, BLACK);
    snprintf(line, sizeof(line), "TX ok:%-6lu fail:%-4lu", stats.sendSuccessCount, stats.sendFailCount);
    M5.Display.print(line);

    // ── Buffer (y=206..220) ──
    y = 206;
    M5.Display.setTextSize(2);
    M5.Display.setCursor(5, y);
    M5.Display.setTextColor(WHITE, BLACK);
    snprintf(line, sizeof(line), "%3d/%-3d", buffer.count(), buffer.capacity());
    M5.Display.print(line);

    // Bar graph — only redraw the boundary between filled/empty
    int barX = 100;
    int barW = 210;
    int barH = 14;
    float fillRatio = (float)buffer.count() / buffer.capacity();
    uint16_t barColor = fillRatio > 0.9 ? RED : fillRatio > 0.7 ? YELLOW : GREEN;
    int fillW = (int)((barW - 2) * fillRatio);
    if (fillW < 0) fillW = 0;
    if (fillW > barW - 2) fillW = barW - 2;

    // Draw border once per frame (thin, fast)
    M5.Display.drawRect(barX, y, barW, barH, WHITE);
    // Filled portion
    if (fillW > 0) {
      M5.Display.fillRect(barX + 1, y + 1, fillW, barH - 2, barColor);
    }
    // Empty portion
    int emptyW = (barW - 2) - fillW;
    if (emptyW > 0) {
      M5.Display.fillRect(barX + 1 + fillW, y + 1, emptyW, barH - 2, BLACK);
    }

    // ── Footer (y=228) ──
    y = 228;
    unsigned long sec = now / 1000;
    M5.Display.setTextSize(1);
    M5.Display.setCursor(5, y);
    M5.Display.setTextColor(DARKGREY, BLACK);
    snprintf(line, sizeof(line), "uptime %02lu:%02lu:%02lu  gps_rx:%-8lu",
             sec / 3600, (sec / 60) % 60, sec % 60, stats.gpsReadCount);
    M5.Display.print(line);
  }

private:
  unsigned long _lastUpdateMs;
  bool _initialized;

  void _drawHeader() {
    M5.Display.fillRect(0, 0, 320, 24, NAVY);
    M5.Display.setTextSize(2);
    M5.Display.setCursor(5, 4);
    M5.Display.setTextColor(WHITE, NAVY);
    M5.Display.print("Vehicle Logger");
  }

  // Draw once in begin() — static labels don't need redraw
  void _sectionLabel(int y, const char* label) {
    M5.Display.setTextSize(1);
    M5.Display.setCursor(5, y);
    M5.Display.setTextColor(CYAN, BLACK);
    M5.Display.printf("--- %s ", label);
    int cursorX = M5.Display.getCursorX();
    M5.Display.drawFastHLine(cursorX, y + 4, 320 - cursorX - 5, CYAN);
  }

  // Status dot — small enough that overdraw is invisible
  void _drawDot(int y, uint16_t color) {
    M5.Display.fillCircle(10, y + 7, 5, color);
  }
};
