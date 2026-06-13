#!/usr/bin/env python3
"""Synthesize medieval atmospheric WAV audio for FIT game.

Outputs:
  game/assets/sounds/worldmap_music.wav  ~18s ambient drone, A-minor choir
  game/assets/sounds/combat_music.wav    ~12s driving brass + percussion
  game/assets/sounds/town_music.wav      ~20s lute/harp melody, C-major
  game/assets/sounds/click.wav           50ms UI click
  game/assets/sounds/pickup.wav          400ms sparkle arpeggio
  game/assets/sounds/hit.wav             200ms impact thud
  game/assets/sounds/spell.wav           500ms magic whoosh
  game/assets/sounds/levelup.wav         900ms ascending fanfare
  game/assets/sounds/victory.wav         1500ms victory brass chord
  game/assets/sounds/buy.wav             250ms coin jingle
"""
import struct, math, os

RATE = 44100


# ── WAV writer ────────────────────────────────────────────────────────────────
def write_wav(path, samples):
    os.makedirs(os.path.dirname(path) or '.', exist_ok=True)
    n = len(samples)
    # 44-byte PCM WAV header
    header = struct.pack('<4sI4s',  b'RIFF', 36 + n * 2, b'WAVE')
    header += struct.pack('<4sIHHIIHH', b'fmt ', 16, 1, 1, RATE, RATE * 2, 2, 16)
    header += struct.pack('<4sI', b'data', n * 2)
    with open(path, 'wb') as f:
        f.write(header)
        f.write(struct.pack(f'<{n}h', *samples))
    print(f"  {path}  ({n/RATE:.2f}s,  {len(header)+n*2} bytes)")


# ── Synthesis helpers ─────────────────────────────────────────────────────────
def clamp(v):
    return max(-1.0, min(1.0, v))

def si(freq, t, phase=0.0):
    return math.sin(2 * math.pi * freq * t + phase)

def adsr(t, dur, a=0.05, d=0.1, s=0.75, r=0.15):
    if t < a:              return t / a
    elif t < a + d:        return 1.0 - (1.0 - s) * (t - a) / d
    elif t < dur - r:      return s
    elif t < dur:          return s * (1.0 - (t - (dur - r)) / r)
    return 0.0

def gen(dur, func, volume=0.85):
    frames = int(RATE * dur)
    out = []
    for i in range(frames):
        v = clamp(func(i / RATE) * volume)
        out.append(int(v * 32767))
    return out

def fade(samples, fade_in=0.0, fade_out=0.0):
    n = len(samples)
    fi = int(RATE * fade_in)
    fo = int(RATE * fade_out)
    out = list(samples)
    for i in range(min(fi, n)):
        out[i] = int(out[i] * (i / fi))
    for i in range(min(fo, n)):
        idx = n - 1 - i
        out[idx] = int(out[idx] * (i / fo))
    return out

def mix(*tracks):
    n = max(len(t) for t in tracks)
    out = []
    for i in range(n):
        v = sum(t[i] / 32767.0 for t in tracks if i < len(t))
        out.append(int(clamp(v / len(tracks) * 1.8) * 32767))
    return out


# ── Instrument voices ─────────────────────────────────────────────────────────

def organ(freq, t, dur, a=0.3, d=0.4, s=0.82, r=0.5):
    env = adsr(t, dur, a, d, s, r)
    # Additive harmonics: fundamental + drawbars
    v  = si(freq,   t) * 1.00
    v += si(freq*2, t) * 0.55
    v += si(freq*3, t) * 0.28
    v += si(freq*4, t) * 0.14
    v += si(freq*6, t) * 0.07
    return v * env * 0.15

def choir(freq, t, dur):
    env = adsr(t, dur, a=1.8, d=0.3, s=0.88, r=1.8)
    v = 0.0
    # Multiple detuned voices simulate choral spread
    for df, amp in [(0, 1.0), (0.004, 0.65), (-0.006, 0.55), (0.009, 0.35)]:
        f = freq * (1 + df)
        v += amp * si(f, t)
        v += amp * 0.25 * si(f * 2, t)
        # Slow vibrato
        vib = 1.0 + 0.003 * si(5.2, t)
        v += amp * 0.12 * si(f * vib, t)
    return v * env * 0.12

def brass(freq, t, dur, a=0.04, d=0.12, s=0.80, r=0.10):
    env = adsr(t, dur, a, d, s, r)
    # Sawtooth approximation: odd + even harmonics
    v  = si(freq,   t) * 1.0
    v += si(freq*2, t) * 0.60
    v += si(freq*3, t) * 0.35
    v += si(freq*4, t) * 0.20
    v += si(freq*5, t) * 0.10
    # Brightness envelope: more harmonics at attack
    brighter = max(0.0, 1.0 - t / max(0.001, a + d))
    v += si(freq*6, t) * 0.08 * brighter
    return v * env * 0.14

