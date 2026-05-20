#ifndef UI_TILES_H
#define UI_TILES_H
#include <stdint.h>

extern const uint8_t ui_tile_data[];
extern const uint8_t ui_tile_count;

/* Tile indices when loaded at VRAM base 0:
 *   0..3 = grayscale shades
 *   4    = cursor outline (empty)
 *   5    = cursor on filled pixel
 *   6..  = font tiles (43 chars, see font_tiles.h FT_*)
 *
 * Font is loaded starting at index UI_FONT_BASE = 6.
 */
#define UI_TILE_PIXEL(shade) ((uint8_t)(shade))   /* shade 0..3 */
#define UI_TILE_CURSOR_EMPTY  4
#define UI_TILE_CURSOR_FILLED 5
#define UI_FONT_BASE          6

/* Char tile = UI_FONT_BASE + FT_<char> */
#define FT_TILE(ft) ((uint8_t)(UI_FONT_BASE + (ft)))

#endif
