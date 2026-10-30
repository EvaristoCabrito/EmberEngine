"""Cemitério dos Esquecidos, the modernized map (the user, 2026-10-20: "versão modernizada do mapa do cemitério ... novos
tiles, recursos, elevações variadas, monstros, e deixe o birolho bem mais escondido"; "we need a waypoint out of the
cemetery"; "pode deixar o seu mapa ativo na campanha").

  python -I tools/scripts/maps/make_cemetery_v2.py [settings.json] [out.json]

Writes content/maps/ember/cemiterio-esquecidos006.json (serial 6: the campaign plays it). The mission's own settings (lighting,
mist, briefing, music, location, the party) are read from `settings.json` (default: that same file). The layout follows the map's own
painting (CemiteryBackground.jpg) and its briefing:

  south   the gate in the gothic wall (the party's entry) with the way out (dungeon-exit: the mission ends there)
  centre  the hill over the crypt, terraced graves up its sides, the crypt's stone door in its west flank
  north   the collapsed chapel on its plateau (down to the Ruínas Submersas)
  east    the old families' mausoleums on a raised terrace (down to the Mausoléu), and the stone avenue through an arch
          to Cemetery Ground Part II
  west    the dead wood, and in it the Birolho's hollow: a pit two levels under the ground, ringed by a ridge three levels
          over it, reached only by one narrow cleft from the north-west, out of sight from everywhere else
  ground  the user's six photo tiles (GroundAssets.h set "cemiterio-v01", by terrain): plains = snow over grass, snow =
          trodden paths, nave = setts, ruins = rubble, woods = leaf litter, hill = stony slope

Checks before writing: every exit, connector and spawn is reachable from the party's spawn over passable, unblocked
hexes (the Birolho's hollow through its cleft only), and no two placements share a hex.
"""

import json
import math
import random
import sys
from collections import deque

SRC = sys.argv[1] if len(sys.argv) > 1 else 'content/maps/ember/cemiterio-esquecidos007.json'
OUT = sys.argv[2] if len(sys.argv) > 2 else 'content/maps/ember/cemiterio-esquecidos008.json'
SERIAL = 8
# the tombs' photographed versions (as map 007 carries them)
TOMB_VERSIONS = {'ember-crypt-sarcophagus-broken-lid': 'ember-crypt-sarcophagus-broken-lid-v02', 'ember-crypt-stone-tomb': 'ember-crypt-stone-tomb-v02',
                 'ember-crypt-sarcophagus': 'ember-crypt-sarcophagus-v02', 'ember-crypt-ancient-tombstone': 'ember-crypt-ancient-tombstone-v02',
                 'ember-extra-grave-marker': 'ember-extra-grave-marker-v03'}
CATALOG = 'content/data/editor-catalog.json'

COLS, ROWS = 60, 48
SQ3 = math.sqrt(3.0)
rng = random.Random(20261020)

catalog = json.load(open(CATALOG, encoding='utf-8'))['decorations']


def wx(c, r):
    return SQ3 * (c + 0.5 * (r & 1))


def wz(r):
    return 1.5 * r


def cells():
    for r in range(ROWS):
        for c in range(COLS):
            yield c, r


def neighbors(c, r):
    odd = [(1, 0), (-1, 0), (1, -1), (0, -1), (1, 1), (0, 1)]
    even = [(1, 0), (-1, 0), (0, -1), (-1, -1), (0, 1), (-1, 1)]
    for dc, dr in (odd if r & 1 else even):
        n = (c + dc, r + dr)
        if 0 <= n[0] < COLS and 0 <= n[1] < ROWS:
            yield n


def dist(c, r, x, z):
    return math.hypot(wx(c, r) - x, wz(r) - z)


def seg_dist(px, pz, ax, az, bx, bz):
    vx, vz = bx - ax, bz - az
    t = max(0.0, min(1.0, ((px - ax) * vx + (pz - az) * vz) / max(vx * vx + vz * vz, 1e-9)))
    return math.hypot(px - ax - t * vx, pz - az - t * vz)


def polyline_cells(points, width):
    """Hexes within `width` world units of the polyline through hex centres `points`."""
    pts = [(wx(c, r), wz(r)) for c, r in points]
    out = set()
    for c, r in cells():
        x, z = wx(c, r), wz(r)
        if any(seg_dist(x, z, *pts[i], *pts[i + 1]) <= width for i in range(len(pts) - 1)):
            out.add((c, r))
    return out


