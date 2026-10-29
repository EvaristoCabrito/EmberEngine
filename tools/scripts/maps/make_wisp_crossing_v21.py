"""Wisp Forest Crossing 021: the makeover of 020 (the user, 2026-10-29: "do a make over on the design and decoration of the wisp
forest", the plan approved: "Approve, do it"; "there are lots of new 3D assets i want you to try in this one").

Reads content/maps/ember/wisp-forest-crossing020.json (left untouched) and writes wisp-forest-crossing021.json (the campaign plays the
highest serial). What changes:
  - the board on its floor: holes inside the board filled; the draft's emberFloorOverWater puts the floor image and the side walls
    under the dry ground, so the edge is the thin lip of every other map (the pond's bed used to lift the whole edge 1.35 above it);
  - the layout: an old paved wisp road (pavers, 2 hexes wide) from the start in the south-west to the temple ruins in the north-east,
    with a branch to the southern ruins and a short way down to the escape exits; a stream from a pond in the north-west to a pool in
    the south, crossing both roads at stone fords; real thickets (forest floor) with the big 3D trees standing in them; low mossy
    mounds (hills, level 1) at the edges of the clearings;
  - the decoration by theme: trees only in the thickets, undergrowth (ferns, bushes, roots, fungi, logs) on the thickets' edges and
    the road's sides, mossy rocks on the mounds and the banks, wisp markers (shrine, waystone, lanterns, glowing mushrooms) along the
    road, the old stones (columns, statues, braziers, rune stones) only in the ruins, five chests in hidden corners;
  - kept as they were: every enemy and the party (moved to the nearest walkable hex only if the new layout put them on water), the
    three escape exits, the three connectors to the Carnivorous Plant's lair, the dawn, the mist, the wisps, the mission's texts.
The script refuses (exit 1) when a spawn, exit, connector or chest cannot be reached from the party's start, or two pieces share a hex.

Usage: python -I tools/scripts/maps/make_wisp_crossing_v21.py
"""
import collections
import json
import math
import pathlib
import random
import sys
import time

ROOT = pathlib.Path(__file__).resolve().parents[3]
SRC = ROOT / "content/maps/ember/wisp-forest-crossing020.json"
OUT = ROOT / "content/maps/ember/wisp-forest-crossing021.json"
SERIAL = 21
rng = random.Random(21)

src = json.loads(SRC.read_text(encoding="utf-8"))
draft = src["draft"]
C, R = draft["cols"], draft["rows"]
tiles = list(draft["tiles"])
variants = list(draft["tileVariants"])
rots = list(draft["tileRots"])


def idx(c, r):
    return r * C + c


def inside(c, r):
    return 0 <= c < C and 0 <= r < R


def neighbors(c, r):  # Battle::neighbors (odd rows shifted right)
    d = [(1, 0), (1, -1), (0, -1), (-1, 0), (0, 1), (1, 1)] if r & 1 else [(1, 0), (0, -1), (-1, -1), (-1, 0), (-1, 1), (0, 1)]
    return [(c + dc, r + dr) for dc, dr in d if inside(c + dc, r + dr)]


def cube(c, r):
    q = c - (r - (r & 1)) // 2
    return q, r, -q - r


def offset(q, r):
    return q + (r - (r & 1)) // 2, r


def dist(a, b):
    x, y = cube(*a), cube(*b)
    return max(abs(x[0] - y[0]), abs(x[1] - y[1]), abs(x[2] - y[2]))


def line(a, b):  # the hexes on the straight line a..b (cube lerp)
    n = dist(a, b)
    (aq, ar, _), (bq, br, _) = cube(*a), cube(*b)
    out = []
    for i in range(n + 1):
        t = i / max(n, 1)
        q, r = aq + (bq - aq) * t, ar + (br - ar) * t
        s = -q - r
        rq, rr, rs = round(q), round(r), round(s)
        if abs(rq - q) > abs(rr - r) and abs(rq - q) > abs(rs - s):
            rq = -rr - rs
        elif abs(rr - r) > abs(rs - s):
            rr = -rq - rs
        out.append(offset(rq, rr))
    return out


def path(points):
    out = []
    for a, b in zip(points, points[1:]):
        for h in line(a, b):
            if not out or out[-1] != h:
                out.append(h)
    return out


def world(c, r):  # hex centre in hex units (for spacing)
    return math.sqrt(3) * (c + 0.5 * (r & 1)), 1.5 * r


def tile(c, r):
    return tiles[idx(c, r)]


def set_tile(c, r, t):
    if inside(c, r) and tile(c, r) != "void":
        tiles[idx(c, r)] = t


