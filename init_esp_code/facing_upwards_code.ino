#if 0
#include <Wire.h>

/* ===================== ADXL345 (minimal I2C) ===================== */
namespace ADXL345 {
  // Registers
  constexpr uint8_t REG_DEVID    = 0x00; // should read 0xE5
  constexpr uint8_t REG_BW_RATE  = 0x2C; // data rate / power mode control
  constexpr uint8_t REG_POWERCTL = 0x2D; // power-saving features
  constexpr uint8_t REG_DATAFMT  = 0x31; // data format control
  constexpr uint8_t REG_DATAX0   = 0x32; // first of 6 data bytes

  // Active I2C address (autodetected in begin)
  static uint8_t I2C_ADDR = 0x53;

  // --- Low-level helpers bound to current I2C_ADDR
  inline void write8(uint8_t reg, uint8_t val) {
    Wire.beginTransmission(I2C_ADDR);
    Wire.write(reg);
    Wire.write(val);
    Wire.endTransmission();
  }

  inline uint8_t read8(uint8_t reg) {
    Wire.beginTransmission(I2C_ADDR);
    Wire.write(reg);
    Wire.endTransmission(false);
    Wire.requestFrom(I2C_ADDR, (uint8_t)1);
    return Wire.available() ? Wire.read() : 0;
  }

  inline bool readXYZ_raw(int16_t &x, int16_t &y, int16_t &z) {
    Wire.beginTransmission(I2C_ADDR);
    Wire.write(REG_DATAX0);
    if (Wire.endTransmission(false) != 0) return false;
    Wire.requestFrom(I2C_ADDR, (uint8_t)6);
    if (Wire.available() < 6) return false;
    uint8_t b0 = Wire.read(), b1 = Wire.read();
    uint8_t b2 = Wire.read(), b3 = Wire.read();
    uint8_t b4 = Wire.read(), b5 = Wire.read();
    x = (int16_t)((b1 << 8) | b0);
    y = (int16_t)((b3 << 8) | b2);
    z = (int16_t)((b5 << 8) | b4);
    return true;
  }

  // Probe a given address and verify DEVID
  inline bool probeAt(uint8_t addr) {
    I2C_ADDR = addr;
    uint8_t id = read8(REG_DEVID);
    return (id == 0xE5);
  }

  // Configure: 25 Hz ODR, FULL_RES, ±8g, measurement on
  inline bool begin() {
    // Try default (SDO=GND) then alt addr (SDO=VCC)
    if (!probeAt(0x53) && !probeAt(0x1D)) return false;

    // 25 Hz ODR (0x08)
    write8(REG_BW_RATE, 0x08);

    // FULL_RES=1 (bit3), range=±8g (bits1:0 = 10b) -> 0b00001010 = 0x0A
    write8(REG_DATAFMT, 0x0A);

    // MEASURE=1 (bit3) -> 0x08
    write8(REG_POWERCTL, 0x08);

    delay(10);
    return true;
  }
}

/* ===================== Face-detection tuning ===================== */
const float ALPHA      = 0.25f;    // EMA smoothing (0..1)
const float MIN_G      = 6.5f;     // required m/s^2 on dominant axis
const float HYSTERESIS = 0.6f;     // bias toward last axis (m/s^2)
const uint16_t PERIOD  = 40;       // loop period (ms) ~25 Hz

// ADXL345 full-res scale: ~3.9 mg/LSB
const float LSB_TO_MPS2 = 0.0039f * 9.80665f;  // ≈ 0.0383 m/s^2 per LSB

/* ===================== State ===================== */
float xf = 0.0f, yf = 0.0f, zf = 9.81f;
enum Axis { AX_X, AX_Y, AX_Z, AX_NONE };
Axis lastAxis = AX_NONE;
String lastOut = "";

/* ===================== Helpers ===================== */
static inline float ema(float prev, float sample, float a) {
  return prev + a * (sample - prev);
}

const char* faceFor(Axis ax, bool positive) {
  if (ax == AX_Z) return positive ? "Upward" : "Downward";
  if (ax == AX_X) return positive ? "Right"  : "Left";
  if (ax == AX_Y) return positive ? "Front"  : "Back";
  return "Unknown";
}

/* ===================== Arduino lifecycle ===================== */
void setup() {
  Serial.begin(115200);
  while (!Serial) { delay(1); }

  // Use default I2C pins (ESP32: SDA=21, SCL=22). Change here if needed:
  Wire.begin();
  delay(5);

  if (!ADXL345::begin()) {
    Serial.println("Failed to find ADXL345!");
    while (true) { delay(1000); }
  }

  // Seed filters with first read (optional but reduces initial jump)
  int16_t rx, ry, rz;
  if (ADXL345::readXYZ_raw(rx, ry, rz)) {
    xf = rx * LSB_TO_MPS2;
    yf = ry * LSB_TO_MPS2;
    zf = rz * LSB_TO_MPS2;
  }

  Serial.println("Face detector ready: Upward/Downward/Left/Right/Front/Back");
}

void loop() {
  // Read raw sensor
  int16_t rx, ry, rz;
  if (!ADXL345::readXYZ_raw(rx, ry, rz)) {
    // If a transient I2C hiccup occurs, skip this frame
    delay(PERIOD);
    return;
  }

  // Convert to m/s^2
  float ax_mps2 = rx * LSB_TO_MPS2;
  float ay_mps2 = ry * LSB_TO_MPS2;
  float az_mps2 = rz * LSB_TO_MPS2;

  // Smooth
  xf = ema(xf, ax_mps2, ALPHA);
  yf = ema(yf, ay_mps2, ALPHA);
  zf = ema(zf, az_mps2, ALPHA);

  // Magnitudes
  float ax = fabs(xf), ay = fabs(yf), az = fabs(zf);

  // Hysteresis toward previous axis
  if (lastAxis == AX_X)      ax += HYSTERESIS;
  else if (lastAxis == AX_Y) ay += HYSTERESIS;
  else if (lastAxis == AX_Z) az += HYSTERESIS;

  // Choose dominant axis
  Axis dom;
  bool pos;
  float domMag;
  if (ax >= ay && ax >= az)      { dom = AX_X; pos = (xf > 0); domMag = ax; }
  else if (ay >= ax && ay >= az) { dom = AX_Y; pos = (yf > 0); domMag = ay; }
  else                           { dom = AX_Z; pos = (zf > 0); domMag = az; }

  // Require enough gravity on that axis
  const char* out = "Unknown";
  if (domMag >= MIN_G) out = faceFor(dom, pos);

  // Print only on change
  if (lastOut != out) {
    Serial.println(out);
    lastOut = out;
  }

  lastAxis = dom;
  delay(PERIOD);
}
#endif