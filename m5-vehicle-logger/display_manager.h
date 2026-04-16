// display_manager.h — M5Stack LCD multi-page display
// Page 0: Dashboard (all sensors overview)
// Page 1: Motion (accel XY circle + gyro bars)
// Page 2: Trend (time-series line graphs)
//
// Zero-flicker: text with background color, partial redraws.
#pragma once

#include <M5Unified.h>
#include "types.h"
#include "gps_provider.h"
#include "data_buffer.h"
#include "network_manager.h"
#include "trend_buffer.h"
#include "config.h"

#define PAGE_DASHBOARD 0
#define PAGE_MOTION    1
#define PAGE_TREND     2
#define PAGE_COUNT     3

struct SensorStats {
  uint32_t gpsReadCount;
  uint32_t imuReadCount;
  uint32_t sendSuccessCount;
  uint32_t sendFailCount;
  uint8_t lastIMUSamples;
};

class DisplayManager {
public:
  DisplayManager() : _lastUpdateMs(0), _initialized(false), _page(PAGE_DASHBOARD),
                     _prevPage(-1) {
    memset(&stats, 0, sizeof(stats));
  }

  SensorStats stats;

  void begin() {
    M5.Display.fillScreen(BLACK);
    _initialized = true;
    _prevPage = -1;  // force full redraw
  }

  void setPage(int page) {
    if (page >= 0 && page < PAGE_COUNT && page != _page) {
      _page = page;
      _prevPage = -1;  // force full redraw
    }
  }

  int page() const { return _page; }

  void update(const GPSData& gps, const SatData& satData,
              const IMUData& imuData, const EnvData& envData,
              const DataBuffer& buffer, const WifiManager& network,
              const TrendBuffer& trend) {
    unsigned long now = millis();
    if (now - _lastUpdateMs < DISPLAY_UPDATE_INTERVAL_MS) return;
    _lastUpdateMs = now;
    if (!_initialized) return;

    // Page changed — clear and draw static elements
    if (_prevPage != _page) {
      M5.Display.fillScreen(BLACK);
      _drawPageHeader();
      _prevPage = _page;
    }

    switch (_page) {
      case PAGE_DASHBOARD:
        _drawDashboard(gps, satData, imuData, envData, buffer, network);
        break;
      case PAGE_MOTION:
        _drawMotion(imuData);
        break;
      case PAGE_TREND:
        _drawTrend(trend);
        break;
    }

    // Footer: page indicator + uptime (common to all pages)
    _drawFooter();
  }

private:
  unsigned long _lastUpdateMs;
  bool _initialized;
  int _page;
  int _prevPage;

  // Motion page state (for incremental drawing)
  int _prevPlotX = 160;
  int _prevPlotY = 110;

  // Peak hold: per-angle maximum G for radar chart
  static const int PEAK_BINS = 72;   // 5° per bin (360/72)
  float _peakG[72];                  // max G magnitude per direction
  bool _peakInited = false;
  float _peakMaxG = 0.0f;            // overall peak G

  // ========== Common ==========

  void _drawPageHeader() {
    const char* titles[] = {"Dashboard", "Motion", "Trend"};
    M5.Display.fillRect(0, 0, 320, 20, NAVY);
    M5.Display.setTextSize(2);
    M5.Display.setCursor(5, 2);
    M5.Display.setTextColor(WHITE, NAVY);
    M5.Display.print(titles[_page]);

    // Button labels at bottom
    M5.Display.setTextSize(1);
    M5.Display.setTextColor(CYAN, BLACK);
    const char* labels[] = {"[A]Dash", "[B]Motion", "[C]Trend"};
    int xpos[] = {10, 125, 250};
    for (int i = 0; i < 3; i++) {
      M5.Display.setCursor(xpos[i], 232);
      if (i == _page) {
        M5.Display.setTextColor(BLACK, CYAN);
      } else {
        M5.Display.setTextColor(CYAN, BLACK);
      }
      M5.Display.print(labels[i]);
    }
  }

  void _drawFooter() {
    unsigned long sec = millis() / 1000;
    char line[54];
    M5.Display.setTextSize(1);
    M5.Display.setCursor(5, 222);
    M5.Display.setTextColor(DARKGREY, BLACK);
    snprintf(line, sizeof(line), "up %02lu:%02lu:%02lu  imu:%lu gps:%lu       ",
             sec / 3600, (sec / 60) % 60, sec % 60,
             stats.imuReadCount, stats.gpsReadCount);
    M5.Display.print(line);
  }

