# Sprite Generation Brief — FIT Game

## Format spec (paste into every ChatGPT / DALL-E request)

```
Generate a pixel art sprite sheet for a 2D strategy game unit.

CANVAS: 512 × 64 pixels, transparent background (PNG)
LAYOUT: 8 frames in a single horizontal row, each frame exactly 64×64px
STYLE: 16-bit pixel art, HoMM3 / Final Fantasy Tactics aesthetic,
       clean dark outlines, limited palette (~24 colours), no anti-aliasing,
       no gradients — dithering only

FRAME ORDER (left→right):
  1  Idle A  — neutral standing pose
  2  Idle B  — slight downward weight shift (~2px lower)
  3  Idle C  — upward bob (~2px higher than neutral)
  4  Idle D  — return to neutral
  5  Attack A — wind-up: weapon raised / body coiled
  6  Attack B — follow-through: weapon at point of impact, forward lunge
  7  Hurt    — recoiling backward, pain expression
  8  Dead    — collapsed or fallen, no longer upright

FACING: character faces RIGHT (enemies are flipped automatically in-engine)
GROUNDING: feet touch the bottom edge of each 64×64 cell
BACKGROUND: fully transparent PNG — no floor, shadow, or scenery
```

---

## Unit list — one image request per row

File naming: `faction_F_tT.png` where F = faction index (0–8), T = tier (1–6)
Drop finished files into: `game/assets/sprites/`

### Faction 0 — Holy Order (gold, white, silver palette)

| File | Character |
|------|-----------|
| faction_0_t1.png | Squire — young soldier in light silver chainmail, kite shield, short sword |
| faction_0_t2.png | Paladin — armoured knight in golden plate with winged helmet and longsword |
| faction_0_t3.png | Crusader — full plate crusader, ornate cross on breastplate, two-handed sword |
| faction_0_t4.png | Battle Cleric — robed priest in gilded armour swinging a heavy mace |
| faction_0_t5.png | Holy Champion — radiant full-plate knight, golden halo, glowing blessed sword |
| faction_0_t6.png | Archangel — winged warrior in white and gold armour, heavenly sword of light |

### Faction 1 — Crimson Wardens (deep red, black, dark leather)

| File | Character |
|------|-----------|
| faction_1_t1.png | Scout — lightly armoured in dark leather, short twin blades |
| faction_1_t2.png | Ranger — red hooded cloak, twin curved swords, quick stance |
| faction_1_t3.png | Hunter — fur-trimmed red cloak, recurve bow drawn, quiver on back |
| faction_1_t4.png | Berserker — bare upper body with crimson war paint, massive great axe |
| faction_1_t5.png | Warden Commander — dark crimson plate armour, war banner on back, broadsword |
| faction_1_t6.png | Warlord — imposing black and red spiked full plate, dual war axes |

### Faction 2 — Thornkin (forest green, brown bark, earthy tones)

| File | Character |
|------|-----------|
| faction_2_t1.png | Vine Sprite — tiny animated creature made of twigs and leaves |
| faction_2_t2.png | Thornkin Warrior — humanoid made of bark, thorn-studded club |
| faction_2_t3.png | Forest Guardian — bark-armoured figure with wooden shield and bone spear |
| faction_2_t4.png | Treant — small tree-creature with claw-like branch arms |
| faction_2_t5.png | Elder Thornkin — druid in root-plate armour, vine whip weapon |
| faction_2_t6.png | Ancient Colossus — massive walking ancient tree, branch weapons, mossy |

### Faction 3 — Eternal Empire (bone white, teal glow, spectral)

| File | Character |
|------|-----------|
| faction_3_t1.png | Skeleton Soldier — bare bone skeleton with rusted sword and cracked shield |
| faction_3_t2.png | Armoured Skeleton — skeleton in teal-glowing bone plate armour |
| faction_3_t3.png | Zombie Warrior — rotting undead in tattered imperial armour |
| faction_3_t4.png | Death Knight — undead knight in ornate bone plate, spectral glow |
| faction_3_t5.png | Lich — robed skeletal sorcerer with spectral blade, hovering slightly |
| faction_3_t6.png | Eternal Emperor — undead emperor in full necromantic regalia and crown |

### Faction 4 — Bloodsworn (dark red, obsidian black, blood runes)

