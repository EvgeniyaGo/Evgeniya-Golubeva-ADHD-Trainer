#include <Arduino.h>
#include <Wire.h>
#include <cstring>   // for std::strcmp
#include <math.h>    // for fabsf

/* ===================== Pins (LEDs) ===================== */
#define LED_UP     5      // 
#define LED_DOWN   18     //
#define LED_LEFT   22     
#define LED_RIGHT  23     // 
#define LED_FRONT  21
#define LED_BACK   19   

/* ===================== I2C pins (ADXL345) ===================== */
// Use spare GPIOs so 21/22 remain free for LEDs
#define SDA_PIN    25
#define SCL_PIN    26

/* ===================== LED state booleans ===================== */
// Exactly one should be true at a time (face that is UP). Others false.
bool ledUpState    = false;
bool ledDownState  = false;
bool ledLeftState  = false;
bool ledRightState = false;
bool ledFrontState = false;
bool ledBackState  = false;

/* ===================== Forward decls to avoid Arduino proto issues ===================== */
enum Axis { AX_X, AX_Y, AX_Z, AX_NONE };
const char* faceFor(Axis ax, bool positive);

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

  inline bool probeAt(uint8_t addr) {
    I2C_ADDR = addr;
    uint8_t id = read8(REG_DEVID);
    return (id == 0xE5);
  }

  // Configure: 25 Hz ODR, FULL_RES, ±8g, measurement on
  inline bool begin() {
    if (!probeAt(0x53) && !probeAt(0x1D)) return false;   // default then alt

    write8(REG_BW_RATE,  0x08); // 25 Hz
    write8(REG_DATAFMT,  0x0A); // FULL_RES=1, range=±8g
    write8(REG_POWERCTL, 0x08); // MEASURE=1
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

// If your sensor is mounted differently, flip axis signs here
const int8_t SIGN_X = +1;  // set to -1 to invert
const int8_t SIGN_Y = +1;
const int8_t SIGN_Z = +1;

/* ===================== State ===================== */
float xf = 0.0f, yf = 0.0f, zf = 9.81f;
Axis lastAxis = AX_NONE;
String lastOut = "";

/* ===================== LED helpers ===================== */
void allOff() {
  digitalWrite(LED_UP,    LOW);
  digitalWrite(LED_DOWN,  LOW);
  digitalWrite(LED_LEFT,  LOW);
  digitalWrite(LED_RIGHT, LOW);
  digitalWrite(LED_FRONT, LOW);
  digitalWrite(LED_BACK,  LOW);
}

void applyLedBools() {
  digitalWrite(LED_UP,     ledUpState    ? HIGH : LOW);
  digitalWrite(LED_DOWN,   ledDownState  ? HIGH : LOW);
  digitalWrite(LED_LEFT,   ledLeftState  ? HIGH : LOW);
  digitalWrite(LED_RIGHT,  ledRightState ? HIGH : LOW);
  digitalWrite(LED_FRONT,  ledFrontState ? HIGH : LOW);
  digitalWrite(LED_BACK,   ledBackState  ? HIGH : LOW);
}

// Set exactly one LED true (face up); others false
void setOnlyFace(const char* name) {
  ledUpState    = false;
  ledDownState  = false;
  ledLeftState  = false;
  ledRightState = false;
  ledFrontState = false;
  ledBackState  = false;

  if      (!std::strcmp(name, "Upward"))   ledUpState   = true;
  else if (!std::strcmp(name, "Downward")) ledDownState = true;
  else if (!std::strcmp(name, "Left"))     ledLeftState = true;
  else if (!std::strcmp(name, "Right"))    ledRightState= true;
  else if (!std::strcmp(name, "Front"))    ledFrontState= true;
  else if (!std::strcmp(name, "Back"))     ledBackState = true;

  applyLedBools();
}

/* ===================== Math helpers ===================== */
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
void initLEDs() {
  pinMode(LED_UP,     OUTPUT);
  pinMode(LED_DOWN,   OUTPUT);
  pinMode(LED_LEFT,   OUTPUT);
  pinMode(LED_RIGHT,  OUTPUT);
  pinMode(LED_FRONT,  OUTPUT);
  pinMode(LED_BACK,   OUTPUT);
  allOff();
}

void setup() {
  Serial.begin(115200);
  initLEDs();

  // Start I2C on chosen pins
  Wire.begin(SDA_PIN, SCL_PIN);
  delay(5);

  if (!ADXL345::begin()) {
    Serial.println("Failed to find ADXL345!");
    // keep LEDs off so it's obvious
    while (true) { delay(1000); }
  }

  // Seed filters with first read to avoid jump
  int16_t rx, ry, rz;
  if (ADXL345::readXYZ_raw(rx, ry, rz)) {
    xf = (SIGN_X * rx) * LSB_TO_MPS2;
    yf = (SIGN_Y * ry) * LSB_TO_MPS2;
    zf = (SIGN_Z * rz) * LSB_TO_MPS2;
  }

  Serial.println("Face detector ready: Upward/Downward/Left/Right/Front/Back");
}

void loop() {
  // Read raw sensor
  int16_t rx, ry, rz;
  if (!ADXL345::readXYZ_raw(rx, ry, rz)) {
    delay(PERIOD);
    return;
  }

  // Convert to m/s^2, apply optional sign flips
  float ax_mps2 = (SIGN_X * rx) * LSB_TO_MPS2;
  float ay_mps2 = (SIGN_Y * ry) * LSB_TO_MPS2;
  float az_mps2 = (SIGN_Z * rz) * LSB_TO_MPS2;

  // Smooth
  xf = ema(xf, ax_mps2, ALPHA);
  yf = ema(yf, ay_mps2, ALPHA);
  zf = ema(zf, az_mps2, ALPHA);

  // Magnitudes
  float ax = fabsf(xf), ay = fabsf(yf), az = fabsf(zf);

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

  // Enough gravity?
  const char* out = "Unknown";
  if (domMag >= MIN_G) out = faceFor(dom, pos);

  // Update LEDs only on change
  if (lastOut != out) {
    Serial.println(out);
    if (std::strcmp(out, "Unknown") == 0) {
      allOff(); // below threshold → no face lit
    } else {
      setOnlyFace(out); // exactly one LED on
    }
    lastOut = out;
  }

  lastAxis = dom;
  delay(PERIOD);
}
