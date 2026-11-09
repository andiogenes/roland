# SPDX-License-Identifier: GPL-3.0-only
"""What the Harmonic Percolator model keeps of the SPICE netlist (spice/perc.py, Albini values; spice/fit_perc.py): the gain stage's
pass-band gain and low-cut corner at 33 positions of the HARMONICS pot (spice/perc_gain.json, the netlist's small-signal response),
and the fitted shapers. Pure Python: tools/gen_perc_tab.py and the float model read it."""
POT = [i / 32 for i in range(33)]
PASS_DB = [-59.5, -19.83, -8.2, -1.92, 1.93, 4.34, 5.86, 6.85, 7.55, 8.08, 8.51, 8.9, 9.26, 9.62, 9.98, 10.36, 10.75, 11.17, 11.63,
           12.13, 12.67, 13.28, 13.95, 14.7, 15.56, 16.56, 17.72, 19.11, 20.83, 23.05, 26.16, 31.18, 40.5]
HP_HZ = [926.5, 917.4, 888.0, 834.7, 758.8, 668.8, 576.8, 492.4, 419.9, 360.0, 311.3, 272.0, 240.3, 214.6, 193.8, 176.9, 163.2, 152.1,
         143.3, 136.3, 131.1, 127.6, 125.6, 125.4, 127.1, 131.2, 138.5, 150.4, 169.8, 203.1, 267.3, 424.1, 926.5]
POT_TAPER = 2.0                         # audio taper: the resistance fraction is the position squared (BALANCE and HARMONICS)
# the fitted shapers (volts at the circuit): Q2's collector clip (+ limit, - limit, knee, mean follower tau s) and each MODE's
FIT = dict(c1=(2.0, 1.0, 2.5, 0.001), GE=(0.05, 0.453, 1.5), SI=(0.665, 1.057, 6.0))
C6_HZ = 32.0
