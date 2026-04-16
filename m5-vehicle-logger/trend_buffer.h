// trend_buffer.h — Ring buffer for trend graph display (120 seconds)
#pragma once

#include <Arduino.h>

#define TREND_SIZE 120  // 120 data points (1 per second at 1 Hz)

// One snapshot per second for trend display
struct TrendPoint {
  float ax, ay, az;       // accelerometer (g)
  float gx, gy, gz;       // gyroscope (deg/s)
  float mx, my, mz;       // magnetometer (uT)
  float pressure;          // hPa
  float temperature;       // C
};

class TrendBuffer {
public:
  TrendBuffer() : _head(0), _count(0) {}

  void push(const TrendPoint& pt) {
    _buf[_head] = pt;
    _head = (_head + 1) % TREND_SIZE;
    if (_count < TREND_SIZE) _count++;
  }

  // Get point by age: 0 = oldest, count()-1 = newest
  const TrendPoint& at(int index) const {
    int start = (_head - _count + TREND_SIZE) % TREND_SIZE;
    return _buf[(start + index) % TREND_SIZE];
  }

  int count() const { return _count; }

  // Get min/max for a given field across all points (for auto-scaling)
  void range(float TrendPoint::*field, float& minVal, float& maxVal) const {
    if (_count == 0) { minVal = 0; maxVal = 1; return; }
    minVal = maxVal = at(0).*field;
    for (int i = 1; i < _count; i++) {
      float v = at(i).*field;
      if (v < minVal) minVal = v;
      if (v > maxVal) maxVal = v;
    }
    // Ensure non-zero range
    if (maxVal - minVal < 0.001f) {
      minVal -= 0.5f;
      maxVal += 0.5f;
    }
  }

private:
  TrendPoint _buf[TREND_SIZE];
  int _head;
  int _count;
};
