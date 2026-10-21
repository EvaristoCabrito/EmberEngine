"""Watchtower, Andar 0 (Guarnição do Portão), the larger tower (the user, 2026-10-21: "take your time to improve on the size and
design of the tower", torches inside "porque está muito escuro", the stairs and the classic exit at the tower's door; the floor
tile from the user's photo, Attachments/watchtower/Watchtower.jpg = GroundAssets.h set "torre-v01").

  python -I tools/scripts/maps/make_watchtower_v2.py [settings.json] [out.json]

Writes content/maps/ember/watchtower-gate-floor007.json (serial 7: the campaign plays it; 006 stays as it was). The mission's own
settings (title, briefing, lighting, win rule, location, the party, the guards) are read from `settings.json` (default: the 006
map). Layout, 40 x 36 hexes, an octagonal tower about 1.6 times the old hall:

  south   the gate: the open stone door in the outer wall, the way out (dungeon-exit) just inside it, a walled vestibule where
          the party stands, a guard room west (tables, benches, weapon racks, the stairs down to the Salões das Fundações) and
          an armory east (racks, a shield stand, barrels, crates, a workbench), each opening on the vestibule and on the hall
  centre  the great hall: a ring of eight stone pillars round four braziers
  north   a dais with the garrison's banners, the stairs up to the Caserna in an alcove of the north wall
  light   wall torches along every wall, about every four hexes, and in each room

Checks before writing: the stairs, the exit and every guard are reachable from the party over passable, unblocked hexes;
nothing stands on the exit or the stairs; no two placements share a hex.
"""

import json
import math
import sys
from collections import deque

SRC = sys.argv[1] if len(sys.argv) > 1 else 'C:/Engine2/maps/ember/watchtower-gate-floor006.json'
OUT = sys.argv[2] if len(sys.argv) > 2 else 'content/maps/ember/watchtower-gate-floor007.json'
CATALOG = 'content/engine2/editor-catalog.json'

COLS, ROWS = 40, 36
SQ3 = math.sqrt(3.0)
catalog = json.load(open(CATALOG, encoding='utf-8'))['decorations']
src = json.load(open(SRC, encoding='utf-8'))
draft = src.get('draft', src)


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


# --- the tower's outline: an octagon in world units round the board's middle -----------------------------------------------
# A wall piece joins only its square-grid neighbours (left / right in its row, up / down in its column: MapStage's architecture
# connections), so the outline is laid in grid steps: the octagon's cut corners are staircases of row and column runs, and the
# wall is every inside cell with an outside cell among its eight grid neighbours (a closed ring of joined pieces; a diagonal run
# of hexes left each piece standing alone, with gaps).
CM, RM = (COLS - 1) / 2, (ROWS - 1) / 2
HALF_W, HALF_H, CUT = 29.0, 23.0, 40.0


def in_tower(c, r):
    dx, dz = abs(c - CM) * SQ3, abs(r - RM) * 1.5
    return dx <= HALF_W and dz <= HALF_H and dx + dz <= CUT


def grid8(c, r):
    return [(c + dc, r + dr) for dc in (-1, 0, 1) for dr in (-1, 0, 1) if (dc, dr) != (0, 0)]


inside = {p for p in cells() if in_tower(*p)}
tiles = {p: ('nave' if p in inside else 'void') for p in cells()}
outer_wall = {p for p in inside if any(n not in inside for n in grid8(*p)) or p[0] in (0, COLS - 1) or p[1] in (0, ROWS - 1)}
floor = inside - outer_wall
SOUTH = max(r for c, r in outer_wall)            # the flat south wall's row
NORTH = min(r for c, r in outer_wall)
MID = COLS // 2

# --- placements --------------------------------------------------------------------------------------------------------------
decos, occupied = [], {}


def footprint(did, x, y):
    fp = catalog[did].get('footprint') or [{'dx': 0, 'dy': 0}]
    return [(x + f['dx'], y + f['dy']) for f in fp]


def free(cs):
    return all(p in floor and p not in occupied for p in cs)


def place(did, x, y, block=False, rot=0, force=False, **extra):
    assert did in catalog, did
    cs = footprint(did, x, y)
    if not force and not free(cs):
        if did != 'light-wall-torch':
            print(f'note: {did} does not fit at {x},{y}: ' + ', '.join(
                f'{p} ' + ('off the floor' if p not in floor else f'taken by {occupied[p] or "a kept-open hex"}') for p in cs if not free([p])))
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


