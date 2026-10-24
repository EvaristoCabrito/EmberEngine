"""Esporabrigo (misty-sporehaven), the fungus-folk's village off the Bacia dos Esporos, rebuilt with the user's art (Attachments/Underworld,
docs/MISTY_MISSING_GROUNDS.md): a new version written beside the old (the campaign plays the highest serial); the old file is only read.

    python -I tools/scripts/maps/make_sporehaven_v2.py content/maps/ember/misty-sporehaven001.json content/maps/ember/misty-sporehaven002.json

The village (hex grid 36 x 28, pointy-top, odd rows shifted right), in a clearing of the basin:
  - the people's tilled beds (plains) filling the clearing, its ragged rim falling to the void;
  - the felted mycelium lanes (nave) from the west landing (the connector back to the Bacia) through the plaza in the middle to the east
    fields, and north and south from the plaza;
  - the stalk-round terraces (hill, level 1; the chronicler's at 2) where the homes stand (the old map's three camps), the harvest refuse
    (ruins) heaped at the clearing's edge;
  - the old map's three people: Lume, the spores' guardian, by the gate; Nara, the cultivator, in the south-west fields; Orun, the
    chronicler, on the north terrace; their conversations as they were.
The old map's settings (title, briefing, objective, win by escape, the party) are kept; no enemies (as before). The script fails if any
floor, spawn, person or the connector cannot be reached from the party.
"""
import json
import math
import sys
import time

COLS, ROWS = 36, 28
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


PLAZA = (18.0, 11.7)


