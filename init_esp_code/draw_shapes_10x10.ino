// === WS2812B 10x10 – THIN (D18) + THICK (D19) =====================
// Board: ESP32
// Library: Adafruit NeoPixel
#include <Adafruit_NeoPixel.h>

// ---------- Types ----------
enum Thickness { THIN, THICK };
enum ColorName { BLUE, YELLOW, RED, GREEN, PURPLE, WHITE };
enum ShapeId {
  SHAPE_1_THIN = 0, SHAPE_2_THIN, SHAPE_3_THIN, SHAPE_4_THIN, SHAPE_5_THIN,
  SHAPE_6_THIN, SHAPE_7_THIN, SHAPE_8_THIN, SHAPE_9_THIN, SHAPE_10_THIN
};

// ---------- Pins & panel config ----------
#define PIN_THIN   13   // panel for THIN shapes
#define PIN_THICK  14   // panel for THICK shapes

static const uint8_t  W = 10, H = 10;
static const uint8_t  BRIGHTNESS = 128; //out of 255
static const uint16_t SHOW_MS     = 4000;   // time per shape

// Orientation (global)
static const bool FLIP_X = false;
static const bool FLIP_Y = false;
static const bool SWAP_XY = false;
// Set to true if your panel is serpentine-wired (zig-zag rows)
static const bool SERPENTINE = false;

// ---------- Strips ----------
Adafruit_NeoPixel stripThin (W * H, PIN_THIN,  NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel stripThick(W * H, PIN_THICK, NEO_GRB + NEO_KHZ800);

// ---------- SHAPES (THIN + THICK) ----------
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
  "....XX....",
  "..XX..XX..",
  ".X......X.",
  ".X......X.",
  "X........X",
  "X........X",
  ".X......X.",
  ".X......X.",
  "..XX..XX..",
  "....XX....",
};
const char* SHAPE2_THICK[10] = {
  "..XXXXXX..",
  ".XXXXXXXX.",
  "XXX....XXX",
  "XX......XX",
  "XX......XX",
  "XX......XX",
  "XX......XX",
  "XXX....XXX",
  ".XXXXXXXX.",
  "..XXXXXX..",
};

const char* SHAPE3_THIN[10] = {
  "....XX....",
  "...X..X...",
  "...X..X...",
  "..X....X..",
  "..X....X..",
  ".X......X.",
  ".X......X.",
  "X........X",
  "XXXXXXXXXX",
  "..........",
};
const char* SHAPE3_THICK[10] = {
  "....XX....",
  "...XXXX...",
  "..XX..XX..",
  "..XX..XX..",
  ".XX....XX.",
  ".XX....XX.",
  "XX......XX",
  "XX......XX",
  "XXXXXXXXXX",
  "XXXXXXXXXX",
};

const char* SHAPE4_THIN[10] = {
  "..XXXXXX..",
  ".XXX...XX.",
  "XXX.....X.",
  "XX........",
  "XX........",
  "XX........",
  "XX........",
  "XXX.....X.",
  ".XXX...XX.",
  "..XXXXXX..",
};
const char* SHAPE4_THICK[10] = {
  "..XXXXX...",
  ".XXXXXXXX.",
  "XXX....XX.",
  "XXX.....X.",
  "XX........",
  "XX........",
  "XXX.....X.",
  "XXX....XX.",
  ".XXXXXXXX.",
  "..XXXXX...",
};

const char* SHAPE5_THIN[10] = {
  "....XX....",
  "...X..X...",
  "..XX..XX..",
  "..X....X..",
  ".X......X.",
  ".X......X.",
  "..X....X..",
  "..XX..XX..",
  "...X..X...",
  "....XX....",
};
const char* SHAPE5_THICK[10] = {
  "....XX....",
  "...XXXX...",
  "..XX..XX..",
  ".XX....XX.",
  "XX......XX",
  "XX......XX",
  ".XX....XX.",
  "..XX..XX..",
  "...XXXX...",
  "....XX....",
};

const char* SHAPE6_THIN[10] = {
  "...XXXX...",
  "...X..X...",
  "...X..X...",
  "XXXX..XXXX",
  "X........X",
  "X........X",
  "XXXX..XXXX",
  "...X..X...",
  "...X..X...",
  "...XXXX...",
};
const char* SHAPE6_THICK[10] = {
  "..XXXXXX..",
  "..XXXXXX..",
  "XXXX..XXXX",
  "XXXX..XXXX",
  "XX......XX",
  "XX......XX",
  "XXXX..XXXX",
  "XXXX..XXXX",
  "..XXXXXX..",
  "..XXXXXX..",
};