def place_near(did, area, target, block=True):
    """The piece on the free spot of `area` nearest `target` where its whole footprint fits inside the area."""
    for p in sorted(area, key=lambda q: math.hypot(wx(*q) - wx(*target), wz(q[1]) - wz(target[1]))):
        if all(q in area for q in footprint(did, *p)) and free(footprint(did, *p)):
            return place(did, *p, block=block)
    print(f'note: {did} found no room near {target}')
    return False


def wall(p):
    decos.append({'id': 'wall-3d-tower', 'x': p[0], 'y': p[1], 'rot': 0, 'wallOrientation': 'horizontal'})
    occupied[p] = 'wall-3d-tower'


# The gate: the open stone door in the south wall (2 hexes), the way out just inside it.
DOOR = (MID - 1, SOUTH)
door_cells = footprint('watchtower-stone-open-door-2hex', *DOOR)
assert all(p in outer_wall for p in door_cells), door_cells
for p in sorted(outer_wall - set(door_cells), key=lambda p: (p[1], p[0])):
    wall(p)
decos.append({'id': 'watchtower-stone-open-door-2hex', 'x': DOOR[0], 'y': DOOR[1], 'rot': 0, 'wallOrientation': 'horizontal'})
for p in door_cells:
    occupied[p] = 'door'
EXIT = (DOOR[0], SOUTH - 1)
assert place('dungeon-exit', *EXIT), 'exit'
exit_cells = set(footprint('dungeon-exit', *EXIT))

# Inner walls: the vestibule's two sides (straight up from the gate), the cross wall closing the guard room and the armory on
# the hall's side; each room keeps a 2-hex doorway onto the vestibule and onto the hall.
VEST_L, VEST_R = MID - 4, MID + 3          # the vestibule's side walls (columns)
CROSS = SOUTH - 8                          # the rooms' north wall (row)
inner = set()
for r in range(CROSS, SOUTH):
    inner |= {(VEST_L, r), (VEST_R, r)}
for c in range(0, COLS):
    if (c, CROSS) in floor and (c <= VEST_L or c >= VEST_R):
        inner.add((c, CROSS))
doorways = {(VEST_L, SOUTH - 4), (VEST_L, SOUTH - 5), (VEST_R, SOUTH - 4), (VEST_R, SOUTH - 5),   # rooms <-> vestibule
            (VEST_L - 6, CROSS), (VEST_L - 7, CROSS), (VEST_R + 6, CROSS), (VEST_R + 7, CROSS)}     # rooms <-> hall
for p in sorted(inner - doorways, key=lambda p: (p[1], p[0])):
    if p in floor:
        wall(p)
inner_walls = {p for p in inner - doorways if p in floor}


def room(c0, c1, r0, r1):
    return {(c, r) for c, r in floor if c0 <= c <= c1 and r0 <= r <= r1} - inner_walls


vestibule = room(VEST_L + 1, VEST_R - 1, CROSS + 1, SOUTH - 1)
guard_room = room(0, VEST_L - 1, CROSS + 1, SOUTH - 1)
armory = room(VEST_R + 1, COLS, CROSS + 1, SOUTH - 1)
hall = {p for p in floor if p[1] < CROSS} | {p for p in floor if p[1] == CROSS and p not in inner_walls}

# The stairs: up in an alcove of the north wall, down in the guard room's far corner.
UP = (MID + 9, NORTH + 2)
assert place('stone-stairs-up-001', *UP, targetMapId='watchtower-barracks', connectorDirection='up', rot=0), 'up stairs'
DOWN = (min(c for c, r in guard_room if r == SOUTH - 3) + 1, SOUTH - 3)
assert place('stone-stairs-down-001', *DOWN, targetMapId='watchtower-undercroft', connectorDirection='down', rot=0), 'down stairs'
stairs_cells = set(footprint('stone-stairs-up-001', *UP)) | set(footprint('stone-stairs-down-001', *DOWN))
# the hexes in front of each stair and of the exit stay open
keep_open = set()
for p in stairs_cells | exit_cells:
    keep_open |= set(neighbors(*p))
keep_open -= stairs_cells | exit_cells
for p in keep_open:
    occupied.setdefault(p, None)

