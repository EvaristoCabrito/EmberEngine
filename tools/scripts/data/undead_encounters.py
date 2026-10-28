"""The road encounters whose latest map fields undead (Prece do Caminho, docs/SKILLS_HEALER.md: no undead ambush while it holds).

Reads every encounter id the world map's cells can roll (content/data/overworld.json), finds the newest local map file of each
(content/maps/ember: the highest serial wins) and lists the ids whose enemy or neutral spawns include
a class Ember marks creatureType "undead" (data.ts isUndeadClass). Writes content/data/undead-encounters.json.

  python -I tools/scripts/data/undead_encounters.py [--game-data content/game]
"""

import argparse
import glob
import json
import os
import re
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))


def undead_classes(ember):
    src = open(os.path.join(ember, "src", "game", "data.ts"), encoding="utf-8").read()
    out = set()
    for m in re.finditer(r'creatureType:\s*"undead"', src):
        chunk = src[max(0, m.start() - 1500):m.start()]
        ids = re.findall(r"^\s*(\w+):\s*\{", chunk, re.M)
        if ids:
            out.add(ids[-1])
    return out


def newest_map(mission_id, folders):
    best = None
    for folder in folders:
        for f in glob.glob(os.path.join(folder, mission_id + "[0-9][0-9][0-9]*.json")) + glob.glob(os.path.join(folder, mission_id + "-[0-9][0-9][0-9]*.json")):
            m = re.search(r"-?(\d{3})\.json$", f)
            serial = int(m.group(1)) if m else 0
            if best is None or serial >= best[0]:
                best = (serial, f)
    return best[1] if best else None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--game-data", default=os.path.join(ROOT, "content", "game"))
    args = ap.parse_args()
    undead = undead_classes(args.game_data)
    world = json.load(open(os.path.join(ROOT, "content", "ember", "overworld.json"), encoding="utf-8"))
    ids = set()
    for c in world["board"]["cells"]:
        ids.update(c.get("encounters", []))
    folders = [os.path.join(ROOT, "content", "maps", "ember")]
    result = []
    for mid in sorted(ids):
        f = newest_map(mid, folders)
        if not f:
            print(f"{mid}: no map", file=sys.stderr)
            continue
        draft = json.load(open(f, encoding="utf-8"))
        draft = draft.get("draft", draft)
        txt = json.dumps(draft.get("enemySpawns")) + json.dumps(draft.get("neutralSpawns"))
        classes = set(re.findall(r'"classId":\s*"([^"]+)"', txt))
        if classes & undead:
            result.append(mid)
            print(f"{mid}: undead ({', '.join(sorted(classes & undead))}) in {os.path.basename(f)}")
    out = os.path.join(ROOT, "content", "ember", "undead-encounters.json")
    with open(out, "w", encoding="utf-8") as fh:
        json.dump({"undeadClasses": sorted(undead), "encounters": result}, fh, indent=2, ensure_ascii=False)
        fh.write("\n")
    print(f"{len(result)} of {len(ids)} encounters field undead -> {out}")


if __name__ == "__main__":
    main()
