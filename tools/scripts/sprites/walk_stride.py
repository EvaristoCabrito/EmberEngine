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


def strides(result, clip):
    """(strides, step frames, loop frames), strides None when no step shows (a float, a glide)."""
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
