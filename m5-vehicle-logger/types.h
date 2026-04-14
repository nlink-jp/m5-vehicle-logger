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

// IMU data point (10 Hz)
struct IMUData {
  float ax, ay, az;   // accelerometer (g)
  float gx, gy, gz;   // gyroscope (deg/s)
  uint16_t offsetMs;   // offset from epoch start (0, 100, 200, ...)
};

// Combined record: 1 second of data
struct SensorRecord {
  GPSData gps;
  IMUData imu[10];     // IMU_SAMPLES_PER_RECORD
  uint8_t imuCount;    // actual number of IMU samples collected
  unsigned long epochMs; // millis() at record creation
};
