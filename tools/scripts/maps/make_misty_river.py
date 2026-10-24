"""As Margens do Rio Negro (misty-blackwater-shelf), the Misty Cave's depth 1, built from the user's art (Attachments/Underworld,
docs/MISTY_RIVER_PROMPTS.md): a new version written beside the old (the campaign plays the highest serial); the old file is only read.

    python -I tools/scripts/maps/make_misty_river.py content/maps/ember/misty-blackwater-shelf002.json content/maps/ember/misty-blackwater-shelf003.json

The river cavern (hex grid 44 x 32, pointy-top, odd rows shifted right), as the briefing tells it ("Um rio subterrâneo divide terraços de
rocha. Duas pontes naturais cruzam o leito; as galerias do sul conduzem à Colônia do Micélio Profundo"):
  - terraces of black shale with gypsum flowers (plains) on both sides, a level higher in the north-west and the south-east;
  - the black river (water) winding north to south through the middle, its rippled silt banks (snow), the river's debris (ruins) on them;
  - two natural rock bridges (hill, a level up: their water-carved backs) over the river, north and south;
  - the river-wardens' grooved path (nave) from the west landing (the connector back up to the Misty Cave) over both bridges to the east
    landing (the way down to the Garganta das Raízes) and to the south landing (the branch to the Micélio Profundo);
  - the old map's chest and rune stone; the old map's four enemies at their roles and four more of their own kinds.
The old map's settings (title, briefing, objective, win by escape, fog, mist, the intro dialogue, the party) are kept. The script fails
if any floor, spawn, the chest or a connector cannot be reached from the party.
"""
import json
import math
import sys
import time

COLS, ROWS = 44, 32
SQ = math.sqrt(3) / 2


def pos(c, r):
    return c + 0.5 * (r & 1), r * SQ


def neighbors(c, r):
    odd = [(1, 0), (-1, 0), (1, -1), (0, -1), (1, 1), (0, 1)]
    even = [(1, 0), (-1, 0), (0, -1), (-1, -1), (0, 1), (-1, 1)]
    for dc, dr in (odd if r & 1 else even):
        if 0 <= c + dc < COLS and 0 <= r + dr < ROWS:
            yield c + dc, r + dr


def in_ellipse(c, r, cx, cy, rx, ry):
    x, y = pos(c, r)
    return ((x - cx) / rx) ** 2 + ((y - cy) / ry) ** 2 <= 1.0


def near_segment(c, r, a, b, radius):
    x, y = pos(c, r)
    ax, ay = a
    bx, by = b
    dx, dy = bx - ax, by - ay
    t = max(0.0, min(1.0, ((x - ax) * dx + (y - ay) * dy) / (dx * dx + dy * dy)))
    return math.hypot(x - (ax + t * dx), y - (ay + t * dy)) <= radius


RIVER = [(25.0, -1.0), (23.6, 6.0), (22.2, 13.0), (24.0, 19.6), (21.6, 28.0)]
NORTH_BRIDGE = ((18.0, 6.6), (29.4, 6.0))
SOUTH_BRIDGE = ((18.4, 19.8), (29.8, 19.4))
WEST, EAST, SOUTH = (2.4, 14.7), (41.6, 14.7), (31.0, 25.6)


