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
import copy
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

    # "sets": a whole sprite set of EmberEngine's own (cut from video by tools/scripts/sprites/video_sprite.py, its
    # cut-result.json beside the cut spec), added to the manifest (replaced on a second run) and made the hero's art for the
    # listed poses; poses the set does not list keep their old art (e.g. Voss's back pose).
    for st in over.get("sets", []):
        cut_dir = os.path.dirname(os.path.join(ROOT, st["cut"]))
        cut_spec = load(os.path.join(ROOT, st["cut"]))
        cut = load(os.path.join(cut_dir, cut_spec.get("result", "cut-result.json")))["clips"]
        spec = {c["clip"]: c for c in cut_spec["clips"]}
        clips = []
        for name, r in cut.items():
            pattern = os.path.join(ROOT, r["pattern"]).replace("\\", "/")
            for i in range(1, r["frames"] + 1):
                if not os.path.exists(pattern.replace("{}", str(i))):
                    raise SystemExit(f"missing frame {pattern.replace('{}', str(i))}")
            clips.append({"fps": 1000.0 / r["frameMs"], "frames": {"count": r["frames"], "pattern": pattern},
                          "mode": spec[name]["mode"], "name": name, "pack": st["packs"].get(name, "combat")})
        manifest["sets"] = [s for s in manifest["sets"] if s["name"] != st["set"]] + [{"name": st["set"], "clips": clips}]
        if "mediumOf" in st:
            # a medium copy (voss-veo-media: the same clips at ~610 px of figure): the roster keeps pointing at the hero's own
            # set and each pose whose clip has a copy names it in "medium" (Opções > Gráficos > Personagens: Média draws it)
            of = st["mediumOf"]
            hero = next((h for h in roster["heroes"] if h.get("key") == of["hero"]), None)
            if hero is None:
                raise SystemExit(f"{of['hero']}: not in {roster_path}")
            marked = []
            for p in hero["poses"]:
                if p.get("set") == of["set"] and p.get("clip") in cut:
                    p["medium"] = st["set"]
                    marked.append(p["pose"])
                else:
                    p.pop("medium", None)
            print(f"{st['set']} {st['version']}: {len(clips)} clips, the medium copy of {of['hero']}'s {', '.join(marked)}")
            continue
        hero = next((h for h in roster["heroes"] if h.get("key") == st["hero"]), None)
        if hero is None and "newHero" in st:
            # a unit of EmberEngine's own (the Corrosive Slime, 2026-10-22): its catalog entry made here, its poses below
            nh = st["newHero"]
            hero = {"name": nh["name"], "key": st["hero"], "group": nh.get("group", "monster"), "sprite": nh.get("sprite", st["set"]),
                    "reversed": bool(nh.get("reversed", False)), "poses": []}
            roster["heroes"].append(hero)
        if hero is None:
            raise SystemExit(f"{st['hero']}: not in {roster_path}")
        for pose, name in st["poses"].items():
            r = cut[name]
            per = r["frameMs"] * 240 / 1000  # ticks a frame (10 at 24 fps)
            pace = [min(r["frames"] - 1, math.floor(t / per)) for t in range(round(r["frames"] * per))]
            fields = dict(set=st["set"], clip=name, box=r["box"], nat=r["nat"], crop=r["crop"], frames=r["frames"], pace=pace,
                          doneAt=-1)
            if "impact" in r:
                fields["impact"] = r["impact"]
            p = next((p for p in hero["poses"] if p["pose"] == pose), None)
            if p is None:
                p = {"pose": pose, "label": st.get("labels", {}).get(pose, pose)}
                hero["poses"].append(p)
            p.pop("bodyShift", None)  # Engine2's per-frame shift belonged to its own art
            p.pop("impact", None)
            p.update(fields)
        for table in strides:
            if table is not None:
                for name, n in st.get("strides", {}).items():
                    table[f"{st['set']}/{name}"] = n
        print(f"{st['set']} {st['version']}: {len(clips)} clips, {st['hero']}'s poses {', '.join(st['poses'])}")

    # "copies" (after every set, so a medium copy's marks come along): another unit drawn with the same art at `scale` (the Large
    # Corrosive Slime: two hexes, the Small one's art twice as big); its poses are the hero's, each box (width, height, foot) scaled.
    for st in over.get("sets", []):
        for cp in st.get("copies", []):
            src = next((h for h in roster["heroes"] if h.get("key") == st["hero"]), None)
            if src is None:
                raise SystemExit(f"{st['hero']}: not in {roster_path} (copy {cp['key']})")
            dst = next((h for h in roster["heroes"] if h.get("key") == cp["key"]), None)
            if dst is None:
                dst = {"key": cp["key"]}
                roster["heroes"].append(dst)
            dst.update(name=cp["name"], group=src.get("group", "monster"), sprite=src.get("sprite"), reversed=src.get("reversed", False))
            dst["poses"] = copy.deepcopy(src["poses"])
            for p in dst["poses"]:
                p["box"] = {k: round(v * cp["scale"], 5) for k, v in p["box"].items()}
            print(f"{cp['key']}: {st['hero']}'s {len(dst['poses'])} poses at x{cp['scale']}")

    # "speed": a pose's own clip played faster (its pace sampled `factor` times as fast, so the pose is that much shorter in
    # the fight); marked on the pose so a second run does nothing.
    for sp in over.get("speed", []):
        for h in roster["heroes"]:
            if h.get("key") != sp["hero"]:
                continue
            for p in h["poses"]:
                if p["pose"] not in sp["poses"] or p.get("speed") == sp["factor"]:
                    continue
                old = p["pace"]
                p["pace"] = [old[min(len(old) - 1, round(i * sp["factor"]))] for i in range(math.ceil(len(old) / sp["factor"]))]
                if p.get("doneAt", -1) >= 0:
                    p["doneAt"] = math.ceil(p["doneAt"] / sp["factor"])
                p["speed"] = sp["factor"]
                print(f"{sp['hero']} {p['pose']}: {sp['factor']}x as fast ({len(old)} -> {len(p['pace'])} ticks)")

    save(manifest_path, manifest, 1)
    save(roster_path, roster, None)
    for p, table in zip(strides_paths, strides):
        if table is not None:
            save(p, dict(sorted(table.items())), 1)


if __name__ == "__main__":
    main()
