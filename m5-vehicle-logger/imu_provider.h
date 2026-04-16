// imu_provider.h — IMU/environment provider interface and implementations
#pragma once

#include <M5Unified.h>
#include <Wire.h>
#include "types.h"
#include "config.h"
#include "gravity_compensator.h"

// --- IMU Interface ---
class IMUProvider {
public:
  virtual ~IMUProvider() {}
  virtual void begin() = 0;
  virtual bool read(IMUData& out) = 0;  // true if data available
};

// --- Environment sensor interface ---
class EnvProvider {
public:
  virtual ~EnvProvider() {}
  virtual void begin() = 0;
  virtual bool read(EnvData& out) = 0;
};

// --- M5Unified IMU (BMI270 auto-detected) + BMM150 direct Wire ---
//
// M5Unified handles BMI270 initialization and firmware upload internally.
// BMM150 is on BMI270's secondary I2C — but the host cannot see it
// when BMI270 owns the aux bus.  M5Unified does NOT expose the
// magnetometer, so we read it via BMI270 auxiliary register passthrough.

// BMI270 register addresses for auxiliary (BMM150) passthrough
#define BMI2_AUX_DEV_ID      0x4B  // aux device I2C address
#define BMI2_AUX_IF_CONF     0x4C  // aux interface configuration
#define BMI2_AUX_RD_ADDR     0x4D  // aux read address
#define BMI2_AUX_WR_ADDR     0x4E  // aux write address
#define BMI2_AUX_WR_DATA     0x4F  // aux write data
#define BMI2_AUX_DATA_BASE   0x04  // aux data registers 0x04-0x0B (8 bytes)
#define BMI2_AUX_IF_TRIM     0x68  // aux IF trim (pull-up config)
#define BMI2_INTERNAL_STATUS  0x21  // internal status register

// BMM150 register addresses
#define BMM150_REG_CHIP_ID    0x40
#define BMM150_REG_DATA_X_LSB 0x42
#define BMM150_REG_POWER_CTRL 0x4B
#define BMM150_REG_OP_MODE    0x4C
#define BMM150_CHIP_ID_VALUE  0x32

class GNSSIMUProvider : public IMUProvider {
public:
  void begin() override {
    // M5Unified already initialized IMU in M5.begin()
    if (M5.Imu.isEnabled()) {
      // getType(): 0=none, 1=unknown, 2=SH200Q, 3=MPU6050, 4=MPU6886,
      //            5=MPU9250, 6=BMI270
      uint8_t imuType = M5.Imu.getType();
      Serial.printf("[IMU] M5Unified IMU detected: type=%d\n", imuType);
      _imuOk = true;

      // Diagnose BMI270 state via raw registers
      uint8_t chipId = readReg(0x00);        // expect 0x24
      uint8_t intStatus = readReg(0x21);     // internal status: 0x01 = init OK
      uint8_t pwrCtrl = readReg(0x7D);       // bit2=acc, bit1=gyr
      Serial.printf("[IMU] BMI270 chip=0x%02X status=0x%02X pwr=0x%02X\n",
                    chipId, intStatus, pwrCtrl);

      // Try reading raw accel data directly (0x0C-0x11)
      Wire.beginTransmission(BMI270_I2C_ADDR);
      Wire.write(0x0C);
      Wire.endTransmission(false);
      Wire.requestFrom(BMI270_I2C_ADDR, (uint8_t)12);
      int16_t raw[6] = {0};
      for (int i = 0; i < 6 && Wire.available() >= 2; i++) {
        uint8_t lo = Wire.read();
        uint8_t hi = Wire.read();
        raw[i] = (int16_t)((hi << 8) | lo);
      }
      Serial.printf("[IMU] raw accel: %d %d %d  gyro: %d %d %d\n",
                    raw[0], raw[1], raw[2], raw[3], raw[4], raw[5]);

      // Test via M5Unified API
      M5.Imu.update();
      auto test = M5.Imu.getImuData();
      Serial.printf("[IMU] M5 API: ax=%.3f ay=%.3f az=%.3f\n",
                    test.accel.x, test.accel.y, test.accel.z);
    } else {
      Serial.println("[IMU] M5Unified IMU NOT detected");
      _imuOk = false;
    }

    // Try to set up BMM150 via BMI270 auxiliary interface
    _magOk = initBMM150();
    if (_magOk) {
      Serial.println("[IMU] BMM150 magnetometer enabled");
    } else {
      Serial.println("[IMU] BMM150 magnetometer NOT available");
    }
  }

