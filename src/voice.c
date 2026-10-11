/* LXRXXL BAZZLINE -- one bass voice, after the Erica Synths Bassline (DB-01) signal path:
 *
 *   OSC (VCO saw/square/triangle | FM | WAVE morph) + SUB (-1 oct square) + NOISE
 *     -> SWARM (BBD detune | unison)  -> DRIVE (pre) -> FILTER (Polivoks 12 dB SVF | diode ladder, LP/BP, 2x oversampled)
 *     -> DRIVE (post) -> TONE -> VCA -> level, pan
 *
 * with an amp envelope (VCA ATK / VCA ENV), a filter envelope (VCF ATK / VCF DEC, amount VCF ENV), a pitch envelope
 * (ENV/CV LVL, PITCH DEC), accent (AMOUNT, the 303-style SWEEP that builds up over consecutive accents), slide
 * (SLIDE TIME) and a modulator (TRI SAW SQR S&H NOISE, free or on the tempo, to PITCH CUTOFF DRIVE or WAVE).
 *
 * The oscillator reads the LXR's band-limited tables (third_party/lxr02open wavetable.c: one table per octave, chosen
 * the way the LXR's oscillator chooses it). The Bassline's own circuits are analogue; nothing here is Erica's code. */
#include <math.h>
#include <string.h>
#include "bl.h"
#include "wavetable.h"

#define CR 8                 /* control rate: pitch, filter and envelope coefficients every 8 frames (0.18 ms) */
#define TWO_PI 6.283185307f

static const float FM_RATIOS[] = {0.25f, 0.5f, 0.75f, 1.f, 1.5f, 2.f, 2.5f, 3.f, 3.5f, 4.f, 5.f, 6.f, 7.f, 8.f};
#define NUM_RATIOS ((int)(sizeof FM_RATIOS / sizeof FM_RATIOS[0]))
/* the modulator on the tempo: 4 bars .. 1/32 (in beats), and the display's names for them */
static const float LFO_BEATS[] = {16, 8, 4, 2, 1, 0.5f, 1.f / 3, 0.25f, 1.f / 6, 0.125f};
const char *const LFO_DIV_NAMES[] = {"4BR", "2BR", "1BR", "1/2", "1/4", "1/8", "8T", "16", "16T", "32"};
#define NUM_LFO_BEATS ((int)(sizeof LFO_BEATS / sizeof LFO_BEATS[0]))

float voice_fmRatio(float v) { int i = (int)lroundf(clampf(v, 0, 1) * (NUM_RATIOS - 1)); return FM_RATIOS[i]; }
float voice_lfoBeats(float v) { int i = (int)lroundf(clampf(v, 0, 1) * (NUM_LFO_BEATS - 1)); return LFO_BEATS[i]; }
float voice_lfoHz(float v) { return 0.05f * exp2f(clampf(v, 0, 1) * 9.6f); }

/* ---------------------------------------------------------------- the LXR tables */
/* phases are 32-bit (one cycle = 2^32), as in the LXR's oscillator */
#define PH_PER_CYCLE 4294967296.0
static uint32_t wave_offset[4];   /* where each wave's fundamental starts, so SQR, SAW, TRI and the sine line up */
static int offsets_done;

static inline float rd1024(const int16_t *t, uint32_t ph)
{
    const uint32_t i = ph >> 22;
    const float fr = (float)(ph & 0x3FFFFFu) * (1.f / 4194304.f);
    const int a = t[i], b = t[(i + 1) & 1023];
    return ((float)a + fr * (float)(b - a)) * (1.f / 32767.f);
}

static inline float rd_sine(uint32_t ph)
{
    const uint32_t i = ph >> 20;   /* TABLESIZE 4096 (+1 guard) */
    const float fr = (float)(ph & 0xFFFFFu) * (1.f / 1048576.f);
    const int a = sine_table[i], b = sine_table[i + 1];
    return ((float)a + fr * (float)(b - a)) * (1.f / 32767.f);
}

