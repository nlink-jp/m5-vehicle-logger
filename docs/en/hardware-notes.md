# Hardware Notes

Notes from initial hardware setup and debugging. Intended to save time
for anyone assembling the same hardware stack.

## Module Stack

```
┌─────────────────────┐
│  M5Stack Basic v2.7 │  ESP32, LCD 320x240, TF card, 3 buttons
│  (top)              │
├─────────────────────┤
│  M5 GPS Module v2.1 │  AT6668 GNSS, external SMA antenna
│  (bottom)           │  Connected via stack connector (bottom pins)
└─────────────────────┘
```

## GPS Module: M5 GPS Module v2.1

### Chip

- **AT6668** (not AT6558 as in the older v2.0)
- Multi-constellation: GPS, BDS (BD2/BD3), GLONASS, Galileo, QZSS
- Default output: **NMEA 0183**
- Default baud rate: **115200** (not 9600)

> **Important:** Many online references for "M5 GPS Module" assume AT6558
> at 9600 baud. The v2.1 with AT6668 uses 115200. Using 9600 will
> produce garbled data that looks like a binary protocol.

### DIP Switch

The module has a DIP switch to select TX/RX pin mapping. For M5Stack
Basic v2.7 via stack connector, set:

| Switch | Setting |
|--------|---------|
| TXD    | G17     |
| RXD    | G16     |
| PPS    | G36     |

### ESP32 Serial Configuration

ESP32 `Serial2.begin()` parameters:

```cpp
// RX pin = G16 (receives from GPS TXD)
// TX pin = G17 (sends to GPS RXD)
Serial2.begin(115200, SERIAL_8N1, 16, 17);
```

> **Gotcha:** `M5.begin()` (M5Unified) may internally initialize Serial2.
> The GPS provider must call `Serial2.end()` before re-initializing with
> the correct pins and baud rate.

### Antenna

- **External SMA antenna required.** The module has no internal antenna.
- Without an antenna connected, the module will not receive any satellite signals.
- Position the antenna with a **clear view of the sky** — not behind walls
  or under metal roofs. Window placement works but may limit visibility.

### Cold Start Timing

| Start type | Condition | Time to first fix |
|-----------|-----------|-------------------|
| Cold start | First power-on, no stored almanac | 5–15 minutes |
| Warm start | Almanac available, ephemeris expired | 30–60 seconds |
| Hot start  | Recent fix, short power interruption | 1–5 seconds |

- During cold start, the module outputs NMEA sentences but with empty
  position fields.
- **Satellites in view** (from GSV sentences) will appear before a
  position fix is obtained. The display shows `no fix N sat` during
  acquisition.
- A position fix requires **4 or more satellites**.

### NMEA Sentences

The AT6668 outputs the following NMEA sentences by default:

| Sentence | Content |
|----------|---------|
| `$GNGGA` | Position, altitude, satellite count, fix quality |
| `$GNGLL` | Position, time |
| `$GNGSA` | DOP and active satellites |
| `$GPGSV` / `$BDGSV` / `$GLGSV` | Satellites in view (per constellation) |
| `$GNRMC` | Position, speed, course, date |
| `$GNVTG` | Course and speed over ground |

> Note: AT6668 uses `GN` prefix (multi-constellation) for most sentences,
> and constellation-specific prefixes (`GP`, `BD`, `GL`) for GSV.

### Troubleshooting

| Symptom | Likely cause | Fix |
|---------|-------------|-----|
| No data at all (0 bytes) | Wrong baud rate or pins | Verify 115200, check DIP switch |
| Binary/garbled data | Baud rate mismatch | Change to 115200 |
| NMEA received but `no fix searching...` | No antenna or poor sky view | Connect antenna, move outdoors |
| `no fix N sat` but no fix | Insufficient satellites (< 4) | Wait or improve antenna placement |
| Data received only before `M5.begin()` | Serial2 conflict | Call `Serial2.end()` before re-init |

## M5Stack Basic v2.7

### Key Differences from v2.6

- Stack connector UART pins may differ from older documentation.
  Always verify with actual hardware.
- **No PSRAM** — `M5Canvas::createSprite(320, 240)` (150 KB) will
  fail silently. Use direct LCD drawing with background-color text
  overwrite instead of sprites.

### M5Unified Library

- Use **M5Unified** (not the legacy M5Stack library).
- Legacy M5Stack library (v0.4.6) is incompatible with ESP32 board
  package v3.x (`rom/miniz.h` missing).
- M5Unified API differences:
  - `M5.Lcd` → `M5.Display`
  - `M5.begin(lcd, sd, serial, i2c)` → `auto cfg = M5.config(); M5.begin(cfg);`
  - `TFT_eSprite` → `M5Canvas` (but avoid large sprites without PSRAM)
  - SD initialized automatically by `M5.begin()`

### ESP32 Board Package

- Use **M5Stack board package v2.1.2** (if using legacy M5Stack library)
- Use **v3.3.7+** with M5Unified

### Class Name Conflict

ESP32 Arduino Core v3.x defines a `NetworkManager` class. Avoid using
this name in your code. This project uses `WifiManager` instead.

## IMU (Planned)

- MPU6886 or compatible, via M5Unified `M5.Imu` API
- Not yet connected; using mock implementation
- M5Unified auto-detects the IMU chip on `M5.begin()`
