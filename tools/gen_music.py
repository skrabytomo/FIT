#!/usr/bin/env python3
"""Generate orchestral faction town themes + world/combat music.

Pipeline: Python MIDI → FluidSynth + FluidR3_GM.sf2 (141 MB) → WAV
Each faction has a unique mode, tempo, and instrument palette.
Outputs: faction_music_{0-8}.wav, worldmap_music.wav, combat_music.wav
"""
import mido, subprocess, os

SF2     = "/usr/share/sounds/sf2/FluidR3_GM.sf2"
TPB     = 480
OUTDIRS = ["game/assets/sounds", "build/assets/sounds"]

# ── GM programs ───────────────────────────────────────────────────────────────
CHURCH_ORG=19; STR1=48; STR2=49; CHOIR=52; OOHS=53; TREMOLO=44; PIZZ=45
TRUMPET=56; TROMBONE=57; TUBA=58; FRENCH_HORN=60; BRASS=61
FLUTE=73; OBOE=68; BASSOON=70; CLARINET=71; HARP=46
CELLO=42; VIOLIN=40; ENG_HORN=69

# ── Percussion (ch 9) ─────────────────────────────────────────────────────────
KICK=36; SNARE=38; HIHAT=42; TOM_H=48; TOM_M=45; CRASH=49

# ── MIDI note numbers ─────────────────────────────────────────────────────────
C2=36; Cs2=37; D2=38; Eb2=39; E2=40; F2=41; Fs2=42; G2=43; Ab2=44; A2=45; Bb2=46; B2=47
C3=48; Cs3=49; D3=50; Eb3=51; E3=52; F3=53; Fs3=54; G3=55
Gs3=56; Ab3=56; A3=57; Bb3=58; Ds3=51; B3=59
C4=60; Cs4=61; D4=62; Eb4=63; E4=64; F4=65; Fs4=66; G4=67
Ab4=68; A4=69; Bb4=70; B4=71
C5=72; Cs5=73; D5=74; Eb5=75; E5=76; F5=77; Fs5=78; G5=79
Ab5=80; A5=81; Bb5=82; B5=83
C6=84; Cs6=85; D6=86; E6=88; Fs6=90

R = None

def tp(bpm): return int(60_000_000 / bpm)
def T(b):   return int(b * TPB)


# ── Melodic track ─────────────────────────────────────────────────────────────
class Tr:
    def __init__(self, prog, ch, t_us, vol=100, rev=50, cho=8):
        self.ch = ch; self.ofs = 0
        self.t  = mido.MidiTrack()
        self.t.append(mido.MetaMessage('set_tempo', tempo=t_us, time=0))
        if ch != 9:
            self.t.append(mido.Message('program_change', channel=ch, program=prog, time=0))
        for cc, v in [(7,vol),(11,110),(91,rev),(93,cho)]:
            self.t.append(mido.Message('control_change', channel=ch, control=cc, value=v, time=0))

    def note(self, p, b, v=75):
        self.t.append(mido.Message('note_on',  channel=self.ch, note=p, velocity=v, time=self.ofs))
        self.t.append(mido.Message('note_off', channel=self.ch, note=p, velocity=0, time=T(b)))
        self.ofs = 0

    def rest(self, b): self.ofs += T(b)

    def chord(self, ps, b, v=60):
        self.t.append(mido.Message('note_on', channel=self.ch, note=ps[0], velocity=v, time=self.ofs))
        for p in ps[1:]:
            self.t.append(mido.Message('note_on', channel=self.ch, note=p, velocity=v, time=0))
        self.ofs = 0
        self.t.append(mido.Message('note_off', channel=self.ch, note=ps[0], velocity=0, time=T(b)))
        for p in ps[1:]:
            self.t.append(mido.Message('note_off', channel=self.ch, note=p, velocity=0, time=0))

    def seq(self, ev, reps=1):
        for _ in range(reps):
            for p, b, v in ev:
                if p is None: self.rest(b)
                else:         self.note(p, b, v)

    def cseq(self, ev, reps=1):
        for _ in range(reps):
            for ps, b, v in ev:
                if ps is None: self.rest(b)
                else:          self.chord(ps, b, v)


# ── Drum track ────────────────────────────────────────────────────────────────
class DrumTr:
    HIT = int(0.065 * TPB)
    def __init__(self, t_us):
        self.t = mido.MidiTrack()
        self.t.append(mido.MetaMessage('set_tempo', tempo=t_us, time=0))
        self._e = []
    def hit(self, note, beat, vel=85):
        at = int(beat * TPB)
        self._e += [(at,'on',note,vel),(at+self.HIT,'off',note,0)]
    def bar(self, pat, idx, bb=4):
        for b,n,v in pat: self.hit(n, idx*bb+b, v)
    def fill(self, pat, n):
        for i in range(n): self.bar(pat, i)
    def build(self):
        self._e.sort(key=lambda e:(e[0],0 if e[1]=='off' else 1))
        prev = 0
        for at,typ,note,vel in self._e:
            self.t.append(mido.Message('note_on' if typ=='on' else 'note_off',
                                       channel=9,note=note,velocity=vel,time=at-prev))
            prev = at
        return self.t


