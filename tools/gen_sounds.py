#!/usr/bin/env python3
"""Generate WAV sound effects and music loops for the strategy game."""

import wave, struct, math, os, random

RATE = 22050

def write_wav(path, samples, rate=RATE):
    os.makedirs(os.path.dirname(path) or '.', exist_ok=True)
    with wave.open(path, 'w') as wf:
        wf.setnchannels(1)
        wf.setsampwidth(2)
        wf.setframerate(rate)
        wf.writeframes(struct.pack(f'<{len(samples)}h', *samples))
    print(f"Written {path} ({len(samples)} samples, {len(samples)/rate:.2f}s)")

def clamp(v): return max(-32767, min(32767, int(v)))

# click.wav: 0.08s, 800Hz sine, fade out
def make_click():
    n = int(RATE * 0.08)
    return [clamp(16000 * math.sin(2*math.pi*800*i/RATE) * (1 - i/n)) for i in range(n)]

# pickup.wav: 0.2s, ascending 600->1200Hz
def make_pickup():
    n = int(RATE * 0.2)
    return [clamp(20000 * math.sin(2*math.pi*(600 + 600*i/n)*i/RATE) * (1 - i/n * 0.5)) for i in range(n)]

# levelup.wav: 0.5s, C-E-G chord, fade out
def make_levelup():
    n = int(RATE * 0.5)
    return [clamp(8000 * (math.sin(2*math.pi*523*i/RATE)
                        + math.sin(2*math.pi*659*i/RATE)
                        + math.sin(2*math.pi*784*i/RATE)) * (1 - i/n)**0.5) for i in range(n)]

# hit.wav: 0.15s, white noise, exponential decay
def make_hit():
    n = int(RATE * 0.15)
    rng = random.Random(42)
    return [clamp(30000 * (rng.random() * 2 - 1) * math.exp(-8 * i/n)) for i in range(n)]

# spell.wav: 0.4s, sweeping sine 300->900Hz with vibrato
def make_spell():
    n = int(RATE * 0.4)
    out = []
    for i in range(n):
        t = i / RATE
        freq = 300 + 600 * i/n
        vibrato = 1 + 0.05 * math.sin(2*math.pi*8*t)
        out.append(clamp(20000 * math.sin(2*math.pi*freq*vibrato*t) * (1 - i/n * 0.8)))
    return out

# buy.wav: 0.15s, two short tones
def make_buy():
    n = int(RATE * 0.075)
    t1 = [clamp(20000 * math.sin(2*math.pi*600*i/RATE) * (1 - i/n)) for i in range(n)]
    t2 = [clamp(20000 * math.sin(2*math.pi*900*i/RATE) * (1 - i/n)) for i in range(n)]
    return t1 + t2

# worldmap_music.wav: 8s, ambient drone with tremolo
def make_worldmap_music():
    n = int(RATE * 8)
    out = []
    for i in range(n):
        t = i / RATE
        tremolo = 0.7 + 0.3 * math.sin(2*math.pi*0.5*t)
        v = (0.4 * math.sin(2*math.pi*110*t)
           + 0.3 * math.sin(2*math.pi*165*t)
           + 0.2 * math.sin(2*math.pi*220*t))
        out.append(clamp(12000 * v * tremolo))
    return out

# combat_music.wav: 6s, tense pulse at 4Hz
def make_combat_music():
    n = int(RATE * 6)
    rng = random.Random(99)
    out = []
    for i in range(n):
        t = i / RATE
        pulse = max(0, math.sin(2*math.pi*4*t)) ** 2
        noise = rng.random() * 2 - 1
        out.append(clamp(20000 * noise * pulse))
    return out

# town_music.wav: 6s, peaceful C-major chord with slow attack/release
def make_town_music():
    n = int(RATE * 6)
    out = []
    for i in range(n):
        t = i / RATE
        # Slow attack first second, hold, release last second
        env = min(t, 1.0) * min(1.0, 6.0-t)
        v = (math.sin(2*math.pi*261*t)
           + math.sin(2*math.pi*329*t)
           + math.sin(2*math.pi*392*t)) / 3
        out.append(clamp(15000 * v * env))
    return out

def write_both(name, samples):
    write_wav(f'build/assets/sounds/{name}', samples)
    write_wav(f'game/assets/sounds/{name}', samples)

write_both('click.wav',          make_click())
write_both('pickup.wav',         make_pickup())
write_both('levelup.wav',        make_levelup())
write_both('hit.wav',            make_hit())
write_both('spell.wav',          make_spell())
write_both('buy.wav',            make_buy())
write_both('worldmap_music.wav', make_worldmap_music())
write_both('combat_music.wav',   make_combat_music())
write_both('town_music.wav',     make_town_music())