static void find_offsets(void)
{
    /* the phase of each table's first harmonic (one DFT bin): shift every wave to start like the sine */
    const int16_t *tabs[3] = {recTable[0], sawTable[0], triTable[0]};
    int w, i;
    double ref;
    {
        double re = 0, im = 0;
        for (i = 0; i < TABLESIZE; i++) { re += sine_table[i] * cos(TWO_PI * i / TABLESIZE); im += sine_table[i] * sin(TWO_PI * i / TABLESIZE); }
        ref = atan2(im, re);
    }
    for (w = 0; w < 3; w++) {
        double re = 0, im = 0, f;
        for (i = 0; i < 1024; i++) { re += tabs[w][i] * cos(TWO_PI * i / 1024.0); im += tabs[w][i] * sin(TWO_PI * i / 1024.0); }
        f = (atan2(im, re) - ref) / TWO_PI;
        f -= floor(f);
        wave_offset[w] = (uint32_t)(f * PH_PER_CYCLE);
    }
    wave_offset[3] = 0;
    offsets_done = 1;
}

/* wave 0 SQR, 1 SAW, 2 TRI, 3 SINE at phase ph for table index ti */
static inline float wave_at(int w, int ti, uint32_t ph)
{
    switch (w) {
    case 0: return rd1024(recTable[ti], ph + wave_offset[0]);
    case 1: return rd1024(sawTable[ti], ph + wave_offset[1]);
    case 2: return rd1024(triTable[ti], ph + wave_offset[2]);
    default: return rd_sine(ph);
    }
}

/* WAVE mode: SINE -> TRI -> SAW -> SQR along pos 0..1 */
static inline float morph_at(float pos, int ti, uint32_t ph)
{
    static const int order[4] = {3, 2, 1, 0};
    const float p = clampf(pos, 0, 1) * 2.999f;
    const int i = (int)p;
    const float fr = p - i;
    const float a = wave_at(order[i], ti, ph);
    return fr < 0.002f ? a : a + fr * (wave_at(order[i + 1], ti, ph) - a);
}

/* ---------------------------------------------------------------- shapes */
static inline float ftanh(float x)
{
    if (x > 3.f) return 1.f;
    if (x < -3.f) return -1.f;
    return x * (27.f + x * x) / (27.f + 9.f * x * x);
}

static inline float shape(int curve, float x, float g)
{
    x *= g;
    switch (curve) {
    case 1: return x > 1.f ? 1.f : x < -1.f ? -1.f : x;                         /* HARD */
    case 2: return sinf(x * 1.5707963f) * (x > 4.f || x < -4.f ? 0.9f : 1.f); /* FOLD */
    default: return ftanh(x);                                                   /* SOFT */
    }
}

static inline float coef(float seconds) { return 1.f - expf(-1.f / (seconds * SR / CR)); }   /* one-pole, per CR frames */

/* ---------------------------------------------------------------- notes */
void voice_init(Voice *v)
{
    memset(v, 0, sizeof *v);
    v->noise = 0x9E3779B9u;
    v->lfoRnd = 0x2545F491u;
    v->note = v->target = 48;
    if (!offsets_done) find_offsets();
}

void voice_kill(Voice *v)
{
    const uint32_t n = v->noise, r = v->lfoRnd;
    voice_init(v);
    v->noise = n;
    v->lfoRnd = r;
}