# ── Drum patterns ─────────────────────────────────────────────────────────────
MARCH  = [(0,KICK,90),(0,HIHAT,60),(0.5,HIHAT,52),(1,SNARE,82),(1,HIHAT,60),
           (1.5,HIHAT,52),(2,KICK,88),(2,HIHAT,60),(2.5,HIHAT,52),(3,SNARE,82),
           (3,HIHAT,60),(3.5,HIHAT,52)]
BATTLE = [(0,KICK,100),(0,HIHAT,65),(0.5,KICK,82),(0.5,HIHAT,55),(1,SNARE,96),
           (1,HIHAT,65),(1.5,HIHAT,55),(2,KICK,100),(2,HIHAT,65),(2.5,KICK,78),
           (3,SNARE,96),(3,HIHAT,65),(3.5,HIHAT,55)]
FOLK   = [(0,KICK,72),(0.5,TOM_H,55),(1,TOM_H,62),(1.5,TOM_H,50),
           (2,KICK,68),(2.5,TOM_H,50),(3,TOM_H,60),(3.5,TOM_H,48)]
MECH   = [(0,KICK,98),(0,HIHAT,70),(0.25,HIHAT,58),(0.5,KICK,80),(0.75,HIHAT,58),
           (1,SNARE,92),(1,HIHAT,68),(1.5,HIHAT,58),(2,KICK,98),(2,HIHAT,70),
           (2.25,HIHAT,58),(2.5,KICK,78),(3,SNARE,92),(3,HIHAT,68),(3.5,HIHAT,58)]
DEMON  = [(0,KICK,100),(0.25,KICK,85),(0.5,HIHAT,65),(0.75,HIHAT,58),
           (1,SNARE,98),(1,HIHAT,65),(1.5,KICK,85),(1.75,HIHAT,55),
           (2,KICK,100),(2.25,KICK,82),(2.5,HIHAT,65),(3,SNARE,98),
           (3,HIHAT,65),(3.5,KICK,82),(3.75,HIHAT,55)]


# ── Render ────────────────────────────────────────────────────────────────────
def render(tracks, name, room=0.6, gain=0.55):
    mid = mido.MidiFile(type=1, ticks_per_beat=TPB)
    for tr in tracks: mid.tracks.append(tr.t)
    tmp = "/tmp/_fit_midi.mid"; mid.save(tmp)
    for d in OUTDIRS:
        os.makedirs(d, exist_ok=True)
        path = f"{d}/{name}"
        subprocess.run(['fluidsynth','-ni',
            '-o',f'synth.reverb.room-size={room}','-o','synth.reverb.level=0.85',
            '-o','synth.reverb.width=9.0','-o',f'synth.gain={gain}',
            SF2,'-F',path,'-r','44100',tmp], check=True, capture_output=True)
    print(f"  {name:<44s}  {mid.length:.1f}s")


# ════════════════════════════════════════════════════════════════════════════════
# 0 — HOLY ORDER  (C Major / Mixolydian, 72 BPM)
# Cathedral grandeur: trumpet fanfare, string pads, choir, cello bass
# 16 bars ≈ 53s
# ════════════════════════════════════════════════════════════════════════════════
def holy_order():
    t = tp(72)
    mel = Tr(TRUMPET, 0, t, vol=92, rev=42, cho=5)
    mel.seq([
        (C5,1,80),(E5,1,80),(G5,1,82),(A5,1,82),  # b1
        (G5,2,76),(E5,2,74),                        # b2
        (F5,1,78),(E5,1,76),(D5,1,74),(C5,1,74),  # b3
        (G4,3,70),(R,1,0),                          # b4
        (E5,1,80),(G5,1,82),(A5,1,84),(C6,1,86),  # b5
        (B5,2,80),(G5,2,78),                        # b6
        (A5,1,76),(G5,1,76),(F5,1,74),(E5,1,72),  # b7
        (C5,4,70),                                  # b8
    ], reps=2)
    pad = Tr(STR1, 1, t, vol=72, rev=68, cho=14)
    pad.cseq([
        ([C3,E3,G3],4,55),([C3,E3,G3],4,55),
        ([F2,A2,C3],4,52),([G2,B2,D3],4,52),
        ([A2,C3,E3],4,52),([G2,B2,D3],4,52),
        ([F2,A2,C3],4,52),([C3,E3,G3],4,55),
    ], reps=2)
    cho = Tr(CHOIR, 2, t, vol=62, rev=80, cho=16)
    cho.seq([
        (G4,2,42),(A4,2,42),(G4,4,40),(A4,4,40),(B4,4,40),
        (C5,2,42),(B4,2,40),(G4,4,40),(A4,2,40),(G4,2,38),(E4,4,40),
    ], reps=2)
    bas = Tr(CELLO, 3, t, vol=88, rev=45)
    bas.seq([
        (C3,2,65),(C3,2,62),(C3,2,62),(G2,2,60),(F2,4,60),(G2,4,60),
        (A2,4,60),(G2,4,60),(F2,4,58),(C3,4,65),
    ], reps=2)
    render([mel,pad,cho,bas], "faction_music_0.wav", room=0.55)


