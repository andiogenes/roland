/* SPDX-License-Identifier: GPL-3.0-only */
/* dfly_host SIZE DECAY DAMP OUT.raw: fx_dfly.c's impulse response at 22.05 kHz (4 s), float32 x 4 a sample (early L R, late L R), in units of the
 * input impulse (8192 = 1.0), the format of tests/dfroom_ref.cpp. Prints the pool used. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
static int32_t mulq15(int32_t a, int32_t b) { return (a * b) >> 15; }
static int32_t mulq16(int32_t a, uint32_t k) { return (int32_t)(((a >> 16) * (int32_t)k) + (int32_t)(((uint32_t)(a & 0xFFFF) * k) >> 16)); }
static int32_t clamp(int32_t v, int32_t lo, int32_t hi) { return v < lo ? lo : v > hi ? hi : v; }
static int32_t osc_sine(uint32_t ph) { return (int32_t)lround(32767.0 * sin(ph * (2 * M_PI / 4294967296.0))); }
#include "../overlay/firmware/src/fx_dfly.c"
int main(int argc, char **argv)
{
    int n = 22050 * 4, i;
    FILE *f = fopen(argv[4], "wb");
    dfr_setup(atoi(argv[1]), atoi(argv[2]), atoi(argv[3]));
    dfr_damp(atoi(argv[3]));
    fprintf(stderr, "pool used %ld of %d samples, bassb %d, loopdecay %d\n", (long)(dfr.p - dfr_pool), DFR_POOL, (int)dfr.bassb, (int)dfr.loopdecay);
    for (i = 0; i < n; i++) {
        int32_t eL, eR, lL, lR;
        float o[4];
        dfr_step(i == 0 ? 8192 : 0, &eL, &eR, &lL, &lR);
#ifdef DFR_DBG
        if (i < 3000 && getenv("DFR_DBGOUT")) fprintf(stderr, "%d %g %g %g %g\n", i, dfr_dbg[1] / 8192.0, dfr_dbg[2] / 8192.0, dfr_dbg[3] / 8192.0, dfr_dbg[4] / 8192.0);
#endif
        o[0] = eL / 8192.0f; o[1] = eR / 8192.0f; o[2] = lL / 8192.0f; o[3] = lR / 8192.0f;
        fwrite(o, 4, 4, f);
    }
    fclose(f);
    return 0;
}
