"""A Garganta das Raízes (misty-root-descent), the Misty Cave's level 2, rebuilt from the user's art (Attachments/MistyCave/2nd Floor,
docs/MISTY_DEPTH2_PROMPTS.md): a new version written beside the old (the campaign plays the highest serial); the old file is only read.

    python -I tools/scripts/maps/make_misty_depth2.py content/maps/ember/misty-root-descent001.json content/maps/ember/misty-root-descent002.json

The gorge (hex grid 40 x 30, pointy-top, odd rows shifted right), as the briefing tells it ("Duas galerias contornam o abismo e
convergem na escada inferior"):
  - the abyss in the middle: void, the panorama's depths showing through it;
  - two galleries round it, the north one and the south one: the carved slate ledge (nave) along the abyss's lip, the root mat
    (plains) against the outer wall, scree (ruins: cover) and sap seeps (snow) along the way;
  - four colossal roots lying across the galleries (hill, levels 1-2: climbed over, high ground), and one root spanning the abyss
    from gallery to gallery (hill, level 2): the risky shortcut, open to every bow;
  - the west landing where the party comes down (the connector back up to the Misty Cave), the east landing with the lower stair
    (the connector on, misty-spore-basin, and the root ladder beside it);
  - a side alcove off the north gallery with the sap-tappers' abandoned camp and the old map's chest.
The old map's settings (title, briefing, objective, win by escape, fog, mist, the intro dialogue, the party) are kept; its four
enemies stand at the same roles and four more of their own kinds join them (the gallery's dead, a pilgrim, a watcher). The script
fails if any floor, spawn, the chest or either connector cannot be reached from the party.
"""
import json
import math
import sys
import time

COLS, ROWS = 40, 30
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