# ════════════════════════════════════════════════════════════════════════════════
# 1 — CRIMSON WARDENS  (D Dorian, 108 BPM)
# War fortress: trombone riff, brass power chords, tuba, battle drums
# 24 bars ≈ 53s
# ════════════════════════════════════════════════════════════════════════════════
def crimson_wardens():
    t = tp(108)
    mel = Tr(TROMBONE, 0, t, vol=100, rev=35, cho=3)
    mel.seq([
        (D4,1,90),(R,0.5,0),(D4,0.5,88),(F4,1,86),(A4,1,86),  # b1
        (D5,2,88),(C5,1,84),(A4,1,82),                          # b2
        (G4,1,82),(A4,1,82),(Bb4,1,84),(A4,1,82),              # b3
        (D4,4,76),                                               # b4
        (F4,1,88),(G4,1,88),(A4,1,88),(C5,1,90),              # b5
        (D5,2,88),(C5,1,84),(A4,1,80),                          # b6
        (Bb4,1,80),(A4,1,80),(G4,1,78),(F4,1,76),             # b7
        (D4,4,72),                                               # b8
    ], reps=3)
    pad = Tr(BRASS, 1, t, vol=78, rev=38, cho=4)
    pad.cseq([
        ([D2,A2,D3],4,70),([D2,A2,D3],4,68),
        ([G2,D3,G3],4,66),([A2,E3,A3],4,68),
        ([F2,C3,F3],4,66),([G2,D3,G3],4,66),
        ([Bb2,F3,Bb3],4,66),([A2,E3,A3],4,68),
    ], reps=3)
    bas = Tr(TUBA, 2, t, vol=95, rev=35)
    bas.seq([
        (D2,1,82),(R,1,0),(D2,1,82),(D2,1,80),
        (A2,4,75),(G2,1,78),(R,1,0),(G2,1,78),(G2,1,76),
        (D2,4,78),(F2,1,78),(R,1,0),(F2,1,78),(F2,1,76),
        (G2,4,75),(Bb2,4,72),(A2,4,75),
    ], reps=3)
    dr = DrumTr(t); dr.fill(BATTLE,24); dr.build()
    render([mel,pad,bas,dr], "faction_music_1.wav", room=0.40, gain=0.50)


# ════════════════════════════════════════════════════════════════════════════════
# 2 — THORNKIN  (A Dorian / Celtic, 68 BPM)
# Forest spirits: flute, harp arpeggios, soft strings, light drums
# 16 bars ≈ 56s
# ════════════════════════════════════════════════════════════════════════════════
def thornkin():
    t = tp(68)
    mel = Tr(FLUTE, 0, t, vol=90, rev=58, cho=10)
    mel.seq([
        (A4,1,78),(B4,1,78),(C5,1,76),(D5,1,78),   # b1 A Dorian ascent
        (E5,2,80),(D5,1,76),(C5,1,74),               # b2
        (B4,1,74),(C5,1,76),(D5,1,78),(E5,1,80),   # b3
        (A5,4,78),                                   # b4 peak
        (Fs5,1,76),(E5,1,76),(D5,1,74),(C5,1,72),  # b5 F# = Dorian color
        (B4,2,72),(A4,2,70),                         # b6
        (G4,1,68),(A4,1,70),(B4,1,72),(C5,1,74),   # b7 closing ascent
        (A4,4,68),                                   # b8 resolution
    ], reps=2)
    hrp = Tr(HARP, 1, t, vol=70, rev=62, cho=8)
    hrp.seq([
        # Am arpeggio × 2 per bar (each arpeg = 4 × 0.5 = 2 beats)
        (A3,0.5,60),(C4,0.5,58),(E4,0.5,56),(A4,0.5,58),
        (A3,0.5,56),(C4,0.5,54),(E4,0.5,52),(A4,0.5,54),  # b1 Am
        (A3,0.5,58),(C4,0.5,56),(E4,0.5,54),(A4,0.5,56),
        (A3,0.5,54),(C4,0.5,52),(E4,0.5,50),(A4,0.5,52),  # b2 Am soft
        (D3,0.5,60),(F3,0.5,58),(A3,0.5,56),(D4,0.5,58),
        (D3,0.5,56),(F3,0.5,54),(A3,0.5,52),(D4,0.5,54),  # b3 Dm
        (E3,0.5,60),(G3,0.5,58),(B3,0.5,56),(E4,0.5,58),
        (E3,0.5,56),(G3,0.5,54),(B3,0.5,52),(E4,0.5,54),  # b4 Em
        (A3,0.5,62),(C4,0.5,60),(E4,0.5,58),(A4,0.5,60),
        (A3,0.5,58),(C4,0.5,56),(E4,0.5,54),(A4,0.5,56),  # b5 Am
        (E3,0.5,58),(G3,0.5,56),(B3,0.5,54),(E4,0.5,56),
        (E3,0.5,54),(G3,0.5,52),(B3,0.5,50),(E4,0.5,52),  # b6 Em
        (D3,0.5,60),(F3,0.5,58),(A3,0.5,56),(D4,0.5,58),
        (D3,0.5,56),(F3,0.5,54),(A3,0.5,52),(D4,0.5,54),  # b7 Dm
        (A3,0.5,62),(C4,0.5,60),(E4,0.5,58),(A4,0.5,60),
        (A3,0.5,60),(C4,0.5,58),(E4,0.5,56),(A4,0.5,58),  # b8 Am strong
    ], reps=2)
    pad = Tr(STR1, 2, t, vol=58, rev=72, cho=12)
    pad.cseq([
        ([A2,C3,E3],4,48),([A2,C3,E3],4,48),
        ([D2,F2,A2],4,46),([E2,G2,B2],4,46),
        ([A2,C3,E3],4,48),([E2,G2,B2],4,46),
        ([D2,F2,A2],4,46),([A2,C3,E3],4,48),
    ], reps=2)
    bas = Tr(CELLO, 3, t, vol=82, rev=50)
    bas.seq([
        (A2,4,62),(A2,4,60),(D2,4,58),(E2,4,60),
        (A2,4,62),(E2,4,58),(D2,4,56),(A2,4,62),
    ], reps=2)
    dr = DrumTr(t); dr.fill(FOLK,16); dr.build()
    render([mel,hrp,pad,bas,dr], "faction_music_2.wav", room=0.65)


