#if 0
// ───────────────────────── 1) Types ──────────────────────────────
// We only keep Face, because the simple pattern code doesn't need Axis/GameState.
enum Face {
  FACE_UP,
  FACE_DOWN,
  FACE_LEFT,
  FACE_RIGHT,
  FACE_FRONT,
  FACE_BACK,
  FACE_UNKNOWN
};

// ───────────────────────── 2) Panel / LED config ────────────────
// Logical matrix size (10x10 WS2812B)
static const uint8_t  W = 10;
static const uint8_t  H = 10;

// Global brightness (0..255)
static const uint8_t  BRIGHTNESS = 128;

// Mapping options (adjust if any panel is wired differently)
static const bool     SERPENTINE = false;   // true if rows zig-zag
static const bool     FLIP_X     = false;
static const bool     FLIP_Y     = false;
static const bool     SWAP_XY    = false;

// ───────────────────────── 3) Pins (6 matrices) ──────────────────
// ⚠️ Adjust these pins to your real wiring.
#define PIN_FACE_UP     26   // matrix #1
#define PIN_FACE_DOWN   12   // matrix #2
#define PIN_FACE_LEFT   33   // matrix #3
#define PIN_FACE_RIGHT  25   // matrix #4
#define PIN_FACE_FRONT  27   // matrix #5
#define PIN_FACE_BACK   14   // matrix #6

// ───────────────────────── 4) Strips (declare once) ─────────────
Adafruit_NeoPixel stripUp    (W * H, PIN_FACE_UP,    NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel stripDown  (W * H, PIN_FACE_DOWN,  NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel stripLeft  (W * H, PIN_FACE_LEFT,  NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel stripRight (W * H, PIN_FACE_RIGHT, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel stripFront (W * H, PIN_FACE_FRONT, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel stripBack  (W * H, PIN_FACE_BACK,  NEO_GRB + NEO_KHZ800);

// ───────────────────────── 5) Panel mapping ─────────────────────
struct Panel {
  Face face;
  Adafruit_NeoPixel* strip;
};

static Panel kPanels[] = {
  { FACE_UP,    &stripUp    },
  { FACE_DOWN,  &stripDown  },
  { FACE_LEFT,  &stripLeft  },
  { FACE_RIGHT, &stripRight },
  { FACE_FRONT, &stripFront },
  { FACE_BACK,  &stripBack  },
};

static const uint8_t NUM_PANELS = sizeof(kPanels) / sizeof(kPanels[0]);
#endif
