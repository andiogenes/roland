/* SPDX-License-Identifier: GPL-3.0-only */
/* comp_host SINE_AMP A B C D E [step]: runs overlay's fx_comp.c on a 1 kHz sine of the given peak for 1 s and prints the
 * steady output peak, the gain reduction in dB at the end; with `step`: the amplitude jumps from 0.05 to AMP at 0.1 s and
 * the time (us) the reduction takes to reach 63 % of its final value, then (amp back to 0.05 at 1.1 s) the time to fall 63 %. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "../overlay/firmware/src/fx_comp.c"
int main(int argc, char **argv)
{
    comp_t s = {0, 0};
    comp_k_t k;
    double amp = atof(argv[1]), fs = 44100;
    int i, n = 3 * 44100, step = argc > 7, t63u = -1, t63d = -1;
    double final = 0, pk = 0;
    comp_coefs(&k, atoi(argv[2]), atoi(argv[3]), atoi(argv[4]), atoi(argv[5]), atoi(argv[6]));
    for (i = 0; i < n; i++) {
        double a = step ? (i < 4410 || i >= 4410 + 44100 ? 0.05 : amp) : amp;
        int32_t x = (int32_t)(a * 32768 * sin(2 * M_PI * 1000 * i / fs)), y = comp_one(&s, &k, x);
        if (!step && i > n - 4410 && abs(y) > pk) pk = abs(y);
        if (step) {
            double gr = s.gr / 65536.0 * 6.0206;
            if (i == 4410 + 44100 - 1) final = gr;
        }
    }
    if (!step) {
        printf("%.4f %.3f\n", pk / 32768, s.gr / 65536.0 * 6.0206);
        return 0;
    }
    {   /* second pass for the times */
        comp_t s2 = {0, 0};
        double g0 = 0, peak = 0;
        for (i = 0; i < n; i++) {
            double a = i < 4410 || i >= 4410 + 44100 ? 0.05 : amp, gr;
            int32_t x = (int32_t)(a * 32768 * sin(2 * M_PI * 1000 * i / fs));
            comp_one(&s2, &k, x);
            gr = s2.gr / 65536.0 * 6.0206;
            if (i == 4409) g0 = gr;
            if (i >= 4410 && t63u < 0 && gr - g0 >= 0.63 * (final - g0)) t63u = (int)((i - 4410) * 1e6 / fs);
            if (i >= 4410 + 44100) {
                if (gr > peak) peak = gr;
                if (t63d < 0 && gr <= g0 + 0.37 * (final - g0)) t63d = (int)((i - 4410 - 44100) * 1e6 / fs);
            }
        }
        printf("%.3f %d %d\n", final, t63u, t63d);
    }
    return 0;
}
