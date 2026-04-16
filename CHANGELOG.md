# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/).

## [0.3.0] - 2026-04-17

### Added

- Automatic gravity compensation with startup calibration (3-second static average)
- GravityCompensator: removes gravity vector and rotates to vehicle coordinate frame
- VehicleAccel struct for gravity-compensated forward/lateral/vertical G values
- Complementary filter for continuous gravity tracking (handles drift/bumps)
- Dashboard: vehicle G display row (F/L/U) with calibration status indicator
- Motion page: radar chart and current dot now use compensated vehicle G
- Trend page: first graph shows compensated vehicle G instead of raw accel

### Changed

- Forward axis reference: sensor Z (display normal) instead of sensor X
- Lateral axis: fwd x up cross product for correct right-positive convention
- IMUData: compensated fields moved to separate VehicleAccel (saves DRAM)

## [0.2.0] - 2026-04-17

### Added

- GNSS Module M135 support (NEO-M9N + BMI270 + BMM150 + BMP280)
- BMI270 IMU via M5Unified built-in driver (accelerometer + gyroscope)
- BMM150 magnetometer via BMI270 auxiliary I2C passthrough
- BMP280 barometric pressure and temperature via Adafruit_BMP280
- EnvProvider interface and BMP280EnvProvider / MockEnvProvider implementations
- 3-page display switchable with BtnA/BtnB/BtnC
- Dashboard page: satellite skyplot with SNR color coding
- Motion page: accel XY radar chart with per-direction peak hold
- Motion page: Catmull-Rom spline + Gaussian smoothing for radar outline
- Motion page: gyroscope 3-axis bar graphs
- Trend page: 120-second time-series graphs (accel, gyro, pressure)
- TrendBuffer ring buffer for graph history
- GSV sentence parser for satellite position data (all constellations)
- SatInfo / SatData structures for satellite tracking
- EnvData structure for temperature/pressure/altitude
- Magnetometer fields (mx, my, mz) in IMUData
- I2C bus scanner diagnostic on startup
- Serial re-initialization after M5.begin() for reliable debug output

### Changed

- GPS baud rate: 115200 (AT6668) -> 38400 (NEO-M9N)
- IMU default: mock -> real (BMI270 on GNSS Module)
- GPS provider comment updated for NEO-M9N compatibility
- MockSender now logs temperature and pressure
- config.example.json: mock.imu default changed to false

### Removed

- M5Module-GNSS library dependency (symbol conflict with M5Unified on ESP32 Core v3.x)
- TinyGPSCustom-based satellite count (replaced by GSV parser)

## [0.1.0] - 2026-04-14

### Added

- Initial scaffolding for M5Stack Basic v2.7 vehicle data logger
- GPS provider interface with TinyGPS++ and mock implementations
- IMU provider interface with mock implementation (MPU6886 placeholder)
- In-memory ring buffer (120 seconds capacity)
- Wi-Fi connection manager with exponential backoff retry
- Data sender interface with mock (Serial output) implementation
- HTTP sender placeholder with API key authentication
- Runtime configuration from TF card JSON file
- LCD status display: GPS, IMU, network, buffer stats with visual indicators
- Modular interface-based architecture for sensor/network swapping
