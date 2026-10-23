"""The game's 2D decorations = the PNGs in decoracoes-2d/ (the user's own folder, 2026-10-21: exported there to be edited by hand,
"cook back this folder with trimmed fat into the game, no dupes"). Whatever is in the folder is cooked, edited files included;
whatever was taken out of it leaves the game:

(Subfolders are never cooked: decoracoes-2d/versoes-anteriores/ keeps the versions a newer one replaced, e.g. the 8 Árido pieces
before defringe_red.py.)

- content/decor.sprites.json: one clip per PNG in the folder (clip = file name without extension), frames
  "decoracoes-2d/<file>" (relative to the repo; fx_sprites.py makes them absolute when it merges the battle manifest);
- build/sprites/battle.sprites.json: its "decor" set replaced the same way (absolute paths, so the Cooker's Engine2 root does
  not apply);
- content/engine2/editor-catalog.json: a removed duplicate's catalog art points at the copy that was kept (content/decor-dupes.json:
  removed file -> kept file; SceneData::load does the same for scenes exported earlier), then every 2D decoration whose art is
  no longer in the folder is dropped from the catalog's decoration tables (decorations, facings, decorationInfo, decoMeta,
  decorationOrder; never elsewhere: an id can also name a place). 3D pieces keep their
  entries: their PNG is only a thumbnail. Then EmberEngine's own 2D decorations (content/decor-own.json: the user's art cut into
  the folder by split_decor_sheet.py) are written into the catalog, each shaped like the existing decoration it names in "like"
  (size, footprint, light, blocking), in the file's own editor section, measured from the folder (measure_art.py's rule).
  Their ids start with "ember-", so a map using them is built by SceneBuilder (MapStage hasEmberProps).

Run it after export_editor_catalog.ts (which rewrites the catalog) and after fx_sprites.py / apply_sprite_overrides.py (which
rewrite the battle manifest), then `measure_art.py --catalog content/engine2/editor-catalog.json C:/Engine2/public` (the kept
copies need their measures) and cook battle.pak again. Idempotent.

    python -I tools/scripts/sprites/decor_folder.py
"""
import json
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
FOLDER = "decoracoes-2d"
PREFIX = "/game/decorations/"


def load(path):
    with open(path, encoding="utf-8") as f:
        return json.load(f)


def save(path, data, indent):
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        json.dump(data, f, indent=indent, ensure_ascii=False, separators=None if indent is not None else (",", ":"))
        f.write("\n")


def folder_clips(old_clips, absolute):
    """The folder's PNGs as clips: an existing clip keeps its fields (order too), only its frames change; new files go last."""
    files = sorted(f for f in os.listdir(os.path.join(ROOT, FOLDER)) if f.lower().endswith(".png"))
    names = {os.path.splitext(f)[0]: f for f in files}
    base = ROOT.replace("\\", "/") + "/" + FOLDER if absolute else FOLDER
    out, seen = [], set()
    for c in old_clips:
        if c["name"] in names:
            c = dict(c, frames={"list": [f"{base}/{names[c['name']]}"]})
            out.append(c)
            seen.add(c["name"])
    for name, f in names.items():
        if name not in seen:
            out.append({"name": name, "fps": 1.0, "mode": "loop", "pack": "decor", "frames": {"list": [f"{base}/{f}"]}})
    return out


def patch_manifest(path, absolute):
    manifest = load(path)
    for s in manifest["sets"]:
        if s["name"] == "decor":
            before = len(s["clips"])
            s["clips"] = folder_clips(s["clips"], absolute)
            print(f"{os.path.relpath(path, ROOT)}: decor {before} -> {len(s['clips'])} clips")
    save(path, manifest, 1)


def scrub(cat, removed):
    """Only the decoration tables: an id can also name a place ("watchtower" is the Torre de Vigia's location and door style)."""
    for table in (cat["decorations"], cat["facings"], cat["editor"]["decorationInfo"], cat["editor"]["decoMeta"]):
        for did in removed:
            table.pop(did, None)
    cat["editor"]["decorationOrder"] = [d for d in cat["editor"]["decorationOrder"] if d not in removed]
    return cat


def remap(v, dupes):
    if isinstance(v, dict):
        return {k: remap(x, dupes) for k, x in v.items()}
    if isinstance(v, list):
        return [remap(x, dupes) for x in v]
    if isinstance(v, str) and v.startswith(PREFIX):
        stem = os.path.splitext(v[len(PREFIX):])[0]
        if stem in dupes:
            return f"{PREFIX}{dupes[stem]}.png"
    return v