  void _sectionLabel(int y, const char* label, int rightLimit = 315) {
    M5.Display.setTextSize(1);
    M5.Display.setCursor(5, y);
    M5.Display.setTextColor(CYAN, BLACK);
    M5.Display.printf("--- %s ", label);
    int cx = M5.Display.getCursorX();
    int lineW = rightLimit - cx;
    if (lineW > 0) {
      M5.Display.drawFastHLine(cx, y + 4, lineW, CYAN);
    }
  }

  void _drawDot(int y, uint16_t color) {
    M5.Display.fillCircle(10, y + 4, 3, color);
  }

  // ========== Page 0: Dashboard ==========

  void _drawDashboard(const GPSData& gps, const SatData& satData,
                      const IMUData& imuData, const EnvData& envData,
                      const DataBuffer& buffer, const WifiManager& network) {
    char line[54];
    int y;

    // --- GPS + Skyplot ---
    y = 24;
    _sectionLabel(y, "GPS", 250);
    y += 12;

    // Left side: text info (x=0..184)
    _drawDot(y, gps.valid ? GREEN : RED);
    M5.Display.setTextSize(2);
    M5.Display.setTextColor(gps.valid ? WHITE : DARKGREY, BLACK);
    M5.Display.setCursor(20, y);
    if (gps.valid) {
      snprintf(line, sizeof(line), "%d sat %5.1fkm/h", gps.satellites, gps.speed);
    } else {
      if (gps.satellites > 0) {
        snprintf(line, sizeof(line), "no fix %d sat  ", gps.satellites);
      } else {
        snprintf(line, sizeof(line), "no fix search..");
      }
    }
    M5.Display.print(line);

    y += 20;
    M5.Display.setTextSize(1);
    M5.Display.setTextColor(WHITE, BLACK);
    M5.Display.setCursor(20, y);
    if (gps.valid) {
      snprintf(line, sizeof(line), "%.6f, %.6f", gps.latitude, gps.longitude);
    } else {
      snprintf(line, sizeof(line), "acquiring signal...  ");
    }
    M5.Display.print(line);

    y += 10;
    M5.Display.setCursor(20, y);
    if (gps.valid) {
      snprintf(line, sizeof(line), "alt:%.0fm crs:%.0f %s", gps.altitude, gps.course, gps.timestamp);
    } else {
      snprintf(line, sizeof(line), "                              ");
    }
    M5.Display.print(line);

    // Right side: skyplot (60px diameter)
    // y starts at 36 (after section label), so skyplot top = 37
    _drawSkyplot(255, 37, 30, satData);

    // --- IMU/MAG ---
    y = 82;
    _sectionLabel(y, "ACCEL/GYRO/MAG", 250);
    y += 12;
    _drawDot(y, stats.imuReadCount > 0 ? GREEN : DARKGREY);
    M5.Display.setTextSize(1);
    M5.Display.setTextColor(stats.imuReadCount > 0 ? WHITE : DARKGREY, BLACK);

    M5.Display.setCursor(20, y);
    snprintf(line, sizeof(line), "A %+6.2f %+6.2f %+6.2f g            ",
             imuData.ax, imuData.ay, imuData.az);
    M5.Display.print(line);

    y += 10;
    M5.Display.setCursor(20, y);
    snprintf(line, sizeof(line), "G %+7.1f %+7.1f %+7.1f d/s         ",
             imuData.gx, imuData.gy, imuData.gz);
    M5.Display.print(line);

    y += 10;
    M5.Display.setCursor(20, y);
    snprintf(line, sizeof(line), "M %+6.1f %+6.1f %+6.1f uT  %2d/s   ",
             imuData.mx, imuData.my, imuData.mz, stats.lastIMUSamples);
    M5.Display.print(line);

    // --- ENV ---
    y = 128;
    _sectionLabel(y, "ENV");
    y += 12;
    _drawDot(y, envData.valid ? GREEN : DARKGREY);
    M5.Display.setCursor(20, y);
    M5.Display.setTextColor(envData.valid ? WHITE : DARKGREY, BLACK);
    if (envData.valid) {
      snprintf(line, sizeof(line), "%.1fC  %.1fhPa  alt:%.0fm         ",
               envData.temperature, envData.pressure, envData.altitude);
    } else {
      snprintf(line, sizeof(line), "BMP280 not ready                     ");
    }
    M5.Display.print(line);

    // --- NET ---
    y = 154;
    _sectionLabel(y, "NET");
    y += 12;
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
    M5.Display.setTextSize(1);
    M5.Display.setCursor(20, y);
    M5.Display.setTextColor(WHITE, BLACK);
    snprintf(line, sizeof(line), "TX ok:%-6lu fail:%-4lu",
             stats.sendSuccessCount, stats.sendFailCount);
    M5.Display.print(line);

    // --- BUF ---
    y = 196;
    _sectionLabel(y, "BUF");
    y += 10;
    M5.Display.setTextSize(1);
    M5.Display.setCursor(5, y);
    M5.Display.setTextColor(WHITE, BLACK);
    snprintf(line, sizeof(line), "%3d/%-3d", buffer.count(), buffer.capacity());
    M5.Display.print(line);

    int barX = 55;
    int barW = 255;
    int barH = 8;
    float ratio = (float)buffer.count() / buffer.capacity();
    uint16_t barColor = ratio > 0.9 ? RED : ratio > 0.7 ? YELLOW : GREEN;
    int fillW = (int)((barW - 2) * ratio);
    if (fillW < 0) fillW = 0;
    if (fillW > barW - 2) fillW = barW - 2;
    M5.Display.drawRect(barX, y, barW, barH, WHITE);
    if (fillW > 0)
      M5.Display.fillRect(barX + 1, y + 1, fillW, barH - 2, barColor);
    int emptyW = (barW - 2) - fillW;
    if (emptyW > 0)
      M5.Display.fillRect(barX + 1 + fillW, y + 1, emptyW, barH - 2, BLACK);
  }

