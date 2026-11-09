/* SPDX-License-Identifier: GPL-3.0-only */
/* The INSERT's faces: each effect of the pool drawn as the device it models, its controls where the device has them and
 * turned to where the track's values put them. Included by ui_graph.c (DRUMONLY); the canvas is the graph panel (x 10..230,
 * y 4..92 on its 100 px scale).
 *
 *   COMP    a 1176-style limiting amplifier in its rack: INPUT OUTPUT ATTACK RELEASE, the gain-reduction meter, the ratio buttons
 *   PRE     the Surveyor-style pedal (the Intersound preamp's family): LEVEL GAIN BASS TREBLE and the HI / LO switch
 *   HARM    the Harmonic Percolator: two sliders, HARMONICS and BALANCE, and the clipping MODE
 *   METAL   the HM-2 pedal: LEVEL, the two COLOR MIX knobs LOW and HIGH, DIST
 *   ROT     a reel-to-reel deck: two reels, the head block and five knobs, WOW DROP LOSS HISS DRIVE
 *
 * The bodies keep the devices' own colours on every palette (they are pictures of boxes); only what the knobs are doing now
 * is the palette's: a ring (T_ACCENT) round the controls the open page edits. No maker's name or logo is drawn. The values
 * a face shows are the INSERT's A .. E in the order params.c ins_desc gives (and the pages INSERT and INSERT 2 turn). */

#define FC_X 10
#define FC_Y 4
#define FC_W 220
#define FC_H 88

/* a colour neutral in RGB565 (R = B, G within 2 of 2 R: the lint's test of GREY and MONO) */
static int fc_neutral(uint16_t c)
{
    int32_t r = c >> 11, g = (c >> 5) & 63, b = c & 31, d = g - 2 * r;
    return r == b && d >= -2 && d <= 2;
}
/* a device's colours are its own on every palette in colour; on GREY and MONO (every pixel gray / neutral) they are their grays */
static uint16_t fc_px(uint16_t c)
{
    return ux.mono || (fc_neutral(ux.bg) && fc_neutral(ux.text) && fc_neutral(ux.theme)) ? ux_gray(ux_luma(c) >> 3) : c;
}

/* the picture's colours (RGB565), a device's paint, not a theme's */
#define FC_SILVER   fc_px(RGB(198, 200, 206))
#define FC_SILVER_D fc_px(RGB(120, 122, 130))
#define FC_INK      fc_px(RGB(28, 28, 32))
#define FC_KNOB     fc_px(RGB(34, 34, 38))
#define FC_CHROME   fc_px(RGB(206, 208, 214))
#define FC_CREAM    fc_px(RGB(236, 216, 152))
#define FC_RED      fc_px(RGB(212, 44, 32))
#define FC_GOLD     fc_px(RGB(214, 188, 66))
#define FC_TAN      fc_px(RGB(216, 198, 150))
#define FC_BROWN    fc_px(RGB(116, 90, 58))
#define FC_ORANGE   fc_px(RGB(240, 132, 28))
#define FC_BLACK    fc_px(RGB(22, 22, 26))
#define FC_DECK     fc_px(RGB(62, 68, 60))
#define FC_TAPE     fc_px(RGB(96, 56, 36))

static int32_t fc_sin(int32_t deg) { return osc_sine((uint32_t)(deg * 11930465)); }   /* Q15; 2^32 / 360 */
static int32_t fc_cos(int32_t deg) { return osc_sine((uint32_t)(deg * 11930465) + 0x40000000u); }

/* does the open page edit parameter id? (its knobs' controls get the ring) */
static int fc_on(const page_t *pg, uint32_t id)
{
    uint32_t k;
    for (k = 0; k < 4u; k++)
        if (pg->id[k] == id)
            return 1;
    return 0;
}

static void fc_disc(int32_t cx, int32_t cy, int32_t r, uint16_t c, uint16_t under)
{
    cv_rrect(cx - r, cy - r, 2 * r, 2 * r, -r, c, under);
}

static uint32_t fc_isqrt(uint32_t v)
{
    uint32_t r = 0, b = 1u << 30;
    while (b > v)
        b >>= 2;
    for (; b; b >>= 2) {
        if (v >= r + b) {
            v -= r + b;
            r = (r >> 1) + b;
        } else {
            r >>= 1;
        }
    }
    return r;
}
/* a ring round (cx, cy) from r_in to r_out px, anti-aliased over whatever is on the canvas (a disc's corner pixels belong to
 * its `under`, which a ring over other drawing cannot name) */
