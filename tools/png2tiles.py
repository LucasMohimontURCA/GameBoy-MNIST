"""Convert title.png (160x144) to GB tile data + tilemap.

Output:
  - title_tiles.c containing:
      title_tile_data: deduplicated 8x8 tiles (each = 16 bytes)
      title_tilemap:   20x18 tile indices

GB tile format: 16 bytes per tile. Each row of 8 pixels = 2 bytes (low bit plane,
then high bit plane). Color index = (high<<1)|low for each pixel.
Default palette BGP=0xE4 maps color indices: 0=white, 1=light, 2=dark, 3=black.

We map the RGB image to 4 shades by simple quantization on the luminance.
"""
from PIL import Image
import numpy as np

img = Image.open('/mnt/user-data/uploads/image.png').convert('RGB')
arr = np.array(img)
print(f"Loaded: {arr.shape}")  # (144, 160, 3)

# Quantize to 4 grayscale levels
# Use luminance, then bucket into 4 levels.
lum = 0.299 * arr[:,:,0] + 0.587 * arr[:,:,1] + 0.114 * arr[:,:,2]

# Pick thresholds — image is mostly dark green on black, with bright text.
# Map: very dark -> color 3 (black on GB)... wait, on DMG with BGP=0xE4,
# color index 0 = lightest, 3 = darkest. We want bright text to read as
# "light" on GB.
# Convention: index 0 = white (lightest), 3 = black (darkest).
# So bright pixels in the source -> low index, dark pixels -> high index.
# Actually, the original is dark-bg/light-text. We want dark-bg on GB too,
# so:  dark source -> index 3 (black on screen), bright source -> index 0.

print(f"Luminance range: {lum.min():.0f} .. {lum.max():.0f}")
# The image is very dark (median ~7) with bright text spikes. Use logarithmic-ish
# bucketing: separate true black, near-black, dim, bright.
idx = np.zeros(lum.shape, dtype=np.uint8)
idx[lum < 10]  = 3    # nearly black -> color 3
idx[(lum >= 10) & (lum < 50)] = 2
idx[(lum >= 50) & (lum < 130)] = 1
idx[lum >= 130] = 0   # bright text -> color 0 (lightest)

# Apply Floyd-Steinberg dithering would be too noisy and explode tile count.
# Instead, quantize stronger — collapse near-black noise (dots/grain) into a
# single solid black. This is the main cause of tile explosion.
# We'll force any tile with <= 3 distinct non-background pixels to be solid.

print(f"Color distribution: 0={np.sum(idx==0)}, 1={np.sum(idx==1)}, 2={np.sum(idx==2)}, 3={np.sum(idx==3)}")

# Now build tiles: image is 18 rows x 20 cols of 8x8 tiles
def encode_tile(pixels_8x8):
    """Convert 8x8 numpy array of color indices (0..3) into 16 bytes GB tile."""
    out = bytearray(16)
    for r in range(8):
        low = 0
        hi = 0
        for c in range(8):
            v = pixels_8x8[r, c] & 3
            low |= ((v & 1) << (7 - c))
            hi  |= (((v >> 1) & 1) << (7 - c))
        out[2*r]     = low
        out[2*r + 1] = hi
    return bytes(out)

# Build tile dictionary (dedup)
# To reduce tile count drastically, we'll "simplify" tiles that are nearly-uniform:
# if a tile has very few non-background pixels, snap it to the background color.
def simplify_tile(tile):
    """If a tile is nearly uniform, snap to that uniform color.
    Reduces noise from grainy texture."""
    out = tile.copy()
    vals, counts = np.unique(out, return_counts=True)
    # Sort by count desc
    order = np.argsort(-counts)
    if counts[order[0]] >= 60:  # 60/64 = 94% one color → snap to that color
        return np.full_like(out, vals[order[0]])
    return out

tile_data = []
tile_map_2d = np.zeros((18, 20), dtype=np.uint8)
dedup = {}
for tr in range(18):
    for tc in range(20):
        tile = idx[tr*8:(tr+1)*8, tc*8:(tc+1)*8]
        tile = simplify_tile(tile)
        enc = encode_tile(tile)
        if enc not in dedup:
            if len(tile_data) >= 220:
                # VRAM full: reuse the most similar existing tile (just use solid black)
                # Find existing nearly-solid-3 tile
                solid_3 = bytes(16 * [0xFF])  # all bits set -> color 3
                if solid_3 in dedup:
                    enc = solid_3
                else:
                    # fallback: tile 0
                    enc = list(dedup.keys())[0]
            else:
                dedup[enc] = len(tile_data)
                tile_data.append(enc)
        tile_map_2d[tr, tc] = dedup[enc]

print(f"Unique tiles: {len(tile_data)}")
# GB tile RAM holds 256 tiles in mode 8000 (BG uses 0x80-0xFF for "signed" mode,
# but with BG_DATA_8000 selected the full 0-255 range is usable for BG).
# Our existing tiles (digits, font, etc) use 0..27 — so title tiles start at 28.
# 256 - 28 = 228 tiles available. We have ~140 unique, should fit.

if len(tile_data) > 220:
    print(f"WARNING: too many tiles ({len(tile_data)}), may overflow VRAM!")

# Emit C source
def fmt_bytes_per_line(bytes_data, indent="    ", per_line=16):
    lines = []
    for i in range(0, len(bytes_data), per_line):
        chunk = ", ".join(f"0x{b:02X}" for b in bytes_data[i:i+per_line])
        lines.append(indent + chunk + ",")
    return "\n".join(lines)

out = []
out.append("/* Auto-generated from title.png by tools/png2tiles.py */")
out.append("#include <stdint.h>")
out.append("")
out.append(f"#define TITLE_TILE_COUNT {len(tile_data)}")
out.append(f"#define TITLE_MAP_W 20")
out.append(f"#define TITLE_MAP_H 18")
out.append("")
out.append(f"const uint8_t title_tile_data[{len(tile_data) * 16}] = {{")
flat = b''.join(tile_data)
out.append(fmt_bytes_per_line(flat))
out.append("};")
out.append("")
out.append(f"const uint8_t title_tilemap[{18 * 20}] = {{")
flat_map = tile_map_2d.flatten()
out.append(fmt_bytes_per_line(flat_map))
out.append("};")
out.append("")

with open('/home/claude/gb_mnist_v4/title_tiles.c', 'w') as f:
    f.write("\n".join(out))

# Header
hdr = """#ifndef TITLE_TILES_H
#define TITLE_TILES_H
#include <stdint.h>

#define TITLE_TILE_COUNT """ + str(len(tile_data)) + """
#define TITLE_MAP_W 20
#define TITLE_MAP_H 18

extern const uint8_t title_tile_data[];
extern const uint8_t title_tilemap[];

#endif
"""
with open('/home/claude/gb_mnist_v4/title_tiles.h', 'w') as f:
    f.write(hdr)

print(f"Wrote title_tiles.c ({len(tile_data)} tiles, {len(tile_data)*16} bytes tile data + 360 bytes tilemap)")
