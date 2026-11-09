/* SPDX-License-Identifier: GPL-3.0-only */
/* ivp_host LEVEL GAIN BASS TREBLE RANGE AMPLITUDE_VOLTS [FREQ]: fx_ivp.c on a sine (volts at the input, 0.25 V = full scale), prints the
 * output's harmonics 1..4 in volts at the output (1 V = 0.3 of full scale), over the last 0.2 s of 0.4 s. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
static int32_t mulq16(int32_t a, uint32_t k) { return (int32_t)(((a >> 16) * (int32_t)k) + (int32_t)(((uint32_t)(a & 0xFFFF) * k) >> 16)); }
static int32_t clamp(int32_t v, int32_t lo, int32_t hi) { return v < lo ? lo : v > hi ? hi : v; }
#include "../overlay/firmware/src/fx_ivp.c"
int main(int argc, char **argv)
{
    ivp_t s;
    ivp_k_t k;
    int n = 44100 * 4 / 10, i, h;
    double amp = atof(argv[6]), f = argc > 7 ? atof(argv[7]) : 1000.0, re[5] = {0}, im[5] = {0}, cnt = 0;
    memset(&s, 0, sizeof s);
    ivp_coefs(&k, atoi(argv[1]), atoi(argv[2]), atoi(argv[3]), atoi(argv[4]), atoi(argv[5]));
    for (i = 0; i < n; i++) {
        int32_t x = (int32_t)lround(amp / 0.25 * 32768.0 * sin(2 * M_PI * f * i / 44100.0)), y = ivp_one(&s, &k, x);
        if (i >= n / 2) {
            for (h = 1; h <= 4; h++) {
                re[h] += y * cos(2 * M_PI * f * h * i / 44100.0);
                im[h] += y * sin(2 * M_PI * f * h * i / 44100.0);
            }
            cnt++;
        }
    }
    for (h = 1; h <= 4; h++)
        printf("%.4f ", 2 * sqrt(re[h] * re[h] + im[h] * im[h]) / cnt / (0.3 * 32768.0));
    printf("\n");
    return 0;
}
