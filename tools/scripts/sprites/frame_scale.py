"""Per-frame size of a pose whose video camera pulls back or pushes in (the user, 2026-11-26: "o birolho não pode mudar de tamanho só
porque colocou os braços de fora"; no new cut, "use what we just did"; then "estou vendo vários tamanhos variados nesses frames"):
the game draws each frame of the pose at the idle's size (MapStage: the pose's frameScale, by the full clip's frame, so Alta, Média,
Leve and Cinemática alike, feet kept on the ground).

The measure: the face's own points (eyes, teeth, skin: SIFT on the Alta frames already cut, drawn at game scale = the roster pose's
box over its crop), matched with RANSAC as a similarity (scale + turn + shift):
  - each frame against the idle's first frame, where the face shows (enough points agree);
  - each frame against the one before it, all through the clip (so a walk turned sideways keeps its own steady size);
  - a clip that follows another of the same video within 8 frames starts at the size that one ends at (same camera: the video pixels
    times the cuts' figurePx), which ties a walk turned sideways to the unfold before it;
solved together in log scale (least squares; a frame with no idle match follows its neighbours, and a clip that never matches the idle
keeps its own size as cut on the median). frameScale = the idle's size over the frame's. (The first version measured the ball's top
arc; the limbs crossing it fooled it: BirolhoLegs came out 7 % too big.)

Writes content/sprites/<set>/frame-scale.json {clip: [scale per frame]}; apply_sprite_overrides.py puts it on the poses ("frameScale").

    python -I tools/scripts/sprites/frame_scale.py <hero key> <set> <clip,clip,...>
"""
import json
import os
import sys

import numpy as np
from PIL import Image
from skimage.feature import SIFT, match_descriptors
from skimage.measure import ransac
from skimage.transform import SimilarityTransform

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
PX = 100          # pixels a game unit, the size every frame is compared at
IDLE_MIN = 60     # agreeing points for a frame-to-idle match to count
STEP_MIN = 25     # agreeing points for a frame-to-frame step to count


def picture(pose, st, version, i):
    c = pose["crop"]
    k = pose["box"]["h"] / (c[3] - c[1]) * PX
    im = Image.open(os.path.join(ROOT, "content", "sprites", st, f"{pose['clip']}-{version}", f"{pose['clip']}-{i}.png")).convert("RGBA")
    im = im.resize((max(1, round(im.width * k)), max(1, round(im.height * k))), Image.LANCZOS)
    a = np.asarray(im).astype(np.float32) / 255.0
    return a[..., :3].mean(2) * (a[..., 3] > 0.5)


def features(g):
    d = SIFT()
    d.detect_and_extract(g)
    return d.keypoints, d.descriptors


def similarity(f1, f2):
    """(scale of f2 against f1, agreeing points); (None, 0) when they do not agree."""
    (k1, d1), (k2, d2) = f1, f2
    if len(k1) < 3 or len(k2) < 3:
        return None, 0
    m = match_descriptors(d1, d2, max_ratio=0.75, cross_check=True)
    if len(m) < 3:
        return None, 0
    src, dst = k1[m[:, 0]][:, ::-1], k2[m[:, 1]][:, ::-1]
    model, inl = ransac((src, dst), SimilarityTransform, min_samples=3, residual_threshold=3, max_trials=2000, rng=0)
    if not model or inl is None:
        return None, 0
    return float(model.scale), int(inl.sum())


def solve(n, direct, steps):
    """log size per frame from the idle matches (direct: i -> (log s, weight)) and the steps (i -> (log s_i - log s_(i-1), weight))."""
    rows, rhs, w = [], [], []
    for i, (v, wt) in direct.items():
        r = np.zeros(n); r[i] = 1; rows.append(r); rhs.append(v); w.append(wt)
    for i, (v, wt) in steps.items():
        r = np.zeros(n); r[i] = 1; r[i - 1] = -1; rows.append(r); rhs.append(v); w.append(wt)
    for i in range(n):  # a weak pull to the size as cut, so every frame is fixed even where nothing matched
        r = np.zeros(n); r[i] = 1; rows.append(r); rhs.append(0.0); w.append(1e-3 if direct else 1e-4)
    A, b, W = np.array(rows), np.array(rhs), np.sqrt(np.array(w))
    x = np.linalg.lstsq(A * W[:, None], b * W, rcond=None)[0]
    if not direct:
        x -= np.median(x)  # no frame sees the idle's face: the clip keeps its own size on the median, only the camera's drift goes
    return x


def main():
    key, st, clips = sys.argv[1], sys.argv[2], sys.argv[3].split(",")
    roster = json.load(open(os.path.join(ROOT, "build", "scenes", "roster.json"), encoding="utf-8"))
    hero = next(h for h in roster["heroes"] if h["key"] == key)
    version = json.load(open(os.path.join(ROOT, "content", "sprites", st, "cut.json"), encoding="utf-8"))["version"]
    idle = next(p for p in hero["poses"] if p["pose"] == "idle")
    ref = features(picture(idle, st, version, 1))
    spec = {c["clip"]: c for c in json.load(open(os.path.join(ROOT, "content", "sprites", st, "cut.json"), encoding="utf-8"))["clips"]}
    solved = {}  # clip -> log drawn size per frame, for the clips after it in the same video
    out_path = os.path.join(ROOT, "content", "sprites", st, "frame-scale.json")
    out = {"about": "tools/scripts/sprites/frame_scale.py: per Alta frame, the size the game draws it at (the idle's face over this "
                    "frame's, matched by SIFT points)", "clips": {}}
    for clip in clips:
        pose = next(p for p in hero["poses"] if p["clip"] == clip and p.get("set") == st)
        n = pose["frames"]
        feats = [features(picture(pose, st, version, i)) for i in range(1, n + 1)]
        direct, steps = {}, {}
        for i, f in enumerate(feats):
            s, k = similarity(ref, f)
            if s and k >= IDLE_MIN:
                direct[i] = (np.log(s), float(k))
            if i:
                s, k = similarity(feats[i - 1], f)
                if s and k >= STEP_MIN:
                    steps[i] = (np.log(s), float(k))
        # A clip that follows another of the same video within a few frames (the walk after the unfold) starts where that one ends: the
        # same camera, so the body has the same size in the video's pixels, and the cuts' figurePx turn it into game units.
        me = spec[clip]
        for other, xo in solved.items():
            o = spec[other]
            if o["src"] == me["src"] and 0 <= me["first"] - o["last"] <= 8:
                direct[0] = (float(xo[-1] + np.log(o["figurePx"] / me["figurePx"])), 2000.0)
                print(f"  {clip} frame 1 continues {other}'s last frame (video {o['last']} -> {me['first']})")
        x = solve(n, direct, steps)
        solved[clip] = x
        scale = np.clip(np.exp(-x), 0.75, 1.35)
        out["clips"][clip] = [round(float(s), 4) for s in scale]
        print(f"{key} {clip}: {n} frames, {len(direct)} matched the idle, {len(steps)} steps; drawn size {np.exp(x).min():.3f}..{np.exp(x).max():.3f}"
              f" of the idle's -> scale {scale.min():.3f}..{scale.max():.3f}", flush=True)
    with open(out_path, "w", encoding="utf-8", newline="\n") as f:
        json.dump(out, f, ensure_ascii=False)
        f.write("\n")
    print("->", os.path.relpath(out_path, ROOT))


if __name__ == "__main__":
    main()
