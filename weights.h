// Auto-generated: CNN weights for Game Boy digit demo (v6)
// Conv(12 @ 3x3, stride 2) -> ReLU -> 108 -> FC -> 10
// Demo images STORED at 16x16, downsampled to 8x8 for inference.
#ifndef WEIGHTS_H
#define WEIGHTS_H
#include <stdint.h>

#define IMG_SIZE 8
#define IMG_PIXELS 64
#define IMG16_SIZE 16
#define IMG16_PIXELS 256
#define CONV_OUT_SIZE 3
#define CONV_STRIDE 2
#define CONV_FILTERS 12
#define CONV_KERNEL 3
#define FEAT_LEN 108
#define NUM_CLASSES 10
#define NUM_TEST_IMAGES 30

extern const int8_t conv_w[108];

extern const int16_t conv_b[12];

extern const int8_t fc_w[1080];

extern const int32_t fc_b[10];

// test_images: 30 images, each 16x16 = 256 bytes, values 0..16
extern const uint8_t test_images16[7680];

extern const uint8_t test_labels[30];

#endif