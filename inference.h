#ifndef INFERENCE_H
#define INFERENCE_H
#include <stdint.h>
#include "weights.h"

/* Classify an 8x8 image (values 0..16). Returns predicted digit 0..9.
 * If scores != 0, fills it with NUM_CLASSES logit values (>>8 scaled). */
uint8_t classify(const uint8_t *img, int16_t *scores);

/* Downsample a 16x16 image to 8x8 by 2x2 block averaging. */
void downsample_16_to_8(const uint8_t *img16, uint8_t *img8);

/* Classify a 16x16 image: downsamples to 8x8, then runs the CNN. */
uint8_t classify16(const uint8_t *img16, int16_t *scores);

#endif