def main():
    src, out = sys.argv[1], sys.argv[2]
    d0 = json.load(open(src, encoding="utf-8"))["draft"]
    tiles, level = {}, {}

    def carve(pred, terrain, over=None):
        for r in range(ROWS):
            for c in range(COLS):
                if pred(c, r) and (over is None or tiles.get((c, r)) in over):
                    tiles[(c, r)] = terrain

    def along(pts, radius):
        return lambda c, r: any(near_segment(c, r, a, b, radius) for a, b in zip(pts, pts[1:]))

    # the cavern floor and its ragged rim
    carve(lambda c, r: in_ellipse(c, r, 22.0, 13.4, 20.6, 12.4), "plains")
    for bite in ((4.0, 3.0), (40.0, 3.2), (4.4, 24.2), (39.6, 24.0), (13.6, 0.8), (33.0, 0.8), (12.0, 26.4), (36.0, 27.0)):
        for (c, r) in list(tiles):
            if in_ellipse(c, r, *bite, 2.8, 1.9):
                del tiles[(c, r)]
    for spot in (WEST, EAST, SOUTH):  # the three landings
        carve(lambda c, r: in_ellipse(c, r, *spot, 2.4, 2.0), "nave")
    # the higher terraces
    for (c, r) in [k for k, t in tiles.items() if t == "plains"]:
        if in_ellipse(c, r, 9.6, 6.2, 6.0, 3.6) or in_ellipse(c, r, 34.6, 21.0, 5.6, 3.2):
            level[(c, r)] = 1
    # the river and its banks, the debris on them
    carve(along(RIVER, 3.2), "snow", over=("plains",))
    carve(along(RIVER, 1.5), "water", over=("snow",))
    for (cx, cy) in ((19.0, 10.6), (26.6, 14.4), (20.4, 23.6), (27.4, 2.4), (25.8, 23.8)):
        carve(lambda c, r: in_ellipse(c, r, cx, cy, 1.2, 0.9), "ruins", over=("snow", "plains"))
    # the two natural bridges, a level up over the river
    for a, b in (NORTH_BRIDGE, SOUTH_BRIDGE):
        carve(lambda c, r: near_segment(c, r, a, b, 1.15), "hill", over=("plains", "snow", "water", "ruins"))
        for (c, r) in list(tiles):
            if tiles[(c, r)] == "hill" and near_segment(c, r, a, b, 1.15):
                level[(c, r)] = 1
    # the wardens' path
    paths = [[WEST, (11.0, 13.4), (15.4, 9.0), NORTH_BRIDGE[0]], [(11.0, 13.4), (15.6, 17.6), SOUTH_BRIDGE[0]],
             [NORTH_BRIDGE[1], (33.4, 9.4), (38.0, 13.6), EAST], [SOUTH_BRIDGE[1], (34.0, 16.6), (38.0, 13.6)], [SOUTH_BRIDGE[1], SOUTH]]
    for pts in paths:
        carve(along(pts, 0.8), "nave", over=("plains", "ruins", "snow", "nave"))
    for k in [k for k, t in tiles.items() if t in ("water", "ruins") or (t == "snow")]:
        level.pop(k, None)
    for k in [k for k in level if k not in tiles]:
        level.pop(k)

    used = set()

    def free(x, y, allowed=("plains", "nave", "ruins", "hill", "snow")):
        best = None
        for (c, r), t in tiles.items():
            if t not in allowed or (c, r) in used:
                continue
            px, py = pos(c, r)
            dd = (px - x) ** 2 + (py - y) ** 2
            if best is None or dd < best[0]:
                best = (dd, (c, r))
        used.add(best[1])
        return best[1]

    players = []
    for k, p in enumerate(d0["playerSpawns"]):
        c, r = free(2.6 + (k % 2) * 1.0, 14.0 + (k // 2) * 0.9, ("nave",))
        players.append(dict(p, x=c, y=r))

    decorations = []
    conn = {dd.get("targetMapId"): dd for dd in d0["decorations"] if dd.get("targetMapId")}
    cells = {}
    for target, spot in (("misty-cave-dungeon", (0.6, 14.7)), ("misty-root-descent", (43.2, 14.7)), ("misty-deep-mycelium", (31.0, 27.0))):
        if target not in conn:
            raise SystemExit(f"{src} has no connector to {target}")
        c, r = free(*spot)
        decorations.append(dict(conn[target], x=c, y=r))
        cells[target] = (c, r)
    chest = next((dd for dd in d0["decorations"] if dd["id"].startswith("chest")), None)
    chest_cell = None
    if chest:
        c, r = free(37.6, 22.2, ("plains",))
        decorations.append(dict(chest, x=c, y=r))
        chest_cell = (c, r)
    rune = next((dd for dd in d0["decorations"] if dd["id"] == "rune-stone"), None)
    if rune:
        c, r = free(8.0, 5.2, ("plains",))
        decorations.append(dict(rune, x=c, y=r))

    by_name = {e["name"]: e for e in d0["enemySpawns"]}
    plan = [
        ("Morto da galeria", (14.4, 18.4)), ("Morto da galeria", (27.4, 12.0)), ("Morto da galeria", (32.6, 24.2)),
        ("Peregrino perdido", (31.4, 8.6)), ("Peregrino perdido", (16.6, 8.2)),
        ("Vigia das ruínas", (34.6, 21.0)), ("Vigia das ruínas", (9.6, 6.2)),
        ("Sentinela das profundezas", (38.6, 14.4)),
    ]
    enemies = []
    for name, spot in plan:
        if name not in by_name:
            raise SystemExit(f"{src} has no enemy named {name}")
        c, r = free(*spot)
        enemies.append(dict(by_name[name], x=c, y=r))

    props = [  # the user's Rio Negro art (content/decor-own-river.json): the wardens' lamps (warm) and the fluorite (violet) are the light
        ("ember-river-mooring-lamp", (5.0, 13.0)), ("ember-river-mooring-lamp", (12.6, 12.0)), ("ember-river-mooring-lamp", (17.0, 5.4)),
        ("ember-river-mooring-lamp", (30.4, 7.6)), ("ember-river-mooring-lamp", (17.4, 21.2)), ("ember-river-mooring-lamp", (30.8, 18.0)),
        ("ember-river-mooring-lamp", (38.6, 16.2)), ("ember-river-mooring-lamp", (32.6, 26.4)), ("ember-river-mooring-lamp", (20.8, 13.6)),
        ("ember-river-fluorite", (7.0, 9.6)), ("ember-river-fluorite", (13.6, 4.0)), ("ember-river-fluorite", (36.0, 6.4)),
        ("ember-river-fluorite", (40.0, 19.6)), ("ember-river-fluorite", (11.4, 22.0)), ("ember-river-fluorite", (28.2, 25.4)),
        ("ember-river-fluorite", (26.4, 9.6)), ("ember-river-fluorite", (19.4, 16.4)), ("ember-river-fluorite", (35.4, 12.0)),
        ("ember-river-column", (8.4, 16.8)), ("ember-river-column", (14.0, 14.8)), ("ember-river-column", (33.0, 4.6)),
        ("ember-river-column", (36.4, 18.2)), ("ember-river-column", (16.0, 24.0)), ("ember-river-column", (29.6, 15.0)),
    ]
    for pid, spot in props:
        c, r = free(*spot, ("plains", "nave", "ruins", "snow"))
        decorations.append({"id": pid, "x": c, "y": r})

    # --- checks -----------------------------------------------------------------------------------------------------------
    floor = ("plains", "nave", "ruins", "hill", "snow")
    solid = {(dd["x"], dd["y"]) for dd in decorations if dd["id"] == "ember-river-column"}
    walk = {k for k, t in tiles.items() if t in floor}
    start = (players[0]["x"], players[0]["y"])
    seen, todo = {start}, [start]
    while todo:
        cur = todo.pop()
        for nb in neighbors(*cur):
            if nb in walk and nb not in solid and nb not in seen:
                seen.add(nb)
                todo.append(nb)
    scraps = sorted(walk - seen - solid)
    if len(scraps) > 6:
        raise SystemExit(f"too much floor cut off from the party: {scraps}")
    for p in scraps:
        del tiles[p]
        level.pop(p, None)
    must = list(cells.values()) + ([chest_cell] if chest_cell else []) + [(e["x"], e["y"]) for e in enemies] + \
           [(p["x"], p["y"]) for p in players] + [(dd["x"], dd["y"]) for dd in decorations if (dd["x"], dd["y"]) not in solid]
    for m in must:
        if m not in seen:
            raise SystemExit(f"cut off from the party: {m}")
    walk = {k for k, t in tiles.items() if t in floor}

    grid = [tiles.get((c, r), "void") for r in range(ROWS) for c in range(COLS)]
    elev = [float(level.get((c, r), 0)) for r in range(ROWS) for c in range(COLS)]
    d = dict(d0)
    d.update({
        "cols": COLS, "rows": ROWS, "tiles": grid, "tileVariants": [None] * (COLS * ROWS), "tileRots": [],
        "terrainElevations": elev, "decorations": decorations, "playerSpawns": players, "enemySpawns": enemies,
        "environment": "indoor", "underground": True, "emberTiles": "river-v01", "backgroundImage": "ember-misty-blackwater-shelf-v01",
    })
    doc = {"serial": int(out[-8:-5]), "savedAt": int(time.time() * 1000), "draft": d}
    open(out, "w", encoding="utf-8", newline="\n").write(json.dumps(doc, ensure_ascii=False, indent=2))
    counts = {}
    for t in grid:
        counts[t] = counts.get(t, 0) + 1
    print(f"{out}: {COLS}x{ROWS}, {counts}, {len(enemies)} enemies, {len(players)} heroes, {len(decorations)} decorations, "
          f"{len(walk)} walkable cells all reachable ({len(scraps)} stray cells dropped)")


if __name__ == "__main__":
    main()
