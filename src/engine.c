/* LXRXXL BAZZLINE -- the plugin engine (mpc-vst-plugins wrapper/engine.h): parameters, MIDI and audio for one instance.
 *
 * The knobs and switches of both pages are the selected track's sound (as on the DB-01, the panel edits the track you
 * pick), the delay's and the gate/duck's. The keys are text parameters whose "<key>_on" is their LED (the wrapper polls
 * those every 10 ms). The display is six read-only pictures (three characters, three dots) the engine moves; the
 * wrapper reports them with the display_rev poll (HAS_DISPLAY_REV). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "engine.h"
#include "bl.h"
#include "param_ids.h"
#include "param_map.h"

typedef struct {
    Bl x;
    char shown[4];
    uint8_t shownDots;
} Inst;

static void *create(const char *dataDir)
{
    Inst *in = calloc(1, sizeof *in);
    if (!in) return NULL;
    if (dataDir) snprintf(in->x.dataDir, sizeof in->x.dataDir, "%s", dataDir);
    bl_init(&in->x);
    if (!in->x.fx.buf[0] || !in->x.fx.buf[1]) { fx_free(&in->x.fx); free(in); return NULL; }
    return in;
}

static void destroy(void *p)
{
    Inst *in = p;
    fx_free(&in->x.fx);
    free(in);
}

static void midi(void *p, const uint8_t *m, int len)
{
    Inst *in = p;
    const uint8_t st = m[0] & 0xF0, ch = m[0] & 0x0F;
    if (len < 3) return;
    if (st == 0x90) ui_midiNote(&in->x, ch, m[1] & 127, m[2] & 127);
    else if (st == 0x80) ui_midiNote(&in->x, ch, m[1] & 127, 0);
    else if (st == 0xB0 && (m[1] == 120 || m[1] == 123)) {   /* all sound / notes off */
        int t;
        for (t = 0; t < NUM_TRACKS; t++) { voice_noteOff(&in->x.v[t]); in->x.ui.held[t] = 0; }
    }
}

/* the parameter a key names; *on: the key asked for its LED ("<key>_on") */
static int key_index(const char *k, int *on)
{
    int i;
    size_t n = strlen(k);
    *on = 0;
    for (i = 0; i < P_COUNT; i++) if (!strcmp(k, PARAM_KEYS[i])) return i;
    if (n > 3 && !strcmp(k + n - 3, "_on")) {
        for (i = 0; i < P_COUNT; i++)
            if (strlen(PARAM_KEYS[i]) == n - 3 && !strncmp(k, PARAM_KEYS[i], n - 3)) { *on = 1; return i; }
    }
    return -1;
}

static float read_value(const ParamMap *m, const char *v)
{
    const float f = (float)atof(v);
    if (m->nopts) {
        int i = atoi(v);
        return (float)(i < 0 ? 0 : i >= m->nopts ? m->nopts - 1 : i);
    }
    return clampf(f, 0, 1);
}

static void set_param(void *p, const char *k, const char *v)
{
    Inst *in = p;
    Bl *x = &in->x;
    int i, on;
    if (!strcmp(k, "state")) { store_deserialize(x, v); return; }
    if (!strcmp(k, "host_time")) {
        int playing = 0;
        double ppq = 0, tempo = 0;
        if (sscanf(v, "%d %lf %lf", &playing, &ppq, &tempo) == 3) seq_hostTime(x, playing, ppq, tempo);
        return;
    }
    i = key_index(k, &on);
    if (i < 0 || on) return;
    {
        const ParamMap *m = &PARAM_MAP[i];
        switch (m->kind) {
        case PK_SOUND:
            x->p.snd[x->ui.track].v[m->id] = read_value(m, v);
            ui_soundTouched(x, m->id);
            break;
        case PK_GLOBAL:
            x->p.g[m->id] = read_value(m, v);
            ui_globalTouched(x, m->id);
            break;
        case PK_UI:
            if (m->id == UI_TRACK) ui_press(x, B_TRK1 + (int)read_value(m, v));
            else ui_knob(x, m->id == UI_SEL ? 0 : 1, (float)atof(v));
            break;
        case PK_BUTTON:
            /* a tap: whatever value the host sends is a press (the key's picture is its LED, which the host toggled on
             * its own: report the real LED again a moment after the tap so the picture follows it) */
            ui_press(x, m->id);
            x->ui.pressFlip[m->id] = x->clock + 600;
            break;
        default: break;   /* the display's pictures: read-only */
        }
    }
    x->displayRev++;
}

