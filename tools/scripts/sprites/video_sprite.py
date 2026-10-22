"""Video -> sprite, step 2: a hero's clips from the matted video frames (video_matte.py), at the video's own resolution.

Reads a cut spec (e.g. content/sprites/voss-veo/cut.json) and for every clip writes
<spec dir>/<clip>-<version>/<clip>-N.png (N from 1, straight alpha, every native frame of the clip's stretch) and
<spec dir>/cut-result.json (per clip: frames, frame time, nat, crop, box, impact share; for content/sprite-overrides.json).

Per frame:
- the backdrop is unmixed from the soft edges and the glows: frame = a * figure + (1 - a) * backdrop, with the backdrop
  taken from the stretch's own frames where the matte is clear (median, a smooth 1/8-size plate); else the dark grey
  studio wall shows as a halo round the robe and inside the purple / white light;
- matte noise under 3% is dropped;
- no resampling: frames are only cut and moved by whole pixels;
- "stand" clips (idle, cast, strikes): the camera is locked, so every frame keeps the video's own place (the clip's
  median boot row is its feet line; a fall with `standUntil` takes the line and the height from its standing frames only);
  a loop with loopMin / loopMax is a hard cut at the stretch's best seam, like the
  walks (the user, 2026-10-19: no blend, no interpolation; the old `crossfade` blend is kept only for old specs);
- "walk" clips: each frame moved sideways so the body (hood + chest) stays put, and up / down so the boot on the ground
  sits on one line (lowest row of a solid boot 16 rows tall, the robe's thin tatters do not count); the loop is the
  stretch's best seam after that alignment (searched between loopMin and loopMax frames).
The world size: each video is framed its own way, so each clip's median hood top (the first row 61 px wide: not the
staff's orb or spikes) to boot row is `figureUnits` tall: the hero stands as tall in every clip and as his other art.

    python -I tools/scripts/sprites/video_sprite.py content/sprites/voss-veo/cut.json [--only clip,clip]
"""
import argparse
import json
import os

import numpy as np
from PIL import Image, ImageFilter


def despeck(px, max_area, min_gap):
    """`despeck` (the user, 2026-10-22, Slime36 / Slime64: "o pixel do lado direito do slime"): a speck of the matte floating on its
    own beside the body (smaller than `max_area` px and more than `min_gap` px from the body) is cleared, with its soft edge; drops
    touching or close to the body stay."""
    from scipy import ndimage
    solid = px[..., 3] > 20
    lab, n = ndimage.label(solid)
    if n < 2:
        return px
    sizes = ndimage.sum(solid, lab, range(1, n + 1))
    body = lab == (int(np.argmax(sizes)) + 1)
    gap = ndimage.distance_transform_edt(~body)
    out = px.copy()
    for i, s in enumerate(sizes):
        piece = lab == i + 1
        if s >= max_area or gap[piece].min() <= min_gap:
            continue
        halo = ndimage.binary_dilation(piece, iterations=3) & (px[..., 3] > 0) & ~body
        out[halo] = 0
    return out

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
FPS = 24.0


def load_frames(src, first, last):
    rgb, alpha = [], []
    for i in range(first, last + 1):
        rgb.append(np.asarray(Image.open(os.path.join(src, f"src-{i:04d}.png")).convert("RGB")))
        alpha.append(np.asarray(Image.open(os.path.join(src, f"alpha-{i:04d}.png")).convert("L")))
    return np.stack(rgb), np.stack(alpha)


