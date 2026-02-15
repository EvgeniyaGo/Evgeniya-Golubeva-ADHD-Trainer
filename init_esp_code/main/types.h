#ifndef TYPES_H
#define TYPES_H

#include <Arduino.h>

// Faces
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

// Colors
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

// Shapes 
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

// Display modes
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

// IMU State
struct ImuState {
  FaceId upFace;
  float tiltPercent;
  bool isLocked;
  float ax;
  float ay;
  float az;
};

// Display Settings
#ifndef BRIGHTNESS
#define BRIGHTNESS 50  // LED brightness (0-255)
#endif

#endif