def lute(freq, t, dur):
    # Fast attack, exponential decay — plucked string character
    env = math.exp(-t * 4.5) if t >= 0 else 0.0
    v  = si(freq,   t) * 1.0
    v += si(freq*2, t) * 0.45
    v += si(freq*3, t) * 0.20
    v += si(freq*4, t) * 0.08
    return v * env * 0.22

def bass_drum(t, dur):
    # Pitched sine thump with fast pitch fall
    f = 90.0 * math.exp(-t * 22.0) + 35.0
    env = math.exp(-t * 14.0)
    return si(f, t) * env * 0.55

def snare(t, freq_seed=0.0):
    # Noise burst with body tone
    x = math.sin((t * 12345.6 + freq_seed) * 43758.5453123)
    noise = (x - math.floor(x)) * 2.0 - 1.0
    body = si(220, t) * math.exp(-t * 30.0)
    env  = math.exp(-t * 18.0)
    return (noise * 0.7 + body * 0.3) * env * 0.40


# ── Note frequencies ──────────────────────────────────────────────────────────
def note(name):
    notes = {'C':0,'D':2,'E':4,'F':5,'G':7,'A':9,'B':11}
    n = name[0]
    if len(name) == 3:
        acc = name[1]; octave = int(name[2])
        semitone = notes[n] + (1 if acc == '#' else -1)
    else:
        octave = int(name[1]); acc = ''
        semitone = notes[n]
    midi = (octave + 1) * 12 + semitone
    return 440.0 * (2.0 ** ((midi - 69) / 12.0))

# Key notes
A2 = note('A2');  A3 = note('A3');  A4 = note('A4');  A5 = note('A5')
C3 = note('C3');  C4 = note('C4');  C5 = note('C5');  C6 = note('C6')
D4 = note('D4');  D5 = note('D5')
E3 = note('E3');  E4 = note('E4');  E5 = note('E5')
F4 = note('F4')
G3 = note('G3');  G4 = note('G4');  G5 = note('G5')
B4 = note('B4')


# ════════════════════════════════════════════════════════════════════════════════
# WORLD MAP MUSIC  — A-minor ambient choir drone (~18s loop)
# ════════════════════════════════════════════════════════════════════════════════
def gen_worldmap():
    DUR = 18.0
    def func(t):
        # Bass organ drone on A minor: A2, E3
        d1 = organ(A2, t, DUR, a=0.5, r=0.8)
        d2 = organ(E3, t, DUR, a=0.8, r=0.8) * 0.7
        # Choir pads on A minor chord: A3, C4, E4, A4
        c1 = choir(A3, t, DUR)
        c2 = choir(C4, t, DUR) * 0.85
        c3 = choir(E4, t, DUR) * 0.70
        c4 = choir(A4, t, DUR) * 0.50
        # Slow melodic organ phrase every ~6 seconds
        mt = t % 6.0
        mel = 0.0
        # Rising phrase: A3 C4 E4 G4 A4
        if mt < 0.8:
            mel = organ(A3, mt, 0.8, a=0.05, d=0.2, s=0.7, r=0.15)
        elif 1.2 <= mt < 2.0:
            mel = organ(C4, mt-1.2, 0.8, a=0.05, d=0.2, s=0.7, r=0.15)
        elif 2.4 <= mt < 3.2:
            mel = organ(E4, mt-2.4, 0.8, a=0.05, d=0.2, s=0.7, r=0.15)
        elif 3.6 <= mt < 4.6:
            mel = organ(A4, mt-3.6, 1.0, a=0.05, d=0.15, s=0.65, r=0.3)
        v = d1 + d2 + c1 + c2 + c3 + c4 + mel * 0.6
        return clamp(v)
    samples = gen(DUR, func, 0.90)
    return fade(samples, fade_in=0.8, fade_out=1.0)


