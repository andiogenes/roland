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
    if (ins_cfg[0]) {
        trk[3].p[P_ITYPE] = ins_cfg[0]; trk[3].p[P_IA] = ins_cfg[1]; trk[3].p[P_IB] = ins_cfg[2];
        trk[3].p[P_IC] = ins_cfg[3]; trk[3].p[P_ID] = ins_cfg[4]; trk[3].p[P_IE] = ins_cfg[5];
    }
    for (i = 0; i < 3; i++)                           /* the other parts silent */
        trk[i].p[P_LEVEL] = 0;
    song.master_q12 = 4096;
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