# ════════════════════════════════════════════════════════════════════════════════
# 3 — ETERNAL EMPIRE  (F# minor, 60 BPM)
# Arcane tower: tremolo strings, voice oohs, harp, dark organ
# 16 bars ≈ 64s
# ════════════════════════════════════════════════════════════════════════════════
def eternal_empire():
    t = tp(60)
    mel = Tr(TREMOLO, 0, t, vol=85, rev=72, cho=18)
    mel.seq([                          # F# minor, chromatic, slow
        (Fs5,2,72),(E5,2,70),   # b1
        (D5,2,68),(Cs5,2,66),   # b2
        (B4,4,70),               # b3
        (R,4,0),                 # b4 silence
        (Fs4,1,75),(Ab4,1,74),(A4,1,72),(B4,1,74),  # b5
        (Cs5,2,76),(D5,2,74),   # b6
        (E5,2,74),(Fs5,2,76),   # b7
        (Ab5,4,72),              # b8 peak (Ab = G#)
    ], reps=2)
    cho = Tr(OOHS, 1, t, vol=65, rev=82, cho=20)
    cho.seq([
        (Fs4,4,48),(E4,4,46),(D4,4,44),(Cs4,4,44),
        (B3,4,46),(Cs4,4,46),(D4,4,44),(Fs4,4,48),
    ], reps=2)
    hrp = Tr(HARP, 2, t, vol=62, rev=65, cho=10)
    hrp.seq([
        (Fs3,0.5,55),(A3,0.5,53),(Cs4,0.5,52),(Fs4,0.5,53),
        (Fs3,0.5,52),(A3,0.5,50),(Cs4,0.5,48),(Fs4,0.5,50),  # b1 F#m
        (E3,0.5,55),(G3,0.5,53),(B3,0.5,52),(E4,0.5,53),
        (E3,0.5,52),(G3,0.5,50),(B3,0.5,48),(E4,0.5,50),      # b2 Em
        (D3,0.5,55),(Fs3,0.5,53),(A3,0.5,52),(D4,0.5,53),
        (D3,0.5,52),(Fs3,0.5,50),(A3,0.5,48),(D4,0.5,50),     # b3 D
        (Cs3,0.5,55),(E3,0.5,53),(Ab3,0.5,52),(Cs4,0.5,53),
        (Cs3,0.5,52),(E3,0.5,50),(Ab3,0.5,48),(Cs4,0.5,50),   # b4 C#m
        (Fs3,0.5,58),(A3,0.5,56),(Cs4,0.5,55),(Fs4,0.5,56),
        (Fs3,0.5,55),(A3,0.5,53),(Cs4,0.5,52),(Fs4,0.5,53),  # b5 F#m
        (B2,0.5,55),(D3,0.5,53),(Fs3,0.5,52),(B3,0.5,53),
        (B2,0.5,52),(D3,0.5,50),(Fs3,0.5,48),(B3,0.5,50),     # b6 Bm
        (E3,0.5,55),(Ab3,0.5,53),(B3,0.5,52),(E4,0.5,53),
        (E3,0.5,52),(Ab3,0.5,50),(B3,0.5,48),(E4,0.5,50),     # b7 E (V chord)
        (Fs3,0.5,60),(A3,0.5,58),(Cs4,0.5,56),(Fs4,0.5,58),
        (Fs3,0.5,58),(A3,0.5,56),(Cs4,0.5,54),(Fs4,0.5,56),  # b8 F#m resolution
    ], reps=2)
    org = Tr(CHURCH_ORG, 3, t, vol=52, rev=75, cho=5)
    org.cseq([
        ([Fs2,A2,Cs3],4,45),([E2,G2,B2],4,43),
        ([D2,Fs2,A2],4,42),([Cs2,E2,Ab2],4,42),
        ([Fs2,A2,Cs3],4,45),([B2,D3,Fs3],4,43),
        ([E2,Ab2,B2],4,43), ([Fs2,A2,Cs3],4,45),
    ], reps=2)
    render([mel,cho,hrp,org], "faction_music_3.wav", room=0.72, gain=0.52)


