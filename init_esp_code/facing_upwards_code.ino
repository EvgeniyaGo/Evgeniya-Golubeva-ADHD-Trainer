#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>

Adafruit_MPU6050 mpu;

// ===== Tuning =====
const float ALPHA     = 0.25f;   // EMA for accel smoothing (0..1); higher = faster, noisier
const float MIN_G     = 6.5f;    // m/s^2 needed on dominant axis to accept a face
const float HYSTERESIS= 0.6f;    // extra bias to stay on the last chosen axis (m/s^2)
const uint16_t PERIOD = 40;      // ms loop (~25 Hz). Increase if still jittery.

// ===== State =====
float xf=0, yf=0, zf=9.81f;      // filtered accel
enum Axis { AX_X, AX_Y, AX_Z, AX_NONE };
Axis lastAxis = AX_NONE;
String lastOut = "";

static inline float ema(float prev, float sample, float a){ return prev + a*(sample - prev); }

const char* faceFor(Axis ax, bool positive) {
  if (ax == AX_Z) return positive ? "Upward" : "Downward";
  if (ax == AX_X) return positive ? "Right"  : "Left";
  if (ax == AX_Y) return positive ? "Front"  : "Back";
  return "Unknown";
}

void setup() {
  Serial.begin(115200);
  while (!Serial) { delay(10); }

  if (!mpu.begin()) {
    Serial.println("Failed to find MPU6050!");
    while (1) delay(10);
  }
  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setFilterBandwidth(MPU6050_BAND_5_HZ); // low bandwidth = quieter readings
  delay(100);

  Serial.println("Face detector ready: Upward/Downward/Left/Right/Front/Back");
}

void loop() {
  sensors_event_t a, g, t;
  mpu.getEvent(&a, &g, &t);

  // Smooth accel
  xf = ema(xf, a.acceleration.x, ALPHA);
  yf = ema(yf, a.acceleration.y, ALPHA);
  zf = ema(zf, a.acceleration.z, ALPHA);

  // Magnitudes
  float ax = fabs(xf), ay = fabs(yf), az = fabs(zf);

  // Hysteresis: bias toward staying with previous axis
  if (lastAxis == AX_X) ax += HYSTERESIS;
  else if (lastAxis == AX_Y) ay += HYSTERESIS;
  else if (lastAxis == AX_Z) az += HYSTERESIS;

  // Pick dominant axis
  Axis dom;
  bool pos;
  float domMag;
  if (ax >= ay && ax >= az)      { dom = AX_X; pos = (xf > 0); domMag = ax; }
  else if (ay >= ax && ay >= az) { dom = AX_Y; pos = (yf > 0); domMag = ay; }
  else                           { dom = AX_Z; pos = (zf > 0); domMag = az; }

  // Require enough gravity on that axis
  const char* out = "Unknown";
  if (domMag >= MIN_G) {
    out = faceFor(dom, pos);
  }

  // Print only on change
  if (lastOut != out) {
    Serial.println(out);
    lastOut = out;
  }

  lastAxis = dom;
  delay(PERIOD);
}
