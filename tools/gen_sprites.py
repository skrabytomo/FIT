#!/usr/bin/env python3
"""
Sprite atlas generator for FIT.
Usage: REPLICATE_API_TOKEN=r8_xxx python3 tools/gen_sprites.py

Generates faction_0.png through faction_8.png (384x384 sprite atlases,
8 animation cols x 6 unit-tier rows) and saves to game/assets/sprites/.
"""

import os, sys, time, io, json, urllib.request
from pathlib import Path
from PIL import Image

TOKEN = os.environ.get("REPLICATE_API_TOKEN", "")
if not TOKEN:
    sys.exit("ERROR: set REPLICATE_API_TOKEN=r8_xxx before running")

ATLAS_W, ATLAS_H = 384, 384
COLS, ROWS = 8, 6
FRAME_W = ATLAS_W // COLS   # 48
FRAME_H = ATLAS_H // ROWS   # 64

ROOT    = Path(__file__).parent.parent
OUT_DIR = ROOT / "game" / "assets" / "sprites"
OUT_DIR.mkdir(parents=True, exist_ok=True)

# Replicate model: pixel-art fine-tuned SDXL
MODEL_VERSION = "8beff3369e81422112d93b89ca01426147de542cd4684c244b673b105188fe5f"

PIXEL_SUFFIX = (
    "pixel art, 16-bit sprite, clean pixel lines, dark solid background, "
    "centered full-body character, game unit sprite, HoMM3 style, fantasy"
)
NEGATIVE = (
    "blurry, photo, realistic, 3d, watermark, text, multiple characters, "
    "busy background, gradient background, jpeg artifacts"
)

FACTIONS = [
    (0, "Holy Order", "gold white silver",
     ["squire in silver chainmail with kite shield",
      "paladin in golden plate armor with winged helm",
      "crusader knight in ornate full plate, holy sword",
      "battle cleric in gilded robes swinging mace",
      "holy champion in radiant full plate, golden halo",
      "archangel warrior with golden wings and glowing sword"]),

    (1, "Crimson Wardens", "deep red black leather",
     ["crimson scout in light leather armor with short blades",
      "warden ranger in red cloak with twin swords",
      "crimson hunter with fur-lined red cloak and recurve bow",
      "blood warden berserker in dark armor with great axe",
      "warden commander in crimson plate armor with war banner",
      "crimson warlord in massive spiked black and red plate"]),

    (2, "Thornkin", "forest green brown bark",
     ["tiny vine sprite made of animated leaves and twigs",
      "thornkin warrior in bark armor with thorn club",
      "forest guardian with wooden shield and bone spear",
      "ancient treant, small tree creature with claw arms",
      "elder thornkin druid in root-plate armor with vine whip",
      "thornkin colossus, massive walking ancient tree with weapon arms"]),

    (3, "Eternal Empire", "bone white teal spectral",
     ["skeleton soldier with rusted sword and cracked shield",
      "armored skeleton warrior in teal-glowing bone plate",
      "zombie warrior in tattered imperial armor",
      "undead knight in ornate bone plate armor with spectral glow",
      "lich warrior in dark robes with spectral blade, floating",
      "eternal emperor undead in full necromantic regalia and crown"]),

    (4, "Bloodsworn", "dark red obsidian black",
     ["blood cultist in tattered robes with ritual dagger",
      "blood warrior with obsidian blade and blood-rune war paint",
      "bloodsworn berserker with twin axes and blood-rune armor",
      "blood champion in dark obsidian plate armor",
      "blood oracle warrior with bone staff and sacrificial armor",
      "bloodsworn avatar, massive demonic warrior in dark spiked armor"]),

    (5, "Voidkin", "purple black void energy",
     ["void sprite, small ethereal wisp-like creature with purple glow",
      "voidkin scout in dark purple robes with phase blade",
      "void stalker in shadow cloak with twin void daggers",
      "voidkin mage in flowing purple and black robes with crystal staff",
      "void wraith, translucent armored spectral warrior",
      "void herald in elaborate dark armor with void energy wings"]),

    (6, "Iron Assembly", "steel grey copper bronze gears",
     ["small mechanical automaton with spinning gear arms and riveted body",
      "iron assembly infantry with mechanical arm replacement and steel shield",
      "clockwork warrior in gear-studded plate armor with piston-driven fist",
      "iron assembly gunner with integrated brass cannon arm",
      "steam colossus with boiler back, smoke stacks, and cannon fists",
      "iron titan, massive quadrupedal war machine with multiple cannons"]),

    (7, "Amalgamate", "flesh pink dark veins bone",
     ["small amalgamate crawler, grotesque blob creature with claw hands",
      "flesh warrior with bone protrusions and organic armor",
      "amalgamate brute with multiple arms and fang-filled mouth",
      "flesh behemoth with integrated bone armor and multiple limbs",
      "amalgamate colossus with organic cannon arm and bone plate",
      "amalgamate apex, massive fused horror with tendrils and bone spires"]),

    (8, "Convergence", "silver chrome mirror reflective",
     ["convergence initiate, human with chrome plating on one arm",
      "convergence soldier, half-organic half-chrome armored warrior",
      "mirror warrior with fully reflective shield and phase sword",
      "convergence champion in silver-chrome full body armor",
      "convergence elite with chromed full plate and energy halberd",
      "convergence prime, perfect chrome-organic fusion in silver battle suit"]),
]


