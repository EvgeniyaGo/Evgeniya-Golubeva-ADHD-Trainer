#ifndef DISPLAY_CONTROL_H
#define DISPLAY_CONTROL_H

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include "types.h"

// Pins (per cube wiring)
#define PIN_LED_UP    32
#define PIN_LED_DOWN  27
#define PIN_LED_LEFT  14
#define PIN_LED_RIGHT 12
#define PIN_LED_BACK  33
#define PIN_LED_FRONT 26

// Matrix config
#define MATRIX_WIDTH   10
#define MATRIX_HEIGHT  10
#define MATRIX_PIXELS  (MATRIX_WIDTH * MATRIX_HEIGHT)

// Layer system
#define MAX_LAYERS_PER_FACE  4

// Functions
void initDisplay();

void clearFace(FaceId face);
void clearAllFaces();
void mapToDisplay(FaceId face, ShapeId shape, ColorId color, DisplayMode mode, uint32_t timeout = 0); // Add/update a shape layer on a face. -timeout is used only for MODE_TIMED (ms). For other modes you can pass 0.
void deleteShape(FaceId face, ShapeId shape);
void recolorShape(FaceId face, ShapeId shape, ColorId newColor);

void startCountdown(uint32_t durationMs);
void updateCountdown(FaceId activeFace);
void stopCountdown();
void setCountdownColor(ColorId color);
bool isCountdownActive();

ColorShades getColorShades(ColorId color);

void setFaceRotation(FaceId face, int8_t rotationSteps);
void renderFace(FaceId face);

#endif
