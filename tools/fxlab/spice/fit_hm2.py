#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Fits the DSP model's linear parts to the HM-2 SPICE reference (hm2.py): the gain stage's band and the tone stage's bands.
python3 -I fit_hm2.py   (ngspice, numpy)"""
import re
import numpy as np
import hm2

F = np.logspace(np.log10(20), np.log10(20000), 60)


def ac(node, **kw):
    out = hm2.run(hm2.netlist(**kw), f"ac dec 20 20 20k\nprint db(v({node}))")
    pts = []
    for ln in out.splitlines():
        m = re.match(r"^\d+\s+([-\d.e+]+)\s+([-\d.e+]+)", ln)
        if m:
            pts.append((float(m.group(1)), float(m.group(2))))
    a = np.array(pts)
    return a[:, 0], a[:, 1]


def chain_db(f, g, fh1, fh2, fl1, fl2):
    h = lambda fc: 1 / np.sqrt(1 + (fc / f) ** 2)
    l = lambda fc: 1 / np.sqrt(1 + (f / fc) ** 2)
    return g + 20 * np.log10(h(fh1) * h(fh2) * l(fl1) * l(fl2))


def fit_pre(dist):
    f, y = ac("o1", dist=dist)
    best = None
    rng = np.random.default_rng(1)
    p = np.array([40.0, 60.0, 4000.0, 4000.0])
    g = float(np.max(y))
    cost = lambda q, g: np.mean((chain_db(f, g, *q) - y) ** 2)
    for it in range(6000):
        q = p * np.exp(rng.normal(0, 0.05, 4))
        gg = g + rng.normal(0, 0.3)
        if cost(q, gg) < cost(p, g):
            p, g = q, gg
    return g, p, np.sqrt(cost(p, g))


if __name__ == "__main__":
    for d in (0.1, 0.5, 0.9):
        g, p, e = fit_pre(d)
        print(f"dist {d}: gain {g:.1f} dB  HP {p[0]:.0f} {p[1]:.0f} Hz  LP {p[2]:.0f} {p[3]:.0f} Hz  rms error {e:.2f} dB")


def eq_curve(low=0.5, high=0.5):
    out = hm2.run(hm2.netlist(low=low, high=high), "ac dec 20 20 20k\nprint db(v(o3)/v(o2))")
    pts = []
    for ln in out.splitlines():
        m = re.match(r"^\d+\s+([-\d.e+]+)\s+([-\d.e+]+)", ln)
        if m:
            pts.append((float(m.group(1)), float(m.group(2))))
    a = np.array(pts)
    return a[:, 0], a[:, 1]


def peak_db(f, f0, q, gdb):
    """magnitude of an RBJ peaking filter in dB (analog prototype: H = (s^2 + s A w/Q + w^2) / (s^2 + s w/(A Q) + w^2))"""
    a = 10 ** (gdb / 40)
    s = 1j * f / f0
    h = (s * s + s * a / q + 1) / (s * s + s / (a * q) + 1)
    return 20 * np.log10(np.abs(h))


def fit_peaks(f, y, centers, gains_guess):
    """y ~ base + sum of peaking filters at fixed centres: fits base, each gain and Q (shared per filter)"""
    rng = np.random.default_rng(2)
    p = np.array([y[-1] if False else float(np.median(y))] + list(gains_guess) + [1.0] * len(centers))
    n = len(centers)

    def model(q):
        m = q[0] * np.ones_like(f)
        for i, c in enumerate(centers):
            m = m + peak_db(f, c, q[1 + n + i], q[1 + i])
        return m
    cost = lambda q: np.mean((model(q) - y) ** 2)
    for it in range(20000):
        q = p + rng.normal(0, 1, len(p)) * np.array([0.2] + [0.6] * n + [0.05] * n)
        q[1 + n:] = np.clip(q[1 + n:], 0.2, 6)
        if cost(q) < cost(p):
            p = q
    return p, np.sqrt(cost(p))


if __name__ == "__main__" and False:
    pass