def replicate_call(prompt: str) -> bytes:
    """Submit prediction and poll until done; return raw PNG bytes."""
    headers = {
        "Authorization": f"Token {TOKEN}",
        "Content-Type": "application/json",
    }
    body = json.dumps({
        "version": MODEL_VERSION,
        "input": {
            "prompt": f"{prompt}, {PIXEL_SUFFIX}",
            "negative_prompt": NEGATIVE,
            "width": 512, "height": 512,
            "num_outputs": 1,
            "num_inference_steps": 25,
            "guidance_scale": 7.5,
            "scheduler": "K_EULER",
        }
    }).encode()

    req = urllib.request.Request(
        "https://api.replicate.com/v1/predictions",
        data=body, headers=headers, method="POST"
    )
    with urllib.request.urlopen(req) as r:
        pred = json.loads(r.read())

    poll_url = pred["urls"]["get"]
    for _ in range(90):
        time.sleep(3)
        req2 = urllib.request.Request(poll_url, headers={"Authorization": f"Token {TOKEN}"})
        with urllib.request.urlopen(req2) as r:
            result = json.loads(r.read())
        s = result["status"]
        if s == "succeeded":
            url = result["output"][0]
            with urllib.request.urlopen(url) as r:
                return r.read()
        if s == "failed":
            raise RuntimeError(result.get("error", "unknown error"))
    raise TimeoutError("Replicate did not complete in 4.5 min")


def make_frames(base: Image.Image) -> list:
    """
    Produce 8 animation frames from a base image:
    [idle×4, attack×2, hurt×1, dead×1]
    Uses simple pixel offsets / colour shifts — no AI re-generation needed.
    """
    w, h = base.size
    frames = []

    # Idle — slight vertical bob (0, -1, -2, -1 px)
    for dy in [0, -1, -2, -1]:
        f = Image.new("RGBA", (w, h), (0,0,0,0))
        f.paste(base, (0, dy), base)
        frames.append(f)

    # Attack — shift right 3 / 6 px + slight brighten
    for dx in [3, 6]:
        f = Image.new("RGBA", (w, h), (0,0,0,0))
        f.paste(base, (dx, -2), base)
        pix = f.load()
        for y in range(h):
            for x in range(w):
                r,g,b,a = pix[x,y]
                if a > 0:
                    pix[x,y] = (min(255,r+22), min(255,g+22), min(255,b+22), a)
        frames.append(f)

    # Hurt — shift left 3 px + redden
    f = Image.new("RGBA", (w, h), (0,0,0,0))
    f.paste(base, (-3, 0), base)
    pix = f.load()
    for y in range(h):
        for x in range(w):
            r,g,b,a = pix[x,y]
            if a > 0:
                pix[x,y] = (min(255,r+50), max(0,g-25), max(0,b-25), a)
    frames.append(f)

    # Dead — flatten to 55% height + darken
    dead_h = max(1, int(h * 0.55))
    flat = base.resize((w, dead_h), Image.LANCZOS)
    f = Image.new("RGBA", (w, h), (0,0,0,0))
    f.paste(flat, (0, h - dead_h), flat)
    pix = f.load()
    for y in range(h):
        for x in range(w):
            r,g,b,a = pix[x,y]
            if a > 0:
                pix[x,y] = (r//2, g//2, b//2, a)
    frames.append(f)

    return frames  # exactly 8


def pixelate(img: Image.Image, tw: int, th: int) -> Image.Image:
    """Resize to pixel art resolution with quantisation."""
    small = img.resize((tw, th), Image.LANCZOS)
    quantised = small.quantize(colors=24, method=Image.Quantize.MEDIANCUT).convert("RGBA")
    return quantised


def build_faction(fid: int, name: str, theme: str, descs: list) -> None:
    atlas = Image.new("RGBA", (ATLAS_W, ATLAS_H), (0,0,0,0))

    for tier, desc in enumerate(descs):
        prompt = f"{desc}, {theme} color palette"
        print(f"  [tier {tier+1}/6] {desc[:55]}...", flush=True)

        png_bytes = replicate_call(prompt)
        base = Image.open(io.BytesIO(png_bytes)).convert("RGBA")
        frames = make_frames(base)  # 8 frames

        for col, fr in enumerate(frames):
            cell = pixelate(fr, FRAME_W, FRAME_H)
            atlas.paste(cell, (col * FRAME_W, tier * FRAME_H), cell)

    out = OUT_DIR / f"faction_{fid}.png"
    atlas.save(out)
    print(f"  → saved {out.name}")


def main():
    print(f"FIT sprite generator  |  frame {FRAME_W}×{FRAME_H}  atlas {ATLAS_W}×{ATLAS_H}")
    print(f"Output: {OUT_DIR}\n")
    for fid, name, theme, descs in FACTIONS:
        print(f"=== Faction {fid}: {name} ===")
        try:
            build_faction(fid, name, theme, descs)
        except Exception as e:
            print(f"  FAILED: {e}")
        print()
    print("Done — rebuild the game to see new sprites.")


if __name__ == "__main__":
    main()
