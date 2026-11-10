/* SPDX-License-Identifier: GPL-3.0-only */
/* HARM: the INSERT's Harmonic Percolator, Albini values (docs/FX-PLAN.md step 4). A model fitted to a SPICE netlist of the circuit
 * (tools/fxlab/spice/perc.py, the Particle schematic; tests/test_perc.py):
 *   HARMONICS  the pot is a series resistor into a low input impedance: a pass-band gain from -60 to +40 dB and a low cut from
 *              about 125 Hz (3/4 up) to 900 Hz (the ends), one table row a knob position (the netlist's small-signal response);
 *   the collector the two transistors' lopsided clip (+2 V, -1 V about rest) with its mean followed (the C4 coupling rectifies);
 *   C6         a 32 Hz low cut;
 *   MODE       GE: D3 and D2 + 4k7 (a soft clip a few tenths of a volt), OFF: none, SI: two 1N914 each way (about +-1 V);
 *   BALANCE    the output pot, an audio taper.
 * A voice's full scale is 0.25 V in; 1 V out is 0.4 of full scale. Needs mulq16, clamp. Voltages Q12. */
#ifndef FX_PERC_C
#define FX_PERC_C
#include "fx_perc_tab.h"

typedef struct { int32_t hp, off, c6; } perc_t;
typedef struct { int32_t gain, hpg, out; const int16_t *mp, *mn; } perc_k_t;

static inline int32_t pc_tab(const int16_t *t, int32_t v, int32_t shift)       /* v >= 0, 129 entries */
{
    int32_t i = v >> shift, f = v & ((1 << shift) - 1);
    if (i >= 128)
        return t[128];
    return t[i] + (((t[i + 1] - t[i]) * f) >> shift);
}
static inline int32_t pc_lp(int32_t *s, int32_t g, int32_t x)
{
    int32_t v = mulq16(x - *s, (uint32_t)g), y = v + *s;
    *s = y + v;
    return y;
}

/* HARM, BAL 0 .. 127; mode 0 GE, 1 OFF, 2 SI */
static void perc_coefs(perc_k_t *k, int32_t harm, int32_t bal, int32_t mode)
{
    int32_t h = clamp(harm, 0, 127), b = clamp(bal, 0, 127);
    static const int16_t *const NONE = 0;
    k->gain = PC_GAIN[h];
    k->hpg = PC_HPG[h];
    k->out = (b * b * 3277) / (127 * 127);                                    /* 1 V = 0.4 of full scale: Q12 volts x 3.2 */
    k->mp = mode == 0 ? PC_GEP : mode == 2 ? PC_SIP : NONE;
    k->mn = mode == 0 ? PC_GEN : mode == 2 ? PC_SIN : NONE;
}

static int32_t perc_one(perc_t *s, const perc_k_t *k, int32_t x)
{
    int32_t v = clamp(x, -262144, 262143), u, o;
    v = ((v >> 2) * (k->gain >> 2)) >> 11;                                    /* Q12 volts after the gain (the input's bits kept) */
    v -= pc_lp(&s->hp, k->hpg, v);
    v = clamp(v, -2097152, 2097151);                           /* (not +-4000000: the build refuses an immediate in the chip ROM range 0xFFC00000 .. 0xFFCFFFFF, and -4000000 is 0xFFC2F700) */
    u = v - s->off;                                                           /* the transistors' lopsided clip, its mean followed */
    o = u >= 0 ? pc_tab(PC_C1P, u, 8) : -pc_tab(PC_C1N, -u, 8);
    s->off += (o * PC_SERVO + 32768) >> 16;
    o -= pc_lp(&s->c6, PC_G_C6, o);                                           /* C6 */
    if (k->mp) {                                                              /* the clipping diodes */
        u = o < 0 ? -o : o;
        u = pc_tab(o >= 0 ? k->mp : k->mn, u, 6);
        o = o < 0 ? -u : u;
    }
    return (o * k->out) >> 10;
}
#endif
