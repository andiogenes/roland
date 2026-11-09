/* SPDX-License-Identifier: GPL-3.0-only */
/* The Dragonfly Room (docs/FX-PLAN.md step 5): Freeverb3's early reflections and Progenitor2 reverb as Dragonfly Reverb drives them,
 * the Medium Clear Room preset, in fixed point at 22.05 kHz. Written from reading
 *   Dragonfly Reverb (GPL-3.0, (c) 2019 Michael Willis, Rob van den Berg), Freeverb3 (GPL-2+, (c) 2006-2018 Teru Kamogashira) and
 *   the Progenitor of David Griesinger,
 * checked against a float transcription (tools/fxlab/dfroom_float.c, in turn checked against Freeverb3's own impulse response:
 * tests/test_dfroom_float.py, tests/test_dfly.py). The same network: 20 modulated input all-passes and 8 cross all-passes, the
 * tank of 4 modulated all-passes, 2 two-section and 2 three-section all-passes and 8 delays with its 40 output taps and the feed-forward
 * combs, the early reflections' 6 taps a side. SIZE scales every length (the primes are found at run time), DECAY sets the RT60
 * through four decay factors, DAMP the damping filters.
 * Samples are Q13 (8192 = a full-scale input) in int16 buffers with headroom of 4; coefficients Q15 (filters Q14). No 64-bit
 * arithmetic. Needs mulq15, mulq16, clamp, osc_sine, and fx_comp.c's cm_log2 / cm_exp2. */
#ifndef FX_DFLY_C
#define FX_DFLY_C
#include "fx_comp.c"
#include "fx_dfly_tab.h"

#define DFR_POOL 52800                      /* int16 samples: SIZE up to 24 m (gen_dfly_tab: 52 800 at 24) */

typedef struct { int16_t *b; uint16_t n, i; } dfd_t;                                  /* a delay or an all-pass buffer */
typedef struct { int16_t *b; uint16_t n, rd, wr, modn; int32_t fb, dec, z1; } dfm_t;   /* modulated all-pass */
typedef struct { dfd_t a1, a2; int16_t f1, f2, d1, d2; } dfa2_t;
typedef struct { int16_t *b1, *b2, *b3; uint16_t n1, n2, n3, rd1, wr1, i2, i3, modn; int16_t f1, f2, f3, d1, d2, d3; } dfa3_t;
typedef struct { int32_t y1; } dfi_t;
typedef struct { int32_t i1, i2, o1, o2; } dfq_t;

#ifdef DFR_DBG
int32_t dfr_dbg[8];
#endif
/* a * b / 32768 truncated toward zero: symmetric (no bias: the all-passes' loops have unity gain at DC, a floored product would build an
 * offset) and never larger than the exact one, so the tails fade to exact silence (no limit cycle) */
static inline int32_t dfr_mr(int32_t a, int32_t b) { int32_t p = a * b; return (p + (p >> 31 & 32767)) >> 15; }
static int16_t dfr_pool[DFR_POOL] __attribute__((section(".pool")));
static struct {
    int16_t *p;                              /* the next free sample of the pool */
    uint8_t ok;                              /* set up (and for which SIZE: sz) */
    int32_t sz, rt, dp;                      /* the knobs last set up for: SIZE, DECAY, DAMP */
    /* early reflections */
    dfd_t lineL, lineR, ltr, rtl;
    uint16_t nt, tapL[6], tapR[6];
    dfq_t axL, axR, a2L, a2R;
    dfi_t eoL, eoR;
    /* late reverb */
    dfm_t iaL[10], iaR[10];
    dfd_t caL[4], caR[4];
    dfm_t m15, m17, m19, m21;
    dfa2_t a2L25, a2R43;
    dfa3_t a3L34, a3R52;
    dfd_t d16, d23, d31, d37, dts, d40, d49, d58, outL, outR, predL, predR;
    uint16_t iOut[11], iOut2[20];
    dfi_t lpfLin, lpfRin, lpfLd, lpfRd, lfo1l, lfo2l;
    dfq_t bapL, bapR, lp9, lp7, o1, o2;
    int32_t dcL[2], dcR[2];
    int32_t loopdecay, bassb;                /* Q15; Q8 */
    int16_t dampLb, dampLa, dampEb, dampEa; int8_t outk;
    const int16_t *oq;                       /* the output filter's row for DAMP */
    uint32_t ph1, ph2, rng;
    int32_t noise, xp;
    int32_t ml, ms;                          /* the last mid and side at 22.05 kHz (the output's interpolation) */
} dfr;

