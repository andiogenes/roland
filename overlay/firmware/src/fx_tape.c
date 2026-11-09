/* SPDX-License-Identifier: GPL-3.0-only */
/* The delay as a tape echo (docs/FX-PLAN.md step 3). Needs osc_sine and softclip (the engines' dsp.c); tests/tape_host.c
 * supplies them to build this alone.
 *   wow and flutter   the read head's position wanders: two slow sines (0.43 Hz, 20 samples deep, and 0.97 Hz, 8) and a
 *                     flutter (6.1 Hz, half a sample): about +-0.2 % of pitch at the slowest, +-0.05 % at the fastest;
 *                     the wander is one value a block, ramped across it
 *   saturation        what goes onto the tape is soft-clipped (tanh): unity for small signals, the repeats lose their
 *                     peaks and warm as they recirculate
 *   head loss         the feedback's low-pass (the COLR knob), as before: each repeat darker than the one before */
#ifndef FX_TAPE_C
#define FX_TAPE_C

typedef struct { uint32_t ph[3]; int32_t m; } tape_t;           /* the phases, the wander now (Q8 samples) */

static int32_t tape_wander(tape_t *tp, uint32_t n)              /* advance n samples; the wander Q8 samples */
{
    tp->ph[0] += 41900u * n;                                    /* 0.43 Hz at 44.1 kHz: 2^32 f / fs */
    tp->ph[1] += 94500u * n;                                    /* 0.97 Hz */
    tp->ph[2] += 594000u * n;                                   /* 6.1 Hz */
    return (osc_sine(tp->ph[0]) * 5120 >> 15) + (osc_sine(tp->ph[1]) * 2048 >> 15) + (osc_sine(tp->ph[2]) * 128 >> 15);
}

/* the tap: the line's sample `dl` (+ the wander m, Q8) before the write position w, linearly interpolated */
static int32_t tape_tap(const int16_t *buf, uint32_t mask, uint32_t w, uint32_t dl, int32_t m)
{
    int32_t d = (int32_t)(dl << 8) + m, x0, x1;
    uint32_t pos;
    if (d < (4 << 8))
        d = 4 << 8;                                              /* never the write head itself */
    pos = (w << 8) - (uint32_t)d;
    x0 = buf[(pos >> 8) & mask];
    x1 = buf[((pos >> 8) + 1u) & mask];
    return x0 + (((x1 - x0) * (int32_t)(pos & 255u)) >> 8);
}
#endif