static void fc_ring(int32_t cx, int32_t cy, int32_t r_in, int32_t r_out, uint16_t col)
{
    int32_t x, y;
    for (y = cy - r_out - 1; y <= cy + r_out; y++)
        for (x = cx - r_out - 1; x <= cx + r_out; x++) {
            int32_t dx = 2 * (x - cx) + 1, dy = 2 * (y - cy) + 1, u = (dx * dx + dy * dy) * 64, d = 0, co, ci, cov, py = y + cv_oy;
            d = (int32_t)fc_isqrt((uint32_t)u);           /* the pixel's distance in 1/16 px */
            co = r_out * 16 - d + 8;
            ci = d - r_in * 16 + 8;
            cov = co < ci ? co : ci;
            if (cov <= 0 || (uint32_t)x >= cv_w || (uint32_t)py >= cv_h)
                continue;
            cv_px[(uint32_t)py * cv_w + (uint32_t)x] =
                swap16(ux_mix(swap16(cv_px[(uint32_t)py * cv_w + (uint32_t)x]), col, cov >= 16 ? 100 : cov * 100 / 16));
        }
}

/* a label's ink centred over x0 .. x0 + w in the band y .. y + h */
static void fc_label(int32_t x0, int32_t y, int32_t w, int32_t h, const char *s, uint16_t fg, uint16_t bg)
{
    cv_text_in(x0, y + CAP_IN(S, h), w, &AF_S, s, fg, bg);
}

/* a rotary knob at pos 0 .. 127 (-135 .. +135 degrees, 12 o'clock at the middle): body, an optional cap, the pointer */
static void fc_knob(int32_t cx, int32_t cy, int32_t r, int32_t pos, uint16_t body, uint16_t cap, uint16_t mark,
                    uint16_t under, int act)
{
    int32_t deg = -135 + clamp(pos, 0, 127) * 270 / 127, sx = fc_sin(deg), cx2 = fc_cos(deg);
    if (act)
        fc_ring(cx, cy, r + 2, r + 4, T_ACCENT);
    fc_disc(cx, cy, r, body, under);
    if (cap)
        fc_disc(cx, cy, r - 4, cap, body);
    cv_line_t(cx + (sx * (r / 3) >> 15), cy - (cx2 * (r / 3) >> 15), cx + (sx * (r - 2) >> 15), cy - (cx2 * (r - 2) >> 15),
              mark, 2);
}

/* a vertical slider (Percolator): the slot from y0 to y0 + h, the cap at pos 0 .. 127 (127 on top) */
static void fc_slider(int32_t x, int32_t y0, int32_t h, int32_t pos, uint16_t under, int act)
{
    int32_t y = y0 + h - 5 - clamp(pos, 0, 127) * (h - 10) / 127;
    cv_rrect(x - 2, y0, 4, h, 2, FC_INK, under);
    if (act)
        cv_rrect(x - 9, y - 7, 18, 14, 3, T_ACCENT, under);
    cv_rrect(x - 7, y - 5, 14, 10, 2, FC_BLACK, act ? T_ACCENT : under);
    cv_rect(x - 5, y, 10, 1, FC_CHROME);
}

/* a 1176's gain-reduction meter: the cream face, the scale's ticks, the needle at `gr` 0 .. 127 (0: at rest, on the right; fx.c ins_meter) */
static void fc_meter(int32_t x, int32_t y, int32_t w, int32_t h, int32_t gr)
{
    int32_t px = x + w / 2, py = y + h - 4, r = h - 10, i, deg;
    cv_rrect(x - 2, y - 2, w + 4, h + 4, 4, FC_INK, FC_SILVER);
    cv_rrect(x, y, w, h, 3, FC_CREAM, FC_INK);
    for (i = 0; i <= 8; i++) {                           /* the scale over -50 .. +50 degrees */
        int32_t d = -50 + i * 100 / 8, s = fc_sin(d), c2 = fc_cos(d), r1 = i >= 6 ? r : r - 4;
        cv_line(px + (s * (r - 8) >> 15), py - (c2 * (r - 8) >> 15), px + (s * r1 >> 15), py - (c2 * r1 >> 15),
                i >= 6 ? FC_RED : FC_INK);
    }
    deg = 50 - clamp(gr, 0, 127) * 100 / 127;            /* no reduction: the right end */
    cv_line(px, py, px + (fc_sin(deg) * (r - 2) >> 15), py - (fc_cos(deg) * (r - 2) >> 15), FC_RED);
    fc_disc(px, py + 1, 3, FC_INK, FC_CREAM);
    fc_label(x, y + 2, w, 10, "GR", FC_INK, FC_CREAM);
}