# --- the board: holes inside it filled (void reached from the grid's border is the outside) -----------------------------------------
outside = set()
todo = [(c, r) for c in range(C) for r in range(R) if (c in (0, C - 1) or r in (0, R - 1)) and tile(c, r) == "void"]
while todo:
    h = todo.pop()
    if h in outside:
        continue
    outside.add(h)
    todo += [n for n in neighbors(*h) if tile(*n) == "void" and n not in outside]
for c in range(C):
    for r in range(R):
        if tile(c, r) == "void" and (c, r) not in outside:
            tiles[idx(c, r)] = "plains"
# the ragged rim: a lone board hex sticking into the void (two board neighbours or fewer) goes
for _ in range(2):
    for c in range(C):
        for r in range(R):
            if tile(c, r) != "void" and sum(tile(*n) != "void" for n in neighbors(c, r)) <= 2:
                tiles[idx(c, r)] = "void"

# --- the ground: grass everywhere but the temple ruins (020's pavers kept), then the new features -----------------------------------
ruins = {(c, r) for c in range(C) for r in range(R) if tile(c, r) in ("nave", "ruins")}
for c in range(C):
    for r in range(R):
        if tile(c, r) not in ("void", "nave", "ruins"):
            tiles[idx(c, r)] = "plains"

start = (draft["playerSpawns"][0]["x"], draft["playerSpawns"][0]["y"])
road = path([(6, 31), (8, 28), (12, 25), (16, 23), (20, 19), (23, 15), (25, 12), (27, 10)])
branch = path([(20, 19), (24, 21), (27, 22), (30, 23)])
south = path([(6, 31), (6, 33)])
roads = set()
for h in road + branch + south:
    roads.add(h)
    # two hexes wide: the neighbour on the road's east side
    c, r = h
    roads.add((c + 1, r))
stream = path([(9, 9), (12, 11), (15, 13), (18, 15), (21, 17), (23, 20), (24, 23), (24, 26)])
pond = {(c, r) for c in range(8, 12) for r in range(8, 11)} | {(c, r) for c in range(23, 27) for r in range(26, 29)}
water = set(stream) | pond
fords = water & roads
for h in water - fords:
    set_tile(*h, "water")
for h in roads:
    if tile(*h) != "void":
        set_tile(*h, "ruins")

# thickets: noisy blobs of forest floor away from the roads and the water
thicket_centres = [(4, 13), (10, 3), (17, 4), (5, 22), (12, 18), (16, 30), (29, 31), (36, 32), (33, 15), (41, 9), (2, 27), (20, 9)]
thicket = set()
for cc in thicket_centres:
    radius = rng.choice([2, 3, 3])
    for c in range(cc[0] - 4, cc[0] + 5):
        for r in range(cc[1] - 4, cc[1] + 5):
            if inside(c, r) and dist(cc, (c, r)) <= radius - (rng.random() < 0.35):
                thicket.add((c, r))
thicket -= roads | water | ruins
for h in thicket:
    set_tile(*h, "woods")

# low mossy mounds at the clearings' edges
mounds = set()
for cc in [(14, 27), (27, 26), (9, 15), (30, 12), (37, 28)]:
    for c in range(cc[0] - 2, cc[0] + 3):
        for r in range(cc[1] - 2, cc[1] + 3):
            if inside(c, r) and dist(cc, (c, r)) <= 1 + (rng.random() < 0.5):
                mounds.add((c, r))
mounds -= roads | water | ruins | thicket
# grass one level up (not the "hill" terrain: it has none of the crossing's ground tiles)
elevations = [None] * (C * R)
for h in mounds:
    if tile(*h) != "void":
        elevations[idx(*h)] = 1

# --- spawns, exits, connectors: kept, on walkable ground ---------------------------------------------------------------------------
walk = lambda h: inside(*h) and tile(*h) not in ("void", "water")
keep_ids = ("escape-exit", "floor-connector")
anchors = [d for d in draft["decorations"] if d["id"] in keep_ids]
for d in anchors:
    if not walk((d["x"], d["y"])):
        set_tile(d["x"], d["y"], "plains")


def nearest_walkable(h, taken):
    best = None
    for c in range(C):
        for r in range(R):
            if walk((c, r)) and (c, r) not in taken and (best is None or dist(h, (c, r)) < dist(h, best)):
                best = (c, r)
    return best


occupied = {(d["x"], d["y"]) for d in anchors}
for s in draft["playerSpawns"] + draft["enemySpawns"]:
    h = (s["x"], s["y"])
    if not walk(h) or h in occupied:
        h = nearest_walkable(h, occupied)
        s["x"], s["y"] = h
    occupied.add(h)

