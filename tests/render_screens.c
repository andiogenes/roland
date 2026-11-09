/* SPDX-License-Identifier: GPL-3.0-only */
/* Draws the drum grid and the DRUM edit pages for chosen kits with the firmware's own UI code (Felucca's tests/ui_render.c is
 * the host display: it includes the UI sources). Built by tests/host_render.sh.
 *   render_screens OUTDIR     (OUTDIR/ppm exists) -> OUTDIR/ppm/GREY_grid_<KIT>.ppm, GREY_kit_<KIT>.ppm, GREY_hit_<KIT>.ppm */
#include "ui_render_lib.c"      /* tests/ui_render.c with its main renamed (host_render.sh makes it): ui_test.c, which it
                                 * includes, undefines any #define main on the way */

int main(int argc, char **argv)
{
    const char *out = argc > 1 ? argv[1] : "build/shots";
    char path[600], name[64];
    uint32_t k;
    (void)path;                                                 /* (OUTDIR/ppm must exist: host_render.sh) */
    rep = fopen("/dev/null", "w");
    for (k = 0; k < DK_COUNT; k++) {
        const char *kn = ENG_DRUM.edit[0].names[k];
        uint32_t pg;
        if (enum_orig(&ENG_DRUM.edit[0], (int32_t)k) != (int32_t)k)
            continue;
        for (pg = 0; pg < 3u; pg++) {
            setup(S_DRUM);                                      /* a playing song, the drum track selected, its grid */
            trk[3].p[P_E0] = (int16_t)k;
            if (pg == 0u) {
                go_page(GR_ROLL); ui.cursor = 4; ui.lane = 1; trk[3].seq_idx = 9;
            } else {
                go_title(pg == 1u ? "EDIT 1" : "EDIT 2");
            }
            pal(UI_GREY_INDEX);
            draw(S_DRUM);
            snprintf(name, sizeof name, "%s_%s", pg == 0u ? "grid" : pg == 1u ? "kit" : "hit", kn);
            write_ppm(out, "GREY", name);
        }
    }
    {   /* the INSERT's four devices (ui_faces.c), each with its two pages: values turned away from their defaults */
        static const struct { const char *name; int16_t v[5]; } DEV[5] = {
            {"COMP", {80, 100, 20, 90, 2}}, {"PRE", {100, 90, 24, -20, 1}},
            {"HARM", {100, 70, 2, 0, 0}}, {"METAL", {90, -30, 25, 110, 0}}, {"ROT", {60, 30, 70, 40, 25}}};
        uint32_t d, pi, q;
        for (d = 0; d < 5u; d++)
            for (pi = 0; pi < 2u; pi++) {
                setup(S_DRUM);
                trk[3].p[P_ITYPE] = (int16_t)(d + 1u);
                trk[3].p[P_IA] = DEV[d].v[0]; trk[3].p[P_IB] = DEV[d].v[1]; trk[3].p[P_IC] = DEV[d].v[2];
                trk[3].p[P_ID] = DEV[d].v[3]; trk[3].p[P_IE] = DEV[d].v[4];
                go_title(pi ? "INSERT 2" : "INSERT");
                snprintf(name, sizeof name, "insert%u_%s", pi + 1u, DEV[d].name);
                pal(UI_GREY_INDEX);
                draw(S_DRUM);
                write_ppm(out, "GREY", name);
                for (q = 0; q < NPALETTES && strcmp(UI_PALETTES[q].name, "NIGHT"); q++)   /* and in colour: the devices' paint */
                    ;
                setup(S_DRUM);
                trk[3].p[P_ITYPE] = (int16_t)(d + 1u);
                trk[3].p[P_IA] = DEV[d].v[0]; trk[3].p[P_IB] = DEV[d].v[1]; trk[3].p[P_IC] = DEV[d].v[2];
                trk[3].p[P_ID] = DEV[d].v[3]; trk[3].p[P_IE] = DEV[d].v[4];
                go_title(pi ? "INSERT 2" : "INSERT");
                pal(q);
                draw(S_DRUM);
                write_ppm(out, "NIGHT", name);
            }
    }
    {   /* the master chain: MASTER A with PRE, MASTER B with COMP, in colour */
        static const int16_t M[2][6] = {{IT_PRE, 100, 90, 24, -20, 1}, {IT_COMP, 80, 100, 20, 90, 2}};
        uint32_t m, q;
        for (q = 0; q < NPALETTES && strcmp(UI_PALETTES[q].name, "NIGHT"); q++)
            ;
        for (m = 0; m < 2u; m++) {
            setup(S_DRUM);
            mtrk[m].p[P_ITYPE] = M[m][0]; mtrk[m].p[P_IA] = M[m][1]; mtrk[m].p[P_IB] = M[m][2];
            mtrk[m].p[P_IC] = M[m][3]; mtrk[m].p[P_ID] = M[m][4]; mtrk[m].p[P_IE] = M[m][5];
            go_title(m ? "MASTER B" : "MASTER A");
            pal(q);
            draw(S_DRUM);
            write_ppm(out, "NIGHT", m ? "master_B" : "master_A");
        }
        mtrk[0].p[P_ITYPE] = mtrk[1].p[P_ITYPE] = 0;
    }
    {   /* the REVERB page: DECAY SIZE DAMP of the Dragonfly Room */
        uint32_t q;
        for (q = 0; q < NPALETTES && strcmp(UI_PALETTES[q].name, "NIGHT"); q++)
            ;
        setup(S_DRUM);
        go_title("REVERB");
        pal(q);
        draw(S_DRUM);
        write_ppm(out, "NIGHT", "reverb_page");
    }
    {   /* the same grid and KIT page in every palette (GREY MONO GREEN PAPER NIGHT) */
        static const char *const PALS[] = {"GREY", "MONO", "GREEN", "PAPER", "NIGHT"};
        uint32_t q, pi;
        for (q = 0; q < sizeof PALS / sizeof PALS[0]; q++) {
            for (pi = 0; pi < NPALETTES && strcmp(UI_PALETTES[pi].name, PALS[q]); pi++)
                ;
            if (pi == NPALETTES)
                continue;
            setup(S_DRUM);
            trk[3].p[P_E0] = (int16_t)DK_606;
            go_page(GR_ROLL); ui.cursor = 4; ui.lane = 1; trk[3].seq_idx = 9;
            pal(pi);
            draw(S_DRUM);
            write_ppm(out, PALS[q], "grid_TR606");
        }
    }
    return 0;
}
