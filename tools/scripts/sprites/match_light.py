"""A clip filmed under other light, matched to the hero's reference clip: one curve per colour channel, in linear light.

The user, 2026-10-24 (Isolde's hit reaction, rendered on a darker stone-wall set): "make her a shade lighter to match the
other light", then "you can match it all if possible". The figure's solid pixels (alpha > 240) of the clip's frames are matched,
channel by channel, quantile to quantile, to the same pixels of the reference frames (her studio idle): the shadows, the mid
tones, the highlights and the tint all land where the reference has them. Written into the cut spec as the clip's
`colorMatch` ({"r": [[in, out], ...], "g": ..., "b": ...}, linear 0..1), which video_sprite.py applies to the clip's frames.
Measure on frames where the figure stands as in the reference (same pose, same framing), not on the motion.

    python -I tools/scripts/sprites/match_light.py <cut.json> <clip> <clip frames first:last[,first:last]> <reference src dir> <reference first:last>
"""
import argparse
import json
import os

import numpy as np
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
QUANTILES = [0.1, 0.5, 1, 2, 3, 5, 7.5, 10, 15, 20, 25, 30, 35, 40, 45, 50, 55, 60, 65, 70, 75, 80, 85, 90, 93, 95, 97, 98, 99, 99.5, 99.9]


def linear(x):
    x = x / 255.0
    return np.where(x <= 0.04045, x / 12.92, ((x + 0.055) / 1.055) ** 2.4)


def ranges(text):
    out = []
    for part in text.split(","):
        a, b = part.split(":")
        out.extend(range(int(a), int(b) + 1))
    return out


def solid_pixels(src, frames):
    """The figure's solid pixels of every frame, linear RGB (N x 3)."""
    px = []
    for i in frames:
        s, a = os.path.join(src, f"src-{i:04d}.png"), os.path.join(src, f"alpha-{i:04d}.png")
        if not (os.path.exists(s) and os.path.exists(a)):
            raise SystemExit(f"missing matted frame {i} in {src}")
        rgb = linear(np.asarray(Image.open(s).convert("RGB"), np.float32))
        al = np.asarray(Image.open(a).convert("L"))
        px.append(rgb[al > 240])
    return np.concatenate(px)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("spec")
    ap.add_argument("clip")
    ap.add_argument("frames")
    ap.add_argument("ref_src")
    ap.add_argument("ref_frames")
    a = ap.parse_args()
    spec_path = os.path.abspath(a.spec)
    spec = json.load(open(spec_path, encoding="utf-8"))
    clip = next(c for c in spec["clips"] if c["clip"] == a.clip)
    mine = solid_pixels(os.path.join(ROOT, clip["src"]), ranges(a.frames))
    ref = solid_pixels(os.path.join(ROOT, a.ref_src), ranges(a.ref_frames))
    curves = {}
    for k, ch in enumerate("rgb"):
        xin = np.percentile(mine[:, k], QUANTILES)
        xout = np.percentile(ref[:, k], QUANTILES)
        mid = QUANTILES.index(50)
        print(f"{ch}: median {xin[mid]:.4f} -> {xout[mid]:.4f} ({np.log2(xout[mid] / xin[mid]):+.2f} stops), "
              f"5% {xin[5]:.4f} -> {xout[5]:.4f}, 95% {xin[-6]:.4f} -> {xout[-6]:.4f}")
        # 8-bit video: the deep shadows repeat one value over several quantiles; each input value keeps one point (the mean
        # of its outputs), the curve starts at black and rises in both columns
        points = {}
        for i, o in zip(xin, xout):
            points.setdefault(round(float(i), 6), []).append(float(o))
        xs = sorted(points)
        ys = np.maximum.accumulate([float(np.mean(points[x])) for x in xs])
        if xs[0] > 0:
            xs, ys = [0.0] + xs, np.concatenate([[0.0], ys])
        curves[ch] = [[x, round(float(y), 6)] for x, y in zip(xs, ys)]
    clip["colorMatch"] = {"reference": f"{a.ref_src} {a.ref_frames}", "frames": a.frames, **curves}
    with open(spec_path, "w", encoding="utf-8") as f:
        json.dump(spec, f, indent=1, ensure_ascii=False)
        f.write("\n")
    print(f"{a.clip}: colorMatch written into {os.path.relpath(spec_path, ROOT)}")


if __name__ == "__main__":
    main()