static int16_t *dfr_take(uint32_t n)
{
    int16_t *p = dfr.p;
    uint32_t i;
    dfr.p += n;
    for (i = 0; i < n; i++)
        p[i] = 0;
    return p;
}
static uint32_t dfr_isprime(uint32_t x)
{
    uint32_t d;
    if (x < 2u)
        return 0;
    for (d = 2; d * d <= x; d++)
        if (x % d == 0)
            return 0;
    return 1;
}
static uint32_t dfr_pf(uint32_t def, uint32_t tf)          /* def * factor (Q16), at least 1 */
{
    uint32_t v = (uint32_t)(((def >> 8) * tf + (((def & 255u) * tf) >> 8)) >> 8);
    return v ? v : 1u;
}
static uint32_t dfr_pp(uint32_t def, uint32_t tf)
{
    uint32_t v = dfr_pf(def, tf);
    while (!dfr_isprime(v))
        v++;
    return v;
}
static void dfr_dly(dfd_t *d, uint32_t n) { d->n = (uint16_t)n; d->i = 0; d->b = n ? dfr_take(n) : 0; }
static void dfr_ap(dfd_t *d, uint32_t n) { dfr_dly(d, n); }
static void dfr_apm(dfm_t *a, uint32_t n, uint32_t modn, int32_t fb, int32_t dec)
{
    if (modn > n)
        modn = n;
    a->n = (uint16_t)(n + modn); a->modn = (uint16_t)modn; a->b = dfr_take(n + modn);
    a->rd = (uint16_t)(modn * 2u); a->wr = 0; a->z1 = 0; a->fb = fb; a->dec = dec;
}