def load_sheet(sheet, first, last):
    """A clip from a sprite sheet (the user's art already cut out: straight alpha, a TexturePacker-style JSON beside it with
    "frames": {"frame_NNN": {"frame": {x, y, w, h}, "duration": ms}}): frames first..last (0-based, the JSON's own numbering),
    as float straight RGBA, and the frame time in ms (the JSON's median duration). Nothing is unmixed: the alpha is the art's."""
    path = os.path.join(ROOT, sheet)
    meta = json.load(open(os.path.splitext(path)[0] + ".json", encoding="utf-8"))["frames"]
    names = sorted(meta)
    img = np.asarray(Image.open(path).convert("RGBA"))
    frames, durs = [], []
    for name in names[first:last + 1]:
        r = meta[name]["frame"]
        frames.append(img[r["y"]:r["y"] + r["h"], r["x"]:r["x"] + r["w"]].astype(np.float32))
        durs.append(float(meta[name].get("duration", 1000.0 / FPS)))
    if len({f.shape for f in frames}) != 1:
        raise SystemExit(f"{sheet}: frames {first}..{last} differ in size")
    return frames, float(np.median(durs))


def backdrop_plate(rgb, alpha, step=8):
    """The wall behind the figure: per 1/8-size cell, the median colour of the frames where the matte is clear there."""
    n, h, w, _ = rgb.shape
    hs, ws = h // step, w // step
    small = rgb[:, :hs * step, :ws * step].reshape(n, hs, step, ws, step, 3).mean((2, 4))
    clear = alpha[:, :hs * step, :ws * step].reshape(n, hs, step, ws, step).max((2, 4)) < 8
    plate = np.full((hs, ws, 3), np.nan, np.float32)
    for y in range(hs):
        for x in range(ws):
            c = clear[:, y, x]
            if c.any():
                plate[y, x] = np.median(small[c, y, x], axis=0)
    # cells the figure never leaves: the nearest known cell along the row, else the column, else the wall's median
    known = ~np.isnan(plate[..., 0])
    fill = np.nanmedian(plate.reshape(-1, 3), axis=0)
    for y in range(hs):
        xs = np.nonzero(known[y])[0]
        for x in range(ws):
            if not known[y, x]:
                plate[y, x] = plate[y, xs[np.argmin(np.abs(xs - x))]] if len(xs) else fill
    img = Image.fromarray(np.clip(plate, 0, 255).astype(np.uint8)).resize((w, h), Image.BICUBIC)
    return np.asarray(img).astype(np.float32)


def unmix(rgb, alpha, plate):
    a = alpha.astype(np.float32) / 255.0
    a = np.where(a < 0.03, 0.0, a)
    c = rgb.astype(np.float32)
    fg = (c - (1.0 - a[..., None]) * plate) / np.maximum(a[..., None], 1e-3)
    fg = np.where(a[..., None] > 0.6, c, fg)  # solid inside: the frame's own pixels, untouched
    fg = np.clip(fg, 0, 255)
    return np.dstack([fg, a * 255.0]).astype(np.float32)


