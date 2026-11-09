#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""METAL, the HM-2 model (overlay/firmware/src/fx_hm2.c): the committed tables are what the generator writes; the fixed-point code
against numbers from the SPICE netlist of the circuit (tools/fxlab/spice/hm2.py, run once and frozen here) and against the float
model.   python3 -I tests/test_hm2.py   (needs cc)"""
import math
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools" / "fxlab"))
import gen_hm2_tab  # noqa: E402

BIN = Path(tempfile.mkdtemp()) / "hm2_host"
subprocess.run(["cc", "-O1", "-o", str(BIN), str(ROOT / "tests" / "hm2_host.c"), "-lm"], check=True)


def run(dist, level, low, high, amp, f=1000.0):
    out = subprocess.run([str(BIN), str(dist), str(level), str(low), str(high), str(amp), str(f)], check=True,
                         capture_output=True, text=True).stdout.split()
    return [float(v) for v in out]


def db(x):
    return 20 * math.log10(x)


# ngspice, the Graviton schematic, a 1 kHz sine of AMP volts: the fundamental (V) at the tone stage's output, LOW / HIGH flat,
# and its second and third harmonics relative to it (spice/hm2.py tran_sine, harmonics)
SPICE = {   # (dist, amp): (h1, h2/h1, h3/h1)
    (0.1, 0.01): (0.540, 0.01, 0.23), (0.5, 0.01): (0.546, 0.01, 0.24), (0.9, 0.01): (0.554, 0.02, 0.26),
    (0.1, 0.1): (0.586, 0.30, 0.21), (0.5, 0.1): (0.586, 0.30, 0.21), (0.9, 0.1): (0.586, 0.30, 0.21),
}


class Hm2(unittest.TestCase):
    def test_committed_tables_are_the_generators(self):
        self.assertEqual((ROOT / "overlay/firmware/src/fx_hm2_tab.h").read_text(), gen_hm2_tab.text())

    def test_against_the_circuit(self):
        for (d, amp), (h1, r2, r3) in SPICE.items():
            h = run(round(d * 127), 127, 0, 0, amp)
            self.assertAlmostEqual(db(h[0]), db(h1), delta=1.6, msg=f"h1 dist {d} amp {amp}")
            self.assertAlmostEqual(h[2] / h[0], r3, delta=0.08, msg=f"h3 dist {d} amp {amp}")
            if amp >= 0.1:
                self.assertAlmostEqual(h[1] / h[0], r2, delta=0.1, msg=f"h2 dist {d} amp {amp}")   # the lopsided clip

    def test_dist_adds_gain_for_quiet_playing(self):
        g = [run(round(d * 127), 127, 0, 0, 0.001)[0] for d in (0.1, 0.5, 0.9)]
        self.assertEqual(g, sorted(g))
        self.assertGreater(g[2], 2 * g[0])                    # the gate and the op-amp stage open up

    def test_a_loud_note_is_a_square_of_one_level(self):
        a = run(64, 127, 0, 0, 0.05)[0]
        b = run(64, 127, 0, 0, 0.5)[0]
        self.assertAlmostEqual(db(a), db(b), delta=0.5)

    def test_low_band_boosts_and_cuts_by_about_19_dB(self):
        up, dn = run(64, 127, 63, 0, 0.1, 87.0)[0], run(64, 127, -64, 0, 0.1, 87.0)[0]
        self.assertAlmostEqual(db(up / dn), 2 * 19.3, delta=4.0)

    def test_high_band_boosts_and_cuts(self):
        up, dn = run(64, 127, 0, 63, 0.1, 1278.0)[0], run(64, 127, 0, -64, 0.1, 1278.0)[0]
        self.assertAlmostEqual(db(up / dn), 2 * 20.2, delta=4.0)               # the netlist: +-20.2 dB at 1278 Hz

    def test_level_is_an_audio_taper(self):
        full, half = run(64, 127, 0, 0, 0.1)[0], run(64, 64, 0, 0, 0.1)[0]
        self.assertAlmostEqual(db(half / full), 20 * math.log10((64 / 127) ** 2), delta=0.3)
        self.assertEqual(run(64, 0, 0, 0, 0.1)[0], 0.0)

    def test_silence_is_silence(self):
        self.assertEqual(run(64, 127, 0, 0, 0.0), [0.0, 0.0, 0.0, 0.0])


if __name__ == "__main__":
    unittest.main(verbosity=2)
