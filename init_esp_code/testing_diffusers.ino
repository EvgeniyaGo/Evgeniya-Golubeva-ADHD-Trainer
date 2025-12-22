#if 0
#include <Adafruit_NeoPixel.h>
#include <Types.h>

// ───────────────────────── 1) XY helper with your mapping ────────
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
};

// ───────────────────────── 2) Everything in a namespace ───────────
namespace DiffuserDemo {

  // Find the panel for a given face
  static Adafruit_NeoPixel* stripForFace(Face f) {
    for (uint8_t i = 0; i < NUM_PANELS; ++i) {
      if (kPanels[i].face == f) return kPanels[i].strip;
    }
    return nullptr;
  }

  // 10-color palette (from your image)
  // #9E0142, #D53E4F, #F46D43, #FDAE61, #FEE08B,
  // #E6F598, #ABDDA4, #66C2A5, #3288BD, #5E4FA2
  static const RGB ROW_GRADIENT[10] = {
    {252, 3, 3}, // row 0
    {252, 3, 3}, // row 0
    {3, 32, 252}, // row 1
    {3, 32, 252}, // row 1
    {15, 252, 3}, // row 2
    {15, 252, 3}, // row 2
    {252, 3, 206}, // row 3
    {252, 3, 206}, // row 3
    {255, 255, 255}, // row 3
    {255, 255, 255}, // row 3
  };

  // 25% of your configured BRIGHTNESS
  static const uint8_t STATIC_BRIGHTNESS = BRIGHTNESS;

  // Helper: get 32-bit pixel color for a given row
  static uint32_t rowColorPixel(Adafruit_NeoPixel &s, uint8_t row){
    const RGB &c = ROW_GRADIENT[row % 10];
    return s.Color(c.r, c.g, c.b);
  }

  // Fill an entire panel with 10 horizontal rows, each its own color
  static void paintPanelRows(Adafruit_NeoPixel &s){
    for (uint8_t y = 0; y < H; ++y) {
      uint32_t col = rowColorPixel(s, y);
      for (uint8_t x = 0; x < W; ++x) {
        Draw::setXY(s, x, y, col);
      }
    }
    s.show();
  }

  // Called from global setup()
  void setupDemo() {
    Serial.begin(115200);

    // Init all strips
    for (uint8_t i = 0; i < NUM_PANELS; ++i) {
      Adafruit_NeoPixel *s = kPanels[i].strip;
      s->begin();
      s->setBrightness(STATIC_BRIGHTNESS);
      s->clear();
      s->show();
    }

    // Faces to paint: left, front, upward
    const Face facesToPaint[] = { FACE_UP, FACE_DOWN, FACE_LEFT, FACE_RIGHT, FACE_FRONT, FACE_BACK };
    const uint8_t numFaces = sizeof(facesToPaint) / sizeof(facesToPaint[0]);

    for (uint8_t i = 0; i < numFaces; ++i) {
      if (Adafruit_NeoPixel *s = stripForFace(facesToPaint[i])) {
        paintPanelRows(*s);
      }
    }

    Serial.println("Static 10-row pattern drawn on LEFT, FRONT, UP.");
  }

  // Called from global loop()
  void loopDemo() {
    // nothing dynamic; keep LEDs as they are
    delay(1000);
  }
}

// ───────────────────────── 3) Arduino entry points ────────────────
void setup() {
  DiffuserDemo::setupDemo();
}

void loop() {
  DiffuserDemo::loopDemo();
}
#endif