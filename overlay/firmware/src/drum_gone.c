/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* FELUCCA_DRUMONLY: what the code that stays (projects, undo, user presets, the editor, the screens) still asks of the engines
 * that are not built. The records those parts keep (a project holds a patch for each track, an undo step the track's FM6
 * patch) keep their layout, so what is already in flash still loads; the patches in them are never used. Included by
 * engines.c in place of the engines. */

/* ---- FM6 (eng_fm6.c, fm6_core.c) ---- */
#define FP_SIZE 155               /* (fm6_core.c) */
#define ENGI_FM6 12u
#define FM6_PACKED 128u
#include "felucca_fm6.h"          /* FM6_INIT, FM6_NFACTORY (the data: a few hundred bytes, only FM6_INIT is used) */
#define FM6_OWN FM6_NFACTORY
static uint8_t fm6_patch[NTRK][FP_SIZE + 1u];
static uint8_t fm6_slot[NTRK];
static const char *const N_FM6_PATCH[] = {"-"};
static int fm6_slot_ok(int32_t s) { (void)s; return 0; }
static void fm6_poll(void) {}
static void fm6_track_loaded(const track_t *t) { (void)t; }
static void fm6_set_patch(uint32_t tr, const uint8_t *v) { (void)tr; (void)v; }
static void fm6_adopt(uint32_t tr) { (void)tr; }
static void fm6_unpack(const uint8_t *b, uint8_t *v) { (void)b; memset(v, 0, FP_SIZE + 1u); }
static void fm6_pack(const uint8_t *v, uint8_t *b) { (void)v; memcpy(b, FM6_INIT, FM6_PACKED); }
static void fm6_init(void) {}
/* the FM6 user-preset patches and voice bank (up_fm6.c, fm6_vbank.c): none */
static void upf_boot(void) {}
static void fvb_boot(void) {}
static int upf_store(uint32_t k, uint32_t tr) { (void)k; (void)tr; return 0; }
static void upf_track_load(track_t *t, uint32_t k) { (void)t; (void)k; }
static int ed_fm6_handle(uint32_t cmd, const uint8_t *a, uint32_t na) { (void)cmd; (void)a; (void)na; return 0; }

/* ---- SAMPLE (eng_sample.c): the user sample slots, none ---- */
/* the editor's SMP_* commands and the backup keep working as "no such slot", their code and ids unchanged */
#define SMP_USER_SLOTS 0u
#define SMP_USER_BASE 0xA0000u
#define SMP_USER_SIZE 0x14000u
#define SMP_USER_DATA 512u
#define SMP_USER_MAGIC 0x504D5346u
typedef struct {
    uint32_t magic;
    uint16_t version;
    uint8_t nz, rsv;
    char name[8];
    uint32_t data_len, crc, rsv2[2];
} smp_user_hdr_t;
static uint8_t usr_nz[1];
static const uint8_t *smp_user_xip(uint32_t k) { (void)k; return 0; }
static void smp_user_scan(uint32_t k) { (void)k; }

/* more of what the screens, the editor and the loads still name */
static uint8_t fm6_own_ok;
#define FM6_NBANK 0u                                   /* (the editor is told: no voice bank) */
enum { ED_FM6B_BEGIN = 74, ED_FM6B_END = 77 };         /* (the editor's FM6 bank commands: ed_fm6_handle ignores them) */
static int32_t fm6_bank_slot(int32_t s) { (void)s; return -1; }
static void phys_legacy(int16_t *e) { (void)e; }
#define SMP_PRESET_TABLE ((const preset_t *)0)         /* (no SAMPLE engine, no aliased sets) */
#define SMP_NALIAS 0u
#define SMP_NSETS 0u
static const uint8_t SMP_SET_ORIG[1];
static int fop_page(uint32_t g) { (void)g; return 0; }   /* (FM6's operator pages: never shown) */
