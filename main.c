/* Game Boy MNIST CNN — full UI with title, menu, draw mode, credits.
 *
 * Drawing strategy (the fix for visual artifacts on real DMG):
 *   Bulk redraws turn the LCD off so we can write the tilemap freely without
 *   racing the PPU. The VBlank window is only ~10 scanlines (~1 ms) — long
 *   enough for a few tile writes but NOT for clearing the screen + writing
 *   text. Trying to bulk-write during active draw silently fails on real
 *   hardware (writes get blocked) and we get residual garbage from the
 *   previous scene.
 *
 *   Convention:
 *     - clear_bg / write_text / set_tiles_row_raw : LCD MUST BE OFF
 *     - set_tile (single tile)                    : LCD on, waits for VBlank
 *
 *   Each state's _enter() function: lcd_off() → bulk writes → lcd_on().
 *   Anything redrawing during gameplay also calls lcd_off/on if it's bulk.
 */
#include "gb_hw.h"
#include "weights.h"
#include "inference.h"
#include "ui_tiles.h"
#include "font_tiles.h"
#include "title_tiles.h"

#define SCR_W 20
#define SCR_H 18

#define STATE_TITLE    0
#define STATE_MENU     1
#define STATE_CNN      2
#define STATE_DRAW     3
#define STATE_CREDITS  4

static uint8_t g_state;
static uint8_t g_prev_btns;
static uint8_t g_menu_idx;

/* ---- Pseudo-random number generator ----
 * The GB has no hardware RNG. We use a 16-bit linear congruential generator.
 * It's seeded from the VBlank frame_counter the first time it's used — since
 * that depends on how long the user lingered on the title/menu, the seed is
 * effectively unpredictable. */
extern volatile uint16_t frame_counter;
static uint16_t g_rng_state;
static uint8_t  g_rng_seeded;

static uint8_t rng_next(void) {
    if (!g_rng_seeded) {
        g_rng_state = frame_counter ^ 0xACE1;
        g_rng_seeded = 1;
    }
    /* LCG: state = state * 25173 + 13849 (mod 2^16) */
    g_rng_state = (uint16_t)(g_rng_state * 25173u + 13849u);
    /* Return the high byte — higher bits of an LCG are more random. */
    return (uint8_t)(g_rng_state >> 8);
}

/* Return a random value in [0, n). */
static uint8_t rng_below(uint8_t n) {
    /* Rejection-free modulo — fine for small n, slight bias is irrelevant here. */
    return (uint8_t)(rng_next() % n);
}

/* ---- Helpers (LCD must be off) ---- */

static void clear_bg(uint8_t tile) {
    uint8_t row[SCR_W];
    uint8_t i, r;
    for (i = 0; i < SCR_W; i++) row[i] = tile;
    for (r = 0; r < SCR_H; r++) set_tiles_row_raw(0, r, SCR_W, row);
}

static uint8_t font_index_for(char c) {
    if (c == ' ')  return FT_TILE(FT_SPACE);
    if (c >= 'A' && c <= 'Z') return FT_TILE(FT_A + (c - 'A'));
    if (c >= 'a' && c <= 'z') return FT_TILE(FT_A + (c - 'a'));
    if (c >= '0' && c <= '9') return FT_TILE(FT_0 + (c - '0'));
    if (c == ':') return FT_TILE(FT_COLON);
    if (c == '-') return FT_TILE(FT_DASH);
    if (c == '.') return FT_TILE(FT_DOT);
    if (c == '?') return FT_TILE(FT_QMARK);
    if (c == '>') return FT_TILE(FT_ARROW);
    if (c == '/') return FT_TILE(FT_SLASH);
    return FT_TILE(FT_SPACE);
}

static void write_text(uint8_t col, uint8_t row, const char *s) {
    uint8_t buf[20];
    uint8_t i = 0;
    while (s[i] && (col + i) < SCR_W && i < 20) {
        buf[i] = font_index_for(s[i]);
        i++;
    }
    set_tiles_row_raw(col, row, i, buf);
}

