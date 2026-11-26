"""Per-frame size of a pose whose video camera pulls back or pushes in (the user, 2026-11-26: "o birolho não pode mudar de tamanho só
porque colocou os braços de fora"; and: no new cut, "use what we just did"): the round body's diameter measured on every Alta frame
already cut, in game units (the roster pose's box over its crop), against the idle's; the game draws each frame at idle / frame
(MapStage: the pose's frameScale, by the full clip's frame, so Alta, Média, Leve and Cinemática alike, feet kept on the ground).

The body: the circle through the top arc of the figure's silhouette (as build/video-cut/birolhos-r-ballfit.py), for a unit whose body
is a ball (the Birolhos). Smoothed (median of 5, then mean of 3) so a limb crossing the arc on one frame does not make it pop.

Writes content/sprites/<set>/frame-scale.json {clip: [scale per frame]}; apply_sprite_overrides.py puts it on the poses ("frameScale").

    python -I tools/scripts/sprites/frame_scale.py <hero key> <set> <clip,clip,...>
"""
import json
import os
import sys

import numpy as np
from PIL import Image, ImageFilter

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))


def ball(path):
    im = Image.open(path).getchannel("A")
    im = im.resize((im.width // 2, im.height // 2)).point(lambda v: 255 if v > 128 else 0)
    im = im.filter(ImageFilter.MinFilter(11)).filter(ImageFilter.MaxFilter(11))
    a = np.asarray(im) > 0
    top = np.nonzero(a.any(1))[0].min()
    c0 = np.nonzero(a[top + 3])[0].mean()
    ds = []
    for h in range(30, 111, 10):
        xs = np.nonzero(a[top + h])[0]
        if len(xs) == 0:
            continue
        runs = np.split(xs, np.nonzero(np.diff(xs) > 1)[0] + 1)
        run = min(runs, key=lambda q: abs((q[0] + q[-1]) / 2 - c0))
        c = run[-1] - run[0] + 1
        ds.append(c * c / (4 * h) + h)
    return 2 * float(np.median(ds))


def folder(st, clip, version):
    return os.path.join(ROOT, "content", "sprites", st, f"{clip}-{version}")


def measure(pose, st, version):
    c = pose["crop"]
    units = pose["box"]["h"] / (c[3] - c[1])
    d = folder(st, pose["clip"], version)
    return [ball(os.path.join(d, f"{pose['clip']}-{i}.png")) * units for i in range(1, pose["frames"] + 1)]


def smooth(v):
    v = np.asarray(v)
    med = np.array([np.median(v[max(0, i - 2):i + 3]) for i in range(len(v))])
    return np.array([med[max(0, i - 1):i + 2].mean() for i in range(len(med))])


def main():
    key, st, clips = sys.argv[1], sys.argv[2], sys.argv[3].split(",")
    roster = json.load(open(os.path.join(ROOT, "build", "scenes", "roster.json"), encoding="utf-8"))
    hero = next(h for h in roster["heroes"] if h["key"] == key)
    version = json.load(open(os.path.join(ROOT, "content", "sprites", st, "cut.json"), encoding="utf-8"))["version"]
    idle = next(p for p in hero["poses"] if p["pose"] == "idle")
    target = float(np.median(measure(idle, st, version)))
    out_path = os.path.join(ROOT, "content", "sprites", st, "frame-scale.json")
    out = {"about": "tools/scripts/sprites/frame_scale.py: per Alta frame, the size the game draws it at (the idle's body over this "
                    f"frame's); the idle's body {target:.3f} units", "clips": {}}
    for clip in clips:
        pose = next(p for p in hero["poses"] if p["clip"] == clip and p.get("set") == st)
        body = smooth(measure(pose, st, version))
        scale = np.clip(target / body, 0.9, 1.3)
        out["clips"][clip] = [round(float(s), 4) for s in scale]
        print(f"{key} {clip}: body {body.min():.3f}..{body.max():.3f} units, idle {target:.3f}: scale {scale.min():.3f}..{scale.max():.3f}")
    with open(out_path, "w", encoding="utf-8", newline="\n") as f:
        json.dump(out, f, ensure_ascii=False)
        f.write("\n")
    print("->", os.path.relpath(out_path, ROOT))


if __name__ == "__main__":
    main()
