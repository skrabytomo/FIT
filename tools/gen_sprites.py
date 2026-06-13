#!/usr/bin/env python3
"""Generate faction unit sprite atlases for combat animation.

Layout per atlas (384×384):
  - 8 columns = animation frames: [idle:4][attack:2][hurt:1][dead:1]
  - 6 rows    = unit tiers 1-6

One PNG per faction (faction_0.png … faction_8.png).
"""
import struct, zlib, os, math

FW, FH   = 48, 64   # pixels per frame cell
NCOLS    = 8         # animation frames per unit
NROWS    = 6         # tiers per faction atlas
ATLAS_W  = FW * NCOLS   # 384
ATLAS_H  = FH * NROWS   # 384

# ── PNG writer ────────────────────────────────────────────────────────────────
def write_png(path, w, h, px):
    def chunk(t, d):
        c = zlib.crc32(t + d) & 0xffffffff
        return struct.pack('>I', len(d)) + t + d + struct.pack('>I', c)
    raw = bytearray()
    for y in range(h):
        raw.append(0)
        raw += px[y*w*4:(y+1)*w*4]
    out  = b'\x89PNG\r\n\x1a\n'
    out += chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 6, 0, 0, 0))
    out += chunk(b'IDAT', zlib.compress(bytes(raw), 6))
    out += chunk(b'IEND', b'')
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, 'wb') as f:
        f.write(out)

# ── Canvas ────────────────────────────────────────────────────────────────────
class Canvas:
    def __init__(self, w, h):
        self.w = w; self.h = h
        self.px = bytearray(w * h * 4)  # RGBA, fully transparent

    def put(self, x, y, r, g, b, a=255):
        if 0 <= x < self.w and 0 <= y < self.h:
            i = (y * self.w + x) * 4
            if a == 255:
                self.px[i:i+4] = [r, g, b, 255]
            else:
                sa = a / 255.0
                da = 1.0 - sa
                self.px[i]   = int(r * sa + self.px[i]   * da)
                self.px[i+1] = int(g * sa + self.px[i+1] * da)
                self.px[i+2] = int(b * sa + self.px[i+2] * da)
                self.px[i+3] = min(255, int(a + self.px[i+3] * da))

    def circ(self, cx, cy, r, col, fill=True):
        r2 = r * r; ri2 = (r - 1) * (r - 1)
        for y in range(max(0, cy-r), min(self.h, cy+r+1)):
            for x in range(max(0, cx-r), min(self.w, cx+r+1)):
                d = (x-cx)**2 + (y-cy)**2
                if fill and d <= r2:           self.put(x, y, *col)
                elif not fill and ri2 <= d <= r2: self.put(x, y, *col)

    def rect(self, x, y, w, h, col, fill=True):
        for ry in range(y, y+h):
            for rx in range(x, x+w):
                if fill or rx==x or rx==x+w-1 or ry==y or ry==y+h-1:
                    self.put(rx, ry, *col)

    def line(self, x0, y0, x1, y1, col, thick=1):
        dx = abs(x1-x0); dy = abs(y1-y0)
        sx = 1 if x0 < x1 else -1
        sy = 1 if y0 < y1 else -1
        err = dx - dy; t = thick // 2
        while True:
            for ox in range(-t, t+1):
                for oy in range(-t, t+1):
                    self.put(x0+ox, y0+oy, *col)
            if x0 == x1 and y0 == y1:
                break
            e2 = 2 * err
            if e2 > -dy: err -= dy; x0 += sx
            if e2 < dx:  err += dx; y0 += sy

# ── Color helpers ─────────────────────────────────────────────────────────────
def lerp(a, b, t):
    return tuple(max(0, min(255, int(a[i] + (b[i]-a[i])*t))) for i in range(4))

def tint(col, factor):
    return (min(255,int(col[0]*factor)), min(255,int(col[1]*factor)),
            min(255,int(col[2]*factor)), col[3])

def brighten(col, add):
    return (min(255,col[0]+add), min(255,col[1]+add), min(255,col[2]+add), col[3])

