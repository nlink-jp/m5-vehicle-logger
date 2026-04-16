// gravity_compensator.h — Gravity removal and vehicle coordinate transform
//
// Regardless of how the M5Stack is mounted (tilted, upright, etc.),
// this module removes the gravity component from accelerometer readings
// and rotates the result into a consistent vehicle coordinate frame:
//   forward = sensor X projected onto horizontal plane
//   lateral = perpendicular to forward in horizontal plane
//   vertical = opposite to gravity (up)
//
// Calibration: average first N seconds of data (vehicle must be stationary).
// Runtime: complementary filter slowly tracks gravity to handle drift/bumps.
#pragma once

#include <Arduino.h>
#include <math.h>

class GravityCompensator {
public:
  // calibrationMs: how long to average at startup (default 3 seconds)
  GravityCompensator(unsigned long calibrationMs = 3000)
    : _calMs(calibrationMs) {}

  // Feed raw accelerometer data. Call at every IMU sample (10 Hz).
  // Returns true when calibration is complete and output is valid.
  bool update(float ax, float ay, float az,
              float &outFwd, float &outLat, float &outVert) {

    if (!_calibrated) {
      // --- Calibration phase: accumulate gravity samples ---
      _sumX += ax;
      _sumY += ay;
      _sumZ += az;
      _calCount++;

      unsigned long now = millis();
      if (_calStart == 0) _calStart = now;

      if (now - _calStart >= _calMs && _calCount > 0) {
        // Compute average gravity vector
        _gravX = _sumX / _calCount;
        _gravY = _sumY / _calCount;
        _gravZ = _sumZ / _calCount;

        _buildRotationMatrix();
        _calibrated = true;

        Serial.printf("[GRAV] calibrated: g=(%.3f, %.3f, %.3f) %d samples\n",
                      _gravX, _gravY, _gravZ, _calCount);
        Serial.printf("[GRAV] fwd=(%.3f, %.3f, %.3f)\n", _fwdX, _fwdY, _fwdZ);
        Serial.printf("[GRAV] lat=(%.3f, %.3f, %.3f)\n", _latX, _latY, _latZ);
        Serial.printf("[GRAV] up =(%.3f, %.3f, %.3f)\n", _upX, _upY, _upZ);
      }

      // Output zeros during calibration
      outFwd = outLat = outVert = 0.0f;
      return false;
    }

    // --- Runtime: complementary filter for gravity tracking ---
    // Slowly blend accelerometer into gravity estimate (handles drift/bumps)
    // Alpha close to 1 = slow tracking; 0.995 at 10Hz ≈ 20s time constant
    const float alpha = 0.995f;
    _gravX = alpha * _gravX + (1.0f - alpha) * ax;
    _gravY = alpha * _gravY + (1.0f - alpha) * ay;
    _gravZ = alpha * _gravZ + (1.0f - alpha) * az;

    // Rebuild rotation matrix periodically (every ~1 second)
    _updateCounter++;
    if (_updateCounter >= 10) {
      _updateCounter = 0;
      _buildRotationMatrix();
    }

    // Remove gravity
    float dynX = ax - _gravX;
    float dynY = ay - _gravY;
    float dynZ = az - _gravZ;

    // Rotate to vehicle frame (dot products with basis vectors)
    outFwd  = dynX * _fwdX + dynY * _fwdY + dynZ * _fwdZ;
    outLat  = dynX * _latX + dynY * _latY + dynZ * _latZ;
    outVert = dynX * _upX  + dynY * _upY  + dynZ * _upZ;

    return true;
  }

  bool isCalibrated() const { return _calibrated; }

  // Get current gravity estimate (for display/debug)
  void getGravity(float &gx, float &gy, float &gz) const {
    gx = _gravX; gy = _gravY; gz = _gravZ;
  }

  // Get mounting tilt angle in degrees (0 = flat, 90 = vertical)
  float getTiltDeg() const {
    if (!_calibrated) return 0;
    float gMag = sqrt(_gravX*_gravX + _gravY*_gravY + _gravZ*_gravZ);
    if (gMag < 0.01f) return 0;
    // Angle between gravity vector and sensor Z axis
    float cosAngle = _gravZ / gMag;
    return acos(constrain(cosAngle, -1.0f, 1.0f)) * 180.0f / PI;
  }

private:
  unsigned long _calMs;
  unsigned long _calStart = 0;
  int _calCount = 0;
  float _sumX = 0, _sumY = 0, _sumZ = 0;
  bool _calibrated = false;
  int _updateCounter = 0;

  // Gravity estimate in sensor frame
  float _gravX = 0, _gravY = 0, _gravZ = 0;

  // Vehicle coordinate basis vectors (in sensor frame)
  // forward: sensor X projected to horizontal plane
  float _fwdX = 1, _fwdY = 0, _fwdZ = 0;
  // lateral: perpendicular to forward in horizontal plane
  float _latX = 0, _latY = 1, _latZ = 0;
  // up: opposite to gravity
  float _upX = 0, _upY = 0, _upZ = 1;

  void _buildRotationMatrix() {
    // Normalize gravity to get "down" direction
    float gMag = sqrt(_gravX*_gravX + _gravY*_gravY + _gravZ*_gravZ);
    if (gMag < 0.01f) return;  // no valid gravity

    // "up" = opposite of gravity (normalized)
    _upX = -_gravX / gMag;
    _upY = -_gravY / gMag;
    _upZ = -_gravZ / gMag;

    // "forward" = sensor Z axis (display normal) projected onto horizontal plane
    // This aligns "forward" with the direction the display faces.
    // sensorZ = (0, 0, 1)
    float dotZUp = _upZ;  // dot((0,0,1), up) = up.z
    float projX = 0.0f - dotZUp * _upX;
    float projY = 0.0f - dotZUp * _upY;
    float projZ = 1.0f - dotZUp * _upZ;

    float projMag = sqrt(projX*projX + projY*projY + projZ*projZ);

    if (projMag < 0.01f) {
      // Sensor Z is nearly parallel to gravity (display facing up/down)
      // Fall back to sensor Y as forward
      float dotYUp = _upY;
      projX = 0.0f - dotYUp * _upX;
      projY = 1.0f - dotYUp * _upY;
      projZ = 0.0f - dotYUp * _upZ;
      projMag = sqrt(projX*projX + projY*projY + projZ*projZ);
    }

    if (projMag < 0.01f) return;  // degenerate case

    _fwdX = projX / projMag;
    _fwdY = projY / projMag;
    _fwdZ = projZ / projMag;

    // "lateral" = forward x up (positive = right when facing forward)
    _latX = _fwdY * _upZ - _fwdZ * _upY;
    _latY = _fwdZ * _upX - _fwdX * _upZ;
    _latZ = _fwdX * _upY - _fwdY * _upX;
  }
};
