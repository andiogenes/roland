#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""SPICE reference of the Harmonic Percolator (AionFX Particle schematic, Albini values, page 5 of its documentation).

  netlist(harm, bal, mode, stock=False, source=...)     harm / bal 0..1 (audio-taper pots, fraction = position squared),
                                                        mode 'GE' (D2 + 4k7, D3), 'SI' (D4 D5 / D6 D7), 'OFF' (no clipping diodes)
Q1 is the PNP germanium, Q2 the NPN silicon, in series (one current): Q2's emitter and Q1's emitter meet at the C3 node, Q1's collector is
the bottom node (R2, R1 back to Q1's base, C5, and C4 to Q2's base), Q2's collector the output node (R4, R3 back to Q2's base)."""
import os
import re
import subprocess
import tempfile

MODELS = """
.model QGE PNP(BF=40 IS=4u VAF=40 RB=120 RE=2 CJE=20p CJC=10p TF=2n)
.model QSI NPN(BF=300 IS=5e-14 VAF=90 RB=60 CJE=15p CJC=4p TF=0.4n)
.model D914 D(IS=2.5n N=1.75 RS=0.6 CJO=4p TT=4n)
.model DGE D(IS=0.3u N=1.3 RS=10 CJO=1p)
"""


def netlist(harm=0.5, bal=0.7, mode="GE", stock=False, source="AC 1", extra=""):
    r1, r2, r3, c4, c5 = (220e3, 20e3, 750e3, 1e-6, 1e-9) if stock else (51e3, 91e3, 3.9e6, 2.2e-6, 1.5e-9)
    f = max(harm, 0.0005) ** 2
    g = max(bal, 0.0005) ** 2
    c = f"""* Harmonic Percolator (Particle schematic)
VA va 0 8.7
Vin in 0 {source}
Rht in w {max(1.0, (1 - f) * 100e3):.6g}
Rhg w 0 {max(1.0, f * 100e3):.6g}
C1 w 0 100p
C2 w b1 47n
R1 b1 e1 {r1:.6g}
R2 e1 0 {r2:.6g}
C5 e1 0 {c5:.6g}
Q1 e1 b1 n1 QGE
C3 n1 0 47u
Q2 vo b2 n1 QSI
R4 va vo 91k
R3 vo b2 {r3:.6g}
C4 b2 e1 {c4:.6g}
C6 vo sig 100n
"""
    if mode == "GE":
        c += "D3 0 sig DGE\nR5 sig m5 4k7\nD2 m5 0 DGE\n"
    elif mode == "SI":
        c += "D6 0 s6 D914\nD7 s6 sig D914\nD5 sig s5 D914\nD4 s5 0 D914\n"
    c += f"""Rbt sig out {max(1.0, (1 - g) * 50e3):.6g}
Rbg out 0 {max(1.0, g * 50e3):.6g}
Rload out 0 1Meg
"""
    return c + MODELS + extra


def run(circuit, control, timeout=300):
    with tempfile.TemporaryDirectory() as d:
        p = os.path.join(d, "c.cir")
        with open(p, "w") as fh:
            fh.write(circuit + "\n.control\n" + control + "\n.endc\n.end\n")
        return subprocess.run(["ngspice", "-b", p], capture_output=True, text=True, timeout=timeout, cwd=d).stdout


def ac_db(node="out", **kw):
    out = run(netlist(**kw), f"ac dec 20 20 20k\nprint db(v({node}))")
    res = {}
    for ln in out.splitlines():
        m = re.match(r"^\d+\s+([-\d.e+]+)\s+([-\d.e+]+)", ln)
        if m:
            res[float(m.group(1))] = float(m.group(2))
    return res


def tran_sine(amp, f=1000.0, cycles=12, settle=8, nodes=("vo", "sig", "out"), **kw):
    import numpy as np
    ctl = f"tran {1 / f / 400:.4g} {cycles / f:.6g} {settle / f:.6g}\nwrdata out.txt " + " ".join(f"v({n})" for n in nodes)
    with tempfile.TemporaryDirectory() as d:
        p = os.path.join(d, "c.cir")
        with open(p, "w") as fh:
            fh.write(netlist(source=f"SIN(0 {amp} {f})", **kw) + "\n.control\n" + ctl + "\n.endc\n.end\n")
        subprocess.run(["ngspice", "-b", p], capture_output=True, text=True, timeout=600, cwd=d)
        a = np.loadtxt(os.path.join(d, "out.txt"))
    return {n: (a[:, 2 * i], a[:, 2 * i + 1]) for i, n in enumerate(nodes)}


if __name__ == "__main__":
    print(run(netlist(), "op\nprint v(vo) v(n1) v(e1) v(b1) v(b2) v(sig)")[-600:])
