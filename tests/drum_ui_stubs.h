/* SPDX-License-Identifier: GPL-3.0-only */
/* Felucca's UI test harness (tests/ui_test.c, ui_render.c) draws the screens of every engine: FM6's algorithm charts and
 * operator pages, SAMPLE's waveform. A FELUCCA_DRUMONLY firmware has none of them; this is what the harness still names, so the
 * screens that remain can be drawn and linted. Test code only: nothing here is in the firmware. */
enum { FMH_BOX, FMH_ROUTE, FMH_CAR, FMH_BUS, FMH_FB, FMH_LABEL, FMH_END };
enum { FO_OP, FO_STG, FO_CRS, FO_FINE, FO_LVL, FO_MODE, FO_DTUN, FO_VEL, FO_RATE, FO_ELVL };
enum { FP_OP = 0, FP_FC = 1, FP_FF = 2, FP_OL = 3, FP_MODE = 4, FP_DET = 5, FP_KVS = 6, FP_R1 = 7, FP_L1 = 11 };
static uint8_t fop_op, fop_stg;
static volatile uint8_t fm6_pgen[NTRK];
#define SAMPLE_WAVE_COLS 96u
typedef struct { uint32_t n; } smp_zone_t;
static struct { int16_t lo[SAMPLE_WAVE_COLS], hi[SAMPLE_WAVE_COLS]; const smp_zone_t *zone; uint32_t key, gen, pos;
                int32_t pred, index, peak; uint8_t ready; } sample_wave;
static void sample_wave_tick(const track_t *t) { (void)t; }
typedef struct { const char *name; uint16_t z0, nz; } smp_set_t;
static const smp_set_t SMP_SETS[1];
#define SMP_NPRESETS 0u
static uint32_t fop_field(uint32_t slot) { (void)slot; return 0; }
static uint32_t fop_byte(uint32_t f) { (void)f; return 0; }
static uint32_t fm6_max(uint32_t b) { (void)b; return 0; }
/* the SLICER's modes and the old INSERT types, which the harness's scenes still name (the firmware has neither) */
enum { SL_OFF, SL_GATE, SL_STUT };
enum { IT_SOFT = 1, IT_HARD, IT_FOLD, IT_FUZZ, IT_CRUSH, IT_PHASER, IT_FLANGER, IT_CHORUS };
