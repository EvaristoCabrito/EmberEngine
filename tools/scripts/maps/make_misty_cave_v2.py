"""As Profundezas Enevoadas (misty-cave-dungeon) rebuilt from the user's own art (Attachments/MistyCave, docs/MISTY_CAVE_PROMPTS.md):
a new version written beside the old (the campaign plays the highest serial); the old file is only read.

    python -I tools/scripts/maps/make_misty_cave_v2.py content/maps/ember/misty-cave-dungeon003.json content/maps/ember/misty-cave-dungeon004.json

The cave, from the entrance to the deep end (hex grid 32 x 24, pointy-top, odd rows shifted right):
  - the entrance: the old worked floor (nave) where the party comes in, south-west;
  - a passage with rockfall (ruins: cover) up into the main cavern (plains, the bedrock floor), its north rim raised in calcite
    ledges (hill, levels 1-2: high ground), an underground pool (water) ringed by its wet margin (snow) in its east half;
  - two vaults, each sealed by an ancient door (door: locked, Arrombar opens it) at the end of a one-hex neck: the north one dry,
    the south one sunk in the wet margin (the briefing's "uma delas afundada"); the old map's four chests, two in each;
  - an east passage with rockfall into the deep chamber, its middle the old worked floor, the expedition's remains, the Birolho,
    and the floor connector on (misty-root-descent).
The old map's enemies (the same 17 units, names, classes, levels) are placed by role: the crossbows and the soldier at the passage
mouth, the pikes and the wolves on the cavern floor, two cultists on the ledges, the golem and the troll at the north door, the
horror and two cultists by the sunken door, the Birolho and two pikes in the deep chamber. The script fails if any floor, spawn,
chest or the connector cannot be reached from the party (doors counted as open), or if a vault opens anywhere but its door.
"""
import json
import math
import sys
import time

COLS, ROWS = 32, 24
SQ = math.sqrt(3) / 2


def pos(c, r):
    return c + 0.5 * (r & 1), r * SQ


def neighbors(c, r):
    odd = [(1, 0), (-1, 0), (1, -1), (0, -1), (1, 1), (0, 1)]
    even = [(1, 0), (-1, 0), (0, -1), (-1, -1), (0, 1), (-1, 1)]
    for dc, dr in (odd if r & 1 else even):
        if 0 <= c + dc < COLS and 0 <= r + dr < ROWS:
            yield c + dc, r + dr


def cell_at(x, y):
    """The cell whose centre is nearest to the point (x, y) in pos space."""
    best = None
    for r in range(ROWS):
        for c in range(COLS):
            px, py = pos(c, r)
            d = (px - x) ** 2 + (py - y) ** 2
            if best is None or d < best[0]:
                best = (d, c, r)
    return best[1], best[2]


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