void voice_noteOn(Voice *v, const Sound *s, float note, float vel, int accent, int slide, int penv, int mod)
{
    const int legato = slide && v->gate && v->aStage;
    v->target = note;
    v->vel = vel;
    v->modOn = (uint8_t)mod;
    if (legato) {
        /* a slide: no new envelopes, the pitch glides (the 303's and the DB-01's slide) */
        v->glide = coef(0.005f * powf(120.f, s->v[SP_SLIDE]));
        v->accent = accent ? 1.f : v->accent;
        return;
    }
    v->note = note;
    v->glide = 0;
    v->gate = 1;
    v->accent = accent ? 1.f : 0.f;
    if (accent) v->sweep = clampf(v->sweep + 0.25f + 0.5f * s->v[SP_ACC_SWEEP], 0, 1.5f);
    v->aStage = 1;
    v->fStage = 1;
    v->fenv = 0;
    if (penv) v->penv = 1.f;
    if (!v->active) {
        v->ph = v->phSub = v->phMod = 0;
        v->phUni[0] = 0x4F5C28F6u; v->phUni[1] = 0x91EB851Fu; v->phUni[2] = 0xD47AE147u;
    }
    v->active = 1;
    v->idle = 0;
    if (s->v[SP_MOD_WAVE] >= 3.f && s->v[SP_MOD_SYNC] >= 1.f) v->lfoPh = 0;   /* a tempo S&H / noise restarts with the note */
}

void voice_noteOff(Voice *v) { v->gate = 0; }

/* ---------------------------------------------------------------- the modulator */
static inline float lfo_step(Voice *v, int wave, double inc)
{
    double ph = v->lfoPh + inc;
    const int wrapped = ph >= 1.0;
    if (wrapped) ph -= floor(ph);
    v->lfoPh = ph;
    switch (wave) {
    case 0: return (float)(ph < 0.5 ? 4 * ph - 1 : 3 - 4 * ph);   /* TRI */
    case 1: return (float)(2 * ph - 1);                           /* SAW */
    case 2: return ph < 0.5 ? 1.f : -1.f;                         /* SQR */
    case 3:                                                       /* S&H */
        if (wrapped) v->lfoHold = (bl_rand(&v->lfoRnd) & 0xffff) / 32767.5f - 1.f;
        return v->lfoHold;
    default:                                                      /* NOISE: smooth random */
        if (wrapped) { v->lfoPrev = v->lfoNext; v->lfoNext = (bl_rand(&v->lfoRnd) & 0xffff) / 32767.5f - 1.f; }
        return v->lfoPrev + (float)ph * (v->lfoNext - v->lfoPrev);
    }
}

