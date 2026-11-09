#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""tools/fxlab/dfroom_float.c (the float transcription of the Dragonfly Room) against Freeverb3's own impulse response
(tests/dfroom_ref.sh, a clone of dragonfly-reverb: skipped when it is not there).   python3 -I tests/test_dfroom_float.py"""
import os
import subprocess
import tempfile
import unittest
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parent.parent
CLONE = Path(os.environ.get("DRAGONFLY_DIR", "/home/user/michaelwillis/dragonfly-reverb"))


def edc_db(x):
    e = np.cumsum((x ** 2)[::-1])[::-1]
    return 10 * np.log10(e / e[0] + 1e-12)


@unittest.skipUnless((CLONE / "common/freeverb/progenitor2.cpp").exists(), "no clone of dragonfly-reverb (DRAGONFLY_DIR)")
class DfRoomFloat(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = Path(tempfile.mkdtemp())
        subprocess.run(["sh", str(ROOT / "tests/dfroom_ref.sh"), str(cls.tmp)], check=True, capture_output=True,
                       env=dict(os.environ, DRAGONFLY_DIR=str(CLONE)))
        subprocess.run(["cc", "-O2", "-DDFR_MAIN", "-o", str(cls.tmp / "dfc"), str(ROOT / "tools/fxlab/dfroom_float.c"), "-lm"], check=True)

    def ir(self, fs):
        tag = {44100: "44", 22050: "22"}[fs]
        subprocess.run([str(self.tmp / "dfc"), str(fs), str(self.tmp / f"c{tag}.raw")], check=True)
        r = np.fromfile(self.tmp / f"ir{tag}.raw", dtype=np.float32).reshape(-1, 4)
        m = np.fromfile(self.tmp / f"c{tag}.raw", dtype=np.float32).reshape(-1, 4)
        return r, m

    def test_late_decay_curve_and_energy(self):
        for fs in (44100, 22050):
            r, m = self.ir(fs)
            for ch in (2, 3):
                self.assertAlmostEqual(10 * np.log10((m[:, ch] ** 2).sum() / (r[:, ch] ** 2).sum()), 0.0, delta=0.6)
                er, em = edc_db(r[:, ch]), edc_db(m[:, ch])
                for t in (0.1, 0.2, 0.4, 0.8):
                    self.assertAlmostEqual(em[int(t * fs)], er[int(t * fs)], delta=1.0, msg=f"fs {fs} ch {ch} t {t}")

    def test_late_spectrum(self):
        for fs in (44100, 22050):
            r, m = self.ir(fs)
            for lo, hi in ((100, 300), (300, 1000), (1000, 3000), (3000, 8000)):
                def band(x):
                    sp = np.abs(np.fft.rfft(x)) ** 2
                    f = np.fft.rfftfreq(len(x), 1 / fs)
                    return 10 * np.log10(sp[(f >= lo) & (f < hi)].sum() + 1e-20)
                self.assertAlmostEqual(band(m[:, 2]), band(r[:, 2]), delta=1.0, msg=f"fs {fs} {lo}-{hi}")

    def test_early_reflections_energy(self):
        for fs in (44100, 22050):
            r, m = self.ir(fs)
            self.assertAlmostEqual(10 * np.log10((m[:, 0] ** 2).sum() / (r[:, 0] ** 2).sum()), 0.0, delta=0.6)


if __name__ == "__main__":
    unittest.main(verbosity=2)