  // ========== Page 1: Motion ==========

  void _drawMotion(const IMUData& imu) {
    // --- Accel XY radar chart with peak hold ---
    const int cx = 100, cy = 118, cr = 78;
    const float accelScale = 2.0f;  // +-2G maps to full radius

    // Init peak bins on first call
    if (!_peakInited) {
      for (int i = 0; i < PEAK_BINS; i++) _peakG[i] = 0.0f;
      _peakInited = true;
    }

    // Update peak: calculate angle and magnitude from accel XY
    float gXY = sqrt(imu.ax * imu.ax + imu.ay * imu.ay);
    if (gXY > _peakMaxG) _peakMaxG = gXY;

    if (gXY > 0.01f) {
      // atan2 returns -PI..PI, convert to 0..360
      float angleDeg = atan2(imu.ax, imu.ay) * 180.0f / PI;  // ax=X(right), ay=Y(up)
      if (angleDeg < 0) angleDeg += 360.0f;
      int bin = ((int)(angleDeg / (360.0f / PEAK_BINS))) % PEAK_BINS;
      if (gXY > _peakG[bin]) _peakG[bin] = gXY;
    }

    // Clear plot area
    M5.Display.fillRect(cx - cr - 2, cy - cr - 2, cr * 2 + 4, cr * 2 + 4, BLACK);

    // Reference circles
    M5.Display.drawCircle(cx, cy, cr, DARKGREY);
    M5.Display.drawCircle(cx, cy, cr / 2, 0x4208);  // 1G
    M5.Display.drawFastHLine(cx - cr, cy, cr * 2, 0x4208);
    M5.Display.drawFastVLine(cx, cy - cr, cr * 2, 0x4208);

    // Smooth peak data: wide kernel blur on circular buffer
    // First spread peaks to neighbors (fills gaps from sampling)
    // Then smooth for visual quality
    float smooth[PEAK_BINS];
    memcpy(smooth, _peakG, sizeof(smooth));

    // Pass 1-2: Spread — propagate peak values to +-2 neighbors (covers ~20°)
    for (int pass = 0; pass < 2; pass++) {
      float tmp[PEAK_BINS];
      for (int i = 0; i < PEAK_BINS; i++) {
        float v = smooth[i];
        for (int d = -2; d <= 2; d++) {
          float neighbor = smooth[(i + d + PEAK_BINS) % PEAK_BINS];
          if (neighbor > v) v = neighbor * 0.85f;  // spread at 85% strength
          if (v < smooth[i]) v = smooth[i];        // never below own peak
        }
        tmp[i] = v;
      }
      memcpy(smooth, tmp, sizeof(smooth));
    }

    // Pass 3-6: Gaussian blur with wider kernel [1,2,3,2,1]/9
    for (int pass = 0; pass < 4; pass++) {
      float tmp[PEAK_BINS];
      for (int i = 0; i < PEAK_BINS; i++) {
        float sum =
          1.0f * smooth[(i - 2 + PEAK_BINS) % PEAK_BINS] +
          2.0f * smooth[(i - 1 + PEAK_BINS) % PEAK_BINS] +
          3.0f * smooth[i] +
          2.0f * smooth[(i + 1) % PEAK_BINS] +
          1.0f * smooth[(i + 2) % PEAK_BINS];
        float blurred = sum / 9.0f;
        // Never shrink below actual recorded peak
        tmp[i] = max(_peakG[i], blurred);
      }
      memcpy(smooth, tmp, sizeof(smooth));
    }

    // Draw peak radar with Catmull-Rom spline on smoothed data
    const int SUB = 4;
    int prevRx = -1, prevRy = -1;
    int firstRx = 0, firstRy = 0;

    for (int i = 0; i <= PEAK_BINS; i++) {
      int i0 = (i - 1 + PEAK_BINS) % PEAK_BINS;
      int i1 = i % PEAK_BINS;
      int i2 = (i + 1) % PEAK_BINS;
      int i3 = (i + 2) % PEAK_BINS;

      int steps = (i < PEAK_BINS) ? SUB : 1;  // last iteration just closes loop
      for (int s = 0; s < steps; s++) {
        float t = (float)s / SUB;
        float t2 = t * t, t3 = t2 * t;
        float gI = 0.5f * (2.0f*smooth[i1]
                   + (-smooth[i0]+smooth[i2])*t
                   + (2.0f*smooth[i0]-5.0f*smooth[i1]+4.0f*smooth[i2]-smooth[i3])*t2
                   + (-smooth[i0]+3.0f*smooth[i1]-3.0f*smooth[i2]+smooth[i3])*t3);
        if (gI < 0) gI = 0;

        float angleDeg = ((float)i + (float)s / SUB) * (360.0f / PEAK_BINS);
        float angleRad = angleDeg * PI / 180.0f;
        float dist = (gI / accelScale) * cr;
        if (dist > cr) dist = cr;

        int rx = cx + (int)(dist * sin(angleRad));
        int ry = cy - (int)(dist * cos(angleRad));

        if (prevRx >= 0) {
          M5.Display.drawLine(prevRx, prevRy, rx, ry, YELLOW);
        } else {
          firstRx = rx; firstRy = ry;
        }
        prevRx = rx;
        prevRy = ry;
      }
    }
    // Close loop
    if (prevRx >= 0) {
      M5.Display.drawLine(prevRx, prevRy, firstRx, firstRy, YELLOW);
    }

    // Current position dot
    int px = cx + (int)(imu.ax / accelScale * cr);
    int py = cy - (int)(imu.ay / accelScale * cr);
    px = constrain(px, cx - cr + 3, cx + cr - 3);
    py = constrain(py, cy - cr + 3, cy + cr - 3);
    M5.Display.fillCircle(px, py, 3, GREEN);

    // Labels
    M5.Display.setTextSize(1);
    M5.Display.setTextColor(WHITE, BLACK);
    char line[40];

    M5.Display.setCursor(cx - cr, 26);
    snprintf(line, sizeof(line), "Accel XY (+-%.0fG)  ", accelScale);
    M5.Display.print(line);

    M5.Display.setCursor(cx - cr, cy + cr + 4);
    snprintf(line, sizeof(line), "Az:%+.2fg  Peak:%.2fG ", imu.az, _peakMaxG);
    M5.Display.print(line);

    // --- Gyro 3-axis bar graph ---
    const int barAreaX = 210;
    const int barW = 30;
    const int barMaxH = 70;  // pixels for max value
    const float gyroScale = 500.0f;  // +-500 deg/s
    const char* labels[] = {"Gx", "Gy", "Gz"};
    float gyro[] = {imu.gx, imu.gy, imu.gz};
    uint16_t colors[] = {RED, GREEN, 0x041F};  // R, G, B

    M5.Display.setTextSize(1);
    M5.Display.setCursor(barAreaX, 28);
    M5.Display.setTextColor(WHITE, BLACK);
    M5.Display.print("Gyro d/s");

    int barCenterY = 120;  // zero line

    for (int i = 0; i < 3; i++) {
      int bx = barAreaX + i * (barW + 4);

      // Clear bar area
      M5.Display.fillRect(bx, barCenterY - barMaxH, barW, barMaxH * 2, BLACK);

      // Zero line
      M5.Display.drawFastHLine(bx, barCenterY, barW, DARKGREY);

      // Draw bar
      float val = constrain(gyro[i], -gyroScale, gyroScale);
      int barH = (int)(abs(val) / gyroScale * barMaxH);
      if (barH > 0) {
        if (val > 0) {
          M5.Display.fillRect(bx + 2, barCenterY - barH, barW - 4, barH, colors[i]);
        } else {
          M5.Display.fillRect(bx + 2, barCenterY, barW - 4, barH, colors[i]);
        }
      }

      // Label + value
      M5.Display.setCursor(bx + 2, barCenterY + barMaxH + 4);
      M5.Display.setTextColor(colors[i], BLACK);
      snprintf(line, sizeof(line), "%s    ", labels[i]);
      M5.Display.print(line);

      M5.Display.setCursor(bx - 4, barCenterY + barMaxH + 14);
      M5.Display.setTextColor(WHITE, BLACK);
      snprintf(line, sizeof(line), "%+.0f  ", gyro[i]);
      M5.Display.print(line);
    }
  }