def main():
    src, out = sys.argv[1], sys.argv[2]
    old = json.load(open(src, encoding="utf-8"))
    d0 = old["draft"]

    tiles = {}  # (c, r) -> terrain; absent = void
    level = {}

    def carve(pred, terrain, keep=False):
        for r in range(ROWS):
            for c in range(COLS):
                if pred(c, r) and (not keep or (c, r) not in tiles):
                    tiles[(c, r)] = terrain

    # --- the spaces -----------------------------------------------------------------------------------------------------
    entrance = (5.0, 16.4)
    carve(lambda c, r: in_ellipse(c, r, *entrance, 3.6, 3.0), "nave")
    carve(lambda c, r: near_segment(c, r, (7.6, 14.6), (11.6, 11.6), 1.7), "plains")           # entrance -> cavern
    cavern = (16.5, 10.2)
    carve(lambda c, r: in_ellipse(c, r, *cavern, 6.8, 4.8), "plains")
    carve(lambda c, r: near_segment(c, r, (22.0, 9.6), (25.4, 8.4), 1.6), "plains")           # cavern -> deep chamber
    deep = (28.0, 8.2)
    carve(lambda c, r: in_ellipse(c, r, *deep, 3.4, 3.4), "plains")
    carve(lambda c, r: in_ellipse(c, r, *deep, 1.9, 1.7), "nave")                             # the old worked floor in its middle
    # the vaults' passages: a corridor, then a one-hex neck with the door at its end
    carve(lambda c, r: near_segment(c, r, (13.0, 6.2), (12.2, 4.6), 1.2), "plains")
    carve(lambda c, r: near_segment(c, r, (12.2, 4.6), (11.2, 2.6), 0.55), "plains")
    vault_n = (10.2, 1.2)
    carve(lambda c, r: in_ellipse(c, r, *vault_n, 2.2, 1.1), "nave")
    carve(lambda c, r: near_segment(c, r, (14.6, 14.4), (14.3, 16.0), 1.2), "snow")
    carve(lambda c, r: near_segment(c, r, (14.3, 16.0), (14.0, 18.0), 0.55), "snow")
    vault_s = (13.4, 19.8)
    carve(lambda c, r: in_ellipse(c, r, *vault_s, 2.6, 1.2), "snow")

    # --- inside the cavern: ledges, the pool, its margin, rockfall --------------------------------------------------------
    for (c, r) in list(tiles):
        if tiles[(c, r)] != "plains":
            continue
        if in_ellipse(c, r, 14.6, 6.4, 4.6, 1.7):
            tiles[(c, r)] = "hill"
            level[(c, r)] = 2 if in_ellipse(c, r, 14.2, 6.0, 2.4, 0.9) else 1
    pool = (19.8, 11.8)
    for (c, r) in list(tiles):
        if tiles[(c, r)] == "plains" and in_ellipse(c, r, *pool, 2.4, 1.6):
            tiles[(c, r)] = "water"
        elif tiles[(c, r)] == "plains" and in_ellipse(c, r, *pool, 3.5, 2.6):
            tiles[(c, r)] = "snow"
    for centre, rx, ry in (((9.6, 12.8), 1.2, 1.0), ((23.6, 9.0), 1.1, 0.9), ((15.8, 13.6), 1.1, 0.8), ((26.2, 10.6), 0.9, 0.8)):
        for (c, r) in list(tiles):
            if tiles[(c, r)] == "plains" and in_ellipse(c, r, *centre, rx, ry):
                tiles[(c, r)] = "ruins"

    # --- doors, columns ---------------------------------------------------------------------------------------------------
    def neck_cell(x, y, vault, terrain):
        """The door: of every carved cell outside the vault that touches it, the one nearest to (x, y); the others become rock,
        so the door is the one way in."""
        touching = []
        for (c, r), t in tiles.items():
            if in_ellipse(c, r, *vault):
                continue
            if any(nb in tiles and in_ellipse(*nb, *vault) for nb in neighbors(c, r)):
                px, py = pos(c, r)
                touching.append(((px - x) ** 2 + (py - y) ** 2, (c, r)))
        touching.sort()
        for _, cell in touching[1:]:
            del tiles[cell]
        return touching[0][1]

    door_n = neck_cell(11.2, 2.6, (*vault_n, 2.2, 1.1), "plains")
    door_s = neck_cell(14.0, 18.0, (*vault_s, 2.6, 1.2), "snow")
    for dc in (door_n, door_s):
        tiles[dc] = "door"
    column_cells = [cell_at(*p) for p in ((12.4, 9.2), (18.0, 8.0), (20.8, 8.8), (11.6, 12.0), (26.6, 6.6), (29.4, 10.0))]
    for cc in column_cells:
        assert tiles.get(cc) in ("plains", "nave", "ruins"), (cc, tiles.get(cc))
        tiles[cc] = "column"

    # --- spawns, chests, the connector, the props ------------------------------------------------------------------------
    used = set(column_cells) | {door_n, door_s}

    def free(x, y, allowed=("plains", "nave", "ruins", "hill", "snow")):
        """The free walkable cell nearest to (x, y)."""
        best = None
        for (c, r), t in tiles.items():
            if t not in allowed or (c, r) in used:
                continue
            px, py = pos(c, r)
            dd = (px - x) ** 2 + (py - y) ** 2
            if best is None or dd < best[0]:
                best = (dd, c, r)
        used.add((best[1], best[2]))
        return best[1], best[2]

    players = []
    for k, p in enumerate(d0["playerSpawns"]):
        c, r = free(4.2 + (k % 2) * 1.0, 16.0 + (k // 2) * 0.9)
        players.append(dict(p, x=c, y=r))

    # role -> where (pos space), in the old list's order of appearance per role
    spots = {
        ("Besteiro", 0): (11.2, 10.8), ("Besteiro", 1): (12.6, 12.2), ("Soldado", 0): (11.0, 12.6),
        ("Piqueiro", 0): (16.6, 11.2), ("Piqueiro", 1): (17.2, 13.2), ("Piqueiro", 2): (28.6, 7.0), ("Piqueiro", 3): (28.6, 9.6),
        ("Mordavian Wolf Final", 0): (15.2, 12.2), ("Mordavian Wolf Final", 1): (18.0, 13.8),
        ("Cultista Ancestral", 0): (13.6, 5.8), ("Cultista Ancestral", 1): (15.6, 6.0),
        ("Cultista Ancestral", 2): (13.2, 15.6), ("Cultista Ancestral", 3): (15.4, 15.8),
        ("Golem Ancião", 0): (12.2, 4.6), ("Troll da caverna", 0): (13.4, 4.2),
        ("Horror", 0): (14.4, 16.4), ("Birolho", 0): (29.2, 8.2),
    }
    seen = {}
    enemies = []
    for e in d0["enemySpawns"]:
        k = seen.get(e["name"], 0)
        seen[e["name"]] = k + 1
        if (e["name"], k) not in spots:
            raise SystemExit(f"no spot for {e['name']} #{k}")
        c, r = free(*spots[(e["name"], k)])
        enemies.append(dict(e, x=c, y=r))

    decorations = []
    chests = [d for d in d0["decorations"] if d["id"] == "locked-chest"]
    for spot, ch in zip(((9.2, 1.0), (11.4, 0.8), (12.2, 19.8), (14.8, 19.6)), chests):
        c, r = free(*spot)
        decorations.append(dict(ch, x=c, y=r))
    for dd in d0["decorations"]:
        if dd["id"] == "floor-connector":
            c, r = free(30.6, 8.6)
            decorations.append(dict(dd, x=c, y=r))
            connector = (c, r)

    props = [  # the user's cave art (content/decor-own-cave.json)
        ("ember-cave-fallen-block", (8.4, 13.6)), ("ember-cave-fallen-block", (22.8, 7.6)), ("ember-cave-fallen-block", (17.0, 15.4)),
        # the light: underground only the cave's own fires light it, so a seam every few hexes along the way (entrance, passages,
        # cavern, both vaults, the deep chamber)
        ("ember-cave-ember-seam", (7.2, 18.6)), ("ember-cave-ember-seam", (2.4, 14.6)), ("ember-cave-ember-seam", (9.0, 14.8)),
        ("ember-cave-ember-seam", (12.8, 13.8)), ("ember-cave-ember-seam", (20.6, 14.4)), ("ember-cave-ember-seam", (17.4, 7.2)),
        ("ember-cave-ember-seam", (21.4, 8.2)), ("ember-cave-ember-seam", (24.6, 9.8)), ("ember-cave-ember-seam", (26.4, 9.8)),
        ("ember-cave-ember-seam", (30.2, 9.6)), ("ember-cave-ember-seam", (10.6, 1.8)), ("ember-cave-ember-seam", (13.6, 20.2)),
        ("ember-cave-ember-seam", (14.6, 16.2)), ("ember-cave-ember-seam", (12.4, 5.2)),
        ("ember-cave-pale-fungi", (8.2, 19.4)), ("ember-cave-pale-fungi", (22.2, 11.4)), ("ember-cave-pale-fungi", (10.8, 8.4)),
        ("ember-cave-pale-fungi", (30.0, 6.6)),
        # the slender columns against the far walls, never between the camera and the party
        ("ember-cave-column-slender", (21.8, 6.4)), ("ember-cave-column-slender", (25.6, 6.2)), ("ember-cave-column-slender", (18.8, 5.6)),
        ("ember-cave-expedition-remains", (27.2, 10.4)),
    ]
    for pid, spot in props:
        c, r = free(*spot, allowed=("plains", "nave", "ruins", "snow"))
        decorations.append({"id": pid, "x": c, "y": r})
    for cc in column_cells:  # the column cells wear the natural column
        decorations.append({"id": "ember-cave-column", "x": cc[0], "y": cc[1]})

    # --- stray scraps: a walkable piece of at most 3 cells cut off by the pool or the walls joins the water (or the rock) ------
    walk = {k for k, t in tiles.items() if t in ("plains", "nave", "ruins", "hill", "snow", "door")}
    start = (players[0]["x"], players[0]["y"])
    main_part, todo = {start}, [start]
    while todo:
        cur = todo.pop()
        for nb in neighbors(*cur):
            if nb in walk and nb not in main_part:
                main_part.add(nb)
                todo.append(nb)
    for k in sorted(walk - main_part):
        part, todo = {k}, [k]
        while todo:
            cur = todo.pop()
            for nb in neighbors(*cur):
                if nb in walk and nb not in part and nb not in main_part:
                    part.add(nb)
                    todo.append(nb)
        if len(part) <= 3 and not any(tiles[p] == "door" for p in part) and not used & part:
            for p in part:
                tiles[p] = "water" if any(tiles.get(nb) == "water" for nb in neighbors(*p)) else "void"
                print(f"scrap {p} -> {tiles[p]}")
        walk -= {p for p in part if tiles.get(p) in ("water", "void") or p not in tiles}
    walk = {k for k, t in tiles.items() if t in ("plains", "nave", "ruins", "hill", "snow", "door")}

    # --- checks -----------------------------------------------------------------------------------------------------------
    seen_cells, todo = {start}, [start]
    while todo:
        cur = todo.pop()
        for nb in neighbors(*cur):
            if nb in walk and nb not in seen_cells:
                seen_cells.add(nb)
                todo.append(nb)
    lost = sorted(walk - seen_cells)
    if lost:
        raise SystemExit(f"cells cut off from the party: {lost}")
    for who in enemies + players:
        assert (who["x"], who["y"]) in seen_cells, who
    # without the doors the vaults are closed: their chests unreachable, the connector reachable
    open_walk = walk - {door_n, door_s}
    reach, todo = {start}, [start]
    while todo:
        cur = todo.pop()
        for nb in neighbors(*cur):
            if nb in open_walk and nb not in reach:
                reach.add(nb)
                todo.append(nb)
    if connector not in reach:
        raise SystemExit("the connector is behind a door")
    for dd in decorations:
        if dd["id"] == "locked-chest" and (dd["x"], dd["y"]) in reach:
            raise SystemExit(f"a vault opens elsewhere than its door: chest at {dd['x']},{dd['y']}")

    # --- write ------------------------------------------------------------------------------------------------------------
    grid = [tiles.get((c, r), "void") for r in range(ROWS) for c in range(COLS)]
    elev = [float(level.get((c, r), 0)) for r in range(ROWS) for c in range(COLS)]
    d = dict(d0)
    d.update({
        "cols": COLS, "rows": ROWS, "tiles": grid, "tileVariants": [None] * (COLS * ROWS), "tileRots": [],
        "terrainElevations": elev, "decorations": decorations, "playerSpawns": players, "enemySpawns": enemies,
        "environment": "indoor", "underground": True, "emberTiles": "cave-v01", "backgroundImage": "ember-misty-cave-v01",
    })
    serial = int(out[-8:-5])
    doc = {"serial": serial, "savedAt": int(time.time() * 1000), "draft": d}
    open(out, "w", encoding="utf-8", newline="\n").write(json.dumps(doc, ensure_ascii=False, indent=2))
    counts = {}
    for t in grid:
        counts[t] = counts.get(t, 0) + 1
    print(f"{out}: {COLS}x{ROWS}, {counts}, {len(enemies)} enemies, {len(players)} heroes, {len(decorations)} decorations, "
          f"{len(walk)} walkable cells all reachable, doors at {door_n} {door_s}")


if __name__ == "__main__":
    main()
