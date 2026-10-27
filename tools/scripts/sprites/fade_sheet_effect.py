"""A sheet's spell effect faded out away from the caster's hand, so it never reaches the frame's square edges.

The user, 2026-10-27, on Selene's cast (Attachments/Selene/Casting, frames 25-33: the burst flies out of the frame and is cut
straight at its edges, frame 26 the worst): "apagar certinho para não ter bordas, só o efeito próximo da mão dela continuaria".
In the listed frames each pixel outside the figure keeps its alpha within `r1` px of the hand, fades smoothly to nothing at
`r2` (smoothstep) and is cleared beyond; the figure (alpha > 20 in the `body` frames, before and after the burst, where she
stands the same) keeps every pixel as drawn. Colour untouched, nothing resampled; the source sheet is never written.

    python -I tools/scripts/sprites/fade_sheet_effect.py <sheet.png> <out.png> --frames 25-33 --hand 317 374 --r1 140 --r2 290
        --body 24,34
"""
import argparse
import json
import os
import shutil

import numpy as np
from PIL import Image


def frame_list(text):
    out = []
    for part in text.split(","):
        a, _, b = part.partition("-")
        out += list(range(int(a), int(b or a) + 1))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("sheet")
    ap.add_argument("out")
    ap.add_argument("--frames", required=True)  # counted from 1, as the user counts them
    ap.add_argument("--hand", nargs=2, type=float, required=True)  # x y inside a frame
    ap.add_argument("--r1", type=float, required=True)
    ap.add_argument("--r2", type=float, required=True)
    ap.add_argument("--body", required=True)
    args = ap.parse_args()
    if os.path.abspath(args.out) == os.path.abspath(args.sheet):
        raise SystemExit("refusing to write over the source sheet")
    meta = json.load(open(os.path.splitext(args.sheet)[0] + ".json"))
    rects = [v["frame"] for _, v in sorted(meta["frames"].items())]
    px = np.array(Image.open(args.sheet).convert("RGBA"))

    def view(n):
        r = rects[n - 1]
        return px[r["y"]:r["y"] + r["h"], r["x"]:r["x"] + r["w"]]

    body = np.zeros(view(1).shape[:2], bool)
    for n in frame_list(args.body):
        body |= view(n)[..., 3] > 20
    h, w = body.shape
    ys, xs = np.mgrid[0:h, 0:w]
    d = np.hypot(xs - args.hand[0], ys - args.hand[1])
    t = np.clip((args.r2 - d) / (args.r2 - args.r1), 0.0, 1.0)
    keep = np.where(body, 1.0, t * t * (3.0 - 2.0 * t))
    for n in frame_list(args.frames):
        f = view(n)
        f[..., 3] = np.round(f[..., 3] * keep).astype(np.uint8)
        edges = max(f[0, :, 3].max(), f[-1, :, 3].max(), f[:, 0, 3].max(), f[:, -1, 3].max())
        print(f"frame {n}: alpha on the frame's edges {edges}")
    os.makedirs(os.path.dirname(os.path.abspath(args.out)), exist_ok=True)
    Image.fromarray(px).save(args.out)
    shutil.copyfile(os.path.splitext(args.sheet)[0] + ".json", os.path.splitext(args.out)[0] + ".json")


if __name__ == "__main__":
    main()
