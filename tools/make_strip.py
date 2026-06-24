#!/usr/bin/env python3
"""
Convert a single character image → 8-frame 512×64 sprite strip.

Usage: python3 tools/make_strip.py <character.png> <faction_F_tT.png>

Input: any size image of ONE character, solid or transparent background.
Output: 512×64 RGBA strip (8 cols × 64px each):
  col 0-3 = idle (slight bob)
  col 4-5 = attack (forward lunge + brighten)
  col   6 = hurt   (shift left + redden)
  col   7 = dead   (flatten + darken)
"""

import sys
import numpy as np
from collections import deque
from PIL import Image, ImageFilter

FRAME_W = 64
FRAME_H = 64
N       = 8


def flood_remove_bg(img: Image.Image, tol: int = 40) -> Image.Image:
    arr = np.array(img.convert('RGBA'))
    h, w = arr.shape[:2]
    bg = arr[0, 0, :3].astype(int)
    def is_bg(y, x):
        return int(np.abs(arr[y,x,:3].astype(int)-bg).sum()) < tol
    visited = np.zeros((h,w), bool)
    mask    = np.zeros((h,w), bool)
    q = deque()
    seeds = [(y,x) for x in range(w) for y in (0,h-1)] + \
            [(y,x) for y in range(h) for x in (0,w-1)]
    for y,x in seeds:
        if not visited[y,x] and is_bg(y,x):
            visited[y,x]=True; mask[y,x]=True; q.append((y,x))
    while q:
        y,x = q.popleft()
        for dy,dx in ((-1,0),(1,0),(0,-1),(0,1)):
            ny,nx = y+dy,x+dx
            if 0<=ny<h and 0<=nx<w and not visited[ny,nx] and is_bg(ny,nx):
                visited[ny,nx]=True; mask[ny,nx]=True; q.append((ny,nx))
    arr[mask,3]=0; arr[~mask,3]=255
    return Image.fromarray(arr)


def tight_crop(img: Image.Image) -> Image.Image:
    arr = np.array(img)
    alpha = arr[:,:,3]
    rows = np.where(alpha.max(axis=1)>10)[0]
    cols = np.where(alpha.max(axis=0)>10)[0]
    if not len(rows) or not len(cols):
        return img
    return img.crop((cols[0], rows[0], cols[-1]+1, rows[-1]+1))


def fit_frame(img: Image.Image) -> Image.Image:
    """Scale character to fill 64×64, bottom-aligned."""
    cw, ch = img.size
    scale = min((FRAME_W-2)/cw, (FRAME_H-2)/ch)
    nw, nh = max(1,int(cw*scale)), max(1,int(ch*scale))
    img = img.resize((nw, nh), Image.LANCZOS)
    frame = Image.new('RGBA', (FRAME_W, FRAME_H), (0,0,0,0))
    ox = (FRAME_W-nw)//2
    oy = FRAME_H-nh
    frame.paste(img, (ox, oy), img)
    return frame


def shift(base: Image.Image, dx: int, dy: int) -> Image.Image:
    out = Image.new('RGBA', (FRAME_W, FRAME_H), (0,0,0,0))
    out.paste(base, (dx, dy), base)
    return out


def tint(frame: Image.Image, r_add:int, g_add:int, b_add:int) -> Image.Image:
    arr = np.array(frame).astype(int)
    arr[:,:,0] = np.clip(arr[:,:,0]+r_add, 0, 255)
    arr[:,:,1] = np.clip(arr[:,:,1]+g_add, 0, 255)
    arr[:,:,2] = np.clip(arr[:,:,2]+b_add, 0, 255)
    return Image.fromarray(arr.astype(np.uint8))


def make_frames(base: Image.Image) -> list:
    """Generate 8 animation frames from a single idle character."""
    frames = []

    # Idle ×4 — gentle vertical bob (0, -2, -3, -2 px)
    for dy in [0, -2, -3, -2]:
        frames.append(shift(base, 0, dy))

    # Attack ×2 — step forward + brighten
    for dx, dy in [(4, -2), (7, -3)]:
        f = shift(base, dx, dy)
        f = tint(f, 25, 20, 10)
        frames.append(f)

    # Hurt — step back + redden
    f = shift(base, -4, 0)
    f = tint(f, 55, -25, -25)
    frames.append(f)

    # Dead — flatten to 40% height + darken + shift down
    arr = np.array(base)
    h, w = arr.shape[:2]
    dead_h = max(1, int(h * 0.40))
    flat = Image.fromarray(arr).resize((w, dead_h), Image.LANCZOS)
    flat = tint(flat.convert('RGBA'), -60, -60, -60)
    dead = Image.new('RGBA', (FRAME_W, FRAME_H), (0,0,0,0))
    dead.paste(flat, ((FRAME_W-w)//2, FRAME_H-dead_h), flat)
    frames.append(dead)

    return frames  # exactly 8


def process(input_path: str, output_path: str) -> None:
    print(f"Input:  {input_path}")
    img = Image.open(input_path)
    W, H = img.size
    print(f"Size:   {W}×{H}  mode={img.mode}")

    # Remove background
    img = flood_remove_bg(img)

    # Tight-crop to character bounds
    img = tight_crop(img)
    print(f"Cropped to character: {img.size}")

    # Fit into 64×64 frame
    base = fit_frame(img)

    # Generate 8 frames
    frames = make_frames(base)

    # Assemble strip
    strip = Image.new('RGBA', (FRAME_W*N, FRAME_H), (0,0,0,0))
    for i, f in enumerate(frames):
        strip.paste(f, (i*FRAME_W, 0), f)

    strip.save(output_path)
    print(f"Output: {output_path}  (512×64 RGBA, 8 frames)")

    # Save 8× preview
    preview_path = output_path.replace('.png', '_preview.png')
    big = strip.resize((FRAME_W*N*8, FRAME_H*8), Image.NEAREST)
    bg  = Image.new('RGB', big.size, (50, 44, 38))
    bg.paste(big, (0,0), big)
    bg.save(preview_path)
    print(f"Preview:{preview_path}")


if __name__ == '__main__':
    if len(sys.argv) != 3:
        print("Usage: python3 make_strip.py <character.png> <faction_F_tT.png>")
        sys.exit(1)
    process(sys.argv[1], sys.argv[2])