def main():
    src, out = sys.argv[1], sys.argv[2]
    d0 = json.load(open(src, encoding="utf-8"))["draft"]
    tiles, level = {}, {}

    def carve(pred, terrain, over=None):
        for r in range(ROWS):
            for c in range(COLS):
                if pred(c, r) and (over is None or tiles.get((c, r)) in over):
                    tiles[(c, r)] = terrain

    # the clearing and its ragged rim
    carve(lambda c, r: in_ellipse(c, r, 18.4, 11.7, 16.0, 10.4), "plains")
    for bite in ((3.6, 3.2), (33.4, 3.0), (3.2, 20.8), (33.8, 20.4), (12.0, 0.6), (24.6, 0.8), (13.4, 22.8), (26.0, 22.6)):
        for (c, r) in list(tiles):
            if in_ellipse(c, r, *bite, 2.4, 1.7):
                del tiles[(c, r)]
    carve(lambda c, r: in_ellipse(c, r, 2.4, 14.7, 2.4, 2.0), "nave")  # the west landing
    # the terraces where the homes stand (before the lanes, so a lane cuts through where it climbs one)
    for (cx, cy, rx, ry, top) in ((9.0, 6.6, 3.0, 1.8, 1), (27.2, 6.2, 3.2, 1.8, 1), (27.0, 17.4, 3.0, 1.8, 1), (10.0, 18.6, 2.8, 1.6, 1),
                                  (17.4, 3.4, 2.8, 1.5, 2)):
        for (c, r) in list(tiles):
            if tiles[(c, r)] == "plains" and in_ellipse(c, r, cx, cy, rx, ry):
                tiles[(c, r)] = "hill"
                level[(c, r)] = top if in_ellipse(c, r, cx, cy, rx * 0.55, ry * 0.5) else 1
    # the harvest refuse heaped at the edge
    for (cx, cy) in ((32.4, 14.8), (4.6, 8.0), (5.6, 19.4), (30.6, 4.2), (21.8, 20.2)):
        carve(lambda c, r: in_ellipse(c, r, cx, cy, 1.2, 0.9), "ruins", over=("plains",))
    # the lanes and the plaza
    lanes = [[(0.6, 14.7), (8.6, 13.6), PLAZA, (27.0, 11.6), (33.4, 10.8)], [PLAZA, (17.6, 6.0), (17.4, 3.2)], [PLAZA, (19.6, 16.6), (20.4, 21.0)],
             [(8.6, 13.6), (9.0, 8.0)], [(27.0, 11.6), (27.2, 7.6)], [(27.0, 11.6), (27.0, 16.0)], [(8.6, 13.6), (10.0, 17.4)]]
    for pts in lanes:
        for a, b in zip(pts, pts[1:]):
            carve(lambda c, r: near_segment(c, r, a, b, 0.85), "nave", over=("plains", "ruins", "hill", "nave"))
    carve(lambda c, r: in_ellipse(c, r, *PLAZA, 3.4, 2.4), "nave", over=("plains", "ruins", "nave"))
    for k in [k for k, t in tiles.items() if t != "hill"]:
        level.pop(k, None)

    used = set()

    def free(x, y, allowed=("plains", "nave", "ruins", "hill")):
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
    back = next((dd for dd in d0["decorations"] if dd.get("targetMapId") == "misty-spore-basin"), None)
    if back is None:
        raise SystemExit(f"{src} has no connector back to misty-spore-basin")
    c, r = free(0.6, 14.7)
    decorations.append(dict(back, x=c, y=r))
    connector = (c, r)

    spots = {"Lume": (6.4, 12.6), "Nara": (11.6, 15.8), "Orun": (17.4, 3.6)}
    neutrals = []
    for n in d0["neutralSpawns"]:
        key = n["name"].split(",")[0]
        if key not in spots:
            raise SystemExit(f"{src}: no place for {n['name']}")
        c, r = free(*spots[key])
        neutrals.append(dict(n, x=c, y=r))

    props = [  # the homes (the old map's camps) on their terraces; the basin's own art (content/decor-own-cave3.json) and its glowing caps
        ("wilds-camp", (8.4, 6.0)), ("wilds-camp", (27.8, 5.8)), ("wilds-camp", (26.4, 18.0)),
        ("ember-cave3-trail-marker", (4.6, 13.0)), ("ember-cave3-trail-marker", (5.0, 16.0)), ("ember-cave3-trail-marker", (12.6, 12.2)),
        ("ember-cave3-trail-marker", (15.0, 9.6)), ("ember-cave3-trail-marker", (21.2, 9.6)), ("ember-cave3-trail-marker", (15.6, 14.0)),
        ("ember-cave3-trail-marker", (21.0, 13.8)), ("ember-cave3-trail-marker", (24.6, 12.8)), ("ember-cave3-trail-marker", (30.6, 12.2)),
        ("ember-cave3-trail-marker", (18.8, 5.4)), ("ember-cave3-trail-marker", (21.2, 18.4)), ("ember-cave3-trail-marker", (11.2, 9.4)),
        ("cave-glowing-mushrooms", (6.0, 4.8)), ("cave-glowing-mushrooms", (13.6, 4.4)), ("cave-glowing-mushrooms", (23.0, 4.0)),
        ("cave-glowing-mushrooms", (31.4, 7.6)), ("cave-glowing-mushrooms", (6.8, 21.0)), ("cave-glowing-mushrooms", (15.2, 20.0)),
        ("cave-glowing-mushrooms", (24.2, 21.0)), ("cave-glowing-mushrooms", (31.6, 17.6)),
        ("ember-cave3-shelf-boulder", (3.0, 10.6)), ("ember-cave3-shelf-boulder", (33.6, 13.2)), ("ember-cave3-shelf-boulder", (14.0, 21.4)),
        ("ember-cave3-shelf-boulder", (29.4, 20.8)),
    ]
    for pid, spot in props:
        c, r = free(*spot)
        decorations.append({"id": pid, "x": c, "y": r})

    # --- checks -----------------------------------------------------------------------------------------------------------
    walk = {k for k, t in tiles.items() if t in ("plains", "nave", "ruins", "hill")}
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
    must = [connector] + [(n["x"], n["y"]) for n in neutrals] + [(p["x"], p["y"]) for p in players] + [(dd["x"], dd["y"]) for dd in decorations]
    for m in must:
        if m not in seen:
            raise SystemExit(f"cut off from the party: {m}")
    walk = {k for k, t in tiles.items() if t in ("plains", "nave", "ruins", "hill")}

    grid = [tiles.get((c, r), "void") for r in range(ROWS) for c in range(COLS)]
    elev = [float(level.get((c, r), 0)) for r in range(ROWS) for c in range(COLS)]
    d = dict(d0)
    d.update({
        "cols": COLS, "rows": ROWS, "tiles": grid, "tileVariants": [None] * (COLS * ROWS), "tileRots": [],
        "terrainElevations": elev, "decorations": decorations, "playerSpawns": players, "neutralSpawns": neutrals,
        "environment": "indoor", "underground": True, "emberTiles": "sporehaven-v01", "backgroundImage": "ember-misty-spore-basin-v01",
    })
    doc = {"serial": int(out[-8:-5]), "savedAt": int(time.time() * 1000), "draft": d}
    open(out, "w", encoding="utf-8", newline="\n").write(json.dumps(doc, ensure_ascii=False, indent=2))
    counts = {}
    for t in grid:
        counts[t] = counts.get(t, 0) + 1
    print(f"{out}: {COLS}x{ROWS}, {counts}, {len(neutrals)} people, {len(players)} heroes, {len(decorations)} decorations, "
          f"{len(walk)} walkable cells all reachable ({len(scraps)} stray cells dropped)")


if __name__ == "__main__":
    main()
