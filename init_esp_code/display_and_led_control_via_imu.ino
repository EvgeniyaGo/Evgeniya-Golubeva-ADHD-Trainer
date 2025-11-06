// === ESP32: ADXL345 face detector + 2 WS2812B matrices — Orientation Game ===
// Libraries: Adafruit NeoPixel, Wire

#include <Adafruit_NeoPixel.h>
#include <Wire.h>
#include <math.h>
#include <Types.h>

// ───────────────────────── 7) Draw helpers (struct Draw) ─────────
struct Draw {
  static inline uint16_t xyToIndex(uint8_t x, uint8_t y){
    uint8_t tx = x, ty = y;
    if (SWAP_XY){ uint8_t t = tx; tx = ty; ty = t; }
    if (FLIP_X) tx = (W-1) - tx;
    if (FLIP_Y) ty = (H-1) - ty;
    if (!SERPENTINE) return uint16_t(ty) * W + tx;
    bool odd = (ty & 1);
    return odd ? uint16_t(ty)*W + (W-1 - tx) : uint16_t(ty)*W + tx;
  }
  static inline void setXY(Adafruit_NeoPixel& s, uint8_t x, uint8_t y, uint32_t c){
    if (x >= W || y >= H) return;
    s.setPixelColor(xyToIndex(x,y), c);
  }
  static void clear(Adafruit_NeoPixel& s){ s.clear(); s.show(); }
  static void square2x2(Adafruit_NeoPixel& s, uint32_t color){
    s.clear();
    uint8_t cx = W/2, cy = H/2;
    for (int dy=-1; dy<=0; ++dy)
      for (int dx=-1; dx<=0; ++dx)
        setXY(s, cx+dx, cy+dy, color);
    s.show();
  }
  static void square4x4(Adafruit_NeoPixel& s, uint32_t color){
    s.clear();
    uint8_t cx = W/2, cy = H/2;
    for (int dy=-2; dy<=1; ++dy)
      for (int dx=-2; dx<=1; ++dx)
        setXY(s, cx+dx, cy+dy, color);
    s.show();
  }
};

// ───────────────────────── 8) ADXL345 (minimal) ──────────────────
namespace ADXL345 {
  constexpr uint8_t REG_DEVID    = 0x00;
  constexpr uint8_t REG_BW_RATE  = 0x2C;
  constexpr uint8_t REG_POWERCTL = 0x2D;
  constexpr uint8_t REG_DATAFMT  = 0x31;
  constexpr uint8_t REG_DATAX0   = 0x32;
  static uint8_t I2C_ADDR = 0x53;

  inline void write8(uint8_t reg, uint8_t val) {
    Wire.beginTransmission(I2C_ADDR);
    Wire.write(reg); Wire.write(val);
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
    Wire.beginTransmission(I2C_ADDR);
    Wire.write(REG_DEVID);
    Wire.endTransmission(false);
    Wire.requestFrom(I2C_ADDR, (uint8_t)1);
    return Wire.available() && (Wire.read() == 0xE5);
  }
  inline bool begin() {
    if (!probeAt(0x53) && !probeAt(0x1D)) return false;
    write8(REG_BW_RATE,  0x08); // 25 Hz
    write8(REG_DATAFMT,  0x0A); // FULL_RES=1, ±8g
    write8(REG_POWERCTL, 0x08); // MEASURE=1
    delay(10);
    return true;
  }
}

