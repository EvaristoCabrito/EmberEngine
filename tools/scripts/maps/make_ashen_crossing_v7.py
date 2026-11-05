"""Ashen Forest Crossing 007: the map remade so that it makes sense (the user, 2026-11-05: "uma versão que faz sentido", "zero inimigos",
"uma rua que não tem começo nem fim ... a rua só pode ficar no mapa. Acabou o tabuleiro, acabou a rua", "usa os recursos 3D", "já tem
panorama, já tem tudo, só coloca", "esse branco não é aceitável"). 006 is only read (its mission text, lighting, weather and panorama
are kept); everything on the board is new:

  - 44 x 64 hexes (006 was 88 x 128, its 32 foes so far apart that none was ever in sight);
  - the cart road is born and dies INSIDE the board: it starts at the party's camp by the south edge, climbs to the crossing
    and ends at the northern passage to Deep Ashen Forest (the exit); nothing points off the board;
  - the crossing itself: a ravine of ash cut across the whole board (its floor one level down, its rims one level up), closed by
    rocks and fallen trunks everywhere but the old stone causeway in the middle, the one way over;
  - encounters with a reason, each a group the party meets as it walks: the brigands' camp holding the south bank of the
    causeway (crates, barrels, a brazier, their chest), the cultists' watch on the north bank, a wolf pack in the western woods,
    the ash wanderers (zombies) in a burnt clearing to the east and in the ruined waystation, the keepers of the passage at the exit;
  - all 3D, nothing 2D: the charred trees of the Wisp / Bosque Morto / Polyhaven kits in the woods, Polyhaven rocks on the ravine,
    crates / barrels / braziers / chests in the camps, dead trunks and hollow logs as litter;
  - the dark ground tiles of the Deep Ashen Forest (`deep-ashen-v01`: charred woods, loose ash on the road), the Bosque Morto's
    forest-floor photo around the board (backdrop-fit.json entry "...v06-wide-alpha.png#cinzas") and 006's own panorama painting.

    python -I tools/scripts/maps/make_ashen_crossing_v7.py content/maps/ember/ashen-forest-crossing006.json content/maps/ember/ashen-forest-crossing007.json

The script fails if any foe, chest or the exit cannot be reached from the party, if the ravine can be crossed anywhere but the
causeway, or if a prop stands on a unit.
"""
import copy
import json
import math
import random
import sys
import time

CATALOG = "content/data/editor-catalog.json"
C, R = 44, 64


def neighbors(c, r):
    odd = [(1, 0), (-1, 0), (1, -1), (0, -1), (1, 1), (0, 1)]
    even = [(1, 0), (-1, 0), (0, -1), (-1, -1), (0, 1), (-1, 1)]
    for dc, dr in (odd if r & 1 else even):
        if 0 <= c + dc < C and 0 <= r + dr < R:
            yield c + dc, r + dr


def cube(c, r):
    x = c - (r - (r & 1)) // 2
    return x, r, -x - r


def hexdist(a, b):
    ax, ay, az = cube(*a)
    bx, by, bz = cube(*b)
    return max(abs(ax - bx), abs(ay - by), abs(az - bz))


def ring(center, radius):
    out = []
    for r in range(max(0, center[1] - radius - 1), min(R, center[1] + radius + 2)):
        for c in range(max(0, center[0] - radius - 1), min(C, center[0] + radius + 2)):
            if hexdist(center, (c, r)) <= radius:
                out.append((c, r))
    return out


def hex_path(a, b):
    """The hexes of a straight walk from a to b, neighbour by neighbour (greedy on the hex distance)."""
    out = [a]
    cur = a
    while cur != b:
        cur = min(neighbors(*cur), key=lambda n: (hexdist(n, b), abs(n[0] - b[0])))
        out.append(cur)
    return out


