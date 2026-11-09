#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""SPICE reference of the part of the Intersound IVP preamp the Surveyor-style PRE models (AionFX IVP documentation, page 15):
the Baxandall BASS / TREBLE stage (IC2A) and the TUBE DRIVE path (IC6A, then the transistor pair Q3 / Q4 on the tail Q5). The
parametric bands stay at flat, the PREGAIN stage is a fixed gain, the transformer is a 1:1 stand-in (its load a resistor).
  tone_netlist(bass, treble)       0 .. 1, 0.5 flat (pots linear, B taper)
  drive_netlist(drive)             0 .. 1 of the 50kA pot (audio taper: position squared)"""
import os
import re
import subprocess
import tempfile

MODELS = """
.model Q5088 NPN(BF=500 IS=1e-14 VAF=100 RB=40 CJE=15p CJC=4p TF=0.3n)
.model D914 D(IS=2.5n N=1.75 RS=0.6 CJO=4p TT=4n)
.subckt OPA p n o vcc vee
Gm 0 x p n 1m
Rx x 0 100Meg
Cx x 0 53p
Bcl 0 x I=-100*(max(V(x)-(V(vcc)-1.5),0)+min(V(x)-(V(vee)+1.5),0))
Bo o 0 V=min(max(V(x), V(vee)+1.5), V(vcc)-1.5)
Ro2 o 0 1e9
.ends
"""
SUP = "VP vp 0 15\nVN vn 0 -15\n"


def tone_netlist(bass=0.5, treble=0.5, source="AC 1"):
    b = lambda p: (max(1.0, p * 50e3), max(1.0, (1 - p) * 50e3))
    bt, bb = b(bass)
    tt, tb = b(treble)
    return f"""* IVP Baxandall stage (IC2A)
{SUP}Vin in 0 {source}
R16s in n0 1
* the bass pot: pin 3 -> R11 -> node a (input side), pin 1 -> R12 -> node o (output side), wiper -> R13 -> inverting node
R11 n0 b3 5k6
Rbl b3 bw {bt:.6g}
Rbr bw b1 {bb:.6g}
R12 b1 o 5k6
C11 b3 b1 47n
R13 bw inv 220k
* the treble pot: input side R14, output side R15, wiper via C12 to the inverting node
R14 n0 t3 3k3
Rtl t3 tw {tt:.6g}
Rtr tw t1 {tb:.6g}
R15 t1 o 3k3
C12 tw inv 470p
X1 0 inv o vp vn OPA
Rload o 0 20k
{MODELS}"""


def drive_netlist(drive=0.5, source="AC 1", tail=True):
    pot = max(1.0, drive ** 2 * 50e3)
    return f"""* IVP TUBE DRIVE (IC6A) and the transistor pair
{SUP}Vin in 0 {source}
R35 in n6 10k
Rtd n6 o6 {pot:.6g}
C39 n6 o6 22p
X6 0 n6 o6 vp vn OPA
C40 o6 b3 10u
R36 b3 0 4k7
R37 b4 0 10k
Q3 c3 b3 em Q5088
Q4 c4 b4 em Q5088
R38 vp c3 1k1
R39 vp c4 1k1
Q5 em b5 e5 Q5088
R40 e5 vn 430
R42 vp b5 430k
R41 b5 vn 36k
Rload c3 c4 4k
{MODELS}"""


def run(circuit, control, timeout=300):
    with tempfile.TemporaryDirectory() as d:
        p = os.path.join(d, "c.cir")
        with open(p, "w") as fh:
            fh.write(circuit + "\n.control\n" + control + "\n.endc\n.end\n")
        return subprocess.run(["ngspice", "-b", p], capture_output=True, text=True, timeout=timeout, cwd=d).stdout


def ac_db(circuit, node="o"):
    out = run(circuit, f"ac dec 20 20 20k\nprint db(v({node}))")
    res = {}
    for ln in out.splitlines():
        m = re.match(r"^\d+\s+([-\d.e+]+)\s+([-\d.e+]+)", ln)
        if m:
            res[float(m.group(1))] = float(m.group(2))
    return res


def tran(circuit, amp, f=1000.0, cycles=12, settle=8, nodes=("c3",)):
    import numpy as np
    ctl = f"tran {1 / f / 400:.4g} {cycles / f:.6g} {settle / f:.6g}\nwrdata out.txt " + " ".join(f"v({n})" for n in nodes)
    with tempfile.TemporaryDirectory() as d:
        p = os.path.join(d, "c.cir")
        with open(p, "w") as fh:
            fh.write(circuit.replace("AC 1", f"SIN(0 {amp} {f})") + "\n.control\n" + ctl + "\n.endc\n.end\n")
        subprocess.run(["ngspice", "-b", p], capture_output=True, text=True, timeout=600, cwd=d)
        a = np.loadtxt(os.path.join(d, "out.txt"))
    return {n: (a[:, 2 * i], a[:, 2 * i + 1]) for i, n in enumerate(nodes)}


if __name__ == "__main__":
    print(run(drive_netlist(), "op\nprint v(c3) v(c4) v(em) v(b3) v(o6) v(b5)")[-500:])