# Smooth periodic-free value noise for gentle relief.
def value_noise(seed, scale):
    g = random.Random(seed)
    grid = {}

    def at(i, j):
        if (i, j) not in grid:
            grid[(i, j)] = g.random() * 2 - 1
        return grid[(i, j)]

    def f(x, z):
        x, z = x / scale, z / scale
        i, j = math.floor(x), math.floor(z)
        fx, fz = x - i, z - j
        sx, sz = fx * fx * (3 - 2 * fx), fz * fz * (3 - 2 * fz)
        a = at(i, j) * (1 - sx) + at(i + 1, j) * sx
        b = at(i, j + 1) * (1 - sx) + at(i + 1, j + 1) * sx
        return a * (1 - sz) + b * sz
    return f


noise = value_noise(7, 9.0)
noise2 = value_noise(11, 4.0)

tiles = {p: 'plains' for p in cells()}
elev = {p: 0.0 for p in cells()}

# --- landmarks (hex coordinates) -------------------------------------------------------------------------------------
GATE = (30, 47)
PLAZA = (31, 36)
HILL = (26, 19)          # the barrow over the crypt (its crown)
CRYPT_DOOR = (26, 24)    # the crypt's door, at the end of the walled cut into the barrow's south face
CRYPT_MOUTH = (26, 31)   # where the cut opens onto the field
CHAPEL = (30, 6)         # the chapel's nave
MAUSO = (47, 11)         # the mausoleum terrace's door
ARCH = (58, 25)          # the avenue's arch, to Part II
LAIR = (8, 13)           # the Birolho's hollow (centre)
POND = (50, 40)

# --- relief: whole levels (Board: one level = one world unit, a hex is 1.73 across) --------------------------------------
BARROW_R = 16.0   # world units
for (c, r) in cells():
    x, z = wx(c, r), wz(r)
    e = 0.0
    # the fields: low swells of one level
    if noise(x, z) > 0.45:
        e = 1.0
    # the rim: the painting's wooded hills close the cemetery on the north, west and east, up to level 7 (rock above 7)
    rim = min(r, c, COLS - 1 - c)
    k = 1.4 if r < 36 else 0.7
    if rim <= 5:
        e = max(e, min(7.0, max(0.0, round((5 - rim) * k + noise2(x, z)))))
    # the barrow over the crypt: steep sides, a flat crown at level 5
    d = dist(c, r, wx(*HILL), wz(HILL[1]))
    if d < BARROW_R:
        e = max(e, min(5.0, round(6.5 * (1 - (d / BARROW_R) ** 2))))
    # the chapel plateau at level 3, stepping down to the field
    if 20 <= c <= 40 and r <= 13:
        e = max(e, 3.0 if r <= 9 else 2.0 if r <= 11 else 1.0)
    # the mausoleum terrace at level 2
    if 41 <= c <= 54 and 5 <= r <= 17:
        e = max(e, 2.0 if 42 <= c <= 53 and 6 <= r <= 16 else 1.0)
    # open graves sunk into the west field
    if 7 <= c <= 15 and 33 <= r <= 41 and r % 4 == 1 and c % 3 != 0:
        e = -1.0
    # the strip along the south wall stays level, so the wall runs at one height edge to edge
    if r >= 44:
        e = 0.0
    elev[(c, r)] = e

# the cut into the barrow: a walled passage at ground level from the field to the crypt's door
CUT_HALF = 1  # the passage: the middle column and one either side
cut_cells = {(CRYPT_MOUTH[0] + dc, rr) for dc in range(-CUT_HALF, CUT_HALF + 1) for rr in range(CRYPT_DOOR[1], CRYPT_MOUTH[1] + 1)}
cut_walls = ({(CRYPT_MOUTH[0] + dc, rr) for dc in (-CUT_HALF - 1, CUT_HALF + 1) for rr in range(CRYPT_DOOR[1], CRYPT_MOUTH[1])}
             | {(CRYPT_DOOR[0] + dc, CRYPT_DOOR[1] - 1) for dc in range(-CUT_HALF - 1, CUT_HALF + 2)})
for p in cut_cells | cut_walls:
    elev[p] = 0.0
