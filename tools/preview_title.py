"""Render title_tiles.c back to an image to verify quality."""
from PIL import Image
import re
import numpy as np

# Parse title_tiles.c
with open('/home/claude/gb_mnist_v4/title_tiles.c') as f:
    src = f.read()

# Extract tile data
m = re.search(r'title_tile_data\[\d+\]\s*=\s*\{([^}]+)\}', src, re.DOTALL)
tile_bytes = [int(x.strip(), 16) for x in m.group(1).strip().rstrip(',').split(',') if x.strip()]
print(f"Tile data bytes: {len(tile_bytes)}, tile count: {len(tile_bytes)//16}")

m = re.search(r'title_tilemap\[\d+\]\s*=\s*\{([^}]+)\}', src, re.DOTALL)
tilemap = [int(x.strip(), 16) for x in m.group(1).strip().rstrip(',').split(',') if x.strip()]
print(f"Tilemap entries: {len(tilemap)}")

# Decode tiles back to color indices
def decode_tile(tb):
    """16 bytes -> 8x8 color indices."""
    out = np.zeros((8, 8), dtype=np.uint8)
    for r in range(8):
        low = tb[2*r]
        hi  = tb[2*r + 1]
        for c in range(8):
            bit_low = (low >> (7 - c)) & 1
            bit_hi  = (hi  >> (7 - c)) & 1
            out[r, c] = (bit_hi << 1) | bit_low
    return out

# Reconstruct 160x144 image
img = np.zeros((144, 160), dtype=np.uint8)
for tr in range(18):
    for tc in range(20):
        tile_idx = tilemap[tr * 20 + tc]
        tb = tile_bytes[tile_idx*16 : tile_idx*16 + 16]
        tile_pix = decode_tile(tb)
        img[tr*8:(tr+1)*8, tc*8:(tc+1)*8] = tile_pix

# Map color indices to grayscale: 0=white(255), 1=light(170), 2=dark(85), 3=black(0)
palette = np.array([255, 170, 85, 0], dtype=np.uint8)
gray = palette[img]
Image.fromarray(gray).save('/tmp/title_preview.png')
print("Saved preview to /tmp/title_preview.png")
