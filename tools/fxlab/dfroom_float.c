/* SPDX-License-Identifier: GPL-3.0-only */
/* A float transcription of the Dragonfly Room's signal path (Freeverb3's earlyref and progenitor2 as Dragonfly's DSP.cpp drives them,
 * the Medium Clear Room preset), written from reading that code (Dragonfly Reverb: GPL-3.0, (c) Michael Willis and Rob van den Berg;
 * Freeverb3: GPL-2+, (c) Teru Kamogashira; the Progenitor is David Griesinger's), as the stepping stone to the firmware's fixed-point
 * port: build with `cc -O2 -DDFR_MAIN dfroom_float.c -lm`, `dfroom_float FS OUT.raw` writes the same four-float-a-sample impulse
 * response as tests/dfroom_ref.cpp (early L R, late L R). The noise source that modulates the allpasses is a cheap stand-in for
 * Freeverb3's fractal pink noise, so the responses match statistically (decay curve, echo density, spectrum), not sample by sample. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { float *b; long n, i; } dly_t;
typedef struct { float *b; long n, i; float fb, dec; } ap_t;
typedef struct { float *b; long n, rd, wr, modn; float fb, dec, z1; } apm_t;
typedef struct { ap_t a1, a2; } ap2_t;      /* two-section allpass (allpass2): fb / decay per section */
typedef struct { float *b1, *b2, *b3; long n1, n2, n3, rd1, wr1, i2, i3, modn; float fb1, fb2, fb3, d1, d2, d3; } ap3_t;
typedef struct { float b1, b2, a2, y1; } iir1_t;
typedef struct { float b0, b1, b2, a1, a2, i1, i2, o1, o2; } bq_t;
typedef struct { float re, im, ar, ai; long c; } lfo_t;

static float *pool;
static size_t pool_n, pool_used;
static float *take(long n)
{
    float *p = pool + pool_used;
    pool_used += (size_t)n;
    if (pool_used > pool_n) { fprintf(stderr, "pool\n"); exit(1); }
    memset(p, 0, (size_t)n * sizeof(float));
    return p;
}
static int is_prime(long x) { long d; if (x < 2) return 0; for (d = 2; d * d <= x; d++) if (x % d == 0) return 0; return 1; }
static long pf(double def, double fac) { long v = (long)(def * fac); return v <= 0 ? 1 : v; }
static long pp(double def, double fac) { long v = pf(def, fac); while (!is_prime(v)) v++; return v; }

