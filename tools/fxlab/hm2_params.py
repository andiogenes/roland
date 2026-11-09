# SPDX-License-Identifier: GPL-3.0-only
"""What the HM-2 model keeps of the SPICE netlist (spice/hm2.py, fitted by spice/fit_hm2.py and checked by spice/hm2_model.py).
Pure Python: tools/gen_hm2.py (the firmware's tables) and the float model both read it."""
C3_GAIN_DB = 42.0                       # in -> Q3's collector (c3) at mid band
C3_HP = (109.0, 122.0)                  # Hz, the two coupling highpasses ahead of it
C3_LP = (6678.0, 4386.0)                # Hz, the two stage lowpasses (R6 / C3, R12 / C7)
DIST_PTS = (0.1, 0.5, 0.9)
G3_DB = (5.0, 7.0, 11.9)                # the op-amp stage's gain at those DIST settings
CLIP1 = (4.08, 2.49, 3.0)               # Q3's collector: + limit V, - limit V, knee (higher: sharper)
CLIP2 = (3.48, 2.72, 4.0)               # the op-amp's rails
SERVO_TAU = 0.003                       # s, the first clip's mean held (C6)
C10_HZ = 16.0
GATE_V = 0.10                           # D5 / D6: a dead zone of u - V tanh(u / V)
CLIP3 = (0.485, 0.485, 3.0)             # D8 / D9
POST_LP_HZ = 7960.0                     # R19 + R20, C11
POST_HP_HZ = 20.0
# the tone stage: (centre Hz, Q, share of the pot's dB) for LOW and the two peaks HIGH drives
EQ_LOW = (86.7, 1.2, 1.0)
EQ_MID = (958.0, 1.1, 0.228)
EQ_HIGH = (1278.0, 0.6, 0.958)
EQ_MAX_DB = 19.3
EQ_TAPER = 2.5                          # dB = MAX * sign(u) |u|^TAPER, u = 2 pot - 1
