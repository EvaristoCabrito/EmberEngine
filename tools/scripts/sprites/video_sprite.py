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
- "stand" clips (idle, cast, strikes): the camera is locked, so every frame keeps the video's own place (one offset for
  the whole clip: the idle's boots on the clip's feet line);
- "walk" clips: each frame moved sideways so the body (hood + chest) stays put, and up / down so the boot on the ground
  sits on one line (lowest row of a solid boot 16 rows tall, the robe's thin tatters do not count); the loop is the
  stretch's best seam after that alignment (searched between loopMin and loopMax frames).
The world size: the idle's head to feet (the hood's top over the body's middle, not the staff's orb) is `figureUnits`
tall, so every clip shares one units-a-pixel and the hero is as tall as his other art.

    python -I tools/scripts/sprites/video_sprite.py content/sprites/voss-veo/cut.json [--only clip,clip]
"""
import argparse
import json
import os

import numpy as np
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
FPS = 24.0


def load_frames(src, first, last):
    rgb, alpha = [], []
    for i in range(first, last + 1):
        rgb.append(np.asarray(Image.open(os.path.join(src, f"src-{i:04d}.png")).convert("RGB")))
        alpha.append(np.asarray(Image.open(os.path.join(src, f"alpha-{i:04d}.png")).convert("L")))
    return np.stack(rgb), np.stack(alpha)


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
    result_path = os.path.join(here, "cut-result.json")
    result = json.load(open(result_path, encoding="utf-8")) if os.path.exists(result_path) else {"clips": {}}

    # one world scale for the hero: the idle's head to feet (first frame of the stand clip named in scaleFrom)
    ref = next(c for c in spec["clips"] if c["clip"] == spec["scaleFrom"])
    ref_a = np.asarray(Image.open(os.path.join(ROOT, ref["src"], f"alpha-{ref['first']:04d}.png")).convert("L"))
    head, feet = head_and_feet(ref_a)
    units_px = spec["figureUnits"] / (feet - head)
    ref_sole = sole_row(ref_a)
    print(f"scale: idle head {head} feet {feet} ({feet - head} px) -> {units_px:.6f} units a pixel; idle boot row {ref_sole}")

    for c in spec["clips"]:
        if only and c["clip"] not in only:
            continue
        rgb, alpha = load_frames(os.path.join(ROOT, c["src"]), c["first"], c["last"])
        plate = backdrop_plate(rgb, alpha)
        frames = [unmix(rgb[i], alpha[i], plate) for i in range(len(rgb))]
        n = len(frames)
        info = {"video": c.get("video"), "first": c["first"], "last": c["last"]}
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
            info.update(loopFirst=c["first"] + s, loopLast=c["first"] + e - 1, seamSteps=round(seam, 2),
                        bootRow=target, bodyDrift=[min(x_off[s:e]), max(x_off[s:e])])
            print(f"{c['clip']}: loop video frames {c['first'] + s}..{c['first'] + e - 1} ({e - s}), seam {seam:.2f} x a step, "
                  f"body moved {min(x_off)}..{max(x_off)} px, boot rows {min(soles)}..{max(soles)} -> {target}")
            sole = target
        else:
            sole = ref_sole  # the camera never moves: the boots stand where the idle's do
        # cut every frame to the clip's union box (+ margin), the feet line kept
        al = np.stack([f[..., 3] for f in frames])
        m = (al > 0).any(0)
        ys, xs = np.nonzero(m)
        pad = 4
        x0, x1 = max(0, xs.min() - pad), min(m.shape[1], xs.max() + 1 + pad)
        y0, y1 = max(0, ys.min() - pad), min(m.shape[0], max(ys.max() + 1, sole + 1) + pad)
        out_dir = os.path.join(here, f"{c['clip']}-{spec['version']}")
        os.makedirs(out_dir, exist_ok=True)
        for old in os.listdir(out_dir):
            if old.endswith(".png"):
                os.remove(os.path.join(out_dir, old))
        for i, f in enumerate(frames):
            px = np.clip(f[y0:y1, x0:x1] + 0.5, 0, 255).astype(np.uint8)
            Image.fromarray(px, "RGBA").save(os.path.join(out_dir, f"{c['clip']}-{i + 1}.png"), optimize=False, compress_level=6)
        nat = [int(x1 - x0), int(y1 - y0)]
        bb = (al[:, y0:y1, x0:x1] > 60).any(0)
        bys, bxs = np.nonzero(bb)
        info.update(frames=len(frames), frameMs=1000.0 / FPS, nat=nat,
                    crop=[int(bxs.min()), int(bys.min()), int(bxs.max() - bxs.min() + 1), int(bys.max() - bys.min() + 1)],
                    box={"w": round(nat[0] * units_px, 5), "h": round(nat[1] * units_px, 5),
                         "foot": round((y1 - 1 - sole) * units_px, 5)},
                    pattern=os.path.relpath(os.path.join(out_dir, f"{c['clip']}-{{}}.png"), ROOT).replace("\\", "/"))
        if "impactFrame" in c:
            info["impact"] = round((c["impactFrame"] - c["first"]) / len(frames), 4)
        result["clips"][c["clip"]] = info
        print(f"{c['clip']}: {len(frames)} frames {nat[0]} x {nat[1]}, box {info['box']}")
    result["unitsPerPixel"] = units_px
    with open(result_path, "w", encoding="utf-8") as f:
        json.dump(result, f, indent=1)
        f.write("\n")


if __name__ == "__main__":
    main()
