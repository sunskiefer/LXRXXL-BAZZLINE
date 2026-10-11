#!/usr/bin/env python3
"""LXRXXL BAZZLINE's two pages: where every control of the artwork sits on MPC's plugin screen.

    python3 tools/panel.py --write     (writes layout.conf from this file)
    python3 tools/panel.py --check     (fails when layout.conf is stale)

The pages are the artwork in art/panel/ by Suns Kiefer (1000 x 625 each):
    front_bg.png / modules_bg.png    the two panels, labels and slots printed, no knobs or thumbs
    knob_big.png, knob_encoder.png   the knob (with its pointer) and the encoders' knob (without)
    btn_lit_big.png / _small.png     a lit key (the unlit keys are printed on the panel)
    thumb_v.png / thumb_h.png        a slider's and a switch's thumb
    knob_ring_ref.png                the knob with its white segmented ring: the rings are cut from it
The front_mockup.png / modules_mockup.png pictures show where the knobs go; the positions below were measured on them
(template matching against the knob pictures, then registered to the backgrounds).

Every position here is in the artwork's pixels (x 0..1000, y 0..625); page() puts them on MPC's 1280 x 628 plugin
area: the panel at MPC's full height, centred, the sides filled with the panel's finish (tools/panel_art.py).
layout.conf only carries the two tabs and their Q-Links for mpc-vst-plugins; tools/panel_art.py then builds every
control of both pages from the artwork. Coordinates in layout.conf are Force Shadow pixels (y 86..714)."""
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ART = os.path.join(ROOT, "art", "panel")
Y0 = 86
W, H = 1280, 628
ART_W, ART_H = 1000, 625
K = H / ART_H                         # 1.0048: the panel at MPC's height, its shape kept
X0 = round((W - ART_W * K) / 2)       # 138: centred


def page(x, y):
    """artwork pixels -> page pixels (0..1280, 0..628)"""
    return X0 + x * K, y * K


TABS = ["PANEL", "MODULES"]

# ---------------------------------------------------------------- knobs: key -> (tab, cx, cy, size, picture, ring)
# size: the knob picture's diameter in artwork pixels (as placed in the mockups); ring: the white segmented ring
# around it (the delay's TIME has its scale printed on the panel instead)
KNOBS = {
    0: [("drive", 98.5, 226.5, 130, "big", 1), ("cutoff", 896, 226, 130, "big", 1),
        ("detune", 244, 256.5, 98, "big", 1), ("vca_env", 373, 256, 86, "big", 1),
        ("vcf_env", 625, 257, 88, "big", 1), ("resonance", 753, 257, 98, "big", 1),
        ("fm_lvl", 62, 388, 98, "big", 1), ("sub_lvl", 191, 385, 98, "big", 1),
        ("noise_lvl", 311, 389, 98, "big", 1), ("lfo_rate", 682, 385, 98, "big", 1),
        ("lfo_lvl", 802, 383, 98, "big", 1), ("env_cv", 931, 385, 98, "big", 1),
        ("sel", 432, 424, 106, "encoder", 1), ("note", 563, 425, 106, "encoder", 1),
        ("dly_wet", 85, 100, 64, "big", 1), ("dly_time", 242, 120, 128, "big", 0),
        ("gd_rel", 712, 142, 72, "big", 1)],
    1: [("dly_time", 145.5, 519.5, 130, "big", 0), ("dly_in", 272, 467, 50, "big", 1),
        ("dly_fb", 344, 468, 50, "big", 1), ("dly_wet", 429, 452, 64, "big", 1),
        ("gd_depth", 621, 518, 72, "big", 1), ("gd_hold", 722, 517, 72, "big", 1),
        ("gd_atk", 818, 517, 72, "big", 1), ("gd_rel", 918, 517, 72, "big", 1),
        ("acc_amt", 419, 88, 52, "big", 1), ("acc_sweep", 419, 161, 52, "big", 1),
        ("slide_time", 490, 125.5, 50, "big", 1), ("keytrack", 341, 382, 50, "big", 1),
        ("sw_rate", 494, 354, 48, "big", 1), ("detune", 577, 380, 50, "big", 1),
        ("sw_mix", 657, 356, 50, "big", 1)],
}