# ── Weapon drawing ────────────────────────────────────────────────────────────
def draw_weapon(cv, x, y, wtype, col, frame, s):
    orb   = (160, 100, 240, 230)
    spark = (255, 240,  80, 220)
    steel = (210, 215, 225, 255)
    if wtype == "sword":
        cv.line(x, y, x+s, y-s*2, steel, 2)
        cv.line(x-2, y-s, x+2, y-s, col, 1)
    elif wtype == "axe":
        cv.line(x, y, x+s//2, y-s*2, steel, 2)
        cv.rect(x+s//2-2, y-s*2-4, 6, 5, col)
    elif wtype == "staff":
        cv.line(x, y, x, y-s*3, col, 2)
        cv.circ(x, y-s*3-2, 3, orb)
        cv.circ(x, y-s*3-2, 2, (200, 160, 255, 255))
    elif wtype == "gun":
        cv.rect(x, y-1, max(4,s*2), 3, col)
        cv.rect(x+1, y, max(3,s*2-1), 1, steel)
        if frame == 5:
            cv.circ(x+max(4,s*2)+1, y, 3, spark)
    elif wtype == "claw":
        cv.line(x, y, x+s,   y-s,   col, 2)
        cv.line(x, y, x+s+2, y-s+3, col, 1)
        cv.line(x, y, x+s-2, y-s-3, col, 1)
    if frame == 5 and wtype not in ("gun",):
        cv.circ(x+s, y-s*2, 3, spark)
        cv.circ(x+s, y-s*2, 1, (255, 255, 200, 255))

# ── Shield drawing ─────────────────────────────────────────────────────────────
def draw_shield(cv, bx, by_unused, x_off, y_off, arm_y, bw, sc, body, accent):
    sw = max(4, int(7 * sc))
    sh = max(5, int(9 * sc))
    sx = bx + x_off - bw//2 - sw - 1
    sy = arm_y - 1 + y_off
    shield_body = tint(body, 0.70)
    shield_hi   = brighten(body, 40)
    shield_acc  = accent
    for dy in range(sh):
        ratio = dy / max(1, sh - 1)
        if ratio < 0.65:
            fw = sw
        else:
            fw = max(1, int(sw * (1.0 - (ratio - 0.65) / 0.35)))
        csx = sx + sw // 2
        for ddx in range(-fw//2, fw//2+1):
            cv.put(csx+ddx, sy+dy, *shield_body)
    # Highlight line on left edge
    for dy in range(sh // 2):
        cv.put(sx, sy+dy, *shield_hi)
    # Boss / rivet in centre
    mid = sy + sh // 3
    cv.put(sx + sw//2,     mid, *shield_acc)
    cv.put(sx + sw//2 - 1, mid, *shield_acc)

# ── Cape drawing ──────────────────────────────────────────────────────────────
def draw_cape(cv, bx, x_off, y_off, torso_y, bw, bh, leg, body):
    def cape_rgba(factor, alpha):
        return (min(255,int(body[0]*factor)), min(255,int(body[1]*factor)),
                min(255,int(body[2]*factor)), alpha)
    cape_h = bh + leg - 2
    for cy_c in range(cape_h):
        ratio = cy_c / max(1, cape_h)
        if ratio < 0.3:
            fw = bw + 4
        else:
            fw = max(2, int((bw + 4) * (1.0 - (ratio - 0.3) / 0.7)))
        base_x = bx + x_off
        col = cape_rgba(0.60, 200) if cy_c < cape_h // 2 else cape_rgba(0.45, 180)
        for ddx in range(-fw//2, fw//2+1):
            cv.put(base_x + ddx, torso_y + cy_c, *col)

# ── Unit drawing ──────────────────────────────────────────────────────────────
SKIN = (220, 180, 140, 255)
DARK = (20,  20,  20,  200)

def draw_unit(cv, fx, fy, body, accent, weapon, tier, frame, skin=None):
    """Draw one 48×64 unit into canvas at cell origin (fx, fy)."""
    if skin is None:
        skin = SKIN
    sc = 0.55 + 0.09*(tier-1)

    hr  = max(3, int(6*sc))
    bw  = max(5, int(10*sc))
    bh  = max(5, int(11*sc))
    leg = max(4, int(10*sc))
    arm = max(3, int(8*sc))

    bx = fx + 24
    by = fy + 58

    y_off = 0; x_off = 0; flash = False; dead = False
    if frame <= 3:
        y_off = [0, -1, -2, -1][frame]
    elif frame == 4:
        x_off = 3; y_off = -1
    elif frame == 5:
        x_off = 7; y_off = 0
    elif frame == 6:
        x_off = 3; flash = True
    elif frame == 7:
        dead = True

    body_c   = lerp(body,   (255,255,255,255), 0.55) if flash else body
    accent_c = lerp(accent, (255,255,255,255), 0.40) if flash else accent
    skin_c   = lerp(skin,   (255,255,255,255), 0.60) if flash else skin

    if dead:
        dc = lerp(body, (40,40,40,0), 0.5)
        dl = max(5, int(bw*1.5))
        dh = max(3, int(bh*0.5))
        cv.rect(bx-dl, by-hr-dh//2, dl*2, dh, dc)
        cv.circ(bx-dl+hr, by-hr, hr, dc)
        return

    # Shadow
    cv.circ(bx+x_off, by+1, max(3, int(bw*0.55)), (0,0,0,50))

    torso_y = by - 1 - leg - bh + y_off

    # Cape behind torso (T5+)
    if tier >= 5:
        draw_cape(cv, bx, x_off, y_off, torso_y, bw, bh, leg, body)

    # Foot circles
    cv.circ(bx+x_off-bw//3, by+y_off, 2, skin_c)
    cv.circ(bx+x_off+bw//3, by+y_off, 2, skin_c)

    # Legs
    leg_top_y = by - 1
    for side in (-1, 1):
        swing = [0, 2, 0, -2][frame] * side if frame <= 3 else 0
        foot_x = bx + x_off + side * bw//3 + swing
        cv.line(foot_x, leg_top_y,
                bx + x_off + side * 2, leg_top_y - leg + y_off,
                skin_c, 2)

    # Torso
    cv.rect(bx+x_off-bw//2, torso_y, bw, bh, body_c)
    # Chest detail strip
    cv.rect(bx+x_off-bw//4, torso_y+2, max(2,bw//2), max(3,bh-4), accent_c)
    # Armor highlight (upper-left catch-light)
    hi = brighten(body_c, 70)
    cv.put(bx+x_off-bw//2+1, torso_y+1, *hi)
    cv.put(bx+x_off-bw//2+2, torso_y+1, *hi)

    arm_y = torso_y + 3

    # Shield (T2+ non-gun)
    if tier >= 2 and weapon != "gun":
        draw_shield(cv, bx, by, x_off, y_off, arm_y, bw, sc, body_c, accent_c)

    # Tier-based armour extras
    if tier >= 3:
        # Helmet crest
        cv.circ(bx+x_off, torso_y-hr-1+y_off-hr+2, hr//2+1, body_c)
    if tier >= 5:
        # Pauldrons (shoulder plates)
        cv.circ(bx+x_off-bw//2-2, arm_y-1, hr//2+1, body_c)
        cv.circ(bx+x_off+bw//2+2, arm_y-1, hr//2+1, body_c)
        cv.put(bx+x_off-bw//2-2, arm_y-2, *brighten(body_c, 50))
        cv.put(bx+x_off+bw//2+2, arm_y-2, *brighten(body_c, 50))

    # Arms
    if frame in (4, 5):
        ext = 4 if frame == 4 else 8
        cv.line(bx+x_off+bw//2, arm_y,
                bx+x_off+bw//2+ext+arm, arm_y+2+y_off, accent_c, 2)
        cv.line(bx+x_off-bw//2, arm_y,
                bx+x_off-bw//2-arm//2, arm_y+arm//2+y_off, accent_c, 2)
        wpx = bx+x_off+bw//2+ext+arm+2
        wpy = arm_y+2+y_off
    else:
        cv.line(bx+x_off+bw//2, arm_y,
                bx+x_off+bw//2+arm//2, arm_y+arm+y_off, accent_c, 2)
        cv.line(bx+x_off-bw//2, arm_y,
                bx+x_off-bw//2+arm//2, arm_y+arm+y_off, accent_c, 2)
        wpx = bx+x_off+bw//2+arm//2+1
        wpy = arm_y+arm+y_off

    draw_weapon(cv, wpx, wpy, weapon, accent_c, frame, max(2, int(4*sc)))

    # Head
    hx = bx + x_off
    hy = torso_y - hr - 1 + y_off
    cv.circ(hx, hy, hr, skin_c)
    if hr >= 4:
        cv.put(hx-hr//2, hy-1, *DARK)
        cv.put(hx+hr//2, hy-1, *DARK)
    # Skin highlight
    hi_s = brighten(skin_c, 45)
    cv.put(hx-hr//3, hy-hr//3, *hi_s)

# ── Faction table ─────────────────────────────────────────────────────────────
# (id, name, body RGBA, accent RGBA, weapon, skin RGBA)
FACTIONS = [
    (0, "HolyOrder",     (155, 120,  35, 255), (230, 210, 140, 255), "sword", (225, 195, 155, 255)),
    (1, "CrimsonWardens",(130,  18,  18, 255), (175, 175, 185, 255), "axe",   (195, 155, 125, 255)),
    (2, "Thornkin",      ( 25,  75,  15, 255), ( 75, 155,  35, 255), "staff", (155, 190, 120, 255)),
    (3, "EternalEmpire", ( 55,  18, 105, 255), (145,  95, 195, 255), "staff", (230, 215, 205, 255)),
    (4, "Bloodsworn",    (100,   8,   8, 255), ( 55,   5,   5, 255), "axe",   (175, 120, 100, 255)),
    (5, "Voidkin",       ( 15,   8,  55, 255), ( 50, 165, 200, 255), "staff", (190, 180, 215, 255)),
    (6, "IronAssembly",  ( 75,  75,  85, 255), (165,  95,  25, 255), "gun",   (155, 155, 165, 255)),
    (7, "Amalgamate",    ( 65, 100,  15, 255), (155, 185,  25, 255), "claw",  (145, 165, 115, 255)),
    (8, "Convergence",   ( 15,  40, 130, 255), (185, 145,  25, 255), "sword", (215, 215, 235, 255)),
]

# ── Main ──────────────────────────────────────────────────────────────────────
def main():
    for fid, name, body, accent, weapon, skin in FACTIONS:
        cv = Canvas(ATLAS_W, ATLAS_H)
        for tier in range(1, NROWS+1):
            fy = (tier-1) * FH
            for frame in range(NCOLS):
                fx = frame * FW
                draw_unit(cv, fx, fy, body, accent, weapon, tier, frame, skin)

        for dest in [
            f"game/assets/sprites/faction_{fid}.png",
            f"build/assets/sprites/faction_{fid}.png",
        ]:
            write_png(dest, ATLAS_W, ATLAS_H, cv.px)
        print(f"  [{fid}] {name:16s}  →  faction_{fid}.png")

if __name__ == "__main__":
    print("Generating faction sprite atlases …")
    main()
    print("Done.")
