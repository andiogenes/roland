/* SPDX-License-Identifier: GPL-3.0-only */
/* PATTERN mode (ui_pat.c, song_chain.c, project.c chain_prepare_pat) on Felucca's UI test stubs (tests/ui_test.c): the
 * eight 32-step patterns (the halves of the project slots), their chain looping, a pattern queued while it plays, the
 * keys and OCT buttons, and SONG's own tests again (the chain engine changed: rows of slots must play as before).
 * Built and run by host_render.sh next to the render. */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
#include "song_probe.h"

static uint32_t pt_until(uint32_t row, uint32_t max)    /* blocks of 32 until the section changes */
{
    uint32_t n = 0;
    while (n < max && chain.running && chain.row == row && !chain.inovr) {
        events_block(32);
        n++;
    }
    return n;
}
static uint32_t pt_blocks(uint32_t steps) { return steps * div_samples(2) / 32u; }
static int pt_near(uint32_t blocks, uint32_t steps) { return blocks + 1u >= pt_blocks(steps) && blocks <= pt_blocks(steps) + 1u; }
static void pt_page(void)
{
    uint32_t i;
    for (i = 0; i < NPAGES && !str_eq(PAGES[i].title, "PATTERN"); i++)
        ;
    ui.home = 0;
    ui.page = (uint8_t)i;
    page_entered();
}
static int test_patterns_drum(void)
{
    int bad = 0, ok;
    uint32_t n, k, i, s;
    ui_power_on();
    /* slot A: T1.. steps 0..31 note 60 (every step), steps 32..63 note 62; LEN 40 of 1/16 (second half 8).
     * slot B: steps 0..31 note 64, 32..63 note 66; LEN 32 (the second half is a whole 32, 1/16) */
    for (s = 0; s < 2u; s++) {
        for (k = 0; k < NTRK; k++) {
            track_t *t = &trk[k];
            track_defaults_steps(t);
            motion_clear(t);
            for (i = 0; i < 64u; i++) {
                t->step[i].note[0] = (uint8_t)((s ? 64u : 60u) + (i >= 32u ? 2u : 0u));
                t->step[i].n = 1;
                t->step[i].time = ST_NOTE;
                t->step[i].vel = 100;
            }
            t->p[P_SLEN] = s ? 32 : 40;
            t->p[P_SDIV] = 2;
            t->p[P_SSWING] = 0;
            t->p[P_SGATE] = 120;
            t->p[P_SQNT] = 1;
        }
        project_save(s);
    }
    for (k = 0; k < NTRK; k++) {
        track_defaults_steps(&trk[k]);
        trk[k].p[P_SLEN] = 3;
        trk[k].p[P_SDIV] = 2;
    }
    pt_page();
    bad += check("PATTERN: the page is ARP's, its keys are 4 (ui_pat.c)", dp_page_on() && dp_keys_on());
    dp_pick(0);
    dp_pick(3);
    bad += check("PATTERN stopped: keys append to the chain (1, 4)", dpat.n == 2 && dpat.id[0] == 0 && dpat.id[1] == 3);
    dp_pick(4);
    bad += check("PATTERN: a pattern whose slot is not saved (C) is refused", dpat.n == 2);
    ok = chain_prepare_pat(dpat.id, dpat.n, dpat.cur) == 0 && chain.armed;
    bad += check("PATTERN: PLAY prepares the chain (A1, B2)", ok);
    events_block(32);
    ok = chain.running && chain.loop && chain.row == 0 && chain.lane[0] == CHAIN_PAT0 + 0;
    ok &= seq_steps(&trk[0]) == chain.source[0].step[0] && trk[0].p[P_SLEN] == 32 && trk[0].seq_notes[0] == 60 && trk[3].seq_notes[0] == 60;
    bad += check("PATTERN: A1 plays steps 1..32 of A, 32 of them", ok);
    n = 1u + pt_until(0, 20000);
    ok = chain.running && chain.row == 1u && chain.lane[0] == CHAIN_PAT0 + 3u && pt_near(n, 32);
    ok &= seq_steps(&trk[0]) == chain.source[1].step[0] + 32 && trk[0].p[P_SLEN] == 32 && trk[0].seq_notes[0] == 66;
    bad += check("PATTERN: after 32 steps B2 (steps 33..64 of B)", ok);
    n = 1u + pt_until(1, 20000);
    ok = chain.running && chain.row == 0u && chain.lane[0] == CHAIN_PAT0 && pt_near(n, 32);
    bad += check("PATTERN: the chain loops (B2 -> A1)", ok);
    /* queue A2 (pattern 2, 8 steps: LEN 40) while A1 plays: it plays once, then B2 */
    dp_pick(1);
    ok = (uint8_t)(chain.qt - chain.qh) == 1u;
    n = 1u + pt_until(0, 20000);
    ok &= chain.running && chain.inovr && chain.lane[0] == CHAIN_PAT0 + 1u && trk[0].p[P_SLEN] == 8 && trk[0].seq_notes[0] == 62;
    ok &= chain.row == 0u && seq_steps(&trk[0]) == chain.source[0].step[0] + 32;
    bad += check("PATTERN playing: a key queues A2; it starts when A1 ends, 8 steps (LEN 40 - 32)", ok);
    n = 0;
    while (n < 20000u && chain.running && chain.inovr) {
        events_block(32);
        n++;
    }
    ok = chain.running && !chain.inovr && chain.row == 1u && chain.lane[0] == CHAIN_PAT0 + 3u && pt_near(n + 1u, 8);
    ok &= (uint8_t)(chain.qt - chain.qh) == 0u;
    bad += check("PATTERN: the queued one plays once, the chain goes on after A1 (B2)", ok);
    seq_stop();
    ok = !chain.running && !song.playing;
    for (k = 0; k < NTRK; k++)
        ok &= trk[k].p[P_SLEN] == 3 && trk[k].p[P_SDIV] == 2 && !trk[k].seq_n;
    bad += check("PATTERN: STOP puts LEN and DIV back", ok);
    /* a chain of one: a queued pattern replaces it */
    ok = chain_prepare_pat(dpat.id, 0, 0) == 0;
    events_block(32);
    ok &= chain.running && chain.config.count == 1u && chain.lane[0] == CHAIN_PAT0;
    dp_pick(2);                                        /* B1 */
    n = 0;
    while (n < 20000u && chain.running && chain.lane[0] == CHAIN_PAT0)
        n++, events_block(32);
    ok &= pt_near(n + 1u, 32) && chain.lane[0] == CHAIN_PAT0 + 2u && !chain.inovr && chain.config.row[0].slot[0] == CHAIN_PAT0 + 2u;
    ok &= trk[0].seq_notes[0] == 64;
    for (n = 0; n < pt_blocks(70); n++)
        events_block(32);
    ok &= chain.running && chain.lane[0] == CHAIN_PAT0 + 2u;
    seq_stop();
    bad += check("PATTERN: a chain of one (A1): a queued key (B1) replaces it, and it loops", ok);
    ok = 1;
    dpat.n = 1;
    dpat.id[0] = 4;
    ok &= chain_prepare_pat(dpat.id, 1, 0) == 3u + 2u && !chain.armed;
    memset(proj_slot, 0, sizeof proj_slot);
    ok &= chain_prepare_pat(dpat.id, 0, 0) == 1u;
    bad += check("PATTERN: a slot not saved refuses (PATTERN C EMPTY); none saved: SAVE A PROJECT", ok);
    /* the buttons: OCT+ the next saved pattern after the last in the chain, both: the chain clears */
    project_save(0);
    project_save(1);
    dpat.n = 0;
    dpat.cur = 3;
    dp_oct(1, 0);
    ok = dpat.n == 1u && dpat.id[0] == 0u;
    dp_oct(-1, 0);
    ok &= dpat.n == 2u && dpat.id[1] == 3u;
    dp_oct(1, 1);
    ok &= dpat.n == 0u;
    bad += check("PATTERN: OCT+ steps to the next saved pattern (wrapping), OCT- back, both clear the chain", ok);
    for (i = 0; i < 17u; i++)
        dp_pick(i & 3u);
    bad += check("PATTERN: the chain holds 16", dpat.n == CHAIN_ROWS);
    dpat.n = 0;
    return bad;
}

int main(void)
{
    int bad = 0;
    setvbuf(stdout, NULL, _IONBF, 0);
    bad += test_patterns_drum();
    bad += test_chain();
    {   /* (the hash itself is checked on the stock configuration, tests/song_probe_check.c: this build's REV differs) */
        uint32_t n = 0;
        (void)song_probe(&n);
        bad += check("SONG of rows (one slot each): as long as before, block by block (song_probe.h)", n == SONG_PROBE_BLOCKS);
    }
    printf("pattern_test: %s\n", bad ? "FAIL" : "ok");
    return bad != 0;
}
