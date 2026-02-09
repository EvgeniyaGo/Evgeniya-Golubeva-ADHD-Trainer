#include "animations.h"
#include "visual_assets.h"
#include "display_control.h"

// ───────────────────────── Animation timing ─────────────────────────
// Increase to slow down all animations globally.
#ifndef ANIM_FRAME_DURATION_MS
#define ANIM_FRAME_DURATION_MS 650
#endif
/*
struct AnimState {
  AnimationId animId = ANIM_START_GAME;
  FaceId      face  = FACE_UP;
  bool        active = false;
  bool        remain = false;       // keep last frame if true, else clear
  uint8_t     currentFrame = 0;
  uint8_t     totalFrames  = 0;
  uint32_t    nextFrameAt  = 0;     // millis() timestamp for next frame
  ColorId     colors[3]    = { COLOR_BLUE, COLOR_GREEN, COLOR_YELLOW }; // palette indices 0..2
};

static AnimState activeAnim;

static uint8_t getAnimFrameCount(AnimationId animId) {
  switch (animId) {
    case ANIM_START_GAME:      return 3;
    case ANIM_COUNTDOWN_321:   return 3;
    case ANIM_ARROW_TO_CROSS:  return 3;
    case ANIM_KING:            return 3;
    default:                   return 0;
  }
}

static const char* getAnimFrame(AnimationId animId, uint8_t frameIdx) {
  switch (animId) {
    case ANIM_START_GAME:
      if (frameIdx == 0) return (const char*)ANIM_START_FRAME01;
      if (frameIdx == 1) return (const char*)ANIM_START_FRAME02;
      if (frameIdx == 2) return (const char*)ANIM_START_FRAME03;
      break;

    case ANIM_COUNTDOWN_321:
      // IMPORTANT: order is 3,2,1 (visual countdown)
      if (frameIdx == 0) return (const char*)ANIM_COUNT_3;
      if (frameIdx == 1) return (const char*)ANIM_COUNT_2;
      if (frameIdx == 2) return (const char*)ANIM_COUNT_1;
      break;

    case ANIM_ARROW_TO_CROSS:
      if (frameIdx == 0) return (const char*)ANIM_ARROW_GLITCH_01;
      if (frameIdx == 1) return (const char*)ANIM_ARROW_GLITCH_02;
      if (frameIdx == 2) return (const char*)ANIM_ARROW_CROSS;
      break;

    case ANIM_KING:
      if (frameIdx == 0) return (const char*)ANIM_KING_01;
      if (frameIdx == 1) return (const char*)ANIM_KING_02;
      if (frameIdx == 2) return (const char*)ANIM_KING_03;
      break;

    default:
      break;
  }
  return nullptr;

}

static const uint8_t* getAnimFrameColors(AnimationId animId, uint8_t frameIdx) {
  // Each frame has its own palette (3 entries max) stored in PROGMEM.
  switch (animId) {
    case ANIM_START_GAME:
      if (frameIdx == 0) return (const uint8_t*)ANIM_START_FRAME01_COLORS;
      if (frameIdx == 1) return (const uint8_t*)ANIM_START_FRAME02_COLORS;
      if (frameIdx == 2) return (const uint8_t*)ANIM_START_FRAME03_COLORS;
      break;

    case ANIM_COUNTDOWN_321:
      if (frameIdx == 0) return (const uint8_t*)ANIM_COUNT_3_COLORS;
      if (frameIdx == 1) return (const uint8_t*)ANIM_COUNT_2_COLORS;
      if (frameIdx == 2) return (const uint8_t*)ANIM_COUNT_1_COLORS;
      break;

    case ANIM_ARROW_TO_CROSS:
      if (frameIdx == 0) return (const uint8_t*)ANIM_ARROW_GLITCH_01_COLORS;
      if (frameIdx == 1) return (const uint8_t*)ANIM_ARROW_GLITCH_02_COLORS;
      if (frameIdx == 2) return (const uint8_t*)ANIM_ARROW_CROSS_COLORS;
      break;

    case ANIM_KING:
      if (frameIdx == 0) return (const uint8_t*)ANIM_KING_01_COLORS;
      if (frameIdx == 1) return (const uint8_t*)ANIM_KING_02_COLORS;
      if (frameIdx == 2) return (const uint8_t*)ANIM_KING_03_COLORS;
      break;

    default:
      break;
  }
  return nullptr;
}

static void loadPaletteForFrame(AnimationId animId, uint8_t frameIdx, ColorId outColors[3]) {
  // Palette is 3 bytes: 0..COLOR_COUNT-1
  const uint8_t* pal = getAnimFrameColors(animId, frameIdx);
  if (!pal) return;

  for (uint8_t i = 0; i < 3; i++) {
    outColors[i] = (ColorId)pgm_read_byte(&pal[i]);
  }
}

static void renderCurrentFrame() {
  if (!activeAnim.active) return;

  const char* frameDef = getAnimFrame(activeAnim.animId, activeAnim.currentFrame);
  if (!frameDef) return;

  // Palette can vary per frame
  loadPaletteForFrame(activeAnim.animId, activeAnim.currentFrame, activeAnim.colors);

  renderAnimationFrame(activeAnim.face, frameDef, ANIM_FRAME_SIZE, activeAnim.colors, 3);
}

void playAnimation(AnimationId animId, FaceId face, bool remain) {
  if (animId >= ANIM_COUNT) return;
  if (face >= FACE_COUNT) return;

  activeAnim.animId = animId;
  activeAnim.face = face;
  activeAnim.active = true;
  activeAnim.remain = remain;
  activeAnim.currentFrame = 0;
  activeAnim.totalFrames = getAnimFrameCount(animId);
  activeAnim.nextFrameAt = millis(); // render immediately

  renderCurrentFrame();
}

void updateAnimations() {
  if (!activeAnim.active) return;

  const uint32_t now = millis();
  if (now < activeAnim.nextFrameAt) return;

  // Advance to next frame
  if (activeAnim.totalFrames == 0) {
    activeAnim.active = false;
    return;
  }

  if (activeAnim.currentFrame + 1 >= activeAnim.totalFrames) {
    // End of animation
    if (!activeAnim.remain) {
      clearFace(activeAnim.face);
    }
    activeAnim.active = false;
    return;
  }

  activeAnim.currentFrame++;
  activeAnim.nextFrameAt = now + ANIM_FRAME_DURATION_MS;

  renderCurrentFrame();
}

bool isAnimationPlaying() {
  return activeAnim.active;
}

void stopAllAnimations() {
  if (!activeAnim.active) return;
  clearFace(activeAnim.face);
  activeAnim.active = false;
}

void setActiveAnimationFace(FaceId face) {
  if (!activeAnim.active) return;
  if (face >= FACE_COUNT) return;
  if (face == activeAnim.face) return;

  // clear old face to avoid multi-face artifacts
  clearFace(activeAnim.face);
  activeAnim.face = face;

  // render same frame on new face
  renderCurrentFrame();
}
*/