#ifndef DISPLAY_CONTROL_H
#define DISPLAY_CONTROL_H

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include "types.h"

// ───────────────────────── Pins ─────────────────────────
// (per your cube wiring)
#define PIN_LED_UP    26
#define PIN_LED_DOWN  12
#define PIN_LED_LEFT  33
#define PIN_LED_RIGHT 32
#define PIN_LED_BACK  14
#define PIN_LED_FRONT 27

// ───────────────────────── Matrix config ─────────────────────────
#define MATRIX_WIDTH   10
#define MATRIX_HEIGHT  10
#define MATRIX_PIXELS  (MATRIX_WIDTH * MATRIX_HEIGHT)

// Layer system
#define MAX_LAYERS_PER_FACE  4

// ───────────────────────── Public API ─────────────────────────
void initDisplay();

// Clear helpers
void clearFace(FaceId face);
void clearAllFaces();

// Add/update a shape layer on a face.
// timeout is used only for MODE_TIMED (ms). For other modes you can pass 0.
void mapToDisplay(FaceId face, ShapeId shape, ColorId color, DisplayMode mode, uint32_t timeout = 0);

// Delete a specific shape layer from a face
void deleteShape(FaceId face, ShapeId shape);

// Recolor a shape layer on a face (keeps same mode/timeout)
void recolorShape(FaceId face, ShapeId shape, ColorId newColor);

// Countdown overlay (drawn on top of layers on the "active" face)
void startCountdown(uint32_t durationMs);
void updateCountdown(FaceId activeFace);
void stopCountdown();
void setCountdownColor(ColorId color);
bool isCountdownActive();

// Color shades for a given ColorId
ColorShades getColorShades(ColorId color);

// Per-face rotation compensation (0..3 steps of 90° clockwise)
void setFaceRotation(FaceId face, int8_t rotationSteps);

// Internal rendering (exposed for testing)
void renderFace(FaceId face);

// Animation helper: render raw frame data directly (bypasses layer system)
void renderAnimationFrame(FaceId face, const char* frameDef, uint8_t frameSize, ColorId colors[], uint8_t numColors);

#endif
