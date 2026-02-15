#include "imu_control.h"
#include <Wire.h>

// ADXL345 registers
static const uint8_t ADXL345_ADDR    = 0x53;
static const uint8_t REG_DEVID       = 0x00;
static const uint8_t REG_BW_RATE     = 0x2C;
static const uint8_t REG_POWER_CTL   = 0x2D;
static const uint8_t REG_DATA_FORMAT = 0x31;
static const uint8_t REG_DATAX0      = 0x32;

static ImuState gImu = { FACE_UNKNOWN, 100.0f, false, 0, 0, 0 };
static bool gImuOk = false;

static bool writeReg(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(ADXL345_ADDR);
  Wire.write(reg);
  Wire.write(value);
  return (Wire.endTransmission() == 0);
}

static bool readReg(uint8_t reg, uint8_t &value) {
  Wire.beginTransmission(ADXL345_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;

  if (Wire.requestFrom(ADXL345_ADDR, (uint8_t)1) != 1) return false;
  value = Wire.read();
  return true;
}

static bool readBytes(uint8_t startReg, uint8_t *buf, uint8_t len) {
  Wire.beginTransmission(ADXL345_ADDR);
  Wire.write(startReg);
  if (Wire.endTransmission(false) != 0) return false;

  if (Wire.requestFrom(ADXL345_ADDR, len) != len) return false;
  for (uint8_t i = 0; i < len; i++) buf[i] = Wire.read();
  return true;
}

static inline float fAbs(float v) { return (v < 0) ? -v : v; }

static inline FaceId faceFromAxes(float ax, float ay, float az) {
  float axA = fAbs(ax), ayA = fAbs(ay), azA = fAbs(az);

  if (azA >= axA && azA >= ayA) return (az >= 0) ? FACE_UP : FACE_DOWN;
  if (axA >= ayA)              return (ax >= 0) ? FACE_FRONT : FACE_BACK;
  return                         (ay >= 0) ? FACE_LEFT : FACE_RIGHT;
}

static inline float tiltPercentFromAxes(float ax, float ay, float az) {
  float mag = sqrtf(ax*ax + ay*ay + az*az);
  if (mag < 0.0001f) return 100.0f;

  float m = fAbs(ax);
  float ayA = fAbs(ay);
  float azA = fAbs(az);
  if (ayA > m) m = ayA;
  if (azA > m) m = azA;

  float ratio = m / mag;
  float tilt = (1.0f - ratio) * 100.0f;
  if (tilt < 0) tilt = 0;
  if (tilt > 100) tilt = 100;
  return tilt;
}

bool initImu() {
  uint8_t id = 0;
  if (!readReg(REG_DEVID, id)) return false;
  if (id != 0xE5) return false;

  if (!writeReg(REG_BW_RATE, 0x0A)) return false;
  if (!writeReg(REG_DATA_FORMAT, 0x09)) return false;
  if (!writeReg(REG_POWER_CTL, 0x08)) return false;

  gImuOk = true;
  return true;
}

void updateImu() {
  if (!gImuOk) {
    gImu.upFace = FACE_UNKNOWN;
    gImu.tiltPercent = 100.0f;
    gImu.isLocked = false;
    return;
  }

  uint8_t b[6];
  if (!readBytes(REG_DATAX0, b, 6)) {
    gImuOk = false;
    gImu.upFace = FACE_UNKNOWN;
    gImu.tiltPercent = 100.0f;
    gImu.isLocked = false;
    return;
  }

  int16_t rawX = (int16_t)((uint16_t)b[1] << 8 | b[0]);
  int16_t rawY = (int16_t)((uint16_t)b[3] << 8 | b[2]);
  int16_t rawZ = (int16_t)((uint16_t)b[5] << 8 | b[4]);

  const float G_PER_LSB = 0.004f; 
  float ax = rawY * G_PER_LSB;
  float ay = rawX * G_PER_LSB;
  float az = rawZ * G_PER_LSB;

  gImu.ax = ax; gImu.ay = ay; gImu.az = az;

  gImu.upFace = faceFromAxes(ax, ay, az);
  gImu.tiltPercent = tiltPercentFromAxes(ax, ay, az);

  bool valid = (gImu.tiltPercent <= VALID_TILT_THRESHOLD_PERCENT);
  gImu.isLocked = valid && (gImu.tiltPercent <= LOCK_TILT_THRESHOLD_PERCENT);

  if (!valid) gImu.upFace = FACE_UNKNOWN;
}

ImuState getImuState() { return gImu; }
bool isValidUpFace() { return (gImu.upFace != FACE_UNKNOWN); }
bool isFaceLocked() { return gImu.isLocked; }
