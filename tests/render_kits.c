/* SPDX-License-Identifier: GPL-3.0-only */
/* Renders every DRUM kit of the drum machine build through the real firmware sources (Felucca's tests/hostsim.c includes
 * the engines, voices, FX and sequencer) to WAV files: a two-bar GM beat on part 4, each kit alone, then all of them in one
 * file. Built by tests/host_render.sh.
 *   render_kits OUTDIR          prints per kit: index, name, peak, RMS, clipped samples */
#define main hostsim_main
#include "hostsim.c"
#undef main

#define SIXTEENTH (FS / 8u)         /* 120 BPM: 125 ms */
/* the beat: per 16th up to four GM notes (0 ends the list). Bar 1 / bar 2 */
static const uint8_t BEAT[32][4] = {
    {36, 42, 0}, {0}, {42, 0}, {37, 0},  {38, 42, 0}, {0}, {42, 0}, {45, 0},
    {36, 42, 0}, {0}, {36, 42, 0}, {41, 0}, {38, 39, 42, 0}, {0}, {46, 0}, {56, 0},
    {36, 49, 0}, {0}, {42, 0}, {42, 0}, {38, 42, 0}, {0}, {42, 0}, {47, 0},
    {36, 42, 0}, {0}, {36, 42, 0}, {37, 0}, {38, 39, 0}, {43, 0}, {46, 0}, {48, 0},
};

static int16_t master_cfg;                          /* the master level, Q12 (0: 4096) */
static int16_t rev_cfg;                             /* the part's reverb send (0: none) */
static int16_t dly_cfg;                             /* the part's delay send (0: none) */
static int16_t ins_cfg[6];                          /* INSERT: TYPE, A .. E (TYPE 0: none) */

static const char *kit_name(uint32_t k)
{
    const param_desc_t *d = &ENG_DRUM.edit[0];
    return d->names[k];
}

/* one kit, `bars` of the beat on part 4 into f (NULL: not written); returns the peak, fills rms and clips */
static int32_t play_kit(uint32_t kit, FILE *f, double *rms, uint32_t *clips)
{
    uint32_t i, frames = FS * 4u + FS, step = 0, fr;
    int32_t peak = 0;
    double acc = 0;
    uint64_t n = 0;
    uint8_t held[4] = {0}, nheld = 0;
    host_tracks_init();
    memset(dly_buf, 0, sizeof dly_buf);                  /* each kit from silence: the delay and reverb tails, the limiter and the */
    rev_clear();                                         /* DC blockers of the kit before must not reach this one */
    fx.dly_lp = 0;
    lim_env = LIM_T;
    dc_l = dc_r = dce_l = dce_r = 0;
    host_drums(&trk[3]);
    trk[3].p[P_E0] = (int16_t)kit;
    if (dly_cfg)
        trk[3].p[P_DLY] = dly_cfg;
    if (rev_cfg)
        trk[3].p[P_REV] = rev_cfg;
    if (ins_cfg[0]) {
        trk[3].p[P_ITYPE] = ins_cfg[0]; trk[3].p[P_IA] = ins_cfg[1]; trk[3].p[P_IB] = ins_cfg[2];
        trk[3].p[P_IC] = ins_cfg[3]; trk[3].p[P_ID] = ins_cfg[4]; trk[3].p[P_IE] = ins_cfg[5];
    }
    for (i = 0; i < 3; i++)                           /* the other parts silent */
        trk[i].p[P_LEVEL] = 0;
    song.master_q12 = master_cfg ? master_cfg : 4096;
    *clips = 0;
    for (fr = 0; fr < frames; fr += CTL) {
        int32_t o[2 * CTL];
        if (fr % SIXTEENTH < CTL && step < 32u && fr / SIXTEENTH == step) {
            for (i = 0; i < nheld; i++)
                trk_note_off(&trk[3], held[i]);
            nheld = 0;
            for (i = 0; i < 4u && BEAT[step][i]; i++) {
                trk_note_on(&trk[3], BEAT[step][i], 100);
                held[nheld++] = BEAT[step][i];
            }
            step++;
        }
        mix_block(o, CTL);
        for (i = 0; i < 2 * CTL; i++) {
            int32_t s = o[i];
            if (s > 32767 || s < -32768)
                (*clips)++;
            s = s > 32767 ? 32767 : s < -32768 ? -32768 : s;
            if (s > peak) peak = s;
            if (-s > peak) peak = -s;
            acc += (double)s * s;
            n++;
            if (f) {
                int16_t v = (int16_t)s;
                fwrite(&v, 2, 1, f);
            }
        }
    }
    *rms = sqrt(acc / (double)n);
    return peak;
}

