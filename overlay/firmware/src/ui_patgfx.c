/* SPDX-License-Identifier: GPL-3.0-only */
/* PATTERN's picture (ui_pat.c): the eight patterns as tiles in two rows (dim: its slot is not saved; filled: playing
 * now; framed in the theme: the last picked, and the ones queued), under them the chain as a strip of cells, and a line
 * with what the keys do. Included by ui_graph.c. */
#define PT_X0 10
#define PT_TW 50
#define PT_TG 4
#define PT_TH 19
#define PT_Y0 3
#define PT_CY 54                                       /* the chain strip */
#define PT_IY 119                                      /* the line under the strip (as the song's SG_IY) */
#define PT_CW 13
#define PT_CH 15

static int32_t pt_digit_y(const aafont_t *f, int32_t cy, int32_t cap_y, int32_t cap_h) { (void)f; return cy - cap_y - cap_h / 2; }
static void graph_pat(void)
{
    const aafont_t *fl = &AF_M, *fs = &AF_S;
    uint32_t p, i, now = dp_now(), run = chain.running != 0, q = 0;
    int32_t ly = PT_IY - (AF_M_CAP_Y + AF_M_CAP_H);
    char b[24], d[4];
    for (i = chain.qh; i != chain.qt && run; i++)
        q |= 1u << ((chain.q[i & 7u] - CHAIN_PAT0) & 7u);
    for (p = 0; p < CHAIN_PATN; p++) {
        int32_t x = PT_X0 + (int32_t)(p & 3u) * (PT_TW + PT_TG), y = PT_Y0 + (int32_t)(p >> 2) * (PT_TH + 3);
        int ok = dp_ok(p), playing = run && p == now, picked = (!run && p == dpat.cur) || ((q >> p) & 1u);
        uint16_t bg = playing ? T_ACCENT : T_SURF, fg = playing ? T_BG : ok ? T_MID : T_DIM;
        cv_rect(x, y, PT_TW, PT_TH, bg);
        cv_frame(x, y, PT_TW, PT_TH, picked ? T_THEME : ok ? T_LINE : T_DIM);
        d[0] = (char)('1' + p);
        d[1] = 0;
        cv_text_on(x + 7, y + pt_digit_y(fl, PT_TH / 2, AF_M_CAP_Y, AF_M_CAP_H), fl, d, fg, bg);
        d[0] = (char)('A' + (p >> 1));                  /* its slot, and the half */
        d[1] = (p & 1u) ? '2' : '1';
        d[2] = 0;
        cv_text_on(x + PT_TW - 6 - text_w(fs, d), y + PT_TH / 2 - AF_S_CAP_Y - AF_S_CAP_H / 2, fs, d,
                   playing ? T_BG : T_DIM, bg);
    }
    for (i = 0; i < CHAIN_ROWS; i++) {                  /* the chain */
        int32_t x = PT_X0 + (int32_t)i * (PT_CW + 1);
        int have = run ? i < chain.config.count : i < dpat.n, on = run && have && i == chain.row && !chain.inovr;
        uint16_t bg = on ? T_ACCENT : T_SURF;
        if (!have) {
            cv_rect(x, PT_CY + PT_CH - 1, PT_CW, 1, T_LINE);
            continue;
        }
        cv_rect(x, PT_CY, PT_CW, PT_CH, bg);
        cv_frame(x, PT_CY, PT_CW, PT_CH, on ? T_ACCENT : T_LINE);
        {
            uint32_t id = run ? (uint32_t)(chain.config.row[i].slot[0] - CHAIN_PAT0) & 7u : dpat.id[i];
            d[0] = (char)('1' + (id & 7u));
            d[1] = 0;
            cv_text_on(x + (PT_CW - text_w(fs, d)) / 2, PT_CY + PT_CH / 2 - AF_S_CAP_Y - AF_S_CAP_H / 2, fs, d,
                       on ? T_BG : T_MID, bg);
        }
    }
    if (dp_queued() && run) {                          /* NEXT 3 5 */
        uint32_t n = 0;
        str_cpy(b, "NEXT", sizeof b);
        n = str_len(b);
        for (i = chain.qh; i != chain.qt && n < 20u; i++) {
            b[n++] = ' ';
            b[n++] = (char)('1' + ((chain.q[i & 7u] - CHAIN_PAT0) & 7u));
        }
        b[n] = 0;
        cv_free_text(PT_X0, PT_CY + PT_CH + 5, fs, b, T_MID, T_SURF, 200);
    }
    if (run) {
        str_cpy(b, "PLAYING ", sizeof b);
        b[8] = (char)('1' + (now & 7u));
        b[9] = 0;
    } else if (dpat.n) {
        fmt_int(b, (int32_t)dpat.n);
        str_cpy(b + str_len(b), " IN CHAIN", 12);
    } else {
        str_cpy(b, "KEYS PICK PATTERNS", sizeof b);
    }
    cv_free_text(PT_X0, ly, fl, b, run ? T_ACCENT : T_MID, T_SURF, 220);
}
