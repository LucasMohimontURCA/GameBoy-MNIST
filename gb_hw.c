/* Minimal Game Boy hardware helpers. */
#include "gb_hw.h"

void wait_vblank(void) {
    vbl_flag = 0;
    while (!vbl_flag) {
        __asm__("halt");
        __asm__("nop");
    }
}

uint8_t read_joypad(void) {
    uint8_t btns;
    uint8_t result = 0;
    rJOYP = 0x20;
    btns = rJOYP;
    btns = rJOYP;
    if (!(btns & 0x01)) result |= J_RIGHT;
    if (!(btns & 0x02)) result |= J_LEFT;
    if (!(btns & 0x04)) result |= J_UP;
    if (!(btns & 0x08)) result |= J_DOWN;
    rJOYP = 0x10;
    btns = rJOYP;
    btns = rJOYP;
    if (!(btns & 0x01)) result |= J_A;
    if (!(btns & 0x02)) result |= J_B;
    if (!(btns & 0x04)) result |= J_SELECT;
    if (!(btns & 0x08)) result |= J_START;
    rJOYP = 0x30;
    return result;
}

static void mem_copy(uint8_t *dst, const uint8_t *src, uint16_t n) {
    while (n--) *dst++ = *src++;
}

void load_tiles(uint8_t tile_idx, uint8_t count_tiles, const uint8_t *src) {
    /* Must be called with LCD off, or in VBlank. */
    uint8_t *dst = (uint8_t *)0x8000 + ((uint16_t)tile_idx * 16);
    mem_copy(dst, src, (uint16_t)count_tiles * 16);
}

/* "Raw" tilemap writes — caller must guarantee LCD is off OR we're in VBlank.
 * No spin-wait, so safe to call many times in sequence without overflowing
 * the VBlank window. */
void set_tile_raw(uint8_t col, uint8_t row, uint8_t tile) {
    uint8_t *dst = (uint8_t *)0x9800 + ((uint16_t)row * 32) + col;
    *dst = tile;
}

void set_tiles_row_raw(uint8_t col, uint8_t row, uint8_t n, const uint8_t *tiles) {
    uint8_t *dst = (uint8_t *)0x9800 + ((uint16_t)row * 32) + col;
    while (n--) *dst++ = *tiles++;
}

/* "Safe" tilemap writes — wait until VBlank, then write.
 * Use for small, infrequent updates while the LCD is on (e.g., refreshing
 * a single tile during gameplay). For bulk redraws use lcd_off() + the _raw
 * variants. */
void set_tile(uint8_t col, uint8_t row, uint8_t tile) {
    /* Wait for VBlank (LY >= 144). If LCD is off, LY stays 0 forever, so
     * also exit if LCDC bit 7 is clear. */
    while ((rLCDC & 0x80) && rLY < 144) { }
    set_tile_raw(col, row, tile);
}

void set_tiles_row(uint8_t col, uint8_t row, uint8_t n, const uint8_t *tiles) {
    while ((rLCDC & 0x80) && rLY < 144) { }
    set_tiles_row_raw(col, row, n, tiles);
}

void lcd_on(void) {
    rBGP = 0xE4;
    rLCDC = LCDC_ON | LCDC_BG_ON | LCDC_BG_DATA_8000 | LCDC_BG_MAP_9800;
}

void lcd_off(void) {
    /* If LCD already off, nothing to do. */
    if ((rLCDC & 0x80) == 0) return;
    /* Wait for VBlank before disabling LCD (required to avoid damaging
     * real DMG hardware). */
    while (rLY < 144) { }
    rLCDC = 0;
}

/* ---- Sound ---- */

void sound_init(void) {
    /* Enable APU power, route both channels to both speakers, max volume. */
    rNR52 = 0x80;       /* APU on */
    rNR51 = 0xFF;       /* all channels to L+R */
    rNR50 = 0x77;       /* volume L=7, R=7 */
}

/* Play a single note on square channel 1.
 *   freq_lo, freq_hi : 11-bit frequency divided into low byte + 3 hi bits
 *   length: 0..63, sound duration = (64 - length) / 256 s
 *           (with length-enable bit set in NR14)
 */
static void play_note(uint8_t freq_lo, uint8_t freq_hi, uint8_t length) {
    rNR10 = 0x00;                       /* no sweep */
    rNR11 = (0x80) | (length & 0x3F);   /* duty 50% + length data */
    rNR12 = 0xF3;                       /* volume 15, env decay, sweep 3 */
    rNR13 = freq_lo;
    rNR14 = 0xC0 | (freq_hi & 0x07);    /* trigger + length enable */
}

/* Crude delay loop. Each iteration is roughly 10 M-cycles on GB.
 * delay_units of ~10000 = ~95 ms wall clock. */
static void delay_units(uint16_t n) {
    while (n--) {
        __asm__("nop"); __asm__("nop"); __asm__("nop");
    }
}

/* Note frequency table values (11-bit GB freq).
 * freq_hz = 131072 / (2048 - value), so value = 2048 - 131072/hz.
 *   C5 ~523Hz -> 1798 = 0x706
 *   E5 ~659Hz -> 1849 = 0x739
 *   G5 ~784Hz -> 1881 = 0x759
 *   C4 ~262Hz -> 1548 = 0x60C
 *   A3 ~220Hz -> 1452 = 0x5AC
 */
void play_success_sound(void) {
    /* 3 ascending notes: C5, E5, G5 */
    play_note(0x06, 0x07, 56);   /* C5, length ~125 ms */
    delay_units(5000);
    play_note(0x39, 0x07, 56);   /* E5 */
    delay_units(5000);
    play_note(0x59, 0x07, 48);   /* G5, longer */
    delay_units(5000);
}

void play_error_sound(void) {
    /* 2 descending notes: C5 -> A3 (slow buzz). */
    play_note(0x06, 0x07, 50);   /* C5 */
    delay_units(6000);
    play_note(0xAC, 0x05, 40);   /* A3 — low buzzy */
    delay_units(6000);
}

/* ---- Timer ---- */

/* The DIV register ticks at 16384 Hz (1 tick = 1/16384 s ≈ 61.035 µs).
 * It's 8-bit, so it wraps every 256/16384 ≈ 15.625 ms.
 * To measure longer durations we accumulate wraparounds in software.
 *
 * Implementation: at start we snapshot DIV. timer_stop_ms then estimates
 * elapsed time by counting wraps via wait_vblank ticks. Simpler approach
 * since we have a VBlank ISR at 59.73 Hz: count frames between start and
 * stop, then add the in-frame remainder from DIV.
 *
 * 1 frame on DMG = 17556 M-cycles = 70224 T-cycles = 16.74 ms.
 */
static uint8_t  timer_start_div;
static uint16_t timer_start_frame_count;
extern volatile uint16_t frame_counter; /* from crt0.s VBlank ISR */

void timer_start(void) {
    timer_start_div = rDIV;
    timer_start_frame_count = frame_counter;
}

uint16_t timer_stop_ms(void) {
    uint16_t frames = frame_counter - timer_start_frame_count;
    /* 1 frame ≈ 16.74 ms. Multiply by 67 then divide by 4 ≈ 16.75 ms/frame. */
    /* For small accuracy improvement: ms = frames * 67 / 4 */
    uint32_t ms = ((uint32_t)frames * 67) >> 2;
    /* Cap at uint16 max */
    if (ms > 65535) ms = 65535;
    return (uint16_t)ms;
}
