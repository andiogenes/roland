/* SPDX-License-Identifier: GPL-3.0-only */
/* hm2_host DIST LEVEL LOW HIGH AMPLITUDE_VOLTS [FREQ]: fx_hm2.c on a sine (volts at the pedal's input, 0.25 V = full scale), prints
 * the output's harmonics 1..4 in volts at the pedal's output (1 V = 0.65 of full scale), over the last 0.2 s of 0.4 s. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
static int32_t mulq16(int32_t a, uint32_t k) { return (int32_t)(((a >> 16) * (int32_t)k) + (int32_t)(((uint32_t)(a & 0xFFFF) * k) >> 16)); }
static int32_t clamp(int32_t v, int32_t lo, int32_t hi) { return v < lo ? lo : v > hi ? hi : v; }
#include "../overlay/firmware/src/fx_hm2.c"
int main(int argc, char **argv)
{
    hm2_t s;
    hm2_k_t k;
    int n = 44100 * 4 / 10, i, h;
    double amp = atof(argv[5]), f = argc > 6 ? atof(argv[6]) : 1000.0, re[5] = {0}, im[5] = {0}, cnt = 0;
    memset(&s, 0, sizeof s);
    hm2_coefs(&k, atoi(argv[1]), atoi(argv[2]), atoi(argv[3]), atoi(argv[4]));
    for (i = 0; i < n; i++) {
        int32_t x = (int32_t)lround(amp / 0.25 * 32768.0 * sin(2 * M_PI * f * i / 44100.0)), y = hm2_one(&s, &k, x);
        if (i >= n / 2) {
            for (h = 1; h <= 4; h++) {
                re[h] += y * cos(2 * M_PI * f * h * i / 44100.0);
                im[h] += y * sin(2 * M_PI * f * h * i / 44100.0);
            }
            cnt++;
        }
    }
    for (h = 1; h <= 4; h++)
        printf("%.4f ", 2 * sqrt(re[h] * re[h] + im[h] * im[h]) / cnt / (0.65 * 32768.0));
    printf("\n");
    return 0;
}
