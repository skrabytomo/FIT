#!/usr/bin/env python3
"""Generate build/assets/icons.png — 256x128 icon atlas (8x4 grid of 32x32 cells).

Layout:
  Row 0: HeroPlayer HeroEnemy TownPlayer TownEnemy TownNeutral Scroll Artifact XPShrine
  Row 1: Cache      Gold      Iron       Faith     Blood       Sap    Mercury  (blank)
  Row 2: Observatory StatShrine BanditCamp Dwelling QuestGiver QuestTarget ForestShrine HighlandRuin
  Row 3: HolyFountain Oasis Campfire LavaCrystal SwampAltar (blank) (blank) (blank)
"""

import struct, zlib, os, math

W, H, CELL = 256, 128, 32
pixels = [(0, 0, 0, 0)] * (W * H)

# ── Primitives ────────────────────────────────────────────────────────────────
def put(x, y, c):
    if 0 <= x < W and 0 <= y < H:
        pixels[y * W + x] = c

def circ(cx, cy, r, c):
    ir = int(r) + 2
    for dy in range(-ir, ir + 1):
        for dx in range(-ir, ir + 1):
            if dx*dx + dy*dy <= r*r:
                put(int(cx)+dx, int(cy)+dy, c)

def ring(cx, cy, r, t, c):
    ri, ro = (r-t)*(r-t), r*r
    for dy in range(-int(r)-1, int(r)+2):
        for dx in range(-int(r)-1, int(r)+2):
            d = dx*dx + dy*dy
            if ri <= d <= ro:
                put(int(cx)+dx, int(cy)+dy, c)

def rect(x, y, w, h, c):
    for row in range(h):
        for col in range(w):
            put(x+col, y+row, c)

def diam(cx, cy, r, c):
    for dy in range(-r, r+1):
        for dx in range(-r, r+1):
            if abs(dx)+abs(dy) <= r:
                put(cx+dx, cy+dy, c)

def poly(pts, c):
    if len(pts) < 3: return
    miny = int(min(p[1] for p in pts))
    maxy = int(max(p[1] for p in pts))
    n = len(pts)
    for y in range(miny, maxy+1):
        xs = []
        for i in range(n):
            x1,y1 = pts[i]; x2,y2 = pts[(i+1)%n]
            if (y1 <= y < y2) or (y2 <= y < y1):
                xs.append(x1 + (y-y1)/(y2-y1)*(x2-x1))
        xs.sort()
        for j in range(0, len(xs)-1, 2):
            for x in range(int(xs[j]), int(xs[j+1])+1):
                put(x, y, c)

def star(cx, cy, ro, ri, c):
    pts = []
    for i in range(10):
        a = math.pi/2 - i*2*math.pi/10
        r = ro if i%2==0 else ri
        pts.append((cx + r*math.cos(a), cy - r*math.sin(a)))
    poly(pts, c)