| File | Character |
|------|-----------|
| faction_4_t1.png | Cultist — tattered robes, ritual dagger, blood-marked face |
| faction_4_t2.png | Blood Warrior — obsidian blade, blood-rune war paint on bare chest |
| faction_4_t3.png | Berserker — twin axes, blood-rune etched armour, frenzied stance |
| faction_4_t4.png | Blood Champion — dark obsidian full plate armour, two-handed sword |
| faction_4_t5.png | Oracle — bone staff, sacrificial armour with blood-filled vials |
| faction_4_t6.png | Bloodsworn Avatar — enormous demonic warrior in spiked dark armour |

### Faction 5 — Voidkin (deep purple, black, void energy wisps)

| File | Character |
|------|-----------|
| faction_5_t1.png | Void Sprite — small ethereal wisp-creature with purple glow |
| faction_5_t2.png | Void Scout — dark purple robes, phase blade that crackles with void energy |
| faction_5_t3.png | Void Stalker — shadow cloak, twin void daggers, crouching stance |
| faction_5_t4.png | Void Mage — flowing purple-black robes, void crystal staff |
| faction_5_t5.png | Void Wraith — translucent armoured spectre, semi-transparent body |
| faction_5_t6.png | Void Herald — elaborate dark armour with crystallised void energy wings |

### Faction 6 — Iron Assembly (steel grey, copper, bronze gears)

| File | Character |
|------|-----------|
| faction_6_t1.png | Automaton — small mechanical robot with spinning gear arms, riveted body |
| faction_6_t2.png | Infantry Unit — humanoid chassis with mechanical arm replacement, steel shield |
| faction_6_t3.png | Clockwork Warrior — gear-studded plate armour, piston-driven punch fist |
| faction_6_t4.png | Gunner — integrated brass cannon arm replacing right hand |
| faction_6_t5.png | Steam Colossus — large walker with boiler on back, smoke stacks, cannon fists |
| faction_6_t6.png | Iron Titan — massive quadrupedal war machine, multiple cannons, steam vents |

### Faction 7 — Amalgamate (flesh pink, dark veins, exposed bone)

| File | Character |
|------|-----------|
| faction_7_t1.png | Crawler — grotesque fleshy blob creature with clawed hands |
| faction_7_t2.png | Flesh Warrior — humanoid with bone protrusions through skin, clawed hands |
| faction_7_t3.png | Brute — multiple arms, fanged maw visible in torso, muscled horror |
| faction_7_t4.png | Behemoth — large creature with integrated bone armour, multiple limbs |
| faction_7_t5.png | Flesh Colossus — organic cannon-arm, bone plate growths, towering horror |
| faction_7_t6.png | Apex — massive fused abomination with tendrils, bone spires, multiple eyes |

### Faction 8 — Convergence (silver chrome, mirror-reflective, half-organic)

| File | Character |
|------|-----------|
| faction_8_t1.png | Initiate — human with chrome plating on one arm, seams visible |
| faction_8_t2.png | Soldier — half-organic half-chrome body, silver armour panels |
| faction_8_t3.png | Mirror Warrior — fully reflective shield, phase sword, chrome body |
| faction_8_t4.png | Champion — silver-chrome full body armour, energy sword |
| faction_8_t5.png | Elite — complete chrome plate with energy halberd, sleek design |
| faction_8_t6.png | Convergence Prime — perfect silver fusion of machine and flesh, imposing |

---

## After you get the images

1. Check each image is **512×64px** with transparent background
2. Rename to the filename in the table above
3. Drop into `game/assets/sprites/`
4. Rebuild the game — sprites appear automatically

If ChatGPT gives you a square image instead of 512×64, run this:
```bash
# Trim and arrange into correct strip (adjust paths as needed)
python3 tools/fix_sprite.py input.png faction_0_t1.png
```

---

## Tips for better ChatGPT results

- **Ask for one unit at a time** — quality drops with multiple characters
- **Add "white paper mockup style"** if it keeps adding backgrounds
- **Request "game sprite sheet"** not just "sprite" — it understands the layout better
- If frames bleed together: ask for **"thin black separator lines between frames"** then crop in Python
- DALL-E 3 in ChatGPT Plus handles this format better than DALL-E 2