// ───────────────────────── 9) App helpers (indicators, mapping) ──
namespace App {
  static inline float ema(float prev, float sample, float a) { return prev + a * (sample - prev); }
  static inline const char* faceName(Face f){
    switch(f){
      case FACE_UP:    return "Upward";
      case FACE_DOWN:  return "Downward";
      case FACE_LEFT:  return "Left";
      case FACE_RIGHT: return "Right";
      case FACE_FRONT: return "Front";
      case FACE_BACK:  return "Back";
      default:         return "Unknown";
    }
  }
  static inline Face faceFromAxis(Axis ax, bool positive){
    if (ax == AX_Z) return positive ? FACE_UP   : FACE_DOWN;
    if (ax == AX_X) return positive ? FACE_RIGHT: FACE_LEFT;
    if (ax == AX_Y) return positive ? FACE_FRONT: FACE_BACK;
    return FACE_UNKNOWN;
  }
  static inline void setOnlyFaceIndicators(Face f){
    digitalWrite(LED_UP,    f == FACE_UP    ? HIGH : LOW);
    digitalWrite(LED_DOWN,  f == FACE_DOWN  ? HIGH : LOW);
    digitalWrite(LED_LEFT,  f == FACE_LEFT  ? HIGH : LOW);
    digitalWrite(LED_RIGHT, f == FACE_RIGHT ? HIGH : LOW);
    digitalWrite(LED_FRONT, f == FACE_FRONT ? HIGH : LOW);
    digitalWrite(LED_BACK,  f == FACE_BACK  ? HIGH : LOW);
  }
  static inline Adafruit_NeoPixel* stripForFace(Face f) {
    for (uint8_t i=0; i<NUM_PANELS; ++i) if (kPanels[i].face == f) return kPanels[i].strip;
    return nullptr;
  }
}

// ───────────────────────── 10) Function declarations ─────────────
static void initIndicators();
static void startTrial();
static void beginGameIfNeeded();
static void clearAllPanels();
static void ensureCue(Face target, ColorName c);
static void ensureCue(Face target, const char* colorName);
static void ensureFeedback(Face target, ColorName c);
static void ensureFeedback(Face target, const char* colorName);
static Face detectRawFaceFromAccel(int16_t rx, int16_t ry, int16_t rz);
static void updateStableFace(Face newRaw, uint32_t now);
static Face pickRandomPanelFace();

// ───────────────────────── 11) Arduino setup ─────────────────────
static void initIndicators() {
  pinMode(LED_UP,     OUTPUT);
  pinMode(LED_DOWN,   OUTPUT);
  pinMode(LED_LEFT,   OUTPUT);
  pinMode(LED_RIGHT,  OUTPUT);
  pinMode(LED_FRONT,  OUTPUT);
  pinMode(LED_BACK,   OUTPUT);
  App::setOnlyFaceIndicators(FACE_UNKNOWN); // all LOW
}

void setup(){
  Serial.begin(115200);
  initIndicators();

  Wire.begin(SDA_PIN, SCL_PIN);
  delay(5);

  if (!ADXL345::begin()) {
    Serial.println("Failed to find ADXL345!");
    while (true) { delay(1000); }
  }

  stripThin.begin();  stripThin.setBrightness(BRIGHTNESS);  stripThin.show();
  stripThick.begin(); stripThick.setBrightness(BRIGHTNESS); stripThick.show();

  // Seed EMA with first sample (optional)
  int16_t rx, ry, rz;
  if (ADXL345::readXYZ_raw(rx, ry, rz)) {
    xf = (SIGN_X * rx) * LSB_TO_MPS2;
    yf = (SIGN_Y * ry) * LSB_TO_MPS2;
    zf = (SIGN_Z * rz) * LSB_TO_MPS2;
  }

  randomSeed((uint32_t)esp_random() ^ micros());
  Serial.println("Ready. Orientation game initialized.");
}

// ───────────────────────── 12) Game state + helpers ──────────────
GameState gState = PREPARE;
const uint32_t T_HEADSTART_MS = 2000;
const uint32_t T_DISCOVERY_MS = 5000; // deadline to first align
const uint32_t T_HOLD_MS      = 3000; // continuous hold
const uint32_t T_FEEDBACK_MS  = 3000;