static inline int32_t dfr_c16(int32_t v) { return v > 32767 ? 32767 : v < -32768 ? -32768 : v; }
/* a plain all-pass: t = buf; x += fb t; out = t - fb x; buf = x */
static int32_t dfr_ap_run(dfd_t *a, int32_t fb, int32_t x)
{
    int32_t t = a->b[a->i], o;
    x = dfr_c16(x + dfr_mr(fb, t));
    o = t - dfr_mr(fb, x);
    a->b[a->i] = (int16_t)x;
    if (++a->i >= a->n)
        a->i = 0;
    return o;
}
/* the modulated all-pass of Freeverb3 (allpassm, process_dc): the read point wanders; mod Q15 -1 .. 1, fmod Q15 added to the feedback */
static int32_t dfr_apm_run(dfm_t *a, int32_t x, int32_t mod, int32_t fmod)
{
    int32_t m = (mod + 32768) * (int32_t)a->modn, fl = m >> 15, frac = 32768 - (m & 32767), fbm = clamp(a->fb + fmod, -32767, 32767), w;
    int32_t ra = (int32_t)a->rd - fl, rb;
    if (ra < 0)
        ra += a->n;
    rb = ra - 1;
    if (rb < 0)
        rb += a->n;
    a->z1 = dfr_c16(a->b[rb] + dfr_mr(frac, a->b[ra] - a->z1));
    if (++a->rd >= a->n)
        a->rd = 0;
    w = dfr_c16(x + dfr_mr(a->z1, fbm));
    a->b[a->wr] = (int16_t)w;
    if (++a->wr >= a->n)
        a->wr = 0;
    return dfr_mr(a->dec, a->z1) - dfr_mr(w, fbm);
}
static int32_t dfr_a2_run(dfa2_t *a, int32_t x)
{
    int32_t t2 = a->a2.b[a->a2.i], t1 = a->a1.b[a->a1.i], o;
    x = dfr_c16(x + dfr_mr(a->f2, t2));
    o = dfr_mr(a->d2, t2) - dfr_mr(x, a->f2);
    x = dfr_c16(x + dfr_mr(a->f1, t1));
    a->a2.b[a->a2.i] = (int16_t)dfr_c16(dfr_mr(a->d1, t1) - dfr_mr(x, a->f1));
    a->a1.b[a->a1.i] = (int16_t)x;
    if (++a->a1.i >= a->a1.n)
        a->a1.i = 0;
    if (++a->a2.i >= a->a2.n)
        a->a2.i = 0;
    return o;
}
static int32_t dfr_a3_run(dfa3_t *a, int32_t x, int32_t mod)
{
    int32_t m = (mod + 32768) * (int32_t)a->modn, fl = m >> 15, frac = m & 32767, o, t1;
    int32_t ra = (int32_t)a->rd1 - fl, rb;
    if (ra < 0)
        ra += a->n1;
    rb = ra - 1;
    if (rb < 0)
        rb += a->n1;
    x = dfr_c16(x + dfr_mr(a->f3, a->b3[a->i3]));
    o = dfr_mr(a->d3, a->b3[a->i3]) - dfr_mr(a->f3, x);
    x = dfr_c16(x + dfr_mr(a->f2, a->b2[a->i2]));
    a->b3[a->i3] = (int16_t)dfr_c16(dfr_mr(a->d2, a->b2[a->i2]) - dfr_mr(a->f2, x));
    t1 = dfr_mr(frac, a->b1[rb]) + dfr_mr(32768 - frac, a->b1[ra]);
    x = dfr_c16(x + dfr_mr(a->f1, t1));
    a->b2[a->i2] = (int16_t)dfr_c16(dfr_mr(a->d1, t1) - dfr_mr(a->f1, x));
    a->b1[a->wr1] = (int16_t)x;
    if (++a->wr1 >= a->n1)
        a->wr1 = 0;
    if (++a->rd1 >= a->n1)
        a->rd1 = 0;
    if (++a->i2 >= a->n2)
        a->i2 = 0;
    if (++a->i3 >= a->n3)
        a->i3 = 0;
    return o;
}
static inline int32_t dfr_dproc(dfd_t *d, int32_t x)       /* a delay: out the oldest, in x */
{
    int32_t o;
    if (!d->n)
        return x;
    o = d->b[d->i];
    d->b[d->i] = (int16_t)dfr_c16(x);
    if (++d->i >= d->n)
        d->i = 0;
    return o;
}
static inline int32_t dfr_dz(const dfd_t *d, uint32_t k)   /* the sample k writes ago (1 .. n) */
{
    int32_t r;
    if (k > d->n) k = d->n;
    if (k < 1u) k = 1u;
    r = (int32_t)d->i - (int32_t)k;
    if (r < 0) r += d->n;
    return d->b[r];
}
static inline int32_t dfr_z1(const int16_t *b, uint32_t n, uint32_t idx, uint32_t k)
{
    int32_t r = (int32_t)idx - (int32_t)k;
    if (r < 0) r += (int32_t)n;
    return b[r];
}

static int32_t dfr_i1(dfi_t *f, int32_t b, int32_t a, int32_t x)           /* a one-pole: y = b x + y1; y1 = a y + b x (Q15) */
{
    int32_t o = dfr_mr(b, x) + f->y1;
    f->y1 = dfr_mr(a, o) + dfr_mr(b, x);
    return o;
}
/* v * c / 16384 for c in Q14 with |c| up to 2 (mulq16 takes a factor below 1: the part above 1 is added whole) */
static inline int32_t dfr_m14(int32_t v, int32_t c)
{
    int32_t m = c < 0 ? -c : c, r = m >= 16384 ? v + mulq16(v, (uint32_t)(m - 16384) * 4u) : mulq16(v, (uint32_t)m * 4u);
    return c < 0 ? -r : r;
}
/* a biquad, direct form 1, its states in Q17 (four bits more than the samples: a 150 Hz all-pass has a pole pair at 0.92 and its DC gain
 * is 1 / 0.0008: whole-LSB rounding in Q13 would put 600 LSB of offset on the output) */
static int32_t dfr_q(dfq_t *f, const int16_t *c, int32_t x)
{
    int32_t xs = x * 16, o = dfr_m14(xs, c[0]) + dfr_m14(f->i1, c[1]) + dfr_m14(f->i2, c[2]) - dfr_m14(f->o1, c[3]) - dfr_m14(f->o2, c[4]);
    o = clamp(o, -(1 << 19), (1 << 19) - 1);
    f->i2 = f->i1; f->i1 = xs; f->o2 = f->o1; f->o1 = o;
    return dfr_c16((o + 8) >> 4);
}
static int32_t dfr_dc(int32_t *s, int32_t x)
{
    int32_t o = x - s[0] + dfr_mr(DFR_DCGAIN, s[1]);
    s[0] = x;
    s[1] = o;
    return o;
}

