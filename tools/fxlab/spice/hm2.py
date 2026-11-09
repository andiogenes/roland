#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""SPICE reference of the HM-2 (Graviton schematic, page 7 of its documentation): netlist builder and ngspice runner.

  hm2.netlist(dist, level, low, high, flip=False, source=...) -> text

The distortion pot is wired as drawn: pin 2 (the wiper) to ground, pin 1 to C5 (Q2's emitter branch), pin 3 to R18 (the op-amp's
gain leg). `dist` 0..1; pin 1 -> wiper is dist * 250k unless flip. Low / High are the 10kG pots, 0 = cut, 0.5 flat, 1 = boost
(wiper at pin 3, the inverting node). The MID pot is not fitted: the mid gyrator hangs on High's wiper, as the document says."""
import os
import re
import subprocess
import tempfile

MODELS = """
.model JN NJF(VTO=-1.5 BETA=1.2m LAMBDA=0.02 CGS=4p CGD=3p)
.model QN NPN(BF=350 IS=5e-14 VAF=100 RB=50 CJE=15p CJC=4p TF=0.3n)
.model QP PNP(BF=250 IS=1e-14 VAF=80 RB=60 CJE=15p CJC=5p TF=0.3n)
.model D914 D(IS=2.5n N=1.75 RS=0.6 CJO=4p TT=4n)
.model D34 D(IS=0.2u N=1.3 RS=5 CJO=1p)
.model BAT D(IS=2u N=1.05 RS=5 CJO=10p)
.subckt OPA p n o vcc
* a 4558: 3 MHz, rails at ~1 V inside the supply, one pole; the pole's node held inside the rails (no integrator wind-up in clipping)
Gm 0 x p n 1m
Bcl 0 x I=-100*(max(V(x)-7.6,0)+min(V(x)-0.6,0))
Rx x 0 100Meg
Cx x 0 53p
Bo o 0 V=min(max(V(x), 1.0), V(vcc)-1.5)
Ro2 o 0 1e9
.ends
"""


def netlist(dist=0.5, level=0.5, low=0.5, high=0.5, flip=False, source="AC 1", ac=True, tran=None, probe="o3", extra=""):
    x = (1.0 - dist) if flip else dist
    pl = lambda b: (max(1.0, b * 10e3), max(1.0, (1 - b) * 10e3))         # pin 1 -> wiper, wiper -> pin 3
    lt, lb = pl(low)
    ht, hb = pl(high)
    lv_top, lv_bot = max(1.0, level * 10e3), max(1.0, (1 - level) * 10e3)
    c = f"""* HM-2 (Graviton schematic)
VA va 0 8.7
R31 va vb 10k
R32 vb 0 10k
C24 vb 0 47u
Vin in 0 {source}
RPD in 0 2.2Meg
R1 in n1 10k
C1 n1 g1 47n
R2 vb g1 1Meg
J1 va g1 s1 JN
R3 s1 0 10k
C2 s1 n2 47n
R4 n2 b2 22k
R5 b2 0 100k
Q2 c2 b2 e2 QN
R6 b2 c2 470k
C3 b2 c2 100p
R7 vr c2 10k
R14 va vr 1k
C4 vr 0 47u
R8 e2 0 22
R9 e2 m1 150
C5 m1 p1 10u
Rp1 p1 0 {max(1.0, x * 250e3):.6g}
Rp3 p3 0 {max(1.0, (1 - x) * 250e3):.6g}
R18 p3 m2 47k
C9 m2 inv 47n
C6 c2 q1 47n
R10 q1 b3 22k
R11 vr b3 100k
R12 b3 c3 470k
C7 b3 c3 100p
Q3 c3 b3 e3 QP
R13 vr e3 120
R16 va c3 68k
R15 c3 0 10k
X1 c3 inv o1 va OPA
R17 inv o1 220k
C8 inv o1 100p
D2 o1 inv D914
D3 inv k1 D914
D4 k1 o1 D914
C10 o1 r1 1u
R19 r1 g5 10k
D5 g5 g6 D34
D6 g6 g5 D34
R20 g6 n7 10k
D8 n7 0 D914
D9 0 n7 D914
C11 n7 0 1n
C12 n7 e1 1u
R21 vb e1 68k
X2 e1 o2 o2 va OPA
R22 o2 ta 3k3
RLt ta wl {lt:.6g}
RLb wl tb {lb:.6g}
RHt ta wh {ht:.6g}
RHb wh tb {hb:.6g}
* gyrators: wiper - Cs - X, X - Cg - vp, Rbias vb - vp, Rb X - buffer(vp)
C15 wl xl 1.5u
C16 xl pl 68n
R25 vb pl 100k
R26 xl bl 330
X3 pl bl bl va OPA
C17 wh xm 150n
C18 xm pm 6n8
R27 vb pm 82k
R28 xm bm 330
X4 pm bm bm va OPA
C19 wh xh 100n
C20 xh ph 4n7
R29 vb ph 100k
R30 xh bh 330
X5 ph bh bh va OPA
X6 ta tb o3 va OPA
R23 tb o3 3k3
C13 tb o3 470p
C14 o3 v1 10u
R24 v1 vo 10k
Rv1 vo vv {lv_top:.6g}
Rv2 vv 0 {lv_bot:.6g}
Rload vo 0 100k
"""
    c = c.replace("Rv1 vo vv", "Rv1 vo vv").replace("Rv2 vv 0", "Rv2 vv 0")
    return c + MODELS + extra


