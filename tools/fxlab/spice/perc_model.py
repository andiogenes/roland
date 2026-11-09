#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Float model of the Harmonic Percolator (what fx_perc.c does in fixed point) and its fit to spice/perc_spice_grid.json (the netlist,
perc.py, harmonics of a 1 kHz sine: mode | HARMONICS | volts in -> h1..h4 volts at the output)."""
import json
import os
import sys
import numpy as np

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
import perc_params as P  # noqa: E402

FS = 44100.0


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


def perc(x, harm, mode, bal=1.0, fit=None):
    fit = fit or P.FIT
    pg = np.interp(harm, P.POT, P.PASS_DB)
    fc = float(np.interp(harm, P.POT, P.HP_HZ))
    v = onepole(x * 10 ** (pg / 20), fc, True)
    pl, nl, kn, tau = fit["c1"]
    k = 1.0 / (tau * FS)
    o = np.zeros_like(v)
    off = 0.0
    for i, s in enumerate(v):
        y = float(railclip(np.array([s - off]), pl, nl, kn)[0])
        off += k * y
        o[i] = y
    o = onepole(o, P.C6_HZ, True)
    if mode != "OFF":
        a, b, kk = fit[mode]
        o = railclip(o, a, b, kk)
    return o * bal ** P.POT_TAPER


def harm4(y, f=1000.0):
    n = int(len(y) * f / FS)
    seg = y[: int(n * FS / f)]
    sp = np.abs(np.fft.rfft(seg)) * 2 / len(seg)
    kk = int(round(len(seg) * f / FS))
    return [sp[kk * h] for h in range(1, 5)]


def sim(amp, harm, mode, fit=None):
    t = np.arange(int(FS * 0.06)) / FS
    y = perc(amp * np.sin(2 * np.pi * 1000 * t), harm, mode, 1.0, fit)
    return harm4(y[int(FS * 0.02):])


def load():
    return {tuple(k.split("|")): v for k, v in json.load(open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "perc_spice_grid.json"))).items()}


def cost(fit, grid, amps=("0.003", "0.03", "0.3")):
    c = 0.0
    for (mode, h, a), ref in grid.items():
        if a not in amps:
            continue
        m = sim(float(a), float(h), mode, fit)
        if ref[0] > 1e-4 and m[0] > 1e-6:
            c += (20 * np.log10(m[0] / ref[0])) ** 2
            c += 60 * ((m[1] / m[0]) - (ref[1] / ref[0])) ** 2 + 60 * ((m[2] / m[0]) - (ref[2] / ref[0])) ** 2
        else:
            c += 25
    return c


if __name__ == "__main__":
    grid = load()
    rng = np.random.default_rng(7)
    fit = {k: tuple(v) for k, v in P.FIT.items()}
    fit.update(c1=(2.0, 1.0, 2.5, 0.001), GE=(0.3, 0.4, 2.0), SI=(1.2, 1.2, 3.0))
    best = cost(fit, grid)
    print("start", round(best, 1))
    for it in range(int(sys.argv[1]) if len(sys.argv) > 1 else 300):
        k = rng.choice(list(fit))
        v = np.array(fit[k]) * np.exp(rng.normal(0, 0.12, len(fit[k])))
        v = np.clip(v, 0.05, 6.0)                                # physical: limits of a fraction of a volt to a few, knees 1.5 .. 6
        if k == "c1":
            v[2] = np.clip(v[2], 1.5, 6.0); v[3] = np.clip(v[3], 0.0003, 0.01)
        else:
            v[2] = np.clip(v[2], 1.5, 6.0)
        cand = dict(fit)
        cand[k] = tuple(float(x) for x in v)
        c = cost(cand, grid)
        if c < best:
            best, fit = c, cand
    print("best", round(best, 1), {k: tuple(round(x, 3) for x in v) for k, v in fit.items()})
    print("(mode, harm, amp): spice h1 h2/h1 h3/h1 | model")
    for (mode, h, a), ref in sorted(grid.items()):
        if a in ("0.003", "0.03", "0.3") and h in ("0.6", "1.0"):
            m = sim(float(a), float(h), mode, fit)
            print(mode, h, a, "| %.3f %.2f %.2f | %.3f %.2f %.2f" % (ref[0], ref[1] / ref[0], ref[2] / ref[0], m[0], m[1] / m[0] if m[0] else 0, m[2] / m[0] if m[0] else 0))
