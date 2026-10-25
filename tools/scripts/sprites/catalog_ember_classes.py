"""EmberEngine's own unit classes (content/ember-classes.json) into the Map Editor's catalog (content/engine2/editor-catalog.json), so
they can be placed on a map and the scene builder draws them: the class's art (its catalog entry's idle pose in
build/engine2-scenes/roster.json: set, clip, box, size, frames, pace; a two-hex body centred over its two hexes), its name and role
in the editor's class list, its sprite; and EmberEngine's own heroes (newHero entries of content/sprite-overrides.json with a
classId) into the editor's hero list. Idempotent; run after apply_sprite_overrides.py (which makes the roster entries) and again
after export_editor_catalog.ts rewrites the catalog.

    python -I tools/scripts/sprites/catalog_ember_classes.py
"""
import json
import os

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))


def main():
    classes = json.load(open(os.path.join(ROOT, "content", "ember-classes.json"), encoding="utf-8"))["classes"]
    roster = json.load(open(os.path.join(ROOT, "build", "engine2-scenes", "roster.json"), encoding="utf-8"))
    path = os.path.join(ROOT, "content", "engine2", "editor-catalog.json")
    catalog = json.load(open(path, encoding="utf-8"))
    editor = catalog["editor"]
    for key, c in classes.items():
        hero = next((h for h in roster["heroes"] if h.get("key") == key), None)
        idle = next((p for p in (hero or {}).get("poses", []) if p["pose"] == "idle"), None)
        if idle is None:
            raise SystemExit(f"{key}: no idle pose in the roster (run apply_sprite_overrides.py first)")
        art = {"key": key, "sprite": c["sprite"], "set": idle["set"], "clip": idle["clip"], "box": idle["box"], "nat": idle["nat"],
               "frames": idle["frames"], "pace": idle["pace"], "reversed": bool(hero.get("reversed", False))}
        if c.get("size", 1) == 2:
            art["front"] = [0, 1]  # the body = its hex and the one to its right; the card stands between them
        catalog["classes"][key] = art
        editor["classes"][key] = {"name": c["name"], "role": c.get("role", ""), "summon": False}
        if key not in editor["classOrder"]:
            editor["classOrder"].append(key)
        editor["classSprite"][key] = c["sprite"]
        print(f"{key}: {c['name']} ({idle['set']}/{idle['clip']}, box {idle['box']}, size {c.get('size', 1)})")
    # EmberEngine's own heroes (content/sprite-overrides.json sets whose newHero is a "hero" with a classId; Isolde, 2026-10-24): placed
    # by name as a player spawn (the editor's hero list), drawn with their idle, playing by their class's rules
    overrides = json.load(open(os.path.join(ROOT, "content", "sprite-overrides.json"), encoding="utf-8"))
    for st in overrides.get("sets", []):
        nh = st.get("newHero")
        if not nh or nh.get("group") != "hero" or "classId" not in nh:
            continue
        hero = next((h for h in roster["heroes"] if h.get("key") == st["hero"]), None)
        idle = next((p for p in (hero or {}).get("poses", []) if p["pose"] == "idle"), None)
        if idle is None:
            raise SystemExit(f"{st['hero']}: no idle pose in the roster (run apply_sprite_overrides.py first)")
        catalog["heroes"][nh["name"]] = {"classId": nh["classId"], "key": st["hero"], "sprite": nh.get("sprite", st["set"]), "set": idle["set"],
                                         "clip": idle["clip"], "box": idle["box"], "nat": idle["nat"], "frames": idle["frames"],
                                         "pace": idle["pace"], "reversed": bool(nh.get("reversed", False))}
        listed = editor["editorHeroes"]
        if not any(h["name"] == nh["name"] for h in listed):
            listed.append({"name": nh["name"], "classId": nh["classId"]})
        print(f"{nh['name']}: hero, class {nh['classId']} ({idle['set']}/{idle['clip']}, box {idle['box']})")
    text = json.dumps(catalog, ensure_ascii=False, separators=(",", ":"))
    json.loads(text)
    with open(path + ".tmp", "w", encoding="utf-8") as f:
        f.write(text)
    os.replace(path + ".tmp", path)


if __name__ == "__main__":
    main()