  bool read(IMUData& out) override {
    if (!_imuOk) return false;

    M5.Imu.update();
    auto data = M5.Imu.getImuData();
    out.ax = data.accel.x;
    out.ay = data.accel.y;
    out.az = data.accel.z;
    out.gx = data.gyro.x;
    out.gy = data.gyro.y;
    out.gz = data.gyro.z;

    if (_magOk) {
      readBMM150(out.mx, out.my, out.mz);
    } else {
      out.mx = out.my = out.mz = 0.0f;
    }

    out.offsetMs = 0;  // caller sets this

    // Gravity compensation: update internal state
    float f, l, v;
    _lastCompensated = _grav.update(out.ax, out.ay, out.az, f, l, v);
    _lastVehicle = {f, l, v, _lastCompensated};

    return true;
  }

  // Get latest gravity-compensated values (for display, not stored in buffer)
  const VehicleAccel& vehicleAccel() const { return _lastVehicle; }
  bool isCalibrated() const { return _grav.isCalibrated(); }
  float getTiltDeg() const { return _grav.getTiltDeg(); }

private:
  bool _imuOk = false;
  bool _magOk = false;
  bool _lastCompensated = false;
  VehicleAccel _lastVehicle = {0, 0, 0, false};
  GravityCompensator _grav;

  // Write a single register on BMI270
  bool writeReg(uint8_t reg, uint8_t val) {
    Wire.beginTransmission(BMI270_I2C_ADDR);
    Wire.write(reg);
    Wire.write(val);
    return (Wire.endTransmission() == 0);
  }

  // Read a single register from BMI270
  uint8_t readReg(uint8_t reg) {
    Wire.beginTransmission(BMI270_I2C_ADDR);
    Wire.write(reg);
    Wire.endTransmission(false);
    Wire.requestFrom(BMI270_I2C_ADDR, (uint8_t)1);
    return Wire.available() ? Wire.read() : 0;
  }

  // Write to BMM150 register via BMI270 auxiliary passthrough
  bool auxWrite(uint8_t bmm_reg, uint8_t val) {
    // 1. Set data to write
    if (!writeReg(BMI2_AUX_WR_DATA, val)) return false;
    // 2. Set BMM150 register address to write to
    if (!writeReg(BMI2_AUX_WR_ADDR, bmm_reg)) return false;
    delay(2);
    return true;
  }

  // Read from BMM150 via BMI270 auxiliary auto-read
  // BMI270 periodically reads from the address set in AUX_RD_ADDR
  // and stores result in AUX_DATA registers (0x04-0x0B)
  bool auxSetReadAddr(uint8_t bmm_reg) {
    return writeReg(BMI2_AUX_RD_ADDR, bmm_reg);
  }

  bool initBMM150() {
    // Check BMI270 is present
    Wire.beginTransmission(BMI270_I2C_ADDR);
    if (Wire.endTransmission() != 0) {
      Serial.println("[MAG] BMI270 not found on I2C");
      return false;
    }
    Serial.println("[MAG] BMI270 found, configuring AUX for BMM150...");

    // Enable manual mode first for BMM150 init commands
    // AUX_IF_CONF (0x4C): bit 7 = manual_en, bit 0 = aux_en
    // Also set man_rd_burst to 8 bytes (bits 1-2 = 0b11)
    writeReg(BMI2_AUX_IF_CONF, 0x87);  // manual_en=1, burst=8bytes, aux_en=1
    delay(2);

    // Set auxiliary device address (BMM150 = 0x10)
    // Register expects 7-bit addr in bits [7:1]
    writeReg(BMI2_AUX_DEV_ID, BMM150_I2C_ADDR << 1);
    delay(2);

    // Set pull-up resistor for aux I2C
    writeReg(BMI2_AUX_IF_TRIM, 0x02);  // 2k pull-up
    delay(2);

    // Enable AUX sensor in BMI270
    // PWR_CTRL register (0x7D): bit 0 = aux_en
    uint8_t pwrCtrl = readReg(0x7D);
    pwrCtrl |= 0x01;  // enable aux power
    writeReg(0x7D, pwrCtrl);
    delay(2);

    // Power on BMM150: write 0x01 to power control (0x4B)
    auxWrite(BMM150_REG_POWER_CTRL, 0x01);
    delay(10);

    // Read BMM150 chip ID to verify connection
    auxSetReadAddr(BMM150_REG_CHIP_ID);
    delay(50);
    uint8_t chipId = readReg(BMI2_AUX_DATA_BASE);
    Serial.printf("[MAG] BMM150 chip ID: 0x%02X (expect 0x%02X)\n",
                  chipId, BMM150_CHIP_ID_VALUE);

    if (chipId != BMM150_CHIP_ID_VALUE) {
      Serial.println("[MAG] BMM150 chip ID mismatch — not connected?");
      return false;
    }

    // Set BMM150 to normal mode: op_mode register (0x4C)
    // bits [3:1] = opmode (00 = normal), bits [7:6] = ODR (00 = 10Hz)
    auxWrite(BMM150_REG_OP_MODE, 0x00);
    delay(10);

    // Switch BMI270 to auto mode: read BMM150 data continuously
    // Set read address to BMM150 data X LSB
    auxSetReadAddr(BMM150_REG_DATA_X_LSB);
    delay(2);

    // AUX_IF_CONF: manual_en=0 (auto mode), burst=8bytes, aux_en=1
    writeReg(BMI2_AUX_IF_CONF, 0x07);  // manual_en=0, burst=8bytes, aux_en=1
    delay(50);

    // Read aux data to verify
    Wire.beginTransmission(BMI270_I2C_ADDR);
    Wire.write(BMI2_AUX_DATA_BASE);
    Wire.endTransmission(false);
    Wire.requestFrom(BMI270_I2C_ADDR, (uint8_t)8);
    uint8_t buf[8] = {0};
    for (int i = 0; i < 8 && Wire.available(); i++) {
      buf[i] = Wire.read();
    }
    Serial.printf("[MAG] aux data: %02X %02X %02X %02X %02X %02X %02X %02X\n",
                  buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7]);

