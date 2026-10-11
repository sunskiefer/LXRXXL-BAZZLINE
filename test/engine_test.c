/* LXRXXL BAZZLINE -- engine tests (x86, under valgrind by test/run_tests.sh):
 *   - the six demo patterns play on MPC's transport: sound, no NaN, no clipping run-away, steps on the grid;
 *   - every key, every parameter and the encoders, in every mode, with random values (no crash, values in range);
 *   - the state chunk round-trips exactly; a damaged chunk is refused;
 *   - MIDI notes play the selected track, notes on the duck's channel duck the mix;
 *   - the delay: HOLD loops, REVERSE and TAPE/DIGITAL run clean; the display shows the tempo.
 * With an argument (a folder) it writes each demo pattern to <folder>/A<n>.wav to listen to. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "engine.h"
#include "bl.h"
#include "param_ids.h"
#include "param_map.h"

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static const mpc_engine_t *E;

static void set(void *in, const char *k, const char *v) { E->set_param(in, k, v); }
static void setf(void *in, const char *k, float v) { char b[32]; snprintf(b, sizeof b, "%g", v); E->set_param(in, k, b); }
static float getf(void *in, const char *k) { char b[64] = ""; E->get_param(in, k, b, sizeof b); return (float)atof(b); }

/* render seconds of audio with MPC playing at tempo from ppq; returns peak, rms in *rms */
static float play(void *in, double *ppq, double tempo, double secs, int playing, double *rms, FILE *wav)
{
    int16_t buf[256];
    int blocks = (int)(secs * 44100 / 128), b, i;
    float peak = 0;
    double sum = 0;
    long n = 0;
    for (b = 0; b < blocks; b++) {
        char ht[96];
        snprintf(ht, sizeof ht, "%d %.9f %.6f", playing, *ppq, tempo);
        set(in, "host_time", ht);
        E->render(in, buf, 128);
        for (i = 0; i < 256; i++) {
            const float v = buf[i] / 32768.f;
            if (fabsf(v) > peak) peak = fabsf(v);
            sum += v * v;
            n++;
        }
        if (wav) fwrite(buf, 2, 256, wav);
        if (playing) *ppq += 128.0 / 44100.0 * tempo / 60.0;
    }
    *rms = sqrt(sum / n);
    return peak;
}

static void wav_header(FILE *f, int frames)
{
    const int data = frames * 4;
    const int hdr[] = {0x46464952, 36 + data, 0x45564157, 0x20746d66, 16, 0x00020001, 44100, 44100 * 4, 0x00100004,
                       0x61746164, data};
    fwrite(hdr, 4, 11, f);
}

