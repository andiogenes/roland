/* SPDX-License-Identifier: GPL-3.0-only */
/* COMP: the INSERT's 1176-style limiting amplifier. No dependency but <stdint.h> (tests/comp_host.c builds it alone).
 *
 * A feed-forward peak compressor in the log2 domain (the level an octave count in Q16, 6.02 dB to the octave):
 *   level      L = log2(|x|) + INPUT          INPUT 0 .. +20 dB into a fixed threshold (-12 dB of a voice's full scale):
 *                                             more INPUT, more compression, as on the unit
 *   envelope   attack: a one-pole toward a rising level, 20 us .. 800 us (ATK up = slower);
 *              release: a fixed fall in dB a second, the time to fall 20 dB 50 ms .. 1.1 s (REL up = slower)
 *   gain       -(L_env - T) * (1 - 1/ratio) above the threshold, 4 8 12 20 : 1; ALL (every button in): about 16 : 1, 3 dB
 *              lower and a twice as fast attack
 *   output     OUT: -24 .. +7.75 dB in quarter dB steps (96 = 0 dB)
 * Fixed point, no 64-bit arithmetic: what the target has. docs/FX-PLAN.md: step 3. */
#ifndef FX_COMP_C
#define FX_COMP_C

typedef struct {
    int32_t lenv;                       /* the envelope, Q16 octaves */
    int32_t gr;                         /* the gain reduction now, Q16 octaves */
} comp_t;
typedef struct {
    int32_t gin, gout;                  /* INPUT and OUT, Q16 octaves */
    int32_t katt, rstep;                /* attack coefficient Q15; release, Q16 octaves a sample */
    int32_t slope, thr;                 /* 1 - 1/ratio, Q15; the threshold, Q16 octaves */
} comp_k_t;

static int32_t cm_log2(uint32_t v)      /* log2(v) in Q16, v >= 1: the exponent and a quadratic for the mantissa (0.02 octave) */
{
    uint32_t n = 31u, f;
    if (!v)
        return 0;
    while (!((v >> n) & 1u))
        n--;
    f = ((v << (31u - n)) & 0x7FFFFFFFu) >> 15;                 /* the mantissa 0 .. 65535 */
    return (int32_t)(n << 16) + (int32_t)(((f >> 1) * (92682u - ((27146u * f) >> 16))) >> 15);
}
static uint32_t cm_exp2(int32_t o)      /* 2^(o / 65536) in Q15 (a cubic for the fraction: 0.6 %), o < 16 octaves */
{
    int32_t n = o >> 16;
    uint32_t f = (uint32_t)o & 0xFFFFu, f2 = (f * f) >> 16, p;
    p = 32768u + ((f * 22713u) >> 16) + ((f2 * 7871u) >> 16) + ((((f2 * f) >> 16) * 1819u) >> 16);
    return n >= 0 ? p << n : (-n > 31 ? 0u : p >> -n);
}

static int comp_ratio_slope(int e) { static const int32_t S[5] = {24576, 28672, 30037, 31130, 30720}; return S[e < 0 ? 0 : e > 4 ? 4 : e]; }

/* the knobs: A INPUT, B OUT, C ATK, D REL (0 .. 127), e the ratio button 0 .. 4 */
static void comp_coefs(comp_k_t *k, int32_t a, int32_t b, int32_t c, int32_t d, int32_t e)
{
    int32_t ka;
    k->gin = a * 1713;
    k->gout = (b - 96) * 2721;
    ka = (int32_t)((cm_exp2(-c * 2745) * 37150u) >> 15) * (e == 4 ? 2 : 1);
    k->katt = ka > 32768 ? 32768 : ka;
    k->rstep = (int32_t)((cm_exp2(-d * 2301) * 99u) >> 15);
    if (k->rstep < 1)
        k->rstep = 1;
    k->slope = comp_ratio_slope(e);
    k->thr = (13 << 16) - (e == 4 ? 32768 : 0);                 /* 8192 = -12 dB of 32768; ALL: -3 dB lower (half an octave of 6 dB) */
}

static int32_t comp_one(comp_t *s, const comp_k_t *k, int32_t x)
{
    int32_t l, over, g;
    uint32_t ax = (uint32_t)(x < 0 ? -x : x);
    l = cm_log2(ax + 1u) + k->gin;
    if (l > s->lenv)
        s->lenv += (((l - s->lenv) >> 4) * k->katt) >> 11;
    else if ((s->lenv -= k->rstep) < l)
        s->lenv = l;
    over = s->lenv - k->thr;
    s->gr = over > 0 ? ((over >> 2) * k->slope) >> 13 : 0;
    g = (int32_t)(cm_exp2(k->gin + k->gout - s->gr) >> 5);      /* Q10 */
    if (x > (1 << 17)) x = 1 << 17;
    if (x < -(1 << 17)) x = -(1 << 17);
    return ((x >> 2) * g) >> 8;
}
#endif
