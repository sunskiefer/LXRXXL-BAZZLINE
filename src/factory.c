/* LXRXXL BAZZLINE -- the factory project: a bass sound on each track and six demo patterns (A1-A6).
 *   1 ACID (diode ladder, saw, accent sweep, slides)   5 FM GROWL (FM, folded)
 *   2 DB-01 style (Polivoks, square, BBD swarm)        6 808 (sine, pitch envelope, long decay)
 *   3 REESE (unison saws, slow filter LFO)             7 PLUCK (square, short envelopes)
 *   4 SUB (sine, gentle)                                8 WAVE (wavetable morph on the modulator) */
#include <string.h>
#include "bl.h"

typedef struct { uint8_t sp; float v; } Set;

#define END {255, 0}
static const Set SOUNDS[NUM_TRACKS][24] = {
    {{SP_OSC_MODE, 0}, {SP_WAVE, 1}, {SP_FMODEL, 1}, {SP_FMODE, 1}, {SP_CUTOFF, .28f}, {SP_RESO, .8f}, {SP_VCF_ENV, .55f},
     {SP_VCF_DEC, .36f}, {SP_VCA_DEC, .55f}, {SP_ACC_AMT, .7f}, {SP_ACC_SWEEP, .55f}, {SP_SLIDE, .33f}, {SP_DRIVE, .3f},
     {SP_TONE, .8f}, {SP_KEYTRACK, .3f}, {SP_LEVEL, .78f}, END},
    {{SP_OSC_MODE, 0}, {SP_WAVE, 0}, {SP_FMODEL, 0}, {SP_FMODE, 1}, {SP_CUTOFF, .33f}, {SP_RESO, .62f}, {SP_VCF_ENV, .5f},
     {SP_VCF_DEC, .4f}, {SP_DRIVE, .45f}, {SP_CURVE, 1}, {SP_SPREAD, .35f}, {SP_SW_MODE, 0}, {SP_SW_MIX, .6f},
     {SP_SW_RATE, .3f}, {SP_SUB, .4f}, {SP_ACC_AMT, .6f}, {SP_LEVEL, .75f}, END},
    {{SP_OSC_MODE, 0}, {SP_WAVE, 1}, {SP_SW_MODE, 1}, {SP_SPREAD, .6f}, {SP_SW_MIX, .8f}, {SP_FMODEL, 0}, {SP_CUTOFF, .45f},
     {SP_RESO, .25f}, {SP_VCF_ENV, .15f}, {SP_VCA_DEC, 0}, {SP_MOD_DEST, 1}, {SP_MOD_WAVE, 0}, {SP_LFO_LVL, .25f},
     {SP_MOD_SYNC, 1}, {SP_LFO_RATE, 1.f / 9}, {SP_DRIVE, .15f}, {SP_DRIVE_POS, 1}, {SP_SUB, .25f}, {SP_LEVEL, .72f}, END},
    {{SP_OSC_MODE, 2}, {SP_WAVE_POS, 0}, {SP_SUB, .3f}, {SP_CUTOFF, .3f}, {SP_RESO, .1f}, {SP_VCF_ENV, .1f},
     {SP_VCA_DEC, .6f}, {SP_DRIVE, .1f}, {SP_LEVEL, .9f}, {SP_ACC_AMT, .3f}, END},
    {{SP_OSC_MODE, 1}, {SP_FM_RATIO, 3.f / 13}, {SP_FM, .55f}, {SP_MOD_DEST, 3}, {SP_MOD_WAVE, 0}, {SP_MOD_SYNC, 1},
     {SP_LFO_RATE, 5.f / 9}, {SP_LFO_LVL, .5f}, {SP_CURVE, 2}, {SP_DRIVE, .4f}, {SP_FMODEL, 1}, {SP_CUTOFF, .5f},
     {SP_RESO, .5f}, {SP_VCF_ENV, .3f}, {SP_LEVEL, .7f}, END},
    {{SP_OSC_MODE, 2}, {SP_WAVE_POS, 0}, {SP_PITCH_AMT, .55f}, {SP_PITCH_DEC, .3f}, {SP_VCA_DEC, .68f}, {SP_CUTOFF, .6f},
     {SP_RESO, 0}, {SP_VCF_ENV, 0}, {SP_DRIVE, .1f}, {SP_DRIVE_POS, 1}, {SP_LEVEL, .85f}, {SP_ACC_AMT, .4f}, END},
    {{SP_OSC_MODE, 0}, {SP_WAVE, 0}, {SP_FMODEL, 0}, {SP_CUTOFF, .25f}, {SP_RESO, .45f}, {SP_VCF_ENV, .7f},
     {SP_VCF_DEC, .25f}, {SP_VCA_DEC, .35f}, {SP_NOISE, .1f}, {SP_ACC_AMT, .5f}, {SP_LEVEL, .65f}, END},
    {{SP_OSC_MODE, 2}, {SP_WAVE_POS, .4f}, {SP_MOD_DEST, 3}, {SP_MOD_WAVE, 0}, {SP_LFO_RATE, .35f}, {SP_LFO_LVL, .6f},
     {SP_FMODEL, 0}, {SP_CUTOFF, .5f}, {SP_RESO, .4f}, {SP_VCF_ENV, .3f}, {SP_SPREAD, .2f}, {SP_LEVEL, .7f}, END},
};