/* base lengths at 34.125 kHz and the tap lists (Freeverb3's progenitor / progenitor2 tables) */
static const uint16_t DFR_IAL[10] = {617, 535, 434, 347, 218, 162, 144, 122, 109, 74}, DFR_IAR[10] = {603, 547, 416, 364, 236, 162, 140, 131, 111, 79};
static const uint16_t DFR_CAL[4] = {430, 341, 264, 174}, DFR_CAR[4] = {447, 324, 247, 191};
static const uint16_t DFR_ALC[8] = {239, 392, 1944, 612, 1212, 121, 816, 1264}, DFR_ARC[8] = {205, 329, 2032, 368, 1452, 5, 688, 1340};
static const uint16_t DFR_OUTC[11] = {276, 468, 625, 312, 8, 24, 36, 40, 1, 192, 1572};
static const uint16_t DFR_OUTC2[20] = {110, 790, 2, 189, 121, 10, 480, 359, 103, 30, 26, 10, 780, 109, 1200, 1310, 780, 10, 310, 800};

/* the decay constants for DECAY rt (Q8 seconds) and SIZE rs (Q8: size / 10): decay_n ^ (rs / rt), Q15 */
static void dfr_decays(int32_t rt_q8, int32_t rs_q8, int32_t *dl0, int32_t *d1, int32_t *d2, int32_t *d3)
{
    int32_t e;
    e = (int32_t)(((int32_t)DFR_LOG2DEC[0] >> 4) * rs_q8 / rt_q8) << 4;
    *dl0 = (int32_t)cm_exp2(e);
    e = (int32_t)(((int32_t)DFR_LOG2DEC[1] >> 4) * rs_q8 / rt_q8) << 4;
    *d1 = (int32_t)cm_exp2(e);
    e = (int32_t)(((int32_t)DFR_LOG2DEC[2] >> 4) * rs_q8 / rt_q8) << 4;
    *d2 = (int32_t)cm_exp2(e);
    e = (int32_t)(((int32_t)DFR_LOG2DEC[3] >> 4) * rs_q8 / rt_q8) << 4;
    *d3 = (int32_t)cm_exp2(e);
}

