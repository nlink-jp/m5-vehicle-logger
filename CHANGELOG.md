# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/).

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
