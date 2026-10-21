"""A new version of a decoration PNG without the red fringe its cut-out left on the soft edge (the user, 2026-10-21: "Limpa as
bordas vermelhas das 9 peças novas como uma versão nova, sem apagar as atuais").

Only the edge is touched: a pixel under --solid alpha (default 128) whose red cast (red minus the larger of green and blue) is
more than --margin (default 20) above the nearest solid pixel's takes that solid pixel's colour; clear pixels take it too (they
show nowhere, only in image viewers). Alpha stays; every solid pixel stays as drawn (rust, embers). The fringe is nearly clear
(alpha 1-12), but the sprite cooker pushes edge colours into the clear pixels around the art, so in the game it reads as a red
outline.

    python -I tools/scripts/sprites/defringe_red.py <in.png> <out.png> [--solid 128] [--margin 20]

Never writes over an existing file. Prints how many pixels changed; 0 = nothing written.
"""
import argparse
import os
import sys

import numpy as np
from PIL import Image
from scipy import ndimage


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("src")
    ap.add_argument("out")
    ap.add_argument("--solid", type=int, default=128)
    ap.add_argument("--margin", type=float, default=20)
    args = ap.parse_args()
    if os.path.exists(args.out):
        sys.exit(f"{args.out} exists: never written over")
    rgba = np.array(Image.open(args.src).convert("RGBA"))
    c = rgba[..., :3].astype(np.float32)
    a = rgba[..., 3]
    solid = a >= args.solid
    _, (iy, ix) = ndimage.distance_transform_edt(~solid, return_indices=True)
    near = c[iy, ix]
    cast = c[..., 0] - np.maximum(c[..., 1], c[..., 2])
    near_cast = near[..., 0] - np.maximum(near[..., 1], near[..., 2])
    fringe = (a > 0) & ~solid & (cast > near_cast + args.margin)
    if not fringe.any():
        print(f"{args.src}: no red fringe, nothing written")
        return
    out = rgba.copy()
    out[fringe | (a == 0), :3] = rgba[iy, ix, :3][fringe | (a == 0)]
    Image.fromarray(out).save(args.out)
    print(f"{args.out}: {int(fringe.sum())} edge pixels recoloured")


if __name__ == "__main__":
    main()