/* (re)build: SIZE 0 .. 127 -> 8 .. 24 m, DECAY 0 .. 127 -> 0.25 .. 5 s (log), DAMP 0 .. 127 */
static void dfr_setup(int32_t size, int32_t decay, int32_t damp)
{
    int32_t m = 80 + (clamp(size, 0, 127) * 160) / 127;                      /* size in m x 10: 80 .. 240 */
    int32_t rs_q8 = (m * 256) / 100;                                        /* RS = size / 10, Q8 */
    int32_t rt_q8, dl0, d1, d2, d3, i;
    uint32_t tf, ex;
    const uint32_t FSQ = 22050u;
    tf = (uint32_t)(((FSQ * (uint32_t)rs_q8) << 8) / 34125u);               /* fs * RS / 34125, Q16 */
    ex = (uint32_t)((FSQ << 16) / 34125u);
    dfr.p = dfr_pool;
    /* DECAY: 0.25 s * 20 ^ (v / 127), in Q8: the exponential by cm_exp2 */
    rt_q8 = (int32_t)((cm_exp2(clamp(decay, 0, 127) * 2230) * 64u) >> 15);   /* 2^(v * 4.32 / 127) = 20 ^ (v / 127): Q16 octaves 2230 a step; x 64 = 0.25 s in Q8 */
    if (rt_q8 < 64)
        rt_q8 = 64;
    dfr_decays(rt_q8, rs_q8, &dl0, &d1, &d2, &d3);
    dfr.loopdecay = dl0;
    /* the early reflections */
    {
        static const uint16_t DL[6] = {199, 354, 389, 414, 699, 796}, DR[6] = {209, 364, 399, 424, 709, 806};   /* ms x 10 */
        uint32_t mx = 0, k;
        dfr.nt = 6;
        for (k = 0; k < 6u; k++) {
            dfr.tapL[k] = (uint16_t)(((FSQ * (uint32_t)m / 100u) * DL[k]) / 10000u);       /* fs * size / 10 * delay */
            dfr.tapR[k] = (uint16_t)(((FSQ * (uint32_t)m / 100u) * DR[k]) / 10000u);
            if (dfr.tapL[k] > mx) mx = dfr.tapL[k];
            if (dfr.tapR[k] > mx) mx = dfr.tapR[k];
        }
        dfr_dly(&dfr.lineL, mx + 12u); dfr_dly(&dfr.lineR, mx + 12u);
        dfr_dly(&dfr.ltr, 4u); dfr_dly(&dfr.rtl, 4u);                      /* the 0.2 ms L-R delay: 4 samples */
    }
    for (i = 0; i < 10; i++) {
        dfr_apm(&dfr.iaL[i], dfr_pp(DFR_IAL[i], tf), dfr_pp(10u, ex), -(70 * 32768 / 120), 32768);
        dfr_apm(&dfr.iaR[i], dfr_pp(DFR_IAR[i], tf), dfr_pp(10u, ex), -(70 * 32768 / 120), 32768);
    }
    for (i = 0; i < 4; i++) {
        dfr_ap(&dfr.caL[i], dfr_pp(DFR_CAL[i], tf));
        dfr_ap(&dfr.caR[i], dfr_pp(DFR_CAR[i], tf));
    }
    dfr_apm(&dfr.m15, dfr_pp(DFR_ALC[0], tf), dfr_pp(32u, ex), 12288, d2);   /* diffusion 1: 0.375 */
    dfr_apm(&dfr.m17, dfr_pp(DFR_ALC[1], tf), dfr_pp(32u, ex), 10223, d3);   /* diffusion 2: 0.312 */
    dfr_apm(&dfr.m19, dfr_pp(DFR_ARC[0], tf), dfr_pp(32u, ex), 12288, d2);
    dfr_apm(&dfr.m21, dfr_pp(DFR_ARC[1], tf), dfr_pp(32u, ex), 10223, d3);
    {
        dfa2_t *a[2] = {&dfr.a2L25, &dfr.a2R43};
        const uint16_t *c[2] = {DFR_ALC, DFR_ARC};
        dfa3_t *b[2] = {&dfr.a3L34, &dfr.a3R52};
        uint32_t k;
        for (k = 0; k < 2u; k++) {
            uint32_t n1 = dfr_pp(c[k][4], tf), m1 = dfr_pp(c[k][5], tf);
            dfr_ap(&a[k]->a1, dfr_pp(c[k][2], tf)); dfr_ap(&a[k]->a2, dfr_pp(c[k][3], tf));
            a[k]->f1 = 8192; a[k]->f2 = 13312; a[k]->d1 = (int16_t)d1; a[k]->d2 = (int16_t)d2;   /* diffusion 4: 0.25; 3: 0.406 */
            if (m1 > n1) m1 = n1;
            b[k]->n1 = (uint16_t)(n1 + m1); b[k]->modn = (uint16_t)m1; b[k]->n2 = (uint16_t)dfr_pp(c[k][6], tf); b[k]->n3 = (uint16_t)dfr_pp(c[k][7], tf);
            b[k]->b1 = dfr_take(b[k]->n1); b[k]->b2 = dfr_take(b[k]->n2); b[k]->b3 = dfr_take(b[k]->n3);
            b[k]->rd1 = (uint16_t)(m1 * 2u); b[k]->wr1 = 0; b[k]->i2 = b[k]->i3 = 0;
            b[k]->f1 = 8192; b[k]->f2 = 8192; b[k]->f3 = 13312; b[k]->d1 = (int16_t)d1; b[k]->d2 = (int16_t)d1; b[k]->d3 = (int16_t)d2;
        }
    }
    dfr_dly(&dfr.d16, dfr_pp(2u, tf)); dfr_dly(&dfr.d23, dfr_pp(1055u, tf)); dfr_dly(&dfr.d31, dfr_pp(344u, tf)); dfr_dly(&dfr.d37, dfr_pp(1572u, tf));
    dfr_dly(&dfr.dts, dfr_pp(1u, tf)); dfr_dly(&dfr.d40, dfr_pp(1460u, tf)); dfr_dly(&dfr.d49, dfr_pp(500u, tf)); dfr_dly(&dfr.d58, dfr_pp(16u, tf));
    for (i = 0; i < 11; i++)
        dfr.iOut[i] = (uint16_t)dfr_pf(DFR_OUTC[i], tf);
    for (i = 0; i < 20; i++)
        dfr.iOut2[i] = (uint16_t)dfr_pf(DFR_OUTC2[i], tf);
    dfr_dly(&dfr.outL, dfr_pp(22u, (FSQ << 16) / 1000u)); dfr_dly(&dfr.outR, dfr_pp(22u, (FSQ << 16) / 1000u));
    dfr_dly(&dfr.predL, 176u); dfr_dly(&dfr.predR, 176u);                   /* 8 ms */
    /* the bass boost: boost 50 / 20 / decay^1.5 * size / 10 (Q8, clamped) */
    {
        int32_t lg = cm_log2((uint32_t)rt_q8) - (8 << 16);                  /* log2(decay) Q16 */
        int32_t p15 = (int32_t)cm_exp2(-(lg * 3) / 2);                      /* decay^-1.5, Q15 */
        int32_t b = (int32_t)(((uint32_t)p15 * 640u) >> 15);                /* 2.5 * 256 = 640: Q8 */
        b = (b * rs_q8) >> 8;
        dfr.bassb = clamp(b, 0, 20000);
    }
    dfr.sz = size; dfr.rt = decay; dfr.dp = damp; dfr.ok = 1;
    dfr.ph1 = dfr.ph2 = 0; dfr.noise = 0; dfr.xp = 0; dfr.ml = dfr.ms = 0;
    dfr.lpfLin.y1 = dfr.lpfRin.y1 = dfr.lpfLd.y1 = dfr.lpfRd.y1 = dfr.lfo1l.y1 = dfr.lfo2l.y1 = dfr.eoL.y1 = dfr.eoR.y1 = 0;
    dfr.dcL[0] = dfr.dcL[1] = dfr.dcR[0] = dfr.dcR[1] = 0;
    {
        dfq_t z = {0, 0, 0, 0};
        dfr.axL = dfr.axR = dfr.a2L = dfr.a2R = dfr.bapL = dfr.bapR = dfr.lp9 = dfr.lp7 = dfr.o1 = dfr.o2 = z;
    }
}

