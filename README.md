# m5-vehicle-logger

Vehicle driving data logger for M5Stack Basic v2.7 with GPS and IMU sensors.

Collects GPS position (1 Hz) and IMU accelerometer/gyroscope data (10 Hz),
buffers in memory, and transmits via Wi-Fi when connected. Designed for
vehicle-mounted operation without battery backup.

## Hardware

- **M5Stack Basic v2.7** (ESP32, LCD, TF card slot, no PSRAM)
- **M5 GPS Module v2.1** — AT6668 chip, **115200 baud**, TXD: G17, RXD: G16, PPS: G36
- **IMU** — MPU6886 or compatible (not yet connected; mock available)
- **External SMA antenna** — required for GPS reception

See [Hardware Notes](docs/en/hardware-notes.md) for detailed setup, gotchas, and troubleshooting.

## Features

- GPS tracking at 1 Hz (position, speed, altitude, heading, satellites)
- IMU sampling at 10 Hz (accelerometer + gyroscope)
- In-memory ring buffer (120 seconds of data)
- Wi-Fi auto-connect with exponential backoff retry
- Batch data transmission with re-queue on failure
- Runtime configuration via TF card JSON file
- LCD status display (GPS, IMU, network, buffer, send stats)
- Fully interface-based sensor/network modules for easy swapping

## Setup

### Arduino IDE

1. Install board: **M5Stack** via Board Manager
2. Install libraries:
   - `M5Stack`
   - `TinyGPSPlus`
   - `ArduinoJson`
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
    "imu": true,
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
| `mock.imu` | Use mock IMU data (true until hardware available) |
| `mock.sender` | Use mock sender (Serial output instead of HTTP) |

## Architecture

```
┌───────────────────────────────────┐
│            Main Loop              │
├───────────────────────────────────┤
│  SensorManager                    │
│  ├── GPSProvider (interface)      │
│  │   ├── TinyGPSProvider          │
│  │   └── MockGPSProvider          │
│  └── IMUProvider (interface)      │
│      ├── MPU6886Provider (TODO)   │
│      └── MockIMUProvider          │
├───────────────────────────────────┤
│  DataBuffer (ring buffer, 120s)   │
├───────────────────────────────────┤
│  NetworkManager                   │
│  ├── Wi-Fi (backoff retry)        │
│  └── DataSender (interface)       │
│      ├── HTTPSender (TODO)        │
│      └── MockSender               │
├───────────────────────────────────┤
│  ConfigManager (TF card JSON)     │
├───────────────────────────────────┤
│  DisplayManager (LCD status)      │
└───────────────────────────────────┘
```

## Data Format

Each second produces one record containing GPS + 10 IMU samples:

```json
{
  "ts": "2026-04-14T15:30:00Z",
  "gps": {
    "lat": 35.6812, "lng": 139.7671,
    "alt": 40.0, "speed": 45.2,
    "course": 180.0, "satellites": 8
  },
  "imu": [
    { "t": 0,   "ax": 0.01, "ay": -0.02, "az": 9.81, "gx": 0.5, "gy": -0.3, "gz": 0.1 },
    { "t": 100, "ax": 0.02, "ay": -0.01, "az": 9.80, "gx": 0.4, "gy": -0.2, "gz": 0.1 },
    ...
  ]
}
```

## License

See [LICENSE](LICENSE).
