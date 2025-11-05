// === ESP32: ADXL345 face detector + 2 WS2812B matrices (Shape 1 on active face) ===
// Board: ESP32
// Libraries: Adafruit NeoPixel, Wire

#include <Adafruit_NeoPixel.h>
#include <Wire.h>
#include <cstring>
#include <math.h>

// ===================== Types first (avoid Arduino proto conflicts) =====================
enum Thickness { THIN, THICK };
enum ColorName { BLUE, YELLOW, RED, GREEN, PURPLE, WHITE };
enum ShapeId {
  SHAPE_1_THIN = 0, SHAPE_2_THIN, SHAPE_3_THIN, SHAPE_4_THIN, SHAPE_5_THIN,
  SHAPE_6_THIN, SHAPE_7_THIN, SHAPE_8_THIN, SHAPE_9_THIN, SHAPE_10_THIN
};
enum Axis { AX_X, AX_Y, AX_Z, AX_NONE };
enum Face { FACE_UP, FACE_DOWN, FACE_LEFT, FACE_RIGHT, FACE_FRONT, FACE_BACK, FACE_UNKNOWN };

// ===================== Indicator LED pins (one per face) =====================
#define LED_UP     5
#define LED_DOWN   18
#define LED_LEFT   22
#define LED_RIGHT  23
#define LED_FRONT  21
#define LED_BACK   19

// ===================== I2C pins (ADXL345) =====================
#define SDA_PIN    25
#define SCL_PIN    26

// ===================== WS2812B panel pins & config =====================
#define PIN_THIN   13   // matrix #1
#define PIN_THICK  14   // matrix #2

static const uint8_t  W = 10, H = 10;
static const uint8_t  BRIGHTNESS = 128;     // 0..255
static const bool     SERPENTINE = false;   // set true if your panel is zig-zag
static const bool     FLIP_X = false, FLIP_Y = false, SWAP_XY = false;

