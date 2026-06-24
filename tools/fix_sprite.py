#!/usr/bin/env python3
"""
Convert a ChatGPT/AI-generated sprite image into the game's 512×64 strip format.

Usage: python3 tools/fix_sprite.py <input.png> <faction_F_tT.png>

Handles:
  - Transparent padding around content (ChatGPT DALL-E output)
  - White/solid background (removes it)
  - Square 4×2 grid layout (2 rows of 4 frames)
  - Already-correct horizontal strip (resizes to 512×64)
"""

import sys
from pathlib import Path
from collections import deque
from PIL import Image
import numpy as np

FRAME_W  = 64
FRAME_H  = 64
N_FRAMES = 8
STRIP_W  = FRAME_W * N_FRAMES  # 512
STRIP_H  = FRAME_H             # 64


def content_bbox(img: Image.Image):
    """Return (left, top, right, bottom) of non-transparent, non-white pixels."""
    img = img.convert('RGBA')
    px = img.load()
    W, H = img.size
    left, top, right, bottom = W, H, 0, 0

    # First try: find non-transparent pixels
    has_alpha_content = False
    for y in range(H):
        for x in range(W):
            r,g,b,a = px[x,y]
            if a > 10:
                left   = min(left, x)
                top    = min(top, y)
                right  = max(right, x)
                bottom = max(bottom, y)
                has_alpha_content = True

    if has_alpha_content and (right - left) > 10 and (bottom - top) > 10:
        return left, top, right, bottom

    # Fallback: remove near-white background, then find bounds
    bg_r, bg_g, bg_b = px[0,0][:3]
    left, top, right, bottom = W, H, 0, 0
    for y in range(H):
        for x in range(W):
            r,g,b,a = px[x,y]
            if abs(r-bg_r)+abs(g-bg_g)+abs(b-bg_b) > 40:
                left   = min(left, x)
                top    = min(top, y)
                right  = max(right, x)
                bottom = max(bottom, y)

    return left, top, right+1, bottom+1


def flood_remove_bg(img: Image.Image, tol: int = 55) -> Image.Image:
    """Remove background by flood-filling from edges — preserves internal dark areas."""
    arr = np.array(img.convert('RGBA'))
    h, w = arr.shape[:2]
    bg = arr[0, 0, :3].astype(int)

    def is_bg(y, x):
        return int(np.abs(arr[y, x, :3].astype(int) - bg).sum()) < tol

    visited = np.zeros((h, w), dtype=bool)
    mask    = np.zeros((h, w), dtype=bool)
    q = deque()
    for x in range(w):
        for y in (0, h-1):
            if not visited[y,x] and is_bg(y,x):
                q.append((y,x)); visited[y,x] = True; mask[y,x] = True
    for y in range(h):
        for x in (0, w-1):
            if not visited[y,x] and is_bg(y,x):
                q.append((y,x)); visited[y,x] = True; mask[y,x] = True
    while q:
        y, x = q.popleft()
        for dy, dx in ((-1,0),(1,0),(0,-1),(0,1)):
            ny, nx = y+dy, x+dx
            if 0<=ny<h and 0<=nx<w and not visited[ny,nx] and is_bg(ny,nx):
                visited[ny,nx] = True; mask[ny,nx] = True
                q.append((ny,nx))

    arr[mask,  3] = 0
    arr[~mask, 3] = 255
    return Image.fromarray(arr)


def make_strip(img: Image.Image) -> Image.Image:
    W, H = img.size
    l, t, r, b = content_bbox(img)
    cW, cH = r - l, b - t
    ratio = cW / max(cH, 1)

    strip = Image.new('RGBA', (STRIP_W, STRIP_H), (0,0,0,0))

    if ratio > 3.5:
        # Wide horizontal strip — N frames side by side
        content = img.crop((l, t, r, b))
        cell_w = cW // N_FRAMES
        for i in range(N_FRAMES):
            cell = content.crop((i*cell_w, 0, (i+1)*cell_w, cH))
            scale = min(FRAME_W/cell_w, FRAME_H/cH)
            nw, nh = int(cell_w*scale), int(cH*scale)
            cell = cell.resize((nw, nh), Image.LANCZOS)
            frame = Image.new('RGBA', (FRAME_W, FRAME_H), (0,0,0,0))
            frame.paste(cell, ((FRAME_W-nw)//2, FRAME_H-nh), cell)
            strip.paste(frame, (i*FRAME_W, 0), frame)
        print(f"Mode: wide strip ({cW}×{cH}) → 512×64")

    elif 0.7 < ratio < 1.4:
        # Square 4×2 grid
        content = img.crop((l, t, r, b))
        cell_w, cell_h = cW//4, cH//2
        idx = 0
        for row in range(2):
            for col in range(4):
                cell = content.crop((col*cell_w, row*cell_h,
                                     (col+1)*cell_w, (row+1)*cell_h))
                scale = min(FRAME_W/cell_w, FRAME_H/cell_h)
                nw, nh = int(cell_w*scale), int(cell_h*scale)
                cell = cell.resize((nw, nh), Image.LANCZOS)
                frame = Image.new('RGBA', (FRAME_W, FRAME_H), (0,0,0,0))
                frame.paste(cell, ((FRAME_W-nw)//2, FRAME_H-nh), cell)
                strip.paste(frame, (idx*FRAME_W, 0), frame)
                idx += 1
        print(f"Mode: square 4×2 grid ({cW}×{cH}) → 512×64")

    else:
        # Full image is content — divide into N equal columns
        cell_w = W // N_FRAMES
        for i in range(N_FRAMES):
            cell = img.crop((i*cell_w, t, (i+1)*cell_w, b))
            cw, ch = cell.size
            scale = min(FRAME_W/cw, FRAME_H/ch)
            nw, nh = int(cw*scale), int(ch*scale)
            cell = cell.resize((nw, nh), Image.LANCZOS)
            frame = Image.new('RGBA', (FRAME_W, FRAME_H), (0,0,0,0))
            frame.paste(cell, ((FRAME_W-nw)//2, FRAME_H-nh), cell)
            strip.paste(frame, (i*FRAME_W, 0), frame)
        print(f"Mode: full-width divide ({W}×{H}) → 512×64")

    return strip


def process(input_path: str, output_path: str) -> None:
    print(f"Input:  {input_path}")
    img = Image.open(input_path).convert('RGBA')
    W, H = img.size
    print(f"Size:   {W}×{H}")

    # Remove white/solid background if alpha channel is unused
    px = img.load()
    px = img.load()
    all_opaque = all(px[x,0][3] == 255 for x in range(0, W, 16))
    if all_opaque:
        img = flood_remove_bg(img)
        print("Background: flood-fill removal")
    else:
        print("Background: already transparent")

    result = make_strip(img)
    result.save(output_path)
    print(f"Output: {output_path}  ({result.size[0]}×{result.size[1]} RGBA)")


if __name__ == '__main__':
    if len(sys.argv) != 3:
        print("Usage: python3 fix_sprite.py <input.png> <faction_F_tT.png>")
        sys.exit(1)
    process(sys.argv[1], sys.argv[2])