Face   targetFace = FACE_UNKNOWN;
bool   discovered = false;
uint32_t stateStart = 0;
uint32_t lastTick   = 0;   // pacing serial prints
uint32_t holdAccum  = 0;   // ms
uint32_t lastSample = 0;

// Absolute DISCOVERY deadline (fix for returning from HOLD)
uint32_t discoveryDeadline = 0;

enum DrawMode : uint8_t { MODE_NONE=0, MODE_CUE2, MODE_FEEDBACK4 };
struct DrawState { DrawMode mode; Face face; ColorName color; };
static DrawState gDraw = { MODE_NONE, FACE_UNKNOWN, WHITE };

static void clearAllPanels(){
  for (uint8_t i=0; i<NUM_PANELS; ++i) Draw::clear(*kPanels[i].strip);
  gDraw = { MODE_NONE, FACE_UNKNOWN, WHITE };
}

static void ensureCue(Face target, ColorName c){
  if (gDraw.mode == MODE_CUE2 && gDraw.face == target && gDraw.color == c) return;
  clearAllPanels();
  if (Adafruit_NeoPixel* s = App::stripForFace(target))
    Draw::square2x2(*s, packColor(c, s));
  gDraw = { MODE_CUE2, target, c };
}
static void ensureCue(Face target, const char* colorName){
  ensureCue(target, (ColorName)colorIdRaw(colorName));
}
static void ensureFeedback(Face target, ColorName c){
  if (gDraw.mode == MODE_FEEDBACK4 && gDraw.face == target && gDraw.color == c) return;
  clearAllPanels();
  if (Adafruit_NeoPixel* s = App::stripForFace(target))
    Draw::square4x4(*s, packColor(c, s));
  gDraw = { MODE_FEEDBACK4, target, c };
}
static void ensureFeedback(Face target, const char* colorName){
  ensureFeedback(target, (ColorName)colorIdRaw(colorName));
}

static Face pickRandomPanelFace() {
  if (NUM_PANELS == 0) return FACE_UNKNOWN;
  return kPanels[random(0, NUM_PANELS)].face;
}

static void startTrial() {
  targetFace = pickRandomPanelFace();
  discovered = false;
  holdAccum = 0;
  gState = HEADSTART;
  stateStart = millis();
  lastTick = stateStart;

  discoveryDeadline = 0; // will be set when DISCOVERY starts

  ensureCue(targetFace, WHITE);           // draw once (no flicker)
  App::setOnlyFaceIndicators(targetFace);

  Serial.printf("\n=== New Trial: target=%s ===\n", App::faceName(targetFace));
}

static void beginGameIfNeeded() {
  if (gState == PREPARE) startTrial();
}

// ───────────────────────── 13) Face utilities ─────────────────────
static Face detectRawFaceFromAccel(int16_t rx, int16_t ry, int16_t rz){
  float ax_mps2 = (SIGN_X * rx) * LSB_TO_MPS2;
  float ay_mps2 = (SIGN_Y * ry) * LSB_TO_MPS2;
  float az_mps2 = (SIGN_Z * rz) * LSB_TO_MPS2;

  xf = App::ema(xf, ax_mps2, ALPHA);
  yf = App::ema(yf, ay_mps2, ALPHA);
  zf = App::ema(zf, az_mps2, ALPHA);

  float ax = fabsf(xf), ay = fabsf(yf), az = fabsf(zf);

  if (lastAxis == AX_X)      ax += HYSTERESIS;
  else if (lastAxis == AX_Y) ay += HYSTERESIS;
  else if (lastAxis == AX_Z) az += HYSTERESIS;

  Axis dom; bool pos; float domMag;
  if (ax >= ay && ax >= az)      { dom = AX_X; pos = (xf > 0); domMag = ax; }
  else if (ay >= ax && ay >= az) { dom = AX_Y; pos = (yf > 0); domMag = ay; }
  else                           { dom = AX_Z; pos = (zf > 0); domMag = az; }

  lastAxis = dom;
  return (domMag >= MIN_G) ? App::faceFromAxis(dom, pos) : FACE_UNKNOWN;
}

