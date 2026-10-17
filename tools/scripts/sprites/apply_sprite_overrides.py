"""Clips whose art EmberEngine replaces or adds (content/sprite-overrides.json; an entry with "add" is a clip and poses
Engine2 does not have, appended once), applied to the files built from Engine2's:

- build/sprites/battle.sprites.json: the clip's frames point at the new art (absolute paths, so the Cooker's Engine2 root
  does not apply), its fps from the art's frame time;
- build/engine2-scenes/roster.json: each listed pose gets the art's size, crop, box, frame count and a pace at the art's
  own frame time (240 Hz samples, like export_heroes.ts);
- content/engine2/walk-strides.json and its copy beside the roster: the strides the new clip holds.

Run it after fx_sprites.py (which rewrites the manifest) and export_heroes.ts (which rewrites the heroes file), then cook
battle.pak again. Idempotent.

    python -I tools/scripts/sprites/apply_sprite_overrides.py
"""
import json
import math
import os

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))


def load(path):
    with open(path, encoding="utf-8") as f:
        return json.load(f)


def save(path, data, indent):
    with open(path, "w", encoding="utf-8") as f:
        json.dump(data, f, indent=indent, ensure_ascii=False)
        if indent is not None:
            f.write("\n")


def main():
    over = load(os.path.join(ROOT, "content", "sprite-overrides.json"))
    manifest_path = os.path.join(ROOT, "build", "sprites", "battle.sprites.json")
    roster_path = os.path.join(ROOT, "build", "engine2-scenes", "roster.json")
    manifest = load(manifest_path)
    roster = load(roster_path)
    strides_paths = [os.path.join(ROOT, "content", "engine2", "walk-strides.json"),
                     os.path.join(ROOT, "build", "engine2-scenes", "walk-strides.json")]
    strides = [load(p) if os.path.exists(p) else None for p in strides_paths]

    for c in over["clips"]:
        pattern = os.path.join(ROOT, c["frames"]["pattern"]).replace("\\", "/")
        count = c["frames"]["count"]
        for i in range(1, count + 1):
            if not os.path.exists(pattern.replace("{}", str(i))):
                raise SystemExit(f"missing frame {pattern.replace('{}', str(i))}")
        fps = 1000.0 / c["frameMs"]
        hit = 0
        for s in manifest["sets"]:
            if s["name"] != c["set"]:
                continue
            for clip in s["clips"]:
                if clip["name"] == c["clip"]:
                    clip["frames"] = {"count": count, "pattern": pattern}
                    clip["fps"] = fps
                    hit += 1
            if not hit and "add" in c:  # a clip Engine2 does not have (EmberEngine's own art)
                s["clips"].append({"fps": fps, "frames": {"count": count, "pattern": pattern}, "mode": c["add"]["mode"],
                                   "name": c["clip"], "pack": c["add"]["pack"]})
                hit += 1
        if not hit:
            raise SystemExit(f"{c['set']}/{c['clip']}: not in {manifest_path}")
        # the pace: frame i for the art's frame time, one loop sampled at 240 Hz
        ticks = round(count * c["frameMs"] * 240 / 1000)
        pace = [min(count - 1, math.floor(t * 1000 / (240 * c["frameMs"]))) for t in range(ticks)]
        for want in c["poses"]:
            done = 0
            for h in roster["heroes"]:
                if h.get("key") != want["hero"]:
                    continue
                for p in h["poses"]:
                    if p["pose"] == want["pose"] and p["set"] == c["set"] and p["clip"] == c["clip"]:
                        p.update(box=want["box"], nat=want["nat"], crop=want["crop"], frames=count, pace=pace, doneAt=-1)
                        done += 1
                if not done and "add" in c:  # a pose Engine2 does not have
                    h["poses"].append({"pose": want["pose"], "label": want.get("label", want["pose"]), "set": c["set"],
                                       "clip": c["clip"], "box": want["box"], "nat": want["nat"], "crop": want["crop"],
                                       "frames": count, "pace": pace, "doneAt": -1})
                    done += 1
            if not done:
                raise SystemExit(f"{want['hero']} {want['pose']}: not in {roster_path}")
        for table in strides:
            if table is not None and "strides" in c:
                table[f"{c['set']}/{c['clip']}"] = c["strides"]
        print(f"{c['set']}/{c['clip']} -> {c['version']}: {count} frames at {c['frameMs']} ms, {len(c['poses'])} pose(s)")

    save(manifest_path, manifest, 1)
    save(roster_path, roster, None)
    for p, table in zip(strides_paths, strides):
        if table is not None:
            save(p, dict(sorted(table.items())), 1)


if __name__ == "__main__":
    main()
