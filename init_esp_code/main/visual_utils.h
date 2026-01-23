#ifndef VISUAL_UTILS_H
#define VISUAL_UTILS_H

#include "visual_assets.h"
#include "types.h"

// Get visual asset definition by ID (unified function for shapes and animations)
inline const char* getVisualAsset(VisualId visualId) {
  switch (visualId) {
    // Shapes
    case VISUAL_FULL: return VISUAL_FULL_DATA;
    case VISUAL_SQUARE_8X8: return VISUAL_SQUARE_8X8_DATA;
    case VISUAL_CIRCLE_6X6: return VISUAL_CIRCLE_6X6_DATA;
    case VISUAL_ARROW_UP: return VISUAL_ARROW_UP_DATA;
    case VISUAL_ARROW_DOWN: return VISUAL_ARROW_DOWN_DATA;
    case VISUAL_ARROW_LEFT: return VISUAL_ARROW_LEFT_DATA;
    case VISUAL_ARROW_RIGHT: return VISUAL_ARROW_RIGHT_DATA;
    case VISUAL_ARROW_RIGHT_GLITCHED: return VISUAL_ARROW_RIGHT_GLITCHED_DATA;
    case VISUAL_CROSS: return VISUAL_CROSS_DATA;
    case VISUAL_SMILEY: return VISUAL_SMILEY_DATA;
    
    // Animation frames
    case VISUAL_ANIM_START_01: return VISUAL_ANIM_START_01_DATA;
    case VISUAL_ANIM_START_02: return VISUAL_ANIM_START_02_DATA;
    case VISUAL_ANIM_START_03: return VISUAL_ANIM_START_03_DATA;
    case VISUAL_ANIM_COUNT_3: return VISUAL_ANIM_COUNT_3_DATA;
    case VISUAL_ANIM_COUNT_2: return VISUAL_ANIM_COUNT_2_DATA;
    case VISUAL_ANIM_COUNT_1: return VISUAL_ANIM_COUNT_1_DATA;
    case VISUAL_ANIM_ARROW_GLITCH_01: return VISUAL_ANIM_ARROW_GLITCH_01_DATA;
    case VISUAL_ANIM_ARROW_GLITCH_02: return VISUAL_ANIM_ARROW_GLITCH_02_DATA;
    case VISUAL_ANIM_ARROW_CROSS: return VISUAL_ANIM_ARROW_CROSS_DATA;
    case VISUAL_ANIM_KING_01: return VISUAL_ANIM_KING_01_DATA;
    case VISUAL_ANIM_KING_02: return VISUAL_ANIM_KING_02_DATA;
    case VISUAL_ANIM_KING_03: return VISUAL_ANIM_KING_03_DATA;
    
    default: return nullptr;
  }
}

#endif
