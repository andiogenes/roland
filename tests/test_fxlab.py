#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""The component values in tools/fxlab/gyrator.py against what the two documents themselves say.   python3 -I tests/test_fxlab.py

A value misread from a schematic would not reproduce the frequencies the documents state in words, so these tests check the
transcription: the HM-2's 86-88 Hz, ~1 kHz and 1.2 kHz poles and the document's own 720 Hz modification; the IVP's four FREQ ranges."""
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "tools"))
from fxlab import gyrator as g  # noqa: E402


class HM2(unittest.TestCase):
    def test_poles_match_the_text(self):
        # "+/-20dB at approximately 88 Hz and 1000 Hz"; "the 86 Hz frequency of the Low knob and the 1.2k frequency of the High
        # knob"; "the two EQ poles are centered ... around 1kHz and 1.2kHz"
        self.assertAlmostEqual(g.hm2_pole_hz("low"), 87.0, delta=2.5)
        self.assertAlmostEqual(g.hm2_pole_hz("mid"), 1000.0, delta=60.0)
        self.assertAlmostEqual(g.hm2_pole_hz("high"), 1200.0, delta=100.0)

    def test_the_documents_own_modification(self):
        # "if you put it down into the 720Hz range ... change R27 to 100k and C18 to 10n"
        self.assertAlmostEqual(g.hm2_pole_hz("mid", ra=100e3, cg=10e-9), 720.0, delta=20.0)


class IVP(unittest.TestCase):
    def test_each_band_covers_the_documented_range(self):
        for name, (lo, hi) in g.IVP_DOC_RANGE.items():
            top, bottom = g.ivp_band_hz(name, 0.0), g.ivp_band_hz(name, 1.0)
            self.assertAlmostEqual(top / hi, 1.0, delta=0.12, msg=f"{name}: top {top:.0f} Hz against {hi}")
            self.assertAlmostEqual(bottom / lo, 1.0, delta=0.08, msg=f"{name}: bottom {bottom:.1f} Hz against {lo}")

    def test_the_bands_overlap_in_order(self):
        tops = [g.ivp_band_hz(n, 0.0) for n in ("low", "low_mid", "hi_mid", "hi")]
        self.assertEqual(tops, sorted(tops))
        self.assertGreater(g.ivp_band_hz("low", 0.0), g.ivp_band_hz("low_mid", 1.0))   # the bands overlap


if __name__ == "__main__":
    unittest.main(verbosity=2)