# --- decoration -------------------------------------------------------------------------------------------------------------------
decor = list(anchors)
spots = set(occupied)
placed_xy = []  # (x, y, radius) in hex units: big pieces keep apart


def free(h, radius=0.0):
    if h in spots or not walk(h) and tile(*h) != "water":
        return False
    x, y = world(*h)
    return all(math.hypot(x - px, y - py) >= radius + pr for px, py, pr in placed_xy)


def put(id_, h, radius=0.0, **extra):
    d = {"id": id_, "x": h[0], "y": h[1], "rot": rng.randrange(6)}
    d.update(extra)
    decor.append(d)
    spots.add(h)
    if radius:
        placed_xy.append((*world(*h), radius))


TREES = ["ember-wisp-crossing-ancient-dead-oak-v02", "ember-wisp-crossing-weeping-willow-v02", "ember-wisp-crossing-lightning-split-v02",
         "ember-wisp-crossing-hollow-sentinel-v02", "ember-wisp-revisited-giant-dead-oak-v02", "ember-wisp-revisited-hollow-snag-v02",
         "ember-wisp-revisited-spiral-twisted-v02", "ember-wisp-revisited-tall-burnt-pine-v02", "ember-wisp-revisited-broken-crown-v02",
         "ember-wisp-revisited-uprooted-leaning-v02", "ember-wisp-revisited-root-arch-v02"]
UNDER = ["ember-nature-ferns", "ember-polyhaven-fern-02-a", "ember-polyhaven-fern-02-b", "ember-polyhaven-fern-02-c",
         "ember-nature-dense-bush", "ember-nature-exposed-roots", "ember-nature-roots-with-mushrooms", "ember-nature-wild-mushrooms",
         "ember-wisp-revisited-roots-and-fungi", "ember-nature-leaf-litter", "ember-forest-floor-cluster"]
LOGS = ["ember-nature-rotten-fallen-log", "ember-polyhaven-dead-tree-trunk-02", "ember-wisp-revisited-rotted-hollow-log-v02",
        "ember-nature-rotten-stump", "ember-wisp-revisited-broken-stump", "ember-nature-uprooted-tree", "ember-forest-stump"]
ROCKS = ["ember-polyhaven-rock-moss-set-01-rock01", "ember-polyhaven-rock-moss-set-01-rock03", "ember-polyhaven-rock-moss-set-01-rock05",
         "ember-polyhaven-rock-moss-set-02-rock07", "ember-polyhaven-rock-moss-set-02-rock10", "ember-polyhaven-rock-moss-set-02-rock11",
         "ember-polyhaven-boulder-01", "ember-nature-lichen-rocks", "ember-forest-moss-boulder"]
FLOWERS = ["ember-polyhaven-celandine-01-a", "ember-polyhaven-celandine-01-b", "ember-polyhaven-celandine-01-c",
           "ember-polyhaven-grass-medium-01-large-a", "ember-polyhaven-grass-medium-01-tall-a", "ember-farmlands-tall-grass"]
WISP = ["wilds-wisp-shrine", "wilds-wisp-waystone", "wilds-wisp-lantern-marker", "wilds-lantern-signpost", "wilds-wisp-mushrooms"]
OLD_STONES = ["wilds-broken-column", "wilds-chained-pillar", "wilds-knight-statue", "wilds-ivy-statue", "rune-stone",
              "wilds-brazier-tripod", "wilds-incense-burner", "tombstones", "dungeon-hanging-cage", "wilds-weathered-signpost"]

# trees: in the thickets, a canopy apart
cells = sorted(thicket)
rng.shuffle(cells)
for h in cells:
    if tile(*h) == "woods" and free(h, 2.6):
        put(rng.choice(TREES), h, 2.6)