static void updateStableFace(Face newRaw, uint32_t now){
  static Face lastRaw = FACE_UNKNOWN;
  if (newRaw != lastRaw) { lastRaw = newRaw; faceChangeSince = now; }
  if (now - faceChangeSince >= T_STABLE_MS && stableFace != newRaw)
    stableFace = newRaw;
}

// ───────────────────────── 14) Main loop ─────────────────────────
void loop(){
  // Read ADXL345 and compute faces
  int16_t rx, ry, rz;
  if (ADXL345::readXYZ_raw(rx, ry, rz)) {
    rawFace = detectRawFaceFromAccel(rx, ry, rz);
    updateStableFace(rawFace, millis());
  }

  // Start game if not started
  beginGameIfNeeded();

  // FSM
  uint32_t now = millis();
  switch (gState) {
    case HEADSTART:
      if (now - stateStart >= T_HEADSTART_MS) {
        gState = DISCOVERY;
        stateStart = lastTick = now;

        // set the absolute deadline ONCE for this trial
        discoveryDeadline = now + T_DISCOVERY_MS;

        Serial.println("DISCOVERY: 5");
        ensureCue(targetFace, "white");
        App::setOnlyFaceIndicators(targetFace);
      }
      break;

    case DISCOVERY: {
      // countdown display, based on absolute deadline
      if (now - lastTick >= 1000) {
        int secsLeft = (discoveryDeadline > now)
          ? (int)((discoveryDeadline - now + 999) / 1000)
          : 0;
        Serial.printf("DISCOVERY: %d\n", secsLeft);
        lastTick = now;
      }

      if (!discovered && stableFace == targetFace) {
        discovered = true;
        gState = HOLD;
        holdAccum = 0;
        lastSample = lastTick = now;
        Serial.println("HOLD: 3");
      }

      if (!discovered && now >= discoveryDeadline) {
        gState = FAIL_FB;
        stateStart = now;
        ensureFeedback(targetFace, "red");
        App::setOnlyFaceIndicators(FACE_UNKNOWN);
        Serial.println("FAIL: not found in time");
      }
      break;
    }

    case HOLD: {
      uint32_t dt = now - lastSample; lastSample = now;

      if (stableFace == targetFace) {
        holdAccum += dt;
      } else {
        // ⇩ return to DISCOVERY without resetting deadline
        discovered = false;
        gState = DISCOVERY;
        lastTick = now; // for clean 1s prints
        holdAccum = 0;  // reset hold timer
        ensureCue(targetFace, "white");
        App::setOnlyFaceIndicators(targetFace);
        break; // exit HOLD case early
      }

      if (now - lastTick >= 250) {
        uint32_t remaining = (holdAccum >= T_HOLD_MS) ? 0 : (T_HOLD_MS - holdAccum);
        int s = (remaining + 999) / 1000; // ceil
        Serial.printf("HOLD: %d\n", s);
        lastTick = now;
      }

      App::setOnlyFaceIndicators(targetFace);
      ensureCue(targetFace, "white"); // idempotent

      if (holdAccum >= T_HOLD_MS) {
        gState = SUCCESS_FB;
        stateStart = now;
        ensureFeedback(targetFace, "green");
        App::setOnlyFaceIndicators(FACE_UNKNOWN);
        Serial.println("SUCCESS");
      }
      break;
    }

    case SUCCESS_FB:
    case FAIL_FB:
      if (now - stateStart >= T_FEEDBACK_MS) {
        gState = INTER_TRIAL;
        stateStart = now;
        clearAllPanels();
      }
      break;

    case INTER_TRIAL:
      if (now - stateStart >= 500) {
        gState = PREPARE;
        startTrial();
      }
      break;

    case PREPARE:
    default:
      break;
  }

  delay(PERIOD);
}
