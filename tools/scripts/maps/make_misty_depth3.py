"""A Bacia dos Esporos (misty-spore-basin), the Misty Cave's level 3, rebuilt from the user's art (Attachments/MistyCave/3rdfloor,
docs/MISTY_DEPTH3_PROMPTS.md): a new version written beside the old (the campaign plays the highest serial); the old file is only read.

    python -I tools/scripts/maps/make_misty_depth3.py content/maps/ember/misty-spore-basin001.json content/maps/ember/misty-spore-basin002.json

The basin (hex grid 40 x 30, pointy-top, odd rows shifted right), as the briefing tells it ("Uma floresta subterrânea de cogumelos
ilumina as galerias. Uma ramificação leva ao pequeno refúgio Esporabrigo; a passagem principal continua descendo"):
  - the forest floor of mycelium loam (plains) filling a wide basin with a ragged rim;
  - the basin people's walkway of cut mushroom caps (nave) winding from the west landing (the connector back up to the Garganta das
    Raízes) to the east landing (the main passage down: the user's chain, 2026-10-23, goes from here to map 4, the Underdark of
    Mordávia), and a branch north to the refuge Esporabrigo (its connector), lit by the people's trail markers;
  - shelf-fungus terraces (hill, levels 1-2) along the north and south rims: high ground;
  - the spore mire (snow) in the low south with a still pool (water), fallen giant mushrooms (ruins: cover) here and there;
  - the old map's chest at the mire's far edge; the old map's four enemies at their roles and four more of their own kinds.
The old map's settings (title, briefing, objective, win by escape, fog, mist, the intro dialogue, the party) are kept. The script fails
if any floor, spawn, the chest or a connector cannot be reached from the party.
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

    def carve(pred, terrain, over=None):
        for r in range(ROWS):
            for c in range(COLS):
                if pred(c, r) and (over is None or tiles.get((c, r)) in over):
                    tiles[(c, r)] = terrain

    # the basin and its ragged rim
    carve(lambda c, r: in_ellipse(c, r, 20.0, 12.6, 16.6, 11.0), "plains")
    for bite in ((6.0, 3.4), (34.4, 3.0), (4.6, 21.6), (35.6, 21.8), (15.4, 1.2), (26.0, 1.4), (13.0, 23.6), (28.0, 23.8)):
        for (c, r) in list(tiles):
            if in_ellipse(c, r, *bite, 2.6, 1.8):
                del tiles[(c, r)]
    # the landings and the refuge's clearing
    carve(lambda c, r: in_ellipse(c, r, 2.6, 12.6, 2.6, 2.4), "nave")
    carve(lambda c, r: in_ellipse(c, r, 37.4, 12.6, 2.6, 2.4), "nave")
    carve(lambda c, r: in_ellipse(c, r, 10.4, 3.4, 2.2, 1.6), "plains")

    # the terraces (before the walkway, so the walkway cuts through where it climbs them)
    for (cx, cy, rx, ry) in ((20.0, 4.6, 6.0, 1.9), (31.0, 5.4, 3.6, 1.6), (14.2, 20.6, 4.6, 1.7), (27.0, 20.4, 4.2, 1.6)):
        for (c, r) in list(tiles):
            if tiles[(c, r)] == "plains" and in_ellipse(c, r, cx, cy, rx, ry):
                tiles[(c, r)] = "hill"
                level[(c, r)] = 2 if in_ellipse(c, r, cx, cy, rx * 0.55, ry * 0.5) else 1
    # the spore mire and its pool
    for (cx, cy, rx, ry) in ((20.4, 17.6, 4.4, 1.8), (7.6, 17.8, 2.6, 1.4)):
        carve(lambda c, r: in_ellipse(c, r, cx, cy, rx, ry), "snow", over=("plains",))
    carve(lambda c, r: in_ellipse(c, r, 20.6, 17.8, 1.8, 0.8), "water", over=("snow",))
    # fallen giant mushrooms
    for (cx, cy) in ((11.2, 10.2), (24.2, 9.4), (30.0, 16.0), (16.8, 8.6), (6.6, 8.8), (33.2, 9.0)):
        carve(lambda c, r: in_ellipse(c, r, cx, cy, 1.2, 0.95), "ruins", over=("plains",))
    # the walkway and the branch north to the refuge (over everything walkable)
    walk_pts = [(1.0, 12.6), (8.0, 12.0), (14.0, 13.8), (20.0, 12.4), (26.0, 13.8), (32.6, 12.4), (39.0, 12.6)]
    for a, b in zip(walk_pts, walk_pts[1:]):
        carve(lambda c, r: near_segment(c, r, a, b, 0.85), "nave", over=("plains", "ruins", "hill", "snow", "nave"))
    branch = [(13.6, 13.6), (12.4, 8.6), (10.6, 4.2)]
    for a, b in zip(branch, branch[1:]):
        carve(lambda c, r: near_segment(c, r, a, b, 0.8), "nave", over=("plains", "ruins", "hill", "snow", "nave"))
    for k in [k for k, t in tiles.items() if t != "hill"]:
        level.pop(k, None)

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
        c, r = free(2.4 + (k % 2) * 1.0, 12.0 + (k // 2) * 0.9, ("nave",))
        players.append(dict(p, x=c, y=r))

    decorations = []
    conn = {dd.get("targetMapId"): dd for dd in d0["decorations"] if dd.get("targetMapId")}
    cells = {}
    for target, spot, new_target in (("misty-root-descent", (0.6, 12.6), None), ("misty-blackwater-shelf", (39.4, 12.6), "mordavia-underdark"),
                                     ("misty-sporehaven", (10.4, 2.6), None)):
        if target not in conn:
            raise SystemExit(f"{src} has no connector to {target}")
        c, r = free(*spot)
        dd = dict(conn[target], x=c, y=r)
        if new_target:
            dd["targetMapId"] = new_target
        decorations.append(dd)
        cells[dd["targetMapId"]] = (c, r)
    chest = next((dd for dd in d0["decorations"] if dd["id"].startswith("chest")), None)
    chest_cell = None
    if chest:
        c, r = free(26.4, 17.8, ("snow", "plains"))
        decorations.append(dict(chest, x=c, y=r))
        chest_cell = (c, r)

    by_name = {e["name"]: e for e in d0["enemySpawns"]}
    plan = [
        ("Morto da galeria", (12.0, 15.6)), ("Morto da galeria", (18.4, 16.0)), ("Morto da galeria", (23.6, 16.4)),
        ("Peregrino perdido", (11.4, 6.4)), ("Peregrino perdido", (25.6, 11.0)),
        ("Vigia das ruínas", (20.0, 4.4)), ("Vigia das ruínas", (27.0, 20.2)),
        ("Sentinela das profundezas", (35.8, 12.4)),
    ]
    enemies = []
    for name, spot in plan:
        if name not in by_name:
            raise SystemExit(f"{src} has no enemy named {name}")
        c, r = free(*spot)
        enemies.append(dict(by_name[name], x=c, y=r))

    props = [  # the user's level 3 art (content/decor-own-cave3.json); the trail markers are the level's light
        ("ember-cave3-trail-marker", (4.8, 11.0)), ("ember-cave3-trail-marker", (4.6, 14.4)), ("ember-cave3-trail-marker", (9.4, 13.4)),
        ("ember-cave3-trail-marker", (14.6, 12.2)), ("ember-cave3-trail-marker", (13.6, 10.2)), ("ember-cave3-trail-marker", (11.4, 5.8)),
        ("ember-cave3-trail-marker", (9.0, 3.8)), ("ember-cave3-trail-marker", (19.6, 13.8)), ("ember-cave3-trail-marker", (23.4, 12.0)),
        ("ember-cave3-trail-marker", (28.6, 14.6)), ("ember-cave3-trail-marker", (32.4, 11.0)), ("ember-cave3-trail-marker", (36.0, 14.4)),
        ("ember-cave3-trail-marker", (20.2, 6.4)), ("ember-cave3-trail-marker", (14.6, 19.0)), ("ember-cave3-trail-marker", (26.2, 19.0)),
        ("ember-cave3-trail-marker", (8.0, 19.2)), ("ember-cave3-trail-marker", (31.4, 7.2)), ("ember-cave3-trail-marker", (25.4, 16.6)),
        ("ember-cave3-shelf-boulder", (7.6, 9.6)), ("ember-cave3-shelf-boulder", (17.2, 10.6)), ("ember-cave3-shelf-boulder", (23.0, 15.0)),
        ("ember-cave3-shelf-boulder", (29.8, 10.2)), ("ember-cave3-shelf-boulder", (33.6, 16.2)), ("ember-cave3-shelf-boulder", (10.6, 16.4)),
        ("ember-cave3-cocoon", (16.4, 15.6)), ("ember-cave3-cocoon", (28.0, 8.2)), ("ember-cave3-cocoon", (6.4, 16.4)),
    ]
    for pid, spot in props:
        c, r = free(*spot, ("plains", "nave", "ruins", "snow"))
        decorations.append({"id": pid, "x": c, "y": r})

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
    if len(scraps) > 6:
        raise SystemExit(f"too much floor cut off from the party: {scraps}")
    for p in scraps:
        del tiles[p]
    must = list(cells.values()) + ([chest_cell] if chest_cell else []) + [(e["x"], e["y"]) for e in enemies] + \
           [(p["x"], p["y"]) for p in players] + [(dd["x"], dd["y"]) for dd in decorations]
    for m in must:
        if m not in seen:
            raise SystemExit(f"cut off from the party: {m}")
    walk = {k for k, t in tiles.items() if t in ("plains", "nave", "ruins", "hill", "snow")}

    grid = [tiles.get((c, r), "void") for r in range(ROWS) for c in range(COLS)]
    elev = [float(level.get((c, r), 0)) for r in range(ROWS) for c in range(COLS)]
    d = dict(d0)
    d.update({
        "cols": COLS, "rows": ROWS, "tiles": grid, "tileVariants": [None] * (COLS * ROWS), "tileRots": [],
        "terrainElevations": elev, "decorations": decorations, "playerSpawns": players, "enemySpawns": enemies,
        "environment": "indoor", "underground": True, "emberTiles": "cave3-v01", "backgroundImage": "ember-misty-spore-basin-v01",
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
