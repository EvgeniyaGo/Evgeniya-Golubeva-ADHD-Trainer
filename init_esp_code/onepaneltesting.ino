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

  // Use just ONE panel: the RIGHT face on pin D25
  // (stripRight is defined in Types.h)
  static Adafruit_NeoPixel& panel = stripRight;

  // 10-color palette (from your image, currently your test colors)
  static const RGB ROW_GRADIENT[10] = {
    {252,   3,   3}, // row 0
    {252,   3,   3}, // row 0
    {  3,  32, 252}, // row 2
    {  3,  32, 252}, // row 2
    { 15, 252,   3}, // row 4
    { 15, 252,   3}, // row 4
    {252,   3, 206}, // row 6
    {  0,   0,   0}, // row 7
    {255, 255, 255}, // row 8
    {  0,   0,   0}, // row 9
  };

  // 25% of your configured BRIGHTNESS
  static const uint8_t STATIC_BRIGHTNESS = BRIGHTNESS / 4;

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
    Serial.println();
    Serial.println("Starting single-panel diffuser demo on D25 (RIGHT face)...");

    // Init only the single panel
    panel.begin();
    panel.setBrightness(STATIC_BRIGHTNESS);
    panel.clear();
    panel.show();

    // Paint rows on this one panel
    paintPanelRows(panel);

    Serial.println("Static 10-row pattern drawn on the single panel.");
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
