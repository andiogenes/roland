#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""The Dragonfly Room port (overlay/firmware/src/fx_dfly.c): the committed tables are the generator's; the fixed-point impulse response against
the float transcription (tools/fxlab/dfroom_float.c, itself checked against Freeverb3 by tests/test_dfroom_float.py) at 22.05 kHz:
decay curve, energy, band spectra, the early reflections; the tail fades to exact silence; SIZE, DECAY and DAMP do what they say; the
pool is big enough for the largest SIZE.   python3 -I tests/test_dfly.py   (needs cc, numpy)"""
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools" / "fxlab"))
import gen_dfly_tab  # noqa: E402

TMP = Path(tempfile.mkdtemp())
subprocess.run(["cc", "-O2", "-o", str(TMP / "dfx"), str(ROOT / "tests/dfly_host.c"), "-lm"], check=True)
subprocess.run(["cc", "-O2", "-DDFR_MAIN", "-o", str(TMP / "dfc"), str(ROOT / "tools/fxlab/dfroom_float.c"), "-lm"], check=True)
FS = 22050
# the Medium Clear Room's knobs: SIZE 12 m (knob 42 of 127: 8 .. 20 m), DECAY 0.4 s, DAMP 0 (the early reflections 10.6 kHz, the late 9.4 kHz as in the preset)
MEDIUM = (42, 20, 0)


def fixed(size, decay, damp):
    out = TMP / f"x_{size}_{decay}_{damp}.raw"
    r = subprocess.run([str(TMP / "dfx"), str(size), str(decay), str(damp), str(out)], check=True, capture_output=True, text=True)
    return np.fromfile(out, dtype=np.float32).reshape(-1, 4), r.stderr


def edc(x):
    e = np.cumsum((x ** 2)[::-1])[::-1]
    return 10 * np.log10(e / e[0] + 1e-12)


def band(x, lo, hi):
    sp = np.abs(np.fft.rfft(x)) ** 2
    f = np.fft.rfftfreq(len(x), 1 / FS)
    return 10 * np.log10(sp[(f >= lo) & (f < hi)].sum() + 1e-20)


class DflyRoom(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        subprocess.run([str(TMP / "dfc"), str(FS), str(TMP / "f.raw")], check=True)
        cls.f = np.fromfile(TMP / "f.raw", dtype=np.float32).reshape(-1, 4)
        cls.x, _ = fixed(*MEDIUM)

    def test_committed_tables_are_the_generators(self):
        self.assertEqual((ROOT / "overlay/firmware/src/fx_dfly_tab.h").read_text(), gen_dfly_tab.text())

    def test_late_energy_and_decay_curve_follow_the_float_transcription(self):
        for ch in (2, 3):
            f, x = self.f[:, ch], self.x[:, ch]
            self.assertAlmostEqual(10 * np.log10((x ** 2).sum() / (f ** 2).sum()), 0.0, delta=1.2)
            ef, ex = edc(f), edc(x)
            for t, tol in ((0.1, 3.0), (0.2, 3.0), (0.4, 4.0)):    # (0.4 s: the fixed-point tail meets its floor a little before the float's; LITE, fx_dfly.c, 3.4 dB)
                self.assertAlmostEqual(ex[int(t * FS)], ef[int(t * FS)], delta=tol, msg=f"ch {ch} t {t}")

    def test_spectrum(self):
        for lo, hi in ((100, 300), (300, 1000), (1000, 3000), (3000, 8000)):
            self.assertAlmostEqual(band(self.x[:, 2], lo, hi), band(self.f[:, 2], lo, hi), delta=1.2, msg=f"{lo}-{hi}")

    def test_early_reflections(self):
        self.assertAlmostEqual(10 * np.log10((self.x[:, 0] ** 2).sum() / (self.f[:, 0] ** 2).sum()), 0.0, delta=1.5)
        i = int(np.argmax(abs(self.f[:, 0])))
        self.assertLessEqual(abs(int(np.argmax(abs(self.x[:, 0]))) - i), 2)            # the same first reflection

    def test_the_tail_ends_in_exact_silence(self):
        self.assertEqual(float(abs(self.x[int(3.0 * FS):, 2:]).max()), 0.0)

    def test_decay_size_and_damp(self):
        def t20(x):
            e = edc(x[:, 2])
            return int(np.argmax(e < -20)) / FS
        short, long_ = fixed(42, 5, 0)[0], fixed(42, 60, 0)[0]
        self.assertLess(t20(short), t20(long_))
        small, big = fixed(0, 20, 0)[0], fixed(127, 20, 0)[0]
        self.assertLess(int(np.argmax(abs(small[:, 0]) > 0.01)), int(np.argmax(abs(big[:, 0]) > 0.01)) + 1)   # the first reflection comes later
        bright, dark = fixed(42, 20, 0)[0], fixed(42, 20, 127)[0]
        # the damping filters sit in front of the tank: what they take is the highs of the first, direct echoes (the tail is mostly the bass-boosted loop)
        tilt = lambda x: band(x[:, 2], 3000, 8000) - band(x[:, 2], 300, 1000)
        self.assertGreater(tilt(bright), tilt(dark) + 3.0)

    def test_the_pool_holds_the_largest_size(self):
        _, err = fixed(127, 20, 0)
        used, total = [int(v) for v in err.split("pool used ")[1].split(" samples")[0].split(" of ")]
        self.assertLessEqual(used, total)


if __name__ == "__main__":
    unittest.main(verbosity=2)
