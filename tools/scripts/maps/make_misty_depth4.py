"""O Underdark de Mordávia (mordavia-underdark), the Misty Cave's map 4, rebuilt from the user's art (Attachments/MistyCave/4rdFloor,
docs/MISTY_DEPTH4_PROMPTS.md): a new version written beside the old (the campaign plays the highest serial); the old file is only read.

    python -I tools/scripts/maps/make_misty_depth4.py content/maps/ember/mordavia-underdark002.json content/maps/ember/mordavia-underdark004.json

(003 was the first run, before the panorama arrived; 004 adds it, the user's "Mar subterrâneo sob estrelas de cristal".)

The crown of the Root-Crown (hex grid 52 x 38, pointy-top, odd rows shifted right), the colossal tree the party has climbed out onto:
  - the crown's boughs (plains: silver bark), broad roads four hexes wide over the void (the drop to the inner sea), from the west
    landing where the party climbs out (the connector back down to the Bacia dos Esporos) to the Enclave's great deck in the middle;
  - from the deck one bough runs north-east to the Mirante, the deck at the crown's edge whose connector is the way out: it opens the
    Atlas do Submundo (OverworldStage::kUnderworldExit, "underworld-atlas"); one north-west to the shrine deck; one south-east down to
    the Glasswater Sea's shore (snow, a level under the boughs) with its still water; a loop between the north-east and south-east boughs;
  - the Enclave's decks and the plank path between them (nave), the crown's knotted burls (hill, levels 1-2: high ground), broken
    branches and fallen crystal (ruins: cover);
  - the old map's chest on the shore; the old map's four enemies at their roles and five more of their own kinds.
The old map's settings (title, briefing, objective, win by escape, fog, mist, the intro dialogue, the party) are kept. The script fails
if any floor, spawn, the chest or a connector cannot be reached from the party.
"""
import json
import math
import sys
import time

COLS, ROWS = 52, 38
SQ = math.sqrt(3) / 2
ATLAS = "underworld-atlas"  # OverworldStage::kUnderworldExit


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


WEST, DECK, MIRANTE, SHRINE, SHORE = (3.2, 15.6), (24.0, 15.6), (45.6, 5.4), (11.0, 4.6), (41.0, 27.6)


