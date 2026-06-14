#!/usr/bin/env python3
"""
Convert a ChatGPT/AI-generated image into the game's sprite strip format.
Usage: python3 tools/fix_sprite.py <input.png> <output_faction_F_tT.png>

Handles two common cases:
  A) Input is already 512×64 (or close) — just resize/pad to exact size
  B) Input is a square (512×512, 1024×1024) — assumed to be 4×2 grid of
     8 frames; crops each cell and reassembles as a horizontal 512×64 strip
"""

import sys
from pathlib import Path
from PIL import Image

FRAME_W   = 64
FRAME_H   = 64
N_FRAMES  = 8
STRIP_W   = FRAME_W * N_FRAMES  # 512
STRIP_H   = FRAME_H             # 64


def remove_background(img: Image.Image, threshold: int = 30) -> Image.Image:
    """Convert near-white or solid-colour background to transparent."""
    img = img.convert("RGBA")
    px = img.load()
    w, h = img.size
    # Sample corner pixels to guess background colour
    corners = [px[0,0], px[w-1,0], px[0,h-1], px[w-1,h-1]]
    bg = max(corners, key=lambda c: c[3])  # most opaque corner
    br, bg_g, bb = bg[0], bg[1], bg[2]
    for y in range(h):
        for x in range(w):
            r,g,b,a = px[x,y]
            if abs(r-br)+abs(g-bg_g)+abs(b-bb) < threshold*3:
                px[x,y] = (r,g,b,0)
    return img


def make_strip_from_square(img: Image.Image) -> Image.Image:
    """Treat square input as 4-col × 2-row grid → horizontal 8-frame strip."""
    w, h = img.size
    cell_w = w // 4
    cell_h = h // 2
    strip = Image.new("RGBA", (STRIP_W, STRIP_H), (0,0,0,0))
    idx = 0
    for row in range(2):
        for col in range(4):
            cell = img.crop((col*cell_w, row*cell_h, (col+1)*cell_w, (row+1)*cell_h))
            cell = cell.resize((FRAME_W, FRAME_H), Image.LANCZOS).convert("RGBA")
            strip.paste(cell, (idx * FRAME_W, 0), cell)
            idx += 1
    return strip


def make_strip_from_wide(img: Image.Image) -> Image.Image:
    """Input is already a horizontal strip — just resize to 512×64."""
    return img.resize((STRIP_W, STRIP_H), Image.LANCZOS).convert("RGBA")


def process(input_path: str, output_path: str) -> None:
    img = Image.open(input_path).convert("RGBA")
    w, h = img.size
    print(f"Input: {w}×{h}")

    # Try to remove solid background if present
    img = remove_background(img)

    ratio = w / h
    if ratio > 3:
        # Wide strip — already in strip format
        result = make_strip_from_wide(img)
        print("Mode: wide strip → resize to 512×64")
    elif 0.8 < ratio < 1.25:
        # Square — interpret as 4×2 grid
        result = make_strip_from_square(img)
        print("Mode: square → 4×2 grid → 512×64 strip")
    else:
        # Ambiguous — just resize
        result = img.resize((STRIP_W, STRIP_H), Image.LANCZOS).convert("RGBA")
        print(f"Mode: resize {w}×{h} → 512×64")

    result.save(output_path)
    print(f"Saved: {output_path}")


if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: python3 fix_sprite.py <input.png> <faction_F_tT.png>")
        sys.exit(1)
    process(sys.argv[1], sys.argv[2])