static uint8_t poll_edges(void) {
    uint8_t b = read_joypad();
    uint8_t edges = b & ~g_prev_btns;
    g_prev_btns = b;
    return edges;
}

static uint8_t shade_for(uint8_t v) {
    if (v <= 3)  return 0;
    if (v <= 7)  return 1;
    if (v <= 11) return 2;
    return 3;
}

/* ---- TITLE ---- */
static void state_title_enter(void) {
    uint8_t r;
    lcd_off();
    load_tiles(0, TITLE_TILE_COUNT, title_tile_data);
    for (r = 0; r < SCR_H; r++)
        set_tiles_row_raw(0, r, SCR_W, &title_tilemap[r * SCR_W]);
    lcd_on();
}

static void state_title_tick(void) {
    uint8_t e = poll_edges();
    if (e & J_START) g_state = STATE_MENU;
}

/* ---- MENU ---- */
static const char *const MENU_LABELS[] = {
    "MNIST CNN",
    "DRAW AND PREDICT",
    "CREDITS",
};
#define MENU_COUNT 3
static const uint8_t MENU_STATES[MENU_COUNT] = {
    STATE_CNN, STATE_DRAW, STATE_CREDITS,
};

static void render_menu(void) {
    /* LCD must be off. */
    clear_bg(0);
    write_text(5, 2, "MNIST CNN");
    write_text(6, 4, "MAIN MENU");
    uint8_t i;
    for (i = 0; i < MENU_COUNT; i++) {
        uint8_t pointer = (i == g_menu_idx) ? FT_TILE(FT_ARROW) : 0;
        set_tile_raw(2, 7 + i * 2, pointer);
        write_text(4, 7 + i * 2, MENU_LABELS[i]);
    }
    write_text(2, 16, "A:SELECT");
}

static void state_menu_enter(void) {
    lcd_off();
    load_tiles(0, ui_tile_count, ui_tile_data);
    load_tiles(UI_FONT_BASE, FONT_TILE_COUNT, font_tile_data);
    render_menu();
    lcd_on();
}

static void state_menu_tick(void) {
    uint8_t e = poll_edges();
    uint8_t old = g_menu_idx;
    if (e & J_UP) {
        g_menu_idx = (g_menu_idx == 0) ? (MENU_COUNT - 1) : (g_menu_idx - 1);
    } else if (e & J_DOWN) {
        g_menu_idx = (g_menu_idx + 1) % MENU_COUNT;
    } else if (e & J_A) {
        g_state = MENU_STATES[g_menu_idx];
        return;
    } else if (e & J_START) {
        g_state = STATE_TITLE;
        return;
    }
    if (g_menu_idx != old) {
        /* Only the arrows changed — just update the two affected tiles.
         * No need to lcd_off for two single-tile writes. */
        set_tile(2, 7 + old * 2,        0);
        set_tile(2, 7 + g_menu_idx * 2, FT_TILE(FT_ARROW));
    }
}

/* ---- CNN demo ---- */
static uint8_t g_cnn_idx;
static uint8_t g_cnn_recompute;
static uint16_t g_cnn_last_ms;        /* inference time, displayed in bottom-right */

/* Draw a 16x16 stored image directly — one tile per pixel, 16x16 tiles.
 * No upscaling: the image is already 16x16, so this is sharp (each stored
 * pixel maps to exactly one 8x8 screen tile of the matching shade).
 * (col_x, row_y) is the top-left tile of the 16x16 region. */
static void draw_cnn_image16_raw(const uint8_t *img16, uint8_t col_x, uint8_t row_y) {
    /* LCD must be off. */
    uint8_t buf[16];
    uint8_t r, c;
    for (r = 0; r < 16; r++) {
        for (c = 0; c < 16; c++) {
            buf[c] = UI_TILE_PIXEL(shade_for(img16[r * 16 + c]));
        }
        set_tiles_row_raw(col_x, row_y + r, 16, buf);
    }
}

/* Write a uint16 as decimal at (col, row), max 5 digits. Pads with spaces.
 * LCD must be off (uses raw writes). */