def main():
    src, out = sys.argv[1], sys.argv[2]
    d0 = json.load(open(src, encoding="utf-8"))["draft"]
    tiles, level = {}, {}

    def carve(pred, terrain, over=None):
        for r in range(ROWS):
            for c in range(COLS):
                if pred(c, r) and (over is None or tiles.get((c, r)) in over):
                    tiles[(c, r)] = terrain

    # the shore of the Glasswater Sea, a level under the boughs, and its still water
    carve(lambda c, r: in_ellipse(c, r, *SHORE, 7.6, 4.0), "snow")
    for (c, r) in [k for k, t in tiles.items() if t == "snow"]:
        level[(c, r)] = -1
    carve(lambda c, r: in_ellipse(c, r, 43.8, 29.6, 4.4, 1.9), "water", over=("snow",))
    # the boughs
    boughs = [
        [WEST, (13.0, 16.4), DECK],
        [DECK, (33.0, 11.0), MIRANTE],
        [DECK, (32.0, 21.0), (38.6, 25.2)],
        [(13.0, 16.4), (12.2, 10.0), SHRINE],
        [(33.0, 11.0), (38.0, 17.0), (32.0, 21.0)],
    ]
    for pts in boughs:
        for a, b in zip(pts, pts[1:]):
            carve(lambda c, r: near_segment(c, r, a, b, 2.0), "plains", over=(None, "plains"))
    # the Enclave's decks and the plank path between them
    for (cx, cy), rx, ry in ((WEST, 2.8, 2.4), (DECK, 4.2, 3.0), (MIRANTE, 3.6, 2.6), (SHRINE, 3.0, 2.2)):
        carve(lambda c, r: in_ellipse(c, r, cx, cy, rx, ry), "nave", over=(None, "plains"))
    for pts in ([WEST, (13.0, 16.4), DECK], [DECK, (33.0, 11.0), MIRANTE]):
        for a, b in zip(pts, pts[1:]):
            carve(lambda c, r: near_segment(c, r, a, b, 0.7), "nave", over=("plains",))
    # the burls (high ground) and the broken branches (cover)
    for (cx, cy, rx, ry) in ((8.0, 14.0, 1.6, 1.2), (19.0, 17.6, 1.6, 1.1), (30.4, 12.4, 1.5, 1.1), (35.4, 22.8, 1.6, 1.1),
                             (11.4, 8.6, 1.2, 1.0), (39.4, 8.4, 1.4, 1.0), (37.4, 16.6, 1.3, 1.0)):
        for (c, r) in list(tiles):
            if tiles[(c, r)] == "plains" and in_ellipse(c, r, cx, cy, rx, ry):
                tiles[(c, r)] = "hill"
                level[(c, r)] = 2 if in_ellipse(c, r, cx, cy, rx * 0.55, ry * 0.5) else 1
    for (cx, cy) in ((15.4, 14.6), (28.0, 19.0), (41.0, 7.0), (35.0, 14.0), (13.2, 12.2), (22.0, 19.4), (34.0, 25.0)):
        carve(lambda c, r: in_ellipse(c, r, cx, cy, 1.1, 0.85), "ruins", over=("plains",))
    for k in [k for k, t in tiles.items() if t not in ("hill", "snow")]:
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
        c, r = free(3.4 + (k % 2) * 1.0, 15.0 + (k // 2) * 0.9, ("nave",))
        players.append(dict(p, x=c, y=r))

    decorations = []
    back = next((dd for dd in d0["decorations"] if dd.get("targetMapId") == "misty-spore-basin"), None)
    if back is None:
        raise SystemExit(f"{src} has no connector back to misty-spore-basin")
    cells = {}
    c, r = free(0.8, 15.6)
    decorations.append(dict(back, x=c, y=r))
    cells["misty-spore-basin"] = (c, r)
    c, r = free(48.6, 4.6, ("nave",))
    decorations.append({"id": "floor-connector", "x": c, "y": r, "targetMapId": ATLAS, "connectorDirection": "down"})
    cells[ATLAS] = (c, r)
    chest = next((dd for dd in d0["decorations"] if dd["id"].startswith("chest")), None)
    chest_cell = None
    if chest:
        c, r = free(36.4, 28.4, ("snow",))
        decorations.append(dict(chest, x=c, y=r))
        chest_cell = (c, r)
    rune = next((dd for dd in d0["decorations"] if dd["id"] == "rune-stone"), None)
    if rune:
        c, r = free(9.0, 3.6, ("nave",))
        decorations.append(dict(rune, x=c, y=r))

    by_name = {e["name"]: e for e in d0["enemySpawns"]}
    plan = [
        ("Morto da galeria", (16.6, 16.8)), ("Morto da galeria", (31.0, 20.0)), ("Morto da galeria", (40.0, 26.4)),
        ("Peregrino perdido", (12.4, 6.6)), ("Peregrino perdido", (26.4, 13.6)),
        ("Vigia das ruínas", (30.4, 12.4)), ("Vigia das ruínas", (35.4, 22.8)),
        ("Sentinela das profundezas", (24.6, 17.4)), ("Sentinela das profundezas", (44.0, 5.8)),
    ]
    enemies = []
    for name, spot in plan:
        if name not in by_name:
            raise SystemExit(f"{src} has no enemy named {name}")
        c, r = free(*spot)
        enemies.append(dict(by_name[name], x=c, y=r))

    props = [  # the user's map 4 art (content/decor-own-cave4.json); the fallen crystals and the shrines are the map's light
        ("ember-cave4-crystal", (6.2, 17.6)), ("ember-cave4-crystal", (10.6, 14.4)), ("ember-cave4-crystal", (17.6, 14.0)),
        ("ember-cave4-crystal", (21.0, 13.2)), ("ember-cave4-crystal", (27.6, 17.8)), ("ember-cave4-crystal", (29.0, 10.6)),
        ("ember-cave4-crystal", (36.6, 9.8)), ("ember-cave4-crystal", (42.6, 7.4)), ("ember-cave4-crystal", (33.6, 18.0)),
        ("ember-cave4-crystal", (37.6, 23.4)), ("ember-cave4-crystal", (44.6, 26.0)), ("ember-cave4-crystal", (13.6, 9.0)),
        ("ember-cave4-crystal", (38.6, 13.6)), ("ember-cave4-crystal", (24.0, 21.0)),
        ("ember-cave4-shrine", (9.6, 5.6)), ("ember-cave4-shrine", (13.0, 3.6)), ("ember-cave4-shrine", (21.4, 13.6)),
        ("ember-cave4-shrine", (46.6, 3.6)),
        ("ember-cave4-bough", (5.8, 13.8)), ("ember-cave4-bough", (16.4, 18.0)), ("ember-cave4-bough", (20.6, 12.8)),
        ("ember-cave4-bough", (27.4, 12.6)), ("ember-cave4-bough", (35.4, 11.6)), ("ember-cave4-bough", (43.0, 3.4)),
        ("ember-cave4-bough", (29.4, 22.0)), ("ember-cave4-bough", (39.6, 19.0)), ("ember-cave4-bough", (10.0, 9.8)),
        ("ember-cave4-burl", (8.8, 17.4)), ("ember-cave4-burl", (18.2, 15.0)), ("ember-cave4-burl", (31.6, 14.6)),
        ("ember-cave4-burl", (26.8, 20.4)), ("ember-cave4-burl", (40.6, 10.4)), ("ember-cave4-burl", (34.0, 20.0)),
    ]
    for pid, spot in props:
        c, r = free(*spot, ("plains", "nave", "ruins", "snow"))
        decorations.append({"id": pid, "x": c, "y": r})

    # --- checks -----------------------------------------------------------------------------------------------------------
    solid = {(dd["x"], dd["y"]) for dd in decorations if dd["id"] in ("ember-cave4-bough", "ember-cave4-burl")}
    walk = {k for k, t in tiles.items() if t in ("plains", "nave", "ruins", "hill", "snow")}
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
    must = list(cells.values()) + ([chest_cell] if chest_cell else []) + [(e["x"], e["y"]) for e in enemies] + \
           [(p["x"], p["y"]) for p in players] + [(dd["x"], dd["y"]) for dd in decorations if (dd["x"], dd["y"]) not in solid]
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
        "environment": "indoor", "underground": True, "emberTiles": "cave4-v01", "backgroundImage": "ember-mordavia-underdark-v01",
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