def main():
    src, out = sys.argv[1], sys.argv[2]
    d0 = json.load(open(src, encoding="utf-8"))["draft"]
    cat = json.load(open(CATALOG, encoding="utf-8"))
    decs, info = cat["decorations"], cat["editor"]["decorationInfo"]
    rng = random.Random(20261105)

    def is3d(pid):
        return bool((decs.get(pid) or {}).get("model3d"))

    # --- the board: woods everywhere, the road, the ravine, the clearings --------------------------------------------------------
    tile = {(c, r): "woods" for r in range(R) for c in range(C)}
    elev = {(c, r): 0 for r in range(R) for c in range(C)}

    # the road: born at the camp by the south edge, over the causeway, dead at the passage in the north
    road_points = [(21, 60), (19, 54), (23, 48), (20, 42), (22, 36), (22, 32), (22, 30), (25, 25), (29, 19), (27, 12), (28, 6)]
    centre = []
    for a, b in zip(road_points, road_points[1:]):
        centre += hex_path(a, b)[:-1]
    centre.append(road_points[-1])
    road = set()
    for cell in centre:
        road.update(ring(cell, 1))
    # the ravine: rows 30-33 across the board, rims on 29 and 34
    ravine = {(c, r) for r in range(30, 34) for c in range(C)}
    rims = {(c, r) for r in (29, 34) for c in range(C)}
    causeway = {(c, r) for r in range(29, 35) for c in range(20, 25)}
    for cell in ravine:
        tile[cell], elev[cell] = "plains", -1
    for cell in rims:
        tile[cell], elev[cell] = "hill", 1
    for cell in road:
        tile[cell] = "ruins"  # the road: the trodden pale-ash tile (ashen-road-v01 "ruins"), lighter than the charred mat around it
    for cell in causeway:
        tile[cell], elev[cell] = "ruins", 0  # the old stone causeway, the one way over
    # clearings: the camp by the south edge, the brigands' bank, the cultists' bank, the zombies' burnt clearing, the waystation
    camp_s, bank_s, bank_n, clearing_e, waystation, wolves_w, passage = (21, 60), (22, 38), (26, 19), (35, 47), (12, 22), (8, 46), (28, 5)
    for centre_cell, rad in ((camp_s, 3), (bank_s, 4), (bank_n, 4), (clearing_e, 4), (wolves_w, 3), (passage, 3)):
        for cell in ring(centre_cell, rad):
            if cell not in ravine and cell not in rims and cell not in road:
                tile[cell] = "plains"
    for cell in ring(waystation, 2):
        tile[cell], elev[cell] = "ruins", 1
    for cell in ring(waystation, 3):
        if cell not in ring(waystation, 2):
            tile[cell] = "plains"
    # a knoll in the north-east, a dip of ash in the south-west
    for cell in ring((38, 10), 4):
        elev[cell] = 1
    for cell in ring((39, 9), 2):
        elev[cell] = 2
    for cell in ring((7, 56), 3):
        elev[cell] = -1

    # --- units -------------------------------------------------------------------------------------------------------------------
    taken = {}
    keep_clear = set()
    party_cells = [(21, 60), (20, 60), (22, 60), (21, 61), (20, 61), (22, 61)]
    heroes = [(u["name"], u["classId"], u.get("level", 8)) for u in d0["playerSpawns"]]
    player_spawns = []
    for (name, cls, lvl), cell in zip(heroes, party_cells):
        player_spawns.append({"name": name, "classId": cls, "x": cell[0], "y": cell[1], "level": lvl})
        taken[cell] = "unit"
        keep_clear.update(ring(cell, 1))
    exit_cell, connector_cell = (28, 3), (27, 4)
    for cell in (exit_cell, connector_cell):
        keep_clear.update(ring(cell, 1))

    enemies = []

    def free(cell):
        return 0 <= cell[0] < C and 0 <= cell[1] < R and cell not in taken and cell not in keep_clear

    def foe(name, cls, cell, level, radius=3):
        for k in range(0, radius + 1):
            options = [c for c in ring(cell, k) if hexdist(c, cell) == k and free(c) and c not in ravine and c not in rims]
            rng.shuffle(options)
            for c in options:
                enemies.append({"name": name, "classId": cls, "x": c[0], "y": c[1], "level": level})
                taken[c] = "unit"
                keep_clear.update(ring(c, 1))
                return c
        raise SystemExit(f"no room for {name} near {cell}")

    # the brigands' camp on the south bank of the causeway: they hold the only way over
    foe("Chefe dos Brigantes", "miliciaV2", (22, 37), 9)
    foe("Brigante da Travessia", "brigandCrossbowman", (20, 38), 8)
    foe("Brigante da Travessia", "brigandCrossbowman", (24, 38), 8)
    foe("Vigia do Acampamento", "archer", (21, 40), 8)
    foe("Vigia do Acampamento", "archer", (25, 36), 8)
    # the cultists' watch on the north bank
    # (well north of the causeway: the party crosses first, then meets them; the user, 2026-11-05: never a crowd of foes at once)
    foe("Guardião das Cinzas", "cultistV2", (25, 20), 9)
    foe("Guardião das Cinzas", "cultistV2", (28, 18), 9)
    foe("Sentinela do Norte", "miliciaV2", (26, 17), 8)
    foe("Sentinela do Norte", "miliciaV2", (24, 18), 8)
    # the wolf pack in the western woods, off the road
    for _ in range(3):
        foe("Lobo das Cinzas", "mordavianWolfFinal", wolves_w, 8, radius=3)
    # the ash wanderers in the burnt clearing to the east
    for _ in range(4):
        foe("Errante das Cinzas", "zombie", clearing_e, 8, radius=3)
    # the ruined waystation
    foe("Errante Calcinado", "zombie", (11, 22), 8)
    foe("Errante Calcinado", "zombie", (14, 23), 8)
    # the keepers of the passage, at the road's end
    foe("Guardião da Passagem", "cultistV2", (27, 9), 10)
    foe("Guardião da Passagem", "cultistV2", (30, 8), 10)
    foe("Besteiro da Passagem", "brigandCrossbowman", (28, 11), 9)

    # --- props (3D only) ---------------------------------------------------------------------------------------------------------
    added = []

    def footprint(deco):
        fp = (decs.get(deco["id"]) or {}).get("footprint") or [{"dx": 0, "dy": 0}]
        return [(deco["x"] + f["dx"], deco["y"] + f["dy"]) for f in fp]

    def put(pid, cell, **extra):
        if not is3d(pid):
            raise SystemExit(f"{pid} is not a 3D model")
        deco = {"id": pid, "x": cell[0], "y": cell[1]}
        deco.update(extra)
        cells = footprint(deco)
        if not all(free(c) for c in cells):
            return False
        for c in cells:
            taken[c] = pid
        added.append(deco)
        return True

    def put_near(pid, cell, radius=3, **extra):
        for k in range(0, radius + 1):
            options = [c for c in ring(cell, k) if hexdist(c, cell) == k and free(c) and c not in ravine]
            rng.shuffle(options)
            for c in options:
                if put(pid, c, **extra):
                    return c
        raise SystemExit(f"no room for {pid} near {cell}")

    # the exit and the passage (the same pieces 006 used, placed at the road's end)
    added.append({"id": "dungeon-exit", "x": exit_cell[0], "y": exit_cell[1]})
    added.append({"id": "floor-connector", "x": connector_cell[0], "y": connector_cell[1], "targetMapId": "deep-ashen-forest", "connectorDirection": "forward"})
    taken[exit_cell] = taken[connector_cell] = "waypoint"

    rocks = ["ember-polyhaven-boulder-01", "ember-polyhaven-namaqualand-boulder", "ember-polyhaven-coast-rocks", "rocks-3d-grey-outcrop",
             "ember-polyhaven-rock-moss-set-01-rock01", "ember-polyhaven-rock-moss-set-01-rock02", "ember-polyhaven-rock-moss-set-01-rock03",
             "ember-polyhaven-rock-moss-set-02-rock07", "ember-polyhaven-rock-moss-set-02-rock09", "ember-polyhaven-rock-moss-set-02-rock12"]
    trunks = ["ember-polyhaven-dead-tree-trunk-02", "ember-wisp-revisited-rotted-hollow-log-v02", "ember-winter-snow-fallen-log"]
    # the ravine closed: every floor hex outside the causeway gets a rock or a fallen trunk (blocking); the causeway stays open
    n_ravine = 0
    for cell in sorted(ravine):
        if cell in causeway:
            continue
        if free(cell) and put(rng.choice(rocks if rng.random() < 0.8 else trunks), cell, blocksPath=True):
            n_ravine += 1
    # rocks on the rims, loose (not every hex), never on the road or the causeway
    n_rim = 0
    for cell in sorted(rims):
        if cell in causeway or cell in road or rng.random() > 0.35:
            continue
        if free(cell) and put(rng.choice(rocks), cell, blocksPath=True):
            n_rim += 1

    # the party's camp by the south edge
    put_near("ember-watchtower-signal-brazier-v01", (23, 62), 2, blocksPath=True)
    put_near("ember-polyhaven-wooden-crate", (19, 62), 2, blocksPath=True)
    put_near("ember-props-sacks-v02", (18, 61), 2, blocksPath=False)
    put_near("ember-polyhaven-lantern-01", (24, 60), 2, blocksPath=False)
    # the brigands' camp
    put_near("ember-watchtower-signal-brazier-v02", (22, 39), 2, blocksPath=True)
    put_near("ember-props-crate-v02", (19, 40), 2, blocksPath=True)
    put_near("ember-polyhaven-wooden-barrels-01", (25, 40), 2, blocksPath=True)
    put_near("ember-props-sacks-v02", (20, 36), 2, blocksPath=False)
    put_near("ember-props-barrel-v02", (24, 35), 2, blocksPath=True)
    put_near("ember-polyhaven-treasure-chest", (23, 41), 3, chestTier="better")
    put_near("ember-polyhaven-wooden-lantern-01", (21, 38), 2, blocksPath=False)
    # the cultists' watch
    put_near("ember-watchtower-signal-brazier-v01", (26, 18), 2, blocksPath=True)
    put_near("ember-dungeon-bone-pile", (27, 21), 2, blocksPath=False)
    put_near("ember-polyhaven-crate-chest", (23, 17), 3, blocksPath=True)
    # the burnt clearing: bones and a stump ring
    put_near("ember-dungeon-bone-pile", clearing_e, 2, blocksPath=False)
    put_near("ember-dungeon-bone-pile", (37, 49), 2, blocksPath=False)
    put_near("ember-wisp-revisited-broken-stump", (33, 45), 2, blocksPath=True)
    put_near("ember-polyhaven-treasure-chest", (38, 45), 3, chestTier="good")
    # the ruined waystation: a broken arch, pillars, a well, a chest
    # ember-dungeon-broken-arch removed on the user's order (2026-11-05): tools/scripts/data/removed_decorations.py
    put_near("ember-dungeon-pillar", (10, 23), 2, blocksPath=True)
    put_near("ember-dungeon-pillar-v02", (14, 21), 2, blocksPath=True)
    put_near("ember-city-well", (13, 24), 2, blocksPath=True)
    put_near("ember-treasure-iron-banded-chest", (12, 23), 3, chestTier="better")
    # the passage: braziers at the mouth
    put_near("ember-watchtower-signal-brazier-v02", (26, 6), 2, blocksPath=True)
    put_near("ember-watchtower-signal-brazier-v02", (30, 6), 2, blocksPath=True)
    put_near("ember-dungeon-bone-pile", (29, 9), 2, blocksPath=False)

    # the charred forest: 3D trees on the woods, never on the road's edge, with groves and gaps
    # only the clean trunks without branches (the user, 2026-11-05: "puta árvore mal feita ... tá cheio de árvore boa, sem galho")
    kinds = [("ember-wisp-revisited-tall-burnt-pine-v02", 5), ("ember-wisp-revisited-hollow-snag-v02", 3), ("ember-bosque-morto-hollow-snag", 3),
             ("ember-polyhaven-dead-tree-trunk-02", 2), ("ember-wisp-revisited-broken-stump", 1)]
    kinds = [(k, w) for k, w in kinds if k in decs and is3d(k)]

    def pick():
        total = sum(w for _, w in kinds)
        x = rng.uniform(0, total)
        for k, w in kinds:
            x -= w
            if x <= 0:
                return k
        return kinds[-1][0]

    groves = [(rng.uniform(0, C), rng.uniform(0, R), rng.uniform(4, 9)) for _ in range(18)]

    def density(cell):
        v = 0.3
        for gx, gy, gr in groves:
            dd = math.hypot(cell[0] - gx, (cell[1] - gy) * 0.866)
            v += 0.5 * math.exp(-(dd / gr) ** 2)
        return min(v, 0.95)

    def near_road(cell, k):
        return any(hexdist(cell, p) <= k for p in road if abs(p[0] - cell[0]) <= k + 1 and abs(p[1] - cell[1]) <= k + 1)

    trees = []
    cand = [k for k, t in tile.items() if t == "woods"]
    rng.shuffle(cand)
    for cell in cand:
        # (the user, 2026-11-05: "não dá nem pra andar nessa bosta, olha quanta árvore", "para de encher de árvore": at most 60 trees, 3 hexes apart, 2 off the road)
        if len(trees) >= 60:
            break
        if not free(cell) or near_road(cell, 2) or cell in rims or rng.random() > density(cell):
            continue
        if any(hexdist(cell, t) < 3 for t in trees if abs(t[0] - cell[0]) <= 3 and abs(t[1] - cell[1]) <= 3):
            continue
        if put(pick(), cell, blocksPath=True):
            trees.append(cell)
    # litter beside the road (3D, not blocking)
    litter = ["ember-wisp-revisited-rotted-hollow-log-v02", "ember-wisp-revisited-broken-stump", "ember-wisp-revisited-ash-leaf-tile",
              "ember-polyhaven-dead-tree-trunk-02"]
    litter = [k for k in litter if k in decs and is3d(k)]
    edge = [k for k, t in tile.items() if t == "woods" and near_road(k, 1)]
    rng.shuffle(edge)
    n_litter = 0
    for cell in edge:
        if n_litter >= 60:
            break
        if rng.random() < 0.3 and free(cell) and put(rng.choice(litter), cell, blocksPath=False):
            n_litter += 1

    # --- the draft -----------------------------------------------------------------------------------------------------------------
    d = copy.deepcopy(d0)
    d["cols"], d["rows"] = C, R
    n = C * R
    d["tiles"] = [tile[(i % C, i // C)] for i in range(n)]
    d["terrainElevations"] = [elev[(i % C, i // C)] for i in range(n)]
    d["tileVariants"] = [0] * n
    if "tileRots" in d:
        d["tileRots"] = [0] * n
    if isinstance(d.get("waterLevels"), list):
        d["waterLevels"] = [0] * n
    d["heights"] = [0] * n if isinstance(d0.get("heights"), list) else d0.get("heights")
    d["decorations"] = added
    d["playerSpawns"], d["enemySpawns"], d["neutralSpawns"] = player_spawns, enemies, []
    d["emberTiles"] = "bosque-morto-v04"  # the Bosque Morto's ash ground (the user, 2026-11-05: "tem as cinzas perfeitas, usa aquelas cinzas"); the road is the ruins stone art
    d["timeOfDay"] = "day"  # the map's own test hour by day: 006's dusk light turned every grey ash tile pink (the user, 2026-11-05)
    d.pop("groundImage", None)  # the ground around the board comes from the panorama's own fit (bosque-ground-v01.jpg, backdrop-fit.json)
    d["briefing"] = ("A estrada de carroças das cinzas nasce no acampamento ao sul e morre na passagem do norte. No meio, a ravina de cinza "
                     "corta a floresta de lado a lado: a única travessia é a velha calçada de pedra, e os brigantes acamparam na sua margem. "
                     "Lobos rondam a mata a oeste, errantes das cinzas arrastam-se na clareira queimada e nas ruínas do posto, e os guardiões "
                     "do culto vigiam a margem norte e a passagem para a Deep Ashen Forest.")
    d["objective"] = "Siga a estrada, tome a travessia dos brigantes e alcance a passagem para a Deep Ashen Forest no fim dela."

    # --- checks -------------------------------------------------------------------------------------------------------------------
    def blocks(deco):
        if "blocksPath" in deco:
            return bool(deco["blocksPath"])
        return bool((info.get(deco["id"]) or {}).get("blocksByDefault"))

    blocked = set()
    for deco in added:
        if blocks(deco):
            blocked.update(footprint(deco))
    for u in player_spawns + enemies:
        if (u["x"], u["y"]) in blocked or any((u["x"], u["y"]) in footprint(x) for x in added):
            raise SystemExit(f"{u['name']} at {(u['x'], u['y'])} under a prop")
    walk = {k for k in tile if k not in blocked}
    seen, todo = {party_cells[0]}, [party_cells[0]]
    while todo:
        cur = todo.pop()
        for nb in neighbors(*cur):
            if nb in walk and nb not in seen:
                seen.add(nb)
                todo.append(nb)

    def reachable_beside(cell):
        return cell in seen or any(nb in seen for nb in neighbors(*cell))

    must = {"exit": exit_cell, "passage": connector_cell}
    for u in enemies:
        must[f"{u['name']} {(u['x'], u['y'])}"] = (u["x"], u["y"])
    for x in added:
        if "chest" in x["id"]:
            must[f"chest {(x['x'], x['y'])}"] = (x["x"], x["y"])
    lost = [k for k, cell in must.items() if not reachable_beside(cell)]
    if lost:
        raise SystemExit(f"cut off from the party: {lost}")
    # the ravine crossed only on the causeway: no walkable ravine hex outside it may touch the north bank from the south
    crossings = {cell for cell in ravine if cell in walk and cell not in causeway}
    if crossings:
        # a walkable ravine hex is fine as long as it is a dead end: check no path from the south rim to the north rim avoids the causeway
        south = {(c, 34) for c in range(C) if (c, 34) in walk and (c, 34) not in causeway}
        seen2, todo2 = set(south), list(south)
        while todo2:
            cur = todo2.pop()
            for nb in neighbors(*cur):
                if nb in walk and nb not in seen2 and nb not in causeway and 29 <= nb[1] <= 34:
                    seen2.add(nb)
                    todo2.append(nb)
        if any(cell[1] == 29 for cell in seen2):
            raise SystemExit("the ravine can be crossed off the causeway")
    unknown = sorted({x["id"] for x in added if x["id"] not in decs})
    if unknown:
        raise SystemExit(f"not in the editor catalog: {unknown}")
    not3d = sorted({x["id"] for x in added if not is3d(x["id"]) and x["id"] not in ("dungeon-exit", "floor-connector")})
    if not3d:
        raise SystemExit(f"2D pieces placed: {not3d}")

    doc = {"serial": int(out[-8:-5]), "savedAt": int(time.time() * 1000), "draft": d}
    open(out, "w", encoding="utf-8", newline="\n").write(json.dumps(doc, ensure_ascii=False, indent=2))
    counts = {}
    for x in added:
        counts[x["id"]] = counts.get(x["id"], 0) + 1
    print(f"{out}: {C}x{R}, road {len(road)} hexes, {len(added)} props ({len(trees)} trees, {n_ravine} on the ravine floor, {n_rim} on the rims, "
          f"{n_litter} litter), foes {len(enemies)}, {len(seen)} of {len(walk)} walkable hexes reached, all {len(must)} required places reachable")
    print(json.dumps(counts, ensure_ascii=False))


if __name__ == "__main__":
    main()
