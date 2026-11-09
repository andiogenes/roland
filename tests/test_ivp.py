#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""PRE, the Surveyor-style preamp on the IVP's signal path (overlay/firmware/src/fx_ivp.c): the committed tables are the generator's;
the TUBE DRIVE path against numbers from the SPICE netlist (tools/fxlab/spice/ivp.py, frozen here) and the shelves against the
documentation's +-15 dB.   python3 -I tests/test_ivp.py   (needs cc)"""
import math
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools" / "fxlab"))
import gen_ivp_tab  # noqa: E402

BIN = Path(tempfile.mkdtemp()) / "ivp_host"
subprocess.run(["cc", "-O1", "-o", str(BIN), str(ROOT / "tests" / "ivp_host.c"), "-lm"], check=True)


def run(level, gain, bass, treble, rng, amp, f=1000.0):
    out = subprocess.run([str(BIN)] + [str(v) for v in (level, gain, bass, treble, rng, amp, f)], check=True,
                         capture_output=True, text=True).stdout.split()
    return [float(v) for v in out]


def db(x):
    return 20 * math.log10(x)


# ngspice: IC6A on the TUBE DRIVE pot (position d, audio taper) into the pair; a 1 kHz sine of AMP volts; the pair's differential output's fundamental (V)
SPICE = {(0.5, 0.01): 0.33, (0.5, 0.05): 1.51, (0.5, 0.2): 2.72, (0.5, 1.0): 2.82,
         (1.0, 0.01): 1.25, (1.0, 0.05): 2.72, (1.0, 0.2): 2.82}


class Ivp(unittest.TestCase):
    def test_committed_tables_are_the_generators(self):
        self.assertEqual((ROOT / "overlay/firmware/src/fx_ivp_tab.h").read_text(), gen_ivp_tab.text())

    def test_the_drive_path_against_the_circuit(self):
        for (d, amp), ref in SPICE.items():
            h = run(127, round(d * 127), 0, 0, 0, amp)
            self.assertAlmostEqual(db(h[0]), db(ref), delta=1.2, msg=f"drive {d} amp {amp}")

    def test_the_pair_clips_hard(self):
        a, b = run(127, 127, 0, 0, 0, 0.2), run(127, 127, 0, 0, 0, 1.0)
        self.assertAlmostEqual(db(a[0]), db(b[0]), delta=0.6)
        self.assertGreater(b[2] / b[0], 0.3)                   # a square: the third harmonic a third of the first

    def test_shelves_are_the_documents_15_dB(self):
        flat = run(127, 40, 0, 0, 0, 0.002, 50)[0]
        self.assertAlmostEqual(db(run(127, 40, 63, 0, 0, 0.002, 50)[0] / flat), 13.5, delta=2.5)      # BASS +15: the first-order shelf is 13.8 at 50 Hz
        self.assertAlmostEqual(db(run(127, 40, -64, 0, 0, 0.002, 50)[0] / flat), -13.5, delta=3.0)
        flat = run(127, 40, 0, 0, 0, 0.002, 10000)[0]
        self.assertAlmostEqual(db(run(127, 40, 0, 63, 0, 0.002, 10000)[0] / flat), 14.5, delta=2.0)   # TREBLE +15 at 10 kHz
        self.assertAlmostEqual(db(run(127, 40, 0, -64, 0, 0.002, 10000)[0] / flat), -14.5, delta=3.0)
        flat = run(127, 40, 0, 0, 0, 0.002, 1000)[0]
        self.assertAlmostEqual(db(run(127, 40, 0, 63, 0, 0.002, 1000)[0] / flat), 3.1, delta=2.0)     # the netlist, scaled to the document

    def test_range_hi_is_14_5_dB_more(self):
        lo, hi = run(127, 20, 0, 0, 0, 0.001)[0], run(127, 20, 0, 0, 1, 0.001)[0]
        self.assertAlmostEqual(db(hi / lo), 14.5, delta=1.0)

    def test_level_is_an_audio_taper(self):
        full, half = run(127, 127, 0, 0, 0, 0.2)[0], run(64, 127, 0, 0, 0, 0.2)[0]
        self.assertAlmostEqual(db(half / full), 20 * math.log10((64 / 127) ** 2), delta=0.3)

    def test_silence_is_silence(self):
        self.assertEqual(run(100, 100, 5, -5, 1, 0.0), [0.0, 0.0, 0.0, 0.0])


if __name__ == "__main__":
    unittest.main(verbosity=2)
