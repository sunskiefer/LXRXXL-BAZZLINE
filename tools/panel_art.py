#!/usr/bin/env python3
"""Build both pages of LXRXXL BAZZLINE's skin from the artwork (art/panel/), into the skin mpc-vst-plugins made from
layout.conf (which only gave the two tabs and their Q-Links).

    python3 tools/panel_art.py "<skin dir>/Plugin Skins"

Geometry: tools/panel.py. For each page:
  - the background: the panel at MPC's full height, centred, the sides in its finish; a white segmented ring printed
    around each knob (as in the mockups); on the front panel the display window cleared for the live characters;
  - knobs: filmstrips of the knob picture turning (-150..+150 degrees) over the panel;
  - sliders (MODULES): filmstrips of the thumb along the printed channel;
  - option controls (the selector bars with the slider under them, OSC MODE, FREE/TEMPO, the toggle switches): a radio
    group of pictures, one per option: tapping an option picks it, the selected one shows its thumb / white box;
  - keys: two pictures each, unlit (the panel) and lit (the lit key in the display's orange; the delay's and the
    duck's coloured keys lit as printed, unlit in grey glass);
  - the display (front), the TRACK number and the duck's MIDI channel (MODULES): one picture per character, shown while
    its parameter is at that character (MPC's IndexedEnabling), in DSEG14 Classic (OFL) in the display's orange with
    its unlit segments and a glow.
Every control's definition in TUI.json is written here; the generated ones are removed."""
import json
import math
import os
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import panel as P  # noqa: E402
import gen_params  # noqa: E402

FONT = os.path.join(P.ROOT, "art", "fonts", "DSEG14Classic-Italic.ttf")
LIT = (251, 144, 19)            # the display's orange (front_bg.png)
GHOST = (54, 31, 7)             # its unlit segments
WINDOW = (8, 6, 4)              # the display window
KNOB_SPAN = 150                 # degrees each side of 12 o'clock
MAX_KNOB_STRIP = 16000          # px: knob strips over 16384 px drift as they turn (mpc-vst-plugins docs/NOTES.md)
KNOB_FRAMES = 101               # 3 degrees a frame
MAX_STRIP = 12288               # px: slider strips
SS = 3


def art(name):
    return Image.open(os.path.join(P.ART, name)).convert("RGBA")


def R(v):
    return int(round(v))


def prect(x, y, w, h):
    """artwork rect -> page rect (integers)"""
    x0, y0 = P.page(x, y)
    x1, y1 = P.page(x + w, y + h)
    return R(x0), R(y0), R(x1) - R(x0), R(y1) - R(y0)


