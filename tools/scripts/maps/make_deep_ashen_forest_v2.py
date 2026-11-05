"""Deep Ashen Forest, Trilha das Cinzas: version 002, built ON TOP of 001 (the user, 2026-11-05: "cada um que passa melhora um
pouquinho"). Every tile, height, decoration, spawn, NPC and line of 001 is kept as it was; this pass only ADDS:

  - the burnt forest made dense: ~170 charred trees (3D, blocking) in the woods and on the ridges, never on the trail's edge;
  - litter of the fire along the trail (charred stumps, burnt branches, ash and leaves, roots) and moss rocks on the ridges;
  - Iria's "marcas de carvão": charred stumps every few hexes beside the trail, from the party's start to the gorge's passage;
  - the vigias' post (the objective's "investigue os vigias"): camp, campfire, crates, sacks, a third crossbowman, a chest;
  - the cultists' dig in the north-east ridge ("procurando alguma coisa sob as raízes"): a pit dug into the ridge, embers burning
    in it, braziers, the reliquary and a containment box on its rim, a ladder, the dig's foreman, a chest;
  - three smouldering ash pits in the woods (the embers that "ainda ardem sob a floresta"): holes with fire at the bottom, two
    Espectros de Brasa beside them;
  - more monsters: a wolf pack in the south-west woods, a giant spider in a webbed lair in the west, burnt saplings in the gorge;
  - two new people (each with its own look): Lia, a lost courier hiding off the trail's north leg, and Tiago, a woodsman by the
    south-west pit; one more question for Iria and for Bento (their lines of 001 untouched, a reply added);
  - stand-ins for 001's nine `ember-nature-*` props, which are no longer in the editor catalog (so they draw nothing): each kept
    where it was and a catalogued piece of the same kind placed beside it.

    python -I tools/scripts/maps/make_deep_ashen_forest_v2.py content/maps/ember/deep-ashen-forest001.json content/maps/ember/deep-ashen-forest002.json

The script fails if the party cannot reach the exit, the passage to the gorge, every person, every foe and every chest, or if any
prop stands on a unit. 001 is only read.
"""
import copy
import json
import math
import random
import sys
import time

CATALOG = "content/data/editor-catalog.json"


def neighbors(c, r, cols, rows):
    odd = [(1, 0), (-1, 0), (1, -1), (0, -1), (1, 1), (0, 1)]
    even = [(1, 0), (-1, 0), (0, -1), (-1, -1), (0, 1), (-1, 1)]
    for dc, dr in (odd if r & 1 else even):
        if 0 <= c + dc < cols and 0 <= r + dr < rows:
            yield c + dc, r + dr


def cube(c, r):
    x = c - (r - (r & 1)) // 2
    return x, r, -x - r


def hexdist(a, b):
    ax, ay, az = cube(*a)
    bx, by, bz = cube(*b)
    return max(abs(ax - bx), abs(ay - by), abs(az - bz))


def ring(center, radius, cols, rows):
    out = []
    for r in range(max(0, center[1] - radius - 1), min(rows, center[1] + radius + 2)):
        for c in range(max(0, center[0] - radius - 1), min(cols, center[0] + radius + 2)):
            if hexdist(center, (c, r)) <= radius:
                out.append((c, r))
    return out


