"""Create the versioned 2026-11 cemetery makeover maps without altering prior saves.

Run from the project root with: python tools/scripts/maps/make_cemetery_overhaul_v01.py

The source maps remain intact. Each new file keeps the same map id, expands the grid,
scales the existing layout and connectors, selects a cemetery ground set, and adds
non-overlapping 3D scenery from the current game catalog.
"""

from __future__ import annotations

import json
import math
import time
from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
MAPS = ROOT / "content/maps/ember"

# source, destination, new columns, new rows, ground set, new 3D props, creature swaps
PLANS = [
    ("cemiterio-esquecidos013.json", "cemiterio-esquecidos014.json", 72, 56, "cemiterio-makeover-v01",
     ["ember-sketchfab-swamp-dead-tree-001", "ember-dungeon-bone-pile"],
     {"zombie": ["zombie2", "zombieDog"], "cultist": ["emberedWraith"]}),
    ("cemiterio-esquecidos-cripta002.json", "cemiterio-esquecidos-cripta003.json", 44, 36, "cemiterio-crypt-makeover-v01",
     ["ember-dungeon-iron-cage", "ember-dungeon-bone-pile", "ember-dungeon-pillar-v02"],
     {"zombie": ["zombie2"], "cultistV2": ["emberedWraith"]}),
    ("cemiterio-esquecidos-cripta-2002.json", "cemiterio-esquecidos-cripta-2003.json", 40, 34, "cemiterio-crypt-makeover-v01",
     ["ember-dungeon-altar", "ember-dungeon-bone-pile", "ember-dungeon-chains"],
     {"zombie": ["minorHorror"], "cultist": ["emberedWraith"]}),
    ("cemiterio-esquecidos-mausoleu002.json", "cemiterio-esquecidos-mausoleu003.json", 40, 32, "cemiterio-mausoleum-makeover-v01",
     ["ember-dungeon-pillar-v02", "ember-dungeon-altar", "ember-dungeon-bone-pile"],
     {"zombie": ["zombieDog"]}),
    ("cemiterio-esquecidos-ruinas003.json", "cemiterio-esquecidos-ruinas004.json", 42, 34, "cemiterio-ruins-makeover-v01",
     ["ember-dungeon-chains", "ember-dungeon-bone-pile", "ember-dungeon-pillar"],
     {"zombie": ["horror"]}),
    ("cemetery-ground-part-ii002.json", "cemetery-ground-part-ii003.json", 76, 56, "cemiterio-makeover-v01",
     ["ember-sketchfab-swamp-dead-tree-002", "ember-sketchfab-swamp-dead-tree-003", "ember-dungeon-bone-pile"],
     {"zombie": ["zombieDog", "zombie2"], "cultistV2": ["emberedWraith"]}),
    ("cemetery-ground-deep-crypt-1002.json", "cemetery-ground-deep-crypt-1003.json", 60, 48, "cemiterio-crypt-makeover-v01",
     ["ember-dungeon-pillar-v02", "ember-dungeon-iron-cage", "ember-dungeon-bone-pile"],
     {"zombie": ["zombie2"], "cultistV2": ["emberedWraith"]}),
    ("cemetery-ground-deep-crypt-2002.json", "cemetery-ground-deep-crypt-2003.json", 60, 48, "cemiterio-crypt-makeover-v01",
     ["ember-dungeon-pillar", "ember-dungeon-chains", "ember-dungeon-bone-pile"],
     {"zombie": ["zombieDog"], "cultistV2": ["undeadOx"]}),
    ("cemetery-ground-deep-crypt-3002.json", "cemetery-ground-deep-crypt-3003.json", 60, 48, "cemiterio-ruins-makeover-v01",
     ["ember-dungeon-chains", "ember-dungeon-iron-cage", "ember-dungeon-bone-pile"],
     {"zombie": ["horror"], "cultistV2": ["emberedWraith"]}),
    ("cemetery-ground-deep-crypt-4002.json", "cemetery-ground-deep-crypt-4003.json", 60, 48, "cemiterio-crypt-makeover-v01",
     ["ember-dungeon-pillar-v02", "ember-dungeon-altar", "ember-dungeon-bone-pile"],
     {"zombie": ["minorHorror"]}),
    ("crypt-of-whispers003.json", "crypt-of-whispers004.json", 38, 32, "cemiterio-crypt-makeover-v01",
     ["ember-dungeon-iron-cage", "ember-dungeon-bone-pile"],
     {"wardog": ["zombieDog"]}),
    ("crypt-of-whispers-2003.json", "crypt-of-whispers-2004.json", 38, 32, "cemiterio-crypt-makeover-v01",
     ["ember-dungeon-chains", "ember-dungeon-pillar-v02", "ember-dungeon-bone-pile"],
     {"cultistV2": ["emberedWraith"]}),
    ("crypt-of-whispers-3002.json", "crypt-of-whispers-3003.json", 38, 32, "cemiterio-crypt-makeover-v01",
     ["ember-dungeon-altar", "ember-dungeon-bone-pile", "ember-dungeon-iron-cage"],
     {"cultistV2": ["minorHorror"]}),
]


