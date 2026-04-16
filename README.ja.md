# m5-vehicle-logger

M5Stack Basic v2.7 + GNSSモジュール(M135)による車両走行データロガー。

GNSS位置情報（1Hz）、IMU加速度/ジャイロ/磁気データ（10Hz）、気圧/温度データ（1Hz）を収集し、メモリにバッファリングしてWi-Fi接続時に送信する。バッテリ非搭載の車載運用を想定。

## ハードウェア

- **M5Stack Basic v2.7**（ESP32、320x240 LCD、TFカードスロット、PSRAM非搭載）
- **M5Stack GNSSモジュール (M135)** — スタック型モジュール:
  - **NEO-M9N** — GNSSレシーバ（GPS/GLONASS/Galileo/BeiDou/QZSS）、UART 38400 baud
  - **BMI270** — 6軸IMU（加速度＋ジャイロ）、I2C 0x68
  - **BMM150** — 3軸磁気センサ、BMI270セカンダリI2C経由
  - **BMP280** — 気圧＋温度センサ、I2C 0x76
- **外部SMAアンテナ** — GNSS受信に必須

> **注意:** M5Module-GNSS Arduinoライブラリは ESP32 Core v3.x の M5Unified と
> シンボル競合するため使用不可。本プロジェクトでは M5Unified 内蔵 BMI270 ドライバ、
> Wire直接の BMM150 AUXパススルー、Adafruit_BMP280 を使用。

詳細なセットアップ、注意点、トラブルシューティングは[ハードウェアノート](docs/ja/hardware-notes.ja.md)を参照。

## 機能

- GNSS 1Hz トラッキング（位置、速度、高度、方位、衛星数）
- IMU 10Hz サンプリング（加速度 + ジャイロ + 磁気）
- 気圧・温度 1Hz サンプリング
- メモリリングバッファ（120秒分）
- Wi-Fi自動接続（指数バックオフリトライ）
- バッチデータ送信（失敗時リキュー）
- TFカードJSON設定ファイルによるランタイム設定
- **重力自動補正** — 任意の角度で設置可能、起動時に自動キャリブレーション
  - 起動後3秒の静止データで重力ベクトルを特定（車両静止が必要）
  - 相補フィルタによる重力追従（設置ずれ・振動に対応）
  - 車両座標系G出力: 前後(Forward)、左右(Lateral)、上下(Vertical)
- **3画面LCD表示**（ボタン切替）:
  - **BtnA — ダッシュボード:** 全センサー概要 + 衛星配置図(skyplot) + 車両G
  - **BtnB — モーション:** 車両Gレーダーチャート(ピークホールド) + ジャイロバーグラフ
  - **BtnC — トレンド:** 120秒時系列グラフ（車両G、ジャイロ、気圧）
- GSV文パーサーによる衛星配置表示（全コンステレーション対応）
- インターフェースベースのセンサー/ネットワークモジュール（差し替え容易）

## セットアップ

### Arduino IDE

1. ボードインストール: Board Managerから **M5Stack**（ESP32 Core v3.3.7以上）
2. ライブラリインストール:
   - `M5Unified`
   - `TinyGPSPlus`
   - `ArduinoJson`
   - `Adafruit BMP280`
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
    "imu": false,
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
| `mock.imu` | モックIMU/ENVデータを使用（GNSSモジュールなしテスト用） |
| `mock.sender` | モック送信を使用（HTTPの代わりにSerial出力） |

## アーキテクチャ

```
┌───────────────────────────────────┐
│            メインループ              │
├───────────────────────────────────┤
│  センサー                           │
│  ├── GPSProvider（インターフェース）   │
│  │   ├── TinyGPSProvider + GSV     │
│  │   └── MockGPSProvider           │
│  ├── IMUProvider（インターフェース）   │
│  │   ├── GNSSIMUProvider           │
│  │   │   (M5Unified BMI270 +      │
│  │   │    BMM150 AUXパススルー)     │
│  │   └── MockIMUProvider           │
│  └── EnvProvider（インターフェース）   │
│      ├── BMP280EnvProvider         │
│      └── MockEnvProvider           │
├───────────────────────────────────┤
│  DataBuffer（リングバッファ、120秒）  │
│  TrendBuffer（グラフ用履歴）         │
├───────────────────────────────────┤
│  WifiManager                       │
│  ├── Wi-Fi（バックオフリトライ）      │
│  └── DataSender（インターフェース）   │
│      ├── HTTPSender（TODO）         │
│      └── MockSender                │
├───────────────────────────────────┤
│  ConfigManager（TFカードJSON）      │
├───────────────────────────────────┤
│  DisplayManager（3画面LCD）         │
│  ├── ダッシュボード + skyplot        │
│  ├── モーション（レーダー + バー）    │
│  └── トレンド（時系列グラフ）        │
└───────────────────────────────────┘
```

## 画面表示

### ダッシュボード (BtnA)

全センサー値を一覧表示。衛星配置図(skyplot)付き。

### モーション (BtnB)

- **加速度XYレーダーチャート:** 方向別ピークG値をCatmull-Romスプライン + ガウシアン平滑化で表示
- **ジャイロバーグラフ:** 3軸角速度を±500 dps範囲で表示

### トレンド (BtnC)

直近120秒の加速度・ジャイロ・気圧を折れ線グラフで時系列表示。

## データ形式

1秒ごとにGPS 1件 + IMU 10件 + 環境データの複合レコードを生成：

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

## ライセンス

[LICENSE](LICENSE)を参照。
