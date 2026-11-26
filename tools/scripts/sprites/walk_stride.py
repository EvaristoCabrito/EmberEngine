"""How many full strides a cut walk loop holds (content/data/walk-strides.json, sprite-overrides "strides"), read off its frames.

The legs (the lower ~38 % of the clip's frame, alpha) compared with themselves k frames later around the loop: the likeness dips
first after one step (the other foot forward, the legs' outline nearly the same), so that first dip is the step's length p in frames
and the loop holds n / p steps, n / (2 p) full strides, rounded to a half as the values set by hand. Checked 2026-11-13 against
those: Kael's walk 110 / 22 / 2 = 2.5, his walk left 50 / 25 / 2 = 1.0, Isolde's walk 130 / 21.5 / 2 = 3.0.

    python -I tools/scripts/sprites/walk_stride.py <cut-result.json> <clip> [<clip> ...]
"""
import json
import os
import sys

import numpy as np
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))


def front_back(result, clip):
    """A walk toward / away from the camera (the clip's name ends in -up / -down): the legs overlap, so the outline of the legs repeats after a
    whole stride, not after a step, and the first dip below reads half the true count (Vespera, 2026-11-25: walk up read 2.0, it is 3.5,
    and the game played the loop 1.7x too fast). Here each foot's lowest point (left and right of the body's centre column, in the legs'
    rows) is followed over the loop and its own period is the stride. (strides, stride frames, loop frames) or None."""
    r = result["clips"][clip]
    n = r["frames"]
    pat = os.path.join(ROOT, r["pattern"])
    low = ([], [])
    for i in range(n):
        a = np.asarray(Image.open(pat.replace("{}", str(i + 1))).convert("RGBA"))[..., 3] > 128
        ys = np.nonzero(a.any(1))[0]
        top, h = ys.min(), ys.max() - ys.min()
        mid = np.nonzero(a[int(top + h * 0.45):int(top + h * 0.55)].any(0))[0]
        if not len(mid):
            return None
        cx = int((mid.min() + mid.max()) / 2)
        legs = a[int(top + h * 0.80):]
        for side, (x0, x1) in zip(low, ((0, cx), (cx, legs.shape[1]))):
            yy = np.nonzero(legs[:, x0:x1].any(1))[0]
            side.append(yy.max() if len(yy) else 0)
    found = []
    for s in low:
        s = np.asarray(s, np.float64)
        s = s - s.mean()
        ac = np.array([np.sum(s * np.roll(s, -k)) for k in range(n)])
        if ac[0] <= 0:
            return None
        ac /= ac[0]
        k = 1
        while k < n // 2 and ac[k] > 0:
            k += 1
        p = next((j for j in range(k, n // 2 + 1) if ac[j] >= ac[j - 1] and (j + 1 >= n or ac[j] >= ac[j + 1]) and ac[j] > 0.2), None)
        if p is None:
            return None
        found.append(p)
    p = sum(found) / 2.0
    return round(2.0 * n / p) / 2, round(p), n


def strides(result, clip):
    """(strides, step frames, loop frames), strides None when no step shows (a float, a glide)."""
    if clip.endswith(("-up", "-down")):
        fb = front_back(result, clip)
        if fb is not None:
            return fb
    r = result["clips"][clip]
    n = r["frames"]
    pat = os.path.join(ROOT, r["pattern"])
    f = []
    for i in range(n):
        a = np.asarray(Image.open(pat.replace("{}", str(i + 1))).convert("RGBA"))[..., 3]
        h = a.shape[0]
        f.append(np.asarray(Image.fromarray(a[int(h * 0.62):]).resize((96, 40)), np.float32) / 255.0)
    d = np.array([np.mean([np.abs(f[i] - f[(i + k) % n]).mean() for i in range(n)]) for k in range(n)])
    hi = d.max()
    k0 = next((k for k in range(1, n) if d[k] > 0.5 * hi), None)
    if k0 is None:
        return None, None, n
    for k in range(k0 + 1, n // 2 + 1):
        if d[k] <= d[k - 1] and d[k] <= d[k + 1] and d[k] < 0.9 * d[:k].max():
            return round(n / k) / 2, k, n
    return None, None, n


if __name__ == "__main__":
    res = json.load(open(sys.argv[1], encoding="utf-8"))
    for c in sys.argv[2:]:
        print(c, *strides(res, c))
