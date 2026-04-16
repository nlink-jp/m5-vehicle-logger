// m5-vehicle-logger.ino — Vehicle data logger for M5Stack Basic v2.7
//
// Hardware: GNSS Module M135 (NEO-M9N + BMI270 + BMM150 + BMP280)
// Collects GPS (1 Hz), IMU (10 Hz), and environment (1 Hz) data,
// buffers in memory, and sends via Wi-Fi when connected.
// All sensor and network modules are interface-based for easy swapping.
//
// Display: 3 pages switchable with buttons A/B/C
//   A: Dashboard (all sensors overview)
//   B: Motion (accel XY plot + gyro bars)
//   C: Trend (time-series graphs)

#include <M5Unified.h>
#include "config.h"
#include "types.h"
#include "config_manager.h"
#include "gps_provider.h"
#include "imu_provider.h"
#include "data_buffer.h"
#include "network_manager.h"
#include "trend_buffer.h"
#include "display_manager.h"

// --- Globals ---
RuntimeConfig rtConfig;

// Providers (allocated in setup based on config)
GPSProvider* gps = nullptr;
IMUProvider* imu = nullptr;
EnvProvider* env = nullptr;
DataSender* sender = nullptr;

DataBuffer dataBuffer;
TrendBuffer trendBuffer;
WifiManager network;
DisplayManager display;

// Current epoch state
SensorRecord currentRecord;
GPSData lastGPSData;
EnvData lastEnvData;
IMUData lastIMUData;
unsigned long lastGPSMs = 0;
unsigned long lastIMUMs = 0;
unsigned long lastSendMs = 0;
uint8_t imuSampleIndex = 0;

// Scan I2C bus and print found devices
void scanI2C() {
  Serial.println("[I2C] scanning bus...");
  int found = 0;
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.printf("[I2C]   0x%02X found\n", addr);
      found++;
    }
  }
  Serial.printf("[I2C] scan done: %d devices\n", found);
}

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);

  // Re-init Serial after M5.begin() — M5Unified may reconfigure UART0
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.println("=== M5 Vehicle Logger ===");
  Serial.flush();

  // Diagnostic: show what's on the I2C bus
  Serial.println("[Main] starting I2C scan...");
  Serial.flush();
  scanI2C();

  // 1. Load config from TF card
  ConfigManager::load(rtConfig);

  // 2. Initialize GPS provider
  if (rtConfig.useMockGPS) {
    gps = new MockGPSProvider();
  } else {
    gps = new TinyGPSProvider(Serial2, GPS_TX_PIN, GPS_RX_PIN, GPS_BAUD);
  }
  gps->begin();
  Serial.printf("[Main] GPS: %s (baud=%d)\n",
                rtConfig.useMockGPS ? "mock" : "NEO-M9N", GPS_BAUD);

  // 3. Initialize IMU provider (BMI270 + BMM150)
  if (rtConfig.useMockIMU) {
    imu = new MockIMUProvider();
    env = new MockEnvProvider();
  } else {
    imu = new GNSSIMUProvider();
    env = new BMP280EnvProvider();
  }
  imu->begin();
  env->begin();
  Serial.printf("[Main] IMU: %s\n", rtConfig.useMockIMU ? "mock" : "BMI270+BMM150");
  Serial.printf("[Main] ENV: %s\n", rtConfig.useMockIMU ? "mock" : "BMP280");

  // 4. Initialize network
  if (rtConfig.useMockSender) {
    sender = new MockSender(rtConfig.endpoint, rtConfig.apiKey);
  } else {
    // sender = new HTTPSender(rtConfig.endpoint, rtConfig.apiKey);
    sender = new MockSender(rtConfig.endpoint, rtConfig.apiKey);
  }
  network.setSender(sender);
  network.begin(rtConfig.wifiSSID, rtConfig.wifiPassword);

  // 5. Initialize display
  display.begin();

  // 6. Initialize epoch
  memset(&currentRecord, 0, sizeof(currentRecord));
  memset(&lastGPSData, 0, sizeof(lastGPSData));
  memset(&lastEnvData, 0, sizeof(lastEnvData));
  memset(&lastIMUData, 0, sizeof(lastIMUData));
  imuSampleIndex = 0;
  lastGPSMs = millis();
  lastIMUMs = millis();

  Serial.println("[Main] setup complete");
}