# ════════════════════════════════════════════════════════════════════════════════
# 4 — BLOODSWORN  (B Phrygian, 128 BPM)
# Hellish assault: low brass, demon choir, double-kick drums
# 24 bars ≈ 45s
# ════════════════════════════════════════════════════════════════════════════════
def bloodsworn():
    t = tp(128)
    mel = Tr(TROMBONE, 0, t, vol=100, rev=32, cho=3)
    mel.seq([
        (B4,1,92),(C5,1,90),(D5,1,88),(B4,1,88),  # b1 Phrygian b2 tension
        (E5,2,90),(D5,1,86),(C5,1,84),              # b2
        (G4,1,82),(A4,1,82),(B4,1,84),(D5,1,86),  # b3
        (C5,2,84),(B4,2,80),                        # b4
        (B4,1,90),(G4,1,88),(A4,1,88),(B4,1,90),  # b5
        (C5,2,92),(B4,2,88),                        # b6
        (A4,1,84),(G4,1,84),(Fs4,1,82),(E4,1,82), # b7
        (B3,4,78),                                  # b8 deep resolution
    ], reps=3)
    pad = Tr(BRASS, 1, t, vol=82, rev=36, cho=4)
    pad.cseq([
        ([B2,F3,B3],4,72),([C3,G3,C4],4,70),
        ([D3,A3,D4],4,68),([E3,B3,E4],4,68),
        ([G2,D3,G3],4,68),([A2,E3,A3],4,70),
        ([G2,D3,G3],4,68),([B2,F3,B3],4,72),
    ], reps=3)
    bas = Tr(TUBA, 2, t, vol=98, rev=32)
    bas.seq([
        (B2,0.5,88),(R,0.5,0),(B2,0.5,85),(R,0.5,0),(B2,1,85),(B2,1,82),
        (E2,4,80),(D2,4,78),(G2,4,78),(B2,4,82),
        (B2,0.5,88),(R,0.5,0),(B2,0.5,85),(R,0.5,0),(B2,1,85),(C3,1,84),
        (C3,4,80),(A2,4,78),(G2,4,78),(B2,4,82),
    ], reps=3)
    cho = Tr(CHOIR, 3, t, vol=60, rev=78, cho=12)
    cho.seq([
        (B3,4,48),(C4,4,46),(D4,4,44),(B3,4,44),
        (G3,4,46),(A3,4,46),(G3,4,44),(B3,4,48),
    ], reps=3)
    dr = DrumTr(t); dr.fill(DEMON,24); dr.build()
    render([mel,pad,bas,cho,dr], "faction_music_4.wav", room=0.38, gain=0.48)


# ════════════════════════════════════════════════════════════════════════════════
# 5 — VOIDKIN  (A natural minor, 50 BPM)
# Spectral necropolis: hollow choir, sparse pizzicato, dark organ
# 12 bars ≈ 58s
# ════════════════════════════════════════════════════════════════════════════════
def voidkin():
    t = tp(50)
    cho = Tr(OOHS, 0, t, vol=70, rev=88, cho=22)
    cho.seq([          # Very slow, haunting — lots of rests for eeriness
        (A4,4,52),(R,4,0),
        (G4,4,48),(R,2,0),(F4,2,46),
        (E4,4,50),(R,4,0),
        (C4,4,46),(A3,4,48),
        (E4,4,50),(D4,4,48),
        (C4,4,46),(R,4,0),
    ])
    piz = Tr(PIZZ, 1, t, vol=78, rev=62, cho=5)
    piz.seq([          # Sparse plucked punctuation
        (A3,0.5,68),(R,1.5,0),(E4,0.5,62),(R,1.5,0),
        (R,4,0),
        (G3,0.5,64),(R,1.5,0),(D4,0.5,60),(R,1.5,0),
        (E3,0.5,68),(R,3.5,0),
        (A3,0.5,68),(R,0.5,0),(A3,0.5,65),(R,2.5,0),
        (R,4,0),
        (C4,0.5,64),(R,1.5,0),(E4,0.5,60),(R,1.5,0),
        (A3,2,60),(R,2,0),
        (E3,0.5,65),(R,1,0),(B3,0.5,60),(R,2,0),
        (D4,1,62),(R,3,0),
        (C3,0.5,68),(R,1.5,0),(G3,0.5,62),(R,1.5,0),
        (A2,4,72),
    ])
    org = Tr(CHURCH_ORG, 2, t, vol=45, rev=85, cho=8)
    org.cseq([
        ([A2,C3,E3],4,38),(R,4,0),
        ([G2,Bb2,D3],4,36),(R,4,0),
        ([A2,C3,E3],4,38),(R,4,0),
        ([E2,G2,B2],4,36),(R,4,0),
        ([A2,C3,E3],4,38),(R,4,0),
        ([D2,F2,A2],4,36),(R,4,0),
    ])
    render([cho,piz,org], "faction_music_5.wav", room=0.85, gain=0.48)