static void dly_init(dly_t *d, long n) { d->n = n; d->i = 0; d->b = take(n); }
static float dly_proc(dly_t *d, float x) { float o = d->b[d->i]; d->b[d->i] = x; if (++d->i >= d->n) d->i = 0; return o; }
static float dly_last(dly_t *d) { return d->b[d->i]; }
static float dly_z(dly_t *d, long k) { long r; if (k > d->n) k = d->n; if (k <= 0) k = 1; r = d->i - k; if (r < 0) r += d->n; return d->b[r]; }
static void ap_init(ap_t *a, long n) { a->n = n; a->i = 0; a->b = take(n); }
static float ap_proc(ap_t *a, float x)
{
    float t = a->b[a->i], o;
    x += a->fb * t;
    o = t - a->fb * x;
    a->b[a->i] = x;
    if (++a->i >= a->n) a->i = 0;
    return o;
}
static float ap_proc_dc(ap_t *a, float x)
{
    float t = a->b[a->i], o;
    x += a->fb * t;
    o = a->dec * t - a->fb * x;
    a->b[a->i] = x;
    if (++a->i >= a->n) a->i = 0;
    return o;
}
static float ap_z(ap_t *a, long k) { long r = a->i - k; if (r < 0) r += a->n; return a->b[r]; }
static void apm_init(apm_t *a, long n, long modn) { if (modn > n) modn = n; a->n = n + modn; a->modn = modn; a->b = take(a->n); a->rd = modn * 2; a->wr = 0; a->z1 = 0; }
/* modulated allpass with the decay (process_dc): the read point wanders by +-modn around the write point's delay */
static float apm_proc_dc(apm_t *a, float x, float mod, float fmod)
{
    float m = (mod + 1.0f) * (float)a->modn, fl = floorf(m), frac = 1.0f - (m - fl), fbm = a->fb + fmod, w;
    long ra = a->rd - (long)fl, rb;
    if (ra < 0) ra += a->n;
    rb = ra - 1;
    if (rb < 0) rb += a->n;
    a->z1 = a->b[rb] + frac * (a->b[ra] - a->z1);
    if (++a->rd >= a->n) a->rd = 0;
    w = x + a->z1 * fbm;
    a->b[a->wr] = w;
    x = a->dec * a->z1 - w * fbm;
    if (++a->wr >= a->n) a->wr = 0;
    return x;
}
static void ap2_init(ap2_t *a, long n1, long n2) { ap_init(&a->a1, n1); ap_init(&a->a2, n2); }
static float ap2_proc(ap2_t *a, float x)
{
    float t2 = a->a2.b[a->a2.i], t1 = a->a1.b[a->a1.i], o;
    x += a->a2.fb * t2;
    o = a->a2.dec * t2 - x * a->a2.fb;
    x += a->a1.fb * t1;
    a->a2.b[a->a2.i] = a->a1.dec * t1 - x * a->a1.fb;
    a->a1.b[a->a1.i] = x;
    if (++a->a1.i >= a->a1.n) a->a1.i = 0;
    if (++a->a2.i >= a->a2.n) a->a2.i = 0;
    return o;
}
static void ap3_init(ap3_t *a, long n1, long m1, long n2, long n3)
{
    if (m1 > n1) m1 = n1;
    a->n1 = n1 + m1; a->modn = m1; a->n2 = n2; a->n3 = n3;
    a->b1 = take(a->n1); a->b2 = take(n2); a->b3 = take(n3);
    a->rd1 = m1 * 2; a->wr1 = 0; a->i2 = a->i3 = 0;
}
static float ap3_proc(ap3_t *a, float x, float mod)
{
    float m = (mod + 1.0f) * (float)a->modn, fl = floorf(m), frac = m - fl, o, t1;
    long ra = a->rd1 - (long)fl, rb;
    if (ra < 0) ra += a->n1;
    rb = ra - 1;
    if (rb < 0) rb += a->n1;
    x += a->fb3 * a->b3[a->i3];
    o = a->d3 * a->b3[a->i3] - a->fb3 * x;
    x += a->fb2 * a->b2[a->i2];
    a->b3[a->i3] = a->d2 * a->b2[a->i2] - a->fb2 * x;
    t1 = a->b1[rb] * frac + a->b1[ra] * (1 - frac);
    x += a->fb1 * t1;
    a->b2[a->i2] = a->d1 * t1 - a->fb1 * x;
    a->b1[a->wr1] = x;
    if (++a->wr1 >= a->n1) a->wr1 = 0;
    if (++a->rd1 >= a->n1) a->rd1 = 0;
    if (++a->i2 >= a->n2) a->i2 = 0;
    if (++a->i3 >= a->n3) a->i3 = 0;
    return o;
}
static float ap3_z1(ap3_t *a, long k) { long r = a->rd1 - k; if (r < 0) r += a->n1; return a->b1[r]; }
static float ap3_z2(ap3_t *a, long k) { long r = a->i2 - k; if (r < 0) r += a->n2; return a->b2[r]; }
static float ap3_z3(ap3_t *a, long k) { long r = a->i3 - k; if (r < 0) r += a->n3; return a->b3[r]; }
static float ap2_z1(ap2_t *a, long k) { return ap_z(&a->a1, k); }
static float ap2_z2(ap2_t *a, long k) { return ap_z(&a->a2, k); }

