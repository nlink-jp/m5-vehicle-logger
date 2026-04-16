# m5-vehicle-logger

Vehicle driving data logger for M5Stack Basic v2.7 with GNSS, IMU, magnetometer,
and barometric pressure sensors.

Collects GPS position (1 Hz), IMU accelerometer/gyroscope/magnetometer data (10 Hz),
and environment data (1 Hz), buffers in memory, and transmits via Wi-Fi when connected.
Designed for vehicle-mounted operation without battery backup.

## Hardware

- **M5Stack Basic v2.7** (ESP32, 320x240 LCD, TF card slot, no PSRAM)
- **M5Stack GNSS Module (M135)** — stacked module with:
  - **NEO-M9N** — GNSS receiver (GPS/GLONASS/Galileo/BeiDou/QZSS), UART 38400 baud
  - **BMI270** — 6-axis IMU (accelerometer + gyroscope), I2C 0x68
  - **BMM150** — 3-axis magnetometer, via BMI270 auxiliary I2C
  - **BMP280** — barometric pressure + temperature, I2C 0x76
- **External SMA antenna** — required for GNSS reception

> **Note:** The M5Module-GNSS Arduino library has symbol conflicts with M5Unified
> on ESP32 Core v3.x. This project uses M5Unified's built-in BMI270 driver for
> accelerometer/gyroscope, Wire-based BMI270 auxiliary passthrough for BMM150, and
> Adafruit_BMP280 for the barometer.

See [Hardware Notes](docs/en/hardware-notes.md) for detailed setup and troubleshooting.

## Features

- GNSS tracking at 1 Hz (position, speed, altitude, heading, satellites)
- IMU sampling at 10 Hz (accelerometer + gyroscope + magnetometer)
- Barometric pressure and temperature at 1 Hz
- **Automatic gravity compensation** — mount at any angle, calibrates on startup
  - 3-second static calibration at boot (vehicle must be stationary)
  - Complementary filter for continuous gravity tracking
  - Outputs vehicle-frame G: forward, lateral, vertical (gravity-free)
- In-memory ring buffer (120 seconds of data)
- Wi-Fi auto-connect with exponential backoff retry
- Batch data transmission with re-queue on failure
- Runtime configuration via TF card JSON file
- **3-page LCD display** switchable with buttons:
  - **BtnA — Dashboard:** all sensors overview + satellite skyplot + vehicle G
  - **BtnB — Motion:** vehicle G radar chart with peak hold + gyro bar graphs
  - **BtnC — Trend:** 120-second time-series graphs (vehicle G, gyro, pressure)
- GSV sentence parser for satellite sky view (all constellations)
- Fully interface-based sensor/network modules for easy swapping

## Setup

### Arduino IDE

1. Install board: **M5Stack** via Board Manager (ESP32 Core v3.3.7+)
2. Install libraries:
   - `M5Unified`
   - `TinyGPSPlus`
   - `ArduinoJson`
   - `Adafruit BMP280`
3. Open `m5-vehicle-logger/m5-vehicle-logger.ino`
4. Select board: **M5Stack-Core-ESP32**
5. Upload

### TF Card Configuration

Copy `sdcard/config.example.json` to TF card as `/config.json` and edit:

```json
{
  "wifi": {
    "ssid": "VEHICLE-AP",
    "password": "your-password-here"
  },
  "endpoint": "http://example.com/api/data",
  "api_key": "your-api-key-here",
  "mock": {
    "gps": false,
    "imu": false,
    "sender": true
  }
}
```

| Field | Description |
|-------|-------------|
| `wifi.ssid` | Vehicle AP SSID |
| `wifi.password` | Wi-Fi password |
| `endpoint` | Data upload URL |
| `api_key` | API key for endpoint authentication |
| `mock.gps` | Use mock GPS data (for testing without module) |
| `mock.imu` | Use mock IMU/ENV data (for testing without GNSS module) |
| `mock.sender` | Use mock sender (Serial output instead of HTTP) |

## Architecture

```
┌───────────────────────────────────┐
│            Main Loop              │
├───────────────────────────────────┤
│  Sensors                          │
│  ├── GPSProvider (interface)      │
│  │   ├── TinyGPSProvider + GSV    │
│  │   └── MockGPSProvider          │
│  ├── IMUProvider (interface)      │
│  │   ├── GNSSIMUProvider          │
│  │   │   (M5Unified BMI270 +     │
│  │   │    BMM150 aux passthrough) │
│  │   └── MockIMUProvider          │
│  └── EnvProvider (interface)      │
│      ├── BMP280EnvProvider        │
│      └── MockEnvProvider          │
├───────────────────────────────────┤
│  DataBuffer (ring buffer, 120s)   │
│  TrendBuffer (graph history)      │
├───────────────────────────────────┤
│  WifiManager                      │
│  ├── Wi-Fi (backoff retry)        │
│  └── DataSender (interface)       │
│      ├── HTTPSender (TODO)        │
│      └── MockSender               │
├───────────────────────────────────┤
│  ConfigManager (TF card JSON)     │
├───────────────────────────────────┤
│  DisplayManager (3-page LCD)      │
│  ├── Dashboard + skyplot          │
│  ├── Motion (radar + bars)        │
│  └── Trend (time-series)          │
└───────────────────────────────────┘
```

## Display Pages

### Dashboard (BtnA)

All sensor values at a glance with satellite skyplot.

### Motion (BtnB)

- **Accel XY radar chart:** peak G per direction with Catmull-Rom spline + Gaussian smoothing
- **Gyro bar graphs:** 3-axis angular velocity with ±500 dps range

### Trend (BtnC)

120-second rolling time-series graphs for accelerometer, gyroscope, and pressure.

## Data Format

Each second produces one record containing GPS + 10 IMU samples + environment:

```json
{
  "ts": "2026-04-14T15:30:00Z",
  "gps": {
    "lat": 35.6812, "lng": 139.7671,
    "alt": 40.0, "speed": 45.2,
    "course": 180.0, "satellites": 8
  },
  "imu": [
    { "t": 0,   "ax": 0.01, "ay": -0.02, "az": 9.81, "gx": 0.5, "gy": -0.3, "gz": 0.1, "mx": 25.0, "my": -15.0, "mz": 40.0 },
    { "t": 100, "ax": 0.02, "ay": -0.01, "az": 9.80, "gx": 0.4, "gy": -0.2, "gz": 0.1, "mx": 25.1, "my": -14.9, "mz": 39.8 }
  ],
  "env": { "temperature": 25.3, "pressure": 1013.2, "altitude": 40.5 }
}
```

## License

See [LICENSE](LICENSE).
