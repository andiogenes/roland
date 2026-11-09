#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""The COMP's fixed-point core (overlay/firmware/src/fx_comp.c) against the ideal static curve and the documented times.
python3 -I tests/test_comp.py   (needs cc)"""
import math
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TMP = tempfile.mkdtemp()
BIN = Path(TMP) / "comp_host"
subprocess.run(["cc", "-O1", "-o", str(BIN), str(ROOT / "tests" / "comp_host.c"), "-lm"], check=True)


def run(*a):
    return [float(x) for x in subprocess.run([str(BIN)] + [str(x) for x in a], check=True, capture_output=True, text=True).stdout.split()]


SLOPE = {0: .75, 1: .875, 2: 11 / 12, 3: .95}
def DB(x):
    return 20 * math.log10(x)


class Comp(unittest.TestCase):
    def test_static_curve(self):
        # INPUT 64 = +10.07 dB into a -12 dB threshold: the reduction above it is (over) * (1 - 1/R), within 1.2 dB
        for e in range(4):
            for amp in (.1, .25, .5, .9):
                peak, gr = run(amp, 64, 96, 32, 64, e)
                over = max(0.0, DB(amp) + 10.07 + 12.04)
                self.assertAlmostEqual(gr, over * SLOPE[e], delta=1.2, msg=f"ratio {e} amp {amp}")

    def test_output_knob_is_quarter_dB_steps(self):
        a = run(.05, 64, 96, 32, 64, 0)[0]
        b = run(.05, 64, 104, 32, 64, 0)[0]
        self.assertAlmostEqual(DB(b / a), 2.0, delta=.15)

    def test_more_input_compresses_more(self):
        g = [run(.3, i, 96, 32, 64, 0)[1] for i in (32, 64, 96, 127)]
        self.assertEqual(g, sorted(g))

    def test_attack_and_release_times(self):
        _, fast_up, fast_dn = run(.9, 64, 96, 0, 0, 0, "step")
        _, slow_up, slow_dn = run(.9, 64, 96, 127, 127, 0, "step")
        self.assertLess(fast_up, 300)                       # us: the 20 us end, a 1 kHz sine's peaks 1 ms apart
        self.assertTrue(600 < slow_up < 2500)               # the 800 us end
        self.assertTrue(15e3 < fast_dn < 80e3)              # the 50 ms end (the time to fall 20 dB)
        self.assertTrue(500e3 < slow_dn < 1200e3)           # the 1.1 s end

    def test_all_buttons_in_is_harder(self):
        self.assertGreater(run(.5, 64, 96, 32, 64, 4)[1], run(.5, 64, 96, 32, 64, 3)[1])


if __name__ == "__main__":
    unittest.main(verbosity=2)