    return true;
  }

  void readBMM150(float &mx, float &my, float &mz) {
    // BMI270 auto-reads BMM150 data into AUX_DATA registers
    Wire.beginTransmission(BMI270_I2C_ADDR);
    Wire.write(BMI2_AUX_DATA_BASE);
    Wire.endTransmission(false);
    Wire.requestFrom(BMI270_I2C_ADDR, (uint8_t)8);

    uint8_t buf[8] = {0};
    for (int i = 0; i < 8 && Wire.available(); i++) {
      buf[i] = Wire.read();
    }

    // BMM150 data format (LSB first):
    // X: buf[0-1], 13-bit signed (bits 3-15 of 16-bit, lower 3 bits = status)
    // Y: buf[2-3], 13-bit signed
    // Z: buf[4-5], 15-bit signed (bit 0 = status)
    // Hall: buf[6-7], 14-bit unsigned
    int16_t raw_x = (int16_t)((buf[1] << 8) | buf[0]) >> 3;  // 13-bit signed
    int16_t raw_y = (int16_t)((buf[3] << 8) | buf[2]) >> 3;
    int16_t raw_z = (int16_t)((buf[5] << 8) | buf[4]) >> 1;  // 15-bit signed

    // Raw values are in LSB, ~0.3 uT/LSB for XY, ~0.3 uT/LSB for Z
    mx = (float)raw_x * 0.3f;
    my = (float)raw_y * 0.3f;
    mz = (float)raw_z * 0.3f;
  }
};

// --- BMP280 implementation ---
#include <Adafruit_BMP280.h>

class BMP280EnvProvider : public EnvProvider {
public:
  void begin() override {
    if (!_bmp.begin(BMP280_I2C_ADDR)) {
      Serial.println("[ENV] BMP280 init FAILED");
      _ok = false;
      return;
    }
    _bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,
                     Adafruit_BMP280::SAMPLING_X2,   // temperature
                     Adafruit_BMP280::SAMPLING_X16,  // pressure
                     Adafruit_BMP280::FILTER_X16,
                     Adafruit_BMP280::STANDBY_MS_500);
    _ok = true;
    Serial.println("[ENV] BMP280 initialized");
  }

  bool read(EnvData& out) override {
    if (!_ok) {
      out.valid = false;
      return false;
    }
    out.temperature = _bmp.readTemperature();
    out.pressure    = _bmp.readPressure() / 100.0f;  // Pa -> hPa
    out.altitude    = _bmp.readAltitude(1013.25f);    // standard sea level
    out.valid       = true;
    return true;
  }

private:
  Adafruit_BMP280 _bmp;
  bool _ok = false;
};

// --- Mock IMU implementation ---
class MockIMUProvider : public IMUProvider {
public:
  void begin() override {
    Serial.println("[MockIMU] initialized");
  }

  bool read(IMUData& out) override {
    unsigned long now = millis();
    float noise = (float)(now % 100) / 1000.0f;

    out.ax = 0.01f + noise;
    out.ay = -0.02f + noise * 0.5f;
    out.az = 9.81f + noise * 0.1f;
    out.gx = 0.5f + noise * 2.0f;
    out.gy = -0.3f + noise * 1.5f;
    out.gz = 0.1f + noise;
    out.mx = 25.0f + noise * 10.0f;
    out.my = -15.0f + noise * 5.0f;
    out.mz = 40.0f + noise * 3.0f;
    out.offsetMs = 0;

    return true;
  }
};

// --- Mock environment implementation ---
class MockEnvProvider : public EnvProvider {
public:
  void begin() override {
    Serial.println("[MockENV] initialized");
  }

  bool read(EnvData& out) override {
    out.temperature = 25.0f + (float)(millis() % 100) / 100.0f;
    out.pressure    = 1013.25f + (float)(millis() % 50) / 10.0f;
    out.altitude    = 40.0f;
    out.valid       = true;
    return true;
  }
};
