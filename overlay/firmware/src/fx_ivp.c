/* SPDX-License-Identifier: GPL-3.0-only */
/* PRE: the INSERT's Surveyor-style preamp, built on the Intersound IVP's signal path (docs/FX-PLAN.md step 4): the input range,
 * the Baxandall BASS / TREBLE stage, the TUBE DRIVE stage and the transistor pair, the master. Fitted to SPICE netlists of the
 * IVP's IC2A and IC6A / Q3 / Q4 / Q5 sections (tools/fxlab/spice/ivp.py; tests/test_ivp.py). The parametric bands stay flat, the
 * transformer is not modelled.
 *   RANGE  LO unity, HI +14.5 dB (the IVP's low-level input);  BASS, TREBLE  first-order shelves (87 Hz, 5.4 kHz), +-15 dB;
 *   GAIN   the TUBE DRIVE pot over R35: 0 .. 5x (audio taper), then the pair: 2.25 V tanh(v / 90 mV), a hard saturation (the
 *          pair's small-signal gain is 25x: anything above a few tens of millivolts is clipped);  LEVEL  the master, audio taper.
 * A voice's full scale is 0.25 V at the input; one volt at the output is 0.3 of full scale. Needs mulq16, clamp. Voltages Q16 up to the pair, Q12 after. */
#ifndef FX_IVP_C
#define FX_IVP_C
#include "fx_ivp_tab.h"

typedef struct { int32_t lb, lt, hp; } ivp_t;
typedef struct { int32_t range, bass, treb, gb, gt, drive, out; } ivp_k_t;

static inline int32_t iv_lp(int32_t *s, int32_t g, int32_t x)
{
    int32_t v = mulq16(x - *s, (uint32_t)g), y = v + *s;
    *s = y + v;
    return y;
}

/* a * c / 4096, c in Q12 up to +-16: mulq16 takes a Q16 factor below 1 (2c as that Q16 factor (below 1 for c up to 5.5 in Q12), then x 8: three bits given up) */
static inline int32_t iv_mul(int32_t a, int32_t c)
{
    int32_t r = mulq16(a, (uint32_t)((c < 0 ? -c : c) * 2)) * 8;
    return c < 0 ? -r : r;
}

/* LEVEL, GAIN 0 .. 127; BASS, TREBLE -64 .. 63; range 0 LO, 1 HI */
static void ivp_coefs(ivp_k_t *k, int32_t level, int32_t gain, int32_t bass, int32_t treble, int32_t range)
{
    int32_t l = clamp(level, 0, 127);
    k->range = range ? IV_RANGE_HI : IV_RANGE_LO;
    k->bass = IV_SHELF[clamp(bass + 64, 0, 127)];
    k->treb = IV_SHELF[clamp(treble + 64, 0, 127)];
    k->gb = IV_GB[clamp(bass + 64, 0, 127)];                         /* (a cut moves the corner: the inverse of a boost) */
    k->gt = IV_GT[clamp(treble + 64, 0, 127)];
    k->drive = IV_DRIVE[clamp(gain, 0, 127)];
    k->out = (l * l * IV_OUT) / (127 * 127);
}

static int32_t ivp_one(ivp_t *s, const ivp_k_t *k, int32_t x)
{
    int32_t v = clamp(x, -262144, 262143) >> 1, u, y;                 /* Q16 volts (a millivolt must keep its bits: the pair is 25x) */
    v = (v * k->range) >> 10;
    v += iv_mul(iv_lp(&s->lb, k->gb, v << 4) >> 4, k->bass);      /* a low shelf: x + (G - 1) lowpass(x) (the 87 Hz filter's
                                                                       * state in Q20: a millivolt moves it by a fraction of a bit) */
    u = v - iv_lp(&s->lt, k->gt, v);                              /* a high shelf: x + (G - 1) highpass(x) */
    v += iv_mul(u, k->treb);
    v = clamp(v, -8000000, 8000000);
    v = ((v >> 4) * k->drive) >> 6;                                   /* IC6A: the TUBE DRIVE pot over R35 */
    u = v < 0 ? -v : v;                                               /* the pair: |v| in Q16 volts, 0 .. 0.5 V in 1/256 V steps */
    u = u >= 32768 ? IV_VSAT : IV_PAIR[u >> 8] + (((IV_PAIR[(u >> 8) + 1] - IV_PAIR[u >> 8]) * (u & 255)) >> 8);
    y = v < 0 ? -u : u;
    y -= iv_lp(&s->hp, IV_G_HP, y);
    return (y * k->out) >> 10;
}
#endif