// ===================== NeoPixel strips =====================
Adafruit_NeoPixel stripThin (W * H, PIN_THIN,  NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel stripThick(W * H, PIN_THICK, NEO_GRB + NEO_KHZ800);

// ===================== Shapes (THIN + THICK) =====================
const char* SHAPE1_THIN[10] = {
  "..........",
  ".XXXXXXXX.",
  ".X......X.",
  ".X......X.",
  ".X......X.",
  ".X......X.",
  ".X......X.",
  ".X......X.",
  ".XXXXXXXX.",
  "..........",
};
const char* SHAPE1_THICK[10] = {
  "..........",
  ".XXXXXXXX.",
  ".XXXXXXXX.",
  ".XX....XX.",
  ".XX....XX.",
  ".XX....XX.",
  ".XX....XX.",
  ".XXXXXXXX.",
  ".XXXXXXXX.",
  "..........",
};

const char* SHAPE2_THIN[10] = {
  "....XX....","..XX..XX..",".X......X.",".X......X.","X........X",
  "X........X",".X......X.",".X......X.","..XX..XX..","....XX....",
};
const char* SHAPE2_THICK[10] = {
  "..XXXXXX..",".XXXXXXXX.","XXX....XXX","XX......XX","XX......XX",
  "XX......XX","XX......XX","XXX....XXX",".XXXXXXXX.","..XXXXXX..",
};

const char* SHAPE3_THIN[10] = {
  "....XX....","...X..X...","...X..X...","..X....X..","..X....X..",
  ".X......X.",".X......X.","X........X","XXXXXXXXXX","..........",
};
const char* SHAPE3_THICK[10] = {
  "....XX....","...XXXX...","..XX..XX..","..XX..XX..",".XX....XX.",
  ".XX....XX.","XX......XX","XX......XX","XXXXXXXXXX","XXXXXXXXXX",
};

const char* SHAPE4_THIN[10] = {
  "..XXXXXX..",".XXX...XX.","XXX.....X.","XX........","XX........",
  "XX........","XX........","XXX.....X.",".XXX...XX.","..XXXXXX..",
};
const char* SHAPE4_THICK[10] = {
  "..XXXXX...",".XXXXXXXX.","XXX....XX.","XXX.....X.","XX........",
  "XX........","XXX.....X.","XXX....XX.",".XXXXXXXX.","..XXXXX...",
};

const char* SHAPE5_THIN[10] = {
  "....XX....","...X..X...","..XX..XX..","..X....X..",".X......X.",
  ".X......X.","..X....X..","..XX..XX..","...X..X...","....XX....",
};
const char* SHAPE5_THICK[10] = {
  "....XX....","...XXXX...","..XX..XX..",".XX....XX.","XX......XX",
  "XX......XX",".XX....XX.","..XX..XX..","...XXXX...","....XX....",
};

const char* SHAPE6_THIN[10] = {
  "...XXXX...","...X..X...","...X..X...","XXXX..XXXX","X........X",
  "X........X","XXXX..XXXX","...X..X...","...X..X...","...XXXX...",
};
const char* SHAPE6_THICK[10] = {
  "..XXXXXX..","..XXXXXX..","XXXX..XXXX","XXXX..XXXX","XX......XX",
  "XX......XX","XXXX..XXXX","XXXX..XXXX","..XXXXXX..","..XXXXXX..",
};

const char* SHAPE7_THIN[10] = {
  "..........","..........","..........",".......X..",".......XX.",
  "XXXXXXXXXX",".......XX.",".......X..","..........","..........",
};
const char* SHAPE7_THICK[10] = {
  "..........","......X...","......XX..","......XXX.","XXXXXXXXXX",
  "XXXXXXXXXX","......XXX.","......XX..","......X...","..........",
};

const char* SHAPE8_THIN[10] = {
  "..........","..........",".......X..",".......XX.","....XXXXXX",
  "...XX..XX.","...X...X..","...X......","...X......","...X......",
};
const char* SHAPE8_THICK[10] = {
  "..........",".......X..",".......XX.","....XXXXXX","...XXXXXXX",
  "...XX..XX.","...XX..X..","...XX.....","...XX.....","...XX.....",
};

const char* SHAPE9_THIN[10] = {
  "..........",".....X....","....X.X...","...X...X..","....X.X...",
  ".....X....",".....X....",".....X....",".....X....",".....X....",
};
const char* SHAPE9_THICK[10] = {
  "..........","....XX....","...XXXX...","..XX..XX..","..XX..XX..",
  "...XXXX...","....XX....","....XX....","....XX....","....XX....",
};

const char* SHAPE10_THIN[10] = {
  "XXXXXXXXXX","XXXXXXXXXX","XXXXXXXXXX","XXXXXXXXXX","XXXXXXXXXX",
  "XXXXXXXXXX","XXXXXXXXXX","XXXXXXXXXX","XXXXXXXXXX","XXXXXXXXXX",
};
const char* SHAPE10_THICK[10] = {
  "XXXXXXXXXX","XXXXXXXXXX","XXXXXXXXXX","XXXXXXXXXX","XXXXXXXXXX",
  "XXXXXXXXXX","XXXXXXXXXX","XXXXXXXXXX","XXXXXXXXXX","XXXXXXXXXX",
};

static const char** THIN_SET[10] = {
  SHAPE1_THIN, SHAPE2_THIN, SHAPE3_THIN, SHAPE4_THIN, SHAPE5_THIN,
  SHAPE6_THIN, SHAPE7_THIN, SHAPE8_THIN, SHAPE9_THIN, SHAPE10_THIN
};
static const char** THICK_SET[10] = {
  SHAPE1_THICK, SHAPE2_THICK, SHAPE3_THICK, SHAPE4_THICK, SHAPE5_THICK,
  SHAPE6_THICK, SHAPE7_THICK, SHAPE8_THICK, SHAPE9_THICK, SHAPE10_THICK
};

// ===================== Draw helpers =====================
struct Draw {
  static inline uint32_t pxColor(Adafruit_NeoPixel& s, ColorName name){
    switch(name){
      case BLUE:   return s.Color(0,0,255);
      case YELLOW: return s.Color(255,200,0);
      case RED:    return s.Color(255,0,0);
      case GREEN:  return s.Color(0,255,0);
      case PURPLE: return s.Color(160,0,160);
      case WHITE: default: return s.Color(255,255,255);
    }
  }
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
  static void fromMask(Adafruit_NeoPixel& s, const char* rows[10], uint32_t color){
    s.clear();
    for(uint8_t y=0; y<10; ++y){
      const char* r = rows[y];
      for(uint8_t x=0; x<10; ++x){
        if (r[x] == 'X') setXY(s, x, y, color);
      }
    }
    s.show();
  }
  static void shape(Adafruit_NeoPixel& s, ShapeId id, Thickness t, ColorName c){
    const char*** set = (t == THICK) ? THICK_SET : THIN_SET;
    fromMask(s, set[id], pxColor(s, c));
  }
};

// ===================== Minimal ADXL345 driver =====================
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
    return (read8(REG_DEVID) == 0xE5);
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

// ===================== Face detection tuning =====================
const float ALPHA      = 0.25f;  // EMA
const float MIN_G      = 6.5f;   // m/s^2 threshold on dominant axis
const float HYSTERESIS = 0.6f;   // m/s^2 bias to last axis
const uint16_t PERIOD  = 40;     // ms loop ~25 Hz

const float LSB_TO_MPS2 = 0.0039f * 9.80665f;
const int8_t SIGN_X = +1, SIGN_Y = +1, SIGN_Z = +1;

// ===================== State =====================
float xf = 0.0f, yf = 0.0f, zf = 9.81f;
Axis  lastAxis = AX_NONE;
Face  lastFace = FACE_UNKNOWN;

// ===================== LED state booleans (indicator diodes) =====================
bool ledUpState=false, ledDownState=false, ledLeftState=false, ledRightState=false, ledFrontState=false, ledBackState=false;

// ===================== Matrix color for Shape 1 =====================
static const ColorName MATRIX_COLOR = WHITE;

// ===================== CONFIG: which faces have matrices =====================
struct PanelMap { Face face; Adafruit_NeoPixel* strip; Thickness thick; };
PanelMap kPanels[] = {
  { FACE_LEFT,    &stripThin,  THIN  },  // Matrix #1 shows Shape 1 when UP is active
  { FACE_BACK, &stripThick, THICK },  // Matrix #2 shows Shape 1 when RIGHT is active
};

// ===================== App helpers (namespaced to defeat auto-prototypes) =====================
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

  static inline void allOffIndicators() {
    digitalWrite(LED_UP,    LOW);
    digitalWrite(LED_DOWN,  LOW);
    digitalWrite(LED_LEFT,  LOW);
    digitalWrite(LED_RIGHT, LOW);
    digitalWrite(LED_FRONT, LOW);
    digitalWrite(LED_BACK,  LOW);
  }

  static inline void applyLedBools() {
    digitalWrite(LED_UP,     ledUpState    ? HIGH : LOW);
    digitalWrite(LED_DOWN,   ledDownState  ? HIGH : LOW);
    digitalWrite(LED_LEFT,   ledLeftState  ? HIGH : LOW);
    digitalWrite(LED_RIGHT,  ledRightState ? HIGH : LOW);
    digitalWrite(LED_FRONT,  ledFrontState ? HIGH : LOW);
    digitalWrite(LED_BACK,   ledBackState  ? HIGH : LOW);
  }

  static inline void setOnlyFaceIndicators(Face f){
    ledUpState = ledDownState = ledLeftState = ledRightState = ledFrontState = ledBackState = false;
    switch(f){
      case FACE_UP:    ledUpState    = true; break;
      case FACE_DOWN:  ledDownState  = true; break;
      case FACE_LEFT:  ledLeftState  = true; break;
      case FACE_RIGHT: ledRightState = true; break;
      case FACE_FRONT: ledFrontState = true; break;
      case FACE_BACK:  ledBackState  = true; break;
      default: break;
    }
    applyLedBools();
  }

  static inline void clearStrip(Adafruit_NeoPixel& s){ s.clear(); s.show(); }

  // Show Shape 1 on the panel mapped to `f`, clear other panels.
  static inline void updatePanelsForFace(Face f){
    clearStrip(stripThin);
    clearStrip(stripThick);
    for (auto &pm : kPanels){
      if (pm.face == f && pm.strip){
        Draw::shape(*pm.strip, SHAPE_1_THIN /*index 0*/, pm.thick, MATRIX_COLOR);
      }
    }
  }
}

