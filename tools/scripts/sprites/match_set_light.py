"""A whole video-cut set brought to one light, and that light to the other heroes'.

The user, 2026-11-11 (Kael V3 beside Isolde on the board): "look at how his light doesnt match". His videos were each filmed
under their own light (the walks brighter and bluer than the idle, the idle matched to the attack), and all of them flatter and
colder than the other heroes' art: milky shadows, a dim face, a grey-blue cast.

Two steps per clip, written as its `colorMatch` (one curve per channel, linear light; video_sprite.py applies it):
1. its video matched to the set's reference stretch, quantile to quantile (match_light.py's measure), on the frames named for
   that video in the spec's `light.videos` (frames where he stands as in the reference; a walk: the whole walk);
2. the set's `light.grade` on top: a gain per channel (the white balance), then out = a * v * (v / (v + toe)) ^ toePower: the
   mid tones and the lights scaled by `a` with their colour kept (a brighter face), the shadows under `toe` pulled down (no milky
   blacks), with a soft shoulder from `shoulder` up so the glints of steel do not burn out.

The spec's `light`: {"reference": "<src dir> first:last", "grade": {"gain": [r, g, b], "a": .., "toe": .., "toePower": .., "shoulder": ..},
"videos": {"<src dir>": "first:last[,first:last]"}} (a video not listed, or the reference's own, gets the grade alone).

    python -I tools/scripts/sprites/match_set_light.py <cut.json> [--preview out.png] [--write]

--preview: one frame of every clip under the new curves on the board's green, with the face's colour printed (nothing written).
--write: the curves into the spec.
"""
import argparse
import json
import os

import numpy as np
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
QUANTILES = [0.1, 0.5, 1, 2, 3, 5, 7.5, 10, 15, 20, 25, 30, 35, 40, 45, 50, 55, 60, 65, 70, 75, 80, 85, 90, 93, 95, 97, 98, 99, 99.5, 99.9]
GRID = np.concatenate([[0.0], np.geomspace(0.0005, 1.0, 47)])
LUMA = np.array([0.2126, 0.7152, 0.0722])


def linear(x):
    x = x / 255.0
    return np.where(x <= 0.04045, x / 12.92, ((x + 0.055) / 1.055) ** 2.4)


def srgb(x):
    return np.where(x <= 0.0031308, x * 12.92, 1.055 * np.clip(x, 0, 1) ** (1 / 2.4) - 0.055).clip(0, 1) * 255


def ranges(text):
    out = []
    for part in text.split(","):
        a, b = part.split(":")
        out.extend(range(int(a), int(b) + 1))
    return out


def frame(src, i):
    rgb = linear(np.asarray(Image.open(os.path.join(src, f"src-{i:04d}.png")).convert("RGB"), np.float32))
    return rgb, np.asarray(Image.open(os.path.join(src, f"alpha-{i:04d}.png")).convert("L"))


def solid_pixels(src, frames):
    px = []
    for i in frames:
        rgb, al = frame(src, i)
        px.append(rgb[al > 240])
    return np.concatenate(px)


def grade(x, k, g):
    v = g["gain"][k] * x
    y = g["a"] * v * (v / (v + g["toe"])) ** g["toePower"]
    t = g["shoulder"]
    return np.where(y <= t, y, t + (1 - t) * (1 - np.exp(-(y - t) / (1 - t))))


def curves_of(src, frames, ref_q, g):
    """Per channel [[in, out], ...]: the video's quantiles onto the reference's (none: the identity), then the grade."""
    mine = solid_pixels(src, frames) if frames else None
    out = {}
    for k, ch in enumerate("rgb"):
        if mine is None:
            xs, ys = GRID, GRID
        else:
            # 8-bit video: the deep shadows repeat one value over several quantiles; each input value keeps one point
            points = {}
            for i, o in zip(np.percentile(mine[:, k], QUANTILES), ref_q[k]):
                points.setdefault(round(float(i), 6), []).append(float(o))
            xs = sorted(points)
            ys = np.maximum.accumulate([float(np.mean(points[x])) for x in xs])
            if xs[0] > 0:
                xs, ys = [0.0] + xs, np.concatenate([[0.0], ys])
        out[ch] = [[round(float(x), 6), round(float(y), 6)] for x, y in zip(xs, np.maximum.accumulate(grade(np.asarray(ys), k, g)))]
    return out


