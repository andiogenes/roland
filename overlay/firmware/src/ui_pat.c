/* SPDX-License-Identifier: GPL-3.0-only */
/* PATTERN (drum build): ARP's button. The drum track has eight patterns of 32 steps: the halves of the project slots
 * A..D (pattern p = slot p / 2, its first or second two pages of the 64 steps; SAVE a slot to make its two patterns
 * available). ARP taps between sequencing and the PATTERN page; there the eight white keys F G A B C D E F are patterns
 * 1..8 and OCT- / OCT+ step through them (both: the chain clears).
 *
 *   stopped   a key appends its pattern to the chain (up to 16); PLAY plays the chain, again and again (an empty chain:
 *             the pattern last picked)
 *   playing   a key queues its pattern: it plays once, when the pattern now playing ends, and the chain goes on after
 *             it (a chain of one: the queued pattern replaces it). Up to 8 queued.
 *
 * The chain engine is the song's (song_chain.c: lane ids CHAIN_PAT0 + p, chain.loop, chain.q); the sources are the saved
 * slots, copied before PLAY (project.c chain_prepare_pat). */
static struct {
    uint8_t n, id[CHAIN_ROWS];    /* the chain: pattern numbers 0..7 */
    uint8_t cur;                  /* the pattern last picked */
    uint8_t back, back_home;      /* the page ARP leaves */
} dpat;

static int dp_page_on(void) { return !ui.home && cur_page()->graph == GR_PAT; }
static int dp_keys_on(void) { return !ui.menu && !ui.confirm && !ui.layer && dp_page_on(); }
static int dp_ok(uint32_t p) { return project_used(p >> 1); }
static uint32_t dp_queued(void) { return (uint8_t)(chain.qt - chain.qh); }
/* the pattern now playing (the chain running), else the last picked */
static uint32_t dp_now(void)
{
    uint32_t s = chain.lane[0];
    return chain.running && s >= CHAIN_PAT0 && s < CHAIN_PAT0 + CHAIN_PATN ? s - CHAIN_PAT0 : dpat.cur;
}
static void dp_pick(uint32_t p)
{
    if (!dp_ok(p)) {
        char b[16] = "A NOT SAVED";
        b[0] = (char)('A' + (p >> 1));
        ui_say("PATTERN ", b);
        return;
    }
    if (chain_busy()) {
        if (dp_queued() >= 8u) {
            ui_message("QUEUE FULL");
            return;
        }
        chain.q[chain.qt & 7u] = (uint8_t)(CHAIN_PAT0 + p);
        __asm__ volatile("" ::: "memory");
        chain.qt++;
    } else {
        if (dpat.n >= CHAIN_ROWS) {
            ui_message("CHAIN FULL");
            return;
        }
        dpat.id[dpat.n++] = (uint8_t)p;
    }
    dpat.cur = (uint8_t)p;
    ui.force = 1;
}
static void dp_keys(uint32_t notes)
{
    uint32_t k;
    for (k = 0; k < 13u; k++)
        if ((notes >> k) & 1u && dp_key(k) >= 0)
            dp_pick((uint32_t)dp_key(k));
}
/* the key LEDs: the pattern now playing (or last picked), and the ones queued */
static uint32_t dp_leds(void)
{
    static const uint8_t K[8] = {0, 2, 4, 6, 7, 9, 11, 12};
    uint32_t m = 1u << K[dp_now() & 7u], i;
    for (i = chain.qh; i != chain.qt && chain_busy(); i++)
        m |= 1u << K[(chain.q[i & 7u] - CHAIN_PAT0) & 7u];
    return m;
}
/* OCT- / OCT+: the previous / next saved pattern after the last in the chain (playing: after the last queued) */
static void dp_oct(int32_t dir, int both)
{
    uint32_t p, i;
    if (both) {                                   /* the chain clears (playing: the queue) */
        fm1_irq_off();
        chain.qh = chain.qt;
        fm1_irq_on();
        dpat.n = 0;
        ui_message("CHAIN CLEARED");
        ui.force = 1;
        return;
    }
    p = chain_busy() && dp_queued() ? (uint32_t)(chain.q[(uint8_t)(chain.qt - 1u) & 7u] - CHAIN_PAT0) & 7u :
        chain_busy() ? dp_now() : dpat.n ? dpat.id[dpat.n - 1u] : dpat.cur;
    for (i = 0; i < 8u; i++) {
        p = (p + (uint32_t)(dir > 0 ? 1 : 7)) & 7u;
        if (dp_ok(p)) {
            dp_pick(p);
            return;
        }
    }
    dp_pick(p);                                  /* (none saved: the message) */
}
static void dp_play(void)
{
    uint32_t rc = chain_prepare_pat(dpat.id, dpat.n, dpat.cur);
    if (!rc) {
        ui.force = 1;
    } else if (rc >= 3u) {
        char b[8] = "A EMPTY";
        b[0] = (char)('A' + rc - 3u);
        ui_say("PATTERN ", b);
    } else {
        ui_message(rc == 1u ? "SAVE A PROJECT" : "STOP FIRST");
    }
}
/* ARP tapped: to the PATTERN page and back to the page it left. 1: done here */
static int dp_arp_tap(void)
{
#if FELUCCA_DRUMONLY
    if (dp_page_on()) {
        ui.home = dpat.back_home;
        ui.page = dpat.back;
        if (ui.home)
            go_home();
        else
            page_entered();
        ui.force = 1;
        return 1;
    }
    if (ui.menu || ui.confirm)
        return 0;
    dpat.back = ui.page;
    dpat.back_home = ui.home;
    return 0;                                     /* (open_family: the page) */
#else
    return 0;
#endif
}
static uint32_t dp_sig(uint32_t h)
{
    uint32_t i;
    h = (h ^ dpat.n * 31u ^ dpat.cur * 977u ^ dp_queued() * 7919u ^ (chain.running ? (dp_now() + 1u) * 104729u : 0u)) * 16777619u;
    for (i = 0; i < dpat.n; i++)
        h = (h ^ dpat.id[i]) * 16777619u;
    for (i = chain.qh; i != chain.qt; i++)
        h = (h ^ chain.q[i & 7u]) * 16777619u;
    for (i = 0; i < 4u; i++)
        h ^= (uint32_t)project_used(i) << (24u + i);
    return h ^ (chain.running ? (chain.row + 1u) * 1299709u : 0u);
}
