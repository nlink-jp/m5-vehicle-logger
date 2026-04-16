# AGENTS.md — m5-vehicle-logger

## Project summary

Vehicle driving data logger using M5Stack Basic v2.7 with GNSS Module M135
(NEO-M9N + BMI270 + BMM150 + BMP280). Arduino IDE project. Collects GPS (1 Hz) +
IMU (10 Hz) + environment (1 Hz), buffers in memory, sends via Wi-Fi.
Part of lab-series.

## Development

- **IDE:** Arduino IDE
- **Board:** M5Stack-Core-ESP32 (ESP32 Core v3.3.7+)
- **Libraries:** M5Unified, TinyGPSPlus, ArduinoJson, Adafruit BMP280

## Key structure

```
m5-vehicle-logger/
├── m5-vehicle-logger/
│   ├── m5-vehicle-logger.ino  ← main sketch (setup, loop, button handling)
│   ├── config.h               ← pin assignments, I2C addresses, sampling rates
│   ├── types.h                ← GPSData, IMUData, EnvData, SensorRecord
│   ├── config_manager.h       ← TF card JSON config reader
│   ├── gps_provider.h         ← GPS interface + TinyGPS++ + GSV parser
│   ├── imu_provider.h         ← IMU/ENV interfaces + BMI270/BMM150/BMP280
│   ├── data_buffer.h          ← ring buffer for network send
│   ├── trend_buffer.h         ← ring buffer for trend graphs
│   ├── network_manager.h      ← Wi-Fi + data sender
│   └── display_manager.h      ← 3-page LCD (dashboard, motion, trend)
└── sdcard/
    └── config.example.json    ← TF card config template
```

## Hardware

- M5Stack Basic v2.7 (ESP32, no PSRAM)
- GNSS Module M135: NEO-M9N (UART 38400), BMI270 (I2C 0x68), BMM150 (aux), BMP280 (I2C 0x76)
- NEO-M9N I2C: 0x75 (UBX protocol, not used)
- External SMA antenna required

## Configuration

Runtime config read from TF card `/config.json`. Fields:
- `wifi.ssid`, `wifi.password` — vehicle AP credentials
- `endpoint`, `api_key` — data upload target
- `mock.gps`, `mock.imu`, `mock.sender` — toggle mock/real modules

## Display pages

- **BtnA — Dashboard:** GPS + skyplot + IMU 9-axis + ENV + network + buffer
- **BtnB — Motion:** accel XY radar chart (peak hold, spline smoothed) + gyro bars
- **BtnC — Trend:** 120s time-series (accel, gyro, pressure)

## Gotchas

- **M5Module-GNSS library is DISABLED** — its Bosch BMI270 API conflicts with M5Unified's
  internal IMU driver, causing M5.begin() to hang. Renamed to .disabled in Arduino libraries.
- **Do NOT call Wire.begin() after M5.begin()** — ESP32 Core v3.x blocks on double-init
- BMM150 is on BMI270's secondary I2C — invisible to host I2C scan
- BMI270 detected by M5Unified as type=6; must call M5.Imu.update() before getImuData()
- No PSRAM: cannot use full-screen Sprite; direct draw with background color overwrite
- No battery: TF card writes risk data loss on power cut, hence memory buffer + Wi-Fi
- ESP32 watchdog: main loop must yield (delay(1)) to prevent reset
