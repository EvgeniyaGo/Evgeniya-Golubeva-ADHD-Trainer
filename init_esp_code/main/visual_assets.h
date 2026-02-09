#ifndef VISUAL_ASSETS_H
#define VISUAL_ASSETS_H

#include <Arduino.h>
#include <pgmspace.h>

// Visual asset definitions (10x10 matrix)
// '.' = off
// 'X','Y','Z','C' = shade selection for shapes
// '0','1','2' = palette index for animations
#define VISUAL_SIZE 10

// ═══════════════════════════════════════════════════════════════
//                        SHAPE ASSETS DATA
// ═══════════════════════════════════════════════════════════════

static const char VISUAL_FULL_DATA[] PROGMEM =
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

static const char VISUAL_SQUARE_8X8_DATA[] PROGMEM =
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

static const char VISUAL_CIRCLE_6X6_DATA[] PROGMEM =
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

static const char VISUAL_ARROW_UP_DATA[] PROGMEM =
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

static const char VISUAL_ARROW_DOWN_DATA[] PROGMEM =
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

static const char VISUAL_ARROW_LEFT_DATA[] PROGMEM =
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

static const char VISUAL_ARROW_RIGHT_DATA[] PROGMEM =
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

static const char VISUAL_ARROW_RIGHT_GLITCHED_DATA[] PROGMEM =
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

static const char VISUAL_CROSS_DATA[] PROGMEM =
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

static const char VISUAL_SMILEY_DATA[] PROGMEM =
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

// ═══════════════════════════════════════════════════════════════
//                    ANIMATION FRAME ASSETS DATA
// ═══════════════════════════════════════════════════════════════

// Start Game Animation (expanding circles)
static const char VISUAL_ANIM_START_01_DATA[] PROGMEM =
"..........\n"
"..........\n"
"....00....\n"
"...0..0...\n"
"..0....0..\n"
"..0....0..\n"
"...0..0...\n"
"....00....\n"
"..........\n"
"..........\n";

static const char VISUAL_ANIM_START_02_DATA[] PROGMEM =
"..........\n"
"...0000...\n"
"..0....0..\n"
".0......0.\n"
".0......0.\n"
".0......0.\n"
".0......0.\n"
"..0....0..\n"
"...0000...\n"
"..........\n";

static const char VISUAL_ANIM_START_03_DATA[] PROGMEM =
"..000000..\n"
".0......0.\n"
"0.000000.0\n"
"0.0....0.0\n"
"0.0....0.0\n"
"0.0....0.0\n"
"0.0....0.0\n"
"0.000000.0\n"
".0......0.\n"
"..000000..\n";

// Countdown Animation (3, 2, 1)
static const char VISUAL_ANIM_COUNT_3_DATA[] PROGMEM =
"..........\n"
"..000000..\n"
"........0.\n"
"........0.\n"
"..000000..\n"
"........0.\n"
"........0.\n"
"..000000..\n"
"..........\n"
"..........\n";

static const char VISUAL_ANIM_COUNT_2_DATA[] PROGMEM =
"..........\n"
"..000000..\n"
"........0.\n"
"........0.\n"
"..000000..\n"
".0........\n"
".0........\n"
".00000000.\n"
"..........\n"
"..........\n";

static const char VISUAL_ANIM_COUNT_1_DATA[] PROGMEM =
"..........\n"
"...00.....\n"
"..000.....\n"
"...00.....\n"
"...00.....\n"
"...00.....\n"
"...00.....\n"
"..000000..\n"
"..........\n"
"..........\n";

// Arrow to Cross Animation (error indication)
static const char VISUAL_ANIM_ARROW_GLITCH_01_DATA[] PROGMEM =
"..........\n"
"......0...\n"
"......00..\n"
"......000.\n"
"..00000000\n"
"......000.\n"
"......00..\n"
"......0...\n"
"..........\n"
"..........\n";

static const char VISUAL_ANIM_ARROW_GLITCH_02_DATA[] PROGMEM =
"..........\n"
"......0...\n"
"......00..\n"
".....0000.\n"
"..00000000\n"
".......00.\n"
"......00..\n"
"......0...\n"
"..........\n"
"..........\n";

static const char VISUAL_ANIM_ARROW_CROSS_DATA[] PROGMEM =
"0........0\n"
".0......0.\n"
"..0....0..\n"
"...0..0...\n"
"....00....\n"
"...0..0...\n"
"..0....0..\n"
".0......0.\n"
"0........0\n"
"..........\n";

// King Animation (victory)
static const char VISUAL_ANIM_KING_01_DATA[] PROGMEM =
"....0..0..\n"
"...000000.\n"
"..0..0..0.\n"
"..0..0..0.\n"
"..000000..\n"
"...0..0...\n"
"..0....0..\n"
".0......0.\n"
"..........\n"
"..........\n";

static const char VISUAL_ANIM_KING_02_DATA[] PROGMEM =
"....0..0..\n"
"...000000.\n"
"..00.0.00.\n"
"...0.0.0..\n"
"..000000..\n"
"...00.0...\n"
"..0....0..\n"
".0......0.\n"
"..........\n"
"..........\n";

static const char VISUAL_ANIM_KING_03_DATA[] PROGMEM =
"....0..0..\n"
"...000000.\n"
"..0.000.0.\n"
"...0..0...\n"
"..000000..\n"
"..0.00.0..\n"
".0......0.\n"
"0........0\n"
"..........\n"
"..........\n";

#endif