static void write_uint16_raw(uint8_t col, uint8_t row, uint16_t v, uint8_t width) {
    /* Build the digits in reverse, then emit padded. */
    uint8_t digits[5];
    uint8_t n = 0;
    if (v == 0) {
        digits[0] = 0;
        n = 1;
    } else {
        while (v > 0 && n < 5) {
            digits[n++] = (uint8_t)(v % 10);
            v /= 10;
        }
    }
    /* Emit width-n spaces, then digits in correct order (high to low). */
    uint8_t buf[5];
    uint8_t i, j = 0;
    if (width > 5) width = 5;
    while (j + n < width) {
        buf[j++] = FT_TILE(FT_SPACE);
    }
    for (i = 0; i < n; i++) {
        buf[j++] = FT_TILE(FT_0 + digits[n - 1 - i]);
    }
    set_tiles_row_raw(col, row, j, buf);
}

static void state_cnn_enter(void) {
    lcd_off();
    load_tiles(0, ui_tile_count, ui_tile_data);
    load_tiles(UI_FONT_BASE, FONT_TILE_COUNT, font_tile_data);
    clear_bg(0);
    write_text(5, 0, "MNIST CNN");
    /* Status bar on row 17 will hold "PRED:N ANS:N    Nms" — written at runtime. */
    lcd_on();
    g_cnn_idx = rng_below(NUM_TEST_IMAGES);
    g_cnn_recompute = 1;
}

static void state_cnn_tick(void) {
    int16_t scores[NUM_CLASSES];
    uint8_t pred;
    if (g_cnn_recompute) {
        /* --- Step 1: show the image FIRST, with PRED:? --- */
        lcd_off();
        draw_cnn_image16_raw(&test_images16[(uint16_t)g_cnn_idx * IMG16_PIXELS], 2, 1);
        /* Status: "PRED:? ANS:N" and clear the timer area */
        uint8_t k;
        for (k = 0; k < 20; k++) set_tile_raw(k, 17, FT_TILE(FT_SPACE));
        write_text(0, 17, "PRED:? ANS:");
        set_tile_raw(11, 17, FT_TILE(FT_0 + test_labels[g_cnn_idx]));
        lcd_on();

        /* Wait a few frames so the image is visibly displayed before
         * we start inference. */
        uint8_t i;
        for (i = 0; i < 8; i++) wait_vblank();

        /* --- Step 2: run inference with timing ---
         * classify16 downsamples the stored 16x16 image to 8x8, then runs
         * the CNN. The downsample cost is tiny vs the conv+FC. */
        timer_start();
        pred = classify16(&test_images16[(uint16_t)g_cnn_idx * IMG16_PIXELS], scores);
        g_cnn_last_ms = timer_stop_ms();

        /* --- Step 3: update display with prediction + time ---
         * Status bar layout on row 17 (cols 0..19):
         *   0:'P' 1:'R' 2:'E' 3:'D' 4:':' 5:N 6:' ' 7:'A' 8:'N' 9:'S' 10:':'
         *   11:N 12:' ' 13:' ' 14:n 15:n 16:n 17:'M' 18:'S' 19:' '
         */
        lcd_off();
        /* Clear cols 12..19 of status bar */
        uint8_t k2;
        for (k2 = 12; k2 < 20; k2++) set_tile_raw(k2, 17, FT_TILE(FT_SPACE));
        /* Show prediction digit */
        set_tile_raw(5, 17, FT_TILE(FT_0 + pred));
        /* Show timing: 3-digit ms + "MS" */
        write_uint16_raw(14, 17, g_cnn_last_ms, 3);
        set_tile_raw(17, 17, FT_TILE(FT_M));
        set_tile_raw(18, 17, FT_TILE(FT_S));
        lcd_on();

        /* --- Step 4: play sound based on correctness --- */
        if (pred == test_labels[g_cnn_idx]) {
            play_success_sound();
        } else {
            play_error_sound();
        }

        g_cnn_recompute = 0;
    }
    uint8_t e = poll_edges();
    if (e & J_A) {
        /* Pick a random image (different from the current one if possible). */
        uint8_t next = rng_below(NUM_TEST_IMAGES);
        if (next == g_cnn_idx) {
            next = (next + 1) % NUM_TEST_IMAGES;
        }
        g_cnn_idx = next;
        g_cnn_recompute = 1;
    } else if (e & J_B) {
        /* B also picks a fresh random image. */
        uint8_t next = rng_below(NUM_TEST_IMAGES);
        if (next == g_cnn_idx) {
            next = (next + 1) % NUM_TEST_IMAGES;
        }
        g_cnn_idx = next;
        g_cnn_recompute = 1;
    } else if (e & J_START) {
        g_state = STATE_MENU;
    }
}

