"""The Misty Cave chain put in order (the user, 2026-10-23: "faz mais uma intermediária entre a 1 e 2, acertar a ordem das outras").

Writes one new version of each map in the chain (serial + 1; the old versions are only read), with its "Profundidade N" title and its
connectors re-pointed so the chain runs:

    Misty Cave (misty-cave-dungeon) -> 1 As Margens do Rio Negro (misty-blackwater-shelf, the new intermediate level)
      -> 2 A Garganta das Raízes (misty-root-descent) -> 3 A Bacia dos Esporos (misty-spore-basin)
      -> 4 Underdark de Mordávia (mordavia-underdark; its exit opens the Atlas do Submundo)

The side branches stay: the Rio Negro's to the Micélio Profundo, the Bacia's to Esporabrigo. O Céu de Pedra (misty-hollow-earth) leaves the
chain (kept as it is). Run from the repo root:

    python -I tools/scripts/maps/misty_chain_order.py
"""
import json
import os
import re
import time

MAPS = "content/maps/ember"

# map id -> (title's depth number or None to keep the title, {old connector target: new target})
PLAN = {
    "misty-cave-dungeon": (None, {"misty-root-descent": "misty-blackwater-shelf"}),
    "misty-blackwater-shelf": (1, {"misty-spore-basin": "misty-cave-dungeon", "misty-hollow-earth": "misty-root-descent"}),
    "misty-root-descent": (2, {"misty-cave-dungeon": "misty-blackwater-shelf"}),
    "misty-spore-basin": (3, {}),
    "mordavia-underdark": (4, {"misty-hollow-earth": "misty-spore-basin"}),
}


def newest(map_id):
    best = None
    for f in os.listdir(MAPS):
        m = re.fullmatch(re.escape(map_id) + r"(\d{3})\.json", f)
        if m and (best is None or int(m.group(1)) > best[0]):
            best = (int(m.group(1)), f)
    return best


def main():
    for map_id, (depth, remap) in PLAN.items():
        serial, name = newest(map_id)
        src = os.path.join(MAPS, name)
        out = os.path.join(MAPS, f"{map_id}{serial + 1:03d}.json")
        if os.path.exists(out):
            raise SystemExit(f"{out} exists")
        doc = json.load(open(src, encoding="utf-8"))
        d = doc["draft"]
        if depth is not None:
            title = d.get("title", "")
            new = re.sub(r"Profundidade \d+", f"Profundidade {depth}", title)
            if new == title:
                raise SystemExit(f"{src}: no 'Profundidade N' in the title {title!r}")
            d["title"] = new
        seen = set()
        for dd in d["decorations"]:
            t = dd.get("targetMapId")
            if t in remap:
                dd["targetMapId"] = remap[t]
                seen.add(t)
        missing = set(remap) - seen
        if missing:
            raise SystemExit(f"{src}: no connector to {sorted(missing)}")
        doc["serial"] = serial + 1
        if isinstance(doc.get("savedAt"), (int, float)):
            doc["savedAt"] = int(time.time() * 1000)
        open(out, "w", encoding="utf-8", newline="\n").write(json.dumps(doc, ensure_ascii=False, indent=2))
        links = [(dd.get("targetMapId"), dd.get("returnConnector")) for dd in d["decorations"] if dd.get("targetMapId")]
        print(f"{name} -> {os.path.basename(out)}: {d.get('title')} | {links}")


if __name__ == "__main__":
    main()
