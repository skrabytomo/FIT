#!/usr/bin/env python3
"""
Split a sprite sheet into individual frames by detecting non-transparent column groups.
Saves each frame as a trimmed PNG with alpha.

Usage:
    python3 split_sprites.py <input.png> [output_prefix]

Output:
    output_prefix_00.png, output_prefix_01.png, ...
"""

import sys
import os
from PIL import Image

def find_column_groups(img, min_gap=4, alpha_threshold=10):
    """
    Scan each column; mark it occupied if any pixel has alpha > threshold.
    Group consecutive occupied columns into frames, separated by >= min_gap empty columns.
    Returns list of (col_start, col_end) tuples (inclusive).
    """
    width, height = img.size
    data = img.load()

    occupied = []
    for x in range(width):
        has_content = any(data[x, y][3] > alpha_threshold for y in range(height))
        occupied.append(has_content)

    groups = []
    in_group = False
    start = 0
    empty_run = 0

    for x in range(width):
        if occupied[x]:
            if not in_group:
                in_group = True
                start = x
            empty_run = 0
        else:
            if in_group:
                empty_run += 1
                if empty_run >= min_gap:
                    groups.append((start, x - empty_run))
                    in_group = False
                    empty_run = 0

    if in_group:
        groups.append((start, width - 1 - empty_run))

    return groups


def split_sprite_sheet(input_path, output_prefix=None, min_gap=4):
    img = Image.open(input_path).convert("RGBA")
    width, height = img.size

    if img.mode != "RGBA":
        print(f"Warning: image mode is {img.mode}, converting to RGBA")
        img = img.convert("RGBA")

    # Check if background is baked checkerboard (no real alpha)
    alpha_values = [img.getpixel((x, y))[3] for x in range(0, width, 10)
                    for y in range(0, height, 10)]
    max_alpha = max(alpha_values) if alpha_values else 0
    min_alpha = min(alpha_values) if alpha_values else 0

    if max_alpha == 255 and min_alpha == 255:
        print("WARNING: Image appears to have no transparency (all alpha=255).")
        print("The background may be baked in as checkerboard pixels.")
        print("Switching to checkerboard-detection mode...")
        remove_checkerboard(img)

    groups = find_column_groups(img, min_gap=min_gap)
    print(f"Found {len(groups)} frames")

    if output_prefix is None:
        base = os.path.splitext(os.path.basename(input_path))[0]
        out_dir = os.path.dirname(input_path) or "."
        output_prefix = os.path.join(out_dir, base)

    saved = []
    for i, (x0, x1) in enumerate(groups):
        # Crop the frame column range
        frame = img.crop((x0, 0, x1 + 1, height))
        # Trim transparent rows (top/bottom)
        bbox = frame.getbbox()
        if bbox:
            frame = frame.crop(bbox)
        out_path = f"{output_prefix}_{i:02d}.png"
        frame.save(out_path)
        saved.append(out_path)
        print(f"  Frame {i:02d}: cols {x0}-{x1} → {out_path} ({frame.size[0]}×{frame.size[1]})")

    return saved


def remove_checkerboard(img):
    """
    Remove standard checkerboard transparency pattern (alternating ~128/77 gray squares, 8px or 16px).
    Sets matching pixels to fully transparent in-place.
    """
    data = img.load()
    w, h = img.size
    # Common checkerboard colors: (153,153,153) and (102,102,102), or (127,127,127)/(76,76,76)
    checker_colors = {
        (153, 153, 153), (102, 102, 102),  # Photoshop default
        (128, 128, 128), (76, 76, 76),     # alt variant
        (200, 200, 200), (150, 150, 150),  # lighter variant
    }
    tolerance = 18
    for y in range(h):
        for x in range(w):
            r, g, b, a = data[x, y]
            # Check if this pixel looks like a checkerboard square
            for cr, cg, cb in checker_colors:
                if (abs(r - cr) <= tolerance and abs(g - cg) <= tolerance
                        and abs(b - cb) <= tolerance):
                    data[x, y] = (r, g, b, 0)
                    break


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(1)
    input_path = sys.argv[1]
    output_prefix = sys.argv[2] if len(sys.argv) > 2 else None
    split_sprite_sheet(input_path, output_prefix)
