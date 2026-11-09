/* SPDX-License-Identifier: GPL-3.0-only */
/* METAL: the INSERT's HM-2 (docs/FX-PLAN.md step 4). A model fitted to a SPICE netlist of the circuit (tools/fxlab/spice/hm2.py,
 * the Graviton schematic; tests/test_hm2.py compares this code with it):
 *   two coupling highpasses and two lowpasses (the first stages), Q3's lopsided clip with its mean held (C6 AC-couples the base:
 *   the long side lasts 38 %, the source of the second harmonic), the op-amp stage (gain 5 .. 12 dB by DIST) into its rails,
 *   the D5 / D6 gate (a dead zone: quiet tails are cut), the D8 / D9 clip, the 8 kHz lowpass, and the three gyrator bands
 *   (LOW 87 Hz, the two HIGH peaks at 958 and 1278 Hz; +-19 dB), then LEVEL (the audio-taper pot).
 * A voice's full scale is 0.25 V at the input; 1 V at the output is 0.65 of full scale (the clip's 0.5 V peaks: a third).
 * Only needs mulq16, clamp; no 64-bit arithmetic. Voltages: Q12 until the tone stage, Q16 in it. */
#ifndef FX_HM2_C
#define FX_HM2_C
#include "fx_hm2_tab.h"

typedef struct {
    int32_t hp1, hp2, lp1, lp2, c10, plp, php;   /* the one-pole states */
    int32_t off;                                  /* clip 1's input offset (the mean follower) */
    struct { int32_t ic1, ic2; } eq[3];
} hm2_t;
typedef struct {
    int32_t g3, lvl;                              /* the op-amp stage's gain Q10; LEVEL Q16 of (x16 -> sample) */
    const int32_t *eq[3];                         /* each band's row: a1, a2, c */
} hm2_k_t;

/* a table lookup with linear interpolation: v >= 0 in Q12 volts, step a power of two */
static inline int32_t hm2_tab(const int16_t *t, int32_t v, int32_t shift, int32_t n)
{
    int32_t i = v >> shift, f = v & ((1 << shift) - 1);
    if (i >= n - 1)
        return t[n - 1];
    return t[i] + (((t[i + 1] - t[i]) * f) >> shift);
}
static inline int32_t hm2_clip(const int16_t *pos, const int16_t *neg, int32_t v)
{
    return v >= 0 ? hm2_tab(pos, v, 9, 129) : -hm2_tab(neg, -v, 9, 129);
}
static inline int32_t hm2_lp(int32_t *s, int32_t g, int32_t x)       /* a one-pole low-pass (topology-preserving) */
{
    int32_t v = mulq16(x - *s, (uint32_t)g), y = v + *s;
    *s = y + v;
    return y;
}

/* DIST, LEVEL, LOW, HIGH: the knobs (DIST, LEVEL 0 .. 127; LOW, HIGH -64 .. 63, 0 flat) */
static void hm2_coefs(hm2_k_t *k, int32_t dist, int32_t level, int32_t low, int32_t high)
{
    int32_t l = clamp(level, 0, 127);
    k->g3 = HM2_G3[clamp(dist, 0, 127)];
    k->lvl = (l * l * 21299) / (127 * 127);                              /* the audio taper, 0.65 of full scale at the top */
    k->eq[0] = &HM2_EQ_LOW[3 * clamp(low + 64, 0, 127)];
    k->eq[1] = &HM2_EQ_MID[3 * clamp(high + 64, 0, 127)];
    k->eq[2] = &HM2_EQ_HIGH[3 * clamp(high + 64, 0, 127)];
}

static int32_t hm2_one(hm2_t *s, const hm2_k_t *k, int32_t x)
{
    static const int32_t EQG[3] = {HM2_EQ_LOW_G, HM2_EQ_MID_G, HM2_EQ_HIGH_G};
    int32_t v = clamp(x, -262144, 262143), u, o, i;
    v = (v * HM2_PRE_GAIN) >> 10;                                        /* Q12 volts at the first stage */
    v -= hm2_lp(&s->hp1, HM2_G_HP1, v);                                  /* the coupling highpasses */
    v -= hm2_lp(&s->hp2, HM2_G_HP2, v);
    v = hm2_lp(&s->lp1, HM2_G_LP1, v);                                   /* the stages' lowpasses */
    v = hm2_lp(&s->lp2, HM2_G_LP2, v);
    v = clamp(v, -4000000, 4000000);
    u = v - s->off;                                                      /* Q3: the lopsided clip, its mean held */
    o = hm2_clip(HM2_CLIP1P, HM2_CLIP1N, u);
    s->off += (o * HM2_SERVO + 32768) >> 16;                             /* (rounded: it must not drift) */
    v = (o * k->g3) >> 10;                                               /* the op-amp stage into its rails */
    v = hm2_clip(HM2_CLIP2P, HM2_CLIP2N, v);
    v -= hm2_lp(&s->c10, HM2_G_C10, v);                                  /* C10 */
    u = v < 0 ? -v : v;                                                  /* D5 / D6: a dead zone */
    if (u >= 2048)
        u -= HM2_GATE_V;
    else
        u = hm2_tab(HM2_GATE, u, 4, 129);
    v = v < 0 ? -u : u;
    u = v < 0 ? -v : v;                                                  /* D8 / D9 */
    u = hm2_tab(HM2_CLIP3, u, 6, 129);
    v = v < 0 ? -u : u;
    v = hm2_lp(&s->plp, HM2_G_POSTLP, v);                                /* R19 + R20, C11 */
    v -= hm2_lp(&s->php, HM2_G_POSTHP, v);
    v <<= 4;                                                             /* the tone stage in Q16 */
    for (i = 0; i < 3; i++) {                                            /* peaking = x + c * (the unity band-pass) */
        const int32_t *r = k->eq[i];
        int32_t v3 = v - s->eq[i].ic2, t = mulq16(v3, (uint32_t)r[1]);
        int32_t v1 = mulq16(s->eq[i].ic1, (uint32_t)r[0]) + t;
        int32_t v2 = s->eq[i].ic2 + mulq16(s->eq[i].ic1, (uint32_t)r[1]) + mulq16(t, (uint32_t)EQG[i]);
        s->eq[i].ic1 = 2 * v1 - s->eq[i].ic1;
        s->eq[i].ic2 = 2 * v2 - s->eq[i].ic2;
        {
            int32_t c = r[2], b = mulq16(v1 << 3, (uint32_t)(c < 0 ? -c : c));   /* (c: Q13, a cut is negative) */
            v += c < 0 ? -b : b;
        }
    }
    return mulq16(v, (uint32_t)k->lvl);
}
#endif
