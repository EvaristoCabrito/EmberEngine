"""Steadies a clip whose video camera drifted: every frame moved so the figure's still part (a band of columns, e.g. the shell and
the rear legs) sits where it sits on frame 1, the moving parts (a claw, the jaws) left to move. The frames are written to a new
folder (the old ones stay on disk) and the cut result's pattern / crop point at it.

The user, 2026-11-11 (the Rastreador de Cinzas): "a weird off set motion going up and down as he idles moves the front claw"; its
idle video sinks ~27 px for a second and springs back at the loop.

    python -I tools/scripts/sprites/steady_clip.py <cut-result.json> <clip> <new folder suffix> [x0 x1 as shares of the width]

e.g. content/sprites/ashes-crawler/cut-result.json base v02 0.0 0.5 (the left half: the crawler's shell and rear legs).
"""
import json
import os
import sys

import numpy as np
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))


def mask(img, x0, x1, step):
    a = np.asarray(img.getchannel("A"))[::step, ::step] > 64
    w = a.shape[1]
    return a[:, int(x0 * w):int(x1 * w)]


def best_shift(ref, cur, reach):
    """(dx, dy) in mask pixels moving `cur` onto `ref`: the most overlap within `reach`."""
    best, at = -1, (0, 0)
    h, w = ref.shape
    for dy in range(-reach, reach + 1):
        for dx in range(-reach // 2, reach // 2 + 1):
            ys, yd = (slice(0, h - dy), slice(dy, h)) if dy >= 0 else (slice(-dy, h), slice(0, h + dy))
            xs, xd = (slice(0, w - dx), slice(dx, w)) if dx >= 0 else (slice(-dx, w), slice(0, w + dx))
            score = np.count_nonzero(ref[yd, xd] & cur[ys, xs]) - np.count_nonzero(ref[yd, xd] ^ cur[ys, xs]) // 2
            if score > best:
                best, at = score, (dx, dy)
    return at


def main():
    result_path, clip, suffix = sys.argv[1], sys.argv[2], sys.argv[3]
    x0, x1 = (float(sys.argv[4]), float(sys.argv[5])) if len(sys.argv) > 5 else (0.0, 0.5)
    result = json.load(open(result_path, encoding="utf-8"))
    r = result["clips"][clip]
    pattern = os.path.join(ROOT, r["pattern"])
    old_dir = os.path.dirname(pattern)
    name = os.path.basename(old_dir)
    new_dir = os.path.join(os.path.dirname(old_dir), name.rsplit("-", 1)[0] + "-" + suffix)
    if os.path.exists(new_dir):
        raise SystemExit(f"{new_dir} already exists")
    os.makedirs(new_dir)
    frames = [Image.open(pattern.replace("{}", str(i))).convert("RGBA") for i in range(1, r["frames"] + 1)]
    W, H = frames[0].size
    step = 2 if W > 800 else 1
    ref = mask(frames[0], x0, x1, step)
    reach = max(4, int(0.06 * H / step))
    union = None
    shifts = []
    for i, f in enumerate(frames, start=1):
        dx, dy = best_shift(ref, mask(f, x0, x1, step), reach)
        dx, dy = dx * step, dy * step
        # a finer pass at full size round the coarse answer
        if step > 1:
            fine_ref = mask(frames[0], x0, x1, 1)
            cur = mask(f, x0, x1, 1)
            ddx, ddy = best_shift(fine_ref, np.roll(np.roll(cur, dy, 0), dx, 1), 2)
            dx, dy = dx + ddx, dy + ddy
        shifts.append((dx, dy))
        out = Image.new("RGBA", (W, H), (0, 0, 0, 0))
        out.paste(f, (dx, dy), f)
        out.save(os.path.join(new_dir, os.path.basename(pattern).replace("{}", str(i))))
        b = out.getchannel("A").getbbox()
        if b:
            union = b if union is None else (min(union[0], b[0]), min(union[1], b[1]), max(union[2], b[2]), max(union[3], b[3]))
    r["pattern"] = os.path.relpath(os.path.join(new_dir, os.path.basename(pattern)), ROOT).replace("\\", "/")
    r["crop"] = [union[0], union[1], union[2] - union[0], union[3] - union[1]]
    r["steadied"] = {"from": os.path.relpath(old_dir, ROOT).replace("\\", "/"), "columns": [x0, x1],
                     "why": "the video's camera drifted (the user, 2026-11-11): each frame moved onto frame 1's still part"}
    with open(result_path, "w", encoding="utf-8") as f:
        json.dump(result, f, indent=1, ensure_ascii=False)
        f.write("\n")
    ys = [s[1] for s in shifts]
    print(f"{clip}: {len(frames)} frames -> {r['pattern']}; dy {min(ys)}..{max(ys)}, dx {min(s[0] for s in shifts)}..{max(s[0] for s in shifts)}")
    print("dy by frame:", ys)


if __name__ == "__main__":
    main()
