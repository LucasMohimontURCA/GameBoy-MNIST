"""Generate font tile data for a 5x7 readable font on 8x8 tiles.
Outputs a C array of tile data for ASCII chars we need.

The set of characters used in the UI:
  A-Z (upper), digits 0-9, space, ':', '-', '.', '?', arrow tiles
"""
import numpy as np

# 5x7 pixel font, hand-drawn glyphs as bitmap strings.
# Each glyph is 7 rows of 5 chars: '.'=background(0), '#'=foreground(3).
FONT = {
    'A': [
        ".###.",
        "#...#",
        "#...#",
        "#####",
        "#...#",
        "#...#",
        "#...#",
    ],
    'B': [
        "####.",
        "#...#",
        "#...#",
        "####.",
        "#...#",
        "#...#",
        "####.",
    ],
    'C': [
        ".###.",
        "#...#",
        "#....",
        "#....",
        "#....",
        "#...#",
        ".###.",
    ],
    'D': [
        "####.",
        "#...#",
        "#...#",
        "#...#",
        "#...#",
        "#...#",
        "####.",
    ],
    'E': [
        "#####",
        "#....",
        "#....",
        "###..",
        "#....",
        "#....",
        "#####",
    ],
    'F': [
        "#####",
        "#....",
        "#....",
        "###..",
        "#....",
        "#....",
        "#....",
    ],
    'G': [
        ".###.",
        "#...#",
        "#....",
        "#.###",
        "#...#",
        "#...#",
        ".###.",
    ],
    'H': [
        "#...#",
        "#...#",
        "#...#",
        "#####",
        "#...#",
        "#...#",
        "#...#",
    ],
    'I': [
        "#####",
        "..#..",
        "..#..",
        "..#..",
        "..#..",
        "..#..",
        "#####",
    ],
    'J': [
        "..###",
        "...#.",
        "...#.",
        "...#.",
        "...#.",
        "#..#.",
        ".##..",
    ],
    'K': [
        "#...#",
        "#..#.",
        "#.#..",
        "##...",
        "#.#..",
        "#..#.",
        "#...#",
    ],
    'L': [
        "#....",
        "#....",
        "#....",
        "#....",
        "#....",
        "#....",
        "#####",
    ],
    'M': [
        "#...#",
        "##.##",
        "#.#.#",
        "#.#.#",
        "#...#",
        "#...#",
        "#...#",
    ],
    'N': [
        "#...#",
        "##..#",
        "##..#",
        "#.#.#",
        "#..##",
        "#..##",
        "#...#",
    ],
    'O': [
        ".###.",
        "#...#",
        "#...#",
        "#...#",
        "#...#",
        "#...#",
        ".###.",
    ],
    'P': [
        "####.",
        "#...#",
        "#...#",
        "####.",
        "#....",
        "#....",
        "#....",
    ],
    'Q': [
        ".###.",
        "#...#",
        "#...#",
        "#...#",
        "#.#.#",
        "#..#.",
        ".##.#",
    ],
    'R': [
        "####.",
        "#...#",
        "#...#",
        "####.",
        "#.#..",
        "#..#.",
        "#...#",
    ],
    'S': [
        ".####",
        "#....",
        "#....",
        ".###.",
        "....#",
        "....#",
        "####.",
    ],
    'T': [
        "#####",
        "..#..",
        "..#..",
        "..#..",
        "..#..",
        "..#..",
        "..#..",
    ],
    'U': [
        "#...#",
        "#...#",
        "#...#",
        "#...#",
        "#...#",
        "#...#",
        ".###.",
    ],
    'V': [
        "#...#",
        "#...#",
        "#...#",
        "#...#",
        "#...#",
        ".#.#.",
        "..#..",
    ],
    'W': [
        "#...#",
        "#...#",
        "#...#",
        "#.#.#",
        "#.#.#",
        "##.##",
        "#...#",
    ],
    'X': [
        "#...#",
        "#...#",
        ".#.#.",
        "..#..",
        ".#.#.",
        "#...#",
        "#...#",
    ],
    'Y': [
        "#...#",
        "#...#",
        ".#.#.",
        "..#..",
        "..#..",
        "..#..",
        "..#..",
    ],
    'Z': [
        "#####",
        "....#",
        "...#.",
        "..#..",
        ".#...",
        "#....",
        "#####",
    ],
    '0': [
        ".###.",
        "#...#",
        "#..##",
        "#.#.#",
        "##..#",
        "#...#",
        ".###.",
    ],
    '1': [
        "..#..",
        ".##..",
        "..#..",
        "..#..",
        "..#..",
        "..#..",
        ".###.",
    ],
    '2': [
        ".###.",
        "#...#",
        "....#",
        "...#.",
        "..#..",
        ".#...",
        "#####",
    ],
    '3': [
        "####.",
        "....#",
        "....#",
        ".###.",
        "....#",
        "....#",
        "####.",
    ],
    '4': [
        "#...#",
        "#...#",
        "#...#",
        "#####",
        "....#",
        "....#",
        "....#",
    ],
    '5': [
        "#####",
        "#....",
        "#....",
        "####.",
        "....#",
        "....#",
        "####.",
    ],
    '6': [
        ".###.",
        "#....",
        "#....",
        "####.",
        "#...#",
        "#...#",
        ".###.",
    ],
    '7': [
        "#####",
        "....#",
        "...#.",
        "..#..",
        ".#...",
        ".#...",
        ".#...",
    ],
    '8': [
        ".###.",
        "#...#",
        "#...#",
        ".###.",
        "#...#",
        "#...#",
        ".###.",
    ],
    '9': [
        ".###.",
        "#...#",
        "#...#",
        ".####",
        "....#",
        "....#",
        ".###.",
    ],
    ' ': [
        ".....",
        ".....",
        ".....",
        ".....",
        ".....",
        ".....",
        ".....",
    ],
    ':': [
        ".....",
        "..#..",
        "..#..",
        ".....",
        "..#..",
        "..#..",
        ".....",
    ],
    '-': [
        ".....",
        ".....",
        ".....",
        ".###.",
        ".....",
        ".....",
        ".....",
    ],
    '.': [
        ".....",
        ".....",
        ".....",
        ".....",
        ".....",
        "..#..",
        "..#..",
    ],
    '?': [
        ".###.",
        "#...#",
        "....#",
        "...#.",
        "..#..",
        ".....",
        "..#..",
    ],
    '>': [   # right arrow / pointer
        "#....",
        "##...",
        "###..",
        "####.",
        "###..",
        "##...",
        "#....",
    ],
    '/': [
        "....#",
        "...#.",
        "...#.",
        "..#..",
        "..#..",
        ".#...",
        ".#...",
    ],
}

