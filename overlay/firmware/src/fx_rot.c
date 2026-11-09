/* SPDX-License-Identifier: GPL-3.0-only */
/* ROT: the INSERT's tape rot (docs/FX-PLAN.md step 3): what an old tape does to a sound. Needs fx_tape.c (the wander), osc_sine,
 * softclip and SVF_G; tests/rot_host.c supplies them to build this alone.
 *   WOW     the read position of a 256-sample line wanders (fx_tape.c tape_wander, scaled up to x4: +-110 samples at 127)
 *   DROP    dropouts: now and then the level dips (30 .. 90 %) and comes back over about 25 ms; 0 never, 127 about 3 a second
 *   LOSS    the highs go: two one-pole low-passes from 16 kHz (0) down to about 1.5 kHz (127)
 *   HISS    tape noise, differenced (more highs than a white noise), up to -36 dB of full scale
 *   DRIVE   1x .. 4x into a soft clip, back to the level it came at */
#ifndef FX_ROT_C
#define FX_ROT_C

#define ROT_LEN 256u
typedef struct {
    tape_t tape;
    uint32_t rng;                       /* xorshift */
    uint16_t wp;
    int32_t dip;                        /* the dropout's attenuation now, Q15 */
    int32_t lp1, lp2, nz;               /* the low-passes (Q4), the last noise sample */
    int16_t line[ROT_LEN];
} rot_t;

static uint32_t rot_rand(rot_t *s)
{
    uint32_t x = s->rng ? s->rng : 0x9E3779B9u;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    return s->rng = x;
}

/* n samples of b in place: a .. e are WOW DROP LOSS HISS DRIVE, 0 .. 127 */
static void rot_run(rot_t *s, int32_t *b, uint32_t n, int32_t a, int32_t dr, int32_t loss, int32_t hiss, int32_t drive)
{
    uint32_t i, mask = ROT_LEN - 1u, g = SVF_G[127 - loss * 48 / 127];
    int32_t m1 = (tape_wander(&s->tape, n) * a) >> 5, m0 = s->tape.m, mstep = (m1 - m0) / (int32_t)n;
    int32_t k = (int32_t)((g << 16) / (4096u + g)), gd = 1024 + drive * 24, inv = (1 << 20) / gd, amp = (hiss * hiss) >> 5;
    /* a dropout: the odds a block grow with the square of DROP (127: 0.0022 a block of 32, about 3 a second) */
    if (dr && (rot_rand(s) & 0xFFFFu) < ((uint32_t)(dr * dr) * 143u * (uint32_t)n / 32u) >> 14) {
        int32_t depth = 10000 + (int32_t)(rot_rand(s) % 19000u);
        if (depth > s->dip)
            s->dip = depth;
    }
    for (i = 0; i < n; i++) {
        int32_t x = b[i], y, v, nz;
        s->line[s->wp & mask] = (int16_t)clamp(x >> 2, -32768, 32767);        /* a quarter: headroom */
        y = tape_tap(s->line, mask, s->wp, 128u, m0 + mstep * (int32_t)i) << 2;
        s->wp++;
        if (drive) {                                    /* soft clip, back to the level it came at */
            v = softclip(clamp(((y >> 2) * gd) >> 8, -65535, 65535));
            y = (v * inv) >> 10;
        }
        y <<= 4;                                        /* LOSS: two one-pole low-passes (x16) */
        v = mulq16(y - s->lp1, (uint32_t)k);
        y = v + s->lp1;
        s->lp1 = y + v;
        v = mulq16(y - s->lp2, (uint32_t)k);
        y = v + s->lp2;
        s->lp2 = y + v;
        y >>= 4;
        s->dip -= s->dip >> 10;                         /* a dropout comes back over ~25 ms */
        y = mulq15(y, 32768 - s->dip);
        nz = (int32_t)(rot_rand(s) >> 16) - 32768;      /* HISS: the noise's first difference */
        y += ((nz - s->nz) * amp) >> 15;
        s->nz = nz;
        b[i] = y;
    }
    s->tape.m = m1;
}
#endif
