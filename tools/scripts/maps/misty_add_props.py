"""The user's late Misty Cave decorations (Attachments/Underworld, 2026-10-23: docs/MISTY_MISSING_ART.md) placed on their levels: one new
version of each map (serial + 1; the old versions are only read), everything else as it was.

    python -I tools/scripts/maps/misty_add_props.py

- misty-cave-dungeon (As Profundezas Enevoadas): the ancient sealed door on the north vault's door tile, the sunken door on the south
  vault's (the briefing's "portas antigas, uma delas afundada"; Arrombar still opens the tile, and the card goes with it: MapStage
  buildCards), the iron lanterns on stakes along the old worked passage (nave);
- misty-root-descent (A Garganta das Raízes): knots of petrified roots as cover on the gallery floor;
- misty-spore-basin (A Bacia dos Esporos): clusters of glowing mushrooms and spore chimneys on the forest floor, the basin people's cap
  lanterns along their walkway.
Spots are picked on the map's own floor, spread out (a least distance to each other and to what stands there), never on a connector,
spawn, chest or door; the script fails if a blocking piece cuts any floor, spawn or connector off from the party.
"""
import json
import math
import os
import re
import time

MAPS = "content/maps/ember"
SQ = math.sqrt(3) / 2
FLOOR = ("plains", "nave", "ruins", "hill", "snow", "door")
BLOCKING = {"ember-cave2-root-knot"}  # solid like its model (a fractured boulder: two hexes)


def newest(map_id):
    best = None
    for f in os.listdir(MAPS):
        m = re.fullmatch(re.escape(map_id) + r"(\d{3})\.json", f)
        if m and (best is None or int(m.group(1)) > best[0]):
            best = (int(m.group(1)), f)
    return best


def pos(c, r):
    return c + 0.5 * (r & 1), r * SQ


def neighbors(c, r, cols, rows):
    odd = [(1, 0), (-1, 0), (1, -1), (0, -1), (1, 1), (0, 1)]
    even = [(1, 0), (-1, 0), (0, -1), (-1, -1), (0, 1), (-1, 1)]
    for dc, dr in (odd if r & 1 else even):
        if 0 <= c + dc < cols and 0 <= r + dr < rows:
            yield c + dc, r + dr


def place(d, plan):
    cols, rows = d["cols"], d["rows"]
    tile = lambda c, r: d["tiles"][r * cols + c]
    taken = {(x["x"], x["y"]) for x in d["decorations"]} | {(s["x"], s["y"]) for k in ("playerSpawns", "enemySpawns", "neutralSpawns")
                                                             for s in d.get(k, [])}
    spots = [pos(x["x"], x["y"]) for x in d["decorations"]]
    added = []
    for pid, terrains, count, spacing, near in plan:
        picked = 0
        cells = [(c, r) for r in range(rows) for c in range(cols) if tile(c, r) in terrains and (c, r) not in taken]
        # spread over the map: walk the cells in a fixed scrambled order, keep a cell far enough from everything placed
        cells.sort(key=lambda k: ((k[0] * 7919 + k[1] * 104729) % 1009, k))
        for c, r in cells:
            if picked == count:
                break
            p = pos(c, r)
            if any(math.dist(p, q) < near for q in spots) or any(math.dist(p, pos(*a[1:])) < spacing for a in added if a[0] == pid):
                continue
            added.append((pid, c, r))
            taken.add((c, r))
            spots.append(p)
            picked += 1
        if picked < count:
            raise SystemExit(f"{d['id']}: only {picked} of {count} spots for {pid}")
    return [{"id": pid, "x": c, "y": r} for pid, c, r in added]


def check(d):
    cols, rows = d["cols"], d["rows"]
    tile = lambda c, r: d["tiles"][r * cols + c]
    solid = set()
    for x in d["decorations"]:
        if x["id"] in BLOCKING:
            solid |= {(x["x"], x["y"]), (x["x"] + 1, x["y"])}
    walk = {(c, r) for r in range(rows) for c in range(cols) if tile(c, r) in FLOOR} - solid
    start = (d["playerSpawns"][0]["x"], d["playerSpawns"][0]["y"])
    seen, todo = {start}, [start]
    while todo:
        cur = todo.pop()
        for nb in neighbors(*cur, cols, rows):
            if nb in walk and nb not in seen:
                seen.add(nb)
                todo.append(nb)
    must = [(x["x"], x["y"]) for x in d["decorations"] if x.get("targetMapId")] + \
           [(s["x"], s["y"]) for k in ("playerSpawns", "enemySpawns", "neutralSpawns") for s in d.get(k, [])]
    for m in must:
        if m not in seen:
            raise SystemExit(f"{d['id']}: {m} cut off from the party")
    lost = len(walk - seen)
    return lost


def doors(d):
    cols = d["cols"]
    cells = sorted(((i % cols, i // cols) for i, t in enumerate(d["tiles"]) if t == "door"), key=lambda k: k[1])
    if len(cells) != 2:
        raise SystemExit(f"{d['id']}: {len(cells)} door tiles, two expected")
    return cells  # north first


def main():
    work = {
        "misty-cave-dungeon": lambda d: (
            [{"id": "ember-cave-sealed-door", "x": doors(d)[0][0], "y": doors(d)[0][1]},
             {"id": "ember-cave-sunken-door", "x": doors(d)[1][0], "y": doors(d)[1][1]}]
            + place(d, [("ember-cave-stake-lantern", ("nave", "plains"), 7, 4.5, 1.6)])),
        "misty-root-descent": lambda d: place(d, [("ember-cave2-root-knot", ("plains", "ruins"), 4, 5.0, 2.4)]),
        "misty-spore-basin": lambda d: place(d, [("ember-cave3-glow-cluster", ("plains",), 8, 4.5, 2.0),
                                                 ("ember-cave3-spore-chimney", ("plains", "snow"), 4, 7.0, 2.4),
                                                 ("ember-cave3-cap-lantern", ("nave",), 6, 4.5, 1.6)]),
    }
    for map_id, add in work.items():
        serial, name = newest(map_id)
        doc = json.load(open(os.path.join(MAPS, name), encoding="utf-8"))
        d = doc["draft"]
        new = add(d)
        d["decorations"] = d["decorations"] + new
        lost = check(d)
        out = os.path.join(MAPS, f"{map_id}{serial + 1:03d}.json")
        if os.path.exists(out):
            raise SystemExit(f"{out} exists")
        doc["serial"] = serial + 1
        doc["savedAt"] = int(time.time() * 1000)
        open(out, "w", encoding="utf-8", newline="\n").write(json.dumps(doc, ensure_ascii=False, indent=2))
        print(f"{name} -> {os.path.basename(out)}: +{len(new)} ({', '.join(sorted({x['id'] for x in new}))}); "
              f"{lost} floor cells only behind doors or pieces")


if __name__ == "__main__":
    main()