/* ---- DRAW ---- */
static uint8_t g_canvas[64];
static uint8_t g_cursor_col, g_cursor_row;
#define CANVAS_X 6
#define CANVAS_Y 3

static uint8_t canvas_tile_at(uint8_t cc, uint8_t cr) {
    uint8_t v = g_canvas[cr * 8 + cc];
    if (cc == g_cursor_col && cr == g_cursor_row) {
        return (v == 0) ? UI_TILE_CURSOR_EMPTY : UI_TILE_CURSOR_FILLED;
    }
    return UI_TILE_PIXEL(shade_for(v));
}

static void draw_canvas_full_raw(void) {
    /* LCD must be off. */
    uint8_t r, c;
    uint8_t buf[8];
    for (r = 0; r < 8; r++) {
        for (c = 0; c < 8; c++) buf[c] = canvas_tile_at(c, r);
        set_tiles_row_raw(CANVAS_X, CANVAS_Y + r, 8, buf);
    }
}

static void state_draw_enter(void) {
    uint8_t i;
    for (i = 0; i < 64; i++) g_canvas[i] = 0;
    g_cursor_col = 4;
    g_cursor_row = 4;

    lcd_off();
    load_tiles(0, ui_tile_count, ui_tile_data);
    load_tiles(UI_FONT_BASE, FONT_TILE_COUNT, font_tile_data);
    clear_bg(0);
    write_text(5, 0, "DRAW MODE");
    write_text(2, 13, "PRED:");
    write_text(0, 15, "A DRAW  B ERASE");
    write_text(0, 16, "SELECT PREDICT");
    write_text(0, 17, "START MENU");
    draw_canvas_full_raw();
    lcd_on();
}

static void state_draw_tick(void) {
    uint8_t e = poll_edges();
    uint8_t old_col = g_cursor_col;
    uint8_t old_row = g_cursor_row;
    uint8_t moved = 0;

    if (e & J_UP) {
        if (g_cursor_row > 0) { g_cursor_row--; moved = 1; }
    } else if (e & J_DOWN) {
        if (g_cursor_row < 7) { g_cursor_row++; moved = 1; }
    } else if (e & J_LEFT) {
        if (g_cursor_col > 0) { g_cursor_col--; moved = 1; }
    } else if (e & J_RIGHT) {
        if (g_cursor_col < 7) { g_cursor_col++; moved = 1; }
    } else if (e & J_A) {
        g_canvas[g_cursor_row * 8 + g_cursor_col] = 16;
        set_tile(CANVAS_X + g_cursor_col, CANVAS_Y + g_cursor_row,
                 canvas_tile_at(g_cursor_col, g_cursor_row));
    } else if (e & J_B) {
        g_canvas[g_cursor_row * 8 + g_cursor_col] = 0;
        set_tile(CANVAS_X + g_cursor_col, CANVAS_Y + g_cursor_row,
                 canvas_tile_at(g_cursor_col, g_cursor_row));
    } else if (e & J_SELECT) {
        int16_t scores[NUM_CLASSES];
        uint8_t pred;
        uint16_t ms;
        set_tile(8, 13, font_index_for('?'));
        timer_start();
        pred = classify(g_canvas, scores);
        ms = timer_stop_ms();
        set_tile(8, 13, FT_TILE(FT_0 + pred));
        /* Show timing in bottom-right of the same row as PRED */
        lcd_off();
        /* Clear cols 11..17 of row 13 first */
        uint8_t k;
        for (k = 11; k < 18; k++) set_tile_raw(k, 13, FT_TILE(FT_SPACE));
        write_uint16_raw(13, 13, ms, 3);
        set_tile_raw(16, 13, FT_TILE(FT_M));
        set_tile_raw(17, 13, FT_TILE(FT_S));
        lcd_on();
        /* In draw mode we don't know the true label, so play success sound
         * for any prediction (the user is the judge). */
        play_success_sound();
    } else if (e & J_START) {
        g_state = STATE_MENU;
        return;
    }

    if (moved) {
        /* Redraw two cells — old (lose cursor) and new (gain cursor). */
        set_tile(CANVAS_X + old_col,       CANVAS_Y + old_row,       canvas_tile_at(old_col, old_row));
        set_tile(CANVAS_X + g_cursor_col,  CANVAS_Y + g_cursor_row,  canvas_tile_at(g_cursor_col, g_cursor_row));
    }
}