def main():
    src, out = sys.argv[1], sys.argv[2]
    doc0 = json.load(open(src, encoding="utf-8"))
    d0 = doc0["draft"]
    cat = json.load(open(CATALOG, encoding="utf-8"))
    decs, info = cat["decorations"], cat["editor"]["decorationInfo"]
    C, R = d0["cols"], d0["rows"]
    d = copy.deepcopy(d0)
    rng = random.Random(20261105)

    tile = {(i % C, i // C): t for i, t in enumerate(d["tiles"])}
    elev = {(i % C, i // C): float(e) for i, e in enumerate(d["terrainElevations"])}
    trail = {k for k, t in tile.items() if t == "plains"}

    def near_trail(cell, k):
        return any(hexdist(cell, p) <= k for p in trail if abs(p[0] - cell[0]) <= k + 1 and abs(p[1] - cell[1]) <= k + 1)

    # --- what 001 already holds ---------------------------------------------------------------------------------------------
    taken = {}  # cell -> what stands there (decoration footprints, units)

    def footprint(deco):
        fp = (decs.get(deco["id"]) or {}).get("footprint") or [{"dx": 0, "dy": 0}]
        return [(deco["x"] + f["dx"], deco["y"] + f["dy"]) for f in fp]

    def blocks(deco):
        if "blocksPath" in deco:
            return bool(deco["blocksPath"])
        return bool((info.get(deco["id"]) or {}).get("blocksByDefault"))

    for deco in d["decorations"]:
        for cell in footprint(deco):
            taken[cell] = deco["id"]
    units = [(u["x"], u["y"]) for k in ("playerSpawns", "enemySpawns", "neutralSpawns") for u in d[k]]
    for cell in units:
        taken[cell] = "unit"
    party = [(u["x"], u["y"]) for u in d["playerSpawns"]]
    exit_deco = next(x for x in d["decorations"] if x["id"] == "dungeon-exit")
    connector = next(x for x in d["decorations"] if x["id"] == "floor-connector")
    landmarks = [(exit_deco["x"], exit_deco["y"]), (connector["x"], connector["y"])]

    keep_clear = set()  # cells nothing new may stand on (units, the waypoints and their mouths, pit floors)
    for cell in units + landmarks:
        keep_clear.update(ring(cell, 1, C, R))

    added = []

    def free(cell, allow_tiles=None):
        return (0 <= cell[0] < C and 0 <= cell[1] < R and cell not in taken and cell not in keep_clear
                and (allow_tiles is None or tile[cell] in allow_tiles))

    def put(pid, cell, **extra):
        deco = {"id": pid, "x": cell[0], "y": cell[1]}
        deco.update(extra)
        cells = footprint(deco)
        if not all(free(c) for c in cells):
            return False
        for c in cells:
            taken[c] = pid
        added.append(deco)
        return True

    def put_near(pid, cell, radius=3, allow_tiles=None, **extra):
        for k in range(0, radius + 1):
            options = [c for c in ring(cell, k, C, R) if hexdist(c, cell) == k and free(c, allow_tiles)]
            rng.shuffle(options)
            for c in options:
                if put(pid, c, **extra):
                    return c
        raise SystemExit(f"no room for {pid} near {cell}")

    def spot_near(cell, radius=3, allow_tiles=None):
        for k in range(0, radius + 1):
            for c in sorted(ring(cell, k, C, R)):
                if hexdist(c, cell) == k and free(c, allow_tiles):
                    return c
        raise SystemExit(f"no free hex near {cell}")

    # --- holes: the cultists' dig and three smouldering ash pits --------------------------------------------------------------
    fx = d.setdefault("elementalFx", [])
    fx_serial = [0]

    def fire(cell):
        fx_serial[0] += 1
        fx.append({"id": f"fx-deepash2-{fx_serial[0]:03d}", "kind": "fire", "x": cell[0], "y": cell[1], "family": "regular"})

    def dig_pit(center, depth):
        cells = ring(center, 1, C, R)
        if any(c in taken or c in keep_clear for c in cells):
            raise SystemExit(f"pit at {center} would swallow something of 001")
        floor = min(elev[c] for c in cells)
        for c in cells:
            elev[c] = max(0.0, floor - depth + 1)
        elev[center] = max(0.0, floor - depth)
        keep_clear.add(center)
        return cells

    dig = (47, 10)
    dig_pit(dig, 3)
    fire(dig)
    pits = [(10, 30), (18, 40), (52, 34)]
    for p in pits:
        dig_pit(p, 3)
        fire(p)

    # --- the people: two new ones, one more question for Iria and Bento ---------------------------------------------------------
    def say(npc_id, speaker, portrait, lines):
        return {"id": npc_id, "startId": lines[0]["id"],
                "lines": [dict(line, speaker=speaker, portrait=portrait) for line in lines]}

    neutrals = d["neutralSpawns"]
    iria = next(n for n in neutrals if n["name"].startswith("Iria"))
    bento = next(n for n in neutrals if n["name"].startswith("Bento"))
    iria["dialog"]["lines"][0]["replies"].insert(0, {"text": "E as covas que soltam fumaça na mata?", "next": "covas"})
    iria["dialog"]["lines"].append({"id": "covas", "speaker": "Iria", "portrait": iria["classId"],
                                    "text": "Não pise na beira. A cinza cede e embaixo é brasa viva. Vi coisas saindo delas à noite, "
                                            "feitas de fumaça e fagulha. Contorne pela trilha.", "next": None})
    bento["dialog"]["lines"][0]["replies"].insert(0, {"text": "Quem são os vigias com bestas?", "next": "vigias"})
    bento["dialog"]["lines"].append({"id": "vigias", "speaker": "Bento", "portrait": bento["classId"],
                                     "text": "Vigia nenhum. São os olhos dos cultistas. Ficam no posto da clareira de cima e atiram "
                                             "em quem sobe para o lado do cume, onde eles cavam. Eu passo de cabeça baixa.", "next": None})

    lia_spot = spot_near((29, 13), 3, ("woods",))
    taken[lia_spot] = "unit"
    keep_clear.update(ring(lia_spot, 1, C, R))
    neutrals.append({"name": "Lia, mensageira perdida", "classId": "lostCourier", "x": lia_spot[0], "y": lia_spot[1], "level": 3,
                     "dialog": say("deep-ash-lia", "Lia", "lostCourier", [
                         {"id": "start", "text": "Não grite. Eles têm bestas. Eu levava uma carta para a Torre de Vigia e saí da "
                                                 "trilha quando vi o fogo no cume. Desde então estou aqui, escondida.",
                          "replies": [{"text": "Que fogo no cume?", "next": "cume"},
                                      {"text": "Que carta?", "next": "carta"},
                                      {"text": "Fique escondida. Nós passamos."}]},
                         {"id": "cume", "text": "Eles cavam um buraco no alto, a nordeste, e a terra lá dentro brilha. Tiraram uma "
                                                "caixa de pedra e um relicário. O capataz grita com todos para cavar mais fundo.",
                          "next": None},
                         {"id": "carta", "text": "Do carvoeiro da garganta para o capitão da Torre: três brasas seladas sob a "
                                                 "ponte velha, e alguém tentando quebrar o selo. Eu não sei ler. Foi o que ele "
                                                 "me disse.", "next": None}])})
    tiago_spot = spot_near((21, 38), 3, ("woods", "plains"))
    taken[tiago_spot] = "unit"
    keep_clear.update(ring(tiago_spot, 1, C, R))
    neutrals.append({"name": "Tiago, lenhador", "classId": "woodsman", "x": tiago_spot[0], "y": tiago_spot[1], "level": 4,
                     "dialog": say("deep-ash-tiago", "Tiago", "woodsman", [
                         {"id": "start", "text": "Cuidado com a cova ali. Ontem o chão engoliu meu carrinho de lenha. E os lobos "
                                                 "de cinza rondam a mata do oeste, perto da clareira baixa.",
                          "replies": [{"text": "Lobos de cinza?", "next": "lobos"},
                                      {"text": "Tem mais alguma coisa na mata?", "next": "aranha"},
                                      {"text": "Obrigado pelo aviso."}]},
                         {"id": "lobos", "text": "Lobos comuns, só que comem o que o fogo deixa. O pelo ficou cinza de tanto "
                                                 "rolar na cinza. Andam em bando de três.", "next": None},
                         {"id": "aranha", "text": "Uma aranha do tamanho de uma carroça fez ninho nas árvores mortas do oeste, "
                                                  "debaixo do morro. Tem teia por todo lado e um baú de algum viajante que não "
                                                  "voltou.", "next": None}])})

    # --- the foes: 001's five kept, the new ones added ---------------------------------------------------------------------------
    enemies = d["enemySpawns"]

    def foe(name, cls, cell, level, radius=3, room=0):
        for k in range(0, radius + 1):
            for c in sorted(ring(cell, k, C, R)):
                if hexdist(c, cell) == k and free(c) and all(free(n) for n in ring(c, room, C, R)):
                    enemies.append({"name": name, "classId": cls, "x": c[0], "y": c[1], "level": level})
                    taken[c] = "unit"
                    for n in ring(c, max(room, 1), C, R):
                        keep_clear.add(n)
                    return c
        raise SystemExit(f"no room for {name} near {cell}")

    foe("Vigia da Mata", "baseCrossbowman", (41, 17), 7)
    foe("Capataz da Escavação", "cultistV2", (46, 12), 9)
    foe("Cultista das Cinzas", "cultist", (49, 12), 7)
    foe("Lobo de Cinza", "mordavianWolf", (10, 37), 8)
    foe("Lobo de Cinza", "mordavianWolf", (12, 38), 8)
    foe("Lobo de Cinza", "mordavianWolf", (9, 39), 8)
    foe("Espectro de Brasa", "emberedWraith", (12, 31), 9)
    foe("Espectro de Brasa", "emberedWraith", (50, 35), 9)
    spider = foe("Tecelã das Cinzas", "giantSpider", (9, 17), 10, radius=4, room=1)
    foe("Muda Calcinada", "sapling", (29, 30), 7)
    foe("Muda Calcinada", "sapling", (31, 32), 7)
    foe("Muda Calcinada", "sapling", (28, 33), 7)

    # --- the places ---------------------------------------------------------------------------------------------------------------
    # the vigias' post at the north edge of the high clearing
    post = (40, 18)
    put_near("wilds-camp", post, 2)
    put_near("city-campfire", (41, 19), 2)
    put_near("ember-props-crate-v02", (39, 18), 2)
    put_near("ember-props-sacks-v02", (42, 18), 2)
    put_near("city-log-stack", (38, 19), 3)
    put_near("ember-treasure-weapon-rack-v02", (43, 18), 3)
    put_near("ember-polyhaven-treasure-chest", (39, 17), 3, chestTier="better")
    # the cultists' dig
    put_near("wilds-brazier-tripod", (45, 9), 2)
    put_near("wilds-brazier-tripod", (49, 8), 2)
    put_near("ember-ember-ancient-reliquary", (49, 10), 2)
    put_near("ember-ember-containment-box", (45, 11), 2)
    put_near("ember-polyhaven-wooden-ladder-02", (48, 11), 2)
    put_near("ember-dungeon-bone-pile", (44, 11), 3)
    put_near("ember-polyhaven-wooden-axe", (46, 8), 3)
    put_near("ember-polyhaven-treasure-chest", (51, 11), 3, chestTier="better")
    # the spider's lair
    for cell in ((7, 15), (11, 15), (8, 19), (12, 18)):
        put_near("wilds-bark-pile", cell, 2)
    put_near("ember-dungeon-bone-pile", (11, 19), 2)
    put_near("cave-webbed-chest", (7, 18), 2)
    put_near("ember-polyhaven-treasure-chest", (6, 17), 3, chestTier="base")
    # the ash pits' rims
    for p in pits + [dig]:
        for cell in ring(p, 2, C, R):
            if hexdist(cell, p) == 2 and rng.random() < 0.35 and free(cell):
                put(rng.choice(["wilds-charred-stump", "wilds-burnt-branch-pile", "wilds-mossy-charred-log"]), cell, blocksPath=False)
    put_near("ember-polyhaven-treasure-chest", (17, 42), 3, chestTier="base")
    put_near("wilds-abandoned-cart", (20, 41), 3)

    # stand-ins beside 001's props that left the catalog
    stand_in = {"ember-nature-ancient-tree-roots": "ember-wisp-revisited-roots-and-fungi",
                "ember-nature-rotten-fallen-log": "ember-wisp-revisited-rotted-hollow-log-v02",
                "ember-nature-exposed-roots": "wilds-exposed-roots",
                "ember-nature-root-arch": "ember-wisp-revisited-root-arch-v02",
                "ember-nature-lichen-rocks": "ember-polyhaven-rock-moss-set-01-rock03",
                "ember-nature-dense-bush": "wilds-dry-bush"}
    for old in list(d0["decorations"]):
        if old["id"] in stand_in and old["id"] not in decs:
            put_near(stand_in[old["id"]], (old["x"], old["y"]), 3)

    # Iria's charcoal marks: the trail's way from the party to the gorge's passage
    start, goal = party[0], landmarks[1]
    prev, todo = {start: None}, [start]
    while todo and goal not in prev:
        nxt = []
        for cur in todo:
            for nb in neighbors(*cur, C, R):
                if nb not in prev and (nb in trail or nb == goal or tile[nb] != "void"):
                    cost_ok = nb in trail or nb == goal or hexdist(nb, start) <= 2
                    if cost_ok:
                        prev[nb] = cur
                        nxt.append(nb)
        todo = nxt
    if goal not in prev:
        raise SystemExit("no trail from the party to the gorge's passage")
    way, cur = [], goal
    while cur:
        way.append(cur)
        cur = prev[cur]
    way.reverse()
    marks = 0
    for i in range(4, len(way) - 2, 5):
        side = [n for n in neighbors(*way[i], C, R) if n not in trail and free(n)]
        if side:
            put(rng.choice(["wilds-charred-stump", "wilds-burnt-branch-pile"]), sorted(side)[0], blocksPath=False)
            marks += 1

    # moss rocks on the ridges
    rocks = ["ember-polyhaven-rock-moss-set-01-rock01", "ember-polyhaven-rock-moss-set-01-rock03", "ember-polyhaven-rock-moss-set-02-rock07",
             "ember-polyhaven-rock-moss-set-02-rock11", "ember-polyhaven-rock-moss-set-02-rock13", "ember-polyhaven-boulder-01",
             "rocks-3d-grey-outcrop"]
    ridge = [k for k, t in tile.items() if t == "hill" and not near_trail(k, 2)]
    rng.shuffle(ridge)
    placed_rocks = 0
    for cell in ridge:
        if placed_rocks >= 26:
            break
        if all(not str(taken.get(n, "")).startswith(("ember-polyhaven-rock", "ember-polyhaven-boulder", "rocks-3d")) for n in ring(cell, 3, C, R)):
            if put(rng.choice(rocks), cell, blocksPath=True):
                placed_rocks += 1

    # the burnt forest
    near_kinds = [("tree-3d-dead-oak", 3), ("tree-3d-dead-oak-v02", 3), ("tree-3d-dead-snag", 3), ("tree-3d-twisted-stump", 2),
                  ("ember-polyhaven-wisp-tree-small-02-bare-v01", 2)]
    deep_kinds = near_kinds + [("ember-wisp-revisited-tall-burnt-pine-v02", 6), ("ember-wisp-revisited-lightning-split-v02", 2),
                               ("ember-wisp-revisited-hollow-snag-v02", 2), ("ember-wisp-revisited-broken-crown-v02", 2),
                               ("ember-wisp-revisited-spiral-twisted-v02", 1), ("ember-wisp-revisited-uprooted-leaning-v03", 1),
                               ("ember-wisp-revisited-giant-dead-oak-v02", 1), ("ember-polyhaven-wisp-fir-tree-01-bare-v01", 3)]

    def pick(kinds):
        total = sum(w for _, w in kinds)
        x = rng.uniform(0, total)
        for k, w in kinds:
            x -= w
            if x <= 0:
                return k
        return kinds[-1][0]

    # groves and gaps: a smooth field from a few random centres
    groves = [(rng.uniform(0, C), rng.uniform(0, R), rng.uniform(5, 11)) for _ in range(16)]

    def density(cell):
        v = 0.25
        for gx, gy, gr in groves:
            dd = math.hypot(cell[0] - gx, (cell[1] - gy) * 0.866)
            v += 0.6 * math.exp(-(dd / gr) ** 2)
        return min(v, 0.95)

    trees = []
    cand = [k for k, t in tile.items() if t in ("woods", "hill")]
    rng.shuffle(cand)
    for cell in cand:
        if len(trees) >= 170:
            break
        if not free(cell) or near_trail(cell, 1) or rng.random() > density(cell):
            continue
        if any(hexdist(cell, t) < 2 for t in trees if abs(t[0] - cell[0]) <= 2 and abs(t[1] - cell[1]) <= 2):
            continue
        kinds = deep_kinds if not near_trail(cell, 2) else near_kinds
        if put(pick(kinds), cell, blocksPath=True):
            trees.append(cell)

    # litter along the trail and in the clearings
    litter = ["wilds-charred-stump", "wilds-burnt-branch-pile", "wilds-mossy-charred-log", "wilds-dry-twig-pile", "wilds-bark-pile",
              "wilds-dead-fern-pile", "wilds-dry-bush", "wilds-bare-branch", "wilds-root-tangle", "ember-wisp-revisited-roots-and-fungi",
              "wilds-hollow-stump"]
    edge = [k for k, t in tile.items() if t == "woods" and near_trail(k, 1)]
    rng.shuffle(edge)
    n_litter = 0
    for cell in edge:
        if n_litter >= 70:
            break
        if rng.random() < 0.35 and free(cell):
            if put(rng.choice(litter), cell, blocksPath=False):
                n_litter += 1
    ash = [k for k in trail if not any(hexdist(k, p) <= 1 for p in party)]
    rng.shuffle(ash)
    n_ash = 0
    for cell in ash:
        if n_ash >= 30:
            break
        if free(cell) and rng.random() < 0.5:
            if put("ember-wisp-revisited-ash-leaf-tile", cell, blocksPath=False):
                n_ash += 1

    d["decorations"] = d0["decorations"] + added
    d["terrainElevations"] = [elev[(i % C, i // C)] for i in range(C * R)]

    # --- checks -------------------------------------------------------------------------------------------------------------------
    blocked = set()
    for deco in d["decorations"]:
        if blocks(deco):
            blocked.update(footprint(deco))
    for kind in ("playerSpawns", "enemySpawns", "neutralSpawns"):
        for u in d[kind]:
            if (u["x"], u["y"]) in blocked:
                raise SystemExit(f"{u['name']} stands on a blocking prop at {(u['x'], u['y'])}")
            hit = [x["id"] for x in added if (u["x"], u["y"]) in footprint(x)]
            if hit:
                raise SystemExit(f"{u['name']} at {(u['x'], u['y'])} under {hit}")
    walk = {k for k, t in tile.items() if t != "void" and k not in blocked}
    seen, todo = {party[0]}, [party[0]]
    while todo:
        cur = todo.pop()
        for nb in neighbors(*cur, C, R):
            if nb in walk and nb not in seen:
                seen.add(nb)
                todo.append(nb)

    def reachable_beside(cell):
        return cell in seen or any(n in seen for n in neighbors(*cell, C, R))

    must = {"exit": (exit_deco["x"], exit_deco["y"]), "passage": (connector["x"], connector["y"])}
    for kind in ("playerSpawns", "enemySpawns", "neutralSpawns"):
        for u in d[kind]:
            must[f"{u['name']} {(u['x'], u['y'])}"] = (u["x"], u["y"])
    for x in added:
        if "treasure-chest" in x["id"]:
            must[f"chest {(x['x'], x['y'])}"] = (x["x"], x["y"])
    lost = [k for k, cell in must.items() if not reachable_beside(cell)]
    if lost:
        raise SystemExit(f"cut off from the party: {lost}")
    unknown = sorted({x["id"] for x in added if x["id"] not in decs})
    if unknown:
        raise SystemExit(f"not in the editor catalog: {unknown}")

    doc = {"serial": int(out[-8:-5]), "savedAt": int(time.time() * 1000), "draft": d}
    open(out, "w", encoding="utf-8", newline="\n").write(json.dumps(doc, ensure_ascii=False, indent=2))
    kinds = {}
    for x in added:
        kinds[x["id"]] = kinds.get(x["id"], 0) + 1
    print(f"{out}: 001's {len(d0['decorations'])} decorations kept + {len(added)} new ({len(trees)} trees, {placed_rocks} rocks, "
          f"{n_litter} litter, {n_ash} ash tiles, {marks} charcoal marks); foes {len(d0['enemySpawns'])} -> {len(enemies)}; "
          f"people {len(d0['neutralSpawns'])} -> {len(neutrals)}; fire {len(fx)}; pits {len(pits) + 1}; "
          f"{len(seen)} of {len(walk)} walkable hexes reached; all {len(must)} required places reachable")
    print("spider at", spider, "| Lia at", lia_spot, "| Tiago at", tiago_spot)
    print(json.dumps(kinds, ensure_ascii=False))


if __name__ == "__main__":
    main()
