/* SPDX-License-Identifier: GPL-3.0-only */
/* Host build of the sample-kit player of patches/felucca-drum.patch (hr_next, hr_start, hr_choke, hr_run, extracted by
 * tests/test_acoustic.py into PLAYER_FILE) against minimal stand-ins for Felucca's types. Plays lane 0..7 of each kit at the
 * knobs given and prints: kit lane samples-until-end peak min-sample max-multiple-of-256-violations. */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define FELUCCA_SMPKITS 1
enum { P_E0, P_E1, P_E2, P_E3, P_COUNT };
#define NLANE 8
#define NVOICE 8
#define DV_HATC 3
#define DV_HATO 4
#define DK_HR 13
typedef struct { uint8_t note, vel, gate, active; uint8_t stage, side; int32_t env, env_out, pitch16, pitch_cur, gstep, fine;
                 uint32_t ph[3]; int32_t s[8]; uint32_t age; uint8_t mvel; int16_t mrnd; } voice_t;
typedef struct track { int16_t p[P_COUNT]; voice_t v[NVOICE]; } track_t;
typedef struct { uint8_t owner; } drum_lane_t;
static inline int32_t clamp(int32_t v, int32_t lo, int32_t hi) { return v < lo ? lo : v > hi ? hi : v; }
static inline int32_t mulq15(int32_t a, int32_t b) { return (a * b) >> 15; }
static uint32_t pow2_q16(int32_t d16) { return (uint32_t)lrint(65536.0 * pow(2.0, d16 / 192.0)); }
static const int16_t IMA_STEP[89] = {IMA_STEP_INIT};
static const int8_t IMA_IDX[8] = {-1, -1, -1, -1, 2, 4, 6, 8};
#include "felucca_hr16.h"
typedef struct { uint32_t off, n, rate; } hr_samp_t_unused;   /* (the player defines hr_samp_t itself) */
#include PLAYER_FILE

int main(int argc, char **argv)
{
    int e1 = argc > 1 ? atoi(argv[1]) : 64, e2 = argc > 2 ? atoi(argv[2]) : 64, e3 = argc > 3 ? atoi(argv[3]) : 64;
    int note = argc > 4 ? atoi(argv[4]) : 45;
    for (int kit = 0; kit < HR16_NKITS; kit++)
        for (int lane = 0; lane < 8; lane++) {
            track_t t;
            memset(&t, 0, sizeof t);
            t.p[P_E0] = (int16_t)(DK_HR + kit); t.p[P_E1] = (int16_t)e1; t.p[P_E2] = (int16_t)e2; t.p[P_E3] = (int16_t)e3;
            voice_t *v = &t.v[0];
            v->note = (uint8_t)(lane == 0 ? 36 : note); v->active = 1;
            hr_start(&t, v, lane);
            int32_t y[32];
            long total = 0, peak = 0, bad = 0, ended = 0;
            for (int b = 0; b < 4 * 44100 / 32; b++) {
                hr_run(&t, v, 0, y, 32);
                for (int i = 0; i < 32; i++, total++) {
                    long a = labs(y[i]);
                    if (a > peak) peak = a;
                    if (kit == 4 && (y[i] & 255)) bad++;
                }
                if (v->s[4] & HRV_END) { ended = total; break; }
            }
            printf("%d %d %ld %ld %ld\n", kit, lane, ended, peak, bad);
        }
    {   /* the choke: an open hat 50 ms in */
        track_t t; memset(&t, 0, sizeof t);
        drum_lane_t K[NLANE]; memset(K, 0, sizeof K);
        t.p[P_E0] = DK_HR; t.p[P_E1] = 64; t.p[P_E2] = 64; t.p[P_E3] = 64;
        voice_t *v = &t.v[0]; v->note = 46; v->active = 1;
        hr_start(&t, v, 4);
        K[DV_HATO].owner = 1;
        int32_t y[32]; long total = 0, ended = 0;
        for (int b = 0; b < 4 * 44100 / 32; b++) {
            if (total >= 2205) hr_choke(&t, K);
            hr_run(&t, v, 0, y, 32); total += 32;
            if (v->s[4] & HRV_END) { ended = total; break; }
        }
        printf("C %ld\n", ended);
    }
    return 0;
}