// ===================== Arduino lifecycle =====================
static void initIndicators() {
  pinMode(LED_UP,     OUTPUT);
  pinMode(LED_DOWN,   OUTPUT);
  pinMode(LED_LEFT,   OUTPUT);
  pinMode(LED_RIGHT,  OUTPUT);
  pinMode(LED_FRONT,  OUTPUT);
  pinMode(LED_BACK,   OUTPUT);
  App::allOffIndicators();
}

void setup(){
  Serial.begin(115200);
  initIndicators();

  // Start I2C
  Wire.begin(SDA_PIN, SCL_PIN);
  delay(5);

  if (!ADXL345::begin()) {
    Serial.println("Failed to find ADXL345!");
    while (true) { delay(1000); }
  }

  // Seed EMA with first sample
  int16_t rx, ry, rz;
  if (ADXL345::readXYZ_raw(rx, ry, rz)) {
    xf = (SIGN_X * rx) * LSB_TO_MPS2;
    yf = (SIGN_Y * ry) * LSB_TO_MPS2;
    zf = (SIGN_Z * rz) * LSB_TO_MPS2;
  }

  // Panels
  stripThin.begin();  stripThin.setBrightness(BRIGHTNESS);  stripThin.show();
  stripThick.begin(); stripThick.setBrightness(BRIGHTNESS); stripThick.show();

  Serial.println("Ready. Face → indicator LED + mapped matrix shows Shape 1.");
}

