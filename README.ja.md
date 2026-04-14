# m5-vehicle-logger

M5Stack Basic v2.7 + GPS + IMU による車両走行データロガー。

GPS位置情報（1Hz）とIMU加速度/ジャイロデータ（10Hz）を収集し、メモリにバッファリングしてWi-Fi接続時に送信する。バッテリ非搭載の車載運用を想定。

## ハードウェア

- **M5Stack Basic v2.7**（ESP32、LCD、TFカードスロット）
- **M5 GPS Module v2.1** — TXD: G17, RXD: G16, PPS: G36
- **IMU** — MPU6886 または互換品（未接続、モック利用可）

## 機能

- GPS 1Hz トラッキング（位置、速度、高度、方位、衛星数）
- IMU 10Hz サンプリング（加速度 + ジャイロ）
- メモリリングバッファ（120秒分）
- Wi-Fi自動接続（指数バックオフリトライ）
- バッチデータ送信（失敗時リキュー）
- TFカードJSON設定ファイルによるランタイム設定
- LCDステータス表示（GPS、IMU、ネットワーク、バッファ、送信統計）
- インターフェースベースのセンサー/ネットワークモジュール（差し替え容易）

## セットアップ

### Arduino IDE

1. ボードインストール: Board Managerから **M5Stack**
2. ライブラリインストール:
   - `M5Stack`
   - `TinyGPSPlus`
   - `ArduinoJson`
3. `m5-vehicle-logger/m5-vehicle-logger.ino` を開く
4. ボード選択: **M5Stack-Core-ESP32**
5. アップロード

### TFカード設定

`sdcard/config.example.json` を TFカードに `/config.json` としてコピーし編集：

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

| フィールド | 説明 |
|-----------|------|
| `wifi.ssid` | 車両AP SSID |
| `wifi.password` | Wi-Fiパスワード |
| `endpoint` | データ送信先URL |
| `api_key` | エンドポイント認証用APIキー |
| `mock.gps` | モックGPSデータを使用（モジュールなしテスト用） |
| `mock.imu` | モックIMUデータを使用（ハードウェア到着まで true） |
| `mock.sender` | モック送信を使用（HTTPの代わりにSerial出力） |

## アーキテクチャ

```
┌───────────────────────────────────┐
│            メインループ              │
├───────────────────────────────────┤
│  センサーマネージャ                   │
│  ├── GPSProvider（インターフェース）   │
│  │   ├── TinyGPSProvider            │
│  │   └── MockGPSProvider            │
│  └── IMUProvider（インターフェース）   │
│      ├── MPU6886Provider（TODO）     │
│      └── MockIMUProvider            │
├───────────────────────────────────┤
│  DataBuffer（リングバッファ、120秒）  │
├───────────────────────────────────┤
│  NetworkManager                     │
│  ├── Wi-Fi（バックオフリトライ）      │
│  └── DataSender（インターフェース）   │
│      ├── HTTPSender（TODO）          │
│      └── MockSender                 │
├───────────────────────────────────┤
│  ConfigManager（TFカードJSON）       │
├───────────────────────────────────┤
│  DisplayManager（LCDステータス）     │
└───────────────────────────────────┘
```

## データ形式

1秒ごとにGPS 1件 + IMU 10件の複合レコードを生成：

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

## ライセンス

[LICENSE](LICENSE)を参照。
