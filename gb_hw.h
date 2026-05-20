/* Minimal Game Boy hardware helpers — no GBDK needed.
 * SDCC sm83 target.
 */
#ifndef GB_HW_H
#define GB_HW_H
#include <stdint.h>

/* Hardware registers (memory-mapped at 0xFF00..) */
#define rJOYP   (*(volatile uint8_t *)0xFF00)
#define rDIV    (*(volatile uint8_t *)0xFF04)   /* 16384 Hz counter (1 tick ≈ 61 µs) */
#define rLCDC   (*(volatile uint8_t *)0xFF40)
#define rSTAT   (*(volatile uint8_t *)0xFF41)
#define rLY     (*(volatile uint8_t *)0xFF44)
#define rBGP    (*(volatile uint8_t *)0xFF47)

/* Sound registers — square channel 1 (sweep) */
#define rNR10   (*(volatile uint8_t *)0xFF10)
#define rNR11   (*(volatile uint8_t *)0xFF11)
#define rNR12   (*(volatile uint8_t *)0xFF12)
#define rNR13   (*(volatile uint8_t *)0xFF13)
#define rNR14   (*(volatile uint8_t *)0xFF14)
/* Master sound control */
#define rNR50   (*(volatile uint8_t *)0xFF24)
#define rNR51   (*(volatile uint8_t *)0xFF25)
#define rNR52   (*(volatile uint8_t *)0xFF26)

#define LCDC_ON         0x80
#define LCDC_BG_ON      0x01
#define LCDC_BG_MAP_9800 0x00
#define LCDC_BG_DATA_8000 0x10

#define J_RIGHT  0x01
#define J_LEFT   0x02
#define J_UP     0x04
#define J_DOWN   0x08
#define J_A      0x10
#define J_B      0x20
#define J_SELECT 0x40
#define J_START  0x80

extern volatile uint8_t vbl_flag;

void wait_vblank(void);
uint8_t read_joypad(void);

/* Load tile data to VRAM. LCD MUST BE OFF. */
void load_tiles(uint8_t tile_idx, uint8_t count_tiles, const uint8_t *src);

/* Safe tilemap writes: wait for VBlank, then write one tile / row of tiles.
 * Use for small, infrequent updates while LCD is on. */
void set_tile(uint8_t col, uint8_t row, uint8_t tile);
void set_tiles_row(uint8_t col, uint8_t row, uint8_t n, const uint8_t *tiles);

/* Raw tilemap writes: no wait. LCD MUST BE OFF, or caller must be in VBlank. */
void set_tile_raw(uint8_t col, uint8_t row, uint8_t tile);
void set_tiles_row_raw(uint8_t col, uint8_t row, uint8_t n, const uint8_t *tiles);

void lcd_on(void);
void lcd_off(void);

/* ---- Sound API ---- */
/* Enable the APU (must be called once at boot). */
void sound_init(void);

/* Play a 3-note ascending arpeggio (used for "prediction correct"). */
void play_success_sound(void);

/* Play a descending 2-tone "buzz" (used for "prediction incorrect"). */
void play_error_sound(void);

/* ---- Timer API ----
 * The DIV register at 0xFF04 ticks at 16384 Hz (1 tick ≈ 61.035 µs).
 * It wraps every ~15.6 ms. For longer measurements we track it across frames.
 *
 * Usage:
 *   timer_start();   // before the work
 *   ...do stuff...
 *   uint16_t ms = timer_stop_ms();  // milliseconds elapsed
 */
void timer_start(void);
uint16_t timer_stop_ms(void);

#endif
