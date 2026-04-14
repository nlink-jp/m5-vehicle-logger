// m5-vehicle-logger.ino — Vehicle data logger for M5Stack Basic v2.7
//
// Collects GPS (1 Hz) and IMU (10 Hz) data, buffers in memory,
// and sends via Wi-Fi when connected. All sensor and network
// modules are interface-based for easy swapping.

#include <M5Unified.h>
#include "config.h"
#include "types.h"
#include "config_manager.h"
#include "gps_provider.h"
#include "imu_provider.h"
#include "data_buffer.h"
#include "network_manager.h"
#include "display_manager.h"

// --- Globals ---
RuntimeConfig rtConfig;

// Providers (allocated in setup based on config)
GPSProvider* gps = nullptr;
IMUProvider* imu = nullptr;
DataSender* sender = nullptr;

DataBuffer dataBuffer;
WifiManager network;
DisplayManager display;

// Current epoch state
SensorRecord currentRecord;
GPSData lastGPSData;
unsigned long lastGPSMs = 0;
unsigned long lastIMUMs = 0;
unsigned long lastSendMs = 0;
uint8_t imuSampleIndex = 0;

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  Serial.println("=== M5 Vehicle Logger ===");

  // 1. Load config from TF card
  ConfigManager::load(rtConfig);

  // 2. Initialize GPS provider
  if (rtConfig.useMockGPS) {
    gps = new MockGPSProvider();
  } else {
    gps = new TinyGPSProvider(Serial2, GPS_TX_PIN, GPS_RX_PIN, GPS_BAUD);
  }
  gps->begin();
  Serial.printf("[Main] GPS: %s\n", rtConfig.useMockGPS ? "mock" : "AT6668");

  // 3. Initialize IMU provider
  if (rtConfig.useMockIMU) {
    imu = new MockIMUProvider();
  } else {
    // imu = new M5IMUProvider();
    imu = new MockIMUProvider();
  }
  imu->begin();
  Serial.printf("[Main] IMU: %s\n", rtConfig.useMockIMU ? "mock" : "M5Unified");

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
  imuSampleIndex = 0;
  lastGPSMs = millis();
  lastIMUMs = millis();

  Serial.println("[Main] setup complete");
}

void loop() {
  M5.update();
  unsigned long now = millis();

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
      }
    }
  }

  // --- GPS + epoch commit at 1 Hz ---
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

    display.stats.lastIMUSamples = imuSampleIndex;

    dataBuffer.push(currentRecord);

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
  display.update(lastGPSData, dataBuffer, network);

  delay(1);
}
