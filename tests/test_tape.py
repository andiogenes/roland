#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""The tape echo's pieces (overlay/firmware/src/fx_tape.c) on a host build.   python3 -I tests/test_tape.py   (needs cc)"""
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BIN = Path(tempfile.mkdtemp()) / "tape_host"
subprocess.run(["cc", "-O1", "-o", str(BIN), str(ROOT / "tests" / "tape_host.c"), "-lm"], check=True)
OUT = subprocess.run([str(BIN)], check=True, capture_output=True, text=True).stdout.split("\n")


class Tape(unittest.TestCase):
    def test_wander_is_bounded_by_its_three_depths(self):
        self.assertLessEqual(float(OUT[0]), 20 + 8 + 0.5 + 0.01)      # samples

    def test_pitch_deviation_is_tape_sized(self):
        self.assertTrue(0.05 < float(OUT[1]) < 0.4)                   # per cent: wow and flutter, not vibrato

    def test_it_is_there_all_the_time(self):
        self.assertGreater(float(OUT[3]), 5.0)                        # samples rms: a wander, not a still read

    def test_tap_is_exact_and_interpolates(self):
        exact, half = (int(v) for v in OUT[2].split())
        self.assertEqual(exact, 12000)
        self.assertEqual(half, 8000)                                  # half a sample earlier: the mean of 12000 and 4000


if __name__ == "__main__":
    unittest.main(verbosity=2)