/* ---------------------------------------------------------------- render */
void voice_render(Voice *v, const Sound *s, float bpm, float *L, float *R, int n)
{
    int i, k;
    float gain, panL, panR;
    if (!v->active) return;
    {
        const float lv = s->v[SP_LEVEL];
        const float pan = clampf(s->v[SP_PAN], 0, 1);
        gain = 1.25f * lv * lv;
        panL = cosf(pan * 1.5707963f) * 1.41421356f;
        panR = sinf(pan * 1.5707963f) * 1.41421356f;
        if (panL > 1.f) panL = 1.f;
        if (panR > 1.f) panR = 1.f;
    }
    const int oscMode = (int)s->v[SP_OSC_MODE];
    const int wave = (int)s->v[SP_WAVE];
    const int curve = (int)s->v[SP_CURVE];
    const int post = s->v[SP_DRIVE_POS] >= 1.f;
    const int diode = s->v[SP_FMODEL] >= 1.f;
    const int bp = s->v[SP_FMODE] < 1.f;
    const int unison = s->v[SP_SW_MODE] >= 1.f;
    const int modWave = (int)s->v[SP_MOD_WAVE], modDest = (int)s->v[SP_MOD_DEST];
    const float ratio = voice_fmRatio(s->v[SP_FM_RATIO]);
    const float fmIdx = s->v[SP_FM] * s->v[SP_FM] * 6.f;
    const float subLvl = s->v[SP_SUB], noiseLvl = s->v[SP_NOISE] * 0.8f;
    const float spread = s->v[SP_SPREAD];
    const float swMix = s->v[SP_SW_MIX] * clampf(spread * 4.f, 0, 1);   /* DETUNE at zero: the swarm is out */
    const float driveK = s->v[SP_DRIVE];
    const float accAmt = s->v[SP_ACC_AMT];
    const float lfoLvl = s->v[SP_LFO_LVL];
    const double lfoInc = (s->v[SP_MOD_SYNC] >= 1.f ? (bpm > 0 ? bpm : 120.f) / 60.f / voice_lfoBeats(s->v[SP_LFO_RATE])
                                                     : voice_lfoHz(s->v[SP_LFO_RATE])) / SR;
    /* envelope times */
    const float aAtk = coef(0.0005f * powf(2000.f, s->v[SP_VCA_ATK]));
    /* VCA ENV fully down: the VCA follows the gate (organ-like, as the DB-01's); else the note decays on its own */
    const int gated = s->v[SP_VCA_DEC] <= 0.02f;
    const float aDec = expf(-1.f / (0.015f * powf(330.f, s->v[SP_VCA_DEC]) * SR / CR));
    const float aRel = expf(-1.f / (0.008f * SR / CR));
    const float fAtk = coef(0.0005f * powf(2000.f, s->v[SP_VCF_ATK]));
    const float fDecT = 0.01f * powf(300.f, s->v[SP_VCF_DEC]);
    const float fDecN = expf(-1.f / (fDecT * SR / CR));
    const float fDecA = expf(-1.f / ((fDecT < 0.2f ? fDecT : 0.2f) * SR / CR));   /* accented: the 303's short decay */
    const float pDec = expf(-1.f / (0.005f * powf(200.f, s->v[SP_PITCH_DEC]) * SR / CR));
    const float pAmt = 48.f * s->v[SP_PITCH_AMT] * s->v[SP_PITCH_AMT];
    const float sweepDec = expf(-1.f / (0.6f * SR / CR));
    const float envOct = s->v[SP_VCF_ENV] * 6.f;
    const float baseOct = log2f(30.f) + s->v[SP_CUTOFF] * 9.2f;
    const float keyTrack = s->v[SP_KEYTRACK];
    const float reso = s->v[SP_RESO];
    const float toneA = 1.f - expf(-TWO_PI * 500.f * powf(40.f, s->v[SP_TONE]) / SR);
    const double swInc = 0.05 * pow(120.0, s->v[SP_SW_RATE]) / SR;
    float uniDet[3];
    uniDet[0] = exp2f(-spread * 0.35f / 12.f);
    uniDet[1] = exp2f(-spread * 0.12f / 12.f);
    uniDet[2] = exp2f(spread * 0.2f / 12.f);

    /* control-rate state */
    float freq = 0, g2 = 0, kRes = 0, drvG = 1, wavePos = s->v[SP_WAVE_POS], fmNow = fmIdx, ampNow = 0;
    int ti = 0, tiSub = 0;
    float out[CR];

    for (i = 0; i < n; i += CR) {
        const int m = n - i < CR ? n - i : CR;
        /* ---- envelopes */
        if (v->aStage == 1) { v->aenv += aAtk * (1.25f - v->aenv); if (v->aenv >= 1.f) { v->aenv = 1.f; v->aStage = 2; } }
        else if (v->aStage == 2) {
            if (gated) { if (!v->gate) v->aenv *= aRel; }
            else v->aenv *= aDec;
            if (v->aenv < 1e-5f) { v->aenv = 0; v->aStage = 0; }
        }
        if (v->fStage == 1) { v->fenv += fAtk * (1.25f - v->fenv); if (v->fenv >= 1.f) { v->fenv = 1.f; v->fStage = 2; } }
        else if (v->fStage == 2) v->fenv *= v->accent > 0.5f ? fDecA : fDecN;
        v->penv *= pDec;
        v->sweep *= sweepDec;
        if (v->glide > 0) {
            v->note += v->glide * (v->target - v->note);
            if (fabsf(v->target - v->note) < 0.001f) { v->note = v->target; v->glide = 0; }
        } else v->note = v->target;
        /* ---- the modulator (one value per CR frames) */
        {
            const float lv = lfo_step(v, modWave, lfoInc * CR) * lfoLvl * (v->modOn ? 1.f : 0.f);
            float pitch = v->note + pAmt * v->penv;
            float cut = baseOct + keyTrack * (v->note - 48.f) / 12.f
                        + envOct * v->fenv * (1.f + accAmt * v->accent)
                        + accAmt * (0.8f * v->accent + 1.2f * v->sweep) ;
            float drv = driveK;
            wavePos = s->v[SP_WAVE_POS];
            fmNow = fmIdx;
            switch (modDest) {
            case 0: pitch += 12.f * lv; break;
            case 1: cut += 4.f * lv; break;
            case 2: drv = clampf(drv + 0.5f * lv, 0, 1); break;
            default:
                if (oscMode == 2) wavePos = clampf(wavePos + 0.5f * lv, 0, 1);
                else fmNow = fmIdx + 3.f * (lv > 0 ? lv : -lv) * lfoLvl;
                break;
            }
            freq = 440.f * exp2f((pitch - 69.f) / 12.f);
            if (freq > 18000.f) freq = 18000.f;
            /* the LXR's table for the octave (Oscillator.c freqToTableIndex), from the note directly */
            ti = pitch < 0.f ? 0 : pitch >= 132.f ? 10 : (int)(pitch / 12.f);
            tiSub = pitch < 12.f ? 0 : pitch >= 144.f ? 10 : (int)((pitch - 12.f) / 12.f);
            {
                /* the filter: at 2x the rate. Polivoks: g = tan(pi fc / fs2); diode ladder: a one-pole per stage */
                float fc = exp2f(clampf(cut, 4.3f, 14.1f));
                if (fc > 16000.f) fc = 16000.f;
                if (!diode) {
                    g2 = tanf(3.14159265f * fc / (2.f * SR));
                    kRes = 2.f * (1.f - 0.985f * reso);
                } else {
                    g2 = 1.f - expf(-TWO_PI * fc / (2.f * SR));
                    kRes = reso * 4.3f * (fc > 6000.f ? 6000.f / fc * 0.5f + 0.5f : 1.f);
                }
            }
            drvG = 1.f + 30.f * drv * drv;
            ampNow = v->aenv * (0.75f + 0.25f * v->vel) * (1.f + accAmt * v->accent * 0.9f);
        }
        /* ---- audio */
        {
            const uint32_t inc = (uint32_t)(freq / SR * PH_PER_CYCLE), incMod = (uint32_t)(freq * ratio / SR * PH_PER_CYCLE);
            const float pmScale = (float)(PH_PER_CYCLE / TWO_PI);
            float swD[2] = {0, 0};
            const float polG = 1.f / (1.f + kRes * g2 + g2 * g2);
            if (!unison && swMix > 0.f) {
                int t;
                for (t = 0; t < 2; t++) {   /* the BBD's modulated delay times, per CR frames */
                    double sp = v->swPh[t] + swInc * CR * (t ? 1.13 : 0.87);
                    sp -= floor(sp);
                    v->swPh[t] = sp;
                    swD[t] = (0.006f + 0.0025f * t + spread * 0.006f * (0.5f + 0.5f * sinf(TWO_PI * (float)sp))) * SR;
                }
            }
            for (k = 0; k < m; k++) {
                float o, x;
                uint32_t pm = v->ph;
                if (fmNow > 0.f) pm += (uint32_t)(int64_t)(rd_sine(v->phMod) * fmNow * pmScale);
                if (oscMode == 0) o = wave_at(wave, ti, pm);
                else if (oscMode == 1) o = rd_sine(pm);
                else o = morph_at(wavePos, ti, pm);
                if (unison && swMix > 0.f) {
                    int u;
                    float uo = 0;
                    const uint32_t dm = pm - v->ph;
                    for (u = 0; u < 3; u++) {
                        const uint32_t up = v->phUni[u] + dm;
                        uo += oscMode == 0 ? wave_at(wave, ti, up) : oscMode == 1 ? rd_sine(up) : morph_at(wavePos, ti, up);
                        v->phUni[u] += (uint32_t)(inc * uniDet[u]);
                    }
                    o = o * (1.f - 0.4f * swMix) + uo * 0.45f * swMix;
                }
                x = o;
                if (subLvl > 0.f) x += subLvl * rd1024(recTable[tiSub], v->phSub);
                if (noiseLvl > 0.f) {
                    v->noise ^= v->noise << 13; v->noise ^= v->noise >> 17; v->noise ^= v->noise << 5;
                    x += noiseLvl * ((int32_t)v->noise * (1.f / 2147483648.f));
                }
                v->ph += inc;
                v->phSub += inc >> 1;
                v->phMod += incMod;
                /* the BBD swarm: two modulated taps of a short delay, darkened */
                if (!unison && swMix > 0.f) {
                    int t;
                    float wet = 0;
                    v->bbd[v->bbdW] = x;
                    for (t = 0; t < 2; t++) {
                        const int di = (int)swD[t];
                        const float fr = swD[t] - di;
                        const int r0 = (v->bbdW - di) & (BBD_LEN - 1), r1 = (r0 - 1) & (BBD_LEN - 1);
                        wet += v->bbd[r0] + fr * (v->bbd[r1] - v->bbd[r0]);
                    }
                    v->bbdW = (v->bbdW + 1) & (BBD_LEN - 1);
                    v->bbdLp += 0.55f * (0.5f * wet - v->bbdLp);
                    x = x * (1.f - 0.45f * swMix) + ftanh(v->bbdLp * 1.2f) * 0.9f * swMix;
                }
                /* drive (pre) */
                if (!post) x = shape(curve, x, drvG) * (curve == 0 ? 1.f : 0.9f);
                /* the filter, twice per frame */
                {
                    int os;
                    float y = 0, xin[2];
                    xin[0] = 0.5f * (v->dcX + x);
                    xin[1] = x;
                    v->dcX = x;
                    for (os = 0; os < 2; os++) {
                        float lp, bpo;
                        if (!diode) {
                            const float hp = (xin[os] - (kRes + g2) * v->s1 - v->s2) * polG;
                            const float b = g2 * hp + v->s1;
                            v->s1 = g2 * hp + b;
                            lp = g2 * b + v->s2;
                            v->s2 = g2 * b + lp;
                            v->s1 = v->s1 / (1.f + 0.08f * fabsf(v->s1));   /* the Polivoks's integrators clip: its grit */
                            bpo = b;
                        } else {
                            const float u = ftanh(1.4f * (xin[os] - kRes * v->d4));
                            v->d1 += g2 * (u - v->d1);
                            v->d2 += g2 * (v->d1 - v->d2);
                            v->d3 += g2 * (v->d2 - v->d3);
                            v->d4 += g2 * (v->d3 - v->d4);
                            lp = v->d4 * (1.f + 0.3f * kRes) * 0.75f;
                            bpo = (v->d2 - v->d4) * (1.2f + 0.2f * kRes);
                        }
                        y += bp ? bpo : lp;
                    }
                    x = y * 0.5f;
                }
                if (post) x = shape(curve, x, drvG) * (curve == 0 ? 1.f : 0.9f);
                v->toneZ += toneA * (x - v->toneZ);
                out[k] = v->toneZ * ampNow;
            }
        }
        for (k = 0; k < m; k++) {
            L[i + k] += out[k] * gain * panL;
            R[i + k] += out[k] * gain * panR;
        }
    }
    /* asleep once silent (the swarm's short tail included) */
    if (v->aStage == 0) {
        v->idle += (uint32_t)n;
        if (v->idle > BBD_LEN * 2) {
            v->active = 0;
            v->s1 = v->s2 = v->d1 = v->d2 = v->d3 = v->d4 = 0;
        }
    } else v->idle = 0;
}