static float iir1(iir1_t *f, float x) { float o = x * f->b1 + f->y1; f->y1 = o * f->a2 + x * f->b2; return o; }
static void iir1_lpf(iir1_t *f, double fc, double fs) { double t = tan(M_PI * fc / fs); f->b1 = f->b2 = (float)(t / (1 + t)); f->a2 = (float)((1 - t) / (1 + t)); }
static float bq(bq_t *f, float x) { float o = x * f->b0 + f->b1 * f->i1 + f->b2 * f->i2 - f->a1 * f->o1 - f->a2 * f->o2; f->i2 = f->i1; f->i1 = x; f->o2 = f->o1; f->o1 = o; return o; }
static double alpha_bw(double fc, double bw, double fs) { double w = 2 * M_PI * fc / fs, sn = sin(w); return sn * sinh(M_LN2 / 2.0 * bw * w / sn); }
static void bq_lpf(bq_t *f, double fc, double bw, double fs)
{
    double w = 2 * M_PI * fc / fs, cs = cos(w), al = alpha_bw(fc, bw, fs), a0 = 1.0 / (1.0 + al);
    f->b0 = (float)(a0 * (1 - cs) * 0.5); f->b1 = (float)(a0 * (1 - cs)); f->b2 = f->b0; f->a1 = (float)(a0 * -2 * cs); f->a2 = (float)(a0 * (1 - al));
}
static void bq_apf(bq_t *f, double fc, double bw, double fs)
{
    double w = 2 * M_PI * fc / fs, cs = cos(w), al = alpha_bw(fc, bw, fs), a0 = 1.0 / (1.0 + al);
    f->b0 = (float)(a0 * (1 - al)); f->b1 = (float)(a0 * -2 * cs); f->b2 = (float)(a0 * (1 + al)); f->a1 = f->b1; f->a2 = f->b0;
}
static float lfo_proc(lfo_t *l)
{
    float out = l->im, nr = l->re * l->ar - l->im * l->ai, ni = l->re * l->ai + l->im * l->ar;
    l->re = nr; l->im = ni;
    if (l->c++ > 1000) { float r = sqrtf(nr * nr + ni * ni); l->c = 0; l->re /= r; l->im /= r; }
    return out > 1 ? 1 : out < -1 ? -1 : out;
}
static void lfo_set(lfo_t *l, double f, double fs) { double th = 2 * M_PI * f / fs; l->ar = (float)cos(th); l->ai = (float)sin(th); l->re = 1; l->im = 0; l->c = 0; }

/* the tables (34.125 kHz) */
static const long IAL[10] = {617, 535, 434, 347, 218, 162, 144, 122, 109, 74}, IAR[10] = {603, 547, 416, 364, 236, 162, 140, 131, 111, 79};
static const long CAL[4] = {430, 341, 264, 174}, CAR[4] = {447, 324, 247, 191};
static const long ALC[8] = {239, 392, 1944, 612, 1212, 121, 816, 1264}, ARC[8] = {205, 329, 2032, 368, 1452, 5, 688, 1340};
static const long DLC[6] = {1, 2, 1055, 344, 1572, 0}, DRC[6] = {1, 625, 835, 500, 16, 1460};
static const long OUTC[11] = {276, 468, 625, 312, 8, 24, 36, 40, 1, 192, 1572};
static const long OUTC2[20] = {110, 790, 2, 189, 121, 10, 480, 359, 103, 30, 26, 10, 780, 109, 1200, 1310, 780, 10, 310, 800};
#define EXC 32

typedef struct {
    double fs, rs;
    /* the early reflections */
    dly_t lineL, lineR, ltr, rtl, wl, wr;
    long nt, tapL[6], tapR[6];
    float gL[6], gR[6], ewet1, ewet2;
    bq_t axL, axR, a2L, a2R;
    iir1_t eoL, eoR;
    /* the late reverb */
    float idiff, odiff, crossfeed, loopdecay, bassb, wander, wander2, wet1, wet2;
    iir1_t dccL_unused, lpfLin, lpfRin, lpfLd, lpfRd;
    float dcy[2][2];
    bq_t bassAPL, bassAPR, lp9, lp7, o1, o2;
    apm_t iaL[10], iaR[10];
    ap_t caL[4], caR[4];
    apm_t m15, m17, m19, m21;
    ap2_t a2L25, a2R43;
    ap3_t a3L34, a3R52;
    dly_t d16, d23, d31, d37, dts, d40, d49, d58;
    long iOut[11], iOut2[20];
    lfo_t lfo1, lfo2;
    iir1_t lfo1l, lfo2l;
    dly_t outL, outR;           /* the output feed-forward combs (modulated gain) */
    long outn;
    dly_t predL, predR;
    float noise, dcL[3], dcR[3];
    unsigned rng;
} room_t;
static room_t R;