int main(int argc, char **argv)
{
    void *in, *in2;
    Bl *x;
    double ppq = 0, rms;
    float peak;
    int p, i, k;
    char key[32];
    E = mpc_engine();
    in = E->create(NULL);
    CHECK(in != NULL, "create");
    x = (Bl *)in;   /* Inst starts with the Bl */

    /* ---- the demo patterns, on MPC's transport */
    for (p = 0; p < 6; p++) {
        FILE *wav = NULL;
        char path[600];
        int16_t dummy;
        (void)dummy;
        seq_loadPattern(x, (uint8_t)p);
        ppq = 0;
        if (argc > 1) {
            snprintf(path, sizeof path, "%s/A%d.wav", argv[1], p + 1);
            wav = fopen(path, "wb");
            if (wav) wav_header(wav, (int)(8.0 * 44100 / 128) * 128);
        }
        play(in, &ppq, 120, 0.05, 0, &rms, NULL);   /* MPC stopped */
        peak = play(in, &ppq, 120, 8.0, 1, &rms, wav);
        if (wav) fclose(wav);
        printf("A%d: peak %.3f rms %.4f\n", p + 1, peak, rms);
        CHECK(rms > 0.01, "pattern A%d is silent (rms %f)", p + 1, rms);
        CHECK(peak < 0.999, "pattern A%d clips (peak %f)", p + 1, peak);
        CHECK(x->seq.running, "the sequencer follows MPC's play");
        play(in, &ppq, 120, 0.1, 0, &rms, NULL);   /* MPC stops */
        CHECK(!x->seq.running, "the sequencer stops with MPC");
        play(in, &ppq, 120, 2.0, 0, &rms, NULL);
    }
    seq_loadPattern(x, 0);

    /* ---- the grid: step 5 fires exactly on beat 2 (ppq 1.0) */
    {
        double q = 0.75 - 128.0 / 44100.0 * 2.0;
        int fired = -1, b2;
        uint32_t before;
        play(in, &q, 120, 0.01, 1, &rms, NULL);
        before = x->seq.n;
        for (b2 = 0; b2 < 200 && fired < 0; b2++) {
            play(in, &q, 120, 128.0 / 44100.0, 1, &rms, NULL);
            if (x->seq.n >= 5 && before < 5) fired = b2;
        }
        CHECK(fired >= 0 && fabs(q - 1.0) < 2 * 128.0 / 44100.0 * 2.0, "step 5 at beat 2 (ppq now %.4f)", q);
        play(in, &q, 120, 0.1, 0, &rms, NULL);
    }

    /* ---- the display: the tempo, MPC's dot */
    play(in, &ppq, 97, 2.0, 0, &rms, NULL);
    CHECK((int)getf(in, "disp2") == (int)(strchr(GLYPHS, '9') - GLYPHS) && (int)getf(in, "disp3") == (int)(strchr(GLYPHS, '7') - GLYPHS),
          "the display shows 97 (%g %g)", getf(in, "disp2"), getf(in, "disp3"));
    CHECK(getf(in, "dot3") == 1.f, "the last dot: MPC's tempo");

    /* ---- every key, every parameter, every mode, random values */
    srand(7);
    for (i = 0; i < 20000; i++) {
        const int r = rand() % P_COUNT;
        if (PARAM_MAP[r].kind == PK_BUTTON) set(in, PARAM_KEYS[r], "1");
        else if (PARAM_MAP[r].nopts) {
            snprintf(key, sizeof key, "%d", rand() % (PARAM_MAP[r].nopts + 2) - 1);
            set(in, PARAM_KEYS[r], key);
        } else setf(in, PARAM_KEYS[r], (rand() % 1200) / 1000.f - 0.1f);
        if (i % 50 == 0) {
            peak = play(in, &ppq, 120 + (i % 7) * 20, 0.05, (i / 500) & 1, &rms, NULL);
            CHECK(peak == peak && peak <= 1.f, "audio after random edits");
        }
        if (i % 97 == 0)
            for (k = 0; k < P_COUNT; k++) {
                char b[128] = "";
                char k2[48];
                E->get_param(in, PARAM_KEYS[k], b, sizeof b);
                snprintf(k2, sizeof k2, "%s_on", PARAM_KEYS[k]);
                E->get_param(in, k2, b, sizeof b);
            }
    }
    for (k = 0; k < P_COUNT; k++) {
        const float v = getf(in, PARAM_KEYS[k]);
        if (PARAM_MAP[k].kind == PK_BUTTON) continue;
        if (PARAM_MAP[k].nopts) CHECK(v >= 0 && v <= PARAM_MAP[k].nopts - 1, "%s option %g", PARAM_KEYS[k], v);
        else CHECK(v >= 0 && v <= 1, "%s value %g", PARAM_KEYS[k], v);
    }
    /* leave every mode */
    for (i = 0; i < 3 && x->ui.mode != MD_NORMAL; i++) set(in, "shift", "1");
    if (x->ui.shift) set(in, "shift", "1");
    CHECK(x->ui.mode == MD_NORMAL && !x->ui.shift, "SHIFT leaves a function");

    /* ---- the state round-trip */
    {
        static char buf[1 << 20];
        static char buf2[1 << 20];
        int len = E->get_param(in, "state", buf, sizeof buf);
        CHECK(len > 1000 && len < (int)sizeof buf, "state length %d", len);
        in2 = E->create(NULL);
        set(in2, "state", buf);
        E->get_param(in2, "state", buf2, sizeof buf2);
        CHECK(!strcmp(buf, buf2), "the state round-trips");
        CHECK(!memcmp(&((Bl *)in2)->p, &x->p, sizeof(Project)), "the project round-trips");
        buf[40] = 'z';
        ((Bl *)in2)->p.bpm = 77;
        set(in2, "state", buf);
        CHECK(((Bl *)in2)->p.bpm == 77, "a damaged state is refused");
        E->destroy(in2);
    }

    /* ---- RESET, MIDI notes, the duck */
    set(in, "shift", "1");
    set(in, "mute", "1");
    CHECK(x->patSlot == 0 && x->p.pats[0].t[0].s[0].f == (ST_GATE | ST_ACC), "SHIFT + MUTE: the factory project");
    play(in, &ppq, 120, 0.5, 0, &rms, NULL);
    {
        uint8_t on[3] = {0x90, 36, 100}, off[3] = {0x80, 36, 0};
        float r1, r2;
        set(in, "trk1", "1");
        E->midi(in, on, 3);
        peak = play(in, &ppq, 120, 0.3, 0, &rms, NULL);
        CHECK(rms > 0.01, "a MIDI note plays the selected track (rms %f)", rms);
        r1 = (float)rms;
        /* the duck: on, a note on its channel (10) */
        set(in, "gd_on", "1");
        setf(in, "gd_depth", 1.f);
        setf(in, "gd_mode", 1);
        {
            uint8_t duck[3] = {0x99, 36, 100};
            E->midi(in, duck, 3);
        }
        play(in, &ppq, 120, 0.05, 0, &rms, NULL);
        r2 = (float)rms;
        CHECK(r2 < r1 * 0.3f, "a note on the duck's channel ducks (%f -> %f)", r1, r2);
        E->midi(in, off, 3);
        set(in, "gd_on", "1");
    }

    /* ---- the delay: HOLD, ADD, REVERSE, TAPE and DIGITAL, a tapped sync */
    setf(in, "dly_wet", 0.5f);
    setf(in, "dly_fb", 1.f);
    for (k = 0; k < 2; k++) {
        setf(in, "dly_mode", (float)k);
        set(in, "play", "1");
        play(in, &ppq, 120, 2.0, 0, &rms, NULL);
        set(in, "d_hold", "1");
        set(in, "d_add", "1");
        set(in, "d_rev", "1");
        setf(in, "dly_time", 0.1f + 0.4f * k);
        peak = play(in, &ppq, 120, 3.0, 0, &rms, NULL);
        CHECK(peak == peak && peak < 1.f && rms > 0.001, "the delay held, reversed (%s): peak %f rms %f", k ? "digital" : "tape", peak, rms);
        set(in, "d_hold", "1");
        set(in, "d_rev", "1");
        set(in, "d_tap", "1");
        play(in, &ppq, 120, 0.3, 0, &rms, NULL);
        set(in, "d_tap", "1");
        CHECK(x->p.dlySync == 2 && x->p.dlyTap > 0.25f && x->p.dlyTap < 0.35f, "taps set the delay's period (%f)", x->p.dlyTap);
        set(in, "stop", "1");
    }
    setf(in, "dly_fb", 1.f);
    peak = play(in, &ppq, 120, 20.0, 0, &rms, NULL);
    CHECK(peak < 1.f, "self-oscillating feedback stays bounded (%f)", peak);

    E->destroy(in);
    printf(fails ? "engine_test: %d FAILED\n" : "engine_test: PASSED\n", fails);
    return fails != 0;
}