def add_own(cat):
    # content/decor-own.json and every content/decor-own-<name>.json beside it (one editor section each: the Misty Cave's own art,
    # 2026-10-23, in decor-own-cave.json).
    content = os.path.join(ROOT, "content")
    for name in ["decor-own.json"] + sorted(f for f in os.listdir(content) if f.startswith("decor-own-") and f.endswith(".json")):
        add_own_file(cat, os.path.join(content, name))


def add_own_file(cat, file):
    from measure_art import measure
    own = load(file)
    ed = cat["editor"]
    section = own["section"]
    if section not in ed["decorationSections"]:
        ed["decorationSections"].append(section)
    for o in own["decorations"]:
        did, image, like = o["id"], f"{PREFIX}{o['image']}.png", o["like"]
        path = os.path.join(ROOT, FOLDER, o["image"] + ".png")
        if not os.path.exists(path):
            raise SystemExit(f"{path} missing ({os.path.basename(file)} {did})")
        cat["decorations"][did] = dict(cat["decorations"][like], name=o["name"])
        if "light" in o:  # the piece's own light in place of its model's (colour, intensity, radius, flicker, minHeight)
            cat["decorations"][did]["light"] = dict(cat["decorations"][like].get("light") or {}, **o["light"])
        cat["facings"][did] = [dict(f, file=o["image"], image=image) for f in cat["facings"][like]]
        ed["decorationInfo"][did] = dict(ed["decorationInfo"][like], section=section, thumb=image)
        ed["decoMeta"][did] = dict(ed["decoMeta"][like], art=image)
        if did not in ed["decorationOrder"]:
            ed["decorationOrder"].append(did)
        cat["measures"][image] = measure(path)
    print(f"catalog: {len(own['decorations'])} own decorations in section {section}")


def add_own_backgrounds(cat):
    # EmberEngine's own map paintings (content/backdrops-own.json: the user's art in content/backdrops), listed in the editor's
    # Imagem de fundo and found by id (SceneBuilder EditorCatalog::backdrop) like the old ones; a map picks one by its
    # "backgroundImage" field. Idempotent.
    path = os.path.join(ROOT, "content", "backdrops-own.json")
    if not os.path.exists(path):
        return
    listed = cat["editor"]["backgrounds"]
    for b in load(path)["backgrounds"]:
        if not os.path.exists(os.path.join(ROOT, "content", "backdrops", b["file"])):
            raise SystemExit(f"content/backdrops/{b['file']} missing (content/backdrops-own.json {b['id']})")
        entry = {"id": b["id"], "label": b["label"], "file": b["file"]}
        listed[:] = [x for x in listed if x.get("id") != b["id"]] + [entry]
        cat["backgrounds"]["byId"][b["id"]] = b["file"]
    print(f"catalog: {len(load(path)['backgrounds'])} own backgrounds")


def patch_catalog(path, dupes):
    raw = open(path, encoding="utf-8").read()
    cat = remap(json.loads(raw), dupes)
    present = set(os.listdir(os.path.join(ROOT, FOLDER)))
    removed = set()
    for did, faces in cat["facings"].items():
        images = {f["image"] for f in faces if isinstance(f, dict) and f.get("art") and f.get("image", "").startswith(PREFIX)}
        d = cat["decorations"].get(did, {})
        if images and not d.get("model3d") and not d.get("model") and all(i[len(PREFIX):] not in present for i in images):
            removed.add(did)
    cat = scrub(cat, removed)
    sys.path.insert(0, os.path.join(ROOT, "tools", "scripts", "engine2"))
    from measure_art import measure
    for kept in dupes.values():  # the kept copy's measures (the catalog had only the removed one's)
        if f"{PREFIX}{kept}.png" not in cat["measures"] and f"{kept}.png" in present:
            cat["measures"][f"{PREFIX}{kept}.png"] = measure(os.path.join(ROOT, FOLDER, f"{kept}.png"))
    add_own(cat)
    add_own_backgrounds(cat)
    out = json.dumps(cat, ensure_ascii=False, separators=(",", ":")) if "\n" not in raw.strip() else json.dumps(cat, ensure_ascii=False, indent=2)
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write(out + ("\n" if raw.endswith("\n") else ""))
    print(f"{os.path.relpath(path, ROOT)}: {len(removed)} decorations dropped: {', '.join(sorted(removed))}")


def main():
    dupes = load(os.path.join(ROOT, "content", "decor-dupes.json"))
    patch_manifest(os.path.join(ROOT, "content", "decor.sprites.json"), absolute=False)
    battle = os.path.join(ROOT, "build", "sprites", "battle.sprites.json")
    if os.path.exists(battle):
        patch_manifest(battle, absolute=True)
    patch_catalog(os.path.join(ROOT, "content", "engine2", "editor-catalog.json"), dupes)


if __name__ == "__main__":
    main()