for p in cut_cells:
    tiles[p] = 'nave'

# --- the Birolho's hollow: pit at -1, ring ridge at +2 / +2.5 (three levels over the pit), one cleft from the north-west
lx, lz = wx(*LAIR), wz(LAIR[1])
lair_floor = set()
for (c, r) in cells():
    d = math.hypot(wx(c, r) - lx, wz(r) - lz)
    if d <= 5.2:
        elev[(c, r)] = -1.0
        tiles[(c, r)] = 'ruins'
        lair_floor.add((c, r))
    elif d <= 9.0:
        elev[(c, r)] = max(elev[(c, r)], 5.0 if d <= 7.6 else 4.0)
        tiles[(c, r)] = 'hill'
# the cleft: a narrow way from the north-west woods down into the pit
cleft = polyline_cells([(1, 3), (2, 6), (3, 8), (5, 10), (7, 12)], 0.95)
for p in cleft:
    tiles[p] = 'snow'
    elev[p] = min(elev[p], 0.5) if p not in lair_floor else -1.0
for i, p in enumerate(sorted(cleft, key=lambda q: q[1])):
    if p not in lair_floor:
        elev[p] = max(-1.0, 2.0 - 0.5 * i)  # the cleft goes down step by step into the pit

# --- water: a black pond in the south-east
for (c, r) in cells():
    d = dist(c, r, wx(*POND), wz(POND[1]))
    if d < 4.2:
        tiles[(c, r)] = 'water'
        elev[(c, r)] = -1.0
    elif d < 5.6:
        elev[(c, r)] = min(elev[(c, r)], 0.0)

# --- woods: the dead wood on the west and the rim ---------------------------------------------------------------------
for (c, r) in cells():
    if tiles[(c, r)] != 'plains':
        continue
    x, z = wx(c, r), wz(r)
    edge = min(c, COLS - 1 - c, r)
    west = c <= 13 and r <= 30
    if edge <= 2 or (west and noise2(x, z) > -0.35) or (c >= 55 and r >= 30 and noise2(x, z) > 0.0):
        tiles[(c, r)] = 'woods'
def slope(p):
    return any(abs(elev[n] - elev[p]) >= 1.0 for n in neighbors(*p))


for p in cells():
    if tiles[p] in ('plains', 'woods') and elev[p] >= 1.0 and slope(p):
        tiles[p] = 'hill'

# --- paths (snow = trodden), avenue + floors (nave = setts), ruins -----------------------------------------------------
paths = set()
paths |= polyline_cells([GATE, (30, 43), PLAZA], 1.3)                       # gate -> plaza
paths |= polyline_cells([PLAZA, (28, 33), (CRYPT_MOUTH[0], CRYPT_MOUTH[1] + 1)], 1.3)  # plaza -> the barrow's cut
paths |= polyline_cells([PLAZA, (38, 32), (39, 20), (34, 14), CHAPEL], 0.95)  # plaza -> chapel (east of the barrow)
paths |= polyline_cells([(34, 12), (42, 12), MAUSO], 0.95)                  # chapel -> mausoleums
paths |= polyline_cells([(24, 33), (15, 30), (13, 20), (18, 12), (23, 10)], 0.9)  # the west loop, round the barrow to the chapel
paths |= polyline_cells([(30, 43), (20, 42), (12, 40)], 0.9)                # gate -> west grave field
paths |= polyline_cells([(30, 43), (42, 42), (46, 37)], 0.9)                # gate -> the pond
paths |= polyline_cells([(13, 20), (8, 24), (2, 20), (1, 12), (1, 6)], 0.75)  # a faint deer track through the dead wood to the cleft
avenue = polyline_cells([PLAZA, (40, 32), (48, 28), (56, 25), ARCH], 1.25)
plaza = {p for p in cells() if dist(*p, wx(*PLAZA), wz(PLAZA[1])) <= 3.2}
for p in paths:
    if tiles[p] in ('plains', 'woods', 'hill') and p not in lair_floor:
        tiles[p] = 'snow'
for p in avenue | plaza:
    if tiles[p] != 'water':
        tiles[p] = 'nave'
# the chapel: rubble floor, a nave aisle of setts, standing columns; the mausoleum terrace floor
for (c, r) in cells():
    if 22 <= c <= 38 and 2 <= r <= 10:
        tiles[(c, r)] = 'nave' if 28 <= c <= 32 else 'ruins'