# The great hall: eight stone pillars on a ring, four braziers round the middle, the dais' banners to the north.
HC = (MID, (NORTH + CROSS) // 2)
hcx, hcz = wx(*HC), wz(HC[1])
for i in range(8):
    a = math.pi * 2 * i / 8 + math.pi / 8
    x, z = hcx + 9.5 * math.cos(a), hcz + 8.0 * math.sin(a)
    p = min(floor, key=lambda q: math.hypot(wx(*q) - x, wz(q[1]) - z))
    place('ember-dungeon-pillar', *p, block=True)
for dc, dr in ((-2, 0), (2, 0), (0, -2), (0, 2)):
    place('light-brazier-bowl', HC[0] + dc, HC[1] + dr, block=True)
for dc in (-6, -2, 2, 6):
    place('city-banner-post', HC[0] + dc, NORTH + 2, block=True)
place('ember-interior-worn-rug', HC[0] - 1, HC[1] + 4)

# The guard room (wide at the hall's side, narrowing to the gate): a table with its benches, a second table with chairs, weapon
# racks against the vestibule wall, a bunk and a barrel in the narrow end.
gx = min(c for c, r in guard_room)
place_near('ember-tavern-table', guard_room, (gx + 3, CROSS + 2))
place_near('ember-interior-long-bench', guard_room, (gx + 3, CROSS + 1))
place_near('ember-interior-long-bench', guard_room, (gx + 3, CROSS + 3))
place_near('ember-tavern-table', guard_room, (gx + 8, CROSS + 2))
place_near('ember-tavern-chair', guard_room, (gx + 7, CROSS + 3))
place_near('ember-tavern-chair', guard_room, (gx + 10, CROSS + 2))
place_near('ember-treasure-weapon-rack', guard_room, (VEST_L - 1, CROSS + 1))
place_near('ember-treasure-weapon-rack', guard_room, (VEST_L - 1, SOUTH - 2))
place_near('ember-interior-simple-bed', guard_room, (VEST_L - 4, SOUTH - 1))
place_near('ember-props-barrel', guard_room, (gx + 2, CROSS + 4))

# The armory: racks and a shield stand on the walls, a workbench, a barrel rack, crates, sacks and barrels.
ax = max(c for c, r in armory)
place_near('ember-treasure-weapon-rack', armory, (VEST_R + 1, CROSS + 1))
place_near('ember-treasure-weapon-rack', armory, (VEST_R + 3, CROSS + 1))
place_near('ember-treasure-shield-stand', armory, (VEST_R + 2, SOUTH - 1))
place_near('ember-treasure-workbench', armory, (ax - 4, CROSS + 2))
place_near('ember-interior-barrel-rack', armory, (ax - 3, CROSS + 5))
place_near('ember-props-crate', armory, (ax - 1, CROSS + 1))
place_near('ember-props-crate', armory, (ax - 1, CROSS + 2))
place_near('ember-props-sacks', armory, (VEST_R + 6, SOUTH - 1))
place_near('ember-props-barrel', armory, (ax - 6, SOUTH - 2))
place_near('city-crate-stack', armory, (ax - 8, CROSS + 1))

# Supplies along the hall's sides (the old floor's crates, barrels and notice board, now in the hall).
place('city-barrels', min(c for c, r in hall if r == HC[1]) + 1, HC[1] - 3, block=True)
place('city-covered-crate', min(c for c, r in hall if r == HC[1]) + 1, HC[1] + 3, block=True)
place('city-crate-stack', max(c for c, r in hall if r == HC[1]) - 1, HC[1] - 3, block=True)
place('city-notice-board-2', MID + 4, CROSS - 1, block=True)

# Wall torches: on the floor hexes against the walls, about four hexes apart along each wall, never in a doorway, on the stairs
# or the exit, nor in front of them.
walls_all = (outer_wall - set(door_cells)) | inner_walls
against = [p for p in floor if any(n in walls_all for n in neighbors(*p)) and p not in occupied and p not in doorways]
torches = []
for p in sorted(against, key=lambda p: (wz(p[1]), wx(*p))):
    if all(math.hypot(wx(*p) - wx(*t), wz(p[1]) - wz(t[1])) >= 6.4 for t in torches):
        if place('light-wall-torch', *p):
            torches.append(p)

for p in keep_open:
    if occupied.get(p) is None:
        occupied.pop(p, None)

# --- the party and the guards -------------------------------------------------------------------------------------------------
party_src = draft['playerSpawns']
party_hexes = [(MID - 1, SOUTH - 3), (MID + 1, SOUTH - 3), (MID - 1, SOUTH - 5), (MID + 1, SOUTH - 5)]
player = [dict(s, x=p[0], y=p[1]) for s, p in zip(party_src, party_hexes)]
guards = draft['enemySpawns']
guard_hexes = [(HC[0] - 3, CROSS - 3), (HC[0] + 3, CROSS - 3),          # the hall's south side, facing the vestibule
               (VEST_R + 5, CROSS + 4),                                  # the armory
               (HC[0], NORTH + 4),                                       # the archer on the dais
               (gx + 6, CROSS + 3),                                      # the guard room
               (UP[0] - 2, UP[1] + 1)]                                   # the pikeman by the stairs up
E = [dict(s, x=p[0], y=p[1]) for s, p in zip(guards, guard_hexes)]
for s in player + E:
    p = (s['x'], s['y'])
    assert p in floor, (s['name'], p)
    if p in occupied:
        did = occupied.pop(p)
        decos[:] = [d for d in decos if not (d['id'] == did and p in footprint(d['id'], d['x'], d['y']))]
        print(f"note: {s['name']} at {p} took the place of {did}")

# --- checks -------------------------------------------------------------------------------------------------------------------
blocked = set(walls_all) | {p for d in decos if d.get('blocksPath') for p in footprint(d['id'], d['x'], d['y'])}
problems = []
units = {(s['x'], s['y']) for s in player + E}
for p in stairs_cells | exit_cells:
    if p in units:
        problems.append(f'a unit stands on the exit / stairs at {p}')
    if p in blocked:
        problems.append(f'the exit / stairs at {p} are blocked')


def reach(start):
    seen, q = {start}, deque([start])
    while q:
        p = q.popleft()
        for n in neighbors(*p):
            if n in seen or (n not in floor and n not in door_cells) or n in blocked:
                continue
            seen.add(n)
            q.append(n)
    return seen


seen = reach(party_hexes[0])
for name, cs in (('stairs up', footprint('stone-stairs-up-001', *UP)), ('stairs down', footprint('stone-stairs-down-001', *DOWN)),
                 ('exit', sorted(exit_cells))):
    if not any(p in seen for p in cs):
        problems.append(f'{name} unreachable')
for s in E:
    if not any(n in seen for n in [(s['x'], s['y']), *neighbors(s['x'], s['y'])]):
        problems.append(f"{s['name']} at {s['x']},{s['y']} unreachable")
for room_name, cs in (('vestibule', vestibule), ('guard room', guard_room), ('armory', armory), ('hall', hall)):
    open_cells = [p for p in cs if p not in blocked]
    if not any(p in seen for p in open_cells):
        problems.append(f'the {room_name} cannot be entered')
walkable = sum(1 for p in floor if p not in blocked)
print(f'board {COLS}x{ROWS}, floor {len(floor)} (old hall 496), walkable {walkable}, reachable {len(seen)}, '
      f'walls {len(walls_all)}, torches {len(torches)}, decorations {len(decos)}, guards {len(E)}')
print(f'gate {DOOR} exit {EXIT} up {UP} down {DOWN} hall centre {HC}')
if problems:
    print('PROBLEMS:\n  ' + '\n  '.join(problems))
    sys.exit(1)

# --- write --------------------------------------------------------------------------------------------------------------------
order = [(c, r) for r in range(ROWS) for c in range(COLS)]
draft = dict(draft)
draft.update({
    'cols': COLS, 'rows': ROWS,
    'tiles': [tiles[p] for p in order],
    'tileVariants': [None] * (COLS * ROWS),
    'tileRots': [0] * (COLS * ROWS),
    'decorations': decos,
    'playerSpawns': player,
    'enemySpawns': E,
    'emberTiles': 'torre-v01',
})
out = {'serial': 7, 'savedAt': src.get('savedAt', 0) + 1, 'draft': draft}
with open(OUT, 'w', encoding='utf-8') as f:
    json.dump(out, f, ensure_ascii=False)
print('wrote', OUT)