def main():
    src, out = sys.argv[1], sys.argv[2]
    d0 = json.load(open(src, encoding="utf-8"))["draft"]
    tiles, level = {}, {}

    def carve(pred, terrain):
        for r in range(ROWS):
            for c in range(COLS):
                if pred(c, r):
                    tiles[(c, r)] = terrain

    centre = (20.0, 12.6)
    # the galleries' ring: root mat out to the wall, the slate ledge along the lip, then the abyss cut out of the middle
    carve(lambda c, r: in_ellipse(c, r, *centre, 14.4, 10.2), "plains")
    carve(lambda c, r: in_ellipse(c, r, *centre, 11.6, 8.0), "nave")
    for (c, r) in list(tiles):
        if in_ellipse(c, r, *centre, 9.8, 6.4):
            del tiles[(c, r)]
    # the landings: west (the way up) and east (the lower stair)
    west, east = (3.6, 12.6), (36.6, 12.6)
    carve(lambda c, r: in_ellipse(c, r, *west, 3.4, 3.0), "nave")
    carve(lambda c, r: in_ellipse(c, r, *east, 3.2, 3.0), "nave")
    # the alcove off the north gallery
    alcove = (20.0, 1.4)
    carve(lambda c, r: in_ellipse(c, r, *alcove, 3.0, 1.3), "plains")
    carve(lambda c, r: near_segment(c, r, (20.0, 2.6), (20.0, 3.4), 1.0), "plains")
    # the outer wall's ragged edge: bites of rock into the root mat
    for bite in ((9.0, 4.8), (31.4, 4.6), (8.6, 20.6), (31.6, 20.8), (14.0, 2.6), (26.4, 22.8)):
        for (c, r) in list(tiles):
            if tiles[(c, r)] == "plains" and in_ellipse(c, r, *bite, 1.4, 1.1):
                del tiles[(c, r)]

    # four colossal roots lying across the galleries (climbed over), and one spanning the abyss
    for a, b in (((10.6, 3.6), (14.2, 7.8)), ((29.6, 3.4), (26.0, 7.6)), ((10.6, 21.6), (14.0, 17.4)), ((29.8, 21.6), (26.2, 17.6))):
        for (c, r) in list(tiles):
            if near_segment(c, r, a, b, 0.95):
                tiles[(c, r)] = "hill"
                level[(c, r)] = 2 if near_segment(c, r, a, b, 0.45) else 1
    carve(lambda c, r: near_segment(c, r, (19.4, 5.2), (20.6, 20.0), 0.9), "hill")
    for (c, r), t in tiles.items():
        if t == "hill" and (c, r) not in level:
            level[(c, r)] = 2
    # scree and sap seeps along the galleries
    for spot, rx, ry, t in (((7.2, 9.0), 1.2, 1.0, "ruins"), ((16.4, 21.0), 1.3, 0.9, "ruins"), ((24.0, 3.6), 1.2, 0.9, "ruins"),
                            ((33.2, 16.4), 1.1, 1.0, "ruins"), ((13.0, 4.4), 1.0, 0.8, "snow"), ((27.6, 21.4), 1.1, 0.8, "snow"),
                            ((6.8, 16.6), 1.1, 0.9, "snow"), ((33.6, 8.6), 1.0, 0.9, "snow")):
        for (c, r) in list(tiles):
            if tiles[(c, r)] in ("plains", "nave") and in_ellipse(c, r, *spot, rx, ry):
                tiles[(c, r)] = t

    # roots descending from the ceiling: the column cells, on the outer edge of the galleries
    def cell_near(x, y, allowed):
        best = None
        for (c, r), t in tiles.items():
            if t not in allowed:
                continue
            px, py = pos(c, r)
            dd = (px - x) ** 2 + (py - y) ** 2
            if best is None or dd < best[0]:
                best = (dd, (c, r))
        return best[1]

    column_cells = []
    for p in ((6.4, 6.4), (15.4, 2.8), (25.2, 2.6), (34.0, 6.6), (6.6, 19.0), (15.6, 22.6), (24.6, 22.8), (33.8, 19.0)):
        cc = cell_near(*p, ("plains",))
        tiles[cc] = "column"
        column_cells.append(cc)

    used = set(column_cells)

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
        c, r = free(3.2 + (k % 2) * 1.0, 12.0 + (k // 2) * 0.9, ("nave",))
        players.append(dict(p, x=c, y=r))

    decorations = []
    back = next(dd for dd in d0["decorations"] if dd.get("targetMapId") == "misty-cave-dungeon")
    on = next(dd for dd in d0["decorations"] if dd.get("targetMapId") == "misty-spore-basin")
    c, r = free(1.0, 12.6, ("nave",))
    decorations.append(dict(back, x=c, y=r))
    back_cell = (c, r)
    c, r = free(39.2, 12.6, ("nave",))
    decorations.append(dict(on, x=c, y=r))
    on_cell = (c, r)
    chest = next((dd for dd in d0["decorations"] if dd["id"].startswith("chest")), None)
    chest_cell = None
    if chest:
        c, r = free(21.6, 1.0, ("plains",))
        decorations.append(dict(chest, x=c, y=r))
        chest_cell = (c, r)

    # the enemies: the old map's four at their roles, four more of their own kinds
    by_name = {e["name"]: e for e in d0["enemySpawns"]}
    plan = [
        ("Morto da galeria", (11.0, 6.6)), ("Morto da galeria", (12.4, 18.4)), ("Morto da galeria", (27.4, 18.8)),
        ("Peregrino perdido", (21.4, 2.0)), ("Peregrino perdido", (28.4, 6.0)),
        ("Vigia das ruínas", (20.0, 11.2)), ("Vigia das ruínas", (30.6, 15.8)),
        ("Sentinela das profundezas", (34.6, 12.2)),
    ]
    enemies = []
    for name, spot in plan:
        if name not in by_name:
            raise SystemExit(f"{src} has no enemy named {name}")
        c, r = free(*spot)
        enemies.append(dict(by_name[name], x=c, y=r))

    props = [  # the user's level 2 art (content/decor-own-cave2.json)
        # the light: the sap pods along both galleries, the landings and the alcove
        ("ember-cave2-sap-pod", (4.8, 15.4)), ("ember-cave2-sap-pod", (5.0, 9.6)), ("ember-cave2-sap-pod", (9.0, 12.6)),
        ("ember-cave2-sap-pod", (2.0, 10.6)), ("ember-cave2-sap-pod", (5.8, 12.2)),  # beside the party's landing
        ("ember-cave2-sap-pod", (12.0, 3.8)), ("ember-cave2-sap-pod", (17.6, 4.6)), ("ember-cave2-sap-pod", (23.0, 4.4)),
        ("ember-cave2-sap-pod", (28.6, 4.4)), ("ember-cave2-sap-pod", (32.6, 9.4)), ("ember-cave2-sap-pod", (12.4, 21.0)),
        ("ember-cave2-sap-pod", (18.0, 21.4)), ("ember-cave2-sap-pod", (23.8, 21.0)), ("ember-cave2-sap-pod", (29.0, 20.6)),
        ("ember-cave2-sap-pod", (33.4, 15.6)), ("ember-cave2-sap-pod", (37.6, 14.8)), ("ember-cave2-sap-pod", (18.6, 0.8)),
        ("ember-cave2-hair-roots", (7.8, 4.0)), ("ember-cave2-hair-roots", (32.2, 3.6)), ("ember-cave2-hair-roots", (7.6, 21.2)),
        ("ember-cave2-hair-roots", (32.6, 21.6)), ("ember-cave2-hair-roots", (20.6, 23.2)),
        ("ember-cave2-root-stumps", (9.2, 18.2)), ("ember-cave2-root-stumps", (26.4, 3.0)), ("ember-cave2-root-stumps", (16.8, 2.6)),
        ("ember-cave2-puffballs", (35.6, 10.0)), ("ember-cave2-puffballs", (35.4, 15.4)), ("ember-cave2-puffballs", (30.4, 19.8)),
        ("ember-cave2-sap-camp", (19.6, 1.0)),
        ("ember-cave2-root-ladder", (39.0, 11.0)),
    ]
    for pid, spot in props:
        c, r = free(*spot, ("plains", "nave", "ruins", "snow"))
        decorations.append({"id": pid, "x": c, "y": r})
    for cc in column_cells:
        decorations.append({"id": "ember-cave2-root-column", "x": cc[0], "y": cc[1]})

    # --- checks -----------------------------------------------------------------------------------------------------------
    walk = {k for k, t in tiles.items() if t in ("plains", "nave", "ruins", "hill", "snow")}
    start = (players[0]["x"], players[0]["y"])
    seen, todo = {start}, [start]
    while todo:
        cur = todo.pop()
        for nb in neighbors(*cur):
            if nb in walk and nb not in seen:
                seen.add(nb)
                todo.append(nb)
    scraps = sorted(walk - seen)
    for p in scraps:  # stray cells the abyss or a wall bite cut off
        del tiles[p]
    if len(scraps) > 6:
        raise SystemExit(f"too much floor cut off from the party: {scraps}")
    walk = {k for k, t in tiles.items() if t in ("plains", "nave", "ruins", "hill", "snow")}
    must = [back_cell, on_cell] + ([chest_cell] if chest_cell else []) + [(e["x"], e["y"]) for e in enemies] + \
           [(p["x"], p["y"]) for p in players] + [(dd["x"], dd["y"]) for dd in decorations if (dd["x"], dd["y"]) not in column_cells]
    for m in must:
        if m not in seen:
            raise SystemExit(f"cut off from the party: {m}")

    grid = [tiles.get((c, r), "void") for r in range(ROWS) for c in range(COLS)]
    elev = [float(level.get((c, r), 0)) for r in range(ROWS) for c in range(COLS)]
    d = dict(d0)
    d.update({
        "cols": COLS, "rows": ROWS, "tiles": grid, "tileVariants": [None] * (COLS * ROWS), "tileRots": [],
        "terrainElevations": elev, "decorations": decorations, "playerSpawns": players, "enemySpawns": enemies,
        "environment": "indoor", "underground": True, "emberTiles": "cave2-v01", "backgroundImage": "ember-misty-root-descent-v01",
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
