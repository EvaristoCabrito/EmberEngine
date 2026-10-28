"""A clip whose frames slide sideways (the feet leave their spot while the body moves) registered back onto frame 1's
feet: each frame's legs band (the lowest 30 % of the frame) is matched against frame 1's by the alpha's best horizontal
overlap, and the whole frame shifted by that much. Frame sizes, timing and the art itself stay; only where each picture
sits in its frame changes. Writes a new version beside the original (never over it) and prints the shifts and the new
union crop box.

The user, 2026-10-28, of Neera's restored dagger strike (neera-v2-001 atk-short): "fix the frames for this dagger
animation because they are off set" (frames 11-29 slid up to 81 px left while the thrust goes right).

    python -I tools/scripts/sprites/register_clip_feet.py <frames dir> <clip> <count> <out dir>
"""
import os
import sys

import numpy as np
from PIL import Image


def legs(path, top, bottom):
    a = np.array(Image.open(path).convert("RGBA"))[:, :, 3].astype(np.float32) / 255.0
    return a[top:bottom]


def main():
    src, clip, count, out = sys.argv[1], sys.argv[2], int(sys.argv[3]), sys.argv[4]
    if os.path.exists(out) and os.listdir(out):
        sys.exit(f"refused: {out} is not empty")
    os.makedirs(out, exist_ok=True)
    paths = [os.path.join(src, f"{clip}-{i}.png") for i in range(1, count + 1)]
    h = Image.open(paths[0]).height
    top, bottom = int(h * 0.70), int(h * 0.99)
    ref = legs(paths[0], top, bottom)
    union = None
    shifts = []
    for i, p in enumerate(paths, 1):
        band = legs(p, top, bottom)
        shift = max(range(-150, 151), key=lambda s: float((np.roll(band, s, axis=1) * ref).sum()))
        shifts.append(shift)
        im = Image.open(p).convert("RGBA")
        moved = Image.new("RGBA", im.size, (0, 0, 0, 0))
        moved.paste(im, (shift, 0))
        lost = np.array(im)[:, :, 3] > 0
        kept = np.array(moved)[:, :, 3] > 0
        if lost.sum() != kept.sum():
            sys.exit(f"frame {i}: a shift of {shift} px would cut the picture at the frame's edge")
        moved.save(os.path.join(out, f"{clip}-{i}.png"))
        ys, xs = np.nonzero(kept)
        box = (xs.min(), ys.min(), xs.max() + 1, ys.max() + 1)
        union = box if union is None else (min(union[0], box[0]), min(union[1], box[1]), max(union[2], box[2]), max(union[3], box[3]))
    print("shifts", shifts)
    print("crop", [int(union[0]), int(union[1]), int(union[2] - union[0]), int(union[3] - union[1])])


if __name__ == "__main__":
    main()