# ---------------------------------------------------------------- sliders (MODULES): key -> channel (x, y0, y1)
# the dark channel printed on the panel; the thumb runs along it, the top is the maximum (an option list: the first
# option at the top)
VSLIDERS = [("osc_mode", 60, 48, 197), ("fm_ratio", 118, 48, 197), ("wave_pos", 175, 48, 197),
            ("drive_pos", 246, 48, 197), ("curve", 305, 48, 197), ("tone", 360, 48, 197),
            ("vcf_atk", 566, 44, 193), ("vcf_dec", 624, 44, 193), ("vca_atk", 681, 44, 193),
            ("vca_env", 738, 44, 193), ("env_cv", 797, 44, 193), ("pitch_dec", 854, 44, 193),
            ("level", 912, 44, 193), ("pan", 970, 44, 193)]
# the option sliders under the selector bars: key -> channel (x0, y, x1); the first option at the left
HSLIDERS = [("drive_pos", 86, 324.5, 151), ("curve", 74, 384, 169), ("fmodel", 310, 334.5, 375),
            ("sw_mode", 546, 333.5, 611), ("mod_wave", 749, 321, 910), ("mod_dest", 754, 392, 915)]
THUMB_V = (9, 17)       # the thumb on every slider (as in the mockups)
THUMB_SW = (8, 16)      # the front panel's toggle switches' thumb

# ---------------------------------------------------------------- selector bars (MODULES): key -> (x, y, w, h, n)
# the printed bar of options: the selected one is a white box (the artwork shows the first selected)
BARS = [("drive_pos", 25, 290, 182, 18, 2), ("curve", 28, 345, 183, 19, 3), ("fmodel", 249, 290, 182, 18, 2),
        ("sw_mode", 485, 289, 182, 18, 2), ("mod_wave", 721, 281, 213, 21, 5), ("mod_dest", 723, 354, 214, 21, 4)]
# option labels shown as a white box when selected: key -> [(x, y, w, h) per option]
LABEL_OPTS = [("osc_mode", [(5, 36, 33, 16), (6, 108, 26, 14), (2, 179, 39, 14)]),
              ("mod_sync", [(941, 292, 39, 14), (938, 391, 47, 12)])]
# the FREE / TEMPO switch: a horizontal thumb in a vertical slot
SYNC_SLOT = (961, 317, 381)       # x, y0, y1
THUMB_SYNC = (17, 9)

# ---------------------------------------------------------------- two- and three-way switches: key -> zones
# (tab, key, slot thumb positions [(x, y)], tap zones [(x, y, w, h)] per option (the slot with its label), orientation)
SWITCHES = [
    (0, "wave", [(442.5, 238), (442.5, 253), (442.5, 268)],
     [(430, 231, 66, 15), (430, 246, 66, 15), (430, 261, 66, 15)], "v"),
    (0, "fmode", [(526, 245), (526, 261)], [(514, 231, 52, 22), (514, 253, 52, 22)], "v"),
    (0, "gd_mode", [(849.5, 133.5), (903.5, 133.5)], [(764, 118, 112, 28), (876, 118, 112, 28)], "h"),
    (1, "gd_mode", [(842.5, 450.5), (896.5, 450.5)], [(756, 436, 113, 26), (869, 436, 113, 26)], "h"),
    (1, "dly_mode", [(292.5, 605.5), (346.5, 605.5)], [(218, 592, 102, 22), (320, 592, 122, 22)], "h"),
]

# ---------------------------------------------------------------- keys: key -> (tab, cx, cy, diameter of the glass)
KEYS = {}
for i, x in enumerate([385, 447, 507, 567, 630, 691, 752, 813]):
    KEYS["trk%d" % (i + 1)] = (0, x, 52, 45)
for k, x, y in [("link", 899, 51), ("mute", 960, 51), ("shift", 42, 471), ("play", 103, 471), ("stop", 167, 472),
                ("rec", 226, 472), ("barl", 712, 474), ("barr", 774, 474), ("octd", 835, 475), ("octu", 897, 475),
                ("tap", 960, 476)]:
    KEYS[k] = (0, x, y, 45)