/* ---- CREDITS ---- */
static const char *const CREDITS_TEXTS[4][3] = {
    {"MAIN",              "DEVELOPER",       "CLAUDE"},
    {"ORIGINAL IDEA BY",  "LUCAS MOHIMONT",  "LILIAN HOLLARD"},
    {"JURGEN SCHMIDHUBER","PROBABLY DID IT", "FIRST"},
    {"SPECIAL THANKS TO", "MODERN VINTAGE",  "GAMER"},
};
static uint8_t g_credit_idx;

static void render_credits_raw(void) {
    /* LCD must be off. */
    clear_bg(0);
    write_text(6, 1, "CREDITS");
    uint8_t buf[3];
    buf[0] = FT_TILE(FT_1 + g_credit_idx);
    buf[1] = FT_TILE(FT_SLASH);
    buf[2] = FT_TILE(FT_4);
    set_tiles_row_raw(16, 1, 3, buf);

    write_text(2, 6,  CREDITS_TEXTS[g_credit_idx][0]);
    write_text(2, 8,  CREDITS_TEXTS[g_credit_idx][1]);
    write_text(2, 10, CREDITS_TEXTS[g_credit_idx][2]);

    write_text(0, 16, "A NEXT  B PREV");
    write_text(0, 17, "START MENU");
}

static void state_credits_enter(void) {
    lcd_off();
    load_tiles(0, ui_tile_count, ui_tile_data);
    load_tiles(UI_FONT_BASE, FONT_TILE_COUNT, font_tile_data);
    g_credit_idx = 0;
    render_credits_raw();
    lcd_on();
}

static void state_credits_tick(void) {
    uint8_t e = poll_edges();
    uint8_t changed = 0;
    if (e & J_A) {
        g_credit_idx = (g_credit_idx + 1) % 4;
        changed = 1;
    } else if (e & J_B) {
        g_credit_idx = (g_credit_idx == 0) ? 3 : (g_credit_idx - 1);
        changed = 1;
    } else if (e & J_START) {
        g_state = STATE_MENU;
        return;
    }
    if (changed) {
        lcd_off();
        render_credits_raw();
        lcd_on();
    }
}

/* ---- Main ---- */
void main(void) {
    g_state = STATE_TITLE;
    g_prev_btns = 0;
    g_menu_idx = 0;
    uint8_t entered_state = 0xFF;

    /* Initialize APU once at startup. */
    sound_init();

    while (1) {
        if (g_state != entered_state) {
            entered_state = g_state;
            switch (g_state) {
                case STATE_TITLE:   state_title_enter();   break;
                case STATE_MENU:    state_menu_enter();    break;
                case STATE_CNN:     state_cnn_enter();     break;
                case STATE_DRAW:    state_draw_enter();    break;
                case STATE_CREDITS: state_credits_enter(); break;
            }
            g_prev_btns = read_joypad();
        }
        switch (g_state) {
            case STATE_TITLE:   state_title_tick();   break;
            case STATE_MENU:    state_menu_tick();    break;
            case STATE_CNN:     state_cnn_tick();     break;
            case STATE_DRAW:    state_draw_tick();    break;
            case STATE_CREDITS: state_credits_tick(); break;
        }
        wait_vblank();
    }
}