# ════════════════════════════════════════════════════════════════════════════════
# 6 — IRON ASSEMBLY  (G minor, 96 BPM)
# Steampunk forge: staccato trumpet, brass chords, mechanical drums
# 24 bars ≈ 60s
# ════════════════════════════════════════════════════════════════════════════════
def iron_assembly():
    t = tp(96)
    mel = Tr(TRUMPET, 0, t, vol=95, rev=38, cho=4)
    mel.seq([
        (G4,0.5,88),(R,0.5,0),(G4,0.5,86),(Bb4,0.5,85),(D5,1,85),(R,1,0),  # b1
        (D5,1,88),(C5,1,86),(Bb4,1,84),(G4,1,82),                            # b2
        (Eb5,0.5,86),(R,0.5,0),(D5,0.5,84),(C5,0.5,82),(Bb4,1,82),(G4,1,80),# b3
        (G4,4,76),                                                             # b4
        (G4,0.5,88),(Bb4,0.5,86),(D5,0.5,85),(G5,0.5,85),(D5,1,84),(G4,1,80),# b5
        (F5,2,86),(Eb5,1,82),(D5,1,80),                                       # b6
        (C5,1,80),(Bb4,1,78),(A4,1,76),(G4,1,76),                            # b7
        (G4,4,72),                                                             # b8
    ], reps=3)
    pad = Tr(BRASS, 1, t, vol=75, rev=40, cho=5)
    pad.cseq([
        ([G2,D3,G3],2,68),([G2,D3,G3],2,65),
        ([G2,D3,G3],2,66),([C3,Eb3,G3],2,63),
        ([Eb3,Bb3,Eb4],2,64),([D3,A3,D4],2,66),
        ([G2,D3,G3],4,68),
        ([G2,D3,G3],2,68),([Bb2,F3,Bb3],2,65),
        ([C3,G3,C4],2,66),([D3,A3,D4],2,66),
        ([Eb3,Bb3,Eb4],2,64),([D3,A3,D4],2,65),
        ([G2,D3,G3],4,68),
    ], reps=3)
    bas = Tr(TUBA, 2, t, vol=92, rev=36)
    bas.seq([
        (G2,1,80),(R,1,0),(G2,1,78),(G2,1,76),
        (G2,4,75),(Eb2,4,72),(D2,4,74),(G2,4,76),
        (G2,1,80),(R,1,0),(G2,1,78),(Bb2,1,76),
        (C3,4,74),(D3,4,74),(Eb3,4,72),(G2,4,76),
    ], reps=3)
    dr = DrumTr(t); dr.fill(MECH,24); dr.build()
    render([mel,pad,bas,dr], "faction_music_6.wav", room=0.42, gain=0.52)


# ════════════════════════════════════════════════════════════════════════════════
# 7 — AMALGAMATE  (C# minor / altered, 70 BPM)
# Swamp mutants: oboe+bassoon in dissonance, low strings, odd meter feel
# 16 bars ≈ 55s
# ════════════════════════════════════════════════════════════════════════════════
def amalgamate():
    t = tp(70)
    mel = Tr(OBOE, 0, t, vol=88, rev=65, cho=12)
    mel.seq([
        (Cs5,1,80),(D5,1,78),(E5,1,76),(Fs5,1,78),   # b1 C# minor
        (Ab5,2,80),(Fs5,2,76),                         # b2 chromatic Ab = G#
        (E5,1,74),(Eb5,1,72),(D5,1,70),(Cs5,1,70),   # b3 descent
        (B4,4,72),                                     # b4
        (Fs4,1,78),(Ab4,1,76),(A4,1,74),(B4,1,76),   # b5
        (Cs5,2,78),(B4,2,74),                          # b6
        (A4,1,72),(Ab4,1,70),(Fs4,1,68),(E4,1,68),   # b7 chromatic drop
        (Cs4,4,70),                                    # b8 dark resolution
    ], reps=2)
    ctr = Tr(BASSOON, 1, t, vol=80, rev=60, cho=8)
    ctr.seq([                          # Counter-melody bassoon, slightly dissonant
        (Cs4,1,72),(B3,1,70),(A3,1,70),(B3,1,72),
        (Cs4,2,70),(A3,2,68),
        (Ab3,1,68),(A3,1,68),(B3,1,70),(A3,1,68),
        (Gs3,4,66),
        (Fs3,1,72),(Ab3,1,70),(A3,1,68),(B3,1,70),
        (Cs4,2,72),(A3,2,68),
        (Ab3,1,68),(Fs3,1,66),(E3,1,64),(D3,1,64),
        (Cs3,4,68),
    ], reps=2)
    pad = Tr(STR2, 2, t, vol=58, rev=70, cho=14)
    pad.cseq([
        ([Cs3,E3,Ab3],4,48),([B2,D3,Fs3],4,46),
        ([A2,Cs3,E3],4,46),([Ab2,B2,Ds3],4,45),
        ([Cs3,E3,Ab3],4,48),([Fs2,A2,Cs3],4,46),
        ([Ab2,B2,Ds3],4,45),([Cs3,E3,Ab3],4,48),
    ], reps=2)
    bas = Tr(CELLO, 3, t, vol=85, rev=55)
    bas.seq([
        (Cs3,4,65),(B2,4,62),(A2,4,60),(Ab2,4,60),
        (Cs3,4,65),(Fs2,4,60),(Ab2,4,58),(Cs3,4,65),
    ], reps=2)
    render([mel,ctr,pad,bas], "faction_music_7.wav", room=0.68, gain=0.52)