static int put(char *buf, int n, const char *s)
{
    if (n <= 0) return 0;
    snprintf(buf, (size_t)n, "%s", s);
    return (int)strlen(s) > 0 ? (int)strlen(s) : 0;
}

static int get_param(void *p, const char *k, char *buf, int n)
{
    Inst *in = p;
    Bl *x = &in->x;
    char t[64];
    int i, on;
    if (!strcmp(k, "state")) {
        int len = 0, r = 0;
        char *s = store_serialize(x, &len);
        if (s) {
            if (len < n) memcpy(buf, s, (size_t)len + 1);
            r = len + 1;   /* the wrapper grows its buffer when this is larger than what it gave */
            free(s);
        }
        return r;
    }
    if (!strcmp(k, "display_rev")) { snprintf(t, sizeof t, "%u", (unsigned)x->displayRev); return put(buf, n, t); }
    i = key_index(k, &on);
    if (i < 0) return 0;
    {
        const ParamMap *m = &PARAM_MAP[i];
        switch (m->kind) {
        case PK_SOUND: {
            const float v = x->p.snd[x->ui.track].v[m->id];
            if (on) return 0;
            if (m->nopts) snprintf(t, sizeof t, "%d", (int)v); else snprintf(t, sizeof t, "%.5f", v);
            return put(buf, n, t);
        }
        case PK_GLOBAL: {
            const float v = x->p.g[m->id];
            if (on) return 0;
            if (m->nopts) snprintf(t, sizeof t, "%d", (int)v); else snprintf(t, sizeof t, "%.5f", v);
            return put(buf, n, t);
        }
        case PK_UI:
            if (on) return 0;
            if (m->id == UI_TRACK) snprintf(t, sizeof t, "%d", x->ui.track);
            else snprintf(t, sizeof t, "%.5f", ui_knobGet(x, m->id == UI_SEL ? 0 : 1));
            return put(buf, n, t);
        case PK_SHOW: {
            char d[4];
            uint8_t dots;
            if (on) return 0;
            ui_display(x, d, &dots);
            if (m->id <= SH_DISP3) {
                const char *g = strchr(GLYPHS, d[m->id - SH_DISP1]);
                snprintf(t, sizeof t, "%d", g && d[m->id - SH_DISP1] ? (int)(g - GLYPHS) : 0);
            } else snprintf(t, sizeof t, "%d", (dots >> (m->id - SH_DOT1)) & 1);
            return put(buf, n, t);
        }
        case PK_BUTTON:
            if (on) {
                int led = ui_led(x, m->id);
                if (x->ui.pressFlip[m->id] && (int32_t)(x->ui.pressFlip[m->id] - x->clock) > 0) led = !led;
                else x->ui.pressFlip[m->id] = 0;
                return put(buf, n, led ? "1" : "0");
            }
            return put(buf, n, " ");
        default: return 0;
        }
    }
}

static void render(void *p, int16_t *out, int frames)
{
    Inst *in = p;
    float L[128], R[128];
    int i;
    char d[4];
    uint8_t dots;
    if (frames > 128) frames = 128;
    bl_render(&in->x, L, R, frames);
    /* the display changed by itself (a message ran out, the tempo moved): report it */
    ui_display(&in->x, d, &dots);
    if (memcmp(d, in->shown, 3) || dots != in->shownDots) {
        memcpy(in->shown, d, 4);
        in->shownDots = dots;
        in->x.displayRev++;
    }
    for (i = 0; i < frames; i++) {
        const float l = L[i] * 32767.f, r = R[i] * 32767.f;
        out[2 * i] = (int16_t)(l > 32767.f ? 32767 : l < -32768.f ? -32768 : (int)l);
        out[2 * i + 1] = (int16_t)(r > 32767.f ? 32767 : r < -32768.f ? -32768 : (int)r);
    }
}

static const mpc_engine_t ENGINE = {create, destroy, midi, set_param, get_param, render, NULL};
const mpc_engine_t *mpc_engine(void) { return &ENGINE; }