for c, r in [(25, 3), (25, 6), (25, 9), (35, 3), (35, 6), (35, 9)]:
    tiles[(c, r)] = 'column'
for (c, r) in cells():
    if 42 <= c <= 53 and 7 <= r <= 15 and tiles[(c, r)] not in ('water',):
        tiles[(c, r)] = 'nave' if r in (11, 12) or c in (47, 48) else 'ruins'
# keep the avenue level where it crosses the terrace edge
for p in avenue:
    elev[p] = 0.0

# --- placements --------------------------------------------------------------------------------------------------------
decos = []
occupied = {}
reserved = set()


def footprint(did, x, y, rot=0):
    d = catalog[did]
    fp = d.get('footprint') or [{'dx': 0, 'dy': 0}]
    if rot and not d.get('architectureSpan') and len(fp) > 1:
        rot = 0  # multi-hex props stay unrotated here (their footprint is what the battle blocks)
    return [(x + f['dx'], y + f['dy']) for f in fp]


def free(cs, allow_path=False):
    for p in cs:
        if not (0 <= p[0] < COLS and 0 <= p[1] < ROWS):
            return False
        if p in occupied or p in reserved:
            return False
        if tiles[p] in ('water', 'column'):
            return False
        if not allow_path and (p in paths or p in avenue or p in plaza):
            return False
    return True


def place(did, x, y, block=True, rot=0, allow_path=False, **extra):
    assert did in catalog, did
    cs = footprint(did, x, y, rot)
    if not free(cs, allow_path):
        return False
    p = {'id': did, 'x': x, 'y': y}
    if rot:
        p['rot'] = rot
    if block:
        p['blocksPath'] = True
    p.update(extra)
    decos.append(p)
    for q in cs:
        occupied[q] = did
    return True


def ring(center, radius):
    cx, cz = wx(*center), wz(center[1])
    return [p for p in cells() if abs(dist(*p, cx, cz) - radius) < 0.9]


# party spawns (by the gate) and their surroundings stay free
party = [(29, 44), (31, 44), (30, 43), (30, 45), (30, 44), (29, 43)]
for p in party:
    reserved.add(p)
    for n in neighbors(*p):
        reserved.add(n)

# exits and connectors (placed first, never blocked)
place('dungeon-exit', 29, 47, block=False, allow_path=True)           # the way out: the mission ends here
place('floor-connector', *CRYPT_DOOR, block=False, allow_path=True, targetMapId='cemiterio-esquecidos-cripta')
place('floor-connector', CHAPEL[0], CHAPEL[1] - 2, block=False, allow_path=True, targetMapId='cemiterio-esquecidos-ruinas')
place('floor-connector', MAUSO[0], MAUSO[1] + 1, block=False, allow_path=True, targetMapId='cemiterio-esquecidos-mausoleu')
place('wilds-temple-door', *MAUSO, allow_path=True)
place('floor-connector', ARCH[0], ARCH[1], block=False, allow_path=True, targetMapId='cemetery-ground-part-ii')
place('door-3d-frame', ARCH[0] - 1, ARCH[1] - 1, block=False, allow_path=True)
connectors = [(d['x'], d['y']) for d in decos if d['id'] in ('floor-connector', 'dungeon-exit')]

# the hollow's rim: movement does not climb by height, so the ridge's inner ring is closed by solid
# rocks and dead trees; the cleft is the only way down
RIM = ['ember-winter-snow-boulder', 'ember-frost-ice-cracked-rocks', 'ember-winter-frost-dead-tree',
       'ember-forest-dead-tree', 'wilds-thorn-bramble', 'dead-tree', 'wilds-twisted-tree']
rim_cells = sorted((p for p in cells() if 5.2 < math.hypot(wx(*p) - lx, wz(p[1]) - lz) <= 7.0 and p not in cleft),
                   key=lambda p: math.atan2(wz(p[1]) - lz, wx(*p) - lx))
for p in rim_cells:
    if p in occupied:
        continue
    did = rng.choice(RIM)
    if not place(did, *p, rot=rng.randrange(6)):
        place(rng.choice(['wilds-thorn-bramble', 'dead-tree', 'ember-winter-frost-dead-tree']), *p, rot=rng.randrange(6))

