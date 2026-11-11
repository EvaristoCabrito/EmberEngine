"""One list of places (the user, 2026-11-11: "they should never differ thats what im saying from start of this game").

The places and their maps were kept in five copies that drifted apart: content/data/overworld.json, overworld-half.json (the one
the game loads), campaign.json, and editor-catalog.json's editor.locations + editor.locais (submaps, slots). The Lighthouse had
its five maps in four of them and none in overworld-half.json, so the map drew its pin and nothing else.

This merges them once into content/data/locations.json, the only list from now on (world/Locations.h reads it; the world map,
the campaign, the classic map and the Map Editor's Locais all use it, and the editor saves into it), and removes the copies.
Every place keeps everything any copy gave it, in order (overworld.json first, then the editor's, then campaign.json, then
overworld-half.json), keeping only maps that exist: a campaign mission or a saved map file (content/maps/ember/<id>NNN.json).
col / row are the loaded grid's (overworld-half.json). Run once from the repository root: python -I tools/scripts/data/one_location_list.py
"""
import io
import json
import pathlib
import re
import sys

sys.stdout.reconfigure(encoding="utf-8")
ROOT = pathlib.Path(__file__).resolve().parents[3]
DATA = ROOT / "content" / "data"
MAPS = ROOT / "content" / "maps" / "ember"
OUT = DATA / "locations.json"


def read(path):
    raw = io.open(path, encoding="utf-8", newline="").read()
    nl = "\r\n" if "\r\n" in raw else "\n"
    text = raw.replace("\r\n", "\n")
    d = json.loads(text)
    layout = None
    for ind in (None, 1, 2, 4):
        for end in ("", "\n"):
            for seps in (None, (",", ":")):
                if json.dumps(d, ensure_ascii=False, indent=ind, separators=seps) + end == text:
                    layout = (ind, end, seps)
    assert layout, f"{path}: its layout is not one json.dumps writes; not touched"
    return d, (nl, layout)


def write(path, d, how):
    nl, (ind, end, seps) = how
    io.open(path, "w", encoding="utf-8", newline="").write((json.dumps(d, ensure_ascii=False, indent=ind, separators=seps) + end).replace("\n", nl))


full, full_how = read(DATA / "overworld.json")
half, half_how = read(DATA / "overworld-half.json")
camp, camp_how = read(DATA / "campaign.json")
cat, cat_how = read(DATA / "editor-catalog.json")
editor = cat["editor"]

missions = {m["id"] for m in camp["missions"]}
saved = set()
for f in MAPS.glob("*.json"):
    m = re.fullmatch(r"(.+?)(\d{3})\.json", f.name)
    if m:
        saved.add(m.group(1))
# a map entry needs a map: a saved file, or its campaign mapFile (vertente / portao: missions with no map, out of the list since 2026-10-29)
map_file = {m["id"]: m.get("mapFile", "") for m in camp["missions"]}
real = lambda mid: mid in saved or bool(map_file.get(mid)) and (MAPS / map_file[mid]).exists()

# each copy as {id: place}
ed_floors = editor.get("locais", {}).get("submaps", {})
ed_slots = editor.get("locais", {}).get("slots", {})
copies = [
    ("overworld.json", {l["id"]: l for l in full["locations"]}),
    ("editor", {l["id"]: {**l, "submaps": ed_floors.get(l["id"], [])} for l in editor.get("locations", [])}),
    ("campaign.json", {l["id"]: l for l in camp["locations"]}),
    ("overworld-half.json", {l["id"]: l for l in half["locations"]}),
]
order = []
for _, c in copies:
    for i in c:
        if i not in order:
            order.append(i)
for loc_id in ed_floors:  # floors the editor holds for a place it does not list
    if loc_id not in order:
        order.append(loc_id)

half_at = {l["id"]: (l["col"], l["row"]) for l in half["locations"]}
dropped = []
places = []
for loc_id in order:
    first = next((c[loc_id] for _, c in copies if loc_id in c), {"id": loc_id})
    p = {"id": loc_id, "name": first.get("name", loc_id)}
    for k in ("x", "y", "pinDx", "pinDy"):
        for _, c in copies:
            if loc_id in c and k in c[loc_id]:
                p[k] = c[loc_id][k]
                break
    if loc_id in half_at:
        p["col"], p["row"] = half_at[loc_id]
    ids, floors, seen = [], [], set()
    for name, c in copies:
        if loc_id not in c:
            continue
        for mid in c[loc_id].get("missionIds", []):
            if mid in seen:
                continue
            seen.add(mid)
            (ids.append(mid) if real(mid) else dropped.append(f"{loc_id}: {mid} ({name}: no mission, no map)"))
    for name, c in copies:
        src = c[loc_id].get("submaps", []) if loc_id in c else (ed_floors.get(loc_id, []) if name == "editor" else [])
        for f in src:
            mid = f["missionId"]
            if mid in seen:
                continue
            seen.add(mid)
            (floors.append({"missionId": mid, "floor": f.get("floor", 0)}) if real(mid) else dropped.append(f"{loc_id}: floor {mid} ({name}: no mission, no map)"))
    p["missionIds"] = ids
    if floors:
        p["submaps"] = floors
    for k in ("openAccess", "encountersAllowed", "warpCity", "deadlineDay"):
        for _, c in copies:
            if loc_id in c and c[loc_id].get(k) not in (None, False):
                p[k] = c[loc_id][k]
                break
    if ed_slots.get(loc_id):
        p["slots"] = ed_slots[loc_id]
    places.append(p)

io.open(OUT, "w", encoding="utf-8", newline="\n").write(json.dumps({"about": __doc__.split("\n\n")[0], "locations": places}, ensure_ascii=False, indent=1) + "\n")

# the copies go
del full["locations"], half["locations"], camp["locations"]
editor.pop("locations", None)
if "locais" in editor:
    editor["locais"].pop("submaps", None)
    editor["locais"].pop("slots", None)
    if not editor["locais"]:
        del editor["locais"]
write(DATA / "overworld.json", full, full_how)
write(DATA / "overworld-half.json", half, half_how)
write(DATA / "campaign.json", camp, camp_how)
write(DATA / "editor-catalog.json", cat, cat_how)

for p in places:
    print(f"{p['id']:18s} {'' if 'x' in p else '(no pin) '}maps {p['missionIds']} floors {[f['missionId'] for f in p.get('submaps', [])]}")
print("left out (no mission, no map):", *dropped, sep="\n  ")
