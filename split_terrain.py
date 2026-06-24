#!/usr/bin/env python3
"""
Split a terrain sprite sheet into individual hex tile PNG files.
Usage: python3 split_terrain.py <input.png> <terrain_name> [output_dir]

Detects individual hex tiles on a white/light background by finding
connected non-background regions, sorts them left-to-right top-to-bottom,
and saves as terrain_name_0.png, terrain_name_1.png, etc.
"""
import sys
import os
import numpy as np
from PIL import Image
from collections import deque

def find_tiles(img_path, bg_thresh=240):
    img = Image.open(img_path).convert('RGBA')
    arr = np.array(img)
    h, w = arr.shape[:2]

    # Create mask: True = non-background pixel
    # Background: all channels near-white (or transparent)
    r, g, b, a = arr[:,:,0], arr[:,:,1], arr[:,:,2], arr[:,:,3]
    is_bg = ((r > bg_thresh) & (g > bg_thresh) & (b > bg_thresh)) | (a < 30)
    content = ~is_bg

    # Label connected components (simple flood fill)
    labeled = np.zeros((h, w), dtype=int)
    label = 0
    for sy in range(h):
        for sx in range(w):
            if content[sy, sx] and labeled[sy, sx] == 0:
                label += 1
                q = deque([(sy, sx)])
                labeled[sy, sx] = label
                while q:
                    cy, cx = q.popleft()
                    for dy, dx in [(-1,0),(1,0),(0,-1),(0,1),
                                   (-1,-1),(-1,1),(1,-1),(1,1)]:
                        ny, nx = cy+dy, cx+dx
                        if 0<=ny<h and 0<=nx<w and content[ny,nx] and labeled[ny,nx]==0:
                            labeled[ny, nx] = label
                            q.append((ny, nx))

    # Get bounding boxes for each label
    bboxes = []
    for lbl in range(1, label+1):
        ys, xs = np.where(labeled == lbl)
        if len(ys) < 500:  # skip tiny noise regions
            continue
        miny, maxy = ys.min(), ys.max()
        minx, maxx = xs.min(), xs.max()
        area = (maxy-miny) * (maxx-minx)
        bboxes.append((miny, minx, maxy, maxx, area))

    if not bboxes:
        print("No tiles found! Try adjusting bg_thresh.")
        return []

    # Filter out very small bboxes (< 10% of median area)
    areas = [b[4] for b in bboxes]
    med = sorted(areas)[len(areas)//2]
    bboxes = [b for b in bboxes if b[4] > med * 0.1]

    # Sort: top-to-bottom, left-to-right (bin rows by y with tolerance)
    bboxes.sort(key=lambda b: (b[0] // (med**0.5 * 0.5), b[1]))

    print(f"Found {len(bboxes)} tiles")
    return bboxes, arr, img


def split_and_save(img_path, terrain_name, output_dir=None):
    if output_dir is None:
        output_dir = os.path.join(os.path.dirname(img_path))

    result = find_tiles(img_path)
    if not result:
        return
    bboxes, arr, img = result

    os.makedirs(output_dir, exist_ok=True)

    # Target size for game tiles
    TARGET = 512

    for i, (miny, minx, maxy, maxx, _) in enumerate(bboxes):
        # Add padding
        pad = 8
        miny = max(0, miny - pad)
        minx = max(0, minx - pad)
        maxy = min(arr.shape[0], maxy + pad)
        maxx = min(arr.shape[1], maxx + pad)

        tile = Image.fromarray(arr[miny:maxy+1, minx:maxx+1])
        # Make square and resize to TARGET
        tw, th = tile.size
        side = max(tw, th)
        square = Image.new('RGBA', (side, side), (255, 255, 255, 0))
        square.paste(tile, ((side-tw)//2, (side-th)//2))
        out = square.resize((TARGET, TARGET), Image.LANCZOS)
        # Convert to RGB (game textures are RGB)
        rgb = Image.new('RGB', out.size, (255, 255, 255))
        rgb.paste(out, mask=out.split()[3] if out.mode == 'RGBA' else None)

        outpath = os.path.join(output_dir, f"{terrain_name}_{i}.png")
        rgb.save(outpath)
        print(f"  Saved tile {i}: {outpath} ({tw}x{th} → {TARGET}x{TARGET})")

    print(f"Done: {len(bboxes)} tiles saved as {terrain_name}_0 .. {terrain_name}_{len(bboxes)-1}")


if __name__ == '__main__':
    if len(sys.argv) < 3:
        print("Usage: python3 split_terrain.py <input.png> <terrain_name> [output_dir]")
        print("Example: python3 split_terrain.py corrupted_sheet.png corrupted game/assets/terrain/")
        sys.exit(1)
    split_and_save(sys.argv[1], sys.argv[2],
                   sys.argv[3] if len(sys.argv) > 3 else None)
