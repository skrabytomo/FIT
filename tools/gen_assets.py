#!/usr/bin/env python3
"""Generate build/assets/icons.png — 256x96 icon atlas (8x3 grid of 32x32 cells).

Layout:
  Row 0: HeroPlayer HeroEnemy TownPlayer TownEnemy TownNeutral Scroll Artifact XPShrine
  Row 1: Cache      Gold      Iron       Faith     Blood       Sap    Mercury  (blank)
  Row 2: Observatory StatShrine BanditCamp Dwelling QuestGiver QuestTarget (blank) (blank)
"""

import struct, zlib, os, math

W, H, CELL = 256, 96, 32
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

# ── Colors ────────────────────────────────────────────────────────────────────
DARK   = (10,8,6,255);      GOLD   = (210,170,30,255); GOLDBR=(255,220,70,255);  GOLDDK=(100,75,5,255)
RED    = (190,30,30,255);   REDBR  = (240,80,80,255);  REDDK =(70,8,8,255)
BLUE   = (30,70,190,255);   BLUEBR = (90,150,255,255); BLUEDK=(8,25,80,255)
BROWN  = (110,70,20,255);   BROWNBR= (165,115,45,255); BROWNDK=(45,25,5,255)
CYAN   = (50,190,220,255);  CYANBR = (140,230,250,255);CYANDK=(15,80,110,255)
PURP   = (110,35,200,255);  PURPBR = (180,100,255,255);PURPDK=(40,8,90,255)
GREEN  = (30,150,50,255);   GREENBR= (100,220,110,255);GREENDK=(10,60,18,255)
GRAY   = (100,105,115,255); GRAYBR = (175,180,190,255);GRAYDK=(45,48,55,255)
WHITE  = (240,242,248,255); YELLOW = (250,215,20,255)
TEAL   = (35,165,155,255);  TEALBR = (100,225,215,255);TEALDK=(12,65,60,255)
PARCH  = (235,215,160,255); PARCHDK=(165,135,75,255)

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

# ─── 22-23: blank ────────────────────────────────────────────────────────────

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