def apply(rgb, curves):
    x = rgb.copy()
    for k, ch in enumerate("rgb"):
        xin, xout = np.array(curves[ch], np.float64).T
        v = x[..., k]
        x[..., k] = np.where(v > xin[-1], v * (xout[-1] / xin[-1]), np.interp(v, xin, xout))
    return x


def face(rgb, al):
    """The face's colour (linear): the skin in the head's rows, its brighter half."""
    ys = np.nonzero((al > 240).any(1))[0]
    y0, h = ys.min(), ys.max() - ys.min()
    m = al > 240
    m[: y0 + int(h * 0.02)] = False
    m[y0 + int(h * 0.17):] = False
    s = srgb(rgb)
    r, gch, b = s[..., 0], s[..., 1], s[..., 2]
    p = rgb[m & (r > gch * 1.08) & (gch > b * 1.02) & (r > 90) & (r - b > 25) & (r - b < 130)]
    if len(p) < 50:
        return None
    y = p @ LUMA
    return np.median(p[(y > np.percentile(y, 40)) & (y < np.percentile(y, 90))], 0)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("spec")
    ap.add_argument("--preview", default="")
    ap.add_argument("--write", action="store_true")
    a = ap.parse_args()
    spec_path = os.path.abspath(a.spec)
    spec = json.load(open(spec_path, encoding="utf-8"))
    light = spec["light"]
    ref_src, ref_frames = light["reference"].split(" ")
    ref = solid_pixels(os.path.join(ROOT, ref_src), ranges(ref_frames))
    ref_q = [np.percentile(ref[:, k], QUANTILES) for k in range(3)]
    by_video, shots = {}, []
    for c in spec["clips"]:
        if "src" not in c:
            continue
        if c["src"] not in by_video:
            frames = ranges(light["videos"][c["src"]]) if c["src"] in light.get("videos", {}) and c["src"] != ref_src else []
            by_video[c["src"]] = (curves_of(os.path.join(ROOT, c["src"]), frames, ref_q, light["grade"]), light["videos"].get(c["src"], ""))
        curves, measured = by_video[c["src"]]
        c["colorMatch"] = {"reference": light["reference"] + " + the set's grade (match_set_light.py)", "frames": measured, **curves}
        rgb, al = frame(os.path.join(ROOT, c["src"]), c["first"])
        after = apply(rgb, curves)
        f0, f1 = face(rgb, al), face(after, al)
        solid = after[al > 240] @ LUMA
        text = f"{c['clip']:14s} frame {c['first']:3d}"
        if f0 is not None and f1 is not None:
            text += (f"  face Y {f0 @ LUMA:.3f} -> {f1 @ LUMA:.3f}, r/g {f0[0] / f0[1]:.2f} -> {f1[0] / f1[1]:.2f}, r/b {f0[0] / f0[2]:.2f} -> {f1[0] / f1[2]:.2f}")
        q = np.percentile(solid, [10, 25, 50, 90, 98])
        mean = after[al > 240].mean(0)
        print(text + "  body 10/25/50/90/98% " + " ".join(f"{v:.4f}" for v in q) + f"  r/b {mean[0] / mean[2]:.2f}")
        if a.preview:
            ys, xs = np.nonzero(al > 20)
            box = (xs.min(), ys.min(), xs.max() + 1, ys.max() + 1)
            for pic in (rgb, after):
                im = Image.fromarray(np.dstack([srgb(pic), al[..., None]]).astype(np.uint8), "RGBA").crop(box)
                im.thumbnail((400, 520))
                shots.append(im)
    if a.preview:
        row = (len(shots) + 1) // 2
        rows = [shots[:row], shots[row:]]
        sheet = Image.new("RGBA", (max(sum(s.width for s in r) for r in rows) + 10, 2 * 530), (62, 84, 48, 255))
        for j, r in enumerate(rows):
            x = 5
            for s in r:
                sheet.alpha_composite(s, (x, 530 * j + 525 - s.height))
                x += s.width
        sheet.convert("RGB").save(a.preview)
        print(a.preview)
    if a.write:
        with open(spec_path, "w", encoding="utf-8") as f:
            json.dump(spec, f, indent=1, ensure_ascii=False)
            f.write("\n")
        print(f"{spec_path}: colorMatch of {len(by_video)} videos written")


if __name__ == "__main__":
    main()
