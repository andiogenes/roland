/* SPDX-License-Identifier: GPL-3.0-only */
/* Host build of overlay/firmware/src/drum_synth606.c against stand-ins for the Felucca helpers it uses (the same formulas
 * as Felucca's tables: DECAY_K, SVF_G; a sine and a triangle in Q15; a pitch table in 1/16 semitones). Renders every sound
 * of the TR-606 kit at velocity 100 and prints: lane, samples until it ended (0 = never within 3 s), peak, RMS of the first
 * 100 ms, and the peak after 1 s. */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define FS 44100
#define CTL 32
#define CTL_LOG2 5
static uint16_t DECAY_K[128], SVF_G[128];
static inline int32_t clamp(int32_t v, int32_t lo, int32_t hi) { return v < lo ? lo : v > hi ? hi : v; }
static inline int32_t mulq15(int32_t a, int32_t b) { return (a * b) >> 15; }
static inline int32_t sine_i(uint32_t ph) { return (int32_t)lrint(32767.0 * sin(ph * (6.283185307179586 / 4294967296.0))); }
static inline int32_t osc_tri(uint32_t ph)
{
    int32_t x = (int32_t)(ph >> 16) - 32768;       /* sawtooth -32768..32767 */
    x = x < 0 ? -x : x;                            /* 0..32768 */
    return (x - 16384) * 2 > 32767 ? 32767 : (x - 16384) * 2;
}
static inline int32_t softclip(int32_t x) { return (int32_t)lrint(32767.0 * tanh(x / 32768.0)); }
static inline uint32_t noise32(int32_t *st)
{
    uint32_t x = (uint32_t)*st;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    *st = (int32_t)x;
    return x;
}
static uint32_t rng_state = 0x1234567u;
static uint32_t rng(void) { rng_state ^= rng_state << 13; rng_state ^= rng_state >> 17; rng_state ^= rng_state << 5; return rng_state; }
static uint32_t pitch_inc(uint32_t p16)
{
    double hz = 440.0 * pow(2.0, ((double)p16 / 16.0 - 69.0) / 12.0);
    return (uint32_t)(hz / FS * 4294967296.0);
}
#include "../overlay/firmware/src/drum_synth606.c"

static const uint8_t NOTE[DS_LANES] = {36, 38, 39, 42, 46, 43, 48, 49, 51, 70, 63, 37, 56, 75, 35, 40};
int main(void)
{
    for (int v = 0; v < 128; v++) {
        double ms = 5.0 * pow(800.0, v / 127.0), fc = 30.0 * pow(16000.0 / 30.0, v / 127.0);
        DECAY_K[v] = (uint16_t)fmin(65535.0, floor(65536.0 * exp(-6.9 / (ms / 1000.0 * FS))));
        SVF_G[v] = (uint16_t)floor(4096.0 * tan(M_PI * fmin(fc, 0.45 * FS) / FS));
    }
    for (int l = 0; l < DS_LANES; l++) {
        dsv_t s;
        int32_t buf[CTL];
        long total = 0, ended = 0, peak = 0, peak1s = 0;
        double e100 = 0;
        long n100 = 0;
        ds_on(&s, TR606_SND, 0, NOTE[l], 100, 0, 4);
        for (int b = 0; b < 3 * FS / CTL; b++) {
            int alive = ds_render(&s, buf, CTL);
            for (int i = 0; i < CTL; i++, total++) {
                long a = labs(buf[i]);
                if (a > peak) peak = a;
                if (total >= FS && a > peak1s) peak1s = a;
                if (total < FS / 10) { e100 += (double)buf[i] * buf[i]; n100++; }
            }
            if (!alive) { ended = total; break; }
        }
        printf("%d %ld %ld %.0f %ld\n", l, ended, peak, sqrt(e100 / n100), peak1s);
    }
    {   /* the choke: an open hat struck, choked after 50 ms, against the same hat left alone */
        dsv_t a, b;
        int32_t buf[CTL];
        long ea = 0, eb = 0, t;
        ds_on(&a, TR606_SND, 0, 46, 100, 0, 4);
        ds_on(&b, TR606_SND, 0, 46, 100, 0, 4);
        for (t = 0; t < 3 * FS / CTL; t++) {
            if (t == 50 * FS / 1000 / CTL) ds_choke(&a);
            if (!ea && !ds_render(&a, buf, CTL)) ea = (t + 1) * CTL;
            if (!eb && !ds_render(&b, buf, CTL)) eb = (t + 1) * CTL;
        }
        printf("C %ld %ld\n", ea, eb);
    }
    return 0;
}
