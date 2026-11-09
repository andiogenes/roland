#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Tests of overlay/tools/gen_hr16.py on synthetic WAV files (no HR-16 audio is needed or used).

  python3 -I tests/test_gen_hr16.py

The generated header is also compiled with the host C compiler, and the firmware's decode (hr_next, copied here from
overlay-patched eng_drum.c, the test checks the copy against the patch) must give the very samples the generator predicts."""
import math
import os
import random
import re
import struct
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
GEN = ROOT / "overlay" / "tools" / "gen_hr16.py"
sys.path.insert(0, str(GEN.parent))
import gen_hr16 as g  # noqa: E402


def tone(rate, sec, hz, decay):
    n = int(rate * sec)
    return [math.sin(2 * math.pi * hz * i / rate) * math.exp(-i / (rate * decay)) for i in range(n)]


def noise(rate, sec, decay, seed=1):
    r = random.Random(seed)
    return [(r.random() * 2 - 1) * math.exp(-i / (rate * decay)) for i in range(int(rate * sec))]


def put_wav(path, x, rate, bits=16, nch=1, tag=1):
    path.parent.mkdir(parents=True, exist_ok=True)
    frames = bytearray()
    for v in x:
        for _ in range(nch):
            if tag == 3:
                frames += struct.pack("<f", v)
            elif bits == 16:
                frames += struct.pack("<h", int(v * 32000))
            elif bits == 24:
                frames += int(v * 8000000).to_bytes(3, "little", signed=True)
            elif bits == 8:
                frames += bytes([int(v * 120) + 128])
    w = bits // 8
    fmt = struct.pack("<HHIIHH", tag, nch, rate, rate * nch * w, nch * w, bits)
    path.write_bytes(b"RIFF" + struct.pack("<I", 36 + len(frames)) + b"WAVEfmt " + struct.pack("<I", 16) + fmt +
                     b"data" + struct.pack("<I", len(frames)) + bytes(frames))


def make_pack(d):
    put_wav(d / "Kick_01.wav", tone(44100, 0.5, 55, 0.12), 44100)
    put_wav(d / "Kick_02.wav", tone(44100, 0.5, 70, 0.08), 44100, bits=24, nch=2)
    put_wav(d / "snare/SD_Hard.wav", noise(44100, 0.4, 0.07), 44100)
    put_wav(d / "snare/SD_Soft.wav", noise(32000, 0.4, 0.05, 2), 32000, bits=8)
    put_wav(d / "Handclap.wav", noise(44100, 0.3, 0.05, 3), 44100)
    put_wav(d / "Hat_Closed.wav", noise(44100, 0.1, 0.01, 4), 44100)
    put_wav(d / "HatOpen.wav", noise(44100, 0.9, 0.2, 5), 44100, tag=3, bits=32)
    put_wav(d / "Tom_Lo.wav", tone(44100, 0.5, 90, 0.15), 44100)
    put_wav(d / "Rimshot.wav", tone(44100, 0.1, 1700, 0.01), 44100)
    put_wav(d / "Cowbell.wav", tone(44100, 0.6, 800, 0.2), 44100)
    put_wav(d / "readme_loop.wav", tone(44100, 0.2, 440, 0.1), 44100)


def run(*args, env=None):
    e = dict(os.environ, ACOUSTIC_DIR="")          # (the bundled acoustic kit is tested in test_acoustic.py)
    e.update(env or {})
    return subprocess.run([sys.executable, "-I", str(GEN), *args], capture_output=True, text=True, env=e)


class Classify(unittest.TestCase):
    def test_names(self):
        c = g.classify
        self.assertEqual(c("Kick_01")[0], "KICK")
        self.assertEqual(c("HR16B_BD2")[0], "KICK")
        self.assertEqual(c("SD_Hard")[0], "SNARE")
        self.assertEqual(c("cs_big_foot_kk")[0], "KICK")        # the Cluster Sound HR-16B names
        self.assertEqual(c("cs_aggressive_sn")[0], "SNARE")
        self.assertEqual(c("cs_r&b_cl_hihat")[0], "HATCL")
        self.assertEqual(c("cs_open_conga")[0], "TOM")
        self.assertEqual(c("cs_gated_sidestick")[0], "RIM")
        self.assertIsNone(c("cs_torque_wrench"))
        self.assertEqual(c("Handclap")[0], "CLAP")
        self.assertEqual(c("Hat_Closed")[0], "HATCL")
        self.assertEqual(c("ClosedHiHat")[0], "HATCL")
        self.assertEqual(c("HatOpen")[0], "HATOP")
        self.assertEqual(c("OpenHat_2")[0], "HATOP")
        self.assertEqual(c("hihatOpen")[0], "HATOP")
        self.assertEqual(c("Tom_Lo")[0], "TOM")
        self.assertEqual(c("Rimshot")[0], "RIM")
        self.assertEqual(c("Cowbell")[0], "BELL")
        self.assertEqual(c("Crash")[0], "BELL")
        self.assertIsNone(c("readme_loop"))
        self.assertIsNone(c("Bottom"))                    # "tom" inside a word is not a tom


class Codec(unittest.TestCase):
    def test_roundtrip_and_decoder(self):
        pcm = [int(12000 * math.sin(i / 20.0) * math.exp(-i / 800)) for i in range(2000)]
        data, dec = g.ima_encode(pcm)
        self.assertEqual(len(data), 1000)
        self.assertEqual(g.ima_decode(data, len(pcm)), dec)         # the decoder reproduces the encoder's state
        err = sum((a - b) ** 2 for a, b in zip(pcm, dec)) / len(pcm)
        sig = sum(a * a for a in pcm) / len(pcm)
        self.assertGreater(10 * math.log10(sig / max(err, 1)), 28)  # IMA 4 bit: about 31 dB on this decaying tone (a faster one gives less)

    def test_odd_length(self):
        data, dec = g.ima_encode([100, -200, 300])
        self.assertEqual(len(data), 2)
        self.assertEqual(g.ima_decode(data, 3), dec)

    def test_prepare(self):
        x = tone(44100, 1.5, 100, 5.0)
        p = g.prepare(x, 44100, "HATCL", 22050)
        self.assertLessEqual(len(p), int(0.25 * 22050))               # the lane's longest
        self.assertLessEqual(max(abs(v) for v in p), g.PEAK["HATCL"])
        self.assertGreater(max(abs(v) for v in p), g.PEAK["HATCL"] * 0.9)
        self.assertEqual(p[-1], 0)                                    # faded out
        with self.assertRaises(ValueError):
            g.prepare([0.0] * 100, 44100, "KICK", 22050)              # silence


class Tool(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.d = Path(self.tmp.name)
        make_pack(self.d / "pack")

    def tearDown(self):
        self.tmp.cleanup()

    def test_list(self):
        r = run("--list", str(self.d / "pack"))
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertIn("KICK   Kick_01.wav, Kick_02.wav", r.stdout)
        self.assertIn("not sorted into a lane: readme_loop.wav", r.stdout)

    def test_build_and_compile(self):
        out = self.d / "gen" / "felucca_hr16.h"
        r = run(str(out), "--preview", str(self.d / "prev"), env={"HR16_DIR": str(self.d / "pack")})
        self.assertEqual(r.returncode, 0, r.stderr + r.stdout)
        self.assertIn("2 kits", r.stdout)                              # two kicks, two snares: 2 kits
        text = out.read_text()
        self.assertIn("#define HR16_NKITS 2", text)
        self.assertIn('X(0, "HR-A", "HR16 A")', text)
        self.assertEqual(len(list((self.d / "prev").glob("*.wav"))), int(re.search(r"HR16_NSAMP (\d+)", text).group(1)))
        # the C side: the header compiles and the firmware's decode equals the generator's
        c = self.d / "t.c"
        c.write_text(r'''
#include <stdint.h>
#include <stdio.h>
#include "felucca_hr16.h"
static const int16_t IMA_STEP[89] = {%s};
static const int8_t IMA_IDX[8] = {-1, -1, -1, -1, 2, 4, 6, 8};
#define clamp(v, lo, hi) ((v) < (lo) ? (lo) : (v) > (hi) ? (hi) : (v))
typedef struct { uint32_t ph[3]; int32_t s[8]; } voice_t;
typedef struct { uint32_t off, n, rate, fmt; } hr_samp_t;
%s
static const hr_samp_t HR16_SAMP[HR16_NSAMP] = {HR16_SAMP_INIT};
int main(void) {
    for (unsigned si = 0; si < HR16_NSAMP; si++) {
        voice_t v = {{0}, {0}};
        printf("%%u %%u %%u", si, HR16_SAMP[si].n, HR16_SAMP[si].rate);
        for (unsigned i = 0; i < HR16_SAMP[si].n; i++) printf(" %%d", hr_next(&HR16_SAMP[si], &v));
        printf("\n");
    }
    for (unsigned k = 0; k < HR16_NKITS; k++) for (int l = 0; l < 8; l++) printf("K %%u %%d %%u\n", k, l, HR16_KIT[k][l]);
    return 0;
}''' % (", ".join(str(v) for v in g.IMA_STEP), hr_next_from_patch()))
        exe = self.d / "t"
        cc = subprocess.run(["gcc", "-Wall", "-Wextra", "-Werror", "-I", str(out.parent), "-o", str(exe), str(c)],
                            capture_output=True, text=True)
        self.assertEqual(cc.returncode, 0, cc.stderr)
        lines = subprocess.run([str(exe)], capture_output=True, text=True).stdout.splitlines()
        nsamp = int(re.search(r"HR16_NSAMP (\d+)", text).group(1))
        blob = bytes(int(v) for v in re.search(r"HR16_DATA\[\d+\] = \{(.*?)\};", text, re.S).group(1).replace("\n", " ").replace(",", " ").split())
        offs = [int(v) for v in re.findall(r"\{(\d+)u, \d+u, \d+u, \d+u\}", text)]
        for si in range(nsamp):
            f = lines[si].split()
            n = int(f[1])
            got = [int(v) for v in f[3:]]
            self.assertEqual(got, g.ima_decode(blob[offs[si]:], n), f"sample {si}")
            self.assertEqual(int(f[2]), round(22050 / 44100 * 65536))
        kit_lines = [l.split() for l in lines if l.startswith("K ")]
        self.assertEqual(len(kit_lines), 16)
        self.assertTrue(all(int(l[3]) < nsamp for l in kit_lines))

    def test_budget_drops_kits(self):
        out = self.d / "o.h"
        r = run(str(out), env={"HR16_DIR": str(self.d / "pack"), "HR16_BUDGET": "40000"})
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertIn("dropping kit HR-B", r.stderr)
        self.assertIn("1 kits", r.stdout)

    def test_acoustic_comes_first_and_is_never_dropped(self):
        out = self.d / "o.h"
        ac = str(ROOT / "assets" / "acoustic")
        r = run(str(out), env={"HR16_DIR": str(self.d / "pack"), "ACOUSTIC_DIR": ac, "HR16_BUDGET": "130000"})
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertIn("dropping kit HR-B", r.stderr)
        self.assertIn("7 kits (ACOUS, AC-DP, AC-TI, AC-BR, AC-DU, DRUMU, HR-A)", r.stdout)
        r = run(str(out), env={"HR16_DIR": str(self.d / "pack"), "ACOUSTIC_DIR": ac, "HR16_BUDGET": "20000"})
        self.assertNotEqual(r.returncode, 0)                          # the acoustic samples alone are over
        self.assertIn("over the 20000 byte budget", r.stderr)

    def test_budget_too_small(self):
        r = run(str(self.d / "o.h"), env={"HR16_DIR": str(self.d / "pack"), "HR16_BUDGET": "500"})
        self.assertNotEqual(r.returncode, 0)
        self.assertIn("over the 500 byte budget", r.stderr)

    def test_missing_lane(self):
        (self.d / "pack" / "Cowbell.wav").unlink()
        r = run(str(self.d / "o.h"), env={"HR16_DIR": str(self.d / "pack")})
        self.assertNotEqual(r.returncode, 0)
        self.assertIn("BELL", r.stderr)

    def test_missing_folder(self):
        r = run(str(self.d / "o.h"), env={"HR16_DIR": str(self.d / "nope")})
        self.assertNotEqual(r.returncode, 0)
        self.assertIn("no sample kits", r.stderr)

    def test_map(self):
        import json
        m = self.d / "map.json"
        m.write_text(json.dumps({"kits": [{"name": "X", "KICK": "Kick_02.wav", "SNARE": "snare/SD_Soft.wav",
                                           "CLAP": "Handclap.wav", "HATCL": "Hat_Closed.wav", "HATOP": "HatOpen.wav",
                                           "TOM": "Tom_Lo.wav", "RIM": "Rimshot.wav", "BELL": "Cowbell.wav"}]}))
        out = self.d / "o.h"
        r = run(str(out), env={"HR16_DIR": str(self.d / "pack"), "HR16_MAP": str(m)})
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertIn('X(0, "HR-X", "HR16 X")', out.read_text())

    def test_bad_wav(self):
        (self.d / "pack" / "Kick_01.wav").write_bytes(b"not a wav")
        r = run(str(self.d / "o.h"), env={"HR16_DIR": str(self.d / "pack")})
        self.assertNotEqual(r.returncode, 0)
        self.assertIn("not a WAV file", r.stderr)


def hr_next_from_patch():
    """hr_next as the patch has it (so a change there that this test does not follow fails here)"""
    patch = (ROOT / "patches" / "felucca-drum.patch").read_text()
    m = re.search(r"^\+static inline int32_t hr_next\(.*?^\+}\n", patch, re.S | re.M)
    assert m, "hr_next not found in patches/felucca-drum.patch"
    return "\n".join(l[1:] for l in m.group(0).splitlines()).replace("HR16_DATA[z->off", "HR16_DATA[z->off")


if __name__ == "__main__":
    unittest.main(verbosity=2)
