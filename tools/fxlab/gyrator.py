# SPDX-License-Identifier: GPL-3.0-only
"""Reference calculations for the effects pool (docs/FX-PLAN.md), step 2.

The component values below are the ones printed in the AionFX documentation the project owner supplied (the Graviton, a clone of
the BOSS HM-2, v1.1.1, and the IVP Preamp, a clone of the Intersound IVP, v2.0.0). They are facts about the circuits, kept here
with their part numbers so each can be checked against the document. The PDFs themselves are not in this repository.

A gyrator (an op-amp follower with two resistors and a capacitor) behaves as an inductor L = C * Ra * Rb, which with a series
capacitor Cs makes the resonance the equalisers of both devices are built on:  f = 1 / (2 pi sqrt(L Cs)).
"""
import math


def gyrator_L(c, ra, rb):
    """the inductance a gyrator makes, henry"""
    return c * ra * rb


def resonance(l, cs):
    """the series resonance of an inductance l with a capacitor cs, Hz"""
    return 1.0 / (2.0 * math.pi * math.sqrt(l * cs))


# ---------------------------------------------------------------------------- BOSS HM-2 (AionFX Graviton parts list, p.3-5)
# per tone pole: (gyrator capacitor, Ra, Rb, series capacitor)  - part numbers in the comments
HM2_POLES = {
    "low":  dict(cg=68e-9, ra=100e3, rb=330.0, cs=1.5e-6),    # C16, R25, R26, C15
    "mid":  dict(cg=6.8e-9, ra=82e3, rb=330.0, cs=150e-9),    # C18, R27, R28, C17   (tied to the High pot, with:)
    "high": dict(cg=4.7e-9, ra=100e3, rb=330.0, cs=100e-9),   # C20, R29, R30, C19
}


def hm2_pole_hz(name, **over):
    p = dict(HM2_POLES[name], **over)
    return resonance(gyrator_L(p["cg"], p["ra"], p["rb"]), p["cs"])


# -------------------------------------------------------------------------------- Intersound IVP (AionFX IVP parts list, p.7-12)
# the four parametric bands: gyrator capacitor, series capacitor (the document's scaling doubles the capacitors by putting a
# second one in parallel, p.3), Ra (820R), and Rb = 13k7 + the FREQ pot (1MC, 0 .. 1 M; the original's 2MC at half the resistors)
IVP_BANDS = {
    "low":     dict(cg=68e-9, cs=470e-9, ra=820.0),               # C16, C15, R17
    "low_mid": dict(cg=2 * 6.8e-9, cs=2 * 120e-9, ra=820.0),      # C20 || C21, C18 || C19, R19
    "hi_mid":  dict(cg=2 * 1.2e-9, cs=68e-9, ra=820.0),           # C25 || C26, C24, R21
    "hi":      dict(cg=2 * 820e-12, cs=2 * 6.8e-9, ra=820.0),     # C29 || C30, C27 || C28, R23
}
IVP_RB_FIXED = 13.7e3                                              # R18, R20, R22, R24
IVP_FREQ_POT = 1e6
# what the document says each FREQ knob covers (Usage, p.5), Hz
IVP_DOC_RANGE = {"low": (30.0, 240.0), "low_mid": (100.0, 800.0), "hi_mid": (460.0, 3600.0), "hi": (1200.0, 9600.0)}


def ivp_band_hz(name, pot=0.0):
    """the band's centre with the FREQ pot at `pot` (0 = the top of the range .. 1 = the bottom, linear in resistance)"""
    b = IVP_BANDS[name]
    return resonance(gyrator_L(b["cg"], b["ra"], IVP_RB_FIXED + pot * IVP_FREQ_POT), b["cs"])
