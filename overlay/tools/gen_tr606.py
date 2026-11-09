#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""The TR-606 drum kit of SLOOP, for the DRUM engine's TR606 KIT -> felucca_tr606.h

  gen_tr606.py OUT.h            (tools/build.py with FELUCCA_TR606=1)
  TR606_GAIN_DB=-3 gen_tr606.py OUT.h     every sound this many dB quieter / louder (default 0)

The sounds are SLOOP's (https://github.com/isod89/sloop-fm1, GPL-3.0, itself based on Felucca): tools/gen_drumkits.py
K808 (the base kit) and K606 (this kit's changes to it), its kick pass for small speakers (punch) and the per-lane levels
SLOOP measured for the 606 (tools/drumkit_levels.json). Each sound is written in musical units (Hz, ms, octaves) and
converted to the 22 numbers of one dsnd_t (firmware/src/eng_drum.c), the table indices of the synth that renders it.
The kit has 16 sounds: KICK SNARE CLAP CHH OHH TOMLO TOMHI CRASH RIDE SHAKER CONGA RIM COWBELL CLAVE KICK2 SNARE2
(a TR-606 itself has no conga, cowbell or clave: SLOOP's 606 keeps the 808's behind the GM notes that ask for them).
"""
import copy
import math
import os
import sys
from pathlib import Path

LANES = ["KICK", "SNARE", "CLAP", "CHH", "OHH", "TOMLO", "TOMHI", "CRASH", "RIDE", "SHAKER",
         "CONGA", "RIM", "COWBELL", "CLAVE", "KICK2", "SNARE2"]
WAVE = {None: 0, "sine": 1, "tri": 2, "square": 3, "fm": 4, "bell": 5}
SRC = {None: 0, "white": 1, "metal": 2, "cym": 3, "chip": 4, "clap": 0x11}
FMODE = {None: 0, "lp": 1, "bp": 2, "hp": 3}

# SLOOP tools/drumkit_levels.json, "606": dB added to each sound (measured there so that every kit is as loud as the others)
LEVELS_606 = {"KICK": 4.0, "SNARE": 3.0, "CLAP": 13.0, "CHH": 20.5, "OHH": 17.0, "TOMLO": 2.25, "TOMHI": 2.0,
              "CRASH": 7.25, "RIDE": 4.5, "SHAKER": 1.75, "CONGA": -0.25, "RIM": 6.0, "COWBELL": 5.25,
              "CLAVE": 1.75, "KICK2": 3.0, "SNARE2": 4.75}


def note16(hz):                    # MIDI note and 1/16 semitones
    n = 69 + 12 * math.log2(hz / 440.0)
    n16 = max(0, min(127 * 16 + 15, int(round(n * 16))))
    return n16 >> 4, n16 & 15


def note(hz):
    return max(0, min(127, int(round(69 + 12 * math.log2(hz / 440.0)))))


def dec(ms):                       # DECAY_K index: 5 ms .. 4 s over 0..127
    ms = max(5.0, min(4000.0, ms))
    return int(round(127 * math.log(ms / 5.0) / math.log(800)))


def cut(hz):                       # CUTOFF_HZ index: 30 Hz .. 16 kHz over 0..127
    hz = max(30.0, min(16000.0, hz))
    return int(round(127 * math.log(hz / 30.0) / math.log(16000 / 30)))


STEPS_PER_OCT = 127 / math.log2(16000 / 30)


def S(wave=None, hz=100, bend=0, bt=20, hold=0, decay=100, tlev=110, t2=0.0, t2lev=0, click=0,
      src=None, nlev=0, nhold=0, ndec=50, filt=None, fenv=0.0, hpf=None, chip=None, drive=0, level=0.0):
    """wave/hz: the tone; bend semitones falling over bt ms; hold ms at full, then decay; t2: the second partial's ratio
    (0 = none) and level; click: the attack burst 0..127; src: the noise, its level, hold and decay; filt: (mode, Hz,
    resonance 0..1[, "all"]) on the noise (or everything); fenv: octaves the cutoff starts above it, falling with the
    pitch; hpf: Hz of the noise high-pass; chip: Hz of the CHIP noise clock; drive 0..127; level dB"""
    return dict(wave=wave, hz=hz, bend=bend, bt=bt, hold=hold, decay=decay, tlev=tlev, t2=t2,
                t2lev=t2lev, click=click, src=src, nlev=nlev, nhold=nhold, ndec=ndec, filt=filt,
                fenv=fenv, hpf=hpf, chip=chip, drive=drive, level=level)


def enc(s, trim=0.0):
    p, f = note16(s["hz"])
    flt, fcut = 0, 127
    if s["filt"]:
        mode, hz, res = s["filt"][:3]
        flt = FMODE[mode] | (4 if "all" in s["filt"][3:] else 0) | (int(round(res * 31)) << 3)
        fcut = cut(hz)
    lev = int(round(128 + 4 * (s["level"] + trim)))
    return [WAVE[s["wave"]], SRC[s["src"]], p, f, int(s["bend"]), dec(s["bt"]),
            min(255, int(round(s["hold"] / 2))), dec(s["decay"]), int(s["tlev"]) if s["wave"] else 0,
            int(round(s["t2"] * 32)) if s["t2"] else 0, int(s["t2lev"]), int(s["click"]),
            int(s["nlev"]) if s["src"] else 0, min(255, int(round(s["nhold"] / 2))), dec(s["ndec"]),
            flt, fcut, int(round(s["fenv"] * STEPS_PER_OCT)),
            0 if s["hpf"] is None else cut(s["hpf"]), note(s["chip"]) if s["chip"] else 0,
            int(s["drive"]), max(0, min(255, lev))]


def kit(base, **mods):
    k = copy.deepcopy(base)
    for lane, ch in mods.items():
        if isinstance(ch, dict) and "wave" in ch:
            k[lane] = ch
        else:
            k[lane].update(ch)
    return k


K808 = dict(
    KICK=S("sine", 50, 12, 32, 8, 820, 124, click=26, drive=8),
    KICK2=S("sine", 42, 7, 60, 70, 1700, 124, click=14, drive=34),
    SNARE=S("sine", 185, 3, 14, 0, 120, 96, 1.78, 92, 22, "white", 100, 0, 170, ("hp", 1700, .12)),
    SNARE2=S("tri", 250, 4, 10, 0, 70, 84, 1.5, 60, 30, "white", 96, 0, 120, ("bp", 5200, .3)),
    CLAP=S(None, src="clap", nlev=122, ndec=230, filt=("bp", 1150, .38), hpf=600),
    CHH=S(None, 205, src="metal", nlev=118, ndec=42, filt=("bp", 9200, .42), hpf=6500, click=10),
    OHH=S(None, 205, src="metal", nlev=114, nhold=20, ndec=380, filt=("bp", 9000, .4), hpf=6200),
    TOMLO=S("sine", 98, 6, 60, 10, 420, 118, 1.5, 40, 16, "white", 18, 0, 30, ("lp", 2000, .1)),
    TOMHI=S("sine", 147, 6, 60, 10, 360, 116, 1.5, 40, 16, "white", 18, 0, 30, ("lp", 2600, .1)),
    CRASH=S(None, 205, src="cym", nlev=108, nhold=10, ndec=1800, filt=("hp", 4600, .22)),
    RIDE=S(None, 262, src="metal", nlev=104, ndec=1300, filt=("bp", 5200, .6), hpf=3000, click=12),
    SHAKER=S(None, src="white", nlev=96, nhold=12, ndec=55, filt=("bp", 7200, .3)),
    CONGA=S("sine", 310, 3, 25, 4, 230, 116, 1.5, 30, 24),
    RIM=S("tri", 1700, 0, 5, 0, 22, 92, 1.47, 70, 40, "white", 40, 0, 6, ("bp", 2600, .4, "all")),
    COWBELL=S("bell", 540, 0, 5, 6, 290, 108, filt=("bp", 2600, .5, "all")),
    CLAVE=S("sine", 2500, 0, 5, 0, 48, 108, click=12),
)
K606 = kit(K808,
    KICK=S("sine", 60, 18, 26, 6, 200, 124, click=34, drive=10),
    KICK2=S("sine", 52, 14, 30, 10, 320, 124, click=24, drive=20),
    SNARE=S("tri", 220, 3, 10, 0, 70, 80, 1.7, 50, 24, "white", 104, 0, 120, ("hp", 2500, .15)),
    CHH=S(None, 260, src="metal", nlev=116, ndec=30, filt=("bp", 10000, .45), hpf=8500, click=8),
    OHH=S(None, 260, src="metal", nlev=110, nhold=10, ndec=210, filt=("bp", 9800, .45), hpf=8000),
    TOMLO=S("sine", 150, 4, 30, 6, 190, 114, click=12),
    TOMHI=S("sine", 220, 4, 30, 6, 170, 112, click=12),
    CRASH=S(None, 300, src="metal", nlev=104, ndec=900, filt=("hp", 6000, .3)),
)

# SLOOP 2.7: the kicks heard on a small speaker (a 50 Hz sine plays nothing on the FM-1's speaker): drive, a deeper sweep
PUNCH_DRIVE, PUNCH_BEND, PUNCH_BT, PUNCH_CLICK = 36, 18, 34, 0.6


def punch(k):
    for lane in ("KICK", "KICK2"):
        d = k[lane]
        if d["wave"] not in ("sine", "tri"):
            continue
        d["drive"] = max(d["drive"], PUNCH_DRIVE)
        d["bend"] = max(d["bend"], PUNCH_BEND)
        d["bt"] = max(d["bt"], PUNCH_BT)
        d["click"] = int(d["click"] * PUNCH_CLICK)
    return k


def rows(gain_db=0.0):
    k = punch(copy.deepcopy(K606))
    out = []
    for lane in LANES:
        v = enc(k[lane], LEVELS_606[lane] + gain_db)
        assert len(v) == 22
        for i, x in enumerate(v):
            lo = -128 if i == 17 else 0
            hi = 127 if i == 17 else 255
            if not lo <= x <= hi:
                raise SystemExit(f"gen_tr606: {lane} field {i} = {x} is out of range {lo}..{hi}"
                                 f" (TR606_GAIN_DB too high?)")
        out.append((lane, v))
    return out


def main(path):
    gain = float(os.environ.get("TR606_GAIN_DB", "0"))
    L = ["/* Generated by tools/gen_tr606.py: SLOOP's TR-606 drum kit (GPL-3.0, https://github.com/isod89/sloop-fm1). */",
         "/* Do not edit. 16 dsnd_t, in the order of eng_drum.c's lanes: " + " ".join(LANES) + " */",
         "#define TR606_INIT \\"]
    r = rows(gain)
    for i, (lane, v) in enumerate(r):
        L.append("    {" + ", ".join(str(x) for x in v) + "}" + ("," if i + 1 < len(r) else "") + f" /* {lane} */ \\")
    Path(path).parent.mkdir(parents=True, exist_ok=True)
    Path(path).write_text("\n".join(L).rstrip(" \\") + "\n")
    print(f"gen_tr606: 16 sounds, gain {gain:+.1f} dB -> {path}")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit(__doc__)
    main(sys.argv[1])
