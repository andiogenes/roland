#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Float reference of the DSP model of the HM-2 (what overlay/firmware/src/fx_hm2.c does in fixed point), and its fit to the SPICE
netlist (hm2.py): gain stage band and gain, the rail clip, the D5 / D6 gate, the D8 / D9 clip, the tone stage's peaks.
Voltages are volts at the pedal; the firmware maps a voice's full scale to 0.25 V in and 1 V out."""
import json
import numpy as np

FS = 44100.0
import os, sys
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
from hm2_params import *  # noqa: E402,F401  (the fitted numbers: one place for the model and the firmware's tables)
PARAMS = dict(p1=CLIP1[0], n1=CLIP1[1], k1=CLIP1[2], p2=CLIP2[0], n2=CLIP2[1], k2=CLIP2[2], vg=GATE_V, lc=CLIP3[0], kc=CLIP3[2])


def interp(d, ys):
    return float(np.interp(d, DIST_PTS, ys))


def onepole(x, fc, hp):
    k = np.tan(np.pi * fc / FS)
    g = k / (1 + k)
    y = np.zeros_like(x)
    s = 0.0
    for i, v in enumerate(x):
        u = (v - s) * g
        lp = u + s
        s = lp + u
        y[i] = v - lp if hp else lp
    return y


def railclip(v, lp, ln, knee):
    L = np.where(v >= 0, lp, ln)
    return v / (1 + np.abs(v / L) ** knee) ** (1.0 / knee)


def servo_clip(v, lp, ln, knee, k):
    """the clip with its input's offset following the output's mean: the base is AC coupled (C6), so the collector's mean stays
    at its bias and a lopsided clip spends 38 % of the time on the long side, not 50 %"""
    o = np.zeros_like(v)
    off = 0.0
    for i, x in enumerate(v):
        y = float(railclip(np.array([x - off]), lp, ln, knee)[0])
        off += k * y
        o[i] = y
    return o


def gate(u, vg):
    return u - vg * np.tanh(u / vg)


def peak_sos(f0, q, gdb):
    a = 10 ** (gdb / 40)
    w = 2 * np.pi * f0 / FS
    al = np.sin(w) / (2 * q)
    c = np.cos(w)
    b = np.array([1 + al * a, -2 * c, 1 - al * a])
    ad = np.array([1 + al / a, -2 * c, 1 - al / a])
    return b / ad[0], ad / ad[0]


def biquad(x, b, a):
    y = np.zeros_like(x)
    z1 = z2 = 0.0
    for i, v in enumerate(x):
        o = b[0] * v + z1
        z1 = b[1] * v - a[1] * o + z2
        z2 = b[2] * v - a[2] * o
        y[i] = o
    return y


def taper(b):
    u = 2 * b - 1
    return EQ_MAX_DB * np.sign(u) * abs(u) ** EQ_TAPER


def hm2(x_volts, dist, low=0.5, high=0.5, p=PARAMS):
    v = x_volts * 10 ** (C3_GAIN_DB / 20)
    for fc in C3_HP:
        v = onepole(v, fc, True)
    for fc in C3_LP:
        v = onepole(v, fc, False)
    v = servo_clip(v, p["p1"], p["n1"], p["k1"], 1.0 / (0.006 * FS))   # Q3's collector: the first, lopsided clip, its mean held
    v = v * 10 ** (interp(dist, G3_DB) / 20)
    v = railclip(v, p["p2"], p["n2"], p["k2"])               # the op-amp's rails
    v = onepole(v, C10_HZ, True)                               # C10
    v = gate(v, p["vg"])                                     # D5 / D6
    v = railclip(v, p["lc"], p["lc"], p["kc"])               # D8 / D9
    v = onepole(v, POST_LP_HZ, False)                            # R19 + R20, C11
    v = onepole(v, POST_HP_HZ, True)
    for (f0, q, share), pot in ((EQ_LOW, low), (EQ_MID, high), (EQ_HIGH, high)):
        b, a = peak_sos(f0, q, share * taper(pot))
        v = biquad(v, b, a)
    return v


def harm(y, f=1000.0, n=4):
    k = int(len(y) * f / FS)
    seg = y[: int(k * FS / f)]
    sp = np.abs(np.fft.rfft(seg * 1.0)) * 2 / len(seg)
    kk = int(round(len(seg) * f / FS))
    return [sp[kk * h] for h in range(1, n + 1)]


def run_sine(amp, dist, f=1000.0, **kw):
    t = np.arange(int(FS * 0.3)) / FS
    x = amp * np.sin(2 * np.pi * f * t)
    y = hm2(x, dist, **kw)
    return y[int(FS * 0.1):]


if __name__ == "__main__":
    for d in (0.1, 0.5, 0.9):
        for amp in (0.001, 0.01, 0.1, 0.5):
            y = run_sine(amp, d)
            h = harm(y)
            print(f"dist {d} amp {amp}: h1 {h[0]:.3f} h2/h1 {h[1] / h[0]:.2f} h3/h1 {h[2] / h[0]:.2f}")
