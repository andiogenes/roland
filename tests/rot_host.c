/* SPDX-License-Identifier: GPL-3.0-only */
/* rot_host MODE ...: facts about overlay's fx_rot.c for tests/test_rot.py
 *   gain F A B C D E        steady gain in dB of a sine at F Hz (peak 0.1) with the knobs A .. E (output rms / input rms)
 *   noise D                 rms of the output for silence in, in full-scale units * 32768, with HISS = D
 *   dips B                  how many 1 ms windows in 20 s of a 1 kHz sine (peak 0.3) fall below 70 % of the level, DROP = B
 *   wobble A                the largest deviation in % of the measured frequency of a 1 kHz sine (zero crossings) with WOW = A */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
static int32_t osc_sine(uint32_t ph) { return (int32_t)lround(32767.0 * sin(ph * (2 * M_PI / 4294967296.0))); }
static int32_t softclip(int32_t x)
{
    double v = tanh(x / 32768.0) * 32767.0;
    return (int32_t)lround(v);
}
static int32_t mulq15(int32_t a, int32_t b) { return (a * b) >> 15; }
static int32_t mulq16(int32_t a, uint32_t k) { return (int32_t)(((a >> 16) * (int32_t)k) + (int32_t)(((uint32_t)(a & 0xFFFF) * k) >> 16)); }
static int32_t clamp(int32_t v, int32_t lo, int32_t hi) { return v < lo ? lo : v > hi ? hi : v; }
static uint32_t SVF_G[128];
#include "../overlay/firmware/src/fx_tape.c"
#include "../overlay/firmware/src/fx_rot.c"
static rot_t R;
static double run(double f, int secs, double amp, int a, int dr, int l, int h, int d, double *outv, int *dips, double *dev)
{
    int n = secs * 44100, i, blk;
    double acc = 0, ain = 0, nn = 0;
    int32_t buf[32];
    int zc = 0, last = 0, lastzc = -1, mn = 0, zt[64];
    memset(&R, 0, sizeof R);
    for (i = 0; i < n; i += 32) {
        for (blk = 0; blk < 32; blk++)
            buf[blk] = (int32_t)lround(amp * 32768 * sin(2 * M_PI * f * (i + blk) / 44100));
        for (blk = 0; blk < 32; blk++) ain += (double)buf[blk] * buf[blk];
        rot_run(&R, buf, 32, a, dr, l, h, d);
        for (blk = 0; blk < 32; blk++) {
            int32_t y = buf[blk];
            if (i > 44100) {
                acc += (double)y * y; nn++;
                if (last < 0 && y >= 0) {
                    zt[zc % 64] = i + blk;                                  /* the frequency over 20 periods (a sample of a period is 2 %) */
                    if (dev && zc >= 20) { double fr = 20 * 44100.0 / (zt[zc % 64] - zt[(zc - 20) % 64]); double e = fabs(fr - f) / f * 100; if (e > *dev) *dev = e; }
                    lastzc = i + blk; zc++;
                }
                last = y;
            }
        }
        if (dips && i > 44100 && (i / 32) % 1 == 0) {                  /* a 1 ms window: the peak of the block pair */
            int pk = 0;
            for (blk = 0; blk < 32; blk++) if (abs(buf[blk]) > pk) pk = abs(buf[blk]);
            if ((i / 32) % 2 == 0) mn = pk; else { if ((mn > pk ? mn : pk) < 0.7 * amp * 32768 * 0.5) (*dips)++; }
        }
    }
    (void)mn;
    *outv = sqrt(acc / nn);
    return sqrt(ain / n);
}
int main(int argc, char **argv)
{
    int i;
    double o, v;
    for (i = 0; i < 128; i++) SVF_G[i] = (uint32_t)(tan(M_PI * 30.0 * pow(16000.0 / 30.0, i / 127.0) / 44100.0) * 4096.0);
    if (!strcmp(argv[1], "gain")) {
        double f = atof(argv[2]);
        v = run(f, 2, 0.1, atoi(argv[3]), atoi(argv[4]), atoi(argv[5]), atoi(argv[6]), atoi(argv[7]), &o, 0, 0);
        printf("%.3f\n", 20 * log10(o / v));
    } else if (!strcmp(argv[1], "noise")) {
        run(1000, 2, 0.0, 0, 0, 0, atoi(argv[2]), 0, &o, 0, 0);
        printf("%.1f\n", o);
    } else if (!strcmp(argv[1], "dips")) {
        int dips = 0;
        run(1000, 20, 0.3, 0, atoi(argv[2]), 0, 0, 0, &o, &dips, 0);
        printf("%d\n", dips);
    } else {
        double dev = 0;
        run(1000, 8, 0.3, atoi(argv[2]), 0, 0, 0, 0, &o, 0, &dev);
        printf("%.3f\n", dev);
    }
    return 0;
}
