/* LXRXXL BAZZLINE -- the instance: setup, the factory project and the audio block. */
#include <math.h>
#include <string.h>
#include "bl.h"
#include "param_map.h"

float bl_tempo(Bl *x)
{
    if (x->p.bpm > 0.f) return x->p.bpm;
    return x->seq.hostTempo > 0 ? (float)x->seq.hostTempo : 120.f;
}

const TrackPat *bl_trackPat(Bl *x, int t)
{
    while (t > 0 && (x->p.links & (1u << t))) t--;   /* linked: the steps of the track before it */
    return &bl_pat(x)->t[t];
}

static void init_sound(Sound *s)
{
    int i;
    for (i = 0; i < SP_COUNT; i++) s->v[i] = SOUND_DEFS[i].def;
}

void bl_initProject(Project *p)
{
    int i, t, k;
    memset(p, 0, sizeof *p);
    for (t = 0; t < NUM_TRACKS; t++) init_sound(&p->snd[t]);
    for (i = 0; i < NUM_PATTERNS; i++)
        for (t = 0; t < NUM_TRACKS; t++) {
            TrackPat *tp = &p->pats[i].t[t];
            tp->len = 16;
            for (k = 0; k < NUM_STEPS; k++) tp->s[k].note = 36;
        }
    for (i = 0; i < G_COUNT; i++) p->g[i] = GLOBAL_DEFS[i].def;
    p->bpm = 0;
    p->gdOn = 0;
}

void bl_init(Bl *x)
{
    int t;
    float *b0 = x->fx.buf[0], *b1 = x->fx.buf[1];
    char dir[512];
    memcpy(dir, x->dataDir, sizeof dir);
    memset(x, 0, sizeof *x);
    memcpy(x->dataDir, dir, sizeof dir);
    x->fx.buf[0] = b0;
    x->fx.buf[1] = b1;
    for (t = 0; t < NUM_TRACKS; t++) voice_init(&x->v[t]);
    fx_init(&x->fx);
    x->nextPat = 0xff;
    x->seq.rng = 0x51ED270Bu;
    x->rnd = 0x7F4A7C15u;
    x->seq.hostTempo = 120;
    bl_factory(x);
    ui_init(x);
}

/* ---------------------------------------------------------------- audio */
static inline float soft(float v)
{
    if (v > 3.f) return 1.f;
    if (v < -3.f) return -1.f;
    return v * (27.f + v * v) / (27.f + 9.f * v * v);
}

void bl_render(Bl *x, float *L, float *R, int n)
{
    int done = 0, t, guard = 0;
    memset(L, 0, sizeof(float) * (size_t)n);
    memset(R, 0, sizeof(float) * (size_t)n);
    while (done < n && guard++ < 4096) {
        const double d = seq_nextEventIn(x);
        int chunk = n - done;
        if (d < chunk) chunk = (int)ceil(d - 1e-9);
        if (chunk > 0) {
            const float bpm = bl_tempo(x);
            for (t = 0; t < NUM_TRACKS; t++) voice_render(&x->v[t], &x->p.snd[t], bpm, L + done, R + done, chunk);
        }
        seq_advance(x, chunk, done);
        done += chunk;
    }
    fx_render(x, L, R, n);
    for (t = 0; t < n; t++) { L[t] = soft(L[t] * 0.8f); R[t] = soft(R[t] * 0.8f); }
    x->clock += (uint32_t)n;
}
