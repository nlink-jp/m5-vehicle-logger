// m5-vehicle-logger.ino — Vehicle data logger for M5Stack Basic v2.7
//
// Collects GPS (1 Hz) and IMU (10 Hz) data, buffers in memory,
// and sends via Wi-Fi when connected. All sensor and network
// modules are interface-based for easy swapping.

#include <M5Stack.h>
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
NetworkManager network;
DisplayManager display;

// Current epoch state
SensorRecord currentRecord;
GPSData lastGPSData; // retain last valid GPS for display between epochs
unsigned long lastGPSMs = 0;
unsigned long lastIMUMs = 0;
unsigned long lastSendMs = 0;
uint8_t imuSampleIndex = 0;

void setup() {
  M5.begin(true, true, true, false); // LCD, SD, Serial, I2C
  Serial.begin(115200);
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
  Serial.printf("[Main] GPS: %s\n", rtConfig.useMockGPS ? "mock" : "TinyGPS++");

  // 3. Initialize IMU provider
  if (rtConfig.useMockIMU) {
    imu = new MockIMUProvider();
  } else {
    // When real IMU is available:
    // imu = new MPU6886Provider();
    imu = new MockIMUProvider(); // fallback
  }
  imu->begin();
  Serial.printf("[Main] IMU: %s\n", rtConfig.useMockIMU ? "mock" : "MPU6886");

  // 4. Initialize network
  if (rtConfig.useMockSender) {
    sender = new MockSender(rtConfig.endpoint, rtConfig.apiKey);
  } else {
    // When cloud is ready:
    // sender = new HTTPSender(rtConfig.endpoint, rtConfig.apiKey);
    sender = new MockSender(rtConfig.endpoint, rtConfig.apiKey); // fallback
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

    // Read GPS
    GPSData gpsData;
    memset(&gpsData, 0, sizeof(gpsData));
    if (gps && gps->read(gpsData)) {
      display.stats.gpsReadCount++;
    }
    currentRecord.gps = gpsData;
    currentRecord.epochMs = now;
    lastGPSData = gpsData; // keep for display

    // Track IMU samples per epoch for display
    display.stats.lastIMUSamples = imuSampleIndex;

    // Push completed record to buffer
    dataBuffer.push(currentRecord);

    // Reset epoch for next second
    memset(&currentRecord, 0, sizeof(currentRecord));
    imuSampleIndex = 0;
  }

  // --- Network update + data send ---
  network.update();

  if (network.isConnected() && !dataBuffer.isEmpty()) {
    // Send in batches, throttled to 1 send/sec
    if (now - lastSendMs >= 1000) {
      lastSendMs = now;
      SensorRecord batch[SEND_BATCH_SIZE];
      int count = dataBuffer.popBatch(batch, SEND_BATCH_SIZE);
      if (count > 0) {
        if (network.sendData(batch, count)) {
          display.stats.sendSuccessCount++;
        } else {
          display.stats.sendFailCount++;
          // Send failed — push records back (best effort)
          for (int i = 0; i < count; i++) {
            dataBuffer.push(batch[i]);
          }
          Serial.println("[Main] send failed, re-queued");
        }
      }
    }
  }

  // --- Display update ---
  display.update(lastGPSData, dataBuffer, network);

  // Small yield to prevent watchdog timeout
  delay(1);
}