def run(circuit, control, timeout=300):
    """ngspice -b on circuit + a .control block; returns the stdout"""
    with tempfile.TemporaryDirectory() as d:
        p = os.path.join(d, "c.cir")
        with open(p, "w") as f:
            f.write(circuit + "\n.control\n" + control + "\n.endc\n.end\n")
        return subprocess.run(["ngspice", "-b", p], capture_output=True, text=True, timeout=timeout, cwd=d).stdout


def ac_gain_db(freqs, node="o3", **kw):
    ctl = "ac dec 20 20 20k\nprint db(v(%s))\n" % node
    out = run(netlist(**kw), ctl)
    res = {}
    for ln in out.splitlines():
        m = re.match(r"^\d+\s+([-\d.e+]+)\s+([-\d.e+]+)", ln)
        if m:
            res[float(m.group(1))] = float(m.group(2))
    return res


if __name__ == "__main__":
    import sys
    out = run(netlist(dist=0.5), "op\nprint v(c2) v(c3) v(o1) v(n7) v(o3) v(vb)")
    print(out[-1500:])


def tran_sine(amp, f=1000.0, cycles=12, settle=8, nodes=("o1", "n7", "o3"), **kw):
    """transient of a sine of `amp` volts peak; returns {node: (t[], v[])} for the last cycles - settle cycles"""
    import numpy as np
    tstop = cycles / f
    ctl = (f"tran {1 / f / 400:.4g} {tstop:.6g} {settle / f:.6g}\nwrdata out.txt " + " ".join(f"v({n})" for n in nodes))
    with tempfile.TemporaryDirectory() as d:
        p = os.path.join(d, "c.cir")
        with open(p, "w") as fh:
            fh.write(netlist(source=f"SIN(0 {amp} {f})", **kw) + "\n.control\n" + ctl + "\n.endc\n.end\n")
        subprocess.run(["ngspice", "-b", p], capture_output=True, text=True, timeout=600, cwd=d)
        a = np.loadtxt(os.path.join(d, "out.txt"))
    res = {}
    for i, n in enumerate(nodes):
        res[n] = (a[:, 2 * i], a[:, 2 * i + 1])
    return res


def harmonics(t, v, f=1000.0, n=6):
    """amplitudes of harmonics 1..n of a signal sampled at t (resampled uniformly over whole cycles)"""
    import numpy as np
    k = int((t[-1] - t[0]) * f)
    T = k / f
    tt = np.linspace(t[0], t[0] + T, 4096, endpoint=False)
    vv = np.interp(tt, t, v)
    sp = np.abs(np.fft.rfft(vv)) * 2 / len(vv)
    return [sp[k * h] for h in range(1, n + 1)], vv.mean()
