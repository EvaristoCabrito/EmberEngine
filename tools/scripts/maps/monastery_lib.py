"""The Monastery maps' shared builder (make_monastery.py = the ground floor, make_monastery_upper.py = Andar II and III): odd-r hex
grids, the monastery kit's walls (ember-arch-mosteiro-*), the catalog's footprints, the people with their conversations, the
checks (no piece on a wall / a person / another piece, every NPC, door and stair reachable on foot, the locked doors really
locked) and the file written as the next serial of its id (never an overwrite).
"""
import json
import os
import sys
import time
from collections import Counter, deque

sys.stdout.reconfigure(encoding='utf-8')
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
MAPS = os.path.join(ROOT, 'content', 'maps', 'ember')
CATALOG = json.load(open(os.path.join(ROOT, 'content', 'data', 'editor-catalog.json'), encoding='utf-8'))['decorations']

WALL, WINDOW, SLIT, TORCH = 'ember-arch-mosteiro-wall', 'ember-arch-mosteiro-window', 'ember-arch-mosteiro-slit', 'ember-arch-mosteiro-wall-torch'
PORTAL, DOOR, GATE = 'ember-arch-mosteiro-portal', 'ember-arch-mosteiro-door', 'ember-arch-mosteiro-gate'
STAIRS_UP, STAIRS_DOWN = 'ember-arch-mosteiro-stairs-up', 'ember-arch-mosteiro-stairs-down'
# the precinct's muralha (openAir: joins and shadows like a wall, encloses no room: the yard stays under the sky)
RAMPART, RAMPART_TORCH, RAMPART_GATE = 'ember-arch-mosteiro-rampart', 'ember-arch-mosteiro-rampart-torch', 'ember-arch-mosteiro-rampart-gate'
LOCKED = 'dungeon-stone-door'  # a lock: "Arrombar" opens it (BattleExport: every model3d door decoration is a lock)
ARCH = {WALL, WINDOW, SLIT, TORCH, PORTAL, DOOR, GATE, STAIRS_UP, STAIRS_DOWN, LOCKED, RAMPART, RAMPART_TORCH, RAMPART_GATE}
OPEN_PIECES = {PORTAL, DOOR, GATE, RAMPART_GATE}  # the party passes (the door opens for whoever comes)
PASSABLE_PROPS = {'light-candle', 'ember-interior-worn-rug', 'floor-connector', 'city-shrineCandle', 'ember-nature-leaf-litter'}


def cells_of(ident, x, y):
    if ident not in CATALOG:
        raise SystemExit(f'unknown decoration {ident} (catalogue the kit first: add_kits_to_catalog.py mosteiro + catalog_models.py)')
    fp = CATALOG[ident].get('footprint') or [{'dx': 0, 'dy': 0}]
    if isinstance(fp, dict):
        fp = [fp]
    return [(x + f.get('dx', 0), y + f.get('dy', 0)) for f in fp]


def ring(x0, y0, x1, y1):
    return {(x, y0) for x in range(x0, x1 + 1)} | {(x, y1) for x in range(x0, x1 + 1)} | {(x0, y) for y in range(y0, y1 + 1)} | {(x1, y) for y in range(y0, y1 + 1)}


def hwall(y, x0, x1):
    return {(x, y) for x in range(x0, x1 + 1)}


def vwall(x, y0, y1):
    return {(x, y) for y in range(y0, y1 + 1)}


# --- conversations (the game's DialogTree: lines with replies branching by `next`) -------------------------------------------
def tree(ident, speaker, lines):
    return {'id': ident, 'startId': lines[0]['id'], 'lines': [dict(l, speaker=l.get('speaker', speaker)) for l in lines]}


def L(ident, text, next_id=None, replies=None, speaker=None):
    d = {'id': ident, 'text': text}
    if replies:
        d['replies'] = replies
    else:
        d['next'] = next_id
    if speaker:
        d['speaker'] = speaker
    return d


def R(text, next_id=None, **extra):
    d = {'text': text}
    if next_id:
        d['next'] = next_id
    d.update(extra)
    return d