#ifdef DFR_NONOISE
static float noise_step(room_t *r) { (void)r; return 0; }
#else
static float noise_step(room_t *r) { r->rng = r->rng * 1664525u + 1013904223u; r->noise += 0.02f * ((float)(r->rng >> 8) / 8388608.0f - 1.0f - r->noise * 0.5f); return r->noise * 3.0f; }
#endif
static float dccut(float *s, float gain, float x) { float o = x - s[0]; s[0] = x; o += gain * s[1]; s[1] = o; return o; }
static float dcgain;
#ifdef DFR_DBG
float dfr_dbg[8];
#endif

/* the Medium Clear Room: size 12 m, width 100, predelay 8 ms, decay 0.4 s, diffuse 70, spin 0.8, wander 40, early damp 10000, late damp 9400, boost 50 at 600 Hz */
static void room_init(room_t *r, double fs, double size, double predelay, double decay, double diffuse, double spin, double wander, double edamp, double ldamp, double boost, double boostlpf)
{
    long i;
    double tf, ex;
    memset(r, 0, sizeof *r);
    pool_used = 0;
    r->fs = fs; r->rs = size / 10.0; r->rng = 12345;
    tf = fs * r->rs / 34125.0; ex = fs / 34125.0;
    /* early */
    {
        static const double dl[6] = {0.0199, 0.0354, 0.0389, 0.0414, 0.0699, 0.0796}, dr[6] = {0.0209, 0.0364, 0.0399, 0.0424, 0.0709, 0.0806};
        static const float gl[6] = {1.020f, 0.818f, 0.635f, 0.719f, 0.267f, 0.242f}, gr[6] = {1.021f, 0.820f, 0.633f, 0.722f, 0.187f, 0.243f};
        double width = 0.8;
        long mx = 0;
        r->nt = 6;
        for (i = 0; i < 6; i++) {
            r->tapL[i] = (long)(fs * r->rs * dl[i]); r->tapR[i] = (long)(fs * r->rs * dr[i]);
            r->gL[i] = gl[i]; r->gR[i] = gr[i];
            if (r->tapL[i] > mx) mx = r->tapL[i];
            if (r->tapR[i] > mx) mx = r->tapR[i];
        }
        dly_init(&r->lineL, mx + 12); dly_init(&r->lineR, mx + 12);
        dly_init(&r->ltr, (long)(fs * 0.3 / 1000.0)); dly_init(&r->rtl, (long)(fs * 0.3 / 1000.0));
        r->ewet1 = (float)(width / 2 + 0.5); r->ewet2 = (float)((1 - width) / 2);
        bq_apf(&r->axL, 750, 4, fs); r->axR = r->axL;
        bq_apf(&r->a2L, 150, 4, fs); r->a2R = r->a2L;
        iir1_lpf(&r->eoL, edamp > fs / 2 ? fs / 2 : edamp, fs); r->eoR = r->eoL;
        r->wl.n = 0; r->wr.n = 0;
    }
    /* late */
    r->idiff = (float)(diffuse / 120.0); r->odiff = r->idiff; r->crossfeed = 0.4f;
    r->wander = (float)(wander / 200.0 + 0.1); r->wander2 = r->wander;
    r->wet1 = 1.0f; r->wet2 = 0.0f;
    {
        double rt60L = decay / r->rs, rt60s = decay * 1.0 / r->rs;
        float dl0 = (float)pow(10.0, log10(0.237) / rt60L), d1 = (float)pow(10.0, log10(0.938) / rt60s), d2 = (float)pow(10.0, log10(0.844) / rt60s), d3 = (float)pow(10.0, log10(0.906) / rt60s);
        r->loopdecay = dl0;
        for (i = 0; i < 10; i++) { apm_init(&r->iaL[i], pp(IAL[i], tf), (long)pp(EXC / 3.0, ex)); apm_init(&r->iaR[i], pp(IAR[i], tf), (long)pp(EXC / 3.0, ex)); r->iaL[i].fb = r->iaR[i].fb = -r->idiff; r->iaL[i].dec = r->iaR[i].dec = 1.0f; }
        for (i = 0; i < 4; i++) { ap_init(&r->caL[i], pp(CAL[i], tf)); ap_init(&r->caR[i], pp(CAR[i], tf)); r->caL[i].fb = r->caR[i].fb = r->odiff; }
        apm_init(&r->m15, pp(ALC[0], tf), pp(EXC, ex)); apm_init(&r->m17, pp(ALC[1], tf), pp(EXC, ex));
        apm_init(&r->m19, pp(ARC[0], tf), pp(EXC, ex)); apm_init(&r->m21, pp(ARC[1], tf), pp(EXC, ex));
        r->m15.fb = r->m19.fb = 0.375f; r->m17.fb = r->m21.fb = 0.312f;
        r->m15.dec = r->m19.dec = d2; r->m17.dec = r->m21.dec = d3;
        ap2_init(&r->a2L25, pp(ALC[2], tf), pp(ALC[3], tf)); ap2_init(&r->a2R43, pp(ARC[2], tf), pp(ARC[3], tf));
        r->a2L25.a1.fb = r->a2R43.a1.fb = 0.25f; r->a2L25.a2.fb = r->a2R43.a2.fb = 0.406f;
        r->a2L25.a1.dec = r->a2R43.a1.dec = d1; r->a2L25.a2.dec = r->a2R43.a2.dec = d2;
        ap3_init(&r->a3L34, pp(ALC[4], tf), pp(ALC[5], tf), pp(ALC[6], tf), pp(ALC[7], tf)); ap3_init(&r->a3R52, pp(ARC[4], tf), pp(ARC[5], tf), pp(ARC[6], tf), pp(ARC[7], tf));
        r->a3L34.fb1 = r->a3R52.fb1 = 0.25f; r->a3L34.fb2 = r->a3R52.fb2 = 0.25f; r->a3L34.fb3 = r->a3R52.fb3 = 0.406f;
        r->a3L34.d1 = r->a3R52.d1 = d1; r->a3L34.d2 = r->a3R52.d2 = d1; r->a3L34.d3 = r->a3R52.d3 = d2;
        dly_init(&r->d16, pp(DLC[1], tf)); dly_init(&r->d23, pp(DLC[2], tf)); dly_init(&r->d31, pp(DLC[3], tf)); dly_init(&r->d37, pp(DLC[4], tf));
        dly_init(&r->dts, pp(1.0, tf)); dly_init(&r->d40, pp(DRC[5], tf)); dly_init(&r->d49, pp(DRC[3], tf)); dly_init(&r->d58, pp(DRC[4], tf));
        for (i = 0; i < 11; i++) r->iOut[i] = pf(OUTC[i], tf);
        for (i = 0; i < 20; i++) r->iOut2[i] = pf(OUTC2[i], tf);
    }
    iir1_lpf(&r->lpfLin, 20000 > fs / 2 ? fs / 2 : 20000, fs); r->lpfRin = r->lpfLin;
    iir1_lpf(&r->lpfLd, ldamp > fs / 2 ? fs / 2 : ldamp, fs); r->lpfRd = r->lpfLd;
    bq_apf(&r->bassAPL, 150, 4, fs); r->bassAPR = r->bassAPL;
    bq_lpf(&r->lp9, boostlpf, 2.0, fs); r->lp7 = r->lp9;
    bq_lpf(&r->o1, (ldamp > fs / 2.5 ? fs / 2.5 : ldamp), 2.0, fs); r->o2 = r->o1;
    r->bassb = (float)(boost / 20.0 / pow(decay, 1.5) * (size / 10.0));
    lfo_set(&r->lfo1, spin, fs); lfo_set(&r->lfo2, sqrt(100.0 - (10.0 - spin) * (10.0 - spin)) / 2.0, fs);
    iir1_lpf(&r->lfo1l, 20 > fs / 2 ? fs / 2 : 20, fs); iir1_lpf(&r->lfo2l, 12, fs);
    r->outn = pp(22.0, fs * 0.001); dly_init(&r->outL, r->outn); dly_init(&r->outR, r->outn);
    dly_init(&r->predL, (long)(fs * predelay / 1000.0)); dly_init(&r->predR, (long)(fs * predelay / 1000.0));
    {   /* dccut: 5 Hz */
        double c = 2 * 5.0 / fs;
        dcgain = (float)((sqrt(3.) - 2. * sin(M_PI * c)) / (sin(M_PI * c) + sqrt(3.) * cos(M_PI * c)));
    }
}