  // ========== Page 2: Trend ==========

  void _drawTrend(const TrendBuffer& trend) {
    if (trend.count() < 2) {
      M5.Display.setTextSize(2);
      M5.Display.setCursor(40, 100);
      M5.Display.setTextColor(DARKGREY, BLACK);
      M5.Display.print("Collecting data...");
      return;
    }

    // 3 graph rows: Accel, Gyro, Pressure
    // Each graph: 60px tall, full width (310px)
    struct GraphDef {
      const char* label;
      uint16_t colors[3];
      float TrendPoint::*fields[3];
      int fieldCount;
    };

    // We'll draw manually since we can't use pointer-to-member in arrays easily
    // Graph areas
    const int gx = 5, gw = 310;
    const int gh = 55;
    const int gy[] = {26, 90, 154};  // y positions for 3 graphs

    int n = trend.count();

    // --- Graph 1: Accelerometer ---
    _drawTrendGraph(trend, gy[0], gh, gw, "Accel(g)",
                    &TrendPoint::ax, &TrendPoint::ay, &TrendPoint::az,
                    RED, GREEN, 0x041F);

    // --- Graph 2: Gyroscope ---
    _drawTrendGraph(trend, gy[1], gh, gw, "Gyro(d/s)",
                    &TrendPoint::gx, &TrendPoint::gy, &TrendPoint::gz,
                    RED, GREEN, 0x041F);

    // --- Graph 3: Pressure + Temp ---
    _drawTrendSingle(trend, gy[2], gh, gw, "hPa",
                     &TrendPoint::pressure, YELLOW);
  }

