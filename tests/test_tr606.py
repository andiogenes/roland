#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Tests of the TR-606 kit: tools/gen_tr606.py, and the ported synth (overlay/firmware/src/drum_synth606.c) built on the
host against stand-ins for Felucca's helpers (tests/tr606_host.c).   python3 -I tests/test_tr606.py   (needs gcc)"""
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "overlay" / "tools"))
import gen_tr606 as g  # noqa: E402


class Gen(unittest.TestCase):
    def test_rows(self):
        r = dict(g.rows())
        self.assertEqual(list(r), g.LANES)
        self.assertEqual(r["KICK"], [1, 0, 34, 8, 18, 36, 3, 70, 124, 0, 0, 20, 0, 0, 44, 0, 127, 0, 0, 0, 36, 144])
        self.assertTrue(all(len(v) == 22 for v in r.values()))
        self.assertEqual(r["CHH"][1], 2)                      # metal noise
        self.assertEqual(r["CLAP"][1], 0x11)                  # the clap source

    def test_606_differs_from_808_where_sloop_says(self):
        k808 = g.kit(g.K808)
        self.assertNotEqual(g.enc(g.K606["KICK"]), g.enc(k808["KICK"]))
        self.assertEqual(g.enc(g.K606["CLAP"]), g.enc(k808["CLAP"]))     # the 606 keeps the 808 clap
        self.assertEqual(g.enc(g.K606["COWBELL"]), g.enc(k808["COWBELL"]))

    def test_gain(self):
        a = dict(g.rows(0.0))["SNARE"][21]
        b = dict(g.rows(-3.0))["SNARE"][21]
        self.assertEqual(a - b, 12)                           # level is in quarter dB
        self.assertEqual(dict(g.rows(40.0))["SNARE"][21], 255)   # the level byte stops at +31.75 dB

    def test_header(self):
        with tempfile.TemporaryDirectory() as d:
            out = Path(d) / "h.h"
            g.main(str(out))
            t = out.read_text()
            self.assertEqual(t.count("{"), 16)
            self.assertFalse(t.rstrip().endswith("\\"))


class Synth(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        d = Path(cls.tmp.name)
        g.main(str(d / "felucca_tr606.h"))
        exe = d / "host"
        cc = subprocess.run(["gcc", "-O1", "-Wall", "-Wextra", "-Werror", "-I", str(d), "-o", str(exe),
                             str(ROOT / "tests" / "tr606_host.c"), "-lm"], capture_output=True, text=True)
        assert cc.returncode == 0, cc.stderr
        out = subprocess.run([str(exe)], capture_output=True, text=True, timeout=120).stdout
        lines = [l.split() for l in out.splitlines()]
        cls.r = {int(a[0]): dict(ended=int(a[1]), peak=int(a[2]), rms=int(a[3]), p1s=int(a[4])) for a in lines if a[0] != "C"}
        cls.choke = [int(v) for a in lines if a[0] == "C" for v in a[1:]]

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def test_all_sounds_sound_and_end(self):
        self.assertEqual(len(self.r), 16)
        for l, v in self.r.items():
            self.assertGreater(v["peak"], 2000, f"lane {l} is silent: {v}")
            self.assertLess(v["peak"], 200000, f"lane {l} is huge: {v}")
            self.assertGreater(v["ended"], 0, f"lane {l} never ended: {v}")

    def test_shapes(self):
        r = self.r
        self.assertLess(r[3]["ended"], r[4]["ended"])         # closed hat shorter than open
        self.assertGreater(r[7]["ended"], r[3]["ended"])      # crash longer than closed hat
        self.assertGreater(r[0]["rms"], 500)                  # the kick has body
        self.assertLess(r[1]["p1s"], r[1]["peak"] // 50 + 1)  # the snare is gone after a second

    def test_choke(self):
        choked, free = self.choke
        self.assertGreater(choked, 0)
        self.assertLess(choked, 50 * 44100 // 1000 + 44100 * 25 // 1000)    # gone within 25 ms of the choke
        self.assertLess(choked, free // 2)                                  # and far sooner than left alone

    def test_peaks_in_a_usable_range(self):
        # drum_render adds x1.25 and a knee at 24000: nothing should sit far above full scale before it
        print("\n lane ended   peak   rms100ms", file=sys.stderr)
        for l, v in sorted(self.r.items()):
            print(f" {g.LANES[l]:7} {v['ended']:6} {v['peak']:6} {v['rms']:6}", file=sys.stderr)
        self.assertLess(max(v["peak"] for v in self.r.values()), 100000)


if __name__ == "__main__":
    unittest.main(verbosity=2)
