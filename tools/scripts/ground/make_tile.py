"""Makes a seamless square ground tile from a photo (the user's top-down ground art), for the board's ground layers.

  python -I tools/scripts/ground/make_tile.py <photo> <out.png> [--size N] [--band PX] [--seed S] [--preview 2x2.png]

The square is cut from the photo's middle at its own pixels (no resampling). Along each axis in turn, the tile's two
borders are taken from the square rolled by half its size (whose middle is continuous across the wrap), joined to it
along the cut of least difference with a two-pixel feather, so grass blades are never cross-faded over a wide band.
"""

import argparse
import sys

import numpy as np
from PIL import Image


def min_cut(error):
    """Column per row of the cheapest top-to-bottom path through `error` (moves of -1, 0, +1 per row), starting and
    ending on the same column (the cheapest column on average), so the cut wraps from the last row to the first."""
    h, w = error.shape
    start = int(np.argmin(error.mean(axis=0)))
    cost = np.full(w, np.inf)
    cost[start] = error[0, start]
    back = np.zeros((h, w), dtype=np.int64)
    for y in range(1, h):
        left = np.concatenate(([np.inf], cost[:-1]))
        right = np.concatenate((cost[1:], [np.inf]))
        options = np.stack((left, cost, right))
        pick = np.argmin(options, axis=0)
        back[y] = np.arange(w) + pick - 1
        cost = options[pick, np.arange(w)] + error[y]
    path = np.empty(h, dtype=np.int64)
    path[-1] = start
    for y in range(h - 1, 0, -1):
        path[y - 1] = back[y, path[y]]
    return path


def main():
    p = argparse.ArgumentParser()
    p.add_argument("photo")
    p.add_argument("out")
    p.add_argument("--size", type=int, default=0, help="square side in pixels (default: the photo's shorter side)")
    p.add_argument("--band", type=int, default=110, help="the cut lies within twice this many pixels of each border")
    p.add_argument("--preview", default="")
    a = p.parse_args()

    src = np.asarray(Image.open(a.photo).convert("RGB"), dtype=np.float32)
    h, w, _ = src.shape
    n = a.size or min(h, w)
    n -= n % 4  # BC7 blocks
    y0, x0 = (h - n) // 2, (w - n) // 2
    out = src[y0:y0 + n, x0:x0 + n]

    # One axis at a time (a roll on both at once leaves the rolled copy's own seams crossing the border bands):
    # the copy rolled by half along that axis covers the two borders across it; its own seam lies in the middle, unused.
    # Where the two meet, the cut follows the path of least difference (image quilting's minimum error boundary), so it
    # runs between grass blades and around dirt patches instead of along a line; it starts and ends at the same place,
    # so it also wraps along the other axis.
    for axis in (1, 0):
        work = out if axis == 1 else out.transpose(1, 0, 2)
        rolled = np.roll(work, n // 2, axis=1)
        error = ((work - rolled) ** 2).sum(axis=2)
        mask = np.ones((n, n), dtype=np.float32)
        for lo, hi, left in ((8, 2 * a.band, True), (n - 2 * a.band, n - 8, False)):
            cut = min_cut(error[:, lo:hi]) + lo
            cols = np.arange(n)[None, :]
            mask *= (cols > cut[:, None]) if left else (cols < cut[:, None])
        # a two-pixel feather across the cut
        k = np.array([0.25, 0.5, 0.25], dtype=np.float32)
        for _ in range(2):
            mask = np.apply_along_axis(lambda r: np.convolve(np.pad(r, 1, mode="edge"), k, mode="valid"), 1, mask)
        blended = work * mask[..., None] + rolled * (1.0 - mask[..., None])
        out = blended if axis == 1 else blended.transpose(1, 0, 2)

    # seam check: the wrap's step vs an ordinary step between neighbouring columns / rows
    def step(a_, b_):
        return float(np.abs(a_ - b_).mean())

    wrap = (step(out[:, 0], out[:, -1]) + step(out[0], out[-1])) / 2
    inner = (step(out[:, n // 3], out[:, n // 3 + 1]) + step(out[n // 3], out[n // 3 + 1])) / 2
    print(f"tile {n}x{n} from {w}x{h} at ({x0}, {y0}); wrap step {wrap:.2f} vs inner step {inner:.2f}")
    if wrap > inner * 1.5:
        print("seam check FAILED", file=sys.stderr)
        return 1

    Image.fromarray(np.clip(out + 0.5, 0, 255).astype(np.uint8)).save(a.out)
    if a.preview:
        Image.fromarray(np.clip(np.tile(out, (2, 2, 1)) + 0.5, 0, 255).astype(np.uint8)).save(a.preview)
    print(f"wrote {a.out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