/* one sample of the late reverb: in L R -> out L R */
static void late_step(room_t *r, float inL, float inR, float *oL, float *oR)
{
    float outL = dccut(r->dcL, dcgain, inL), outR = dccut(r->dcR, dcgain, inR), mn = noise_step(r), lfo = (lfo_proc(&r->lfo1) + 0.09f * mn) * r->wander, sg = -1, crossL, crossR, mnoise;
    float Dout, Bout, fpL, fpR, l2;
    long i;
    lfo = iir1(&r->lfo1l, lfo);
    mnoise = mn * 0.06f;
    for (i = 0; i < 10; i++) {
        outL = apm_proc_dc(&r->iaL[i], outL, lfo * sg, mnoise);
        outR = apm_proc_dc(&r->iaR[i], outR, lfo, mnoise * sg);
        sg = -sg;
    }
#ifdef DFR_DBG
    dfr_dbg[1] = outL;
#endif
    crossL = outL; crossR = outR;
    for (i = 0; i < 4; i++) { crossL = ap_proc(&r->caL[i], crossL); crossR = ap_proc(&r->caR[i], crossR); }
    outL = iir1(&r->lpfLin, outL + r->crossfeed * crossR);
    outR = iir1(&r->lpfRin, outR + r->crossfeed * crossL);
#ifdef DFR_DBG
    dfr_dbg[2] = outL;
#endif
    crossR = dly_last(&r->d58); crossL = dly_last(&r->d37);
    outL += r->loopdecay * (crossR + r->bassb * bq(&r->lp9, bq(&r->bassAPL, crossR)));
    outR += r->loopdecay * (crossL + r->bassb * bq(&r->lp7, bq(&r->bassAPR, crossL)));
#ifdef DFR_DBG
    dfr_dbg[3] = outL;
#endif
    outL = apm_proc_dc(&r->m17, dly_proc(&r->d16, apm_proc_dc(&r->m15, iir1(&r->lpfLd, outL), lfo, mnoise)), -lfo, -mnoise);
    outR = apm_proc_dc(&r->m21, dly_proc(&r->dts, apm_proc_dc(&r->m19, iir1(&r->lpfRd, outR), -lfo, -mnoise)), lfo, mnoise);
    dly_proc(&r->d37, ap3_proc(&r->a3L34, dly_proc(&r->d31, ap2_proc(&r->a2L25, dly_proc(&r->d23, outL))), lfo));
    dly_proc(&r->d58, ap3_proc(&r->a3R52, dly_proc(&r->d49, ap2_proc(&r->a2R43, dly_proc(&r->d40, outR))), -lfo));
    Dout = dly_z(&r->d23, r->iOut[8]) * 0.469f
        + (dly_z(&r->d31, r->iOut[7]) - dly_z(&r->d49, r->iOut[9]) + dly_z(&r->d31, r->iOut[0]) - dly_z(&r->d40, r->iOut2[0]) - dly_z(&r->d49, r->iOut[1])) * 0.219f
        + (dly_z(&r->d37, r->iOut[10]) + ap2_z1(&r->a2L25, r->iOut2[4]) + ap2_z2(&r->a2L25, r->iOut2[6]) - ap2_z2(&r->a2R43, r->iOut2[8])
           + ap3_z1(&r->a3L34, r->iOut2[10]) + ap3_z2(&r->a3L34, r->iOut2[12]) + ap3_z3(&r->a3L34, r->iOut2[14]) - ap3_z2(&r->a3R52, r->iOut2[18])) * 0.064f
        + dly_z(&r->d37, r->iOut2[16]) * 0.045f;
    Bout = dly_z(&r->d40, r->iOut[2]) * 0.469f
        + (dly_z(&r->d49, r->iOut[1]) - dly_z(&r->d31, r->iOut[3]) + dly_z(&r->d49, r->iOut[5]) - dly_z(&r->d37, r->iOut[6]) - dly_z(&r->d23, r->iOut2[1]) - dly_z(&r->d31, r->iOut2[3])) * 0.219f
        + (dly_z(&r->d58, r->iOut[4]) + ap2_z1(&r->a2R43, r->iOut2[5]) + ap2_z2(&r->a2R43, r->iOut2[7]) - ap2_z2(&r->a2L25, r->iOut2[9])
           + ap3_z1(&r->a3R52, r->iOut2[11]) + ap3_z2(&r->a3R52, r->iOut2[13]) + ap3_z3(&r->a3R52, r->iOut2[15]) - ap3_z2(&r->a3L34, r->iOut2[19])) * 0.064f
        + dly_z(&r->d58, r->iOut2[17]) * 0.045f;
#ifdef DFR_DBG
    dfr_dbg[4] = Dout;
#endif
    l2 = iir1(&r->lfo2l, lfo_proc(&r->lfo2) * r->wander2);
    {   /* the output combs: a feed-forward comb whose gain is the LFO */
        float oa = r->outL.b[r->outL.i] * l2 + Dout, ob = r->outR.b[r->outR.i] * -l2 + Bout;
        r->outL.b[r->outL.i] = Dout; r->outR.b[r->outR.i] = Bout;
        if (++r->outL.i >= r->outL.n) r->outL.i = 0;
        if (++r->outR.i >= r->outR.n) r->outR.i = 0;
        outL = oa; outR = ob;
    }
    fpL = dly_proc(&r->predL, bq(&r->o1, outL));
    fpR = dly_proc(&r->predR, bq(&r->o2, outR));
    *oL = fpL * r->wet1 + fpR * r->wet2;
    *oR = fpR * r->wet1 + fpL * r->wet2;
}

