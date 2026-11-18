"""Torre de Vigia at three times the size: every floor 3x wider and 3x taller, the same building (the user, 2026-11-17: "everything
is too small and claustrophobic", "every single map in the tower 3x").

  python -I tools/scripts/maps/scale_watchtower_3x.py

Each floor is scaled on the hex lattice (axial coordinates x3): every wall line is drawn again three times as long and closed (the
hexes between two neighbouring wall pieces become wall), windows, slits and torches stay where they were along it, doors, stairs and
the gate keep their real size (2 and 4 hexes) in the longer wall, furniture and enemies move with their room, the party keeps its
formation around its leader. The floor is everything inside the outer wall. Writes a new version of each floor (the old files stay)
and moves the Térreo's Ativar pin (content/maps/active.json) to its new version.
"""
import json
import time
from collections import deque
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
MAPS = ROOT / 'content/maps/ember'
ACTIVE = ROOT / 'content/maps/active.json'
K = 3
MARGIN = 2

FLOORS = [  # (source stem, new stem, new serial)
    ('watchtower-gate-floor010', 'watchtower-gate-floor013', 13),
    ('watchtower-barracks007', 'watchtower-barracks008', 8),
    ('watchtower-command006', 'watchtower-command007', 7),
    ('watchtower-beacon007', 'watchtower-beacon008', 8),
    ('watchtower-undercroft004', 'watchtower-undercroft005', 5),
    ('watchtower-prison007', 'watchtower-prison008', 8),
]
WALL = 'ember-arch-watchtower-wall'
LINE = {WALL, 'ember-arch-watchtower-wall-torch', 'ember-arch-watchtower-window', 'ember-arch-watchtower-slit',
        'ember-arch-watchtower-door', 'ember-arch-watchtower-door-locked', 'ember-arch-watchtower-gate'}
SPAN = {'ember-arch-watchtower-door': 2, 'ember-arch-watchtower-door-locked': 2, 'ember-arch-watchtower-gate': 4}


# --- odd-r offset <-> cube --------------------------------------------------------------------------------------------------------
def cube(x, y):
    q = x - (y - (y & 1)) // 2
    return q, y, -q - y


def offset(q, r):
    return q + (r - (r & 1)) // 2, r


def neighbors(x, y):
    d = ((1, 0), (1, -1), (0, -1), (-1, 0), (0, 1), (1, 1)) if y & 1 else ((1, 0), (0, -1), (-1, -1), (-1, 0), (-1, 1), (0, 1))
    return [(x + a, y + b) for a, b in d]


def dist(a, b):
    (aq, ar, as_), (bq, br, bs) = cube(*a), cube(*b)
    return max(abs(aq - bq), abs(ar - br), abs(as_ - bs))


def cube_round(q, r):
    s = -q - r
    rq, rr, rs = round(q), round(r), round(s)
    dq, dr, ds = abs(rq - q), abs(rr - r), abs(rs - s)
    if dq > dr and dq > ds:
        rq = -rr - rs
    elif dr > ds:
        rr = -rq - rs
    return rq, rr


def line(a, b):
    (aq, ar, _), (bq, br, _) = cube(*a), cube(*b)
    n = max(dist(a, b), 1)
    out = []
    for i in range(n + 1):
        t = i / n
        out.append(offset(*cube_round(aq + (bq - aq) * t + 1e-6, ar + (br - ar) * t + 2e-6)))
    return out


def scale(x, y):
    q, r, _ = cube(x, y)
    return offset(K * q, K * r)


