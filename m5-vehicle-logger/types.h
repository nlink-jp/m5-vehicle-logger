// types.h — Shared data types
#pragma once

#include <Arduino.h>

// GPS data point (1 Hz)
struct GPSData {
  double latitude;
  double longitude;
  double altitude;
  double speed;       // km/h
  double course;      // degrees
  uint8_t satellites;
  bool valid;
  char timestamp[24]; // ISO 8601: "2026-04-14T15:30:00Z"
};

// IMU data point (10 Hz) — stored in SensorRecord buffer
struct IMUData {
  float ax, ay, az;   // accelerometer raw (g) — sensor frame
  float gx, gy, gz;   // gyroscope (deg/s)
  float mx, my, mz;   // magnetometer (uT) — BMM150 via BMI270
  uint16_t offsetMs;   // offset from epoch start (0, 100, 200, ...)
};

// Gravity-compensated vehicle acceleration (display only, not buffered)
struct VehicleAccel {
  float fwd, lat, vert;  // vehicle frame (g)
  bool compensated;       // true after calibration complete
};

// Environment data point (1 Hz, sampled with GPS epoch)
struct EnvData {
  float temperature;   // degrees C — BMP280
  float pressure;      // hPa — BMP280
  float altitude;      // estimated altitude (m) from pressure
  bool valid;
};

// Combined record: 1 second of data
struct SensorRecord {
  GPSData gps;
  IMUData imu[10];     // IMU_SAMPLES_PER_RECORD
  uint8_t imuCount;    // actual number of IMU samples collected
  EnvData env;         // BMP280 environment data
  unsigned long epochMs; // millis() at record creation
};
