"""Cuts a sheet of decorations (several props on one transparent image) into one PNG per prop, for decoracoes-2d/.

Each prop = a connected region of the alpha at or above --solid (0..255); the region grown by --grow pixels keeps its own pixels
(edges and their soft alpha included), everything else in its box (the neighbour props, a faint background glow) becomes clear.
Pieces are named in reading order (top to bottom, then left to right); the colour is untouched.

    python -I tools/scripts/sprites/split_decor_sheet.py <sheet.png> <out dir> <name> [<name> ...] [--solid 24] [--grow 6]

The number of names must match the number of props found (regions over 2000 pixels), or nothing is written.
"""
import argparse
import os
import sys

import numpy as np
from PIL import Image
from scipy import ndimage


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("sheet")
    ap.add_argument("out")
    ap.add_argument("names", nargs="+")
    ap.add_argument("--solid", type=int, default=24)
    ap.add_argument("--grow", type=int, default=6)
    ap.add_argument("--margin", type=int, default=4)
    args = ap.parse_args()

    rgba = np.array(Image.open(args.sheet).convert("RGBA"))
    alpha = rgba[..., 3]
    labels, count = ndimage.label(alpha >= args.solid)
    sizes = ndimage.sum(np.ones_like(alpha), labels, range(1, count + 1))
    props = [i + 1 for i, s in enumerate(sizes) if s > 2000]
    if len(props) != len(args.names):
        sys.exit(f"{args.sheet}: {len(props)} props found, {len(args.names)} names given")
    boxes = ndimage.find_objects(labels)
    props.sort(key=lambda p: (boxes[p - 1][0].start // 64, boxes[p - 1][1].start))
    disk = np.hypot(*np.mgrid[-args.grow:args.grow + 1, -args.grow:args.grow + 1]) <= args.grow
    for name, p in zip(args.names, props):
        own = ndimage.binary_dilation(labels == p, structure=disk)
        ys, xs = np.nonzero(own)
        y0, y1 = max(0, ys.min() - args.margin), min(alpha.shape[0], ys.max() + 1 + args.margin)
        x0, x1 = max(0, xs.min() - args.margin), min(alpha.shape[1], xs.max() + 1 + args.margin)
        piece = rgba[y0:y1, x0:x1].copy()
        piece[..., 3] = np.where(own[y0:y1, x0:x1], piece[..., 3], 0)
        path = os.path.join(args.out, name + ".png")
        if os.path.exists(path):
            sys.exit(f"{path} exists: never written over")
        Image.fromarray(piece).save(path)
        print(f"{path}: {x1 - x0} x {y1 - y0}")


if __name__ == "__main__":
    main()