/* COMP: INPUT A, OUTPUT B, ATTACK C, RELEASE D, RATIO E (4 8 12 20 ALL) */
static void face_comp(const track_t *t, const page_t *pg)
{
    static const char *const R[5] = {"4", "8", "12", "20", "ALL"};
    static const char *const K[4] = {"IN", "OUT", "ATK", "REL"};
    static const uint8_t ID[4] = {P_IA, P_IB, P_IC, P_ID};
    int32_t i, ratio = clamp(t->p[P_IE], 0, 4);
    cv_rrect(FC_X, FC_Y, FC_W, FC_H, 4, FC_SILVER, T_SURF);
    for (i = 0; i < 4; i++) {                             /* the knobs, two by two */
        int32_t cx = FC_X + 28 + (i & 1) * 36, cy = FC_Y + 27 + (i >> 1) * 42;
        fc_label(cx - 16, cy - 25, 32, 11, K[i], FC_INK, FC_SILVER);
        fc_knob(cx, cy, 10, t->p[ID[i]], FC_KNOB, 0, FC_CHROME, FC_SILVER, fc_on(pg, ID[i]));
    }
    fc_meter(FC_X + 92, FC_Y + 10, 72, 52, ins_meter(t));
    if (fc_on(pg, P_IE))
        cv_rrect(FC_X + 169, FC_Y + 2, 36, FC_H - 4, 4, T_ACCENT, FC_SILVER);
    for (i = 0; i < 5; i++) {                             /* the ratio buttons, one in */
        int32_t y = FC_Y + 5 + i * 16;
        cv_rrect(FC_X + 172, y, 30, 13, 3, i == ratio ? FC_RED : FC_KNOB, fc_on(pg, P_IE) ? T_ACCENT : FC_SILVER);
        fc_label(FC_X + 172, y, 30, 13, R[i], i == ratio ? FC_CHROME : FC_SILVER_D, i == ratio ? FC_RED : FC_KNOB);
    }
    cv_rect(FC_X + 4, FC_Y + 4, 3, 3, FC_SILVER_D);       /* the rack screws */
    cv_rect(FC_X + 4, FC_Y + FC_H - 7, 3, 3, FC_SILVER_D);
}

/* PRE: LEVEL A, GAIN B, BASS C, TREBLE D (bass and treble: 0 at noon), HI / LO E */
static void face_pre(const track_t *t, const page_t *pg)
{
    static const char *const K[4] = {"LEVEL", "GAIN", "BASS", "TREBLE"};
    static const uint8_t ID[4] = {P_IA, P_IB, P_IC, P_ID};
    int32_t i, hi = t->p[P_IE] > 0, act = fc_on(pg, P_IE);
    cv_rrect(FC_X, FC_Y, FC_W, FC_H, 4, FC_BLACK, T_SURF);
    cv_rrect(FC_X + 6, FC_Y + 4, FC_W - 12, FC_H - 8, 6, FC_GOLD, FC_BLACK);     /* the gold frame line */
    cv_rrect(FC_X + 7, FC_Y + 5, FC_W - 14, FC_H - 10, 6, FC_BLACK, FC_GOLD);
    for (i = 0; i < 4; i++) {
        int32_t cx = FC_X + (i & 1 ? 158 : 62), cy = FC_Y + (i >> 1 ? 55 : 15), pos = t->p[ID[i]];
        if (i >= 2)
            pos += 64;                                    /* BASS TREBLE: -64 .. 63 */
        fc_knob(cx, cy, 11, pos, FC_KNOB, FC_CHROME, FC_BLACK, FC_BLACK, fc_on(pg, ID[i]));
        fc_label(cx - 28, cy + 15, 56, 11, K[i], FC_GOLD, FC_BLACK);
    }
    fc_label(FC_X + 94, FC_Y + 12, 32, 12, "HI", FC_GOLD, FC_BLACK);          /* the switch between them */
    cv_rrect(FC_X + 103, FC_Y + 26, 14, 24, 4, act ? T_ACCENT : FC_SILVER_D, FC_BLACK);
    cv_rrect(FC_X + 105, FC_Y + 28, 10, 20, 3, FC_INK, act ? T_ACCENT : FC_SILVER_D);
    cv_rrect(FC_X + 106, hi ? FC_Y + 29 : FC_Y + 38, 8, 9, 2, FC_CHROME, FC_INK);
    fc_label(FC_X + 94, FC_Y + 52, 32, 12, "LO", FC_GOLD, FC_BLACK);
    fc_disc(FC_X + 110, FC_Y + 76, 6, FC_CHROME, FC_BLACK);                    /* the footswitch */
    fc_disc(FC_X + 110, FC_Y + 76, 3, FC_SILVER_D, FC_CHROME);
}

