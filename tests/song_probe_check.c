/* SPDX-License-Identifier: GPL-3.0-only */
/* SONG's chain engine (song_chain.c) after the drum patterns were added: a song of rows (one slot each) must play bit for
 * bit as the firmware before lanes did (tests/song_probe.h, SONG_PROBE_GOLDEN). Built by host_render.sh WITHOUT
 * FELUCCA_DRUMONLY: the drum build has its own REV (a hash over it would differ) */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
#include "song_probe.h"
int main(void)
{
    uint32_t n = 0, h = song_probe(&n);
    int ok = h == SONG_PROBE_GOLDEN && n == SONG_PROBE_BLOCKS;
    printf("song_probe_check: %08x, %u blocks: %s\n", (unsigned)h, (unsigned)n, ok ? "ok" : "FAIL");
    return !ok;
}