static float line_at(dly_t *d, long k) { long r = d->i + k; if (r >= d->n) r -= d->n; return d->b[r]; }
static void line_put(dly_t *d, float x) { d->i = d->i ? d->i - 1 : d->n - 1; d->b[d->i] = x; }

/* the early reflections: in L R -> out L R (the wet part only) */
static void early_step(room_t *r, float inL, float inR, float *oL, float *oR)
{
    float wl = 0, wr = 0;
    long i;
    line_put(&r->lineL, inL); line_put(&r->lineR, inR);
    for (i = 0; i < r->nt; i++) { wl += r->gL[i] * line_at(&r->lineL, r->tapL[i]); wr += r->gR[i] * line_at(&r->lineR, r->tapR[i]); }
    {
        float rl = r->rtl.n ? dly_proc(&r->rtl, inR + wr) : inR + wr, lr = r->ltr.n ? dly_proc(&r->ltr, inL + wl) : inL + wl;
        *oL = iir1(&r->eoL, bq(&r->a2L, r->ewet1 * wl + r->ewet2 * bq(&r->axL, rl)));
        *oR = iir1(&r->eoR, bq(&r->a2R, r->ewet1 * wr + r->ewet2 * bq(&r->axR, lr)));
    }
}

#ifdef DFR_MAIN
int main(int argc, char **argv)
{
    double fs = argc > 1 ? atof(argv[1]) : 44100;
    long n = (long)(fs * 4), i;
    FILE *f;
    pool_n = 4u << 20;
    pool = malloc(pool_n * sizeof(float));
    room_init(&R, fs, 12, 8, 0.4, 70, 0.8, 40, 10000, 9400, 50, 600);
    f = fopen(argc > 2 ? argv[2] : "ir_c.raw", "wb");
    for (i = 0; i < n; i++) {
        float x = i == 0 ? 1.0f : 0.0f, eL, eR, lL, lR, o[4];
        early_step(&R, x, x, &eL, &eR);
        late_step(&R, 0.2f * eL + x, 0.2f * eR + x, &lL, &lR);
#ifdef DFR_DBG
        if (i < 3000 && getenv("DFR_DBGOUT")) fprintf(stderr, "%ld %g %g %g %g\n", i, dfr_dbg[1], dfr_dbg[2], dfr_dbg[3], dfr_dbg[4]);
#endif
        o[0] = eL; o[1] = eR; o[2] = lL; o[3] = lR;
        fwrite(o, 4, 4, f);
    }
    fclose(f);
    return 0;
}
#endif