# the gothic wall across the whole south side, edge to edge on one row (row 46), the gate (28..31) its only opening
for x in list(range(0, 28, 4)) + list(range(32, 60, 4)):
    place('bridge-parapet-gothic-wall-001' if (x // 4) % 2 else 'bridge-parapet-gothic-statues-001', x, 46, allow_path=True)
place('wilds-cemetery-lantern', 27, 44, allow_path=True)
place('wilds-cemetery-lantern', 32, 44, allow_path=True)

# the cut into the barrow: crypt walls (3D, joined) on both sides and across its end, the door in the end wall
for p in sorted(cut_walls):
    if p not in ((CRYPT_DOOR[0], CRYPT_DOOR[1] - 1), (CRYPT_DOOR[0] + 1, CRYPT_DOOR[1] - 1)):
        place('wall-3d-crypt', *p, allow_path=True)
place('dungeon-stone-door', CRYPT_DOOR[0], CRYPT_DOOR[1] - 1, allow_path=True)
for dc in (-2, 2):
    place('wilds-knight-statue', CRYPT_MOUTH[0] + dc * 3 // 2, CRYPT_MOUTH[1], allow_path=True)
    place('Brazier3', CRYPT_MOUTH[0] + dc * 3 // 2, CRYPT_MOUTH[1] + 1, allow_path=True)
place('dungeon-ossuary', CRYPT_MOUTH[0] - 5, CRYPT_MOUTH[1] + 1)
place('dungeon-ossuary', CRYPT_MOUTH[0] + 4, CRYPT_MOUTH[1] + 1)
# the barrow's crown: the fallen king, the old family's stones
place('wilds-fallen-king', HILL[0] - 1, HILL[1])
place('ember-crypt-sarcophagus-broken-lid', HILL[0] + 3, HILL[1] + 1)
place('wilds-gibbet-tree', HILL[0] - 4, HILL[1] - 2)
place('wilds-ivy-statue', HILL[0] + 3, HILL[1] - 3)

# the chapel: broken arches, the altar, rubble, braziers at its mouth
place('wilds-gothic-arch', 30, 10, allow_path=True, block=False)
for c, r in [(27, 4), (33, 4), (27, 8), (33, 8)]:
    place('ember-crypt-cracked-pillar', c, r)
place('ember-dungeon-broken-arch', 29, 3)
place('ember-crypt-deity-statue', 30, 2)
for c, r in [(23, 3), (37, 5), (24, 8), (36, 9), (26, 10), (34, 2)]:
    place(rng.choice(['ember-crypt-collapsed-ceiling', 'ember-structures-scattered-blocks', 'ember-crypt-column-base']), c, r)
place('Brazier3', 28, 11, allow_path=True)
place('Brazier3', 32, 11, allow_path=True)

# the mausoleum terrace: the families' tombs in rows, statues, braziers by the door
for c, r in [(43, 8), (43, 14), (51, 8), (51, 14)]:
    place('stone-hut', c, r)
for c, r in [(45, 10), (49, 10)]:
    place('wilds-knight-statue', c, r, allow_path=True)
place('Brazier3', 46, 13, allow_path=True)
place('Brazier3', 49, 13, allow_path=True)
for c, r in [(44, 11), (52, 11), (45, 15), (50, 16), (44, 6), (52, 6)]:
    place(rng.choice(['ember-crypt-sarcophagus', 'ember-crypt-sarcophagus-broken-lid', 'ember-crypt-stone-tomb']), c, r)
for c, r in [(42, 16), (53, 16), (42, 7), (53, 9)]:
    place('ember-crypt-funeral-urns', c, r)

# the avenue: lanterns and statues in pairs, the arch at its end
av = sorted(avenue, key=lambda p: wx(*p))
for i, p in enumerate(av[4::7]):
    for n in neighbors(*p):
        if n not in avenue and n not in paths:
            place('wilds-lantern-post' if i % 2 == 0 else 'ember-crypt-ancient-tombstone', *n)
            break
place('wilds-gothic-arch', ARCH[0] - 2, ARCH[1] + 1, allow_path=True)

# lanterns along the paths
for i, p in enumerate(sorted(paths, key=lambda q: (q[1], q[0]))[::23]):
    for n in neighbors(*p):
        if n not in paths and tiles[n] in ('plains', 'snow', 'hill') and free([n]):
            place('lamppost' if i % 2 else 'wilds-cemetery-lantern', *n)
            break

# the grave fields (3D stones only): family plots, rows straight along the hex rows, one kind of stone per plot and a monument at its head,
# graves only on level ground
PLOTS = [  # c0, c1, r0, r1, stone, monument
    (7, 15, 32, 41, 'ember-crypt-ancient-tombstone', 'wilds-ruined-wayside-shrine'),
    (18, 25, 37, 42, 'ember-extra-grave-marker', 'wilds-knight-statue'),
    (35, 42, 37, 42, 'ember-extra-grave-marker', 'wilds-candle-menhir'),
    (44, 53, 30, 35, 'ember-crypt-ancient-tombstone', 'wilds-ivy-statue'),
    (20, 32, 15, 23, 'ember-crypt-ancient-tombstone', None),   # on the barrow's crown
    (14, 19, 13, 19, 'ember-extra-grave-marker', 'dungeon-ossuary'),
]
for (c0, c1, r0, r1, stone, monument) in PLOTS:
    if monument:
        place(monument, (c0 + c1) // 2, r0)
    for r in range(r0 + 1, r1 + 1, 2):
        for c in range(c0, c1 + 1, 2):
            p = (c, r)
            if tiles[p] not in ('plains', 'hill') or slope(p) or elev[p] < 0:
                continue
            if rng.random() < 0.12:
                place(rng.choice(['ember-crypt-stone-tomb', 'ember-crypt-sarcophagus']), c, r)
            else:
                place(stone, c, r)

# the dead wood: trees (3D and cards), brambles, fallen logs; denser on the lair's ridge so its rim cannot be seen over
TREES_3D = ['ember-winter-frost-dead-tree', 'ember-forest-dead-tree']
TREES_CARD = ['dead-tree', 'wilds-dead-oak', 'wilds-twisted-tree', 'wilds-gibbet-tree']
UNDER = ['wilds-thorn-bramble', 'wilds-dry-bush', 'ember-frost-snowy-dry-bush', 'ember-frost-frozen-stump']
for (c, r) in cells():
    t = tiles[(c, r)]
    if t not in ('woods', 'hill') or (c, r) in cleft:
        continue
    near_lair = math.hypot(wx(c, r) - lx, wz(r) - lz) <= 9.5
    chance = 0.42 if near_lair else (0.2 if t == 'woods' else 0.07)
    if rng.random() > chance:
        continue
    roll = rng.random()
    if roll < 0.3:
        place(rng.choice(TREES_3D), c, r, rot=rng.randrange(6))
    elif roll < 0.7:
        place(rng.choice(TREES_CARD), c, r)
    else:
        place(rng.choice(UNDER), c, r)
for c, r in [(18, 38), (44, 26), (12, 22), (52, 44), (40, 20)]:
    place(rng.choice(['ember-winter-snow-fallen-log', 'ember-nature-rotten-fallen-log', 'ember-winter-snow-boulder']), c, r)

# snow drifts and frozen things by the pond, rocks on the hills
for c, r in [(46, 42), (54, 38), (47, 36)]:
    place(rng.choice(['ember-frost-snow-drift', 'ember-frost-ice-cracked-rocks']), c, r)
for (c, r) in cells():
    if tiles[(c, r)] == 'hill' and rng.random() < 0.05:
        place(rng.choice(['ember-nature-lichen-rocks', 'ember-forest-rock-cluster', 'ember-winter-snow-boulder']), c, r)

# --- the Birolho's hollow -------------------------------------------------------------------------------------------
place('ember-dungeon-iron-cage', 10, 11)
place('wilds-gibbet-tree', 5, 15)
place('ember-crypt-ritual-brazier', 11, 15)
for c, r in [(7, 11), (6, 14), (10, 16), (9, 10)]:
    place(rng.choice(['ember-dungeon-bone-pile', 'ember-crypt-lone-skull', 'ember-crypt-incomplete-skeleton']), c, r, block=False)
place('locked-chest', 11, 13)
BIROLHO = (8, 13)   # two hexes: (8, 13) and (9, 13)

# --- units ------------------------------------------------------------------------------------------------------------
src = json.load(open(SRC, encoding='utf-8'))
draft = src['draft']
player = []
for (x, y), s in zip(party, draft['playerSpawns']):  # the mission's own heroes, by the gate (the game adds the rest)
    s = dict(s)
    s['x'], s['y'] = x, y
    player.append(s)
print('heroes', [s['name'] for s in player])

E = []


def enemy(name, cls, x, y, level, **extra):
    s = {'name': name, 'classId': cls, 'x': x, 'y': y, 'level': level}
    s.update(extra)
    E.append(s)


# the west grave field: the dead walking between the rows
enemy('Zumbi', 'zombie', 10, 37, 5)
enemy('Zumbi', 'zombie', 13, 35, 5)
enemy('Zumbi', 'zombie2', 8, 40, 5)
# the crypt's door
enemy('Saqueador de Covas', 'miliciaV2', 26, 27, 5)
# the chapel
enemy('Acólito das Cinzas', 'cultist', 27, 6, 5)
enemy('Acólito das Cinzas', 'cultist', 33, 6, 5)
enemy('Coveiro', 'punisher', 30, 8, 6)
enemy('Necromante da Capela', 'necromancer', 29, 5, 7)
# the mausoleums
enemy('Ladrão de Túmulos', 'brigand', 45, 12, 5)
enemy('Ladrão de Túmulos', 'brigand', 50, 12, 5)
enemy('Ladrão de Túmulos', 'assassin', 48, 9, 5)
# the avenue, by the arch
enemy('Zumbi', 'zombie2', 53, 26, 6)
enemy('Zumbi', 'zombie2', 55, 24, 6)
# the pond
enemy('Zumbi', 'zombie', 46, 39, 5)
enemy('Zumbi', 'zombie', 54, 41, 5)
# the hill's crown, among the high graves
enemy('Zumbi', 'zombie2', 23, 21, 5)
# the dead wood near the deer track (they guard the way to the cleft without showing it)
enemy('Zumbi', 'zombie2', 4, 22, 6)
# loose in the corners: the apparition among the north-east hills, the zombie ox in the south-west field
enemy('Aparição', 'apparition', 52, 4, 7)
enemy('Boi Zumbi', 'undeadOx', 3, 42, 7)
# the hollow
enemy('O Birolho', 'birolhoLegs2', BIROLHO[0], BIROLHO[1], 10, guaranteedDrop=True)

neutral = [
    {'name': 'Mordavian Wolf Final', 'classId': 'mordavianWolfFinal', 'x': 56, 'y': 6, 'level': 4},
    {'name': 'Mordavian Wolf Final', 'classId': 'mordavianWolfFinal', 'x': 57, 'y': 9, 'level': 4},
    {'name': 'Bezerro do Pântano', 'classId': 'swampBlueCalf', 'x': 52, 'y': 36, 'level': 4},
    {'name': 'Bezerro do Pântano', 'classId': 'swampBlueCalf', 'x': 56, 'y': 40, 'level': 4},
]


def unit_cells(s):
    two = s['classId'] in ('birolhoLegs2', 'undeadOx', 'mordavianWolfFinal', 'swampBlueCalf')
    return [(s['x'], s['y'])] + ([(s['x'] + 1, s['y'])] if two else [])


# clear any placement standing where a unit stands (units win), then check the board
for s in player + E + neutral:
    for p in unit_cells(s):
        if p in occupied:
            did = occupied[p]
            for d in [d for d in decos if d['id'] == did and p in footprint(d['id'], d['x'], d['y'], d.get('rot', 0))]:
                decos.remove(d)
                for q in footprint(d['id'], d['x'], d['y'], d.get('rot', 0)):
                    occupied.pop(q, None)
        assert tiles[p] not in ('water', 'column'), (s['name'], p, tiles[p])

# any rim hex still open (a unit cleared it, or nothing fitted) takes a bramble
for p in rim_cells:
    if p not in occupied and p not in {q for s in E + neutral for q in unit_cells(s)}:
        decos.append({'id': 'wilds-thorn-bramble', 'x': p[0], 'y': p[1], 'blocksPath': True})
        occupied[p] = 'wilds-thorn-bramble'

PASSABLE = {'plains', 'woods', 'ruins', 'hill', 'nave', 'snow'}
blocked = set()
for d in decos:
    if d.get('blocksPath'):
        blocked |= set(footprint(d['id'], d['x'], d['y'], d.get('rot', 0)))
units_at = {p for s in E + neutral for p in unit_cells(s)}
kept = {(d['x'], d['y']) for d in decos if d['id'] in ('floor-connector', 'dungeon-exit')}
for p in connectors:
    if p not in kept:
        print(f'PROBLEM: exit {p} lost under a unit')
        sys.exit(1)
for s in player + E + neutral:
    for q in unit_cells(s):
        if q in kept:
            print(f"PROBLEM: {s['name']} stands on the exit {q}")
            sys.exit(1)


def reach(start):
    seen = {start}
    q = deque([start])
    while q:
        p = q.popleft()
        for n in neighbors(*p):
            if n in seen or tiles[n] not in PASSABLE or n in blocked:
                continue
            seen.add(n)
            q.append(n)
    return seen


seen = reach(party[0])
problems = []
# the hollow through its cleft only: with the cleft closed, its floor is out of reach
saved = dict(tiles)
for p in cleft - lair_floor:
    tiles[p] = 'water'
if any(p in reach(party[0]) for p in lair_floor):
    problems.append('the hollow can be entered outside its cleft')
tiles.update(saved)
for name, p in (('Birolho', BIROLHO), ('chest', (11, 13)), ('cage', (10, 11))):
    if p not in lair_floor:
        problems.append(f'{name} {p} outside the hollow floor')
if (BIROLHO[0] + 1, BIROLHO[1]) not in lair_floor:
    problems.append('Birolho second hex outside the hollow')
rim_open = [p for p in rim_cells if p not in blocked]
south = [x for x in range(COLS) if (x, 46) not in blocked]
if south != [28, 29, 30, 31]:
    problems.append(f'the south wall is open at {south} (only the gate 28..31 may be)')
for p in cut_walls:
    if p not in blocked:
        problems.append(f'the crypt cut is open at {p}')
for p in cut_cells:
    if p in blocked:
        problems.append(f'the crypt cut is blocked at {p}')
if rim_open:
    problems.append(f'rim open at {rim_open}')
for p in connectors:
    if p not in seen:
        problems.append(f'exit {p} unreachable')
for s in E + neutral:
    if not any(n in seen for q in unit_cells(s) for n in [q, *neighbors(*q)]):
        problems.append(f"{s['name']} at {s['x']},{s['y']} unreachable")
walkable = sum(1 for p in cells() if tiles[p] in PASSABLE and p not in blocked)
# the hollow: its floor must be entered only through the cleft
ridge_gate = [p for p in lair_floor if p in seen]
inner_neighbours = {n for p in lair_floor for n in neighbors(*p) if n not in lair_floor and tiles[n] in PASSABLE and n not in blocked and abs(elev[n] - elev[p]) <= 1.0}
print(f'walkable {walkable}, reachable {len(seen)} ({len(seen) * 100 // max(walkable, 1)}%), decorations {len(decos)}, enemies {len(E)}')
print('hollow entries (neighbours of the pit floor not on the ridge):', sorted(inner_neighbours))
if problems:
    print('PROBLEMS:\n  ' + '\n  '.join(problems))
    sys.exit(1)

# --- write --------------------------------------------------------------------------------------------------------------
order = [(c, r) for r in range(ROWS) for c in range(COLS)]
draft = dict(draft)
draft.update({
    'cols': COLS, 'rows': ROWS,
    'tiles': [tiles[p] for p in order],
    'tileVariants': [None] * (COLS * ROWS),
    'tileRots': [0] * (COLS * ROWS),
    'terrainElevations': [elev[p] for p in order],
    'decorations': decos,
    'playerSpawns': player,
    'enemySpawns': E,
    'neutralSpawns': neutral,
    'emberTiles': 'cemiterio-v01',
    'objective': 'Explore o cemitério, desça às criptas, às ruínas e aos mausoléus, atravesse a alameda leste para Cemetery Ground Part II — ou volte pelo portão sul',
})
for d in draft['decorations']:
    d['id'] = TOMB_VERSIONS.get(d['id'], d['id'])
out = {'serial': SERIAL, 'savedAt': src.get('savedAt', 0) + 1, 'draft': draft}
with open(OUT, 'w', encoding='utf-8') as f:
    json.dump(out, f, ensure_ascii=False)
print('wrote', OUT)
