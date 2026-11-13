"""Garran Holt's temporary sprites (the user's own quick sheets, content/sprites/moonwarden-v01/human: 4 frames a clip, 384 px, alpha
as drawn) into a sprite set the game draws: writes content/sprites/moonwarden-v01/cut.json (the clips, their poses and modes) and
cut-result.json (per clip: frames, frame time, size, crop, box, impact) in the format tools/scripts/sprites/video_sprite.py writes, the
frames used where they are (no copy, no resampling). Then content/sprite-overrides.json's "moonwarden-v01" set makes them the hero's.

Scale: the idle's median figure (top of the head to the feet, alpha > 128) stands figureUnits tall (Kael: 2.66); every clip uses the
idle's scale, so a crouch or a fall stays smaller. The feet line of each clip is its lowest solid row.

    python -I tools/scripts/sprites/moonwarden_temp_set.py
"""
import glob
import json
import os
import statistics

from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
BASE = "content/sprites/moonwarden-v01"
FIGURE_UNITS = 2.66
# clip name in the set, the folder, the pose, its mode, the frame time, the impact share (strikes)
CLIPS = [
    ("base", "idle", "idle", "pingpong", 180, None),
    ("move", "walk", "walk", "loop", 120, None),
    ("move-left", "walk-left", "walkLeft", "loop", 120, None),
    ("atk", "attack", "attack", "once", 110, 0.6),
    ("atk-left", "attack-left", "attackLeft", "once", 110, 0.6),
    ("special", "special", "specialAttack", "once", 120, 0.6),
    ("block", "block", "block", "once", 130, None),
    ("hit", "hit", "hit", "once", 110, None),
    ("fall", "fall", "death", "once", 160, None),
]


def solid_box(path):
    with Image.open(path) as im:
        return im.size, im.getchannel("A").point(lambda v: 255 if v > 128 else 0).getbbox()


def main():
    frames = {}
    for clip, folder, *_ in CLIPS:
        files = sorted(glob.glob(os.path.join(ROOT, BASE, "human", folder, "*.png")))
        if not files or len(files) > 9:
            raise SystemExit(f"{folder}: {len(files)} frames (1-9 expected: the pattern is 0N.png)")
        frames[clip] = files
    idle = [solid_box(f)[1] for f in frames["base"]]
    figure_px = statistics.median(b[3] - b[1] for b in idle)
    spec = {"about": "Garran Holt's temporary sprites (the user, 2026-11-13: quick temp sheets, 4 frames a clip; tools/scripts/sprites/"
                     "moonwarden_temp_set.py). Human form only.",
            "set": "moonwarden-v01", "version": "v01", "figureUnits": FIGURE_UNITS, "clips": []}
    result = {"clips": {}}
    for clip, folder, pose, mode, ms, impact in CLIPS:
        sizes, boxes = zip(*(solid_box(f) for f in frames[clip]))
        w, h = sizes[0]
        x0, y0 = min(b[0] for b in boxes), min(b[1] for b in boxes)
        x1, y1 = max(b[2] for b in boxes), max(b[3] for b in boxes)
        k = FIGURE_UNITS / figure_px
        spec["clips"].append({"clip": clip, "pose": pose, "kind": "walk" if pose.startswith("walk") else "stand", "mode": mode,
                              "frames": f"{BASE}/human/{folder}"})
        r = {"video": f"{BASE}/human/{folder}", "frames": len(frames[clip]), "frameMs": float(ms), "bootRow": y1 - 1, "figurePx": figure_px,
             "nat": [w, h], "crop": [x0, y0, x1 - x0, y1 - y0],
             "box": {"w": round(w * k, 5), "h": round(h * k, 5), "foot": round((h - y1) * k, 5)},
             "pattern": f"{BASE}/human/{folder}/0{{}}.png"}
        if impact is not None:
            r["impact"] = impact
        result["clips"][clip] = r
        print(f"{clip}: {len(frames[clip])} frames, {ms} ms, box {r['box']}")
    for name, data in (("cut.json", spec), ("cut-result.json", result)):
        with open(os.path.join(ROOT, BASE, name), "w", encoding="utf-8", newline="\n") as f:
            json.dump(data, f, ensure_ascii=False, indent=1)
            f.write("\n")


if __name__ == "__main__":
    main()