# ── Cell helpers ──────────────────────────────────────────────────────────────
def ox(i): return (i % 8) * CELL
def oy(i): return (i // 8) * CELL
def ccx(i): return ox(i) + 16
def ccy(i): return oy(i) + 16

# ── Colors — deep medieval palette ───────────────────────────────────────────
DARK   = (8,6,4,255);       GOLD   = (185,145,18,255); GOLDBR=(240,205,55,255);  GOLDDK=(85,60,3,255)
RED    = (165,22,22,255);   REDBR  = (220,65,65,255);  REDDK =(58,6,6,255)
BLUE   = (22,55,175,255);   BLUEBR = (75,135,240,255); BLUEDK=(6,20,70,255)
BROWN  = (95,58,14,255);    BROWNBR= (148,98,35,255);  BROWNDK=(38,20,4,255)
CYAN   = (40,175,210,255);  CYANBR = (115,215,240,255);CYANDK=(12,65,95,255)
PURP   = (95,25,185,255);   PURPBR = (160,85,245,255); PURPDK=(32,6,78,255)
GREEN  = (22,130,38,255);   GREENBR= (80,200,92,255);  GREENDK=(8,50,14,255)
GRAY   = (88,92,102,255);   GRAYBR = (158,163,175,255);GRAYDK=(38,40,48,255)
WHITE  = (235,238,245,255); YELLOW = (245,205,15,255)
TEAL   = (28,150,140,255);  TEALBR = (85,210,200,255); TEALDK=(9,55,50,255)
PARCH  = (225,200,148,255); PARCHDK=(148,118,62,255)

# ─── 0: Hero Player (gold diamond + H) ───────────────────────────────────────
i=0; diam(ccx(i),ccy(i),14,GOLDDK); diam(ccx(i),ccy(i),12,GOLD); diam(ccx(i),ccy(i),9,GOLDBR)
rect(ox(i)+8, oy(i)+9, 4,14,DARK); rect(ox(i)+20,oy(i)+9,4,14,DARK); rect(ox(i)+12,oy(i)+14,8,4,DARK)
put(ox(i)+9,oy(i)+9,(255,240,180,255)); put(ox(i)+9,oy(i)+10,(255,240,180,255))

# ─── 1: Hero Enemy (red diamond + E) ─────────────────────────────────────────
i=1; diam(ccx(i),ccy(i),14,REDDK); diam(ccx(i),ccy(i),12,RED); diam(ccx(i),ccy(i),9,REDBR)
rect(ox(i)+8,oy(i)+9,4,14,DARK); rect(ox(i)+12,oy(i)+9,10,3,DARK)
rect(ox(i)+12,oy(i)+14,7,3,DARK); rect(ox(i)+12,oy(i)+20,10,3,DARK)
put(ox(i)+9,oy(i)+9,(255,120,120,255)); put(ox(i)+9,oy(i)+10,(255,120,120,255))

# ─── Castle helper ────────────────────────────────────────────────────────────
def castle(i, body, dk, gate):
    bx,by = ox(i),oy(i)
    rect(bx+5, by+12,22,20,body)           # tower body
    rect(bx+5, by+5, 6, 8,body)            # left merlon
    rect(bx+13,by+5, 6, 8,body)            # mid merlon
    rect(bx+21,by+5, 6, 8,body)            # right merlon
    rect(bx+11,by+19,10,13,gate)           # gate opening
    rect(bx+12,by+17,8, 3, gate)           # gate arch
    rect(bx+13,by+16,6, 2, gate)
    rect(bx+7, by+14,4, 4, gate)           # left window
    rect(bx+21,by+14,4, 4, gate)           # right window
    rect(bx+26,by+5, 1,27,dk)              # right shadow
    rect(bx+25,by+5, 1, 8,dk)

castle(2, BLUE,  BLUEDK,  (5,10,50,255))   # 2: Town Player
castle(3, RED,   REDDK,   (40,5,5,255))    # 3: Town Enemy
castle(4, BROWN, BROWNDK, (25,12,3,255))   # 4: Town Neutral

# ─── 5: ObjScroll ────────────────────────────────────────────────────────────
i=5; bx,by=ox(i),oy(i)
circ(ccx(i),ccy(i),13,CYANDK); circ(ccx(i),ccy(i),11,CYAN)
rect(bx+9,by+8,14,16,PARCH); rect(bx+9,by+8,14,2,PARCHDK); rect(bx+9,by+22,14,2,PARCHDK)
for ly in [11,13,15,17,19,21]: rect(bx+11,by+ly,10,1,PARCHDK)

# ─── 6: ObjArtifact (gold star) ──────────────────────────────────────────────
i=6
circ(ccx(i),ccy(i),13,GOLDDK)
star(ccx(i),ccy(i),12,5,GOLD)
star(ccx(i),ccy(i),9,4,GOLDBR)
circ(ccx(i),ccy(i),3,GOLDDK)
put(ccx(i)-1,ccy(i)-1,(255,255,200,255))

# ─── 7: ObjXP (purple + cross) ───────────────────────────────────────────────
i=7
circ(ccx(i),ccy(i),13,PURPDK); circ(ccx(i),ccy(i),11,PURP)
rect(ox(i)+13,oy(i)+6,6,20,YELLOW); rect(ox(i)+6,oy(i)+13,20,6,YELLOW)
circ(ccx(i),ccy(i),5,PURP); circ(ccx(i),ccy(i),2,PURPBR)

# ─── 8: ObjCache (green chest) ───────────────────────────────────────────────
i=8; bx,by=ox(i),oy(i)
rect(bx+5,by+15,22,15,GREENDK); rect(bx+5,by+10,22,6,GREEN); rect(bx+5,by+10,22,2,GREENBR)
circ(ccx(i),ccy(i)+4,3,GOLD)
rect(bx+5,by+15,22,1,GOLDDK); rect(bx+5,by+21,22,1,GOLDDK)
rect(bx+7,by+14,3,3,GOLDDK); rect(bx+22,by+14,3,3,GOLDDK)

# ─── 9: ResGold (coin) ───────────────────────────────────────────────────────
i=9
circ(ccx(i),ccy(i),13,(130,95,0,255)); circ(ccx(i),ccy(i),11,GOLD); circ(ccx(i),ccy(i),8,GOLDBR)
rect(ox(i)+14,oy(i)+9,4,14,(130,95,0,255))
rect(ox(i)+10,oy(i)+10,12,3,(130,95,0,255)); rect(ox(i)+10,oy(i)+14,12,3,(130,95,0,255))
rect(ox(i)+10,oy(i)+18,12,3,(130,95,0,255))

# ─── 10: ResIron (anvil) ─────────────────────────────────────────────────────
i=10; bx,by=ox(i),oy(i)
rect(bx+4,by+9,24,8,GRAYBR); rect(bx+8,by+17,16,10,GRAY); rect(bx+6,by+22,20,3,GRAYDK)
rect(bx+4,by+9,1,8,WHITE); rect(bx+4,by+9,24,1,WHITE); rect(bx+27,by+9,1,8,GRAYDK)

# ─── 11: ResFaith (crystal) ──────────────────────────────────────────────────
i=11
poly([(ccx(i),oy(i)+3),(ox(i)+25,oy(i)+13),(ox(i)+23,oy(i)+22),(ccx(i),oy(i)+29),
      (ox(i)+9,oy(i)+22),(ox(i)+7,oy(i)+13)], (185,195,225,255))
poly([(ccx(i),oy(i)+3),(ox(i)+25,oy(i)+13),(ox(i)+23,oy(i)+22),(ccx(i),oy(i)+29)], WHITE)
for y in range(oy(i)+3,oy(i)+29): put(ccx(i),y,(215,225,250,255))

# ─── 12: ResBlood (teardrop) ─────────────────────────────────────────────────
i=12
poly([(ccx(i),oy(i)+4),(ox(i)+22,oy(i)+14),(ox(i)+24,oy(i)+21),(ox(i)+20,oy(i)+27),
      (ccx(i),oy(i)+29),(ox(i)+12,oy(i)+27),(ox(i)+8,oy(i)+21),(ox(i)+10,oy(i)+14)], REDDK)
poly([(ccx(i),oy(i)+6),(ox(i)+21,oy(i)+15),(ox(i)+22,oy(i)+21),(ox(i)+19,oy(i)+26),
      (ccx(i),oy(i)+27),(ox(i)+13,oy(i)+26),(ox(i)+10,oy(i)+21),(ox(i)+11,oy(i)+15)], RED)
circ(ccx(i)-2,ccy(i)-3,3,REDBR)

# ─── 13: ResSap (leaf) ───────────────────────────────────────────────────────
i=13
poly([(ccx(i),oy(i)+3),(ox(i)+25,oy(i)+10),(ox(i)+27,oy(i)+18),(ox(i)+22,oy(i)+26),
      (ccx(i),oy(i)+29),(ox(i)+10,oy(i)+26),(ox(i)+5,oy(i)+18),(ox(i)+7,oy(i)+10)], GREENDK)
poly([(ccx(i),oy(i)+5),(ox(i)+23,oy(i)+11),(ox(i)+25,oy(i)+18),(ox(i)+20,oy(i)+25),
      (ccx(i),oy(i)+27),(ox(i)+12,oy(i)+25),(ox(i)+7,oy(i)+18),(ox(i)+9,oy(i)+11)], GREEN)
for y in range(oy(i)+5,oy(i)+27): put(ccx(i),y,GREENBR)
circ(ccx(i)-3,ccy(i)-4,3,GREENBR)

# ─── 14: ResMercury (metallic orb) ───────────────────────────────────────────
i=14
circ(ccx(i),ccy(i),13,TEALDK); circ(ccx(i),ccy(i),11,TEAL)
circ(ccx(i)-3,ccy(i)-3,6,(60,190,180,255)); circ(ccx(i)-4,ccy(i)-4,3,TEALBR)
circ(ccx(i),ccy(i),3,TEALDK); put(ccx(i)-1,ccy(i)-1,(200,250,245,255))

# ─── 15: blank (transparent, already zero) ───────────────────────────────────

# ─── 16: Observatory (gray circle with eye shape) ────────────────────────────
i=16; bx,by=ox(i),oy(i)
circ(ccx(i),ccy(i),13,GRAYDK); circ(ccx(i),ccy(i),11,GRAY)
# Eye shape: outer ellipse
for dy in range(-5,6):
    w2 = int((1.0 - (dy/5.0)**2)**0.5 * 10)
    rect(ccx(i)-w2, ccy(i)+dy, w2*2, 1, WHITE)
# Pupil
circ(ccx(i),ccy(i),4,(30,30,120,255)); circ(ccx(i),ccy(i),2,BLUEBR)

# ─── 17: StatShrine (red-orange circle with sword) ───────────────────────────
i=17
circ(ccx(i),ccy(i),13,(120,40,0,255)); circ(ccx(i),ccy(i),11,(200,80,20,255))
# Sword: vertical line
rect(ccx(i)-1,ccy(i)-10,3,16,GOLDBR)
# Cross guard
rect(ccx(i)-5,ccy(i)-3,10,3,GOLDBR)

# ─── 18: BanditCamp (dark camp with skull) ───────────────────────────────────
i=18; bx,by=ox(i),oy(i)
circ(ccx(i),ccy(i),13,BROWNDK); circ(ccx(i),ccy(i),11,(80,50,20,255))
# Skull outline
circ(ccx(i),ccy(i)-2,7,GRAYBR)
rect(bx+10,by+20,12,5,GRAYBR)
rect(bx+10,by+17,4,6,BROWNDK)
rect(bx+18,by+17,4,6,BROWNDK)
# Eyes
circ(ccx(i)-3,ccy(i)-3,2,BROWNDK); circ(ccx(i)+3,ccy(i)-3,2,BROWNDK)

# ─── 19: UnitDwelling (wooden hut) ───────────────────────────────────────────
i=19; bx,by=ox(i),oy(i)
# Walls
rect(bx+5,by+16,22,14,BROWN)
rect(bx+5,by+16,22,2,BROWNBR)
# Door
rect(bx+13,by+22,6,8,BROWNDK)
# Roof triangle
poly([(ccx(i),oy(i)+5),(ox(i)+3,oy(i)+17),(ox(i)+29,oy(i)+17)],(60,35,10,255))
poly([(ccx(i),oy(i)+7),(ox(i)+5,oy(i)+16),(ox(i)+27,oy(i)+16)],BROWNDK)
# Window
rect(bx+7,by+19,5,4,GOLDBR)
rect(bx+20,by+19,5,4,GOLDBR)

# ─── 20: QuestGiver (! on blue) ──────────────────────────────────────────────
i=20
circ(ccx(i),ccy(i),13,(15,50,150,255)); circ(ccx(i),ccy(i),11,BLUE)
# Exclamation mark
rect(ccx(i)-2,ccy(i)-9,5,13,YELLOW)
circ(ccx(i),ccy(i)+7,3,YELLOW)
rect(ccx(i)-2,ccy(i)+4,5,3,(15,50,150,255))

# ─── 21: QuestTarget (flag on pole) ──────────────────────────────────────────
i=21; bx,by=ox(i),oy(i)
circ(ccx(i),ccy(i),13,(0,80,30,255)); circ(ccx(i),ccy(i),11,(0,120,50,255))
# Pole
rect(ccx(i)-1,ccy(i)-10,2,20,GRAYBR)
# Flag
poly([(ccx(i),oy(i)+6),(ox(i)+22,oy(i)+10),(ccx(i),oy(i)+14)],REDBR)

# ─── 22: ForestShrine (green circle, tree silhouette) ────────────────────────
i=22; bx,by=ox(i),oy(i)
circ(ccx(i),ccy(i),13,GREENDK); circ(ccx(i),ccy(i),11,GREEN)
# Trunk
rect(ccx(i)-1,ccy(i)+2,3,7,BROWNDK)
# Canopy triangles
poly([(ccx(i),by+6),(bx+8,by+16),(bx+24,by+16)],(20,100,30,255))
poly([(ccx(i),by+10),(bx+9,by+18),(bx+23,by+18)],GREENBR)

# ─── 23: HighlandRuin (gray, crumbled tower) ─────────────────────────────────
i=23; bx,by=ox(i),oy(i)
circ(ccx(i),ccy(i),13,GRAYDK); circ(ccx(i),ccy(i),11,(80,85,90,255))
# Tower base
rect(bx+9,by+16,14,14,GRAYBR)
# Broken top — offset stones
rect(bx+9, by+10,5,7,GRAYBR); rect(bx+18,by+12,5,5,GRAYBR)
# Crack
rect(ccx(i),by+16,1,10,GRAYDK)

# ─── 24: HolyFountain (blue with arc sprays) ─────────────────────────────────
i=24; bx,by=ox(i),oy(i)
circ(ccx(i),ccy(i),13,BLUEDK); circ(ccx(i),ccy(i),11,BLUE)
# Basin
rect(bx+7,by+21,18,5,(60,130,220,255))
# Pillar
rect(ccx(i)-1,by+15,3,7,BLUEBR)
# Spray arcs (dots)
for ox2,oy2,r in [(ccx(i)-5,by+12,2),(ccx(i)+5,by+11,2),(ccx(i),by+9,3)]:
    circ(ox2,oy2,r,(160,220,255,255))

# ─── 25: Oasis (sandy with palm) ─────────────────────────────────────────────
i=25; bx,by=ox(i),oy(i)
circ(ccx(i),ccy(i),13,(140,100,30,255)); circ(ccx(i),ccy(i),11,(190,150,60,255))
# Water pool
circ(ccx(i),ccy(i)+3,5,(30,120,200,180))
# Palm trunk
rect(ccx(i)-1,ccy(i)-7,2,10,(110,70,20,255))
# Fronds
for dx,dy in [(-6,-8),(-3,-10),(0,-11),(3,-10),(6,-8)]:
    rect(ccx(i)+dx,ccy(i)+dy-7,2,2,GREEN)

# ─── 26: Campfire (orange with flames) ───────────────────────────────────────
i=26; bx,by=ox(i),oy(i)
circ(ccx(i),ccy(i),13,(100,40,5,255)); circ(ccx(i),ccy(i),11,(160,70,15,255))
# Log base
rect(bx+9,by+21,14,3,(100,55,15,255))
# Flames
poly([(ccx(i),by+8),(bx+11,by+21),(bx+21,by+21)],(255,130,20,255))
poly([(ccx(i),by+11),(bx+12,by+21),(bx+20,by+21)],(255,200,40,255))
circ(ccx(i),ccy(i)-1,4,(255,240,100,200))

# ─── 27: LavaCrystal (red with spikes) ───────────────────────────────────────
i=27; bx,by=ox(i),oy(i)
circ(ccx(i),ccy(i),13,(100,10,5,255)); circ(ccx(i),ccy(i),11,(180,40,20,255))
# Crystal spikes
for dx,dy,h2 in [(-4,0,10),(0,-2,13),(4,0,10),(-2,2,7),(2,2,7)]:
    poly([(ccx(i)+dx,by+h2),(ccx(i)+dx-2,by+20),(ccx(i)+dx+2,by+20)],(220,80,40,255))
circ(ccx(i),ccy(i),4,(255,140,80,255))

# ─── 28: SwampAltar (dark with drips) ────────────────────────────────────────
i=28; bx,by=ox(i),oy(i)
circ(ccx(i),ccy(i),13,(20,50,20,255)); circ(ccx(i),ccy(i),11,(40,80,35,255))
# Stone block
rect(bx+7,by+18,18,10,(70,70,50,255))
rect(bx+7,by+13,18,6,(90,90,65,255))
# Drips
for dx2 in [-4,0,4]:
    rect(ccx(i)+dx2,by+23,2,4,(60,140,40,200))
# Symbol
rect(ccx(i)-2,by+15,5,2,PURPBR); rect(ccx(i),by+13,2,5,PURPBR)

# ─── 29-31: blank ─────────────────────────────────────────────────────────────

# ── Write PNG ─────────────────────────────────────────────────────────────────
def write_png(path, w, h, pxls):
    def chunk(tag, data):
        crc = zlib.crc32(tag+data) & 0xFFFFFFFF
        return struct.pack('>I',len(data)) + tag + data + struct.pack('>I',crc)
    ihdr = struct.pack('>IIBBBBB', w, h, 8, 6, 0, 0, 0)
    raw = bytearray()
    for y in range(h):
        raw.append(0)
        for x in range(w):
            raw.extend(pxls[y*w+x])
    out  = b'\x89PNG\r\n\x1a\n'
    out += chunk(b'IHDR', ihdr)
    out += chunk(b'IDAT', zlib.compress(bytes(raw), 9))
    out += chunk(b'IEND', b'')
    os.makedirs(os.path.dirname(path) or '.', exist_ok=True)
    with open(path, 'wb') as f:
        f.write(out)
    print(f"Written {path} ({w}x{h}, {len(out)} bytes)")

write_png('build/assets/icons.png',  W, H, pixels)
write_png('game/assets/icons.png',   W, H, pixels)