# ════════════════════════════════════════════════════════════════════════════════
# COMBAT MUSIC  — A-minor brass + drums (~12s loop, 120 BPM)
# ════════════════════════════════════════════════════════════════════════════════
def gen_combat():
    DUR  = 12.0
    BPM  = 120.0
    BEAT = 60.0 / BPM  # 0.5s

    # Melody pattern (A-minor pentatonic): repeating 8-beat phrase
    MEL = [
        (A3, BEAT),  (A3, BEAT*0.5), (C4, BEAT*0.5),
        (E4, BEAT),  (G4, BEAT*0.5), (E4, BEAT*0.5),
        (C4, BEAT),  (A3, BEAT),
    ]
    mel_events = []
    t_acc = 0.0
    for _ in range(3):  # 3 phrase repeats = 12 beats = 6s each phrase x 2
        for freq, dur_n in MEL:
            mel_events.append((t_acc, freq, dur_n * 0.9))
            t_acc += dur_n

    # Low brass counter-melody: A2, E3 on beats 1,3
    bass_events = []
    t_acc = 0.0
    while t_acc < DUR:
        bass_events.append((t_acc, A2, BEAT * 1.8))
        t_acc += BEAT * 2.0
        if t_acc < DUR:
            bass_events.append((t_acc, E3, BEAT * 0.9))
            t_acc += BEAT * 2.0

    def func(t):
        v = 0.0
        # Melody brass
        for t0, freq, dur_n in mel_events:
            dt = t - t0
            if 0 <= dt < dur_n:
                v += brass(freq, dt, dur_n)
        # Bass brass
        for t0, freq, dur_n in bass_events:
            dt = t - t0
            if 0 <= dt < dur_n:
                v += brass(freq, dt, dur_n, a=0.02, d=0.15, s=0.75, r=0.12) * 0.8
        # Percussion: bass drum every beat
        beat_t = t % BEAT
        v += bass_drum(beat_t, BEAT)
        # Snare on beats 2, 4 (offset 0.5)
        snare_t = (t + BEAT) % (BEAT * 2)
        if snare_t < 0.25:
            v += snare(snare_t, t)
        return clamp(v)

    samples = gen(DUR, func, 0.88)
    return fade(samples, fade_in=0.3, fade_out=0.6)


# ════════════════════════════════════════════════════════════════════════════════
# TOWN MUSIC  — C-major lute melody (~20s loop)
# ════════════════════════════════════════════════════════════════════════════════
def gen_town():
    DUR = 20.0
    NOTE_DUR = 0.42

    # C-major melody: two phrases of 8 notes each, repeated
    PHRASE_A = [C4, E4, G4, A4, G4, E4, D4, C4]
    PHRASE_B = [E4, G4, A4, C5, A4, G4, E4, D4]
    MELODY   = (PHRASE_A + PHRASE_B) * 3   # 48 notes

    mel_events = []
    t_acc = 0.0
    for freq in MELODY:
        if t_acc >= DUR:
            break
        mel_events.append((t_acc, freq, NOTE_DUR))
        t_acc += NOTE_DUR

    # Simple bass: root notes on beats
    BASS = [C3, C3, G3, G3, A2, A2, G3, G3]
    BASS_DUR = NOTE_DUR * 2
    bass_events = []
    t_acc = 0.0
    bi = 0
    while t_acc < DUR:
        bass_events.append((t_acc, BASS[bi % len(BASS)], BASS_DUR * 0.85))
        t_acc += BASS_DUR
        bi += 1

    # Harmonic accompaniment: C E G held chords
    def bg_chord(t):
        phase = t / 4.0  # 4-second chord cycles
        if phase % 2 < 1.0:
            freqs = [C4, E4, G4]
        else:
            freqs = [G3, B4, D4]
        v = 0.0
        for f in freqs:
            v += organ(f, t % 4.0, 3.8, a=0.4, d=0.3, s=0.6, r=0.4) * 0.35
        return v

    def func(t):
        v = bg_chord(t)
        # Lute melody
        for t0, freq, dur_n in mel_events:
            dt = t - t0
            if 0 <= dt < dur_n:
                v += lute(freq, dt, dur_n)
        # Bass lute
        for t0, freq, dur_n in bass_events:
            dt = t - t0
            if 0 <= dt < dur_n:
                v += lute(freq, dt, dur_n) * 0.70
        return clamp(v)

    samples = gen(DUR, func, 0.88)
    return fade(samples, fade_in=0.5, fade_out=0.8)


# ════════════════════════════════════════════════════════════════════════════════
# SFX
# ════════════════════════════════════════════════════════════════════════════════

def gen_click():
    DUR = 0.055
    def func(t):
        env = math.exp(-t * 55.0)
        return si(820, t) * env * 0.6 + si(1200, t) * env * 0.3
    return gen(DUR, func, 0.85)

