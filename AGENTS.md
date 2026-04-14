# AGENTS.md — m5-vehicle-logger

## Project summary

Vehicle driving data logger using M5Stack Basic v2.7, GPS module, and IMU.
Arduino IDE project. Collects GPS (1 Hz) + IMU (10 Hz), buffers in memory,
sends via Wi-Fi. Part of lab-series.

## Development

- **IDE:** Arduino IDE
- **Board:** M5Stack-Core-ESP32
- **Libraries:** M5Stack, TinyGPSPlus, ArduinoJson

## Key structure

```
m5-vehicle-logger/
├── m5-vehicle-logger/
│   ├── m5-vehicle-logger.ino  ← main sketch
│   ├── config.h               ← pin assignments, sampling rates
│   ├── types.h                ← GPSData, IMUData, SensorRecord
│   ├── config_manager.h       ← TF card JSON config reader
│   ├── gps_provider.h         ← GPS interface + implementations
│   ├── imu_provider.h         ← IMU interface + implementations
│   ├── data_buffer.h          ← ring buffer
│   ├── network_manager.h      ← Wi-Fi + data sender
│   └── display_manager.h      ← LCD status display
└── sdcard/
    └── config.example.json    ← TF card config template
```

## Hardware

- M5Stack Basic v2.7 (ESP32)
- M5 GPS Module v2.1: TXD G17, RXD G16, PPS G36
- IMU: MPU6886 (planned, currently mock)

## Configuration

Runtime config read from TF card `/config.json`. Fields:
- `wifi.ssid`, `wifi.password` — vehicle AP credentials
- `endpoint`, `api_key` — data upload target
- `mock.gps`, `mock.imu`, `mock.sender` — toggle mock/real modules

## Gotchas

- No battery: TF card writes risk data loss on power cut, hence memory buffer + Wi-Fi
- GPS module may change: abstracted behind GPSProvider interface
- IMU hardware not yet available: using MockIMUProvider
- Cloud infra not ready: using MockSender (Serial output)
- ESP32 watchdog: main loop must yield (delay(1)) to prevent reset
