"""Ashen Forest Crossing 008 = 006 as it is (the user, 2026-11-05: "eu falei para usar o mapa que já existia, que tinha uma rua gigante"),
with only what was wrong fixed:

  - the tiles: the very image that lies around the board (bosque-ground-v01.jpg, set ashen-ground-v01 at the same 16-unit scale),
    so board and ground are one; the road and the giant stone street keep their ruins stone art;
  - no ground image of its own (the white one with a road painted beyond the board is gone): the panorama's own fit lays the
    Bosque Morto's ash photo around the board (backdrop-fit.json "...v06-wide-alpha.png#cinzas");
  - the 32 foes of 006, all kept (names, classes, levels), gathered into seven groups along the way the party walks (the south
    road, the middle road, the west end of the stone street, the crossing's centre, the east end of the street, the north road,
    the exit), so that they are met instead of standing in pairs twenty rows apart ("zero inimigos");
  - everything else (board, heights, 006's 350 props, the exit, the passage, the mission text) untouched.

    python -I tools/scripts/maps/make_ashen_crossing_v8.py content/maps/ember/ashen-forest-crossing006.json content/maps/ember/ashen-forest-crossing008.json

Fails if a foe lands on a prop or cannot be reached from the party.
"""
import copy
import json
import random
import sys
import time

CATALOG = "content/data/editor-catalog.json"


def neighbors(c, r, C, R):
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


def main():
    src, out = sys.argv[1], sys.argv[2]
    d0 = json.load(open(src, encoding="utf-8"))["draft"]
    cat = json.load(open(CATALOG, encoding="utf-8"))
    decs, info = cat["decorations"], cat["editor"]["decorationInfo"]
    C, R = d0["cols"], d0["rows"]
    d = copy.deepcopy(d0)
    rng = random.Random(8)
    tile = {(i % C, i // C): t for i, t in enumerate(d["tiles"])}

    def footprint(deco):
        fp = (decs.get(deco["id"]) or {}).get("footprint") or [{"dx": 0, "dy": 0}]
        return [(deco["x"] + f["dx"], deco["y"] + f["dy"]) for f in fp]

    def blocks(deco):
        if "blocksPath" in deco:
            return bool(deco["blocksPath"])
        return bool((info.get(deco["id"]) or {}).get("blocksByDefault"))

    taken = set()
    blocked = set()
    for deco in d["decorations"]:
        for cell in footprint(deco):
            taken.add(cell)
            if blocks(deco):
                blocked.add(cell)
    party = [(u["x"], u["y"]) for u in d["playerSpawns"]]
    taken.update(party)
    for cell in list(party):
        for n in neighbors(*cell, C, R):
            taken.add(n)

    # the groups along the way: anchor, how many
    groups = [((8, 108), 4), ((8, 88), 4), ((10, 72), 5), ((45, 62), 6), ((76, 58), 5), ((9, 18), 4), ((80, 8), 4)]
    foes = d["enemySpawns"]
    assert sum(n for _, n in groups) == len(foes), (sum(n for _, n in groups), len(foes))
    # the wanderers (zombies) and wolves go to the woods' groups, the brigands / archers / cultists / militia to the road's: just
    # keep 006's order and deal them out
    order = sorted(range(len(foes)), key=lambda i: (foes[i]["classId"], i))
    k = 0
    for anchor, n in groups:
        for _ in range(n):
            u = foes[order[k]]
            k += 1
            placed = None
            for rad in range(0, 7):
                options = [(c, r) for r in range(max(0, anchor[1] - rad), min(R, anchor[1] + rad + 1)) for c in range(max(0, anchor[0] - rad), min(C, anchor[0] + rad + 1))
                           if hexdist((c, r), anchor) == rad and (c, r) not in taken and tile[(c, r)] in ("plains", "ruins", "woods")]
                rng.shuffle(options)
                if options:
                    placed = options[0]
                    break
            if placed is None:
                raise SystemExit(f"no room near {anchor}")
            u["x"], u["y"] = placed
            taken.add(placed)

    d["emberTiles"] = "ashen-ground-v01"  # the very image that lies around the board, same scale (the user: "é branco igual o ground, exatamente igual"); ruins keep their stone art
    d.pop("groundImage", None)  # the white ground image with its painted road is gone; the fit's ash photo lies around the board

    # checks
    walk = {k for k, t in tile.items() if t != "void" and k not in blocked}
    seen, todo = {party[0]}, [party[0]]
    while todo:
        cur = todo.pop()
        for nb in neighbors(*cur, C, R):
            if nb in walk and nb not in seen:
                seen.add(nb)
                todo.append(nb)
    for u in foes:
        cell = (u["x"], u["y"])
        if cell in blocked:
            raise SystemExit(f"{u['name']} on a blocking prop at {cell}")
        if cell not in seen and not any(n in seen for n in neighbors(*cell, C, R)):
            raise SystemExit(f"{u['name']} at {cell} cannot be reached")
    exit_deco = next(x for x in d["decorations"] if x["id"] == "dungeon-exit")
    e = (exit_deco["x"], exit_deco["y"])
    if e not in seen and not any(n in seen for n in neighbors(*e, C, R)):
        raise SystemExit("the exit cannot be reached")

    doc = {"serial": int(out[-8:-5]), "savedAt": int(time.time() * 1000), "draft": d}
    open(out, "w", encoding="utf-8", newline="\n").write(json.dumps(doc, ensure_ascii=False, indent=2))
    print(f"{out}: 006 kept ({C}x{R}, {len(d['decorations'])} props), {len(foes)} foes in {len(groups)} groups, {len(seen)} of {len(walk)} walkable hexes reached")
    for anchor, n in groups:
        print(" ", anchor, [f"{u['name']} {(u['x'], u['y'])}" for u in foes if hexdist((u["x"], u["y"]), anchor) <= 6])


if __name__ == "__main__":
    main()