# ════════════════════════════════════════════════════════════════════════════════
# 8 — CONVERGENCE  (D Lydian, 66 BPM)
# Celestial realm: French horn, soaring strings, angelic choir, harp
# 16 bars ≈ 58s
# ════════════════════════════════════════════════════════════════════════════════
def convergence():
    t = tp(66)
    mel = Tr(FRENCH_HORN, 0, t, vol=92, rev=58, cho=8)
    mel.seq([                          # D Lydian: D E F# G# A B C# D (raised 4th!)
        (D5,1,82),(E5,1,82),(Fs5,1,84),(Ab5,1,85),   # b1 G# = Ab (Lydian #4)
        (A5,2,86),(B5,2,84),                           # b2
        (Cs6,2,85),(A5,2,82),                          # b3 C# = Cs6
        (Fs5,4,80),                                    # b4
        (D5,1,82),(Fs5,1,84),(A5,1,86),(D6,1,88),    # b5 soaring
        (Cs6,2,86),(B5,2,84),                          # b6
        (A5,1,82),(Ab5,1,80),(Fs5,1,78),(E5,1,76),   # b7 descend
        (D5,4,74),                                     # b8
    ], reps=2)
    pad = Tr(STR1, 1, t, vol=75, rev=75, cho=16)
    pad.cseq([
        ([D3,Fs3,A3],4,58),([D3,Fs3,A3],4,56),
        ([G2,B2,D3],4,54),([A2,Cs3,E3],4,54),
        ([B2,D3,Fs3],4,54),([A2,Cs3,E3],4,54),
        ([G2,B2,D3],4,52),([D3,Fs3,A3],4,58),
    ], reps=2)
    cho = Tr(CHOIR, 2, t, vol=68, rev=82, cho=18)
    cho.seq([
        (A4,2,44),(B4,2,44),(Fs4,4,42),(E4,4,42),(Fs4,4,44),
        (A4,2,44),(B4,2,44),(Cs5,4,44),(A4,4,42),(Fs4,4,44),
    ], reps=2)
    hrp = Tr(HARP, 3, t, vol=65, rev=65, cho=10)
    hrp.seq([
        (D3,0.5,58),(Fs3,0.5,56),(A3,0.5,54),(D4,0.5,56),
        (D3,0.5,55),(Fs3,0.5,53),(A3,0.5,51),(D4,0.5,53),  # b1 D
        (G2,0.5,58),(B2,0.5,56),(D3,0.5,54),(G3,0.5,56),
        (G2,0.5,55),(B2,0.5,53),(D3,0.5,51),(G3,0.5,53),   # b2 G
        (A2,0.5,58),(Cs3,0.5,56),(E3,0.5,54),(A3,0.5,56),
        (A2,0.5,55),(Cs3,0.5,53),(E3,0.5,51),(A3,0.5,53),  # b3 A
        (B2,0.5,58),(D3,0.5,56),(Fs3,0.5,54),(B3,0.5,56),
        (B2,0.5,55),(D3,0.5,53),(Fs3,0.5,51),(B3,0.5,53),  # b4 Bm
        (D3,0.5,60),(Fs3,0.5,58),(A3,0.5,56),(D4,0.5,58),
        (D3,0.5,58),(Fs3,0.5,56),(A3,0.5,54),(D4,0.5,56),  # b5 D
        (A2,0.5,58),(Cs3,0.5,56),(E3,0.5,54),(A3,0.5,56),
        (A2,0.5,55),(Cs3,0.5,53),(E3,0.5,51),(A3,0.5,53),  # b6 A
        (G2,0.5,56),(B2,0.5,54),(D3,0.5,52),(G3,0.5,54),
        (G2,0.5,53),(B2,0.5,51),(D3,0.5,49),(G3,0.5,51),   # b7 G
        (D3,0.5,62),(Fs3,0.5,60),(A3,0.5,58),(D4,0.5,60),
        (D3,0.5,60),(Fs3,0.5,58),(A3,0.5,56),(D4,0.5,58),  # b8 D strong
    ], reps=2)
    render([mel,pad,cho,hrp], "faction_music_8.wav", room=0.65, gain=0.54)


# ════════════════════════════════════════════════════════════════════════════════
# WORLD MAP  (G Major, 88 BPM)
# Adventure overworld: French horn melody, full strings, light march
# 24 bars ≈ 65s
# ════════════════════════════════════════════════════════════════════════════════
def world_map():
    t = tp(88)
    mel = Tr(FRENCH_HORN, 0, t, vol=90, rev=52, cho=8)
    mel.seq([
        (G4,1,82),(A4,1,82),(B4,1,82),(D5,1,84),   # b1 G major
        (D5,2,82),(C5,1,78),(B4,1,76),               # b2
        (A4,1,78),(B4,1,78),(C5,1,78),(D5,1,80),   # b3
        (G5,4,82),                                   # b4 peak
        (E5,1,80),(D5,1,78),(C5,1,76),(B4,1,76),   # b5
        (A4,2,74),(G4,2,72),                         # b6
        (B4,1,76),(D5,1,78),(G5,1,80),(Fs5,1,78),  # b7
        (G5,4,76),                                   # b8
    ], reps=3)
    ctr = Tr(OBOE, 1, t, vol=72, rev=55, cho=6)
    ctr.seq([                          # Oboe counter-melody
        (B4,2,65),(D5,2,65),(G5,2,65),(Fs5,2,63),
        (E5,4,63),(D5,4,62),(E5,4,63),(G4,4,62),
    ], reps=3)
    pad = Tr(STR1, 2, t, vol=70, rev=65, cho=12)
    pad.cseq([
        ([G2,B2,D3],4,55),([G2,B2,D3],4,55),
        ([C2,E2,G2],4,52),([D2,Fs2,A2],4,52),
        ([E2,G2,B2],4,52),([D2,Fs2,A2],4,52),
        ([C2,E2,G2],4,52),([G2,B2,D3],4,55),
    ], reps=3)
    bas = Tr(CELLO, 3, t, vol=85, rev=48)
    bas.seq([
        (G2,4,65),(G2,4,62),(C2,4,60),(D2,4,62),
        (E2,4,60),(D2,4,60),(C2,4,58),(G2,4,65),
    ], reps=3)
    dr = DrumTr(t); dr.fill(MARCH,24); dr.build()
    render([mel,ctr,pad,bas,dr], "worldmap_music.wav", room=0.55, gain=0.55)