class Floor:
    def __init__(self, map_id, cols, rows, base_tile='plains'):
        self.id, self.cols, self.rows = map_id, cols, rows
        self.tiles = [base_tile] * (cols * rows)
        self.walls, self.openings = set(), []
        self.windows, self.slits, self.torches = set(), set(), set()
        self.ramparts = set()  # wall cells that are the muralha (RAMPART / RAMPART_TORCH instead of WALL / TORCH)
        self.decorations, self.owner, self.people, self.spawns = [], {}, {}, []
        self.neutrals, self.players, self.enemies = [], [], []

    # tiles
    def put(self, x, y, t):
        if 0 <= x < self.cols and 0 <= y < self.rows:
            self.tiles[y * self.cols + x] = t

    def rect(self, x0, y0, x1, y1, t):
        for y in range(y0, y1 + 1):
            for x in range(x0, x1 + 1):
                self.put(x, y, t)

    def neighbours(self, x, y):
        odd = y & 1
        for dx, dy in ((1, 0), (-1, 0), (odd, -1), (odd - 1, -1), (odd, 1), (odd - 1, 1)):
            n = (x + dx, y + dy)
            if 0 <= n[0] < self.cols and 0 <= n[1] < self.rows:
                yield n

    # walls
    def opening(self, piece, x, y):
        self.openings.append((piece, x, y))

    def lay_walls(self):
        taken = set()
        for piece, x, y in self.openings:
            cells = cells_of(piece, x, y)
            for c in cells:
                if c not in self.walls:
                    raise SystemExit(f'{self.id}: {piece} at {x},{y}: {c} is not on a wall')
            taken |= set(cells)
            self.decorations.append({'id': piece, 'x': x, 'y': y, 'rot': 0, 'wallOrientation': 'horizontal'})
            for c in cells:
                self.owner[c] = piece
        for c in self.windows | self.slits | self.torches:
            if c not in self.walls:
                raise SystemExit(f'{self.id}: {c} marked as a window / slit / torch is not on a wall')
            if c in taken:
                raise SystemExit(f'{self.id}: {c} is both an opening and a window / slit / torch')
        for (x, y) in sorted(self.walls):
            if (x, y) in taken:
                continue
            pid = WINDOW if (x, y) in self.windows else SLIT if (x, y) in self.slits else TORCH if (x, y) in self.torches else WALL
            if (x, y) in self.ramparts:
                pid = RAMPART_TORCH if (x, y) in self.torches else RAMPART
            vertical = (x, y - 1) in self.walls or (x, y + 1) in self.walls
            horizontal = (x - 1, y) in self.walls or (x + 1, y) in self.walls
            self.decorations.append({'id': pid, 'x': x, 'y': y, 'rot': 0, 'wallOrientation': 'horizontal' if horizontal or not vertical else 'vertical'})
            self.owner[(x, y)] = pid

    # props and people
    def place(self, ident, x, y, rot=None, **extra):
        cells = cells_of(ident, x, y)
        for c in cells:
            if not (0 <= c[0] < self.cols and 0 <= c[1] < self.rows):
                raise SystemExit(f'{self.id}: {ident} at {x},{y} is off the board at {c}')
            if c in self.owner and ident != 'floor-connector':
                raise SystemExit(f'{self.id}: {ident} at {x},{y} lands on {self.owner[c]} at {c}')
            if c in self.people and ident not in PASSABLE_PROPS:
                raise SystemExit(f'{self.id}: {ident} at {x},{y} lands on {self.people[c]}')
        d = {'id': ident, 'x': x, 'y': y}
        if rot is not None:
            d['rot'] = rot
        d.update(extra)
        self.decorations.append(d)
        if ident != 'floor-connector':
            for c in cells:
                self.owner[c] = ident
        return d

    def stairs(self, piece, x, y, target, direction, back=False):
        self.place(piece, x, y, rot=0, wallOrientation='horizontal')
        extra = {'targetMapId': target, 'connectorDirection': direction}
        if back:
            extra['returnConnector'] = True
        self.place('floor-connector', x, y, **extra)

    def person(self, name, class_id, x, y, level=3, dialog=None, player=False, enemy=False):
        c = (x, y)
        if c in self.owner and self.owner[c] not in PASSABLE_PROPS:
            raise SystemExit(f'{self.id}: {name} stands on {self.owner[c]} at {c}')
        if c in self.people:
            raise SystemExit(f'{self.id}: {name} stands on {self.people[c]} at {c}')
        self.people[c] = name
        s = {'name': name, 'classId': class_id, 'x': x, 'y': y, 'level': level}
        if dialog:
            s['dialog'] = dialog
        (self.players if player else self.enemies if enemy else self.neutrals).append(s)
        return s

    # checks and the file
    def reach(self, start, unlock=False):
        blocked = {c for c, i in self.owner.items() if i in ARCH and i not in (STAIRS_UP, STAIRS_DOWN)}
        for piece, x, y in self.openings:
            if piece in OPEN_PIECES or (unlock and piece == LOCKED):
                blocked -= set(cells_of(piece, x, y))
        seen, todo = {start}, deque([start])
        while todo:
            x, y = todo.popleft()
            for n in self.neighbours(x, y):
                if n not in seen and n not in blocked and n not in self.people:
                    seen.add(n)
                    todo.append(n)
        return seen

    def check(self, spawn, sealed=(), beyond=()):
        """sealed: the locked doors' cells (must not be walkable); beyond: cells that must be unreachable from the spawn."""
        for d in self.decorations:
            if d['id'] in PASSABLE_PROPS or d['id'] in ARCH:
                continue
            for c in cells_of(d['id'], d['x'], d['y']):
                if c in self.people:
                    raise SystemExit(f"{self.id}: {d['id']} at {d['x']},{d['y']} lands on {self.people[c]}")
        seen = self.reach(spawn, unlock=True)  # the locked doors opened: everyone, every door and stair must be reachable then
        for c, who in self.people.items():
            if c != spawn and not any(n in seen for n in self.neighbours(*c)):
                raise SystemExit(f'{self.id}: nobody can walk beside {who} at {c}')
        for piece, x, y in self.openings:
            if piece == LOCKED:
                continue
            if not any(n in seen for c in cells_of(piece, x, y) for n in self.neighbours(*c)):
                raise SystemExit(f'{self.id}: the {piece} at {x},{y} cannot be reached')
        for d in self.decorations:
            if d['id'] in (STAIRS_UP, STAIRS_DOWN) and (d['x'], d['y']) not in beyond and (d['x'], d['y']) not in seen \
                    and not any(n in seen for n in self.neighbours(d['x'], d['y'])):
                raise SystemExit(f"{self.id}: the stairs at {d['x']},{d['y']} cannot be reached")
        seen = self.reach(spawn)  # and with them shut, what they seal stays sealed
        for c in sealed:
            if c in seen:
                raise SystemExit(f'{self.id}: the locked door at {c} is not a lock')
        for c in beyond:
            if c in seen:
                raise SystemExit(f'{self.id}: {c} is reachable without opening the locked door')
        return seen

    def show(self):
        grid = [['.'] * self.cols for _ in range(self.rows)]
        marks = {WINDOW: 'W', SLIT: 's', TORCH: 'T', DOOR: 'D', PORTAL: 'O', GATE: 'G', RAMPART_GATE: 'G', LOCKED: 'L', STAIRS_DOWN: 'S', STAIRS_UP: 'U', RAMPART: '=', RAMPART_TORCH: 't'}
        for (x, y), i in self.owner.items():
            grid[y][x] = marks.get(i, '#' if i == WALL else 'c' if i == 'light-candle' else 'o')
        for (x, y), who in self.people.items():
            grid[y][x] = 'K' if who == 'Kael' else 'N'
        for y, row in enumerate(grid):
            print(f'{y:2d} ' + ''.join(row))

    def write(self, base, seen, **fields):
        draft = dict(base)
        n = self.cols * self.rows
        draft.update({'id': self.id, 'cols': self.cols, 'rows': self.rows, 'tiles': list(self.tiles), 'tileVariants': [None] * n,
                      'terrainElevations': [0] * n, 'waterLevels': [0] * n, 'tileRots': [0] * n, 'decorations': self.decorations,
                      'elementalFx': [], 'playerSpawns': self.players, 'enemySpawns': self.enemies, 'neutralSpawns': self.neutrals,
                      'win': 'rout', 'hub': False, 'explore': True, 'locationId': 'mosteiro'})
        for key in ('aboveGround', 'underground', 'emberTiles', 'emberFloor', 'emberBaseLevels'):
            draft.pop(key, None)
        draft.update(fields)
        serial = 0
        for f in os.listdir(MAPS):
            if f.startswith(self.id) and len(f) == len(self.id) + 8 and f[len(self.id):-5].isdigit():
                serial = max(serial, int(f[len(self.id):-5]))
        serial += 1
        out = os.path.join(MAPS, f'{self.id}{serial:03}.json')
        if os.path.exists(out):
            raise SystemExit(f'{out} exists')
        self.show()
        json.dump({'serial': serial, 'savedAt': int(time.time() * 1000), 'draft': draft}, open(out, 'w', encoding='utf-8'), ensure_ascii=False)
        print(f'wrote {os.path.relpath(out, ROOT)}: {len(self.decorations)} pieces, {len(self.neutrals)} people, {len(seen)} hexes reachable;',
              dict(Counter(d['id'] for d in self.decorations).most_common(10)))
        return out


def base_draft(name='cruzadotest001'):
    return json.load(open(os.path.join(MAPS, name + '.json'), encoding='utf-8'))['draft']
