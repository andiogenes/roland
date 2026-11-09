# SPDX-License-Identifier: GPL-3.0-only
"""What the PRE (Surveyor-style, Intersound IVP family) model keeps of the SPICE netlist (spice/ivp.py) and of the IVP documentation.
Pure Python: tools/fxlab/gen_ivp_tab.py (the firmware's tables) reads it."""
BASS_FC = 87.0              # Hz: a first-order low shelf (spice/ivp.py tone_netlist, fitted to the IC2A Baxandall stage)
TREB_FC = 5426.0            # Hz: a first-order high shelf
SHELF_DB = 15.0             # the documentation's +-15 dB (the netlist's own pots reach +-18 / +-21 dB: scaled to the document)
SHELF_TAPER = 1.3           # dB = SHELF_DB * sign(u) |u|^1.3, u = pot position * 2 - 1 (the netlist: 7.5 dB at a quarter turn of 18)
RANGE_DB = (0.0, 14.5)      # the input range switch: LO, HI (the documentation: the low input level gets +14.5 dB)
DRIVE_MAX = 5.0             # IC6A: the TUBE DRIVE pot (50kA, audio taper: position squared) over R35 10k: 0 .. 5x
PAIR_VSAT = 2.25            # V: the transistor pair's differential output, tanh(v / VK) * VSAT (the netlist: 25x small-signal)
PAIR_VK = 0.09              # V
OUT_V = 0.3                 # full scale per volt at the output
HP_HZ = 20.0                # the coupling capacitors
