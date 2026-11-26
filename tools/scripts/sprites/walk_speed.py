"""How fast each walk clip's feet cover the ground, as the video plays (the user, 2026-11-26 3 AM: "they must move like in the videos
period"; "o resultado deve ser andar igual no vídeo"; "tem que tocar igual no vídeo e adaptar para menos frames nas resoluções baixas").

The walk videos are walks in place: the body stays put (video_sprite.py holds it) and the foot on the ground slides back under it at the
walking speed. Per frame pair, the shift that best lines up the bottom strip of the figure (the rows the planted foot is in; the foot in
the air is above them) is that foot's slide; the median over the loop is the clip's ground speed in pixels a frame. With the clip's own
timing (the roster's pace: the video's frame time) and its box (world units over the frame's pixels), that is world units a second at the
unit's size 1. The game moves the unit that fast and plays the clip at the video's own speed (MapStage.h feetSpeed / walkClock).

Every walk pose of every unit in the roster, the same way (side walks measured; the up / down walks, whose feet slide toward or away from
the camera, take their unit's side speed: the same gait). A clip whose strip cannot be read (no feet on the ground: fliers) is left out
and walks at the game's step with its frames at the video's speed.
Writes content/data/walk-speeds.json and build/scenes/walk-speeds.json ("set/clip": world units a second).

    python -I tools/scripts/sprites/walk_speed.py [--show]"""
import json
import os
import sys

import numpy as np
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
GAME = os.path.join(ROOT, "content", "assets", "game")
SIDE = ("walk", "walkLeft", "walkSide")
UPDOWN = ("walkUp", "walkDown", "walkBack", "walkFront")


def frames_of(clip):
    f = clip["frames"]
    pat = f["pattern"] if os.path.isabs(f["pattern"]) else os.path.join(GAME, f["pattern"])
    start = f.get("start", 1)
    out = []
    for k in range(f["count"]):
        p = pat.replace("{}", str(start + k))
        if not os.path.exists(p):
            p = pat.replace("{}", str(k))
        out.append(p)
    return out


def strip_profiles(paths, width=480):
    """Column profile of the bottom strip of each frame (alpha), all frames at one width."""
    al = []
    for p in paths:
        im = Image.open(p).convert("RGBA")
        a = np.asarray(im)[..., 3]
        al.append(a)
    h, w = al[0].shape
    rows = np.where(np.max(np.stack([x.max(axis=1) for x in al]), axis=0) > 64)[0]
    if len(rows) == 0:
        return None, w
    # the ground: the lowest row most frames touch; the strip: 3 % of the figure's height above it
    lows = [np.where(x.max(axis=1) > 64)[0].max() for x in al if (x.max(axis=1) > 64).any()]
    ground = int(np.median(lows))
    fig = ground - rows.min()
    top = max(0, ground - max(3, int(fig * 0.03)))
    prof = []
    for x in al:
        s = (x[top:ground + 1] > 64).sum(axis=0).astype(np.float32)
        prof.append(np.interp(np.linspace(0, w - 1, width), np.arange(w), s))
    return np.array(prof), w


def slide(prof, w, width=480):
    """Median shift (source pixels a frame) of the strip between neighbouring frames; None when the strip has no feet."""
    k = width / w
    maxs = int(width * 0.08)
    shifts = []
    for a, b in zip(prof[:-1], prof[1:]):
        if a.sum() < 3 or b.sum() < 3:
            continue
        best, bs = None, -1
        for s in range(-maxs, maxs + 1):
            if s >= 0:
                v = float(np.dot(a[s:], b[:width - s]))
            else:
                v = float(np.dot(a[:width + s], b[-s:]))
            if v > bs:
                bs, best = v, s
        shifts.append(best / k)
    if len(shifts) < 4:
        return None
    sh = np.array(shifts)
    sign = np.sign(np.median(sh)) or 1.0
    moving = sh[np.sign(sh) == sign]
    return float(np.median(np.abs(moving))) if len(moving) >= len(sh) * 0.4 else None


def main():
    show = "--show" in sys.argv
    man = {s["name"]: {c["name"]: c for c in s["clips"]} for s in
           json.load(open(os.path.join(ROOT, "build", "sprites", "battle.sprites.json"), encoding="utf-8"))["sets"]}
    roster = json.load(open(os.path.join(ROOT, "build", "scenes", "roster.json"), encoding="utf-8"))["heroes"]
    out, done = {}, {}
    for h in roster:
        side_speed = None
        poses = [p for p in h.get("poses", []) if p.get("pose") in SIDE + UPDOWN]
        for p in sorted(poses, key=lambda p: p["pose"] not in SIDE):
            key = f"{p['set']}/{p['clip']}"
            if key in done:
                if p["pose"] in SIDE and done[key]:
                    side_speed = side_speed or done[key]
                continue
            clip = man.get(p["set"], {}).get(p["clip"])
            pace = p.get("pace") or []
            if not clip or not pace:
                continue
            n = p.get("frames", clip["frames"]["count"])
            seconds_a_frame = len(pace) / 240.0 / max(1, n)
            if p["pose"] in SIDE:
                paths = frames_of(clip)
                prof, w = strip_profiles(paths)
                px = slide(prof, w) if prof is not None else None
                if px is None:
                    done[key] = None
                    if show:
                        print(f"{h['key']:<22} {p['pose']:<9} {key:<44} no feet on the ground: left out")
                    continue
                units = px * p["box"]["w"] / w / seconds_a_frame
                done[key] = units
                side_speed = side_speed or units
                out[key] = round(units, 4)
                if show:
                    print(f"{h['key']:<22} {p['pose']:<9} {key:<44} {px:6.2f} px a frame, {1 / seconds_a_frame:5.1f} fps -> "
                          f"{units:5.2f} units/s ({1.732 / units:4.2f} s a hex)")
            elif side_speed:
                out[key] = round(side_speed, 4)
                done[key] = side_speed
                if show:
                    print(f"{h['key']:<22} {p['pose']:<9} {key:<44} the side walk's {side_speed:5.2f} units/s")
    # One speed a unit, every direction (the same gait): the fastest of its side walks (a strip read short where both feet stand
    # still reads slow, never fast); a unit read under 0.15 units/s (a still strip: placeholders drawn at 1-2 fps) is left out.
    by_unit = {}
    for h in roster:
        keys = [f"{p['set']}/{p['clip']}" for p in h.get("poses", []) if p.get("pose") in SIDE + UPDOWN]
        side = [done.get(f"{p['set']}/{p['clip']}") for p in h.get("poses", []) if p.get("pose") in SIDE]
        side = [v for v in side if v]
        if side and max(side) >= 0.15:
            for k in keys:
                by_unit[k] = round(max(side), 4)
    out = by_unit
    for path in (os.path.join(ROOT, "content", "data", "walk-speeds.json"), os.path.join(ROOT, "build", "scenes", "walk-speeds.json")):
        json.dump(dict(sorted(out.items())), open(path, "w", encoding="utf-8"), indent=1, ensure_ascii=False)
    print(len(out), "walk clips with their video's ground speed")


if __name__ == "__main__":
    main()
