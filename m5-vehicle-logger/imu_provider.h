// imu_provider.h — IMU provider interface and implementations
#pragma once

#include "types.h"

// --- Interface ---
class IMUProvider {
public:
  virtual ~IMUProvider() {}
  virtual void begin() = 0;
  virtual bool read(IMUData& out) = 0;  // true if data available
};

// --- M5Unified IMU implementation (placeholder for real hardware) ---
// Uncomment when IMU hardware is available:
//
// #include <M5Unified.h>
// class M5IMUProvider : public IMUProvider {
// public:
//   void begin() override {
//     // M5Unified auto-detects and initializes IMU in M5.begin()
//     Serial.println("[IMU] M5Unified IMU initialized");
//   }
//   bool read(IMUData& out) override {
//     auto data = M5.Imu.getImuData();
//     out.ax = data.accel.x;
//     out.ay = data.accel.y;
//     out.az = data.accel.z;
//     out.gx = data.gyro.x;
//     out.gy = data.gyro.y;
//     out.gz = data.gyro.z;
//     out.offsetMs = 0;  // caller sets this
//     return true;
//   }
// };

// --- Mock implementation ---
class MockIMUProvider : public IMUProvider {
public:
  void begin() override {
    Serial.println("[MockIMU] initialized");
  }

  bool read(IMUData& out) override {
    // Simulate vehicle vibration + slight tilt
    unsigned long now = millis();
    float noise = (float)(now % 100) / 1000.0f;

    out.ax = 0.01f + noise;
    out.ay = -0.02f + noise * 0.5f;
    out.az = 9.81f + noise * 0.1f;
    out.gx = 0.5f + noise * 2.0f;
    out.gy = -0.3f + noise * 1.5f;
    out.gz = 0.1f + noise;
    out.offsetMs = 0;  // caller sets this

    return true;
  }
};
