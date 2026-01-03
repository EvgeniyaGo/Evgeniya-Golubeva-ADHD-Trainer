#if 0
// === ESP32: 6 WS2812B matrices — simple 2x2 / 4x4 pattern ===
// Libraries: Adafruit NeoPixel

#include <Adafruit_NeoPixel.h>
#include <Types_six.h>   // must define: W, H, SWAP_XY, FLIP_X, FLIP_Y, SERPENTINE,
                         // BRIGHTNESS, NUM_PANELS, kPanels[]

/*
  Assumed from Types_six.h:

  struct Panel {
    Face face;
    Adafruit_NeoPixel* strip;
    // ... maybe more fields
  };

  extern const uint8_t NUM_PANELS;
  extern Panel kPanels[];
*/

// ───────────────────────── Draw helpers (struct Draw) ─────────
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

  static void clear(Adafruit_NeoPixel& s){
    s.clear();
    s.show();
  }

  static void square2x2(Adafruit_NeoPixel& s, uint32_t color){
    s.clear();
    uint8_t cx = W/2;
    uint8_t cy = H/2;
    for (int dy=-1; dy<=0; ++dy){
      for (int dx=-1; dx<=0; ++dx){
        setXY(s, cx + dx, cy + dy, color);
      }
    }
    s.show();
  }

  static void square4x4(Adafruit_NeoPixel& s, uint32_t color){
    s.clear();
    uint8_t cx = W/2;
    uint8_t cy = H/2;
    for (int dy=-2; dy<=1; ++dy){
      for (int dx=-2; dx<=1; ++dx){
        setXY(s, cx + dx, cy + dy, color);
      }
    }
    s.show();
  }
};

// ───────────────────────── Pattern state ─────────────────────
enum PatternMode : uint8_t {
  PATTERN_2X2 = 0,
  PATTERN_4X4 = 1
};

static PatternMode gMode       = PATTERN_2X2;
static uint32_t    gLastChange = 0;

// How often to switch between 2x2 and 4x4 (ms)
static const uint32_t PATTERN_INTERVAL_MS = 3000;

// Color used for all panels & both sizes
static uint32_t gColor = 0;  // filled in setup()

// ───────────────────────── Helpers for all 6 panels ──────────
static void initAllPanels() {
  for (uint8_t i = 0; i < NUM_PANELS; ++i) {
    Adafruit_NeoPixel* s = kPanels[i].strip;
    if (!s) continue;
    s->begin();
    s->setBrightness(BRIGHTNESS);
    s->show();   // clear
  }
}

static void draw2x2OnAllPanels() {
  for (uint8_t i = 0; i < NUM_PANELS; ++i) {
    Adafruit_NeoPixel* s = kPanels[i].strip;
    if (!s) continue;
    Draw::square2x2(*s, gColor);
  }
}

static void draw4x4OnAllPanels() {
  for (uint8_t i = 0; i < NUM_PANELS; ++i) {
    Adafruit_NeoPixel* s = kPanels[i].strip;
    if (!s) continue;
    Draw::square4x4(*s, gColor);
  }
}

// ───────────────────────── Arduino setup ─────────────────────
void setup() {
  Serial.begin(115200);
  Serial.println("Simple 2x2 / 4x4 pattern on all 6 matrices, IMU removed.");

  initAllPanels();

  // Pick any color you like here:
  // gColor = kPanels[0].strip->Color(255,   0,   0); // red
  // gColor = kPanels[0].strip->Color(  0, 255,   0); // green
  // gColor = kPanels[0].strip->Color(  0,   0, 255); // blue
  gColor = kPanels[0].strip->Color(0, 0, 255); // example: blue

  // Start with 2x2 on all panels
  draw2x2OnAllPanels();
  gMode       = PATTERN_2X2;
  gLastChange = millis();
}

// ───────────────────────── Arduino loop ──────────────────────
void loop() {
  uint32_t now = millis();

  if (now - gLastChange >= PATTERN_INTERVAL_MS) {
    gLastChange = now;
      gMode = PATTERN_2X2;
      draw2x2OnAllPanels();
      Serial.println("Switched to 2x2 on all panels");
  }

  // no delay(); free to add other stuff later
}
#endif