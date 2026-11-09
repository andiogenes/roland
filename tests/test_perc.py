#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""HARM, the Harmonic Percolator model (overlay/firmware/src/fx_perc.c): the committed tables are what the generator writes, and the
fixed-point code against the SPICE netlist's own numbers (tools/fxlab/spice/perc_spice_grid.json: 1 kHz sines, three modes, three
HARMONICS settings, five levels).   python3 -I tests/test_perc.py   (needs cc)"""
import json
import math
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools" / "fxlab"))
import gen_perc_tab  # noqa: E402

BIN = Path(tempfile.mkdtemp()) / "perc_host"
subprocess.run(["cc", "-O1", "-o", str(BIN), str(ROOT / "tests" / "perc_host.c"), "-lm"], check=True)
GRID = json.loads((ROOT / "tools/fxlab/spice/perc_spice_grid.json").read_text())
MODE = {"GE": 0, "OFF": 1, "SI": 2}


def run(harm, bal, mode, amp, f=1000.0):
    out = subprocess.run([str(BIN), str(harm), str(bal), str(mode), str(amp), str(f)], check=True, capture_output=True, text=True).stdout.split()
    return [float(v) for v in out]


def db(x):
    return 20 * math.log10(x)


class Perc(unittest.TestCase):
    def test_committed_tables_are_the_generators(self):
        self.assertEqual((ROOT / "overlay/firmware/src/fx_perc_tab.h").read_text(), gen_perc_tab.text())

    def test_against_the_circuit(self):
        worst = 0.0
        for key, ref in GRID.items():
            mode, h, amp = key.split("|")
            if float(amp) < (0.03 if mode == "GE" else 0.01):        # (the Ge diodes' knee at a few mV is not modelled: the shaper is a clip)
                continue
            m = run(round(float(h) * 127), 127, MODE[mode], amp)
            d = abs(db(m[0]) - db(ref[0]))
            worst = max(worst, d)
            self.assertLess(d, 3.0, f"h1 {key}: model {m[0]:.3f} circuit {ref[0]:.3f}")
            self.assertLess(abs(m[2] / m[0] - ref[2] / ref[0]), 0.25, f"h3 {key}")
        self.assertLess(worst, 3.0)

    def test_a_quiet_signal_is_linear_and_the_pot_is_the_gain(self):
        g = [run(h, 127, 1, 0.002)[0] / 0.002 for h in (40, 76, 100, 120, 127)]
        self.assertEqual(g, sorted(g))
        self.assertAlmostEqual(db(g[1]), 10.7, delta=1.5)      # the netlist: 10.6 dB at mid-pot (1 kHz)

    def test_the_diode_modes_clip_by_their_own_amounts(self):
        ge, off, si = (run(127, 127, m, 0.1)[0] for m in (0, 1, 2))
        self.assertTrue(ge < si < off, (ge, si, off))

    def test_balance_is_an_audio_taper(self):
        full, half = run(76, 127, 1, 0.05)[0], run(76, 64, 1, 0.05)[0]
        self.assertAlmostEqual(db(half / full), 20 * math.log10((64 / 127) ** 2), delta=0.3)

    def test_silence_is_silence(self):
        self.assertEqual(run(100, 100, 0, 0.0), [0.0, 0.0, 0.0, 0.0])


if __name__ == "__main__":
    unittest.main(verbosity=2)