void loop() {
  M5.update();
  unsigned long now = millis();

  // --- Button handling: page switch ---
  if (M5.BtnA.wasPressed()) display.setPage(PAGE_DASHBOARD);
  if (M5.BtnB.wasPressed()) display.setPage(PAGE_MOTION);
  if (M5.BtnC.wasPressed()) display.setPage(PAGE_TREND);

  // --- Feed GPS parser continuously ---
  if (gps) {
    gps->update();
  }

  // --- IMU sampling at 10 Hz ---
  if (now - lastIMUMs >= IMU_SAMPLE_INTERVAL_MS) {
    lastIMUMs = now;

    if (imu && imuSampleIndex < IMU_SAMPLES_PER_RECORD) {
      IMUData imuData;
      if (imu->read(imuData)) {
        imuData.offsetMs = imuSampleIndex * IMU_SAMPLE_INTERVAL_MS;
        currentRecord.imu[imuSampleIndex] = imuData;
        imuSampleIndex++;
        currentRecord.imuCount = imuSampleIndex;
        display.stats.imuReadCount++;
        lastIMUData = imuData;
      }
    }
  }

  // --- GPS + environment + epoch commit at 1 Hz ---
  if (now - lastGPSMs >= GPS_SAMPLE_INTERVAL_MS) {
    lastGPSMs = now;

    GPSData gpsData;
    memset(&gpsData, 0, sizeof(gpsData));
    if (gps && gps->read(gpsData)) {
      display.stats.gpsReadCount++;
    }
    currentRecord.gps = gpsData;
    currentRecord.epochMs = now;
    lastGPSData = gpsData;

    // Read environment sensor (BMP280) once per epoch
    EnvData envData;
    memset(&envData, 0, sizeof(envData));
    if (env) {
      env->read(envData);
    }
    currentRecord.env = envData;
    lastEnvData = envData;

    display.stats.lastIMUSamples = imuSampleIndex;

    // Push to data buffer (for network send)
    dataBuffer.push(currentRecord);

    // Push to trend buffer (for graph display)
    TrendPoint tp;
    tp.ax = lastIMUData.ax;  tp.ay = lastIMUData.ay;  tp.az = lastIMUData.az;
    tp.gx = lastIMUData.gx;  tp.gy = lastIMUData.gy;  tp.gz = lastIMUData.gz;
    tp.mx = lastIMUData.mx;  tp.my = lastIMUData.my;  tp.mz = lastIMUData.mz;
    tp.pressure = envData.pressure;
    tp.temperature = envData.temperature;
    trendBuffer.push(tp);

    memset(&currentRecord, 0, sizeof(currentRecord));
    imuSampleIndex = 0;
  }

  // --- Network update + data send ---
  network.update();

  if (network.isConnected() && !dataBuffer.isEmpty()) {
    if (now - lastSendMs >= 1000) {
      lastSendMs = now;
      SensorRecord batch[SEND_BATCH_SIZE];
      int count = dataBuffer.popBatch(batch, SEND_BATCH_SIZE);
      if (count > 0) {
        if (network.sendData(batch, count)) {
          display.stats.sendSuccessCount++;
        } else {
          display.stats.sendFailCount++;
          for (int i = 0; i < count; i++) {
            dataBuffer.push(batch[i]);
          }
        }
      }
    }
  }

  // --- Display update ---
  display.update(lastGPSData, gps->satellites(), lastIMUData, lastEnvData,
                 dataBuffer, network, trendBuffer);

  delay(1);
}
