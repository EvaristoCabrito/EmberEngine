"""A still pose from one picture of the user's (e.g. a hero seen from the back), sized like the hero's Engine2 art.

The picture is cut to the figure (full width, the figure's top to its feet plus a small margin), scaled so the figure is
`--scale` times as many pixels tall as in the hero's Engine2 idle frame (sharper than the clips, same size in the world),
and written as frame 1 of a one-frame clip. It prints the pose's box (world units: Engine2's own units a pixel for that
hero, so the figure stands as tall as in its idle, feet on the same line), the frame's size and its crop, for
content/sprite-overrides.json.

    python -I tools/scripts/sprites/prepare_still_pose.py <picture.png> <engine2 idle frame.png> <engine2 box h> <out.png>
        [--scale 1.5]

Heights are measured head to feet: the topmost opaque pixel over the middle fifth of the picture's width (the head, not a
staff held to one side) down to the lowest opaque pixel.
"""
import argparse
import json

import numpy as np
from PIL import Image

OPAQUE = 60


def head_and_feet(alpha):
    w, h = alpha.size
    mask = alpha.point(lambda v: 255 if v > OPAQUE else 0)
    box = mask.getbbox()
    feet = box[3]
    x0, x1 = (box[0] + box[2]) // 2 - w // 10, (box[0] + box[2]) // 2 + w // 10
    band = mask.crop((max(0, x0), 0, min(w, x1), h)).getbbox()
    return band[1], feet, box


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("picture")
    ap.add_argument("idle")
    ap.add_argument("idle_box_h", type=float)
    ap.add_argument("out")
    ap.add_argument("--scale", type=float, default=1.5)
    a = ap.parse_args()

    idle = Image.open(a.idle).convert("RGBA")
    units_per_px = a.idle_box_h / idle.height  # Engine2's k for this hero
    i_head, i_feet, _ = head_and_feet(idle.getchannel("A"))
    world_figure = (i_feet - i_head) * units_per_px

    pic = Image.open(a.picture).convert("RGBA")
    p_head, p_feet, p_box = head_and_feet(pic.getchannel("A"))
    margin = 8
    top, bottom = max(0, p_box[1] - margin), min(pic.height, p_feet + margin)
    cut = pic.crop((0, top, pic.width, bottom))
    target_figure = round((i_feet - i_head) * a.scale)
    s = target_figure / (p_feet - p_head)
    size = (round(cut.width * s), round(cut.height * s))
    # premultiplied resample, so the transparent surround's colour does not bleed into the edges
    arr = np.asarray(cut).astype(np.float32) / 255.0
    arr[..., :3] *= arr[..., 3:4]
    chans = [Image.fromarray(arr[..., c]).resize(size, Image.Resampling.LANCZOS) for c in range(4)]
    out = np.stack([np.asarray(c) for c in chans], axis=-1)
    out = np.clip(out, 0.0, 1.0)
    alpha = out[..., 3:4]
    out[..., :3] = np.where(alpha > 1e-4, out[..., :3] / np.maximum(alpha, 1e-4), 0.0)
    img = Image.fromarray((np.clip(out, 0.0, 1.0) * 255.0 + 0.5).astype(np.uint8), "RGBA")
    img.save(a.out)

    k = world_figure / ((p_feet - p_head) * s)
    feet_px = (p_feet - top) * s
    bb = img.getchannel("A").point(lambda v: 255 if v > OPAQUE else 0).getbbox()
    print(json.dumps({
        "nat": [img.width, img.height],
        "crop": [bb[0], bb[1], bb[2] - bb[0], bb[3] - bb[1]],
        "box": {"w": round(img.width * k, 5), "h": round(img.height * k, 5), "foot": round((img.height - feet_px) * k, 5)},
        "figure": {"idlePx": i_feet - i_head, "worldUnits": round(world_figure, 4), "outPx": round((p_feet - p_head) * s)},
    }))


if __name__ == "__main__":
    main()
