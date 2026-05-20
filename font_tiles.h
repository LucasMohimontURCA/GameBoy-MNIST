#ifndef FONT_TILES_H
#define FONT_TILES_H
#include <stdint.h>

#define FONT_TILE_COUNT 43
extern const uint8_t font_tile_data[];

/* If you load font tiles starting at VRAM index FONT_BASE, then
   tile index for character c = FONT_BASE + FONT_OFFSET[c]. */

#define FT_SPACE 0
#define FT_0 1
#define FT_1 2
#define FT_2 3
#define FT_3 4
#define FT_4 5
#define FT_5 6
#define FT_6 7
#define FT_7 8
#define FT_8 9
#define FT_9 10
#define FT_A 11
#define FT_B 12
#define FT_C 13
#define FT_D 14
#define FT_E 15
#define FT_F 16
#define FT_G 17
#define FT_H 18
#define FT_I 19
#define FT_J 20
#define FT_K 21
#define FT_L 22
#define FT_M 23
#define FT_N 24
#define FT_O 25
#define FT_P 26
#define FT_Q 27
#define FT_R 28
#define FT_S 29
#define FT_T 30
#define FT_U 31
#define FT_V 32
#define FT_W 33
#define FT_X 34
#define FT_Y 35
#define FT_Z 36
#define FT_COLON 37
#define FT_DASH 38
#define FT_DOT 39
#define FT_QMARK 40
#define FT_ARROW 41
#define FT_SLASH 42

#endif