#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""The tape rot (overlay/firmware/src/fx_rot.c) on a host build.   python3 -I tests/test_rot.py   (needs cc)"""
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BIN = Path(tempfile.mkdtemp()) / "rot_host"
subprocess.run(["cc", "-O1", "-o", str(BIN), str(ROOT / "tests" / "rot_host.c"), "-lm"], check=True)


def run(*a):
    return float(subprocess.run([str(BIN)] + [str(x) for x in a], check=True, capture_output=True, text=True).stdout)


class Rot(unittest.TestCase):
    def test_everything_off_is_transparent(self):
        for f in (200, 1000):
            self.assertAlmostEqual(run("gain", f, 0, 0, 0, 0, 0), 0.0, delta=0.1)

    def test_loss_takes_the_highs_and_leaves_the_lows(self):
        self.assertLess(run("gain", 8000, 0, 0, 127, 0, 0), -20)
        self.assertGreater(run("gain", 200, 0, 0, 127, 0, 0), -1)

    def test_hiss_is_silence_plus_noise_up_to_minus_38_dB(self):
        self.assertEqual(run("noise", 0), 0.0)
        rms = run("noise", 127)
        self.assertTrue(250 < rms < 600, rms)                  # 32768 = full scale: -42 .. -35 dB

    def test_dropouts_only_when_asked(self):
        self.assertEqual(run("dips", 0), 0)
        self.assertGreater(run("dips", 127), 10)

    def test_wow_grows_with_its_knob(self):
        w = [run("wobble", a) for a in (0, 32, 127)]
        self.assertEqual(w[0], 0.0)
        self.assertTrue(w[0] < w[1] < w[2] < 2.5, w)           # per cent of pitch

    def test_drive_keeps_the_level_for_small_signals(self):
        self.assertAlmostEqual(run("gain", 1000, 0, 0, 0, 0, 127), 0.0, delta=0.8)


if __name__ == "__main__":
    unittest.main(verbosity=2)