void loop(){
  // Read ADXL345
  int16_t rx, ry, rz;
  if (!ADXL345::readXYZ_raw(rx, ry, rz)) { delay(PERIOD); return; }

  // Convert & smooth
  float ax_mps2 = (SIGN_X * rx) * LSB_TO_MPS2;
  float ay_mps2 = (SIGN_Y * ry) * LSB_TO_MPS2;
  float az_mps2 = (SIGN_Z * rz) * LSB_TO_MPS2;

  xf = App::ema(xf, ax_mps2, ALPHA);
  yf = App::ema(yf, ay_mps2, ALPHA);
  zf = App::ema(zf, az_mps2, ALPHA);

  float ax = fabsf(xf), ay = fabsf(yf), az = fabsf(zf);

  // Hysteresis
  if (lastAxis == AX_X)      ax += HYSTERESIS;
  else if (lastAxis == AX_Y) ay += HYSTERESIS;
  else if (lastAxis == AX_Z) az += HYSTERESIS;

  // Dominant axis
  Axis dom; bool pos; float domMag;
  if (ax >= ay && ax >= az)      { dom = AX_X; pos = (xf > 0); domMag = ax; }
  else if (ay >= ax && ay >= az) { dom = AX_Y; pos = (yf > 0); domMag = ay; }
  else                           { dom = AX_Z; pos = (zf > 0); domMag = az; }

  Face face = FACE_UNKNOWN;
  if (domMag >= MIN_G) face = App::faceFromAxis(dom, pos);

  if (face != lastFace){
    Serial.println(App::faceName(face));

    if (face == FACE_UNKNOWN){
      App::allOffIndicators();
      App::clearStrip(stripThin);
      App::clearStrip(stripThick);
    } else {
      App::setOnlyFaceIndicators(face); // light the diode
      App::updatePanelsForFace(face);   // show Shape 1 on mapped matrix (if any)
    }
    lastFace = face;
  }

  lastAxis = dom;
  delay(PERIOD);
}
