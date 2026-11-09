/* SPDX-License-Identifier: GPL-3.0-only */
/* tape_host: prints the facts about overlay's fx_tape.c that tests/test_tape.py checks:
 *   line 1: the largest |wander| in samples over 60 s of 32-sample blocks
 *   line 2: the largest pitch deviation in % (the wander's change a sample, as a fraction of the sample rate)
 *   line 3: the tap of an impulse written 1000 samples back: the sample, and with the read halfway between two samples
 *   line 4: the wander's rms in samples (so a quiet one is told from none) */
#include <stdint.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
static int32_t osc_sine(uint32_t ph) { return (int32_t)lround(32767.0 * sin(ph * (2 * M_PI / 4294967296.0))); }
#include "../overlay/firmware/src/fx_tape.c"
int main(void)
{
    tape_t tp = {{0, 0, 0}, 0};
    int32_t prev = 0, mx = 0, k;
    double dev = 0, acc = 0, nn = 0;
    static int16_t buf[65536];
    for (k = 0; k < 60 * 44100 / 32; k++) {
        int32_t m = tape_wander(&tp, 32), i;
        if (abs(m) > mx) mx = abs(m);
        for (i = 1; i <= 32; i++) {                      /* the block's ramp, as fx_buses draws it */
            double v = prev + (m - prev) * (double)i / 32.0, step = fabs(v - (prev + (m - prev) * (double)(i - 1) / 32.0)) / 256.0;
            if (k && step * 100 > dev) dev = step * 100;
            acc += (v / 256) * (v / 256); nn++;
        }
        prev = m;
    }
    printf("%.2f\n%.4f\n", mx / 256.0, dev);
    buf[(5000 - 1000) & 65535] = 12000;
    {
        int32_t a = tape_tap(buf, 65535, 5000, 1000, 0);
        buf[(5000 - 1000 + 1) & 65535] = 4000;
        printf("%d %d\n", a, tape_tap(buf, 65535, 5000, 1000, -128));
    }
    printf("%.3f\n", sqrt(acc / nn));
    return 0;
}
