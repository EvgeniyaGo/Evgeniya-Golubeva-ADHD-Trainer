#ifndef ANIMATIONS_H
#define ANIMATIONS_H

#include <Arduino.h>
#include "types.h"

// Play animation on specified face
// remain: true = last frame stays, false = clear after animation
void playAnimation(AnimationId animId, FaceId face, bool remain);

// Update currently playing animations (call in loop)
void updateAnimations();

// Check if any animation is playing
bool isAnimationPlaying();

// Stop all animations
void stopAllAnimations();

// Move currently playing animation to a different face (used for "follow face")
void setActiveAnimationFace(FaceId face);

#endif
