"""A video walk's real ground (the user, 2026-11-10, the Veo walks: "fully natural"): how far its feet carry it over the whole clip.

In a cut walk (video_sprite.py: the body held still), the boot on the ground slides back at the walk's own speed. Per pair of
frames, the shift of the lowest band of the figure (2.5% of its height: only the planted boot) that best lines it up with the
next (normalised correlation, parabola-refined to a tenth of a pixel); the median shift x the frames is the loop's ground, in
world units from the clip's figure size (figureUnits / figurePx). Prints a walk-natural.json entry per clip.

    python -I tools/scripts/sprites/walk_ground.py content/sprites/<set>/cut.json <clip> [<clip> ...] [--rate 1.7]
"""
import argparse, glob, json, os, re
import numpy as np
from PIL import Image


def shifts(frames_dir):
    fs = sorted(glob.glob(os.path.join(frames_dir, "*.png")), key=lambda f: int(re.findall(r"-(\d+)\.png$", f)[0]))
    A = [np.asarray(Image.open(f))[..., 3].astype(np.float32) / 255 for f in fs]
    H = max(a.shape[0] for a in A); W = max(a.shape[1] for a in A)
    A = [np.pad(a, ((H - a.shape[0], 0), (0, W - a.shape[1]))) for a in A]
    rows = np.nonzero(np.array([a.max(1) for a in A]).max(0) > 0.5)[0]
    top, bot = rows.min(), rows.max()
    band = max(3, int((bot - top) * 0.025))
    P = [a[bot - band:bot + 1].sum(0) for a in A]
    out = []
    for i in range(len(P)):
        p, q = P[i], P[(i + 1) % len(P)]
        if p.sum() < band * 5 or q.sum() < band * 5:
            continue
        cs = []
        for s in range(-30, 31):
            a, b = (p[s:], q[:len(q) - s]) if s >= 0 else (p[:s], q[-s:])
            cs.append(np.dot(a, b) / (np.linalg.norm(a) * np.linalg.norm(b) + 1e-9))
        cs = np.array(cs); k = int(np.argmax(cs))
        if cs[k] < 0.8 or k in (0, len(cs) - 1):
            continue
        d = (cs[k - 1] - cs[k + 1]) / (2 * (cs[k - 1] - 2 * cs[k] + cs[k + 1]) + 1e-12)
        out.append(k - 30 + d)
    return np.array(out), len(P)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("spec"); ap.add_argument("clips", nargs="+"); ap.add_argument("--rate", type=float, default=1.7)
    a = ap.parse_args()
    here = os.path.dirname(a.spec)
    spec = json.load(open(a.spec, encoding="utf-8"))
    res = json.load(open(os.path.join(here, spec.get("result", "cut-result.json")), encoding="utf-8"))["clips"]
    for clip in a.clips:
        sh, n = shifts(os.path.join(here, f"{clip}-{spec['version']}"))
        moving = sh[np.abs(sh) > 0.5]
        px = float(np.median(np.abs(moving)))
        r = res[clip]
        upp = spec["figureUnits"] / r["figurePx"]
        ground = px * n * upp
        secs = n * r["frameMs"] / 1000
        print(f'"{spec["set"]}/{clip}": {{"ground": {ground:.3f}, "rate": {a.rate}}},  '
              f'# {px:.2f} px a frame over {n} frames ({len(moving)}/{len(sh)} pairs), {ground / secs:.2f} u/s in the video, '
              f'{1.732 / (ground / secs * a.rate):.2f} s a hex at x{a.rate}')


if __name__ == "__main__":
    main()