def gen_pickup():
    # Ascending sparkle arpeggio: C5-E5-G5-C6
    NOTES = [(C5, 0.0), (E5, 0.10), (G5, 0.20), (C6, 0.32)]
    DUR = 0.50
    def func(t):
        v = 0.0
        for freq, t0 in NOTES:
            dt = t - t0
            if dt >= 0:
                env = math.exp(-dt * 9.0)
                v += (si(freq, dt) * 0.7 + si(freq*2, dt) * 0.3) * env
        return clamp(v * 0.50)
    return gen(DUR, func)

def gen_hit():
    DUR = 0.22
    def func(t):
        # Low thud + brief noise
        thud = si(70, t) * math.exp(-t * 22.0) * 0.65
        x = math.sin(t * 98765.432) * 43758.5453
        noise_v = ((x - math.floor(x)) * 2.0 - 1.0) * math.exp(-t * 35.0) * 0.30
        return clamp(thud + noise_v)
    return gen(DUR, func)

def gen_spell():
    DUR = 0.55
    def func(t):
        ratio = t / DUR
        # Frequency rises from 180 Hz to 920 Hz
        freq = 180.0 + 740.0 * ratio * ratio
        env  = math.sin(math.pi * t / DUR)
        # Bright multi-harmonic
        v  = si(freq,   t) * 0.6
        v += si(freq*2, t) * 0.25
        v += si(freq*3, t) * 0.12
        # Shimmer
        v += si(freq * 1.5, t + 0.3) * 0.15 * env
        return clamp(v * env * 0.80)
    return gen(DUR, func)

def gen_levelup():
    # Ascending arpeggio C5-E5-G5-C6 with held final note
    NOTES = [(C5, 0.0, 0.18), (E5, 0.18, 0.18), (G5, 0.36, 0.18), (C6, 0.54, 0.38)]
    DUR = 0.95
    def func(t):
        v = 0.0
        for freq, t0, dur_n in NOTES:
            dt = t - t0
            if dt >= 0:
                env = adsr(dt, dur_n, a=0.02, d=0.08, s=0.75, r=0.10)
                v += (si(freq, dt) + si(freq*2, dt)*0.4 + si(freq*3, dt)*0.15) * env
        return clamp(v * 0.30)
    return gen(DUR, func)

def gen_victory():
    # C-major fanfare: C5-E5-G5 → G5-E5-C6 climb, then full chord
    NOTES = [
        (C5, 0.00, 0.20), (E5, 0.20, 0.20), (G5, 0.40, 0.20),
        (G5, 0.60, 0.15), (E5, 0.75, 0.15), (C6, 0.90, 0.60),
        # Final chord
        (C5, 0.90, 0.55), (E5, 0.90, 0.55), (G5, 0.90, 0.55),
    ]
    DUR = 1.60
    def func(t):
        v = 0.0
        for freq, t0, dur_n in NOTES:
            dt = t - t0
            if dt >= 0:
                env = adsr(dt, dur_n, a=0.03, d=0.10, s=0.80, r=0.15)
                v += brass(freq, dt, dur_n) * 1.4
        return clamp(v)
    samples = gen(DUR, func, 0.82)
    return fade(samples, fade_out=0.5)

def gen_buy():
    # Two-note coin jingle: G5-C6
    NOTES = [(G5, 0.00, 0.12), (C6, 0.13, 0.14)]
    DUR = 0.30
    def func(t):
        v = 0.0
        for freq, t0, dur_n in NOTES:
            dt = t - t0
            if dt >= 0:
                env = math.exp(-dt * 15.0)
                v += (si(freq, dt) * 0.6 + si(freq*2, dt) * 0.35 + si(freq*3, dt) * 0.12) * env
        return clamp(v * 0.65)
    return gen(DUR, func)


# ── Main ──────────────────────────────────────────────────────────────────────
DIRS = ["game/assets/sounds", "build/assets/sounds"]

def save(name, samples):
    for d in DIRS:
        write_wav(f"{d}/{name}.wav", samples)

if __name__ == "__main__":
    print("Synthesizing audio …")
    print("  [music] world map …");  save("worldmap_music", gen_worldmap())
    print("  [music] combat …");     save("combat_music",   gen_combat())
    print("  [music] town …");       save("town_music",     gen_town())
    print("  [sfx]   click …");      save("click",          gen_click())
    print("  [sfx]   pickup …");     save("pickup",         gen_pickup())
    print("  [sfx]   hit …");        save("hit",            gen_hit())
    print("  [sfx]   spell …");      save("spell",          gen_spell())
    print("  [sfx]   levelup …");    save("levelup",        gen_levelup())
    print("  [sfx]   victory …");    save("victory",        gen_victory())
    print("  [sfx]   buy …");        save("buy",            gen_buy())
    print("Done.")