# ════════════════════════════════════════════════════════════════════════════════
# COMBAT  (D minor, 140 BPM)
# Urgent battle: staccato brass, fast strings, relentless percussion
# 24 bars ≈ 41s
# ════════════════════════════════════════════════════════════════════════════════
def combat():
    t = tp(140)
    mel = Tr(TRUMPET, 0, t, vol=100, rev=35, cho=4)
    mel.seq([
        (D5,0.5,92),(R,0.5,0),(D5,0.5,90),(F5,0.5,88),(A5,1,90),(R,1,0),  # b1
        (D6,2,92),(C6,1,88),(A5,1,86),                                       # b2
        (G5,0.5,88),(R,0.5,0),(G5,0.5,86),(Bb5,0.5,85),(D6,1,88),(R,1,0), # b3
        (A5,2,86),(F5,2,82),                                                  # b4
        (C6,1,90),(Bb5,1,88),(A5,1,86),(G5,1,84),                           # b5
        (F5,2,82),(E5,2,80),                                                  # b6
        (D5,0.5,90),(E5,0.5,88),(F5,0.5,86),(G5,0.5,85),(A5,1,88),(D5,1,84),# b7
        (D5,4,76),                                                             # b8
    ], reps=3)
    pad = Tr(BRASS, 1, t, vol=82, rev=36, cho=4)
    pad.cseq([
        ([D2,A2,D3],2,72),([D2,A2,D3],2,70),
        ([Bb2,F3,Bb3],2,70),([A2,E3,A3],2,72),
        ([F2,C3,F3],2,70),([G2,D3,G3],2,70),
        ([Bb2,F3,Bb3],2,68),([A2,E3,A3],2,72),
    ], reps=3)
    bas = Tr(TUBA, 2, t, vol=96, rev=34)
    bas.seq([
        (D2,0.5,88),(R,0.5,0),(D2,0.5,85),(R,0.5,0),(D2,1,85),(D2,1,82),
        (Bb2,4,78),(A2,4,76),(F2,4,74),(G2,4,76),
        (D2,0.5,88),(R,0.5,0),(D2,0.5,85),(R,0.5,0),(D2,1,85),(E2,1,82),
        (F2,4,76),(G2,4,76),(Bb2,4,74),(A2,4,76),
    ], reps=3)
    dr = DrumTr(t); dr.fill(DEMON,24); dr.build()
    render([mel,pad,bas,dr], "combat_music.wav", room=0.38, gain=0.50)


# ════════════════════════════════════════════════════════════════════════════════
# TOWN (generic fallback for towns without a faction match)
# C Major, 76 BPM — bright, welcoming
# ════════════════════════════════════════════════════════════════════════════════
def town_generic():
    t = tp(76)
    mel = Tr(CLARINET, 0, t, vol=88, rev=52, cho=10)
    mel.seq([
        (C5,1,80),(E5,1,80),(G5,1,78),(A5,1,80),
        (G5,2,76),(E5,2,74),
        (F5,1,76),(G5,1,76),(A5,1,76),(G5,1,74),
        (E5,4,70),
        (D5,1,78),(F5,1,78),(A5,1,80),(C6,1,82),
        (B5,2,78),(G5,2,75),
        (A5,1,74),(G5,1,74),(F5,1,72),(E5,1,70),
        (C5,4,68),
    ], reps=2)
    pad = Tr(STR1, 1, t, vol=68, rev=65, cho=14)
    pad.cseq([
        ([C3,E3,G3],4,52),([C3,E3,G3],4,52),
        ([F2,A2,C3],4,50),([G2,B2,D3],4,50),
        ([A2,C3,E3],4,50),([G2,B2,D3],4,50),
        ([F2,A2,C3],4,50),([C3,E3,G3],4,52),
    ], reps=2)
    bas = Tr(CELLO, 2, t, vol=85, rev=48)
    bas.seq([
        (C3,2,62),(C3,2,60),(F2,4,58),(G2,4,60),
        (A2,4,58),(G2,4,58),(F2,4,56),(C3,4,62),
    ], reps=2)
    render([mel,pad,bas], "town_music.wav", room=0.58)


# ── Main ──────────────────────────────────────────────────────────────────────
if __name__ == "__main__":
    print("Generating orchestral music via FluidSynth + FluidR3_GM.sf2 …")
    print()
    print("Faction town themes:")
    holy_order()        # 0 HolyOrder
    crimson_wardens()   # 1 CrimsonWardens
    thornkin()          # 2 Thornkin
    eternal_empire()    # 3 EternalEmpire
    bloodsworn()        # 4 Bloodsworn
    voidkin()           # 5 Voidkin
    iron_assembly()     # 6 IronAssembly
    amalgamate()        # 7 Amalgamate
    convergence()       # 8 Convergence
    print()
    print("Shared tracks:")
    world_map()
    combat()
    town_generic()
    print()
    print("Done.")
