#include <Adafruit_NeoPixel.h>   // Install "Adafruit NeoPixel" via Library Manager

// -------- Pins & panel geometry --------
#define PIN_10x10  18
#define PIN_10x10_2    19 

const uint16_t W1 = 10, H1 = 10;   // 10x10 panel
const uint16_t W2 =  10, H2 =  10;   // 8x8 panel

// Set true if rows zig-zag (common on matrices)
const bool SERPENTINE_10x10 = true;
const bool SERPENTINE_10x10_2   = true;

// -------- Behavior --------
const uint8_t  BRIGHTNESS = 64;   // 0..255
const uint16_t STEP_MS    = 300;  // ms per step

// -------- Strips --------
Adafruit_NeoPixel panel10x10(W1 * H1, PIN_10x10, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel panel10x10_2  (W2 * H2, PIN_10x10_2,   NEO_GRB + NEO_KHZ800);

// (row, col) -> linear index with optional serpentine wiring
static inline uint16_t idx2D(uint16_t row, uint16_t col, uint16_t W, bool serpentine) {
  if (serpentine && (row & 1)) return row * W + (W - 1 - col);
  return row * W + col;
}

uint16_t row10 = 0;
uint16_t row10_2  = 0;

void setup() {
  // Share GND between ESP32 and both panels
  panel10x10.begin();
  panel10x10_2.begin();
  panel10x10.setBrightness(BRIGHTNESS);
  panel10x10_2.setBrightness(BRIGHTNESS);
  panel10x10.clear(); panel10x10.show();
  panel10x10_2.clear();   panel10x10_2.show();
}

void loop() {
  // --- draw next row on 10x10 ---
  panel10x10.clear();
  for (uint16_t c = 0; c < W1; ++c) {
    uint16_t i = idx2D(row10, c, W1, SERPENTINE_10x10);
    panel10x10.setPixelColor(i, panel10x10.Color(255, 40, 0)); // orange-red
  }
  panel10x10.show();

  // --- draw next row on 8x8 ---
  panel10x10_2.clear();
  for (uint16_t c = 0; c < W2; ++c) {
    uint16_t i = idx2D(row10_2, c, W2, SERPENTINE_10x10_2);
    panel10x10_2.setPixelColor(i, panel10x10_2.Color(0, 80, 255)); // blue
  }
  panel10x10_2.show();

  // step rows (wrap)
  row10 = (row10 + 1) % H1;
  row10_2  = (row10_2  + 1) % H2;

  delay(STEP_MS);
}