def resize_grid(values: list, old_cols: int, old_rows: int, cols: int, rows: int) -> list:
    """Nearest-cell resize keeps the painted regions and their per-cell metadata aligned."""
    out = []
    for y in range(rows):
        sy = min(old_rows - 1, int((y + 0.5) * old_rows / rows))
        for x in range(cols):
            sx = min(old_cols - 1, int((x + 0.5) * old_cols / cols))
            out.append(values[sy * old_cols + sx])
    return out


def scale_positions(value, sx: float, sy: float):
    if isinstance(value, list):
        for item in value:
            scale_positions(item, sx, sy)
    elif isinstance(value, dict):
        if isinstance(value.get("x"), (int, float)) and isinstance(value.get("y"), (int, float)):
            value["x"] = max(0, int(math.floor((value["x"] + 0.5) * sx)))
            value["y"] = max(0, int(math.floor((value["y"] + 0.5) * sy)))
        for child in value.values():
            if isinstance(child, (dict, list)):
                scale_positions(child, sx, sy)


def occupied(draft: dict) -> set[tuple[int, int]]:
    cells = set()
    for key in ("decorations", "playerSpawns", "enemySpawns", "neutralSpawns", "elementalFx"):
        for item in draft.get(key, []):
            if isinstance(item, dict) and isinstance(item.get("x"), int) and isinstance(item.get("y"), int):
                cells.add((item["x"], item["y"]))
    return cells


def clear_spawn_overlaps(draft: dict) -> None:
    """When a larger grid separates cells, move any actor shifted onto a prop to nearby floor."""
    cols, rows = draft["cols"], draft["rows"]
    tiles = draft["tiles"]
    prop_cells = {(d["x"], d["y"]) for d in draft.get("decorations", [])
                  if isinstance(d, dict) and isinstance(d.get("x"), int) and isinstance(d.get("y"), int)}
    actors = [(key, item) for key in ("playerSpawns", "enemySpawns", "neutralSpawns")
              for item in draft.get(key, []) if isinstance(item, dict)
              and isinstance(item.get("x"), int) and isinstance(item.get("y"), int)]
    claimed: set[tuple[int, int]] = set()
    for _, spawn in actors:
        origin = (spawn["x"], spawn["y"])
        if origin in prop_cells or origin in claimed:
            choices = []
            for y in range(1, rows - 1):
                for x in range(1, cols - 1):
                    cell = (x, y)
                    if cell in prop_cells or cell in claimed or tiles[y * cols + x] in ("void", "water"):
                        continue
                    choices.append(((x - origin[0]) ** 2 + (y - origin[1]) ** 2, y, x))
            if choices:
                _, y, x = min(choices)
                spawn["x"], spawn["y"] = x, y
                origin = (x, y)
        claimed.add(origin)