/* HARM: HARMONICS A, BALANCE B (sliders), MODE C (GE, OFF, SI: the clipping diodes) */
static void face_harm(const track_t *t, const page_t *pg)
{
    static const char *const M[3] = {"GE", "OFF", "SI"};
    int32_t i, mode = clamp(t->p[P_IC], 0, 2), act = fc_on(pg, P_IC);
    cv_rrect(FC_X, FC_Y, FC_W, FC_H, 4, FC_TAN, T_SURF);
    cv_rect(FC_X, FC_Y + 4, 5, FC_H - 8, FC_BROWN);                              /* the folded edges */
    cv_rect(FC_X + FC_W - 5, FC_Y + 4, 5, FC_H - 8, FC_BROWN);
    fc_label(FC_X + 12, FC_Y + 3, 80, 12, "HARMONICS", FC_INK, FC_TAN);
    fc_label(FC_X + 128, FC_Y + 3, 80, 12, "BALANCE", FC_INK, FC_TAN);
    fc_slider(FC_X + 52, FC_Y + 18, 62, t->p[P_IA], FC_TAN, fc_on(pg, P_IA));
    fc_slider(FC_X + 168, FC_Y + 18, 62, t->p[P_IB], FC_TAN, fc_on(pg, P_IB));
    if (act)
        cv_rrect(FC_X + 71, FC_Y + 14, 78, 20, 4, T_ACCENT, FC_TAN);
    for (i = 0; i < 3; i++) {                                                    /* MODE: the three diode sets */
        int32_t x = FC_X + 74 + i * 25;
        cv_rrect(x, FC_Y + 17, 24, 14, 3, i == mode ? FC_INK : FC_TAN, act ? T_ACCENT : FC_TAN);
        cv_frame(x, FC_Y + 17, 24, 14, FC_INK);
        fc_label(x, FC_Y + 17, 24, 14, M[i], i == mode ? FC_TAN : FC_INK, i == mode ? FC_INK : FC_TAN);
    }
    fc_disc(FC_X + 110, FC_Y + 62, 10, FC_BROWN, FC_TAN);                        /* the footswitch */
    fc_disc(FC_X + 110, FC_Y + 62, 7, FC_CHROME, FC_BROWN);
    fc_disc(FC_X + 110, FC_Y + 62, 3, FC_SILVER_D, FC_CHROME);
    fc_disc(FC_X + 92, FC_Y + 44, 2, FC_BROWN, FC_TAN);                          /* the two jewels */
    fc_disc(FC_X + 92, FC_Y + 52, 2, FC_BROWN, FC_TAN);
}

