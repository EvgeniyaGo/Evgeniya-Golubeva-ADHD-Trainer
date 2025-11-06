// ───────────────────────── 1) Types FIRST ─────────────────────────
enum ColorName { BLUE, YELLOW, RED, GREEN, PURPLE, WHITE };
enum Axis      { AX_X, AX_Y, AX_Z, AX_NONE };
enum Face      { FACE_UP, FACE_DOWN, FACE_LEFT, FACE_RIGHT, FACE_FRONT, FACE_BACK, FACE_UNKNOWN };
enum GameState { PREPARE, HEADSTART, DISCOVERY, HOLD, SUCCESS_FB, FAIL_FB, INTER_TRIAL };

// ───────────────────────── 2) Pins & panel config ─────────────────
#define LED_UP     5
#define LED_DOWN   18
#define LED_LEFT   22
#define LED_RIGHT  23
#define LED_FRONT  21
#define LED_BACK   19

#define SDA_PIN    25
#define SCL_PIN    26

#define PIN_THIN   13   // matrix #1
#define PIN_THICK  14   // matrix #2

static const uint8_t  W = 10, H = 10;
static const uint8_t  BRIGHTNESS = 128;     // 0..255
static const bool     SERPENTINE = false;   // set true if your panel is zig-zag
static const bool     FLIP_X = false, FLIP_Y = false, SWAP_XY = false;

// ───────────────────────── 3) Color palette + dictionary ─────────
struct RGB { uint8_t r,g,b; };
static constexpr RGB PALETTE[] = {
  /* BLUE   */ {  0,   0, 255},
  /* YELLOW */ {255, 200,   0},
  /* RED    */ {255,   0,   0},
  /* GREEN  */ {  0, 255,   0},
  /* PURPLE */ {160,   0, 160},
  /* WHITE  */ {255, 255, 255},
};
struct ColorEntry { const char* name; uint8_t id; };
static const ColorEntry COLOR_DICT[] = {
  {"blue",   (uint8_t)BLUE},   {"yellow", (uint8_t)YELLOW}, {"red",   (uint8_t)RED},
  {"green",  (uint8_t)GREEN},  {"purple", (uint8_t)PURPLE}, {"white", (uint8_t)WHITE},
};
static inline char lowerc(char c){ return (c>='A'&&c<='Z') ? (c-'A'+'a') : c; }
static bool eqIgnoreCase(const char* a, const char* b){
  if (!a || !b) return false;
  while (*a && *b){ if (lowerc(*a++) != lowerc(*b++)) return false; }
  return *a == *b;
}
// IMPORTANT: return raw uint8_t to dodge Arduino auto-prototype bug
static uint8_t colorIdRaw(const char* name){
  for (auto &e : COLOR_DICT) if (eqIgnoreCase(name, e.name)) return e.id;
  return (uint8_t)WHITE; // default
}
#define COLOR_ID(nameLiteral) ((ColorName)colorIdRaw(nameLiteral))
#define COLOR_OF(nameEnum)    (PALETTE[(nameEnum)])

// ───────────────────────── 4) Strips (declare ONCE) ──────────────
Adafruit_NeoPixel stripThin (W * H, PIN_THIN,  NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel stripThick(W * H, PIN_THICK, NEO_GRB + NEO_KHZ800);

// Pack color using any strip (defaults to stripThin)
static inline uint32_t packColor(ColorName c, Adafruit_NeoPixel* s = nullptr) {
  Adafruit_NeoPixel& ref = s ? *s : stripThin;
  RGB rgb = COLOR_OF(c);
  return ref.Color(rgb.r, rgb.g, rgb.b);
}
static inline uint32_t packColor(const char* cName, Adafruit_NeoPixel* s = nullptr){
  return packColor((ColorName)colorIdRaw(cName), s);
}

// ───────────────────────── 5) Panel mapping ──────────────────────
struct PanelMap { Face face; Adafruit_NeoPixel* strip; };
static PanelMap kPanels[] = {
  { FACE_LEFT,  &stripThin  },  // Panel A
  { FACE_BACK,  &stripThick },  // Panel B
};
static const uint8_t NUM_PANELS = sizeof(kPanels)/sizeof(kPanels[0]);

// ───────────────────────── 6) Face detection config/state ────────
const float ALPHA      = 0.25f;  // EMA
const float MIN_G      = 6.5f;   // m/s^2 threshold on dominant axis
const float HYSTERESIS = 0.6f;   // m/s^2 bias to last axis
const uint16_t PERIOD  = 40;     // ms loop ~25 Hz
const float LSB_TO_MPS2 = 0.0039f * 9.80665f;
const int8_t SIGN_X = +1, SIGN_Y = +1, SIGN_Z = +1;

float xf = 0.0f, yf = 0.0f, zf = 9.81f;
Axis  lastAxis = AX_NONE;
Face  rawFace  = FACE_UNKNOWN;

const uint32_t T_STABLE_MS = 200;
Face  stableFace = FACE_UNKNOWN;
uint32_t faceChangeSince = 0;