def wall_key(rgb, alpha, plate, key):
    """A clip's `wallKey` (the cast, 2026-10-20): inside a region of the video, whatever differs from the wall is figure too.
    The model drops a bright light against the dark wall as background and the staff's head with it (the cast's flash and
    burst); the video plate is no good there either (the head never leaves that spot, so its only "clear" frames are the
    broken ones). So each frame's wall is a smooth surface (a degree-4 polynomial, the studio backdrop's gradient) fitted
    to the sure wall outside the region, 20 px clear of anything the model kept; the difference from it, from `low` to
    `high`, is alpha (the staff, the burst, the light it throws), kept where it is more than the model's, faded out over
    `feather` px at the region's inner edges. Returns the alpha and the plate to unmix with."""
    x0, y0, x1, y1 = key["region"]
    h, w, _ = rgb.shape
    far = np.asarray(Image.fromarray((alpha > 4).astype(np.uint8) * 255).filter(ImageFilter.MaxFilter(41))) == 0
    outside = np.ones((h, w), bool)
    outside[y0:y1, x0:x1] = False
    ys, xs = np.nonzero(far & outside)
    sel = np.random.default_rng(1).choice(len(ys), min(len(ys), 20000), replace=False)
    deg = 4

    def basis(y, x):
        u, v = x / w * 2 - 1, y / h * 2 - 1
        return np.stack([u ** i * v ** j for i in range(deg + 1) for j in range(deg + 1 - i)], -1)
    coef = np.linalg.lstsq(basis(ys[sel].astype(np.float64), xs[sel].astype(np.float64)),
                           rgb[ys[sel], xs[sel]].astype(np.float64), rcond=None)[0]
    gy, gx = np.mgrid[y0:y1, x0:x1]
    fit = (basis(gy.astype(np.float64), gx.astype(np.float64)) @ coef).astype(np.float32)
    d = np.abs(rgb[y0:y1, x0:x1].astype(np.float32) - fit).max(-1)
    k = np.clip((d - key["low"]) / (key["high"] - key["low"]), 0, 1)
    # the window: 1 inside, fading to 0 over `feather` px at every edge that is not the frame's own edge
    f = float(key["feather"])
    wx = np.minimum(1.0, np.minimum((gx - x0 + 0.5) / f if x0 > 0 else 1e9, (x1 - 0.5 - gx) / f if x1 < w else 1e9))
    wy = np.minimum(1.0, np.minimum((gy - y0 + 0.5) / f if y0 > 0 else 1e9, (y1 - 0.5 - gy) / f if y1 < h else 1e9))
    win = np.clip(np.minimum(wx, wy), 0, 1).astype(np.float32)
    a = alpha.copy()
    a[y0:y1, x0:x1] = np.maximum(alpha[y0:y1, x0:x1], np.round(k * win * 255).astype(np.uint8))
    p = plate.copy()
    p[y0:y1, x0:x1] = fit * win[..., None] + plate[y0:y1, x0:x1] * (1 - win[..., None])
    return a, p


def resize_premultiplied(f, nw, nh):
    """A straight-alpha float RGBA frame resized to nw x nh: premultiplied (no dark or light fringe from the clear pixels),
    Lanczos per channel, ringing clamped (alpha 0..255, colour within its alpha), then straight again."""
    a = f[..., 3] / 255.0
    chans = [f[..., k] * a for k in range(3)] + [f[..., 3]]
    out = [np.asarray(Image.fromarray(ch.astype(np.float32), "F").resize((nw, nh), Image.LANCZOS)) for ch in chans]
    alpha = np.clip(out[3], 0, 255)
    an = alpha / 255.0
    rgb = [np.clip(out[k], 0, alpha) / np.maximum(an, 1e-4) for k in range(3)]
    rgb = [np.where(an > 1e-4, ch, 0) for ch in rgb]
    return np.dstack(rgb + [alpha]).astype(np.float32)


def sole_row(a):
    solid = a > 230
    h = a.shape[0]
    for y in range(h - 1, 16, -1):
        col = solid[y - 15:y + 1].all(0)
        run = best = 0
        for v in col:
            run = run + 1 if v else 0
            best = max(best, run)
        if best >= 14:
            return y
    return None