/* step lists: step number (1-16), note, flags */
typedef struct { uint8_t pat, track, step, note, f; } Note;
#define G ST_GATE
#define A (ST_GATE | ST_ACC)
#define S (ST_GATE | ST_SLIDE)
#define AS (ST_GATE | ST_ACC | ST_SLIDE)
static const Note NOTES[] = {
    /* A1: acid on 1, the sub on 4 */
    {0, 0, 1, 36, A}, {0, 0, 2, 36, G}, {0, 0, 3, 48, S}, {0, 0, 4, 39, G}, {0, 0, 5, 36, A}, {0, 0, 7, 43, S},
    {0, 0, 8, 46, G}, {0, 0, 9, 36, A}, {0, 0, 10, 48, G}, {0, 0, 12, 41, S}, {0, 0, 13, 43, A}, {0, 0, 14, 36, G},
    {0, 0, 15, 51, S}, {0, 0, 16, 48, G},
    {0, 3, 1, 36, G}, {0, 3, 5, 36, G}, {0, 3, 9, 36, G}, {0, 3, 13, 36, G},
    /* A2: the DB-01 line on 2 */
    {1, 1, 1, 36, A}, {1, 1, 3, 36, G}, {1, 1, 4, 48, S}, {1, 1, 5, 43, G}, {1, 1, 7, 36, A}, {1, 1, 8, 46, G},
    {1, 1, 9, 36, G}, {1, 1, 11, 51, S}, {1, 1, 12, 48, G}, {1, 1, 13, 41, A}, {1, 1, 15, 43, S}, {1, 1, 16, 48, G},
    /* A3: the Reese on 3, held with slides */
    {2, 2, 1, 36, S}, {2, 2, 2, 36, S}, {2, 2, 3, 36, S}, {2, 2, 4, 36, S}, {2, 2, 5, 36, S}, {2, 2, 6, 36, S},
    {2, 2, 7, 36, S}, {2, 2, 8, 36, S}, {2, 2, 9, 34, S}, {2, 2, 10, 34, S}, {2, 2, 11, 34, S}, {2, 2, 12, 34, S},
    {2, 2, 13, 31, S}, {2, 2, 14, 31, S}, {2, 2, 15, 33, S}, {2, 2, 16, 33, G},
    /* A4: the 808 on 6, the pluck on 7 */
    {3, 5, 1, 36, A}, {3, 5, 4, 36, G}, {3, 5, 7, 36, G}, {3, 5, 11, 43, G}, {3, 5, 13, 34, A},
    {3, 6, 3, 60, G}, {3, 6, 7, 63, G}, {3, 6, 11, 67, A}, {3, 6, 15, 65, G},
    /* A5: FM growl on 5 */
    {4, 4, 1, 36, A}, {4, 4, 2, 36, G}, {4, 4, 4, 48, S}, {4, 4, 5, 46, G}, {4, 4, 7, 36, A}, {4, 4, 9, 39, G},
    {4, 4, 10, 41, S}, {4, 4, 11, 43, G}, {4, 4, 13, 36, A}, {4, 4, 15, 48, G},
    /* A6: the wavetable on 8 */
    {5, 7, 1, 36, G}, {5, 7, 3, 39, G}, {5, 7, 5, 43, S}, {5, 7, 6, 41, G}, {5, 7, 9, 36, A}, {5, 7, 11, 46, G},
    {5, 7, 13, 43, S}, {5, 7, 14, 39, G},
};

void bl_factory(Bl *x)
{
    int t, i;
    bl_initProject(&x->p);
    for (t = 0; t < NUM_TRACKS; t++)
        for (i = 0; i < 24 && SOUNDS[t][i].sp != 255; i++) x->p.snd[t].v[SOUNDS[t][i].sp] = SOUNDS[t][i].v;
    for (i = 0; i < (int)(sizeof NOTES / sizeof NOTES[0]); i++) {
        Step *st = &x->p.pats[NOTES[i].pat].t[NOTES[i].track].s[NOTES[i].step - 1];
        st->note = NOTES[i].note;
        st->f = NOTES[i].f;
    }
    x->p.pats[2].t[2].scale = 0;
    x->patSlot = 0;
    x->nextPat = 0xff;
    if (x->seq.running) seq_stop(x);
    for (t = 0; t < NUM_TRACKS; t++) voice_kill(&x->v[t]);
    fx_init(&x->fx);
    x->displayRev++;
}
