/* CNN inference v2 — conv 3x3 stride 2, 12 filters, FC asm-optimized.
 *
 * Architecture (from weights.h):
 *   Input  : 8x8 uint8
 *   Conv   : 12 filters, 3x3, STRIDE 2 -> 3x3x12 = 108 features
 *   ReLU
 *   FC     : 108 -> 10
 *
 * Compared to v1 (4 filters, stride 1):
 *   - 12*9*3*3 = 972 conv MACs (vs 1296 in v1) — same magnitude
 *   - 108*10 = 1080 FC MACs (vs 1440)         — 25% fewer FC ops
 *   - With asm FC, this gives ~16% wall-clock speedup
 */
#include "inference.h"

static int16_t feat[FEAT_LEN];   /* 108 int16 = 216 B */

/* From fc_asm.s */
extern uint8_t *fc_w_ptr;
extern int16_t *fc_f_ptr;
extern int32_t fc_acc_val;
extern uint8_t fc_count_val;
extern void fc_run(void);

uint8_t classify(const uint8_t *img, int16_t *scores) {
    uint8_t f, i, j, c;
    int16_t conv_acc;
    int16_t feat_idx = 0;

    /* ---- Conv layer: 12 filters, 3x3, stride 2 ---- */
    for (f = 0; f < CONV_FILTERS; f++) {
        const int8_t *kf = &conv_w[f * 9];
        int16_t bias = conv_b[f];
        for (i = 0; i < CONV_OUT_SIZE; i++) {
            for (j = 0; j < CONV_OUT_SIZE; j++) {
                conv_acc = bias;
                /* Stride 2: top-left pixel is at (i*2, j*2). */
                const uint8_t *p = &img[(i * 2) * IMG_SIZE + (j * 2)];
                conv_acc += (int16_t)kf[0] * (int16_t)p[0];
                conv_acc += (int16_t)kf[1] * (int16_t)p[1];
                conv_acc += (int16_t)kf[2] * (int16_t)p[2];
                p += IMG_SIZE;
                conv_acc += (int16_t)kf[3] * (int16_t)p[0];
                conv_acc += (int16_t)kf[4] * (int16_t)p[1];
                conv_acc += (int16_t)kf[5] * (int16_t)p[2];
                p += IMG_SIZE;
                conv_acc += (int16_t)kf[6] * (int16_t)p[0];
                conv_acc += (int16_t)kf[7] * (int16_t)p[1];
                conv_acc += (int16_t)kf[8] * (int16_t)p[2];
                if (conv_acc < 0) conv_acc = 0;
                feat[feat_idx++] = conv_acc;
            }
        }
    }

    /* ---- FC layer (asm-optimized) ---- */
    int32_t max_val = -2147483648L;
    int32_t logits[NUM_CLASSES];
    uint8_t best = 0;

    for (c = 0; c < NUM_CLASSES; c++) {
        fc_w_ptr = (uint8_t *)&fc_w[(uint16_t)c * FEAT_LEN];
        fc_f_ptr = feat;
        fc_acc_val = fc_b[c];
        fc_count_val = FEAT_LEN;  /* 108 */
        fc_run();
        logits[c] = fc_acc_val;
        if (fc_acc_val > max_val) {
            max_val = fc_acc_val;
            best = c;
        }
    }

    if (scores) {
        for (c = 0; c < NUM_CLASSES; c++) {
            int32_t v = logits[c] >> 8;
            if (v > 32767L) v = 32767L;
            if (v < -32768L) v = -32768L;
            scores[c] = (int16_t)v;
        }
    }
    return best;
}

/* Downsample a 16x16 image to 8x8 by averaging each 2x2 block.
 * Values stay in the 0..16 range. The +2 rounds to nearest. */
void downsample_16_to_8(const uint8_t *img16, uint8_t *img8) {
    uint8_t r, c;
    for (r = 0; r < 8; r++) {
        const uint8_t *row0 = &img16[(r * 2)     * IMG16_SIZE];
        const uint8_t *row1 = &img16[(r * 2 + 1) * IMG16_SIZE];
        for (c = 0; c < 8; c++) {
            uint16_t sum = (uint16_t)row0[c * 2]   + (uint16_t)row0[c * 2 + 1]
                         + (uint16_t)row1[c * 2]   + (uint16_t)row1[c * 2 + 1];
            img8[r * 8 + c] = (uint8_t)((sum + 2) >> 2);
        }
    }
}

/* Classify a 16x16 image: downsample to 8x8, then run the CNN. */
uint8_t classify16(const uint8_t *img16, int16_t *scores) {
    uint8_t img8[IMG_PIXELS];
    downsample_16_to_8(img16, img8);
    return classify(img8, scores);
}