const char* SHAPE7_THIN[10] = {
  "..........",
  "..........",
  "..........",
  ".......X..",
  ".......XX.",
  "XXXXXXXXXX",
  ".......XX.",
  ".......X..",
  "..........",
  "..........",
};
const char* SHAPE7_THICK[10] = {
  "..........",
  "......X...",
  "......XX..",
  "......XXX.",
  "XXXXXXXXXX",
  "XXXXXXXXXX",
  "......XXX.",
  "......XX..",
  "......X...",
  "..........",
};

const char* SHAPE8_THIN[10] = {
  "..........",
  "..........",
  ".......X..",
  ".......XX.",
  "....XXXXXX",
  "...XX..XX.",
  "...X...X..",
  "...X......",
  "...X......",
  "...X......",
};
const char* SHAPE8_THICK[10] = {
  "..........",
  ".......X..",
  ".......XX.",
  "....XXXXXX",
  "...XXXXXXX",
  "...XX..XX.",
  "...XX..X..",
  "...XX.....",
  "...XX.....",
  "...XX.....",
};

const char* SHAPE9_THIN[10] = {
  "..........",
  ".....X....",
  "....X.X...",
  "...X...X..",
  "....X.X...",
  ".....X....",
  ".....X....",
  ".....X....",
  ".....X....",
  ".....X....",
};
const char* SHAPE9_THICK[10] = {
  "..........",
  "....XX....",
  "...XXXX...",
  "..XX..XX..",
  "..XX..XX..",
  "...XXXX...",
  "....XX....",
  "....XX....",
  "....XX....",
  "....XX....",
};
const char* SHAPE10_THIN[10] = {
  "XXXXXXXXXX",
  "XXXXXXXXXX",
  "XXXXXXXXXX",
  "XXXXXXXXXX",
  "XXXXXXXXXX",
  "XXXXXXXXXX",
  "XXXXXXXXXX",
  "XXXXXXXXXX",
  "XXXXXXXXXX",
  "XXXXXXXXXX",
};
const char* SHAPE10_THICK[10] = {
  "XXXXXXXXXX",
  "XXXXXXXXXX",
  "XXXXXXXXXX",
  "XXXXXXXXXX",
  "XXXXXXXXXX",
  "XXXXXXXXXX",
  "XXXXXXXXXX",
  "XXXXXXXXXX",
  "XXXXXXXXXX",
  "XXXXXXXXXX",
};

// Registries
static const char** THIN_SET[10] = {
  SHAPE1_THIN, SHAPE2_THIN, SHAPE3_THIN, SHAPE4_THIN, SHAPE5_THIN,
  SHAPE6_THIN, SHAPE7_THIN, SHAPE8_THIN, SHAPE9_THIN, SHAPE10_THIN
};
static const char** THICK_SET[10] = {
  SHAPE1_THICK, SHAPE2_THICK, SHAPE3_THICK, SHAPE4_THICK, SHAPE5_THICK,
  SHAPE6_THICK, SHAPE7_THICK, SHAPE8_THICK, SHAPE9_THICK, SHAPE10_THICK
};

// ---------- Helpers (wrapped in a struct to avoid Arduino auto-prototypes) ----------
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
    // serpentine mapping: even rows L->R, odd rows R->L
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

// ---------- Demo ----------
void setup(){
  stripThin.begin();
  stripThin.setBrightness(BRIGHTNESS);
  stripThin.show();

  stripThick.begin();
  stripThick.setBrightness(BRIGHTNESS);
  stripThick.show();
}

void loop(){
//  Color per shape index
  ColorName colors[10] = { BLUE, YELLOW, RED, GREEN, PURPLE, WHITE, BLUE, YELLOW, GREEN, WHITE};

  for (int i = 0; i < 10; i++) {
    // Draw same index on both panels:
    //   THIN  -> D18
    //   THICK -> D19
    Draw::shape(stripThin,  (ShapeId)i, THIN,  colors[i]);
    Draw::shape(stripThick, (ShapeId)i, THICK, colors[i]);
    delay(SHOW_MS);
  }

  Draw::shape(stripThin,  SHAPE_10_THIN, THIN,   RED);
  Draw::shape(stripThick, SHAPE_10_THIN, THICK,  RED);
    delay(SHOW_MS);

  Draw::shape(stripThin,  SHAPE_10_THIN, THIN,   GREEN);
  Draw::shape(stripThick, SHAPE_10_THIN, THICK,  GREEN);
    delay(SHOW_MS);

  Draw::shape(stripThin,  SHAPE_10_THIN, THIN,   BLUE);
  Draw::shape(stripThick, SHAPE_10_THIN, THICK,  BLUE);
    delay(SHOW_MS);

  Draw::shape(stripThin,  SHAPE_10_THIN, THIN,   WHITE);
  Draw::shape(stripThick, SHAPE_10_THIN, THICK,  WHITE);
    delay(SHOW_MS);
}