# ------------------------------------------------------------------------------------------------ drawing helpers
def ring_layer(size_px):
    """the white segmented ring of the mockups: 10 segments over 300 degrees, at 0.505..0.57 of the knob's size"""
    S = size_px * 1.25
    n = int(math.ceil(S)) * 4
    im = Image.new("L", (n, n), 0)
    d = ImageDraw.Draw(im)
    c = n / 2
    r0, r1 = 0.505 * size_px * 4, 0.57 * size_px * 4
    gap = 6.0
    seg = (300.0 - 9 * gap) / 10
    for i in range(10):
        a0 = -150 + i * (seg + gap)
        a1 = a0 + seg
        # PIL angles: 0 = 3 o'clock, clockwise; ours: 0 = 12 o'clock, clockwise
        d.arc([c - (r0 + r1) / 2, c - (r0 + r1) / 2, c + (r0 + r1) / 2, c + (r0 + r1) / 2], a0 - 90, a1 - 90,
              fill=255, width=max(1, int(round(r1 - r0))))
    im = im.resize((n // 4, n // 4), Image.LANCZOS)
    rng = np.random.default_rng(3)
    a = np.asarray(im, np.float32) / 255.0
    tex = 215 + rng.normal(0, 10, a.shape)          # the printed white's grain
    rgb = np.clip(np.stack([tex, tex, tex - 4], -1), 0, 255)
    out = np.concatenate([rgb, (a * 255)[..., None]], -1).astype(np.uint8)
    return Image.fromarray(out, "RGBA")


def side_fill(panel):
    """the page's sides: the panel's dark finish (its edge colour, a little grain), the top strip carried on"""
    pa = np.asarray(panel.convert("RGB"), np.float32)
    edge = np.median(np.concatenate([pa[:, 2:12], pa[:, -12:-2]], 1), axis=1)   # per row
    edge[30:] = np.minimum(edge[30:], np.median(edge[30:], axis=0))              # below the top strip: the plain finish
    rng = np.random.default_rng(11)
    side = edge[:, None, :] + rng.normal(0, 2.0, (pa.shape[0], 8, 1))
    return np.clip(side, 0, 255)


def make_background(tab):
    bg_art = art("front_bg.png" if tab == 0 else "modules_bg.png")
    # rings around the knobs, at the artwork's scale
    for key, cx, cy, size, img, ring in P.KNOBS[tab]:
        if ring:
            rl = ring_layer(size)
            bg_art.alpha_composite(rl, (R(cx - rl.size[0] / 2), R(cy - rl.size[1] / 2)))
    if tab == 0:
        # the display window: cleared (the live characters are pictures over it)
        x, y, w, h = P.DISPLAY_BOX
        a = np.asarray(bg_art).copy()
        sub = a[y:y + h, x:x + w, :3].astype(np.int32)
        seg = (sub[..., 0] > 40) & (sub[..., 0] > sub[..., 2] * 2)   # the printed segments (lit and ghost)
        seg = np.asarray(Image.fromarray((seg * 255).astype(np.uint8)).filter(ImageFilter.MaxFilter(5))) > 0
        sub[seg] = WINDOW
        a[y:y + h, x:x + w, :3] = sub
        bg_art = Image.fromarray(a, "RGBA")
        for cx, cy in P.DISPLAY_CELLS:   # the unlit segments under each character
            bg_art.alpha_composite(glyph_cell("~", ghost_only=True), (cx - 3, cy - 4))
    else:
        # "TRACK" in the display's font, lit, in a display window (the track number beside it is a picture);
        # the duck's MIDI channel's window
        x, y, w, h = P.TRACK_WORD
        bg_art.alpha_composite(word_image("TRACK", w, h), (x, y))
        for x, y, w, h in (P.TRACK_DIGIT, P.CH_CELL):
            ImageDraw.Draw(bg_art).rectangle([x, y, x + w - 1, y + h - 1], fill=WINDOW + (255,))
    scaled = bg_art.resize((R(P.ART_W * P.K), P.H), Image.LANCZOS)
    page = Image.new("RGBA", (P.W, P.H), (14, 14, 14, 255))
    side = side_fill(scaled)
    left = Image.fromarray(np.repeat(side[:, :1], P.X0, axis=1).astype(np.uint8), "RGB").convert("RGBA")
    right_w = P.W - P.X0 - scaled.size[0]
    right = Image.fromarray(np.repeat(side[:, -1:], right_w, axis=1).astype(np.uint8), "RGB").convert("RGBA")
    page.paste(left, (0, 0))
    page.paste(right, (P.X0 + scaled.size[0], 0))
    page.alpha_composite(scaled, (P.X0, 0))
    return page


def crop(bg, rect):
    x, y, w, h = rect
    return bg.crop((x, y, x + w, y + h)).convert("RGBA")


# ------------------------------------------------------------------------------------------------ the display font
_font_cache = {}


def font_px(px):
    if px not in _font_cache:
        _font_cache[px] = ImageFont.truetype(FONT, px)
    return _font_cache[px]


def fit_font(text, w, h):
    """the largest size whose '~' (all segments) fits h and text fits w"""
    px = 10
    for p in range(8, 200):
        f = font_px(p)
        b = f.getbbox("~" * max(1, len(text)))
        if b[3] - b[1] > h or b[2] - b[0] > w:
            break
        px = p
    return font_px(px)


def lit_text(text, w, h, f, ghost=True, glow=True, window=True):
    """text in the display's look, w x h: the window, its unlit segments, the lit ones and their glow"""
    layer = Image.new("RGBA", (w * SS, h * SS), WINDOW + (255,) if window else (0, 0, 0, 0))
    big = font_px(round(f.size * SS))
    gb = big.getbbox("~" * len(text))
    ox = (w * SS - (gb[2] - gb[0])) / 2 - gb[0]
    oy = (h * SS - (gb[3] - gb[1])) / 2 - gb[1]
    if ghost:
        ImageDraw.Draw(layer).text((ox, oy), "~" * len(text), font=big, fill=GHOST + (255,))
    lit = Image.new("RGBA", layer.size, (0, 0, 0, 0))
    ImageDraw.Draw(lit).text((ox, oy), text, font=big, fill=LIT + (255,))
    if glow:
        g = np.asarray(lit.filter(ImageFilter.GaussianBlur(3 * SS))).astype(np.float32)
        g[..., 3] *= 0.75
        layer.alpha_composite(Image.fromarray(g.astype(np.uint8), "RGBA"))
    layer.alpha_composite(lit)
    return layer.resize((w, h), Image.LANCZOS)


def cell_font():
    """the front display's size: the largest whose all-segments character with its dot fits a cell"""
    px = 10
    for p in range(8, 200):
        b = font_px(p).getbbox("~.")
        if b[3] - b[1] > P.CELL_H or b[2] - b[0] > P.CELL_W:
            break
        px = p
    return font_px(px)


def cell_origin(big):
    gb = big.getbbox("~.")
    return 3 * SS - gb[0], 4 * SS - gb[1] + (P.CELL_H * SS - (gb[3] - gb[1])) / 2


def glyph_cell(ch, ghost_only=False, ghost=True):
    """one character of the front display, artwork scale: (CELL_W + 6) x (CELL_H + 8), the cell at (3, 4)"""
    w, h = P.CELL_W + 6, P.CELL_H + 8
    layer = Image.new("RGBA", (w * SS, h * SS), (0, 0, 0, 0))
    big = font_px(round(cell_font().size * SS))
    ox, oy = cell_origin(big)
    if ghost:
        ImageDraw.Draw(layer).text((ox, oy), "~.", font=big, fill=GHOST + (255,))
    if not ghost_only and ch != " ":
        lit = Image.new("RGBA", layer.size, (0, 0, 0, 0))
        ImageDraw.Draw(lit).text((ox, oy), ch, font=big, fill=LIT + (255,))
        g = np.asarray(lit.filter(ImageFilter.GaussianBlur(3.5 * SS))).astype(np.float32)
        g[..., 3] *= 0.8
        layer.alpha_composite(Image.fromarray(g.astype(np.uint8), "RGBA"))
        layer.alpha_composite(lit)
    return layer.resize((w, h), Image.LANCZOS)


def dot_cell(on):
    """a character's dot alone (lit or unlit), with the rect it sits in (artwork scale, relative to the cell layer)"""
    w, h = P.CELL_W + 6, P.CELL_H + 8
    layer = Image.new("RGBA", (w * SS, h * SS), (0, 0, 0, 0))
    big = font_px(round(cell_font().size * SS))
    ox, oy = cell_origin(big)
    tmp = Image.new("RGBA", layer.size, (0, 0, 0, 0))
    ImageDraw.Draw(tmp).text((ox, oy), "8.", font=big, fill=(255, 255, 255, 255))
    only8 = Image.new("RGBA", layer.size, (0, 0, 0, 0))
    ImageDraw.Draw(only8).text((ox, oy), "8", font=big, fill=(255, 255, 255, 255))
    dmask = np.clip(np.asarray(tmp)[..., 3].astype(np.int32) - np.asarray(only8)[..., 3], 0, 255).astype(np.uint8)
    ys, xs = np.nonzero(dmask > 20)
    col = LIT if on else GHOST
    dot = np.zeros(layer.size[::-1] + (4,), np.uint8)
    dot[..., :3] = col
    dot[..., 3] = dmask
    dl = Image.fromarray(dot, "RGBA")
    if on:
        g = np.asarray(dl.filter(ImageFilter.GaussianBlur(3 * SS))).astype(np.float32)
        g[..., 3] *= 0.8
        layer.alpha_composite(Image.fromarray(g.astype(np.uint8), "RGBA"))
    layer.alpha_composite(dl)
    pad = 5 * SS
    box = (max(0, xs.min() - pad), max(0, ys.min() - pad), min(layer.size[0], xs.max() + pad), min(layer.size[1], ys.max() + pad))
    box = tuple(int(round(v / SS)) * SS for v in box)
    piece = layer.crop(box).resize(((box[2] - box[0]) // SS, (box[3] - box[1]) // SS), Image.LANCZOS)
    return piece, (box[0] // SS, box[1] // SS)


def word_image(text, w, h):
    f = fit_font(text, w, h)
    return lit_text(text, w, h, f, ghost=True)


# ------------------------------------------------------------------------------------------------ keys
def recolour(img, rgb):
    """a lit key's glass in another colour: its shading kept, the hue the display's"""
    a = np.asarray(img).astype(np.float32)
    lum = a[..., :3] @ np.array([0.30, 0.59, 0.11], np.float32)
    ref = np.percentile(lum[a[..., 3] > 200], 60) if (a[..., 3] > 200).any() else 128
    k = lum / max(ref, 1)
    out = np.clip(np.array(rgb, np.float32)[None, None, :] * k[..., None], 0, 255)
    hi = np.clip((k - 1.15) / 0.6, 0, 1)[..., None]       # the glass's highlight stays near white
    out = out * (1 - hi) + 255 * hi
    return Image.fromarray(np.concatenate([out, a[..., 3:]], -1).astype(np.uint8), "RGBA")


def grey_glass(img_rect_crop, cx, cy, r):
    """a coloured lit key as unlit grey glass (inside its circle)"""
    a = np.asarray(img_rect_crop).astype(np.float32)
    h, w = a.shape[:2]
    yy, xx = np.mgrid[0:h, 0:w]
    m = np.clip(r + 0.5 - np.hypot(xx - cx, yy - cy), 0, 1)[..., None]
    lum = a[..., :3] @ np.array([0.30, 0.59, 0.11], np.float32)
    inside = m[..., 0] > 0.5
    top = np.percentile(lum[inside], 92) if inside.any() else 200.0
    grey = np.clip(lum / max(top, 1) * 205, 0, 255)[..., None].repeat(3, -1)   # every colour to the same grey glass
    a[..., :3] = a[..., :3] * (1 - m) + grey * m
    return Image.fromarray(a.astype(np.uint8), "RGBA")


# ------------------------------------------------------------------------------------------------ TUI.json pieces
def bounds_s(x, y, w, h):
    return "%d %d %d %d" % (R(x), R(y), R(w), R(h))


def sub(name, typ, data, b, visible="Always"):
    return {"version": 2, "componentData": {"version": 1, "name": name, "type": typ, "data": data},
            "handle remapping": {"version": 1, "map": []},
            "bounds": {"version": 2, "acceptsHWFocus": "No", "showWhenDataModelInvalid": "Show", "whenVisible": visible,
                       "boundsType": "Absolute", "bounds": b, "additionalInvalidatingHandles": []}}


def focus(w, h):
    return sub("Focus", "Focus", {"version": 1, "backgroundColour": "00000000", "outlineColour": "00000000",
                                  "backgroundInset": 2.0, "outlineThickness": 0.0}, bounds_s(0, 0, w, h), "WhenFocussed")


def action(on, handler, extra=""):
    return {"version": 2, "onAction": on, "handler": handler, "handleName": "" if handler == "Show Overlay" else "Data",
            "additionalData": extra, "handle remapping": {"version": 1, "map": []}}


KNOB_ACTIONS = [action("Mouse Down", "Q-Link"), action("Double Click", "Show Overlay", "knob overlay"),
                action("Enter Pressed", "Show Overlay", "knob overlay")]
KEY_ACTIONS = [action("Mouse Down", "Q-Link"), action("Enter Pressed", "Toggle Switch")]
RADIO_ACTIONS = [action("Mouse Down", "Q-Link")]


def definition(key, actions, parts):
    return {"key": key, "value": {
        "version": 4, "actions": actions,
        "backgroundData": {"version": 1, "focussed": {"version": 1, "colour": "0", "image": ""},
                           "unfocussed": {"version": 1, "colour": "0", "image": ""}},
        "ignoreMousePresses": False, "disableCoarseDataWheel": False, "repeats": 1, "hideQLinkBounds": True,
        "componentsData": parts}}


def filmstrip(name, img, frames, w, h, drag="Vertical", x=0, y=0):
    return sub(name, "Knob", {"version": 5, "knobType": "FilmStrip", "filmStrip": img, "numFrames": frames,
                              "invert": False, "dragOrientation": drag, "handleName": "Data"}, bounds_s(x, y, w, h))


def button(on_img, off_img, bid, n, w, h):
    return sub("Button", "Button", {"version": 2, "onImage": on_img, "offImage": off_img, "buttonId": bid,
                                    "numButtonsInGroup": n, "handleName": "Data", "gestureBehaviour": "Instant"},
               bounds_s(0, 0, w, h))


def placed(dkey, name, index, rect, focus_on=True):
    x, y, w, h = rect
    return {"version": 2,
            "componentData": {"version": 1, "name": name, "type": dkey, "data": {"version": 1, "handleName": "Data"}},
            "handle remapping": {"version": 1, "map": [{"key": "Data", "value": "Parameter %d" % index}]},
            "bounds": {"version": 2, "acceptsHWFocus": "Yes" if focus_on else "No", "showWhenDataModelInvalid": "Hide",
                       "whenVisible": "Always", "boundsType": "Absolute", "bounds": bounds_s(x, y, w, h),
                       "additionalInvalidatingHandles": []}}


def picture(img, rect, index, option, count):
    """an image shown while parameter index is at option (of count): MPC's IndexedEnabling"""
    c = sub("Picture", "Image", {"version": 2, "imageType": "Regular", "colour": "0", "image": img}, bounds_s(*rect))
    c["bounds"]["additionalInvalidatingHandles"] = ["IndexedEnabling/%d/%d/Parameter %d" % (option, count, index)]
    return c


# ------------------------------------------------------------------------------------------------ the builder
class Skin:
    def __init__(self, skin, params):
        self.skin = skin
        self.index = {p["key"]: i for i, p in enumerate(params)}
        self.params = params
        self.defs = []
        self.used = set()
        self.saved = {}

    def save(self, img, name, rgb=True):
        """write a picture once (identical pictures share one file)"""
        data = img.convert("RGB" if rgb else "RGBA")
        h = hash(data.tobytes()) ^ hash(data.size)
        if h in self.saved:
            return self.saved[h]
        base, n = name[:-4], 2
        while name in self.used:   # a name already taken by another picture: a new one
            name = "%s_%d.png" % (base, n)
            n += 1
        data.save(os.path.join(self.skin, name))
        self.used.add(name)
        self.saved[h] = name
        return name

    def opts(self, key):
        return len(self.params[self.index[key]].get("options") or [])


def build_page(sk, tab):
    """(the page's background, its components)"""
    bg = make_background(tab)
    kids = []
    t = "f" if tab == 0 else "m"
    seen_focus = set()

    def place(dkey, name, key, rect):
        f = key not in seen_focus
        seen_focus.add(key)
        kids.append(placed(dkey, name, sk.index[key], rect, f))

    # ---- knobs: one transparent filmstrip per picture and size, shared by every knob of that size (the ring and the
    # panel are in the background, as with MPC's own generic knobs)
    for key, cx, cy, size, img, ring in P.KNOBS[tab]:
        s = R(size * P.K)
        pcx, pcy = P.page(cx, cy)
        x0, y0 = R(pcx - s / 2), R(pcy - s / 2)
        frames = min(KNOB_FRAMES, MAX_KNOB_STRIP // s)
        name = "bz_knob_%s_%d.png" % (img, s)
        if name not in sk.used:
            pic = art("knob_big.png" if img == "big" else "knob_encoder.png")
            big = pic.resize((s * SS, s * SS), Image.LANCZOS)
            strip = Image.new("RGBA", (s, s * frames), (0, 0, 0, 0))
            for k in range(frames):
                ang = -KNOB_SPAN + 2 * KNOB_SPAN * k / (frames - 1)
                strip.paste(big.rotate(-ang, resample=Image.BICUBIC).resize((s, s), Image.LANCZOS), (0, k * s))
            strip.save(os.path.join(sk.skin, name))
            sk.used.add(name)
        tb = s if key in ("sw_rate", "sw_mix") else max(s, 64)   # small knobs: a finger-sized touch box, room allowing
        dkey = "bzKnob_%s_%d_%d" % (img, s, tb)
        if not any(d["key"] == dkey for d in sk.defs):
            sk.defs.append(definition(dkey, KNOB_ACTIONS, [focus(tb, tb), filmstrip("Knob", name, frames - 1, s, s, "Vertical",
                                                                                     (tb - s) // 2, (tb - s) // 2)]))
        place(dkey, key, key, (x0 - (tb - s) // 2, y0 - (tb - s) // 2, tb, tb))

    # ---- sliders (MODULES): the thumb along the channel; the top is the maximum
    if tab == 1:
        thumb = art("thumb_v.png")
        tw, th = R(P.THUMB_V[0] * P.K), R(P.THUMB_V[1] * P.K)
        tpic = thumb.resize((tw, th), Image.LANCZOS)
        for key, x, y0, y1 in P.VSLIDERS:
            n = sk.opts(key)
            if n:
                continue   # option sliders are radio groups (below)
            px, py0 = P.page(x, y0)
            _, py1 = P.page(x, y1)
            w = tw + 14
            rx, ry, rh = R(px - w / 2), R(py0) - 2, R(py1 - py0) + 4
            under = crop(bg, (rx, ry, w, rh))
            frames = min(101, MAX_STRIP // rh)
            strip = Image.new("RGB", (w, rh * frames))
            for k in range(frames):
                v = k / (frames - 1)
                fr = under.copy()
                ty = 2 + R((1 - v) * (rh - 4 - th))
                fr.alpha_composite(tpic, ((w - tw) // 2, ty))
                strip.paste(fr.convert("RGB"), (0, k * rh))
            name = sk.save(strip, "bz_m_slider_%s.png" % key)
            tb = 46
            dkey = "bzSlider_%s" % key
            sk.defs.append(definition(dkey, KNOB_ACTIONS, [focus(tb, rh), filmstrip("Slider", name, frames, w, rh, "Vertical",
                                                                                     (tb - w) // 2, 0)]))
            place(dkey, key, key, (rx - (tb - w) // 2, ry, tb, rh))

    # ---- option controls: radio groups of pictures
    def radio(key, zones, draw_on, draw_off=None, label=None):
        """zones: page rects per option; draw_on(i, img, rect) draws option i selected onto a copy of its zone"""
        n = len(zones)
        for i, rect in enumerate(zones):
            off = crop(bg, rect)
            if draw_off:
                draw_off(i, off, rect)
            on = off.copy()
            draw_on(i, on, rect)
            on_n = sk.save(on, "bz_%s_opt_%s_%s_%d_on.png" % (t, key, label, i))
            off_n = sk.save(off, "bz_%s_opt_%s_%s_%d_off.png" % (t, key, label, i))
            dkey = "bzOpt_%s_%s_%d_%s" % (t, key, i, label or "")
            sk.defs.append(definition(dkey, RADIO_ACTIONS, [button(on_n, off_n, i, n, rect[2], rect[3])]))
            place(dkey, "%s %d" % (key, i), key, rect)

    def thumb_at(img, rect, pos, pic):
        px, py = P.page(*pos)
        img.alpha_composite(pic, (R(px - pic.size[0] / 2) - rect[0], R(py - pic.size[1] / 2) - rect[1]))

    tv = art("thumb_v.png")
    sw_pic = tv.resize((R(P.THUMB_SW[0] * P.K), R(P.THUMB_SW[1] * P.K)), Image.LANCZOS)
    sl_pic = tv.resize((R(P.THUMB_V[0] * P.K), R(P.THUMB_V[1] * P.K)), Image.LANCZOS)
    sync_pic = art("thumb_h.png").resize((R(P.THUMB_SYNC[0] * P.K), R(P.THUMB_SYNC[1] * P.K)), Image.LANCZOS)

    for st_tab, key, thumbs, zones, orient in P.SWITCHES:
        if st_tab != tab:
            continue
        pz = [prect(*z) for z in zones]
        radio(key, pz, lambda i, img, rect, th=thumbs: thumb_at(img, rect, th[i], sw_pic), label="sw")

    if tab == 1:
        bars = {b[0]: b for b in P.BARS}
        hs = {h[0]: h for h in P.HSLIDERS}
        for key, bx, by, bw, bh, n in P.BARS:
            hx0, hy, hx1 = hs[key][1:]
            # the column of each option: its part of the bar and the slider under it
            col_w = bw / n
            thumbs = [(hx0 + 4.5 + i * (hx1 - hx0 - 9) / (n - 1), hy) for i in range(n)]
            seg_rects = [(bx + i * col_w, by, col_w, bh) for i in range(n)]
            looks = bar_looks(art("modules_bg.png"), seg_rects)
            # the bar: tap an option (the selected one is the white box)
            radio(key, [prect(bx + i * col_w, by - 2, col_w, bh + 4) for i in range(n)],
                  lambda i, img, rect, seg_rects=seg_rects, looks=looks: paste_look(img, rect, seg_rects[i], looks[i][1]),
                  lambda i, img, rect, seg_rects=seg_rects, looks=looks: paste_look(img, rect, seg_rects[i], looks[i][0]),
                  label="bar")
            # the slider under it: a stretch of the channel per option, its thumb on the selected one
            sw = (hx1 - hx0 + 8) / n
            radio(key, [prect(hx0 - 4 + i * sw, hy - 10, sw, 20) for i in range(n)],
                  lambda i, img, rect, thumbs=thumbs: thumb_at(img, rect, thumbs[i], sl_pic), label="hs")
        # the same options on the top row's vertical sliders: DRIVE POS (2) and CURVE (3); OSC MODE with its labels
        lab = {k: v for k, v in P.LABEL_OPTS}
        for key, x, y0, y1 in P.VSLIDERS:
            n = sk.opts(key)
            if not n:
                continue
            step = (y1 - y0) / n
            thumbs = [(x, y0 + 8.5 + i * (y1 - y0 - 17) / (n - 1)) for i in range(n)]
            if key == "osc_mode":
                zones = [prect(0, y0 - 14, 72, step + 14)] + [prect(0, y0 + i * step, 72, step) for i in range(1, n)]
                looks = label_looks(art("modules_bg.png"), lab["osc_mode"])
            else:
                zones = [prect(x - 22, y0 + i * step, 44, step) for i in range(n)]
                looks = None

            def on_draw(i, img, rect, thumbs=thumbs, looks=looks, key=key):
                if looks:
                    paste_look(img, rect, lab[key][i], looks[i][1])
                thumb_at(img, rect, thumbs[i], sl_pic)

            def off_draw(i, img, rect, looks=looks, key=key):
                if looks:
                    paste_look(img, rect, lab[key][i], looks[i][0])

            radio(key, zones, on_draw, off_draw, label="v")
        # FREE / TEMPO
        sx, sy0, sy1 = P.SYNC_SLOT
        zones = [prect(938, 285, 54, 64), prect(938, 349, 54, 60)]
        thumbs = [(sx, sy0 + 6), (sx, sy1 - 6)]
        looks = label_looks(art("modules_bg.png"), lab["mod_sync"])
        radio("mod_sync", zones,
              lambda i, img, rect: (paste_look(img, rect, lab["mod_sync"][i], looks[i][1]), thumb_at(img, rect, thumbs[i], sync_pic)),
              lambda i, img, rect: paste_look(img, rect, lab["mod_sync"][i], looks[i][0]), label="sync")

    # ---- keys
    lit_big = recolour(art("btn_lit_big.png"), LIT)
    lit_small = recolour(art("btn_lit_small.png"), LIT)
    for key, (ktab, cx, cy, dia) in P.KEYS.items():
        if ktab != tab:
            continue
        pcx, pcy = P.page(cx, cy)
        box = R(max(dia + 10, 52) * P.K) if tab == 0 else R(46 * P.K)
        rect = (R(pcx - box / 2), R(pcy - box / 2), box, box)
        off = crop(bg, rect)
        if tab == 0:
            src = lit_big if dia > 50 else lit_small
            d = R(src.size[0] * P.K)
            g = src.resize((d, R(src.size[1] * P.K)), Image.LANCZOS)
            on = off.copy()
            on.alpha_composite(g, (R(pcx - g.size[0] / 2) - rect[0], R(pcy - g.size[1] / 2) - rect[1]))
        else:
            on = off
            off = grey_glass(off, pcx - rect[0], pcy - rect[1], dia / 2 * P.K + 1)
        on_n = sk.save(on, "bz_%s_key_%s_on.png" % (t, key))
        off_n = sk.save(off, "bz_%s_key_%s_off.png" % (t, key))
        dkey = "bzKey_%s" % key
        sk.defs.append(definition(dkey, KEY_ACTIONS, [focus(box, box), button(on_n, off_n, 1, 1, box, box)]))
        place(dkey, key, key, rect)

    # ---- the MODULES page's tap areas: TRACK (the next track), MIDI CH (the next channel)
    if tab == 1:
        for key, (ktab, x, y, w, h) in P.TAPS.items():
            rect = prect(x, y, w, h)
            off = crop(bg, rect)
            a = np.asarray(off).astype(np.float32)
            a[..., :3] = np.clip(a[..., :3] * 1.25 + 12, 0, 255)
            on = Image.fromarray(a.astype(np.uint8), "RGBA")
            on_n = sk.save(on, "bz_m_tap_%s_on.png" % key)
            off_n = sk.save(off, "bz_m_tap_%s_off.png" % key)
            dkey = "bzTap_%s" % key
            sk.defs.append(definition(dkey, KEY_ACTIONS, [focus(rect[2], rect[3]), button(on_n, off_n, 1, 1, rect[2], rect[3])]))
            place(dkey, key, key, rect)

    # ---- the characters
    if tab == 0:
        glyphs = gen_params.GLYPHS
        for ci, (cx, cy) in enumerate(P.DISPLAY_CELLS):
            rect = prect(cx - 3, cy - 4, P.CELL_W + 6, P.CELL_H + 8)
            under = crop(bg, rect)
            for gi, ch in enumerate(glyphs):
                cell = glyph_cell(ch, ghost=False).resize((rect[2], rect[3]), Image.LANCZOS)
                im = under.copy()
                im.alpha_composite(cell)
                name = sk.save(im, "bz_f_glyph_%d.png" % gi)
                kids.append(picture(name, rect, sk.index["disp%d" % (ci + 1)], gi, len(glyphs)))
        for ci, (cx, cy) in enumerate(P.DISPLAY_CELLS):
            piece, (ox, oy) = dot_cell(True)
            r = prect(cx - 3 + ox, cy - 4 + oy, piece.size[0], piece.size[1])
            im = crop(bg, r)
            im.alpha_composite(piece.resize((r[2], r[3]), Image.LANCZOS))
            name = sk.save(im, "bz_f_dot_on.png")
            kids.append(picture(name, r, sk.index["dot%d" % (ci + 1)], 1, 2))
    else:
        x, y, w, h = P.TRACK_DIGIT
        rect = prect(x, y, w, h)
        f = fit_font("8", w, h)
        for i in range(8):
            im = crop(bg, rect)
            im.alpha_composite(lit_text(str(i + 1), w, h, f).resize((rect[2], rect[3]), Image.LANCZOS))
            name = sk.save(im, "bz_m_track_%d.png" % i)
            kids.append(picture(name, rect, sk.index["track"], i, 8))
        x, y, w, h = P.CH_CELL
        rect = prect(x, y, w, h)
        f = fit_font("88", w, h)
        for i in range(16):
            txt = str(i + 1)
            im = crop(bg, rect)
            im.alpha_composite(lit_text(txt.rjust(2) if len(txt) < 2 else txt, w, h, f).resize((rect[2], rect[3]), Image.LANCZOS))
            name = sk.save(im, "bz_m_ch_%d.png" % i)
            kids.append(picture(name, rect, sk.index["gd_ch"], i, 16))
    return bg, kids


# ------------------------------------------------------------------------------------------------ selector looks
def _levels(a, rect):
    x, y, w, h = [R(v) for v in rect]
    sub_ = a[y:y + h, x:x + w, :3].astype(np.float32)
    lum = sub_ @ np.array([0.30, 0.59, 0.11], np.float32)
    return lum, np.percentile(lum, 5), np.percentile(lum, 97)


def bar_looks(src, rects):
    """(unselected, selected) pictures (artwork scale, H x W x 3) of each option of a printed selector, whose first option
    is printed selected (a white box, dark text) and the others not (light text on the dark): each missing look is the
    printed one with its two levels swapped"""
    a = np.asarray(src.convert("RGB"))
    lum0, s_text, s_box = _levels(a, rects[0])     # selected: box bright, text dark
    _, u_dark, u_text = _levels(a, rects[1])       # unselected: dark ground, text light
    looks = []
    for i, r in enumerate(rects):
        lum, lo, hi = _levels(a, r)
        x, y, w, h = [R(v) for v in r]
        printed = a[y:y + h, x:x + w, :3].astype(np.float32)
        if i == 0:
            t = np.clip((s_box - lum) / max(s_box - s_text, 1), 0, 1)          # 0 box, 1 text
            unsel = u_dark + t * (u_text - u_dark)
            looks.append((np.repeat(unsel[..., None], 3, -1), printed))
        else:
            t = np.clip((lum - lo) / max(hi - lo, 1), 0, 1)                    # 0 ground, 1 text
            sel = s_box + t * (s_text - s_box)
            looks.append((printed, np.repeat(sel[..., None], 3, -1)))
    return looks


def label_looks(src, rects):
    return bar_looks(src, rects)


def paste_look(img, zone_rect, art_rect, look):
    """put an artwork-scale look (H x W x 3 array) at its place inside a zone picture"""
    x, y, w, h = [R(v) for v in art_rect]
    pic = Image.fromarray(np.asarray(look).astype(np.uint8), "RGB").convert("RGBA")
    pr = prect(x, y, w, h)
    pic = pic.resize((pr[2], pr[3]), Image.LANCZOS)
    img.alpha_composite(pic, (pr[0] - zone_rect[0], pr[1] - zone_rect[1]))


# ------------------------------------------------------------------------------------------------ main
def main(skin):
    tui_path = os.path.join(skin, "TUI.json")
    tui = json.load(open(tui_path))
    cdefs = tui["pageData"]["componentDefinitions"]
    defs = cdefs["localComponentDefinitions"]
    params = json.load(open(os.path.join(P.ROOT, "params.json")))["params"]
    sk = Skin(skin, params)
    pages = {}
    for tab, name in enumerate(P.TABS):
        bg, kids = build_page(sk, tab)
        bgname = "bz_bg_%d.png" % tab
        bg.convert("RGB").save(os.path.join(skin, bgname))
        sk.used.add(bgname)
        pages[name] = [sub("Background", "Image", {"version": 2, "imageType": "Regular", "colour": "0", "image": bgname},
                           bounds_s(0, 0, P.W, P.H))] + kids
    keep = []
    for e in defs:
        tabname = e["key"].split("|")[0]
        if "|" in e["key"] and tabname in pages:
            e["value"]["componentsData"] = pages[tabname]
            e["value"]["backgroundData"] = {"version": 1, "focussed": {"version": 1, "colour": "ff0e0e0e", "image": ""},
                                            "unfocussed": {"version": 1, "colour": "ff0e0e0e", "image": ""}}
            keep.append(e)
    cdefs["localComponentDefinitions"] = keep + sk.defs
    json.dump(tui, open(tui_path, "w"), indent=1)
    for f in os.listdir(skin):
        if f.endswith(".png") and f not in sk.used:
            os.remove(os.path.join(skin, f))
    print("panel_art: %d definitions, %d pictures" % (len(sk.defs), len(sk.used)))


if __name__ == "__main__":
    main(sys.argv[1])
