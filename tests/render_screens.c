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
    return 0;
}