# undergrowth on the thickets' rims and logs inside them
rim = [h for h in sorted(thicket) if any(tile(*n) != "woods" for n in neighbors(*h))]
rng.shuffle(rim)
for h in rim[: len(rim) // 3]:
    if free(h, 0.6):
        put(rng.choice(UNDER), h, 0.6)
inner = [h for h in sorted(thicket) if h not in rim]
rng.shuffle(inner)
for h in inner[:10]:
    if free(h, 1.2):
        put(rng.choice(LOGS), h, 1.2)
# mossy rocks on the mounds and the banks
banks = sorted({n for h in water for n in neighbors(*h) if tile(*n) not in ("water", "void") and n not in roads})
rng.shuffle(banks)
for h in sorted(mounds) + banks[:14]:
    if rng.random() < 0.5 and free(h, 1.0):
        put(rng.choice(ROCKS), h, 1.0)
# the road's sides: wisp markers every few hexes, roots and fungi, flowers in the clearings
side = sorted({n for h in road + branch for n in neighbors(*h) if n not in roads and walk(n) and tile(*n) != "woods"})
for i, h in enumerate(side[::7]):
    if free(h, 0.8):
        put(WISP[i % len(WISP)], h, 0.8)
rng.shuffle(side)
for h in side[:16]:
    if free(h, 0.6):
        put(rng.choice(UNDER[:4] + ["ember-nature-exposed-roots", "ember-wisp-revisited-roots-and-fungi"]), h, 0.6)
grass = [(c, r) for c in range(C) for r in range(R) if tile(c, r) == "plains"]
rng.shuffle(grass)
for h in grass[:40]:
    if free(h, 0.9):
        put(rng.choice(FLOWERS), h, 0.9)
# a log over the stream away from the fords
for h in stream[2:-2:5]:
    if h not in fords and free(h, 1.5):
        put("ember-nature-log-over-stream", h, 1.5)
        break
# the old stones: in the ruins only
ruin_cells = sorted(h for h in ruins if walk(h))
rng.shuffle(ruin_cells)
for i, h in enumerate(ruin_cells[:22]):
    if free(h, 1.1):
        put(OLD_STONES[i % len(OLD_STONES)], h, 1.1)
# five chests in hidden corners: thicket hexes beside a tree's canopy, far from the road
hidden = [h for h in sorted(thicket) if walk(h) and min(dist(h, x) for x in roads) >= 4]
rng.shuffle(hidden)
tiers = ["base", "base", "better", "better", "best"]
for h in hidden:
    if not tiers:
        break
    if h not in spots:
        put("ember-polyhaven-treasure-chest", h, 0, chestTier=tiers.pop(0))
if tiers:
    sys.exit(f"refused: {len(tiers)} chests found no hidden hex")

# --- ground variants: the crossing's own tiles in patches (020 mixed them the same way) ----------------------------------------
# plains: 65 the crossing ground, 63 exposed roots, 64 ash and leaves, 62 dead leaves; woods (the thickets' floor): roots and leaves.
def patch(c, r, scale, seed):
    x, y = world(c, r)
    return (math.sin(x / scale + seed) + math.sin(y / scale * 1.3 + seed * 2.1) + math.sin((x + y) / scale * 0.7 + seed * 0.7)) / 3


for c in range(C):
    for r in range(R):
        i, t = idx(c, r), tile(c, r)
        if t == "plains":
            n, m = patch(c, r, 3.2, 1.0), patch(c, r, 2.1, 4.0)
            variants[i] = 63 if n > 0.45 else 62 if m > 0.6 else 64 if m < -0.7 else 65
        elif t == "woods":
            variants[i] = 63 if patch(c, r, 2.6, 7.0) > -0.1 else 62
        elif t in ("nave", "ruins"):
            variants[i] = draft["tileVariants"][i] if draft["tiles"][i] == t else 0
        else:
            variants[i] = 0
        rots[i] = rots[i] if t in ("nave", "ruins") else 0

# --- checks: everything reachable from the party's start, nothing on another piece -----------------------------------------------
seen, todo = {start}, [start]
while todo:
    h = todo.pop()
    for n in neighbors(*h):
        if n not in seen and walk(n):
            seen.add(n)
            todo.append(n)
must = [(s["x"], s["y"], s.get("name")) for s in draft["playerSpawns"] + draft["enemySpawns"]]
must += [(d["x"], d["y"], d["id"]) for d in decor if d["id"] in keep_ids or d["id"] == "ember-polyhaven-treasure-chest"]
lost = [m for m in must if (m[0], m[1]) not in seen]
if lost:
    sys.exit(f"refused: not reachable from the start: {lost}")
twice = [h for h, n in collections.Counter((d["x"], d["y"]) for d in decor).items() if n > 1]
if twice:
    sys.exit(f"refused: two pieces on one hex: {twice}")

draft = dict(draft)
draft.update({"tiles": tiles, "tileVariants": variants, "tileRots": rots, "decorations": decor, "emberFloorOverWater": True,
              "terrainElevations": elevations})
out = {"draft": draft, "savedAt": int(time.time() * 1000), "serial": SERIAL}
if OUT.exists() and "--again" not in sys.argv:
    sys.exit(f"refused: {OUT.name} exists (a change is a new version; --again only before the user has seen it)")
OUT.write_text(json.dumps(out, ensure_ascii=False), encoding="utf-8")
count = collections.Counter(tiles)
print(f"{OUT.name}: {dict(count)}; {len(decor)} pieces ({collections.Counter(d['id'].split('-')[0] for d in decor)}); "
      f"{len(seen)} hexes reachable")