for i, x in enumerate([39, 101, 164, 225, 287, 350, 411, 472, 534, 595, 658, 719, 781, 843, 905, 968]):
    KEYS["s%d" % (i + 1)] = (0, x, 545, 57)
for k, x, y in [("d_add", 270.5, 539.5), ("d_hold", 336.5, 540), ("d_rev", 410.5, 540), ("d_tap", 490.5, 539.5),
                ("gd_on", 886.5, 600.5)]:
    KEYS[k] = (1, x, y, 36)   # the coloured keys of the delay and the duck: printed lit, unlit made from them
# the MODULES page's tap areas: the TRACK display (the next track) and the MIDI CH legend (the next channel)
TAPS = {"trk_next": (1, 388, 2, 156, 33), "ch_next": (1, 560, 586, 100, 37)}

# ---------------------------------------------------------------- displays
DISPLAY_CELLS = [(427, 297), (480, 297), (533, 297)]   # the three characters' top-left (40 x 54, dot included)
CELL_W, CELL_H = 40, 54
DISPLAY_BOX = (420, 289, 160, 70)                       # the window repainted under them
TRACK_WORD = (392, 4, 110, 29)                          # "TRACK", in the display's font (MODULES)
TRACK_DIGIT = (520, 4, 20, 29)                          # the selected track
CH_CELL = (662, 580, 38, 44)                            # the duck's MIDI channel

# ---------------------------------------------------------------- Q-Links: one tab, two banks of 8 (Force knobs)
QLINKS = {
    "PANEL": ["cutoff", "resonance", "vcf_env", "vca_env", "drive", "detune", "dly_wet", "dly_time",
              "fm_lvl", "sub_lvl", "noise_lvl", "lfo_rate", "lfo_lvl", "env_cv", "gd_rel", "note"],
    "MODULES": ["fm_ratio", "wave_pos", "tone", "acc_amt", "acc_sweep", "slide_time", "keytrack", "sw_rate",
                "sw_mix", "vcf_atk", "vcf_dec", "vca_atk", "pitch_dec", "level", "dly_fb", "gd_depth"],
}


def layout_lines():
    """layout.conf: each tab with a knob per Q-Link control (panel_art.py replaces every control) and its Q-Links"""
    L = ["# LXRXXL BAZZLINE -- page layout (mpc-vst-plugins layout.conf syntax: tools/shadow_skin.py).",
         "# GENERATED by tools/panel.py: edit that, then run `python3 tools/panel.py --write`. tools/panel_art.py then",
         "# builds every control of both pages from the artwork (art/panel/).",
         "theme_bg=0e0e0e", "theme_panel=0e0e0e", "theme_ink=ececec", "theme_ink_dim=cfcfcf", "theme_accent=fb9013",
         "theme_knob_ring=fb9013", "theme_seg_active=fb9013", ""]
    for t, name in enumerate(TABS):
        L.append("[tab %s]" % name)
        placed = {k[0]: k for k in KNOBS[t]}
        sliders = {s[0]: s for s in VSLIDERS}
        for i, key in enumerate(QLINKS[name]):
            if key in placed:
                _, x, y, size, *_ = placed[key]
            elif key in sliders:
                _, x, y0_, y1_ = sliders[key]
                y = (y0_ + y1_) / 2
            else:
                x, y = 40 + 60 * i, 300
            cx, cy = page(x, y)
            L.append('knob cx=%d cy=%d r=20 label="" key=%s ns=0 bw=40' % (round(cx), round(cy + Y0), key))
        L.append('qlinks "%s" = %s' % (name, ",".join(QLINKS[name])))
        L.append("")
    return L


if __name__ == "__main__":
    text = "\n".join(layout_lines()) + "\n"
    if "--write" in sys.argv:
        open(os.path.join(ROOT, "layout.conf"), "w").write(text)
        print("layout.conf written")
    elif "--check" in sys.argv:
        if open(os.path.join(ROOT, "layout.conf")).read() != text:
            raise SystemExit("layout.conf is stale: run python3 tools/panel.py --write")
    else:
        sys.stdout.write(text)
