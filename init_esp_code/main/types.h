#ifndef TYPES_H
#define TYPES_H

#include <Arduino.h>

// ═══════════════════════════════════════════════════════════════
//                        HARDWARE TYPES
// ═══════════════════════════════════════════════════════════════

// ───────────────────────── Faces ─────────────────────────
enum FaceId : uint8_t {
  FACE_UP = 0,
  FACE_DOWN,
  FACE_LEFT,
  FACE_RIGHT,
  FACE_BACK,
  FACE_FRONT,
  FACE_UNKNOWN,
  FACE_COUNT = 6
};

struct Vec3 {
  float x;
  float y;
  float z;
};

// ───────────────────────── Colors ─────────────────────────
enum ColorId : uint8_t {
  COLOR_BLACK = 0,
  COLOR_BLUE,
  COLOR_GREEN,
  COLOR_YELLOW,
  COLOR_RED,
  COLOR_PURPLE,
  COLOR_CYAN,
  COLOR_ORANGE,
  COLOR_WHITE,
  COLOR_COUNT
};

// Color shades structure (X, Y, Z, C variants)
struct ColorShades {
  uint32_t x;  // Primary shade
  uint32_t y;  // Lighter shade
  uint32_t z;  // Lightest shade
  uint32_t c;  // Darkest shade
};

// ───────────────────────── Shapes ─────────────────────────
enum ShapeId : uint8_t {
  SHAPE_SQUARE_8X8 = 0,     // legacy name; rendered at SHAPE_SIZE (10x10)
  SHAPE_CIRCLE_6X6,
  SHAPE_ARROW_UP,
  SHAPE_ARROW_DOWN,
  SHAPE_ARROW_LEFT,
  SHAPE_ARROW_RIGHT,
  SHAPE_ARROW_RIGHT_GLITCHED,
  SHAPE_CROSS,
  SHAPE_SMILEY,
  SHAPE_FULL,
  SHAPE_COUNT
};

// ───────────────────────── Display modes ─────────────────────────
enum DisplayMode : uint8_t {
  DISPLAY_STATIC = 0,         // stays until deleted
  MODE_DISAPPEAR_INACTIVE,  // placeholder for future (not used yet)
  MODE_TIMED                // disappears after timeout
  
};

// Shape layer structure
struct ShapeLayer {
  ShapeId shapeId;
  ColorId colorId;
  DisplayMode mode;
  uint32_t timestamp;
  uint32_t timeout;
  bool active;
};

// ───────────────────────── Animations ─────────────────────────
enum AnimationId : uint8_t {
  ANIM_START_GAME = 0,
  ANIM_END_GAME,
  ANIM_COUNTDOWN_321,
  ANIM_ARROW_TO_CROSS,
  ANIM_KING,
  ANIM_COUNT
};

// ───────────────────────── IMU State ─────────────────────────
struct ImuState {
  FaceId upFace;
  float tiltPercent;
  bool isLocked;
  float ax;
  float ay;
  float az;
};

// ───────────────────────── Display Settings ─────────────────────────
#ifndef BRIGHTNESS
#define BRIGHTNESS 50  // LED brightness (0-255)
#endif

// ═══════════════════════════════════════════════════════════════
//                        GAME TYPES
// ═══════════════════════════════════════════════════════════════

// Arrow direction definitions
enum ArrowDirection : uint8_t {
  DIR_UP = 0,
  DIR_DOWN,
  DIR_LEFT,
  DIR_RIGHT,
  DIR_COUNT
};

// Audio phrase types
enum AudioPhrase : uint8_t {
  PHRASE_SIMON_SAYS = 0,    // Follow arrow
  PHRASE_DIMON_SAYS,        // Opposite direction
  PHRASE_SIMONA_SAYS,       // Opposite direction
  PHRASE_SVETLIO_SAYS,      // Opposite direction
  PHRASE_NIKI_SAYS,         // Opposite direction
  PHRASE_ADI_SAYS,          // Opposite direction
  PHRASE_COUNT
};

// Game state machine states
enum GameState : uint8_t {
  STATE_IDLE = 0,
  STATE_GAME_START_ANIMATION,
  STATE_INITIAL_COUNTDOWN,
  STATE_PAUSE,
  STATE_PLAYING_AUDIO,
  STATE_WAITING_RESPONSE,
  STATE_LOCKED_CONFIRMATION,
  STATE_ROUND_END,
  STATE_GAME_END_ANIMATION,
  STATE_GAME_OVER
};

// Round data structure
struct RoundData {
  AudioPhrase phrase;           // Which phrase was "said"
  ArrowDirection arrowDir;      // Arrow shown on cube
  FaceId arrowFace;             // Face where arrow appears
  FaceId expectedFace;          // Expected correct face
  uint32_t roundStartTime;      // When round started (after pause)
  uint32_t responseStartTime;   // When player can respond
  uint32_t lockStartTime;       // When correct face was locked
  bool isSimonSays;             // True if "Simon says"
  bool responded;               // Player made a move
  bool correct;                 // Response was correct
  uint32_t responseTime;        // Time to respond (ms)
  ColorId arrowColor;           // Color for this round's arrow
  uint32_t countdownStartedAt;  // Track when countdown started
  uint32_t countdownDuration;   // Track countdown total duration
};

// Game statistics structure
struct GameStats {
  uint16_t totalRounds;
  uint16_t correctRounds;
  uint16_t wrongRounds;
  uint16_t omissions;           // Failed to act when should have
  uint16_t commissions;         // Acted when should not have
  uint32_t totalResponseTime;   // Sum of all response times
  uint16_t responseCount;       // Number of valid responses
};

#endif