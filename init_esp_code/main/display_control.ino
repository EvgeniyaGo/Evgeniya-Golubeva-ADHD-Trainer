#include "display_control.h"
#include "shape_definitions.h"
#include "types.h"

// LED strip objects
Adafruit_NeoPixel stripUp(MATRIX_PIXELS, PIN_LED_UP, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel stripDown(MATRIX_PIXELS, PIN_LED_DOWN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel stripLeft(MATRIX_PIXELS, PIN_LED_LEFT, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel stripRight(MATRIX_PIXELS, PIN_LED_RIGHT, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel stripBack(MATRIX_PIXELS, PIN_LED_BACK, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel stripFront(MATRIX_PIXELS, PIN_LED_FRONT, NEO_GRB + NEO_KHZ800);

static Adafruit_NeoPixel* faceStrips[FACE_COUNT] = {
  &stripUp, &stripDown, &stripLeft, &stripRight, &stripBack, &stripFront
};

// Face rotation compensation (0, 1, 2, 3, -1 = 0°, 90°, 180°, 270°, -90°)
static int8_t faceRotations[FACE_COUNT] = {0, 0, 0, 0, 0, 0};
/*
 [0] FACE_UP    → PIN_LED_UP    (26)
 [1] FACE_DOWN  → PIN_LED_DOWN  (12)
 [2] FACE_LEFT  → PIN_LED_LEFT  (33)
 [3] FACE_RIGHT → PIN_LED_RIGHT (32)
 [4] FACE_BACK  → PIN_LED_BACK  (14)
 [5] FACE_FRONT → PIN_LED_FRONT (27)
*/


// Layer management per face
static ShapeLayer faceLayers[FACE_COUNT][MAX_LAYERS_PER_FACE];
static uint8_t layerCount[FACE_COUNT] = {0};

// Countdown state
static bool countdownActive = false;
static uint32_t countdownStartTime = 0;
static uint32_t countdownDuration = 0;
static FaceId countdownFace = FACE_UNKNOWN;
static ColorId countdownColor = COLOR_BLUE;
static uint8_t countdownPixelsRemaining = (MATRIX_WIDTH * 4) - 4;

// Color definitions
static ColorShades colorPalette[COLOR_COUNT];

// Initialize color palette
void initColorPalette() {
  // Blue
  colorPalette[COLOR_BLUE].x = stripUp.Color(0, 0, 200);
  colorPalette[COLOR_BLUE].y = stripUp.Color(50, 50, 255);
  colorPalette[COLOR_BLUE].z = stripUp.Color(120, 120, 255);
  colorPalette[COLOR_BLUE].c = stripUp.Color(0, 0, 100);
  
  // Red
  colorPalette[COLOR_RED].x = stripUp.Color(200, 0, 0);
  colorPalette[COLOR_RED].y = stripUp.Color(255, 50, 50);
  colorPalette[COLOR_RED].z = stripUp.Color(255, 120, 120);
  colorPalette[COLOR_RED].c = stripUp.Color(100, 0, 0);
  
  // Green
  colorPalette[COLOR_GREEN].x = stripUp.Color(0, 200, 0);
  colorPalette[COLOR_GREEN].y = stripUp.Color(50, 255, 50);
  colorPalette[COLOR_GREEN].z = stripUp.Color(120, 255, 120);
  colorPalette[COLOR_GREEN].c = stripUp.Color(0, 100, 0);
  
  // Yellow
  colorPalette[COLOR_YELLOW].x = stripUp.Color(200, 200, 0);
  colorPalette[COLOR_YELLOW].y = stripUp.Color(255, 255, 50);
  colorPalette[COLOR_YELLOW].z = stripUp.Color(255, 255, 150);
  colorPalette[COLOR_YELLOW].c = stripUp.Color(100, 100, 0);
  
  // Purple
  colorPalette[COLOR_PURPLE].x = stripUp.Color(150, 0, 200);
  colorPalette[COLOR_PURPLE].y = stripUp.Color(200, 50, 255);
  colorPalette[COLOR_PURPLE].z = stripUp.Color(220, 120, 255);
  colorPalette[COLOR_PURPLE].c = stripUp.Color(75, 0, 100);
  
  // Cyan
  colorPalette[COLOR_CYAN].x = stripUp.Color(0, 200, 200);
  colorPalette[COLOR_CYAN].y = stripUp.Color(50, 255, 255);
  colorPalette[COLOR_CYAN].z = stripUp.Color(120, 255, 255);
  colorPalette[COLOR_CYAN].c = stripUp.Color(0, 100, 100);
  
  // Orange
  colorPalette[COLOR_ORANGE].x = stripUp.Color(255, 100, 0);
  colorPalette[COLOR_ORANGE].y = stripUp.Color(255, 150, 50);
  colorPalette[COLOR_ORANGE].z = stripUp.Color(255, 200, 120);
  colorPalette[COLOR_ORANGE].c = stripUp.Color(127, 50, 0);

  colorPalette[COLOR_BLACK].x = stripUp.Color(0, 0, 0);
  colorPalette[COLOR_BLACK].y = stripUp.Color(0, 0, 0);
  colorPalette[COLOR_BLACK].z = stripUp.Color(0, 0, 0);
  colorPalette[COLOR_BLACK].c = stripUp.Color(0, 0, 0);
}

void initDisplay() {
  initColorPalette();
  
  for (uint8_t i = 0; i < FACE_COUNT; i++) {
    faceStrips[i]->begin();
    faceStrips[i]->setBrightness(BRIGHTNESS);
    faceStrips[i]->clear();
    faceStrips[i]->show();
    
    for (uint8_t j = 0; j < MAX_LAYERS_PER_FACE; j++) {
      faceLayers[i][j].active = false;
    }
    layerCount[i] = 0;
  }
  
  Serial.println("[Display] Initialized");
}

ColorShades getColorShades(ColorId color) {
  if (color < COLOR_COUNT) return colorPalette[color];
  return colorPalette[COLOR_BLUE];
}

void setFaceRotation(FaceId face, int8_t rotationSteps) {
  if (face < FACE_COUNT) {
    faceRotations[face] = rotationSteps;
  }
}

// Rotate pixel coordinates by rotation steps (90° increments)
void rotatePixel(uint8_t &x, uint8_t &y, int8_t steps) {
  steps = steps % 4;
  if (steps < 0) steps += 4;
  
  for (int8_t i = 0; i < steps; i++) {
    uint8_t temp = x;
    x = MATRIX_WIDTH - 1 - y;
    y = temp;
  }
}

// Convert 2D coordinates to 1D pixel index
uint16_t coordToIndex(uint8_t x, uint8_t y) {
  if (x >= MATRIX_WIDTH || y >= MATRIX_HEIGHT) return 0xFFFF;
  return y * MATRIX_WIDTH + x;
}

void mapToDisplay(FaceId face, ShapeId shape, ColorId color, DisplayMode mode, uint32_t timeout) {
  if (face >= FACE_COUNT || shape >= SHAPE_COUNT || color >= COLOR_COUNT) return;
  
  // Find existing layer or create new one
  int8_t layerIdx = -1;
  for (uint8_t i = 0; i < MAX_LAYERS_PER_FACE; i++) {
    if (faceLayers[face][i].active && faceLayers[face][i].shapeId == shape) {
      layerIdx = i;
      break;
    }
  }
  
  if (layerIdx == -1) {
    for (uint8_t i = 0; i < MAX_LAYERS_PER_FACE; i++) {
      if (!faceLayers[face][i].active) {
        layerIdx = i;
        break;
      }
    }
  }
  
  if (layerIdx == -1) {
    Serial.println("[Display] No free layer slots");
    return;
  }
  
  faceLayers[face][layerIdx].shapeId = shape;
  faceLayers[face][layerIdx].colorId = color;
  faceLayers[face][layerIdx].mode = mode;
  faceLayers[face][layerIdx].timestamp = millis();
  faceLayers[face][layerIdx].timeout = timeout;
  faceLayers[face][layerIdx].active = true;
  
  renderFace(face);
}

void deleteShape(FaceId face, ShapeId shape) {
  if (face >= FACE_COUNT) return;
  
  for (uint8_t i = 0; i < MAX_LAYERS_PER_FACE; i++) {
    if (faceLayers[face][i].active && faceLayers[face][i].shapeId == shape) {
      faceLayers[face][i].active = false;
    }
  }
  
  renderFace(face);
}

void clearFace(FaceId face) {
  if (face >= FACE_COUNT) return;
  
  for (uint8_t i = 0; i < MAX_LAYERS_PER_FACE; i++) {
    faceLayers[face][i].active = false;
  }
  
  faceStrips[face]->clear();
  faceStrips[face]->show();
}

void clearAllFaces() {
  for (uint8_t i = 0; i < FACE_COUNT; i++) {
    clearFace((FaceId)i);
  }
}

void recolorShape(FaceId face, ShapeId shape, ColorId newColor) {
  if (face >= FACE_COUNT || newColor >= COLOR_COUNT) return;
  
  for (uint8_t i = 0; i < MAX_LAYERS_PER_FACE; i++) {
    if (faceLayers[face][i].active && faceLayers[face][i].shapeId == shape) {
      faceLayers[face][i].colorId = newColor;
    }
  }
  
  renderFace(face);
}

// Render shape layer to buffer
void renderShapeLayer(uint32_t* buffer, const ShapeLayer& layer, int8_t rotation) {
  const char* shapeDef = getShapeDefinition(layer.shapeId);
  if (!shapeDef) return;
  
  ColorShades shades = getColorShades(layer.colorId);
  
  for (uint8_t y = 0; y < SHAPE_SIZE; y++) {
    for (uint8_t x = 0; x < SHAPE_SIZE; x++) {
      char pixel = pgm_read_byte(&shapeDef[y * (SHAPE_SIZE + 1) + x]);
      if (pixel == '.') continue;
      
      uint8_t rx = x, ry = y;
      rotatePixel(rx, ry, rotation);
      uint16_t idx = coordToIndex(rx, ry);
      if (idx == 0xFFFF) continue;
      
      switch (pixel) {
        case 'X': buffer[idx] = shades.x; break;
        case 'Y': buffer[idx] = shades.y; break;
        case 'Z': buffer[idx] = shades.z; break;
        case 'C': buffer[idx] = shades.c; break;
      }
    }
  }
}

// Render countdown border on buffer
void renderCountdownBorder(uint32_t* buffer, ColorShades shades) {
  if (!countdownActive) return;

  const uint8_t PERIMETER_LEN = (MATRIX_WIDTH * 4) - 4; // 10x10 => 36
  const uint8_t toDraw = (countdownPixelsRemaining > PERIMETER_LEN) ? PERIMETER_LEN : countdownPixelsRemaining;

  auto idxToXY = [](uint8_t idx, uint8_t &x, uint8_t &y) {
    // Clockwise, starting at (0,0):
    // top row (0,0 -> 9,0) : 10
    // right col (9,1 -> 9,9) : 9
    // bottom row (8,9 -> 0,9) : 9
    // left col (0,8 -> 0,1) : 8
    if (idx < MATRIX_WIDTH) {                 // top
      x = idx; y = 0;
    } else if (idx < MATRIX_WIDTH + (MATRIX_HEIGHT - 1)) { // right
      x = MATRIX_WIDTH - 1;
      y = (idx - MATRIX_WIDTH) + 1;
    } else if (idx < MATRIX_WIDTH + 2*(MATRIX_HEIGHT - 1)) { // bottom
      uint8_t k = idx - (MATRIX_WIDTH + (MATRIX_HEIGHT - 1));
      x = (MATRIX_WIDTH - 2) - k;
      y = MATRIX_HEIGHT - 1;
    } else {                                  // left
      uint8_t k = idx - (MATRIX_WIDTH + 2*(MATRIX_HEIGHT - 1));
      x = 0;
      y = (MATRIX_HEIGHT - 2) - k;
    }
  };

  // Draw remaining pixels
  for (uint8_t i = 0; i < toDraw; i++) {
    uint8_t x, y;
    idxToXY(i, x, y);
    uint16_t p = coordToIndex(x, y);
    if (p != 0xFFFF) buffer[p] = shades.c;
  }
}

void renderFace(FaceId face) {
  if (face >= FACE_COUNT) return;
  
  Adafruit_NeoPixel* strip = faceStrips[face];
  uint32_t buffer[MATRIX_PIXELS] = {0};
  
  // Render all active layers bottom to top
  for (uint8_t i = 0; i < MAX_LAYERS_PER_FACE; i++) {
    if (faceLayers[face][i].active) {
      renderShapeLayer(buffer, faceLayers[face][i], faceRotations[face]);
    }
  }
  
  // Render countdown on top if this is the active face
  if (countdownActive && face == countdownFace) {
    ColorShades shades = getColorShades(countdownColor);
    renderCountdownBorder(buffer, shades);
  }
  
  // Push buffer to strip
  for (uint16_t i = 0; i < MATRIX_PIXELS; i++) {
    strip->setPixelColor(i, buffer[i]);
  }
  strip->show();
}

void startCountdown(uint32_t durationMs) {
  countdownActive = true;
  countdownStartTime = millis();
  countdownDuration = durationMs;
  countdownFace = FACE_UNKNOWN;
  countdownPixelsRemaining = (MATRIX_WIDTH * 4) - 4; // full perimeter (10x10 => 36)
  countdownColor = COLOR_BLUE;
  Serial.printf("[Countdown] Started: %dms\n", durationMs);
}

void updateCountdown(FaceId activeFace) {
  if (!countdownActive) return;

  const uint32_t now = millis();
  const uint32_t elapsed = now - countdownStartTime;

  if (elapsed >= countdownDuration) {
    countdownPixelsRemaining = 0;
    stopCountdown();
    return;
  }

  // Full perimeter pixels (10x10 => 36) -> remove pixel-by-pixel
  const uint32_t total = countdownDuration;
  const uint8_t totalPixels = (MATRIX_WIDTH * 4) - 4;
  const uint32_t remainingMs = total - elapsed;

  // ceil so the first pixel doesn't disappear instantly
  uint32_t px = (remainingMs * totalPixels + total - 1) / total;
  if (px > totalPixels) px = totalPixels;
  countdownPixelsRemaining = (uint8_t)px;

  // Move countdown to active face if changed
  if (activeFace != countdownFace && activeFace < FACE_COUNT) {
    FaceId oldFace = countdownFace;
    countdownFace = activeFace;

    if (oldFace < FACE_COUNT) {
      renderFace(oldFace);
    }
  }

  if (countdownFace < FACE_COUNT) {
    renderFace(countdownFace);
  }
}

void stopCountdown() {
  if (!countdownActive) return;
  
  countdownActive = false;
  if (countdownFace < FACE_COUNT) {
    renderFace(countdownFace);
  }
  Serial.println("[Countdown] Stopped");
}

void setCountdownColor(ColorId color) {
  if (color >= COLOR_COUNT) return;
  if (countdownColor == color) return;
  countdownColor = color;
  if (countdownActive && countdownFace < FACE_COUNT) {
    renderFace(countdownFace);
  }
}

bool isCountdownActive() {
  return countdownActive;
}

// Animation helper function
void renderAnimationFrame(FaceId face, const char* frameDef, uint8_t frameSize, ColorId colors[], uint8_t numColors) {
  if (face >= FACE_COUNT || !frameDef) return;
  
  Adafruit_NeoPixel* strip = faceStrips[face];
  if (!strip) return;
  
  strip->clear();
  
  ColorShades shades[3];
  for (uint8_t i = 0; i < numColors && i < 3; i++) {
    shades[i] = getColorShades(colors[i]);
  }
  
  for (uint8_t y = 0; y < frameSize; y++) {
    for (uint8_t x = 0; x < frameSize; x++) {
      char pixel = pgm_read_byte(&frameDef[y * (frameSize + 1) + x]);
      
      if (pixel == '.') continue;
      
      uint8_t colorIdx = pixel - '0';
      if (colorIdx >= numColors) continue;
      
      uint16_t idx = coordToIndex(x, y);
      if (idx == 0xFFFF) continue;
      
      strip->setPixelColor(idx, shades[colorIdx].x);
    }
  }
  
  strip->show();
}