  // Draw a 3-channel trend graph
  void _drawTrendGraph(const TrendBuffer& trend, int gy, int gh, int gw,
                       const char* label,
                       float TrendPoint::*f0, float TrendPoint::*f1, float TrendPoint::*f2,
                       uint16_t c0, uint16_t c1, uint16_t c2) {
    int n = trend.count();
    const int gx = 5;

    // Find range across all 3 channels
    float minV = 1e9, maxV = -1e9;
    for (int i = 0; i < n; i++) {
      const TrendPoint& p = trend.at(i);
      float vals[] = {p.*f0, p.*f1, p.*f2};
      for (int j = 0; j < 3; j++) {
        if (vals[j] < minV) minV = vals[j];
        if (vals[j] > maxV) maxV = vals[j];
      }
    }
    if (maxV - minV < 0.01f) { minV -= 1; maxV += 1; }

    // Clear graph area
    M5.Display.fillRect(gx, gy, gw, gh, BLACK);

    // Zero line if in range
    if (minV < 0 && maxV > 0) {
      int zy = gy + (int)(maxV / (maxV - minV) * (gh - 1));
      M5.Display.drawFastHLine(gx, zy, gw, 0x4208);
    }

    // Draw lines
    float TrendPoint::*fields[] = {f0, f1, f2};
    uint16_t colors[] = {c0, c1, c2};

    for (int ch = 0; ch < 3; ch++) {
      int prevX = -1, prevY = -1;
      for (int i = 0; i < n; i++) {
        int px = gx + (i * (gw - 1)) / (n > 1 ? n - 1 : 1);
        float v = trend.at(i).*(fields[ch]);
        int py = gy + (int)((maxV - v) / (maxV - minV) * (gh - 1));
        py = constrain(py, gy, gy + gh - 1);

        if (prevX >= 0) {
          M5.Display.drawLine(prevX, prevY, px, py, colors[ch]);
        }
        prevX = px;
        prevY = py;
      }
    }

    // Label
    M5.Display.setTextSize(1);
    M5.Display.setCursor(gx + 2, gy + 2);
    M5.Display.setTextColor(WHITE, BLACK);
    M5.Display.print(label);

    // Scale
    char buf[16];
    M5.Display.setCursor(gx + gw - 60, gy + 2);
    snprintf(buf, sizeof(buf), "%.1f", maxV);
    M5.Display.print(buf);
    M5.Display.setCursor(gx + gw - 60, gy + gh - 10);
    snprintf(buf, sizeof(buf), "%.1f", minV);
    M5.Display.print(buf);
  }

