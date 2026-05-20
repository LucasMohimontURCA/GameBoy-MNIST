# Game Boy MNIST CNN — v6

A handwritten-digit CNN running natively on a Game Boy (DMG, 4.19 MHz,
8 KB WRAM, 32 KB ROM). No GBDK — hand-rolled crt0, custom hardware layer,
hand-tuned sm83 assembly for the FC layer.

## What changed in v6

1. **LeNet menu option removed** — the menu now has 3 entries:
   MNIST CNN, Draw and Predict, Credits.
2. **Digits stored at 16×16, downsampled for inference** — demo images are
   stored at 16×16 and displayed sharply (smooth gradients). At inference
   time the GB downsamples 16×16 → 8×8 by 2×2 averaging. This replaces v5's
   blocky 2× upscale of an 8×8 image; the digits are noticeably more
   readable.
3. **Random image order** — the A/B buttons in CNN mode now pick a random
   image instead of cycling in order. Uses a 16-bit LCG seeded from the
   VBlank frame counter.

## Important: dataset note

The original intent was to store **real MNIST (28×28 NIST handwriting)**
downsampled to 8×8. Real MNIST could not be fetched — this build
environment restricts network access to package hosts (pypi/npm/github),
and MNIST's download mirrors return HTTP 403.

As a result, the 16×16 demo images are sklearn's `digits` (8×8) **upscaled
with bicubic interpolation** — smooth, not blocky. This still delivers the
readability improvement, and the full 16×16 → 8×8 downsample pipeline is in
place. To switch to real MNIST later, only the image source in
`train_v6.py` needs to change (the C code is dataset-agnostic).

Measured: the 16×16-store → 8×8-downsample pipeline scores 95.56% quantized
accuracy — marginally *better* than raw 8×8 (95.28%), because the
upscale+redownsample smooths quantization noise.

## Architecture (unchanged)

```
Stored image : 16×16 uint8 (values 0..16)
  ↓ downsample 2×2 average
CNN input    : 8×8 uint8
Conv2D       : 12 filters, 3×3, stride 2 → 3×3×12
ReLU
Flatten      : 108 features
FC           : 108 → 10  (inner loop in hand-tuned sm83 asm)
Argmax
```

Inference ~520 ms on DMG, shown live on-screen.

## Controls

| State | Controls |
|---|---|
| Title    | START → menu |
| Menu     | ↑↓ navigate · A select · START → title |
| CNN      | A / B random image · START → menu |
| Draw     | ↑↓←→ cursor · A paint · B erase · SELECT predict · START → menu |
| Credits  | A next page · B previous · START → menu |

## Memory

| Region | Used | Total |
|---|---|---|
| ROM    | 19.2 KB | 32 KB |
| ROM free | 12.5 KB | — |

The +5.6 KB vs v5 is the 16×16 image storage (30 × 256 B vs 30 × 64 B).

## Files

```
main.c             State machine, UI, CNN/draw/credits
inference.c        CNN forward pass + downsample_16_to_8 + classify16
fc_asm.s           Hand-tuned FC inner loop (sm83)
crt0.s             ROM header, boot, VBlank ISR + frame counter
gb_hw.{c,h}        Joypad, VRAM, LCD, sound, timer
ui_tiles.{c,h}     Grayscale + cursor tiles
font_tiles.{c,h}   43-glyph 5×7 font
title_tiles.{c,h}  Title screen (from tools/title.png)
weights.{c,h}      Quantized model + 30 demo images (16×16)
train_v6.py        Training script (generates weights.h)
Makefile           `make` → gb_mnist_v6.gb
tools/             PNG→tiles, font generator, title preview
```

## Build

```bash
apt install sdcc
make
```

Reproducible: `make clean && make` yields a byte-identical ROM (MD5 verified).

## Regenerating the model

```bash
pip install torch scikit-learn
python3 train_v6.py    # writes weights.h (model + 30 demo images at 16×16)
# split weights.h into weights.h + weights.c (see project notes)
make
```

## Draw mode

Still an 8×8 canvas (1 tile per pixel) so the cursor stays precise.
Predictions on hand-drawn input are often wrong — the model is trained on
sklearn `digits`, a distribution unlike pixel-art drawings. This is
expected; reliable draw-mode recognition needs training on real MNIST.

## Credits

- Main developer: Claude
- Original idea: Lucas Mohimont & Lilian Hollard
- Prior art: Jürgen Schmidhuber probably did it first
- Special thanks: Modern Vintage Gamer

Nintendo logo bytes in `crt0.s` are required by the boot ROM checksum.
