"""Per-frame body drift of the poses Ember holds on their hex (units/catalog.ts centerBody: Neera's off-hand dagger sheet,
"her off-hand dagger sheet drifts sideways mid-swing: hold her body on her hex"), written into the heroes file as
`bodyShift` (a fraction of the pose's native width per frame, right of frame 1 positive), the way units/frames.ts
measureBodyShift does it: the median x of the opaque pixels in the bottom quarter of the frame (the legs; arms, a swung
weapon and a flaring cape sit higher and would skew it), relative to frame 1.

    python -I tools/scripts/data/measure_body_shift.py build/scenes/roster.json content/assets

Run it after export_heroes.ts (which rewrites the heroes file). Needs Pillow."""

import json
import os
import sys

from PIL import Image

# units/catalog.ts: poses.attackShort = { ...poses.attackShort, centerBody: true } (Neera only)
CENTER_BODY = {("neera", "attackShort")}


def centre(path):
    im = Image.open(path).convert("RGBA")
    w, h = im.size
    alpha = im.getchannel("A").load()
    top = h - h // 4  # the bottom quarter of the frame (the file's rows are upright here, flipped in the browser)
    xs = [x for y in range(top, h, 2) for x in range(w) if alpha[x, y] > 100]
    if not xs:
        return w / 2
    xs.sort()
    return xs[len(xs) >> 1]


def main():
    roster, public = sys.argv[1], sys.argv[2]
    with open(roster, encoding="utf-8") as f:
        data = json.load(f)
    done = 0
    for hero in data["heroes"]:
        for pose in hero["poses"]:
            if (hero.get("key"), pose["pose"]) not in CENTER_BODY:
                continue
            prefix_dir = pose["set"]
            prefix = pose["clip"] + "-"
            nat_w = pose["nat"][0]
            # the prepared frames (tools/prepare-sprites.mjs) when they exist, else the originals
            base = os.path.join(public, "game", "sprites-e2", prefix_dir)
            if not os.path.exists(os.path.join(base, prefix + "1.png")):
                base = os.path.join(public, "game", "sprites", prefix_dir)
            centres = []
            for i in range(1, pose["frames"] + 1):
                centres.append(centre(os.path.join(base, f"{prefix}{i}.png")))
            pose["bodyShift"] = [round((c - centres[0]) / nat_w, 5) for c in centres]
            done += 1
            print(f"{hero['key']} {pose['pose']}: {len(centres)} frames, drift {min(pose['bodyShift']):+.3f} .. {max(pose['bodyShift']):+.3f} of the width")
    with open(roster, "w", encoding="utf-8") as f:
        json.dump(data, f, ensure_ascii=False)
    print(f"bodyShift written for {done} pose(s) in {roster}")


main()