def char_to_tile_bytes(rows):
    """Convert 7-row 5-col glyph to 16-byte GB tile.
    Glyph is centered horizontally (1 pad each side -> 7px wide effectively).
    Vertically the 7 rows go into rows 0-6, row 7 is blank.
    """
    out = bytearray(16)
    for r in range(7):
        low = 0
        hi = 0
        # pad: 1 col left blank, glyph in cols 1-5, 2 cols right blank
        for c in range(5):
            v = 3 if rows[r][c] == '#' else 0
            col_in_tile = c + 1
            low |= ((v & 1) << (7 - col_in_tile))
            hi  |= (((v >> 1) & 1) << (7 - col_in_tile))
        out[2*r]     = low
        out[2*r + 1] = hi
    # row 7 stays 0 (blank)
    return bytes(out)

# Ordered char list for tile assignment
# We want predictable indices. We'll assign:
#   FONT_BASE_TILE = some number (e.g., 28). Then chars in order:
chars_ordered = [
    ' ', '0','1','2','3','4','5','6','7','8','9',
    'A','B','C','D','E','F','G','H','I','J','K','L','M',
    'N','O','P','Q','R','S','T','U','V','W','X','Y','Z',
    ':', '-', '.', '?', '>', '/',
]

# Build tile_data
out_bytes = bytearray()
for c in chars_ordered:
    out_bytes.extend(char_to_tile_bytes(FONT[c]))

# Output as C
lines = []
lines.append("/* Auto-generated font tile data (5x7 glyphs in 8x8 tiles) */")
lines.append("#include <stdint.h>")
lines.append("")
lines.append(f"const uint8_t font_tile_data[{len(out_bytes)}] = {{")
for i in range(0, len(out_bytes), 16):
    chunk = ", ".join(f"0x{b:02X}" for b in out_bytes[i:i+16])
    lines.append("    " + chunk + ",")
lines.append("};")
lines.append("")
lines.append(f"const uint8_t font_tile_count = {len(chars_ordered)};")
lines.append("")
lines.append(f"/* Char -> tile index mapping. Total chars: {len(chars_ordered)} */")

with open('/home/claude/gb_mnist_v4/font_tiles.c', 'w') as f:
    f.write("\n".join(lines))

# Header with char->index lookups
hdr = ["#ifndef FONT_TILES_H", "#define FONT_TILES_H", "#include <stdint.h>", ""]
hdr.append(f"#define FONT_TILE_COUNT {len(chars_ordered)}")
hdr.append("extern const uint8_t font_tile_data[];")
hdr.append("")
hdr.append("/* If you load font tiles starting at VRAM index FONT_BASE, then")
hdr.append("   tile index for character c = FONT_BASE + FONT_OFFSET[c]. */")
hdr.append("")
# Macros for each char
for i, c in enumerate(chars_ordered):
    if c.isalnum():
        hdr.append(f"#define FT_{c} {i}")
    elif c == ' ':
        hdr.append(f"#define FT_SPACE {i}")
    elif c == ':':
        hdr.append(f"#define FT_COLON {i}")
    elif c == '-':
        hdr.append(f"#define FT_DASH {i}")
    elif c == '.':
        hdr.append(f"#define FT_DOT {i}")
    elif c == '?':
        hdr.append(f"#define FT_QMARK {i}")
    elif c == '>':
        hdr.append(f"#define FT_ARROW {i}")
    elif c == '/':
        hdr.append(f"#define FT_SLASH {i}")
hdr.append("")
hdr.append("#endif")
with open('/home/claude/gb_mnist_v4/font_tiles.h', 'w') as f:
    f.write("\n".join(hdr))

print(f"Font: {len(chars_ordered)} chars, {len(out_bytes)} bytes of tile data")
