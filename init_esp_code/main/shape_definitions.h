#ifndef SHAPE_DEFINITIONS_H
#define SHAPE_DEFINITIONS_H

#include <Arduino.h>
#include <pgmspace.h>
#include "types.h"

// 10x10 shapes, stored as text (10 lines × 10 chars + '\n')
// '.' = off
// 'X','Y','Z','C' = shade selection (see ColorShades in display_control)

#define SHAPE_SIZE 10

//  Basic shapes 

static const char SHAPE_FULL_DEF[] PROGMEM =
"XXXXXXXXXX\n"
"XXXXXXXXXX\n"
"XXXXXXXXXX\n"
"XXXXXXXXXX\n"
"XXXXXXXXXX\n"
"XXXXXXXXXX\n"
"XXXXXXXXXX\n"
"XXXXXXXXXX\n"
"XXXXXXXXXX\n"
"XXXXXXXXXX\n";


static const char SHAPE_SQUARE_8X8_DEF[] PROGMEM =
"XXXXXXXXXX\n"
"X........X\n"
"X........X\n"
"X........X\n"
"X........X\n"
"X........X\n"
"X........X\n"
"X........X\n"
"X........X\n"
"XXXXXXXXXX\n";

static const char SHAPE_CIRCLE_6X6_DEF[] PROGMEM =
"..XXXXXX..\n"
".XXXXXXXX.\n"
"XXXXXXXXXX\n"
"XXX....XXX\n"
"XX......XX\n"
"XX......XX\n"
"XXX....XXX\n"
"XXXXXXXXXX\n"
".XXXXXXXX.\n"
"..XXXXXX..\n";

// Arrows 
static const char SHAPE_ARROW_UP_DEF[] PROGMEM =
"....XX....\n"
"...XXXX...\n"
"..XXXXXX..\n"
".XXXXXXXX.\n"
"....XX....\n"
"....XX....\n"
"....XX....\n"
"....XX....\n"
"....XX....\n"
"..........\n";

static const char SHAPE_ARROW_DOWN_DEF[] PROGMEM =
"..........\n"
"....XX....\n"
"....XX....\n"
"....XX....\n"
"....XX....\n"
"....XX....\n"
".XXXXXXXX.\n"
"..XXXXXX..\n"
"...XXXX...\n"
"....XX....\n";

static const char SHAPE_ARROW_LEFT_DEF[] PROGMEM =
"..........\n"
"...X......\n"
"..XX......\n"
".XXX......\n"
"XXXXXXXX..\n"
".XXX......\n"
"..XX......\n"
"...X......\n"
"..........\n"
"..........\n";

static const char SHAPE_ARROW_RIGHT_DEF[] PROGMEM =
"..........\n"
"......X...\n"
"......XX..\n"
"......XXX.\n"
"..XXXXXXXX\n"
"......XXX.\n"
"......XX..\n"
"......X...\n"
"..........\n"
"..........\n";

static const char SHAPE_ARROW_RIGHT_GLITCHED_DEF[] PROGMEM =
"..........\n"
"......X...\n"
"......XX..\n"
".....XXXX.\n"
"..XXXXXXXX\n"
".......XX.\n"
"......XX..\n"
"......X...\n"
"..........\n"
"..........\n";

//  Cross & Smiley 
static const char SHAPE_CROSS_DEF[] PROGMEM =
"X........X\n"
".X......X.\n"
"..X....X..\n"
"...X..X...\n"
"....XX....\n"
"...X..X...\n"
"..X....X..\n"
".X......X.\n"
"X........X\n"
"..........\n";

static const char SHAPE_SMILEY_DEF[] PROGMEM =
"..........\n"
"..XX..XX..\n"
"..XX..XX..\n"
"..........\n"
"..........\n"
"X........X\n"
".X......X.\n"
"..X....X..\n"
"...XXXX...\n"
"..........\n";

static inline const char* getShapeDefinition(ShapeId shapeId) { // used in renderShapeLayer in display_control
  switch (shapeId) {
    case SHAPE_FULL:                 return SHAPE_FULL_DEF;
    case SHAPE_SQUARE_8X8:           return SHAPE_SQUARE_8X8_DEF;
    case SHAPE_CIRCLE_6X6:           return SHAPE_CIRCLE_6X6_DEF;
    case SHAPE_ARROW_UP:             return SHAPE_ARROW_UP_DEF;
    case SHAPE_ARROW_DOWN:           return SHAPE_ARROW_DOWN_DEF;
    case SHAPE_ARROW_LEFT:           return SHAPE_ARROW_LEFT_DEF;
    case SHAPE_ARROW_RIGHT:          return SHAPE_ARROW_RIGHT_DEF;
    case SHAPE_ARROW_RIGHT_GLITCHED: return SHAPE_ARROW_RIGHT_GLITCHED_DEF;
    case SHAPE_CROSS:                return SHAPE_CROSS_DEF;
    case SHAPE_SMILEY:               return SHAPE_SMILEY_DEF;
    default:                         return nullptr;
  }
}

#endif