def add_props(draft: dict, prop_ids: list[str]) -> None:
    cols, rows = draft["cols"], draft["rows"]
    tiles = draft["tiles"]
    used = occupied(draft)
    entrances = [(d["x"], d["y"]) for d in draft.get("decorations", [])
                 if isinstance(d, dict) and d.get("id") in ("floor-connector", "dungeon-exit")]
    party = [(s["x"], s["y"]) for s in draft.get("playerSpawns", [])]
    # Spread the new models toward the outer parts of each room, away from entrances and the party.
    targets = [(0.2, 0.25), (0.8, 0.28), (0.22, 0.72), (0.78, 0.72)]
    for index, prop_id in enumerate(prop_ids):
        tx, ty = targets[index % len(targets)]
        target = (int(cols * tx), int(rows * ty))
        choices = []
        for y in range(1, rows - 1):
            for x in range(1, cols - 1):
                if (x, y) in used:
                    continue
                if tiles[y * cols + x] in ("void", "water"):
                    continue
                if any((x - ex) ** 2 + (y - ey) ** 2 < 25 for ex, ey in entrances + party):
                    continue
                score = (x - target[0]) ** 2 + (y - target[1]) ** 2
                choices.append((score, y, x))
        if not choices:
            continue
        _, y, x = min(choices)
        draft.setdefault("decorations", []).append({"id": prop_id, "x": x, "y": y, "blocksPath": True})
        used.add((x, y))


def change_creatures(draft: dict, rules: dict[str, list[str]]) -> None:
    counters = {class_id: 0 for class_id in rules}
    for spawn in draft.get("enemySpawns", []):
        old = spawn.get("classId")
        if old not in rules:
            continue
        # Keep the final named boss intact when a floor deliberately has one.
        if "Última Vigília" in spawn.get("name", ""):
            continue
        alternatives = rules[old]
        new = alternatives[counters[old] % len(alternatives)]
        counters[old] += 1
        spawn["classId"] = new
        labels = {
            "zombie2": "Morto do Ossuário",
            "zombieDog": "Cão Sepulcral",
            "emberedWraith": "Espectro Sepulcral",
            "minorHorror": "Horror Menor da Cripta",
            "horror": "Horror Afogado",
            "undeadOx": "Boi Morto-vivo",
        }
        spawn["name"] = labels.get(new, spawn.get("name", new))


def main() -> None:
    stamp = int(time.time() * 1000)
    for source_name, output_name, cols, rows, ground_set, props, swaps in PLANS:
        source = MAPS / source_name
        output = MAPS / output_name
        if output.exists():
            raise FileExistsError(f"Refusing to replace an existing map version: {output}")
        data = json.loads(source.read_text(encoding="utf-8"))
        draft = data.get("draft", data)
        old_cols, old_rows = draft["cols"], draft["rows"]
        sx, sy = cols / old_cols, rows / old_rows

        for key in ("tiles", "tileVariants", "tileRots", "terrainElevations", "waterLevels"):
            grid = draft.get(key)
            if isinstance(grid, list) and len(grid) == old_cols * old_rows:
                draft[key] = resize_grid(grid, old_cols, old_rows, cols, rows)
        for key in ("decorations", "playerSpawns", "enemySpawns", "neutralSpawns", "elementalFx"):
            if key in draft:
                scale_positions(draft[key], sx, sy)

        draft["cols"], draft["rows"] = cols, rows
        draft["emberTiles"] = ground_set
        if draft.get("id") == "cemetery-ground-part-ii":
            draft["locationId"] = "cemetery"
        data["serial"] = int(data.get("serial", 0)) + 1
        data["savedAt"] = stamp
        clear_spawn_overlaps(draft)
        change_creatures(draft, swaps)
        add_props(draft, props)
        output.write_text(json.dumps(data, ensure_ascii=False, separators=(",", ":")) + "\n", encoding="utf-8")
        print(f"{source_name} -> {output_name} ({old_cols}x{old_rows} -> {cols}x{rows})")


if __name__ == "__main__":
    main()