/* METAL: LEVEL A, LOW B and HIGH C (the COLOR MIX pair: 0 at noon), DIST D */
static void face_metal(const track_t *t, const page_t *pg)
{
    static const char *const K[4] = {"LEVEL", "L", "H", "DIST."};
    static const uint8_t ID[4] = {P_IA, P_IB, P_IC, P_ID};
    static const uint8_t X[4] = {30, 84, 136, 190};
    int32_t i;
    cv_rrect(FC_X, FC_Y, FC_W, FC_H, 4, FC_BLACK, T_SURF);
    fc_disc(FC_X + 110, FC_Y + 9, 3, FC_RED, FC_BLACK);                           /* the CHECK lamp */
    for (i = 0; i < 4; i++) {
        int32_t pos = t->p[ID[i]] + (i == 1 || i == 2 ? 64 : 0);
        fc_knob(FC_X + X[i], FC_Y + 36, 13, pos, FC_ORANGE, 0, FC_BLACK, FC_BLACK, fc_on(pg, ID[i]));
        fc_label(FC_X + X[i] - 20, FC_Y + 54, 40, 12, K[i], FC_ORANGE, FC_BLACK);
    }
    cv_rect(FC_X + 84, FC_Y + 70, 52, 1, FC_ORANGE);                              /* the COLOR MIX bracket */
    cv_rect(FC_X + 84, FC_Y + 67, 1, 4, FC_ORANGE);
    cv_rect(FC_X + 135, FC_Y + 67, 1, 4, FC_ORANGE);
    fc_label(FC_X + 70, FC_Y + 72, 80, 12, "COLOR MIX", FC_ORANGE, FC_BLACK);
}

/* ROT: WOW A, DROP B, LOSS C, HISS D, DRIVE E (a deck's front: the reels over the head block, five knobs under) */
static void face_rot(const track_t *t, const page_t *pg)
{
    static const char *const K[5] = {"WOW", "DROP", "LOSS", "HISS", "DRIVE"};
    static const uint8_t ID[5] = {P_IA, P_IB, P_IC, P_ID, P_IE};
    int32_t i, r, spin = song.playing ? (int32_t)((ui.frame * 3u) % 120u) : 0;   /* (the reels turn while the song plays) */
    cv_rrect(FC_X, FC_Y, FC_W, FC_H, 4, FC_DECK, T_SURF);
    for (r = 0; r < 2; r++) {                              /* the reels: a disc of tape, a silver flange, three spokes */
        int32_t cx = FC_X + (r ? 156 : 64), cy = FC_Y + 27;
        fc_disc(cx, cy, 21, FC_TAPE, FC_DECK);
        fc_disc(cx, cy, 17, FC_CHROME, FC_TAPE);
        fc_disc(cx, cy, 13, FC_DECK, FC_CHROME);
        for (i = 0; i < 3; i++) {
            int32_t d = spin + i * 120;
            cv_line_t(cx, cy, cx + (fc_sin(d) * 13 >> 15), cy - (fc_cos(d) * 13 >> 15), FC_CHROME, 2);
        }
        fc_disc(cx, cy, 4, FC_CHROME, FC_DECK);
    }
    cv_line(FC_X + 64, FC_Y + 48, FC_X + 100, FC_Y + 44, FC_TAPE);                 /* the tape's path past the head block */
    cv_line(FC_X + 100, FC_Y + 44, FC_X + 120, FC_Y + 44, FC_TAPE);
    cv_line(FC_X + 120, FC_Y + 44, FC_X + 156, FC_Y + 48, FC_TAPE);
    cv_rrect(FC_X + 100, FC_Y + 38, 20, 6, 2, FC_SILVER_D, FC_DECK);
    for (i = 0; i < 5; i++) {
        int32_t cx = FC_X + 22 + i * 44;
        fc_knob(cx, FC_Y + 64, 8, t->p[ID[i]], FC_CREAM, 0, FC_INK, FC_DECK, fc_on(pg, ID[i]));
        fc_label(cx - 21, FC_Y + 74, 42, 11, K[i], FC_CREAM, FC_DECK);
    }
}

static void graph_insert_face(const track_t *t, uint16_t c)
{
    const page_t *pg = cur_page();
    switch (clamp(t->p[P_ITYPE], 0, IT_N - 1)) {
    case IT_COMP:
        face_comp(t, pg);
        break;
    case IT_PRE:
        face_pre(t, pg);
        break;
    case IT_HARM:
        face_harm(t, pg);
        break;
    case IT_METAL:
        face_metal(t, pg);
        break;
    case IT_ROT:
        face_rot(t, pg);
        break;
    default:                                              /* OFF: the dry line, dim */
        cv_line_t(PANEL_X0 + PANEL_W / 6, FC_Y + FC_H - 12, PANEL_X0 + PANEL_W * 5 / 6, FC_Y + 12, T_RAISE, 2);
        break;
    }
    (void)c;
}