# --- one floor --------------------------------------------------------------------------------------------------------------------
def scale_floor(src_stem, new_stem, serial):
    d = json.loads((MAPS / f'{src_stem}.json').read_text(encoding='utf-8'))['draft']
    C, R = d['cols'], d['rows']
    void = {(i % C, i // C) for i, t in enumerate(d['tiles']) if t == 'void'}

    def near_void(c):
        return any(n in void or not (0 <= n[0] < C and 0 <= n[1] < R) for n in neighbors(*c))

    # every hex a wall line passes through in the source (a door / the gate covers its whole span)
    owner = {}
    for o in d['decorations']:
        if o['id'] in LINE:
            for k in range(SPAN.get(o['id'], 1)):
                owner[(o['x'] + k, o['y'])] = o
    cells = set(owner)
    # the scaled grid: shift so that everything sits MARGIN hexes in (an even row shift keeps the lattice)
    corners = [scale(x, y) for x in (0, C - 1) for y in (0, R - 1)]
    minx = min(c[0] for c in corners) - MARGIN
    miny = min(c[1] for c in corners) - MARGIN
    miny -= miny & 1
    NC = max(c[0] for c in corners) - minx + MARGIN + 2
    NR = max(c[1] for c in corners) - miny + MARGIN + 2

    def S(x, y):
        sx, sy = scale(x, y)
        return sx - minx, sy - miny

    walls = {}  # new hex -> piece id
    OPENING = {'ember-arch-watchtower-window', 'ember-arch-watchtower-slit'}
    for c in cells:
        for n in neighbors(*c):
            if n in cells and n > c:
                hexes = line(S(*c), S(*n))
                for h in hexes[1:-1]:
                    # a window / slit grows with its wall: the hex of the line right beside it is window too (3 hexes wide at 3x)
                    pid = WALL
                    for end in (c, n):
                        if owner[end]['id'] in OPENING and dist(h, S(*end)) == 1:
                            pid = owner[end]['id']
                    walls.setdefault(h, pid)
                for h in (hexes[0], hexes[-1]):
                    walls.setdefault(h, WALL)
    # the outer wall closes across one-hex gaps (two outer pieces two hexes apart with floor between)
    outer = [c for c in cells if near_void(c)]
    for i, a in enumerate(outer):
        for b in outer[i + 1:]:
            if dist(a, b) == 2 and not any(m in cells for m in neighbors(*a) if m in neighbors(*b)):
                for h in line(S(*a), S(*b)):
                    walls.setdefault(h, WALL)
    pieces = {}
    for h, pid in walls.items():
        pieces[h] = {'id': pid, 'x': h[0], 'y': h[1], 'rot': 0, 'wallOrientation': 'horizontal'}
    spans = []
    for o in d['decorations']:
        if o['id'] in LINE and o['id'] != WALL:
            x, y = S(o['x'], o['y'])
            for k in range(SPAN.get(o['id'], 1)):
                pieces.pop((x + k, y), None)
            pieces[(x, y)] = dict(o, x=x, y=y)
            spans.append((x, y, SPAN.get(o['id'], 1)))
    blocked = set(pieces)
    for x, y, n in spans:
        blocked |= {(x + k, y) for k in range(n)}

    # the inside: everything the outside cannot reach without crossing a wall
    outside = set()
    todo = deque((x, y) for x in range(NC) for y in (0, NR - 1)) + deque((x, y) for y in range(NR) for x in (0, NC - 1))
    while todo:
        c = todo.popleft()
        if c in outside or c in blocked or not (0 <= c[0] < NC and 0 <= c[1] < NR):
            continue
        outside.add(c)
        todo.extend(neighbors(*c))
    # the pockets a thick wall leaves between its two faces are solid stone, not floor: whatever the party cannot reach walking
    # (doors and the gate count as open) and is small belongs to the outside
    walls_only = {h for h, p in pieces.items() if p['id'] not in SPAN}
    lead = d['playerSpawns'][0]
    start, reach, todo = S(lead['x'], lead['y']), set(), deque()
    todo.append(start)
    while todo:
        c = todo.popleft()
        if c in reach or c in outside or c in walls_only or not (0 <= c[0] < NC and 0 <= c[1] < NR):
            continue
        reach.add(c)
        todo.extend(neighbors(*c))
    pocket_seen = set()
    for y in range(NR):
        for x in range(NC):
            c = (x, y)
            if c in reach or c in outside or c in blocked or c in pocket_seen:
                continue
            group, todo = set(), deque([c])
            while todo:
                g = todo.popleft()
                if g in group or g in reach or g in outside or g in blocked or not (0 <= g[0] < NC and 0 <= g[1] < NR):
                    continue
                group.add(g)
                todo.extend(neighbors(*g))
            pocket_seen |= group
            if len(group) < 40:
                outside |= group
    inside_count = NC * NR - len(outside)
    if inside_count < K * K * (C * R - len(void)) * 0.6:
        raise SystemExit(f'{src_stem}: the outer wall leaks ({inside_count} hexes inside)')

    # tiles: outside void, inside the source hex's own tile (nearest source hex)
    tiles, variants, rots = [], [], []
    for y in range(NR):
        for x in range(NC):
            if (x, y) in outside:
                tiles.append('void'), variants.append(None), rots.append(0)
                continue
            q, r, _ = cube(x + minx, y + miny)
            sq, sr = cube_round(q / K, r / K)
            sx, sy = offset(sq, sr)
            if 0 <= sx < C and 0 <= sy < R and (sx, sy) not in void:
                i = sy * C + sx
                tiles.append(d['tiles'][i]), variants.append(d['tileVariants'][i]), rots.append(d['tileRots'][i])
            else:
                tiles.append(d.get('baseTile') or 'nave'), variants.append(None), rots.append(0)

    taken = set(blocked)

    def free_near(x, y):
        seen, todo = set(), deque([(x, y)])
        while todo:
            c = todo.popleft()
            if c in seen:
                continue
            seen.add(c)
            if c not in taken and c not in outside and 0 <= c[0] < NC and 0 <= c[1] < NR:
                return c
            todo.extend(neighbors(*c))
        raise SystemExit(f'no free hex near {x},{y}')

    decorations = list(pieces.values())
    for o in d['decorations']:
        if o['id'] in LINE:
            continue
        x, y = S(o['x'], o['y'])
        decorations.append(dict(o, x=x, y=y))
        taken.add((x, y))
        if 'arch-watchtower-stairs' in o['id']:
            taken.add((x + 1, y))

    def move_units(units, keep_formation):
        out = []
        if not units:
            return out
        lead = units[0]
        lx, ly = S(lead['x'], lead['y'])
        lq, lr, _ = cube(lx, ly)
        oq, orr, _ = cube(lead['x'], lead['y'])
        for u in units:
            if keep_formation:
                q, r, _ = cube(u['x'], u['y'])
                x, y = offset(lq + q - oq, lr + r - orr)
            else:
                x, y = S(u['x'], u['y'])
            x, y = free_near(x, y)
            taken.add((x, y))
            v = dict(u, x=x, y=y)
            if isinstance(u.get('dialogRequiresOpenDoor'), dict):
                v['dialogRequiresOpenDoor'] = dict(zip(('x', 'y'), S(u['dialogRequiresOpenDoor']['x'], u['dialogRequiresOpenDoor']['y'])))
            out.append(v)
        return out

    new = dict(d)
    new.update({'cols': NC, 'rows': NR, 'tiles': tiles, 'tileVariants': variants, 'tileRots': rots, 'decorations': decorations,
                'playerSpawns': move_units(d.get('playerSpawns', []), True),
                'enemySpawns': move_units(d.get('enemySpawns', []), False),
                'neutralSpawns': move_units(d.get('neutralSpawns', []), False)})
    extra = EXTRA.get(new_stem)
    if extra:
        extra(new, taken, outside)
    path = MAPS / f'{new_stem}.json'
    if path.exists():
        raise SystemExit(f'{path.name} already exists: not overwritten')
    path.write_text(json.dumps({'serial': serial, 'savedAt': int(time.time() * 1000), 'draft': new}, ensure_ascii=False, indent=1) + '\n',
                    encoding='utf-8')
    print(f'{new_stem}: {NC}x{NR} ({inside_count} floor hexes, was {C * R - len(void)}), {len(decorations)} pieces')


def undercroft_arcades(d, taken, outside):
    """The foundations' hall carries the tower on rows of pillars: at three times the size, a pillar every six hexes."""
    have = [o for o in d['decorations'] if o['id'] == 'ember-dungeon-pillar-v02']
    xs, ys = [o['x'] for o in have], [o['y'] for o in have]
    added = 0
    for y in range(min(ys), max(ys) + 1, 6):
        for x in range(min(xs), max(xs) + 1, 6):
            if (x, y) in taken or (x, y) in outside or any((n in taken) for n in neighbors(x, y)):
                continue
            d['decorations'].append({'id': 'ember-dungeon-pillar-v02', 'x': x, 'y': y, 'blocksPath': True})
            taken.add((x, y))
            added += 1
    print(f'  + {added} pillars in the arcades')


EXTRA = {'watchtower-undercroft005': undercroft_arcades}


def main():
    for src, new, serial in FLOORS:
        scale_floor(src, new, serial)
    active = json.loads(ACTIVE.read_text(encoding='utf-8'))
    active['watchtower-gate-floor'] = 13
    text = ACTIVE.read_text(encoding='utf-8')
    ACTIVE.write_text(json.dumps(active, ensure_ascii=False, indent=2) + ('\n' if text.endswith('\n') else ''), encoding='utf-8')


if __name__ == '__main__':
    main()