  // Draw a single-channel trend graph
  void _drawTrendSingle(const TrendBuffer& trend, int gy, int gh, int gw,
                        const char* label, float TrendPoint::*field,
                        uint16_t color) {
    int n = trend.count();
    const int gx = 5;

    float minV = 1e9, maxV = -1e9;
    for (int i = 0; i < n; i++) {
      float v = trend.at(i).*field;
      if (v < minV) minV = v;
      if (v > maxV) maxV = v;
    }
    if (maxV - minV < 0.01f) { minV -= 0.5f; maxV += 0.5f; }

    M5.Display.fillRect(gx, gy, gw, gh, BLACK);

    int prevX = -1, prevY = -1;
    for (int i = 0; i < n; i++) {
      int px = gx + (i * (gw - 1)) / (n > 1 ? n - 1 : 1);
      float v = trend.at(i).*field;
      int py = gy + (int)((maxV - v) / (maxV - minV) * (gh - 1));
      py = constrain(py, gy, gy + gh - 1);

      if (prevX >= 0) {
        M5.Display.drawLine(prevX, prevY, px, py, color);
      }
      prevX = px;
      prevY = py;
    }

    M5.Display.setTextSize(1);
    M5.Display.setCursor(gx + 2, gy + 2);
    M5.Display.setTextColor(WHITE, BLACK);
    M5.Display.print(label);

    char buf[16];
    M5.Display.setCursor(gx + gw - 60, gy + 2);
    snprintf(buf, sizeof(buf), "%.1f", maxV);
    M5.Display.print(buf);
    M5.Display.setCursor(gx + gw - 60, gy + gh - 10);
    snprintf(buf, sizeof(buf), "%.1f", minV);
    M5.Display.print(buf);
  }

  // ========== Skyplot ==========
  // Draws a polar satellite sky view.
  // cx,cy = top-left corner; r = radius
  void _drawSkyplot(int cx, int cy, int r, const SatData& sd) {
    int centerX = cx + r;
    int centerY = cy + r;

    // Clear area
    M5.Display.fillRect(cx, cy, r * 2 + 1, r * 2 + 1, BLACK);

    // Horizon circle + 45° ring + crosshair
    M5.Display.drawCircle(centerX, centerY, r, DARKGREY);
    M5.Display.drawCircle(centerX, centerY, r / 2, 0x4208);
    M5.Display.drawFastHLine(centerX - r, centerY, r * 2, 0x4208);
    M5.Display.drawFastVLine(centerX, centerY - r, r * 2, 0x4208);

    // NESW labels
    M5.Display.setTextSize(1);
    M5.Display.setTextColor(DARKGREY, BLACK);
    M5.Display.setCursor(centerX - 2, cy - 2);
    M5.Display.print("N");

    // Plot each satellite
    for (int i = 0; i < sd.count; i++) {
      const SatInfo& s = sd.sats[i];
      if (s.elevation > 90) continue;

      // Elevation: 90°=center, 0°=edge
      float dist = (float)(90 - s.elevation) / 90.0f * r;
      // Azimuth: 0°=N(up), 90°=E(right), clockwise
      float azRad = s.azimuth * PI / 180.0f;
      int sx = centerX + (int)(dist * sin(azRad));
      int sy = centerY - (int)(dist * cos(azRad));

      // Color by SNR: >=35=green, >=20=yellow, >0=red, 0=darkgrey
      uint16_t color;
      if (s.snr == 0)       color = 0x4208;   // dark grey — not tracking
      else if (s.snr >= 35) color = GREEN;
      else if (s.snr >= 20) color = YELLOW;
      else                  color = RED;

      M5.Display.fillCircle(sx, sy, 2, color);
    }

    // Satellite count
    M5.Display.setTextSize(1);
    M5.Display.setTextColor(WHITE, BLACK);
    char buf[8];
    snprintf(buf, sizeof(buf), "%d", sd.count);
    M5.Display.setCursor(cx, cy + r * 2 + 2);
    M5.Display.print(buf);
  }
};
