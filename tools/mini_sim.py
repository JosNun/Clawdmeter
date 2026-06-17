#!/usr/bin/env python3
"""Offline previewer for the SSD1306 mini-creature rendering.

Parses firmware/src/splash_animations.h (the generated claudepix data) and
renders a creature exactly as the 1-bit panel would show it — including the
luminance threshold the SSD1306 driver applies — so we can iterate on the
look (outline masking, blink timing, walk path) without flashing hardware.

Outputs PNGs to /tmp:
  - one composited "contact sheet" of a walk sequence over a mock usage view,
    so the outline-vs-content masking is visible.

Usage:  python3 tools/mini_sim.py [anim_name]   (default: "dance bounce")
"""
import re, sys, os

HERE = os.path.dirname(os.path.abspath(__file__))
HDR = os.path.join(HERE, "..", "firmware", "src", "splash_animations.h")
GRID = 20

def lum565(c):
    r = ((c >> 11) & 0x1F) * 255 // 31
    g = ((c >> 5) & 0x3F) * 255 // 63
    b = (c & 0x1F) * 255 // 31
    return (r * 77 + g * 150 + b * 29) >> 8  # matches display.cpp rgb565_is_on

def load():
    src = open(HDR).read()
    pals, frames = {}, {}
    for n, body in re.findall(r'splash_(\w+)_palette\[\d+\]\s*=\s*\{([^}]*)\}', src):
        pals[n] = [int(v, 16) for v in re.findall(r'0x[0-9A-Fa-f]+', body)]
    for m in re.finditer(r'splash_(\w+)_frames\[(\d+)\]\[400\]\s*=\s*\{(.*?)\};', src, re.S):
        frames[m.group(1)] = [[int(x) for x in f.split(',') if x.strip()]
                              for f in re.findall(r'\{([0-9,\s]+)\}', m.group(3))]
    return pals, frames

# Render one frame to a 20x20 grid of states: 'on' (white px), 'off' (black px,
# incl. the outline halo), or None (transparent — content shows through).
def render_states(pal, frame, outline=True):
    def nonzero(gx, gy):
        return 0 <= gx < GRID and 0 <= gy < GRID and frame[gy * GRID + gx] != 0
    st = [[None] * GRID for _ in range(GRID)]
    for gy in range(GRID):
        for gx in range(GRID):
            code = frame[gy * GRID + gx]
            if code != 0:
                st[gy][gx] = 'on' if lum565(pal[code]) >= 96 else 'off'
            elif outline:
                # background cell: black outline if it touches the silhouette
                if any(nonzero(gx + dx, gy + dy)
                       for dx in (-1, 0, 1) for dy in (-1, 0, 1) if (dx or dy)):
                    st[gy][gx] = 'off'
    return st

def mock_usage_bg(W, H):
    """A fake usage view (white-on-black) to test occlusion/outline contrast."""
    from PIL import Image, ImageDraw
    img = Image.new('L', (W, H), 0)
    d = ImageDraw.Draw(img)
    # two rows of "stats": a label block + a long bar, like S/W rows
    for row_y in (2, 18):
        d.rectangle([2, row_y, 10, row_y + 10], fill=255)          # label glyph block
        d.rectangle([46, row_y + 2, 100, row_y + 9], outline=255)  # bar outline
        d.rectangle([46, row_y + 2, 80, row_y + 9], fill=255)      # bar fill
        d.rectangle([104, row_y, 118, row_y + 10], fill=255)       # reset block
    return img

def composite(bg, states, x, y):
    """Draw creature states onto an 'L' bg at (x,y). on=255, off=0, None=skip."""
    out = bg.copy()
    px = out.load()
    W, H = out.size
    for gy in range(GRID):
        for gx in range(GRID):
            s = states[gy][gx]
            if s is None:
                continue
            X, Y = x + gx, y + gy
            if 0 <= X < W and 0 <= Y < H:
                px[X, Y] = 255 if s == 'on' else 0
    return out

# Hopping-arc path: x sweeps edge->centre->edge; y bobs in arcs (|sin|) so the
# creature hops a few times instead of gliding flat. Returns [(x, y), ...].
import math
def stroll_path(W, base_y=6, arc=6, hops=2, n_in=6, n_out=6, n_pause=2):
    cx = (W - 20) // 2
    pts = []
    for i in range(n_in):          # hop in from the right edge to centre
        p = i / (n_in - 1)
        x = round(W + (cx - W) * p)
        y = round(base_y - arc * abs(math.sin(p * math.pi * hops)))
        pts.append((x, y))
    pts += [(cx, base_y)] * n_pause  # pause/blink at centre
    for i in range(n_out):         # hop out to the left edge
        p = i / (n_out - 1)
        x = round(cx + (-20 - cx) * p)
        y = round(base_y - arc * abs(math.sin(p * math.pi * hops)))
        pts.append((x, y))
    return pts

