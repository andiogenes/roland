#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Tests of SLOOP's acoustic kit through gen_hr16.py, and of the sample-kit player in patches/felucca-drum.patch: its code
is cut out of the patch, built on the host (tests/player_host.c) and run on the real acoustic recordings (assets/acoustic).
   python3 -I tests/test_acoustic.py   (needs gcc)"""
import os
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "overlay" / "tools"))
import gen_hr16 as g  # noqa: E402

NAMES = ["ACOUS", "AC-DP", "AC-TI", "AC-BR", "AC-DU", "DRUMU"]


def player_source():
    """the sample player as the patch adds it, from the HR-16 section's start to its #else"""
    patch = (ROOT / "patches" / "felucca-drum.patch").read_text()
    added = "\n".join(l[1:] for l in patch.splitlines() if l.startswith("+") and not l.startswith("+++"))
    a = added.index("typedef struct { uint32_t off, n, rate, fmt; } hr_samp_t;")
    b = added.index("#else\n#define HRV_ON 0", a)
    return added[a:b]


class Acoustic(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        d = Path(cls.tmp.name)
        env = dict(os.environ, HR16_DIR="", ACOUSTIC_DIR=str(ROOT / "assets" / "acoustic"))
        r = subprocess.run([sys.executable, "-I", str(ROOT / "overlay" / "tools" / "gen_hr16.py"), str(d / "felucca_hr16.h")],
                           capture_output=True, text=True, env=env)
        assert r.returncode == 0, r.stderr + r.stdout
        cls.gen_out = r.stdout
        cls.header = (d / "felucca_hr16.h").read_text()
        (d / "player.c").write_text(player_source())
        steps = ", ".join(str(v) for v in g.IMA_STEP)
        cls.exe = d / "host"
        cc = subprocess.run(["gcc", "-O1", "-Wall", "-Wextra", "-Werror", f"-DPLAYER_FILE=\"{d / 'player.c'}\"",
                             f"-DIMA_STEP_INIT={steps}", "-I", str(d), "-o", str(cls.exe), str(ROOT / "tests" / "player_host.c"),
                             "-lm"], capture_output=True, text=True)
        assert cc.returncode == 0, cc.stderr

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def play(self, *knobs):
        out = subprocess.run([str(self.exe), *map(str, knobs)], capture_output=True, text=True, timeout=120).stdout.splitlines()
        res = {}
        for l in out:
            f = l.split()
            if f[0] == "C":
                res["choke"] = int(f[1])
            elif f[0] == "D":
                res.setdefault("pcm8", {})[(int(f[1]), int(f[2]))] = dict(fmt=int(f[3]), n=int(f[4]), distinct=int(f[5]),
                                                                          bad=int(f[6]), sum=int(f[7]))
            else:
                res[(int(f[0]), int(f[1]))] = dict(ended=int(f[2]), peak=int(f[3]), bad=int(f[4]))
        return res

    def test_generated(self):
        self.assertIn("6 kits", self.gen_out)
        self.assertIn("#define HR16_NKITS 6", self.header)
        self.assertIn("HR16_TREAT[HR16_NKITS] = {0, 1, 2, 3, 4, 0}", self.header)
        for i, n in enumerate(NAMES):
            self.assertIn(f'X({i}, "{n}"', self.header)
        self.assertIn("#define HR16_NSAMP 16", self.header)              # the acoustic set (8) for five kits, DRUMU's own 8
        rows = re.findall(r"\{(\d+)u, (\d+)u, (\d+)u, (\d+)u\}", self.header)
        self.assertEqual({int(r[2]) for r in rows}, {round(22050 / 44100 * 65536), round(32000 / 44100 * 65536),
                                                     round(26040 / 44100 * 65536)})
        self.assertEqual([int(r[3]) > 0 for r in rows], [False] * 8 + [True] * 8)   # fmt: ADPCM, then 8-bit PCM

    def test_every_sound_plays_and_ends(self):
        r = self.play()
        for kit in range(6):
            for lane in range(8):
                v = r[(kit, lane)]
                self.assertGreater(v["peak"], 2000, (kit, lane, v))
                self.assertLess(v["peak"], 32768, (kit, lane, v))
                self.assertGreater(v["ended"], 0, (kit, lane, v))

    def test_treatments(self):
        r = self.play()
        for lane in range(8):
            plain, deep, tight, bright, dust = (r[(k, lane)] for k in range(5))
            self.assertGreater(deep["ended"], plain["ended"], lane)       # pitched down: slower
            self.assertLess(bright["ended"], plain["ended"], lane)        # pitched up: quicker
            self.assertGreater(dust["ended"], plain["ended"], lane)       # one semitone down
            self.assertLessEqual(tight["ended"], plain["ended"], lane)
            self.assertEqual(dust["bad"], 0, lane)                        # DUST: multiples of 256
            self.assertEqual(plain["bad"], 0)                             # (only DUST is counted)
        self.assertLess(r[(2, 7)]["ended"], r[(0, 7)]["ended"] * 0.8)     # TIGHT cuts the crash well short
        self.assertLess(r[(1, 1)]["peak"], r[(0, 1)]["peak"])             # DEEP's low-pass takes the snare's peak down

    def test_drumu_is_8_bit_and_short(self):
        d = self.play()["pcm8"]
        self.assertEqual(len(d), 8)
        for (kit, lane), v in d.items():
            self.assertEqual(kit, 5)
            self.assertEqual(v["bad"], 0, (lane, v))                   # every value a whole step of the byte
            self.assertLessEqual(v["distinct"], 255, (lane, v))        # 8 bits
            self.assertGreater(v["distinct"], 100, (lane, v))          # and used (full scale)
        # the C decode equals the bytes the header holds
        blob = bytes(int(v) for v in re.search(r"HR16_DATA\[\d+\] = \{(.*?)\};", self.header, re.S).group(1).replace("\n", " ").replace(",", " ").split())
        rows = [(int(a), int(b), int(c), int(f)) for a, b, c, f in re.findall(r"\{(\d+)u, (\d+)u, (\d+)u, (\d+)u\}", self.header)]
        kit = [[int(v) for v in l.split("{")[1].split("}")[0].split(",")] for l in self.header.splitlines()
               if "/* DRUMU:" in l][0]
        for lane in range(8):
            off, n, _, fmt = rows[kit[lane]]
            want = sum((b - 256 if b > 127 else b) * fmt for b in blob[off:off + n])
            self.assertEqual(d[(5, lane)]["sum"], want, lane)
            self.assertEqual(d[(5, lane)]["n"], n)

    def test_drumu_plays_short_and_rings_less_than_the_acoustic_kit(self):
        r = self.play()
        for lane in range(8):
            self.assertLess(r[(5, lane)]["ended"], r[(0, lane)]["ended"] * 1.3 + 1, lane)
        self.assertLess(r[(5, 3)]["ended"], 0.2 * 44100)               # the closed hat: 80 ms

    def test_deep_kick_is_lower_than_other_notes(self):
        # lane 0 plays note 36 (down 5 semitones), the others note 45 (down 2): the kick lengthens more
        r = self.play()
        k = r[(1, 0)]["ended"] / r[(0, 0)]["ended"]
        s = r[(1, 1)]["ended"] / r[(0, 1)]["ended"]
        self.assertAlmostEqual(k, 2 ** (5 / 12), delta=0.12)
        self.assertAlmostEqual(s, 2 ** (2 / 12), delta=0.12)

    def test_knobs(self):
        base = self.play()
        short = self.play(64, 64, 20)                                     # DECY 20: a shorter ring
        for lane in (0, 4, 7):
            self.assertLess(short[(0, lane)]["ended"], base[(0, lane)]["ended"], lane)
        dark = self.play(64, 10, 64)                                      # TONE 10: darker
        for lane in (3, 4):
            self.assertLess(dark[(0, lane)]["peak"], base[(0, lane)]["peak"], lane)
        up = self.play(100, 64, 64)                                       # TUNE up: quicker
        self.assertLess(up[(0, 1)]["ended"], base[(0, 1)]["ended"])

    def test_choke(self):
        c = self.play()["choke"]
        self.assertGreater(c, 0)
        self.assertLess(c, 2205 + 600)                                    # a few ms after the closed hat


if __name__ == "__main__":
    unittest.main(verbosity=2)
