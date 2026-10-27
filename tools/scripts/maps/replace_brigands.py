"""The old Besteiro (class `brigand`, the old-school sprite) leaves every enemy line-up: each live map (the version the
campaign plays: content/maps/active.json, else the highest serial) whose enemySpawns carry it gets a new version with those
spawns turned into the Besteiro Salteador (`brigandCrossbowman`, as on the Bosque Morto). Neutral spawns keep the old sprite
(NPCs only). Names, places and levels stay. Never overwrites a file; the older versions stay as they are.

The user, 2026-10-27: "this unit have been replaced by the crossbowmen in the Bosque Morto, they should be replaced
everywhere. These old school sprites can be used for NPCs only."

    python -I tools/scripts/maps/replace_brigands.py [--dry-run]
"""
import json
import os
import re
import sys
import time

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
MAPS = os.path.join(ROOT, "content", "maps", "ember")
OLD, NEW = "brigand", "brigandCrossbowman"


def main():
    dry = "--dry-run" in sys.argv
    active = json.load(open(os.path.join(ROOT, "content", "maps", "active.json"), encoding="utf-8"))
    live = {}
    for f in os.listdir(MAPS):
        m = re.match(r"^(.*?)(\d{3})\.json$", f)
        if not m:
            continue
        try:
            d = json.load(open(os.path.join(MAPS, f), encoding="utf-8"))
        except (OSError, ValueError):
            continue
        mid, serial = d.get("draft", {}).get("id"), int(m.group(2))
        if not mid or (mid in active and serial != active[mid]):
            continue
        if mid not in live or serial > live[mid][0]:
            live[mid] = (serial, m.group(1), d)
    written = 0
    for mid, (serial, prefix, d) in sorted(live.items()):
        spawns = [s for s in d["draft"].get("enemySpawns") or [] if s.get("classId") == OLD]
        if not spawns:
            continue
        out = os.path.join(MAPS, f"{prefix}{serial + 1:03d}.json")
        if os.path.exists(out):
            sys.exit(f"refused: {out} exists")
        for s in spawns:
            s["classId"] = NEW
        d["serial"] = serial + 1
        d["savedAt"] = (int(time.time() * 1000) if isinstance(d.get("savedAt"), (int, float))
                        else time.strftime("%Y-%m-%dT%H:%M:%S.000000Z", time.gmtime()))
        print(f"{os.path.basename(out)}: {len(spawns)} enemy Besteiro -> {NEW}")
        if not dry:
            with open(out, "w", encoding="utf-8") as fh:
                json.dump(d, fh, ensure_ascii=False, indent=2)
        written += 1
    print(f"{written} maps{' (dry run)' if dry else ''}")


if __name__ == "__main__":
    main()
