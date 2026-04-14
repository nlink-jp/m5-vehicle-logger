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

// --- MPU6886 implementation (placeholder for real hardware) ---
// Uncomment and implement when IMU hardware is available:
//
// #include <M5Stack.h>
// class MPU6886Provider : public IMUProvider {
// public:
//   void begin() override { M5.IMU.Init(); }
//   bool read(IMUData& out) override {
//     M5.IMU.getAccelData(&out.ax, &out.ay, &out.az);
//     M5.IMU.getGyroData(&out.gx, &out.gy, &out.gz);
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