def head_and_feet(a):
    m = a > 60
    ys, xs = np.nonzero(m)
    x0, x1 = xs.min(), xs.max()
    mid = (x0 + x1) // 2
    band = m[:, max(0, mid - a.shape[1] // 10): mid + a.shape[1] // 10]
    head = np.nonzero(band.any(1))[0].min()
    return int(head), int(ys.max())


def hood_top(a, width=None, thick=None):
    """The figure's top: the first row holding a solid block `width` px wide and `thick` rows tall. The staff, its spikes
    and the orb's ring are thinner, so they do not count (in profile they rise above the hood); the hood is wider."""
    # sized to the video: 61 x 8 px in a 1280-tall frame (the cast's 720-tall video: 34 x 4)
    width = width or max(9, round(a.shape[0] * 61 / 1280))
    thick = thick or max(3, round(a.shape[0] * 8 / 1280))
    m = (a > 60).astype(np.int32)
    v = np.cumsum(np.pad(m, ((1, 0), (0, 0))), axis=0)
    m = np.zeros_like(m)
    m[:-(thick - 1)] = (v[thick:] - v[:-thick]) == thick  # solid `thick` rows down (the orb's ring is ~6 px)
    c = np.cumsum(np.pad(m, ((0, 0), (1, 0))), axis=1)
    wide = (c[:, width:] - c[:, :-width]) == width
    rows = np.nonzero(wide.any(1))[0]
    return int(rows.min())


def body_shift(prev, cur, rows, cols, rng=40):
    """Horizontal shift that lays `cur`'s body patch (alpha) on `prev`'s."""
    pa = prev[rows[0]:rows[1], cols[0]:cols[1]].astype(np.float32)
    best = (1e18, 0)
    for dx in range(-rng, rng + 1):
        c0, c1 = cols[0] + dx, cols[1] + dx
        if c0 < 0 or c1 > cur.shape[1]:
            continue
        e = np.abs(pa - cur[rows[0]:rows[1], c0:c1].astype(np.float32)).mean()
        if e < best[0]:
            best = (e, dx)
    return best[1]


def shifted(img, dx, dy):
    out = np.zeros_like(img)
    h, w = img.shape[:2]
    ys, yd = (slice(0, h - dy), slice(dy, h)) if dy >= 0 else (slice(-dy, h), slice(0, h + dy))
    xs, xd = (slice(0, w - dx), slice(dx, w)) if dx >= 0 else (slice(-dx, w), slice(0, w + dx))
    out[yd, xd] = img[ys, xs]
    return out


def best_loop(frames, lo, hi):
    """(start, end) inside the stretch whose seam (3 frames after the end against the start's 3) differs least."""
    small = [np.asarray(Image.fromarray(f.astype(np.uint8)).resize((f.shape[1] // 8, f.shape[0] // 8))).astype(np.float32)
             for f in frames]
    steps = np.median([np.abs(small[i] - small[i - 1]).mean() for i in range(1, len(small))])
    best = None
    for s in range(0, len(small)):
        for e in range(s + lo, min(len(small) - 2, s + hi + 1)):
            d = np.mean([np.abs(small[e + k] - small[s + k]).mean() for k in range(3)])
            if best is None or d < best[0]:
                best = (d, s, e)
    return best[1], best[2], best[0] / steps


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("spec")
    ap.add_argument("--only", default="")
    a = ap.parse_args()
    spec_path = os.path.abspath(a.spec)
    spec = json.load(open(spec_path, encoding="utf-8"))
    here = os.path.dirname(spec_path)
    only = set(filter(None, a.only.split(",")))
    result_path = os.path.join(here, spec.get("result", "cut-result.json"))  # a second set from the same frames writes its own
    try:  # the clips cut before; a result a power cut zero-filled (Slime64's, 2026-10-22) counts as none
        result = json.load(open(result_path, encoding="utf-8")) if os.path.exists(result_path) else {"clips": {}}
    except ValueError:
        result = {"clips": {}}

    for c in spec["clips"]:
        if only and c["clip"] not in only:
            continue
        frame_ms = 1000.0 / FPS
        if "sheet" in c:
            # a sprite sheet (the Cultist's hit and death, 2026-10-22): the user's own frames and timing, alpha as drawn
            frames, frame_ms = load_sheet(c["sheet"], c["first"], c["last"])
            alpha = np.stack([np.clip(f[..., 3], 0, 255).astype(np.uint8) for f in frames])
        else:
            rgb, alpha = load_frames(os.path.join(ROOT, c["src"]), c["first"], c["last"])
            plate = backdrop_plate(rgb, alpha)
        if "sheet" in c:
            pass
        elif "wallKey" in c:
            # a key's `frames` [first, last] (video numbering) limits it to those frames (the death: only while the staff falls)
            lo, hi = c["wallKey"].get("frames", [c["first"], c["last"]])
            frames = [unmix(rgb[i], *wall_key(rgb[i], alpha[i], plate, c["wallKey"])) if lo <= c["first"] + i <= hi
                      else unmix(rgb[i], alpha[i], plate) for i in range(len(rgb))]
        else:
            frames = [unmix(rgb[i], alpha[i], plate) for i in range(len(rgb))]
        n = len(frames)
        info = {"video": c.get("video", c.get("sheet")), "first": c["first"], "last": c["last"]}
        # Every video is framed its own way (the cast is landscape, ~660 px of figure; the staff counter ~4% closer than the
        # idle) and he steps in some (both strikes settle back in their first half second): each clip's frames are set on
        # its own feet line, and its median hood top to boot row is `figureUnits` tall, so he stands as tall on the same
        # ground in every clip with no resampling.
        if c["kind"] == "walk":
            al = [f[..., 3] for f in frames]
            h, w = al[0].shape
            # body patch: hood + chest, the rows between 8% and 40% of the figure, centred on frame 0's body
            hd, ft = head_and_feet(al[0].astype(np.uint8))
            rows = (hd + int((ft - hd) * 0.08), hd + int((ft - hd) * 0.40))
            ys, xs = np.nonzero(al[0][rows[0]:rows[1]] > 128)
            mid = int(np.median(xs))
            cols = (max(0, mid - 110), min(w, mid + 110))
            x_off = [0]
            for i in range(1, n):
                x_off.append(x_off[-1] - body_shift(al[i - 1], al[i], rows, cols))
            soles = [sole_row(x.astype(np.uint8)) for x in al]
            target = int(np.median(soles))
            frames = [shifted(f, x_off[i], target - soles[i]) for i, f in enumerate(frames)]
            s, e, seam = best_loop(frames, c["loopMin"], c["loopMax"])
            frames = frames[s:e]
            tops = [hood_top(f[..., 3]) for f in frames]
            height = float(np.median([target - t for t in tops]))
            info.update(loopFirst=int(c["first"] + s), loopLast=int(c["first"] + e - 1), seamSteps=round(float(seam), 2),
                        bootRow=int(target), bodyDrift=[int(min(x_off[s:e])), int(max(x_off[s:e]))])
            print(f"{c['clip']}: loop video frames {c['first'] + s}..{c['first'] + e - 1} ({e - s}), seam {seam:.2f} x a step, "
                  f"body moved {min(x_off)}..{max(x_off)} px, boot rows {min(soles)}..{max(soles)} -> {target}")
            sole = target
        else:
            # the boot row per frame, smoothed over 5 frames (a misread frame must not shake him), every frame moved onto
            # the clip's median line
            raw = [sole_row(alpha[i]) for i in range(n)]
            known = [r for r in raw if r is not None]
            raw = [r if r is not None else int(np.median(known)) for r in raw]
            boots = [int(np.median(raw[max(0, i - 2):i + 3])) for i in range(n)]
            tops = [hood_top(alpha[i]) for i in range(n)]
            if "standUntil" in c:
                # a fall (death): he leaves his feet, so the feet line and the height come only from the frames up to video
                # frame `standUntil`, where he still stands, and no frame is moved (the camera is locked)
                k = c["standUntil"] - c["first"] + 1
                sole = int(np.median(boots[:k]))
                height = float(np.median([boots[i] - tops[i] for i in range(k)]))
                boots = [sole] * n
            else:
                sole = int(np.median(boots))
                height = float(np.median([boots[i] - tops[i] for i in range(n)]))
            frames = [shifted(f, 0, sole - boots[i]) if boots[i] != sole else f for i, f in enumerate(frames)]
            info["bootRow"], info["bootRows"] = sole, [int(min(boots)), int(max(boots))]
            print(f"{c['clip']}: boot rows {min(boots)}..{max(boots)} -> {sole}")
            if "loopMin" in c:
                # a stand loop: a hard cut at the stretch's best seam, every frame the video's own (no blend)
                s, e, seam = best_loop(frames, c["loopMin"], c["loopMax"])
                frames = frames[s:e]
                height = float(np.median([boots[i] - tops[i] for i in range(s, e)]))
                info.update(loopFirst=int(c["first"] + s), loopLast=int(c["first"] + e - 1), seamSteps=round(float(seam), 2))
                print(f"{c['clip']}: loop video frames {c['first'] + s}..{c['first'] + e - 1} ({e - s}), seam {seam:.2f} x a step")
            xf = c.get("crossfade", 0)
            if xf:
                # a loop's seam: its last `xf` frames blend into the video frames just before its first (premultiplied),
                # so the last frame leads into the first as the video itself does
                pre_rgb, pre_a = load_frames(os.path.join(ROOT, c["src"]), c["first"] - xf, c["first"] - 1)
                pre = [unmix(pre_rgb[i], pre_a[i], plate) for i in range(xf)]
                for k in range(xf):
                    t = (k + 1) / (xf + 1)
                    f, g = frames[n - xf + k], pre[k]
                    pa = f[..., 3:] / 255 * (1 - t) + g[..., 3:] / 255 * t
                    pc = f[..., :3] * f[..., 3:] / 255 * (1 - t) + g[..., :3] * g[..., 3:] / 255 * t
                    frames[n - xf + k] = np.dstack([pc / np.maximum(pa, 1e-4), pa * 255]).astype(np.float32)
                info["crossfade"] = xf
        # `frameCount` (a clip's or the spec's; the user, 2026-10-22, "Slime36 with 36 frames per animation"): that many of the
        # clip's frames, picked evenly from its first to its last, each kept as it is (no blend); the clip keeps its duration
        n_full = len(frames)
        want_n = c.get("frameCount", spec.get("frameCount"))
        if want_n and want_n < n_full:
            pick = [round(k * (n_full - 1) / (want_n - 1)) for k in range(want_n)]
            frames = [frames[k] for k in pick]
            frame_ms = frame_ms * n_full / want_n
            info["frameCountFrom"] = n_full
        # `figurePx` (a clip's or the spec's): one fixed figure height in pixels instead of the clip's measured one, for a creature
        # whose shape changes (the Corrosive Slime flattens to walk and to strike, 2026-10-22) filmed with one camera in every video:
        # every clip then keeps the same pixel scale, so it is drawn the same size in every pose
        if "figurePx" in c or "figurePx" in spec:
            height = float(c.get("figurePx", spec.get("figurePx")))
        clip_units_px = spec["figureUnits"] / height
        info["figurePx"] = round(height, 1)
        print(f"{c['clip']}: figure {height:.0f} px -> {clip_units_px:.6f} units a pixel")
        # cut every frame to the clip's union box (+ margin), the feet line kept
        al = np.stack([f[..., 3] for f in frames])
        m = (al > 0).any(0)
        ys, xs = np.nonzero(m)
        pad = 4
        x0, x1 = max(0, xs.min() - pad), min(m.shape[1], xs.max() + 1 + pad)
        y0, y1 = max(0, ys.min() - pad), min(m.shape[0], max(ys.max() + 1, sole + 1) + pad)
        if "bodyAt" in c:
            # the card stands centred on its hex: the hood's centre must sit `bodyAt` units from the frame's centre as in
            # the clip's other poses, or he slides sideways when the pose starts (cast v02's glow widened its frame on the
            # left only); the frame is widened with clear columns on the side that needs them
            t = hood_top(frames[0][..., 3])
            hx = np.nonzero((frames[0][t + 20:t + 120, :, 3] > 128).any(0))[0]
            hood = (hx.min() + hx.max()) / 2
            want = c["bodyAt"] / clip_units_px  # px from the frame's centre
            shift = int(round(2 * (hood - want) - (x0 + x1)))  # x0 + x1 must become 2 * (hood - want)
            # past the video's own edge when it must (the slime's hit and death fill the whole width): clear columns there
            if shift > 0:
                x1 = x1 + shift
            elif shift < 0:
                x0 = x0 + shift
            info["bodyAt"] = round(((hood - (x0 + x1) / 2)) * clip_units_px, 4)
        out_dir = os.path.join(here, f"{c['clip']}-{c.get('version', spec['version'])}")  # a clip's own version wins
        os.makedirs(out_dir, exist_ok=True)
        for old in os.listdir(out_dir):
            if old.endswith(".png"):
                os.remove(os.path.join(out_dir, old))
        # `scaleTo` (the user, 2026-10-21: "deixar todos em ~610 px"): the clip's figure brought down to that many pixels
        # tall, so every clip of the hero is drawn at one size on the GPU (the videos were framed ~1100 vs ~610 px); the only
        # resampling in the pipeline, done once on the cut frames: premultiplied alpha, Lanczos, every frame kept
        scale = min(1.0, c["scaleTo"] / height) if "scaleTo" in c else 1.0
        if "scaleBy" in c:  # a share of the video's own size (a creature with no ~1100 px figure: the slime's Média = 0.555, Voss's ratio)
            scale = min(1.0, float(c["scaleBy"]))
        w0, h0 = int(x1 - x0), int(y1 - y0)
        nw, nh = (max(1, round(w0 * scale)), max(1, round(h0 * scale))) if scale < 1.0 else (w0, h0)
        units_out = clip_units_px * w0 / nw if scale < 1.0 else clip_units_px
        sole_out = (sole - y0 + 0.5) * nh / h0 - 0.5
        bb = np.zeros((nh, nw), bool)
        for i, f in enumerate(frames):
            if x0 >= 0 and x1 <= f.shape[1]:
                cut = f[y0:y1, x0:x1]
            else:  # widened past the video's edge (bodyAt): clear columns beyond it
                cut = np.zeros((y1 - y0, x1 - x0, 4), np.float32)
                sx0, sx1 = max(0, x0), min(f.shape[1], x1)
                cut[:, sx0 - x0:sx1 - x0] = f[y0:y1, sx0:sx1]
            if scale < 1.0:
                cut = resize_premultiplied(cut, nw, nh)
            px = np.clip(cut + 0.5, 0, 255).astype(np.uint8)
            dsp = c.get("despeck", spec.get("despeck"))
            if dsp:  # its sizes are in the video's own pixels: a scaled copy scales them
                px = despeck(px, dsp["maxArea"] * scale * scale, dsp["minGap"] * scale)
            if c.get("mirror"):  # the clip faces the other way in its video (the slime's strike): turned to face as the others
                px = np.ascontiguousarray(px[:, ::-1])
            bb |= px[..., 3] > 60
            Image.fromarray(px, "RGBA").save(os.path.join(out_dir, f"{c['clip']}-{i + 1}.png"), optimize=False, compress_level=6)
        nat = [nw, nh]
        bys, bxs = np.nonzero(bb)
        if scale < 1.0:
            info["scaledTo"] = round(height * nh / h0, 1)
            if "bodyAt" in info:
                info["bodyAt"] = round(info["bodyAt"], 4)  # units: unchanged by the scale
        info.update(frames=len(frames), frameMs=frame_ms, nat=nat,
                    crop=[int(bxs.min()), int(bys.min()), int(bxs.max() - bxs.min() + 1), int(bys.max() - bys.min() + 1)],
                    box={"w": round(nat[0] * units_out, 5), "h": round(nat[1] * units_out, 5),
                         "foot": round(float(nh - 1 - sole_out) * units_out, 5)},
                    pattern=os.path.relpath(os.path.join(out_dir, f"{c['clip']}-{{}}.png"), ROOT).replace("\\", "/"))
        if "impactFrame" in c:
            info["impact"] = round((c["impactFrame"] - c["first"]) / n_full, 4)
        result["clips"][c["clip"]] = info
        print(f"{c['clip']}: {len(frames)} frames {nat[0]} x {nat[1]}, box {info['box']}")
    text = json.dumps(result, indent=1) + "\n"  # whole before the file is touched
    with open(result_path + ".tmp", "w", encoding="utf-8") as f:
        f.write(text)
    os.replace(result_path + ".tmp", result_path)


if __name__ == "__main__":
    main()