# ---- Playful "Claude's day" mode preview ----
# Mirrors ui.cpp: a dithered tide (bright waterline + 50% checker body) rising
# from the bottom to surface = H - H*session/100, a creature floating just above
# the waterline, and a dim "S## W##" readout top-left. Renders the true 1-bit
# look so we can tune brightness/visibility without flashing.
def playful_scene(pals, frames, session, weekly, frame_idx=0, phase=0, sun_r=3,
                  W=128, H=32, calm="idle blink", panic="dance bounce", panic_pct=80):
    from PIL import Image, ImageDraw
    img = Image.new('L', (W, H), 0)
    px = img.load()
    s = max(0, min(100, int(round(session))))
    w = max(0, min(100, int(round(weekly))))
    surface = H - s * H // 100
    # tide: rippling 1px waterline + 50% checker body (matches playful_paint_scene)
    for x in range(W):
        crest = surface - (1 if (x + phase) % 6 < 3 else 0)
        for y in range(crest, H):
            if y == crest or ((x + y) & 1) == 0:
                if 0 <= y < H:
                    px[x, y] = 255
    # sun on top: filled disc on the right, height tracks weekly (independent of tide)
    sun_cx = W - 11
    sun_cy = sun_r + 1 + (H - 2 * sun_r - 2) * w // 100
    for dy in range(-sun_r, sun_r + 1):
        for dx in range(-sun_r, sun_r + 1):
            if dx * dx + dy * dy <= sun_r * sun_r:
                if 0 <= sun_cx + dx < W and 0 <= sun_cy + dy < H:
                    px[sun_cx + dx, sun_cy + dy] = 255
    # creature floats just above the waterline, clamped on-screen
    key = (panic if s >= panic_pct else calm).replace(" ", "_")
    pal, frs = pals[key], frames[key]
    st = render_states(pal, frs[frame_idx % len(frs)], outline=True)
    cy = max(0, min(H - 20, surface - 18))
    img = composite(img, st, (W - 20) // 2, cy)
    # black-backed readout chip, top-left (approximate font; real one is styrene_12)
    d = ImageDraw.Draw(img)
    tag = f"S{s} W{w}"
    d.rectangle([0, 0, 6 * len(tag), 8], fill=0)
    d.text((1, 0), tag, fill=255)
    return img

def main_playful():
    from PIL import Image
    pals, frames = load()
    levels = [10, 40, 73, 95]
    W, H, SCALE = 128, 32, 6
    tiles = [playful_scene(pals, frames, lv, lv // 2).resize(
                (W * SCALE, H * SCALE), Image.NEAREST).convert('RGB') for lv in levels]
    sheet = Image.new('RGB', (W * SCALE, H * SCALE * len(tiles) + 6 * (len(tiles) - 1)), (40, 40, 40))
    y = 0
    for t in tiles:
        sheet.paste(t, (0, y)); y += H * SCALE + 6
    out = "/tmp/playful_sim.png"
    sheet.save(out)
    print("wrote %s (session levels %s)" % (out, levels))

def main():
    from PIL import Image
    if len(sys.argv) > 1 and sys.argv[1] == "playful":
        main_playful(); return
    name = sys.argv[1] if len(sys.argv) > 1 else "dance bounce"
    key = name.replace(" ", "_")
    arc   = int(sys.argv[2]) if len(sys.argv) > 2 else 6
    hops  = int(sys.argv[3]) if len(sys.argv) > 3 else 2
    basey = int(sys.argv[4]) if len(sys.argv) > 4 else 6
    pals, frames = load()
    if key not in frames:
        print("unknown anim:", name, "->", list(frames)); return
    pal, frs = pals[key], frames[key]
    W, H, SCALE = 128, 32, 4

    path = stroll_path(W, base_y=basey, arc=arc, hops=hops)
    bg = mock_usage_bg(W, H)
    tiles = []
    for i, (x, y) in enumerate(path):
        st = render_states(pal, frs[i % len(frs)], outline=True)
        comp = composite(bg, st, x, y).resize((W * SCALE, H * SCALE), Image.NEAREST)
        tiles.append(comp.convert('RGB'))
    sheet = Image.new('RGB', (W * SCALE, H * SCALE * len(tiles) + 6 * (len(tiles) - 1)), (40, 40, 40))
    yoff = 0
    for t in tiles:
        sheet.paste(t, (0, yoff)); yoff += H * SCALE + 6
    out = "/tmp/mini_sim_%s.png" % key
    sheet.save(out)
    print("wrote %s (%d steps, arc=%d hops=%d over mock usage view)" % (out, len(path), arc, hops))

if __name__ == "__main__":
    main()