/* DAMP changes the damping filters only */
static void dfr_damp(int32_t damp)
{
    int32_t d = clamp(damp, 0, 127);
    dfr.dampLb = DFR_DAMPL_B[d]; dfr.dampLa = DFR_DAMPL_A[d]; dfr.dampEb = DFR_DAMPE_B[d]; dfr.dampEa = DFR_DAMPE_A[d];
    dfr.oq = &DFR_OUTLP[5 * d];
    dfr.dp = damp;
}

/* one 22.05 kHz sample: x (Q13) in, early and late stereo out */
static void dfr_step(int32_t x, int32_t *eL, int32_t *eR, int32_t *lL, int32_t *lR)
{
    int32_t wl = 0, wr = 0, k, rl, lr, inL, inR, outL, outR, mn, lfo, lfo2, crossL, crossR, mnoise, sg = -1, Dout, Bout, u, fpL, fpR;
    /* early reflections (the mono send in both channels) */
    {
        dfd_t *a = &dfr.lineL, *b = &dfr.lineR;
        a->i = a->i ? (uint16_t)(a->i - 1u) : (uint16_t)(a->n - 1u); a->b[a->i] = (int16_t)x;
        b->i = b->i ? (uint16_t)(b->i - 1u) : (uint16_t)(b->n - 1u); b->b[b->i] = (int16_t)x;
        for (k = 0; k < 6; k++) {
            static const int16_t GL[6] = {16712, 13403, 10404, 11780, 4375, 3965}, GR[6] = {16728, 13435, 10371, 11830, 3064, 3981};   /* Q14 (1.02 does not fit Q15) */
            uint32_t ia = (uint32_t)a->i + dfr.tapL[k], ib = (uint32_t)b->i + dfr.tapR[k];
            if (ia >= a->n) ia -= a->n;
            if (ib >= b->n) ib -= b->n;
            wl += (GL[k] * a->b[ia]) >> 14;
            wr += (GR[k] * b->b[ib]) >> 14;
        }
        rl = dfr_dproc(&dfr.rtl, x + wr);
        lr = dfr_dproc(&dfr.ltr, x + wl);
        *eL = dfr_i1(&dfr.eoL, dfr.dampEb, dfr.dampEa, dfr_q(&dfr.a2L, DFR_EAP2, dfr_mr(29491, wl) + dfr_mr(3277, dfr_q(&dfr.axL, DFR_EAP1, rl))));
        *eR = dfr_i1(&dfr.eoR, dfr.dampEb, dfr.dampEa, dfr_q(&dfr.a2R, DFR_EAP2, dfr_mr(29491, wr) + dfr_mr(3277, dfr_q(&dfr.axR, DFR_EAP1, lr))));
        *eL = dfr_c16(*eL); *eR = dfr_c16(*eR);
    }
    /* the late reverb: input = x + 0.2 * early */
    inL = dfr_c16(x + dfr_mr(6554, *eL));
    inR = dfr_c16(x + dfr_mr(6554, *eR));
    outL = dfr_dc(dfr.dcL, inL);
    outR = dfr_dc(dfr.dcR, inR);
    dfr.rng = dfr.rng * 1664525u + 1013904223u;                                      /* the modulation noise: a slow random walk */
    dfr.noise += dfr_mr(((int32_t)(dfr.rng >> 16) - 32768) - (dfr.noise >> 1), 655);
    mn = dfr.noise * 3;
#ifdef DFR_NONOISE
    mn = 0;
#endif
    lfo = dfr_i1(&dfr.lfo1l, DFR_LFO1L_B, DFR_LFO1L_A, dfr_mr(osc_sine(dfr.ph1) + dfr_mr(2949, mn), DFR_WANDER));
    dfr.ph1 += DFR_INC1;
    mnoise = dfr_mr(mn, 1966);                                                       /* 0.06 */
    for (k = 0; k < 10; k++) {
        outL = dfr_apm_run(&dfr.iaL[k], outL, lfo * sg, mnoise);
        outR = dfr_apm_run(&dfr.iaR[k], outR, lfo, mnoise * sg);
        sg = -sg;
    }
#ifdef DFR_DBG
    dfr_dbg[1] = outL;
#endif
    crossL = outL; crossR = outR;
    for (k = 0; k < 4; k++) {
        crossL = dfr_ap_run(&dfr.caL[k], 70 * 32768 / 120, crossL);
        crossR = dfr_ap_run(&dfr.caR[k], (70 * 32768 / 120), crossR);
    }
    outL = dfr_i1(&dfr.lpfLin, DFR_LPIN_B, DFR_LPIN_A, dfr_c16(outL + dfr_mr(13107, crossR)));
    outR = dfr_i1(&dfr.lpfRin, DFR_LPIN_B, DFR_LPIN_A, dfr_c16(outR + dfr_mr(13107, crossL)));
#ifdef DFR_DBG
    dfr_dbg[2] = outL;
#endif
    crossR = dfr.d58.b[dfr.d58.i]; crossL = dfr.d37.b[dfr.d37.i];
    u = dfr_q(&dfr.lp9, DFR_BASSLP, dfr_q(&dfr.bapL, DFR_BASSAP, crossR));
    outL += dfr_mr(dfr.loopdecay, crossR + ((u * dfr.bassb) >> 8));
    u = dfr_q(&dfr.lp7, DFR_BASSLP, dfr_q(&dfr.bapR, DFR_BASSAP, crossL));
    outR += dfr_mr(dfr.loopdecay, crossL + ((u * dfr.bassb) >> 8));
#ifdef DFR_DBG
    dfr_dbg[3] = outL;
#endif
    outL = dfr_apm_run(&dfr.m17, dfr_dproc(&dfr.d16, dfr_apm_run(&dfr.m15, dfr_i1(&dfr.lpfLd, dfr.dampLb, dfr.dampLa, dfr_c16(outL)), lfo, mnoise)), -lfo, -mnoise);
    outR = dfr_apm_run(&dfr.m21, dfr_dproc(&dfr.dts, dfr_apm_run(&dfr.m19, dfr_i1(&dfr.lpfRd, dfr.dampLb, dfr.dampLa, dfr_c16(outR)), -lfo, -mnoise)), lfo, mnoise);
    dfr_dproc(&dfr.d37, dfr_a3_run(&dfr.a3L34, dfr_dproc(&dfr.d31, dfr_a2_run(&dfr.a2L25, dfr_dproc(&dfr.d23, outL))), lfo));
    dfr_dproc(&dfr.d58, dfr_a3_run(&dfr.a3R52, dfr_dproc(&dfr.d49, dfr_a2_run(&dfr.a2R43, dfr_dproc(&dfr.d40, outR))), -lfo));
    {
        const dfa2_t *A2L = &dfr.a2L25, *A2R = &dfr.a2R43;
        const dfa3_t *A3L = &dfr.a3L34, *A3R = &dfr.a3R52;
        const uint16_t *o = dfr.iOut, *p = dfr.iOut2;
        Dout = dfr_mr(15368, dfr_dz(&dfr.d23, o[8]))
             + dfr_mr(7176, dfr_dz(&dfr.d31, o[7]) - dfr_dz(&dfr.d49, o[9]) + dfr_dz(&dfr.d31, o[0]) - dfr_dz(&dfr.d40, p[0]) - dfr_dz(&dfr.d49, o[1]))
             + dfr_mr(2097, dfr_dz(&dfr.d37, o[10]) + dfr_z1(A2L->a1.b, A2L->a1.n, A2L->a1.i, p[4]) + dfr_z1(A2L->a2.b, A2L->a2.n, A2L->a2.i, p[6])
                      - dfr_z1(A2R->a2.b, A2R->a2.n, A2R->a2.i, p[8]) + dfr_z1(A3L->b1, A3L->n1, A3L->rd1, p[10]) + dfr_z1(A3L->b2, A3L->n2, A3L->i2, p[12])
                      + dfr_z1(A3L->b3, A3L->n3, A3L->i3, p[14]) - dfr_z1(A3R->b2, A3R->n2, A3R->i2, p[18]))
             + dfr_mr(1475, dfr_dz(&dfr.d37, p[16]));
        Bout = dfr_mr(15368, dfr_dz(&dfr.d40, o[2]))
             + dfr_mr(7176, dfr_dz(&dfr.d49, o[1]) - dfr_dz(&dfr.d31, o[3]) + dfr_dz(&dfr.d49, o[5]) - dfr_dz(&dfr.d37, o[6]) - dfr_dz(&dfr.d23, p[1]) - dfr_dz(&dfr.d31, p[3]))
             + dfr_mr(2097, dfr_dz(&dfr.d58, o[4]) + dfr_z1(A2R->a1.b, A2R->a1.n, A2R->a1.i, p[5]) + dfr_z1(A2R->a2.b, A2R->a2.n, A2R->a2.i, p[7])
                      - dfr_z1(A2L->a2.b, A2L->a2.n, A2L->a2.i, p[9]) + dfr_z1(A3R->b1, A3R->n1, A3R->rd1, p[11]) + dfr_z1(A3R->b2, A3R->n2, A3R->i2, p[13])
                      + dfr_z1(A3R->b3, A3R->n3, A3R->i3, p[15]) - dfr_z1(A3L->b2, A3L->n2, A3L->i2, p[19]))
             + dfr_mr(1475, dfr_dz(&dfr.d58, p[17]));
    }
#ifdef DFR_DBG
    dfr_dbg[4] = Dout;
#endif
    lfo2 = dfr_i1(&dfr.lfo2l, DFR_LFO2L_B, DFR_LFO2L_A, dfr_mr(osc_sine(dfr.ph2), DFR_WANDER));
    dfr.ph2 += DFR_INC2;
    {   /* the output combs: a feed-forward comb whose gain is the LFO */
        int32_t oa = dfr_mr(dfr.outL.b[dfr.outL.i], lfo2) + Dout, ob = dfr_mr(dfr.outR.b[dfr.outR.i], -lfo2) + Bout;
        dfr.outL.b[dfr.outL.i] = (int16_t)dfr_c16(Dout); dfr.outR.b[dfr.outR.i] = (int16_t)dfr_c16(Bout);
        if (++dfr.outL.i >= dfr.outL.n) dfr.outL.i = 0;
        if (++dfr.outR.i >= dfr.outR.n) dfr.outR.i = 0;
        outL = oa; outR = ob;
    }
    fpL = dfr_dproc(&dfr.predL, dfr_q(&dfr.o1, dfr.oq, dfr_c16(outL)));
    fpR = dfr_dproc(&dfr.predR, dfr_q(&dfr.o2, dfr.oq, dfr_c16(outR)));
    *lL = fpL;
    *lR = fpR;
}
#endif