int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : ".";
    char path[512];
    uint32_t k, shown = 0;
    FILE *all;
    snprintf(path, sizeof path, "%s/all_kits.wav", dir);
    all = fopen(path, "wb");
    if (!all)
        return 1;
    wav_hdr(all, 0);                                  /* (the sizes are patched below) */
    printf("kits: %u (DK_COUNT)\n", (unsigned)DK_COUNT);
    for (k = 0; k < DK_COUNT; k++) {
        double rms;
        uint32_t clips;
        int32_t peak;
        FILE *f;
        if (enum_orig(&ENG_DRUM.edit[0], (int32_t)k) != (int32_t)k)
            continue;                                 /* a retired alias */
        snprintf(path, sizeof path, "%s/kit_%02u_%s.wav", dir, (unsigned)k, kit_name(k));
        f = fopen(path, "wb");
        wav_hdr(f, FS * 5u);
        peak = play_kit(k, f, &rms, &clips);
        fclose(f);
        printf("%2u %-6s peak %5d rms %6.0f clipped %u\n", (unsigned)k, kit_name(k), (int)peak, rms, (unsigned)clips);
        {   /* the same into the combined file */
            double r2;
            uint32_t c2;
            play_kit(k, all, &r2, &c2);
        }
        shown++;
    }
    {   /* the INSERT's COMP on the TR606 kit: INPUT 100, OUT 80, fast attack, medium release, 12:1 */
        static const int16_t C[6] = {IT_COMP, 100, 80, 20, 50, 2};
        double rms;
        uint32_t clips;
        int32_t peak;
        FILE *f;
        memcpy(ins_cfg, C, sizeof ins_cfg);
        if (argc > 2)                                     /* render_kits OUTDIR a,b,c,d,e: other knob values */
            sscanf(argv[2], "%hd,%hd,%hd,%hd,%hd", &ins_cfg[1], &ins_cfg[2], &ins_cfg[3], &ins_cfg[4], &ins_cfg[5]);
        snprintf(path, sizeof path, "%s/insert_comp_TR606.wav", dir);
        f = fopen(path, "wb");
        wav_hdr(f, FS * 5u);
        peak = play_kit(DK_606, f, &rms, &clips);
        fclose(f);
        printf("COMP on TR606: peak %5d rms %6.0f clipped %u\n", (int)peak, rms, (unsigned)clips);
        memset(ins_cfg, 0, sizeof ins_cfg);
    }
    {   /* the INSERT's ROT on the TR606 kit: wow 60, dropouts 40, loss 70, hiss 50, drive 30 */
        static const int16_t C[6] = {IT_ROT, 60, 40, 70, 50, 30};
        double rms;
        uint32_t clips;
        int32_t peak;
        FILE *f;
        memcpy(ins_cfg, C, sizeof ins_cfg);
        snprintf(path, sizeof path, "%s/insert_rot_TR606.wav", dir);
        f = fopen(path, "wb");
        wav_hdr(f, FS * 5u);
        peak = play_kit(DK_606, f, &rms, &clips);
        fclose(f);
        printf("ROT on TR606: peak %5d rms %6.0f clipped %u\n", (int)peak, rms, (unsigned)clips);
        memset(ins_cfg, 0, sizeof ins_cfg);
    }
    {   /* the INSERT's METAL (the HM-2) on the TR606 kit: LEVEL 70, LOW +10, HIGH 0, DIST 50 */
        static const int16_t C[6] = {IT_METAL, 70, 10, 0, 50, 0};
        double rms;
        uint32_t clips;
        int32_t peak;
        FILE *f;
        memcpy(ins_cfg, C, sizeof ins_cfg);
        snprintf(path, sizeof path, "%s/insert_metal_TR606.wav", dir);
        f = fopen(path, "wb");
        wav_hdr(f, FS * 5u);
        peak = play_kit(DK_606, f, &rms, &clips);
        fclose(f);
        printf("METAL on TR606: peak %5d rms %6.0f clipped %u\n", (int)peak, rms, (unsigned)clips);
        memset(ins_cfg, 0, sizeof ins_cfg);
    }
    {   /* the INSERT's HARM (the Percolator) on the TR606 kit: HARMONICS 100, BALANCE 90, MODE GE */
        static const int16_t C[6] = {IT_HARM, 100, 90, 0, 0, 0};
        double rms;
        uint32_t clips;
        int32_t peak;
        FILE *f;
        memcpy(ins_cfg, C, sizeof ins_cfg);
        snprintf(path, sizeof path, "%s/insert_harm_TR606.wav", dir);
        f = fopen(path, "wb");
        wav_hdr(f, FS * 5u);
        peak = play_kit(DK_606, f, &rms, &clips);
        fclose(f);
        printf("HARM on TR606: peak %5d rms %6.0f clipped %u\n", (int)peak, rms, (unsigned)clips);
        memset(ins_cfg, 0, sizeof ins_cfg);
    }
    {   /* the INSERT's PRE (the Surveyor-style preamp) on the TR606 kit: LEVEL 90, GAIN 50, BASS +20, TREBLE +10, RANGE LO */
        static const int16_t C[6] = {IT_PRE, 90, 50, 20, 10, 0};
        double rms;
        uint32_t clips;
        int32_t peak;
        FILE *f;
        memcpy(ins_cfg, C, sizeof ins_cfg);
        snprintf(path, sizeof path, "%s/insert_pre_TR606.wav", dir);
        f = fopen(path, "wb");
        wav_hdr(f, FS * 5u);
        peak = play_kit(DK_606, f, &rms, &clips);
        fclose(f);
        printf("PRE on TR606: peak %5d rms %6.0f clipped %u\n", (int)peak, rms, (unsigned)clips);
        memset(ins_cfg, 0, sizeof ins_cfg);
    }
    {   /* the master chain: slot A COMP (INPUT 80, OUT 90, 4:1), slot B HARM (HARMONICS 60, BALANCE 100, MODE GE, MIX 50) on the whole mix */
        double rms;
        uint32_t clips;
        int32_t peak;
        FILE *f;
        mtrk[0].p[P_ITYPE] = IT_COMP; mtrk[0].p[P_IA] = 80; mtrk[0].p[P_IB] = 90; mtrk[0].p[P_IC] = 30; mtrk[0].p[P_ID] = 60; mtrk[0].p[P_IE] = 0; mtrk[0].p[P_IMIX] = 127;
        mtrk[1].p[P_ITYPE] = IT_HARM; mtrk[1].p[P_IA] = 60; mtrk[1].p[P_IB] = 100; mtrk[1].p[P_IC] = 0; mtrk[1].p[P_IMIX] = 50;
        snprintf(path, sizeof path, "%s/insert_master_TR606.wav", dir);
        f = fopen(path, "wb");
        wav_hdr(f, FS * 5u);
        peak = play_kit(DK_606, f, &rms, &clips);
        fclose(f);
        printf("master chain on TR606: peak %5d rms %6.0f clipped %u\n", (int)peak, rms, (unsigned)clips);
        mtrk[0].p[P_ITYPE] = mtrk[1].p[P_ITYPE] = 0;
    }
    {   /* the reverb: the TR606 beat dry and with the reverb send at 64, the master 12 dB down (no limiter at work): the reverb's own level is
         * the difference, what a change of reverb must keep */
        double rms;
        uint32_t clips;
        int32_t peak;
        FILE *f;
        master_cfg = 1024;
        snprintf(path, sizeof path, "%s/reverb_off_TR606.wav", dir);
        f = fopen(path, "wb");
        wav_hdr(f, FS * 5u);
        play_kit(DK_606, f, &rms, &clips);
        fclose(f);
        rev_cfg = 64;
        snprintf(path, sizeof path, "%s/reverb_on_TR606.wav", dir);
        f = fopen(path, "wb");
        wav_hdr(f, FS * 5u);
        peak = play_kit(DK_606, f, &rms, &clips);
        fclose(f);
        printf("reverb on TR606 (master -12 dB): peak %5d rms %6.0f clipped %u\n", (int)peak, rms, (unsigned)clips);
        master_cfg = 0;
        snprintf(path, sizeof path, "%s/insert_reverb_TR606.wav", dir);       /* the same at the normal master level: the clip */
        f = fopen(path, "wb");
        wav_hdr(f, FS * 5u);
        play_kit(DK_606, f, &rms, &clips);
        fclose(f);
        rev_cfg = 0;
    }
    {   /* a hit struck again while it still sounds must start over (a sample kit used to go on from where it was: the voice's phases were
         * kept over the retrigger, voice.c): the kick of every sample kit (ACOUS .. HR-x, the Drumulator's) twice, 100 ms apart; the first
         * 20 ms of the second hit against the first's, the lowest over the kits */
        uint32_t kit, k2;
        double worst = 1e9;
        const char *wn = "";
        for (kit = DK_HR; kit < DK_COUNT; kit++) {
            double e1 = 0, e2 = 0;
            if (enum_orig(&ENG_DRUM.edit[0], (int32_t)kit) != (int32_t)kit)
                continue;
            host_tracks_init();
            memset(dly_buf, 0, sizeof dly_buf);
            rev_clear();
            host_drums(&trk[3]);
            trk[3].p[P_E0] = (int16_t)kit;
            for (k2 = 0; k2 < 3; k2++)
                trk[k2].p[P_LEVEL] = 0;
            song.master_q12 = 4096;
            for (k2 = 0; k2 < FS / 2u; k2 += CTL) {
                int32_t o[2 * CTL];
                uint32_t i, t2 = (FS / 10u) / CTL * CTL;
                if (k2 == 0)
                    trk_note_on(&trk[3], 36, 100);
                if (k2 == t2)
                    trk_note_on(&trk[3], 36, 100);
                mix_block(o, CTL);
                for (i = 0; i < 2 * CTL; i += 2) {
                    uint32_t fr = k2 + i / 2u;
                    double x = o[i];
                    if (fr < FS / 50u)
                        e1 += x * x;
                    else if (fr >= t2 && fr < t2 + FS / 50u)
                        e2 += x * x;
                }
            }
            if (e1 > 0 && sqrt(e2 / e1) < worst) {
                worst = sqrt(e2 / e1);
                wn = kit_name(kit);
            }
        }
        printf("retrigger: the second hit's first 20 ms against the first's, lowest over the sample kits (%s): %.2f\n", wn, worst);
    }
    {   /* the delay as a tape echo: send 80, the delay's own defaults (FDBK 60 %, COLR 70, MIX 90) */
        double rms;
        uint32_t clips;
        int32_t peak;
        FILE *f;
        dly_cfg = 80;
        snprintf(path, sizeof path, "%s/insert_tape_delay_TR606.wav", dir);
        f = fopen(path, "wb");
        wav_hdr(f, FS * 5u);
        peak = play_kit(DK_606, f, &rms, &clips);
        fclose(f);
        printf("tape delay on TR606: peak %5d rms %6.0f clipped %u\n", (int)peak, rms, (unsigned)clips);
        dly_cfg = 0;
    }
    {   /* patch the combined file's sizes */
        uint32_t frames = shown * FS * 5u, v;
        fseek(all, 4, SEEK_SET); v = 36 + frames * 4; fwrite(&v, 4, 1, all);
        fseek(all, 40, SEEK_SET); v = frames * 4; fwrite(&v, 4, 1, all);
        fclose(all);
    }
    return 0;
}
