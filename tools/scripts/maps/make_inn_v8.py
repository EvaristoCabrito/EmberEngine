"""A Estalagem, both floors, larger and walled (2026-10-25; the user, with the Inn's word: "make better walls, larger tavern overall ...
add the 3D objects, not just a couple of table", "even the main Map, it cant be just one wide open area, very amateur looking", "we need
one big open area with large tables and all that, but some separate rooms for NPCs and services, use the old school warrior, besteiro
etc sprites instead of enemies", the seven Andar II torches moved inside the walls).

  python -I tools/scripts/maps/make_inn_v8.py

Writes estalagem008 (from 007) and estalagem-andar-2007 (from 006); every older version untouched; refuses to overwrite.

Both floors are closed buildings of the Inn's 3D kit (ember-arch-inn-*): an outer ring of walls with casement windows, inner walls
with animated doors, candle lanterns (ember-arch-inn-sconce) on the walls in place of the torch cards (a sconce lights the side of
its wall that faces the board's centre, so every lantern stands on a wall whose centre side is the room it lights).

Salão (40 x 28): the great hall (x 1-29, y 10-26) with the bar along its north wall (Brue), the Meshy hearth in its north-west corner,
three rows of long refectory tables with benches and chairs, small tables along the south wall; north wing: the kitchen, the store
behind the bar, the healer's room, Vargan's armoury; east wing: the back room (the shady patron, the guest) and the stairs room.
Andar II (41 x 27): eight bedrooms (four above the corridor, four below; the stairs room in the south-east), the corridor between.

The NPCs keep their dialogs; the enemy sprites they wore become the old soldiers' (Vargan: Guerreiro; the off-duty guard: Besteiro;
the guest: Ofélia's sprite, a woman); four mercenaries drink at a long table (Soldado, Piqueiro, Besteiro, Capitão).

The script refuses (and writes nothing) if a piece lands on a wall, another piece, an NPC or off the board, if a door, the stairs or
any NPC cannot be reached on foot from the spawn, or if a lantern stands on a wall that is not between its room and the centre.
"""
import json
import os
import time
from collections import Counter, deque

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
MAPS = os.path.join(ROOT, 'content', 'maps', 'ember')

WALL, WINDOW, DOOR, DOORWAY, SCONCE = ('ember-arch-inn-wall', 'ember-arch-inn-window', 'ember-arch-inn-door', 'ember-arch-inn-doorway',
                                       'ember-arch-inn-sconce')
ARCH = {WALL, WINDOW, DOOR, DOORWAY, SCONCE}
# 3D pieces and their footprints (one: the anchor; pair: anchor + east; triple: west, anchor, east)
FOOT = {
    'ember-tavern-table-messhall': 'triple', 'ember-tavern-table-small': 'pair', 'ember-tavern-table-v02': 'pair',
    'ember-tavern-counter': 'pair', 'ember-tavern-stone-hearth': 'pair', 'ember-interior-long-bench-v02': 'pair',
    'ember-interior-simple-bed-v02': 'pair', 'ember-interior-worn-rug': 'pair', 'ember-interior-barrel-rack-v02': 'pair',
    'ember-treasure-workbench-v02': 'pair', 'ember-treasure-shield-stand-v02': 'pair', 'ember-interior-folding-screen': 'pair',
    'ember-interior-cloth-curtain': 'pair', 'inn-stairs-up': 'pair', 'inn-stairs-down': 'pair',
}
PASSABLE = {'light-candle', 'ember-interior-worn-rug', 'floor-connector'}  # never hold the path
STAIRS = {'inn-stairs-up', 'inn-stairs-down', 'floor-connector'}
# chair facings (the 3D chair's back: v02 toward +z = the next row at rot 0; v04 the other way)
CHAIR, CHAIR4 = 'ember-tavern-chair-v02', 'ember-tavern-chair-v04'
FACES = {'n': 0, 'e': 1, 's': 2, 'w': 3}  # the way a v02 chair faces at each rot (back to the opposite side)


def chair(x, y, face, v04=False):
    rot = FACES[face]
    return {'id': CHAIR4 if v04 else CHAIR, 'x': x, 'y': y, 'rot': (rot + 2) % 4 if v04 else rot}


def p(ident, x, y, rot=0, **extra):
    d = {'id': ident, 'x': x, 'y': y}
    if rot:
        d['rot'] = rot
    d.update(extra)
    return d


def cells_of(d):
    foot = FOOT.get(d['id'], 'one')
    x, y = d['x'], d['y']
    if foot == 'pair':
        return [(x, y), (x + 1, y)]
    if foot == 'triple':
        return [(x - 1, y), (x, y), (x + 1, y)]
    return [(x, y)]


def neighbours(x, y):
    odd = y & 1
    for dx, dy in ((1, 0), (-1, 0), (odd, -1), (odd - 1, -1), (odd, 1), (odd - 1, 1)):
        yield x + dx, y + dy


def walls_of(cols, rows, runs):
    """The ring plus the inner runs: ('h', y, x0, x1) / ('v', x, y0, y1), inclusive."""
    cells = {(x, 0) for x in range(cols)} | {(x, rows - 1) for x in range(cols)} | {(0, y) for y in range(rows)} | {(cols - 1, y) for y in range(rows)}
    for kind, a, b0, b1 in runs:
        for b in range(b0, b1 + 1):
            cells.add((b, a) if kind == 'h' else (a, b))
    return cells


def build(src_name, dst_name, serial, cols, rows, runs, windows, doors, doorways, sconces, furniture, npcs, spawn, extra_npcs, sprites):
    path = os.path.join(MAPS, dst_name + '.json')
    if os.path.exists(path):
        raise SystemExit(f'{dst_name}.json already exists: a change is a new version, never an overwrite')
    with open(os.path.join(MAPS, src_name + '.json'), encoding='utf-8') as f:
        file = json.load(f)
    draft = file['draft']
    base, variant = draft['baseTile'], draft['baseVariant']
    draft['cols'], draft['rows'] = cols, rows
    draft['tiles'] = [base] * (cols * rows)
    draft['tileVariants'] = [variant] * (cols * rows)
    draft['tileRots'] = [0] * (cols * rows)
    walls = walls_of(cols, rows, runs)
    openings = {(x, y) for x, y, _ in doors} | {(x, y) for x, y, _ in doorways}
    for c in openings | set(windows) | set(sconces):
        if c not in walls:
            raise SystemExit(f'{dst_name}: {c} is not on a wall')
    centre = ((cols - 1) / 2, (rows - 1) / 2)
    decos = []
    for (x, y) in sorted(walls):
        if (x, y) in openings:
            continue
        pid = WINDOW if (x, y) in windows else SCONCE if (x, y) in sconces else WALL
        vertical = (x, y - 1) in walls or (x, y + 1) in walls
        horizontal = (x - 1, y) in walls or (x + 1, y) in walls
        decos.append({'id': pid, 'x': x, 'y': y, 'rot': 0, 'wallOrientation': 'horizontal' if horizontal or not vertical else 'vertical'})
    for pid, places in ((DOOR, doors), (DOORWAY, doorways)):
        for x, y, rot in places:
            decos.append({'id': pid, 'x': x, 'y': y, 'rot': rot, 'wallOrientation': 'horizontal' if rot % 2 == 0 else 'vertical'})
    owner = {c: 'wall' for c in walls - openings}
    people = {}
    for s in npcs + extra_npcs:
        people[(s['x'], s['y'])] = s['name']
    people[spawn] = 'Kael'
    for d in furniture:
        for c in cells_of(d):
            if not (0 < c[0] < cols - 1 and 0 < c[1] < rows - 1):
                raise SystemExit(f'{dst_name}: {d["id"]} at {d["x"]},{d["y"]} reaches the ring or off the board at {c}')
            if d['id'] == 'floor-connector':
                continue
            if c in owner:
                raise SystemExit(f'{dst_name}: {d["id"]} at {d["x"]},{d["y"]} lands on {owner[c]} at {c}')
            if c in people and d['id'] not in PASSABLE:
                raise SystemExit(f'{dst_name}: {d["id"]} at {d["x"]},{d["y"]} lands on {people[c]}')
            if c in openings:
                raise SystemExit(f'{dst_name}: {d["id"]} at {d["x"]},{d["y"]} blocks the doorway {c}')
            owner[c] = d['id']
        decos.append(d)
    # a sconce lights the side of its wall facing the board's centre: that side must be inside the building (not the ring's outside)
    for x, y in sconces:
        dx, dy = centre[0] - x, centre[1] - y
        horizontal = (x - 1, y) in walls or (x + 1, y) in walls
        step = (0, 1 if dy > 0 else -1) if horizontal else (1 if dx > 0 else -1, 0)
        inside = (x + step[0], y + step[1])
        if not (0 < inside[0] < cols - 1 and 0 < inside[1] < rows - 1) or inside in walls:
            raise SystemExit(f'{dst_name}: the lantern at {x},{y} faces {inside}, not a room')
    # every NPC, door and the stairs reachable on foot from the spawn
    blocked = {c for c, i in owner.items() if i not in PASSABLE and i not in STAIRS}
    seen, todo = {spawn}, deque([spawn])
    while todo:
        x, y = todo.popleft()
        for n in neighbours(x, y):
            if 0 <= n[0] < cols and 0 <= n[1] < rows and n not in seen and n not in blocked and n not in people:
                seen.add(n)
                todo.append(n)
    for c, who in people.items():
        if c != spawn and not any(n in seen for n in neighbours(*c)):
            raise SystemExit(f'{dst_name}: nobody can walk beside {who} at {c}')
    for c in openings:
        if c not in seen:
            raise SystemExit(f'{dst_name}: the door at {c} cannot be reached')
    for d in furniture:
        if d['id'] in STAIRS and (d['x'], d['y']) not in seen:
            raise SystemExit(f'{dst_name}: {d["id"]} at {d["x"]},{d["y"]} cannot be reached')
    # the NPCs at their new places, the old soldiers' sprites where an enemy's stood
    by_name = {s['name']: s for s in draft['neutralSpawns']}
    out = []
    for s in npcs:
        n = dict(by_name[s['name']])
        n['x'], n['y'] = s['x'], s['y']
        if s['name'] in sprites:
            n['classId'], portrait = sprites[s['name']]
            if portrait and n.get('dialog'):
                for line in n['dialog'].get('lines', []):
                    if line.get('speaker') == s['name']:
                        line['portrait'] = portrait
        out.append(n)
    missing = set(by_name) - {s['name'] for s in npcs}
    if missing:
        raise SystemExit(f'{dst_name}: NPCs left without a place: {sorted(missing)}')
    out += extra_npcs
    draft['neutralSpawns'] = out
    draft['playerSpawns'][0]['x'], draft['playerSpawns'][0]['y'] = spawn
    draft['decorations'] = decos
    file['serial'] = serial
    file['savedAt'] = int(time.time() * 1000)
    return file, path, len(seen), decos, walls


def talk(ident, speaker, portrait, text):
    return {'id': ident, 'startId': 'l1', 'lines': [{'id': 'l1', 'speaker': speaker, 'portrait': portrait, 'text': text, 'next': None}]}


def salao():
    cols, rows = 40, 28
    runs = [('h', 9, 0, 39),            # the north wing's wall
            ('v', 11, 1, 8), ('v', 20, 1, 8), ('v', 30, 1, 8),  # kitchen | store | healer | armoury
            ('v', 30, 10, 26),          # the east wing's wall
            ('h', 18, 31, 38)]          # back room | stairs room
    windows = [(4, 0), (8, 0), (15, 0), (25, 0), (34, 0), (0, 13), (0, 18), (0, 23), (6, 27), (24, 27), (28, 27), (39, 4), (39, 13), (39, 22), (35, 27)]
    doors = [(6, 9, 2), (17, 9, 2), (25, 9, 2), (34, 9, 2),  # the north wing's rooms (the leaf swings north, into the room)
             (15, 27, 2)]                                     # the front door (swings into the hall)
    doorways = [(30, 13, 1), (30, 22, 1)]                     # the back room, the stairs room
    sconces = [(2, 0), (13, 0), (18, 0), (22, 0), (28, 0), (32, 0), (37, 0),    # in the north rooms (on the outer wall)
               (3, 9), (9, 9), (12, 9), (21, 9), (28, 9),                     # the hall's north wall
               (0, 11), (0, 16), (0, 21), (0, 25),                            # its west wall
               (3, 27), (10, 27), (19, 27), (26, 27),                         # its south wall
               (30, 11), (30, 17), (30, 20), (30, 25),                        # the east wing's wall, the hall's side
               (39, 11), (39, 16), (39, 20), (39, 25)]                        # the east rooms (on the outer wall)
    f = []
    # -- the great hall --
    f += [p('ember-tavern-counter', 13, 12), p('ember-tavern-counter', 15, 12), p('ember-tavern-counter', 17, 12)]  # the bar, 13-18
    f += [p('ember-interior-drink-shelf', 13, 10), p('ember-interior-drink-shelf', 18, 10), p('ember-interior-barrel-rack-v02', 15, 10),
          p('inn-provisions-display', 19, 11), p('ember-tavern-bottles-jugs', 12, 11)]
    f += [p('ember-interior-stool', x, 13) for x in (13, 15, 17, 18)]  # the bar's stools
    f += [p('ember-tavern-stone-hearth', 2, 10), p('ember-interior-worn-rug', 2, 12), p('ember-interior-long-bench-v02', 2, 14),
          chair(5, 12, 'w'), chair(1, 13, 'e'), p('city-firewood-pile', 5, 10), p('ember-interior-fireplace-grate', 4, 11)]
    for y in (16, 20, 24):  # three rows of long tables: benches north and south, a chair at each end
        for x in (6, 13, 20):
            f += [p('ember-tavern-table-messhall', x, y), p('ember-interior-long-bench-v02', x - 1, y - 1), p('ember-interior-long-bench-v02', x - 1, y + 1),
                  chair(x - 2, y, 'e'), chair(x + 2, y, 'w')]
    f += [p('ember-tavern-table-small', 25, 15), chair(24, 15, 'e', True), chair(27, 15, 'w', True),
          p('ember-tavern-table-small', 25, 19), chair(24, 19, 'e', True), chair(27, 19, 'w', True),
          p('ember-tavern-table-small', 25, 23), chair(24, 23, 'e', True), chair(27, 23, 'w', True)]
    f += [p('light-candle', 22, 12), p('light-candle', 9, 13), p('light-candle', 27, 26), p('city-barrels', 28, 11), p('city-barrels', 1, 26),
          p('ember-props-barrel-v02', 29, 26), p('ember-interior-folding-screen', 22, 26)]
    # -- the kitchen (x 1-10) --
    f += [p('ember-interior-cauldron', 2, 2), p('ember-interior-kitchen-cabinet', 4, 1), p('ember-interior-pantry-shelf', 6, 1),
          p('ember-treasure-workbench-v02', 3, 5), p('ember-interior-iron-pot', 7, 4), p('ember-treasure-ceramic-jars', 9, 1),
          p('city-provisions', 10, 3), p('city-bucket', 1, 7), p('city-basket', 9, 7), p('light-candle', 5, 7), p('ember-props-sacks-v02', 1, 4)]
    # -- the store behind the bar (x 12-19) --
    f += [p('ember-interior-barrel-rack-v02', 12, 1), p('ember-interior-barrel-rack-v02', 16, 1), p('city-crate-stack', 19, 1), p('city-covered-crate', 19, 3),
          p('city-sack-pile', 12, 5), p('ember-props-crate-v02', 12, 7), p('ember-props-barrel-v02', 19, 7), p('ember-treasure-open-barrel', 18, 7),
          p('light-candle', 15, 4)]
    # -- the healer's room (x 21-29) --
    f += [p('inn-herb-shelf', 21, 1), p('ember-interior-bookshelf', 23, 1), p('ember-interior-simple-bed-v02', 27, 2), p('ember-interior-folding-screen', 27, 5),
          p('ember-tavern-table-small', 22, 5), chair(24, 5, 'w', True), p('ember-interior-table-candelabra', 21, 7), p('ember-treasure-ceramic-jars', 29, 7),
          p('ember-interior-worn-rug', 24, 3)]
    # -- Vargan's armoury (x 31-38) --
    f += [p('ember-treasure-weapon-rack-v02', 32, 1), p('ember-treasure-weapon-rack-v02', 35, 1), p('ember-treasure-shield-stand-v02', 37, 1),
          p('ember-treasure-workbench-v02', 32, 5), p('ember-treasure-iron-banded-chest', 38, 4), p('ember-props-crate-v02', 31, 7), p('light-candle', 36, 7)]
    # -- the back room (x 31-38, y 10-17) --
    f += [p('ember-tavern-table-small', 34, 13), chair(33, 13, 'e', True), chair(36, 13, 'w', True), p('ember-interior-cloth-curtain', 31, 10),
          p('ember-interior-bookshelf', 38, 10), p('ember-treasure-plain-chest', 38, 16), p('light-candle', 32, 16), p('ember-interior-worn-rug', 34, 15)]
    # -- the stairs room (x 31-38, y 19-26) --
    f += [p('inn-stairs-up', 35, 24), p('floor-connector', 35, 24, targetMapId='estalagem-andar-2'),
          p('ember-props-crate-v02', 31, 19), p('ember-props-barrel-v02', 38, 19), p('city-sack-pile', 31, 26), p('ember-props-sacks-v02', 38, 26),
          p('ember-interior-long-bench-v02', 33, 20)]
    npcs = [
        {'name': 'Brue', 'x': 16, 'y': 11},
        {'name': 'Velha da Sopa', 'x': 4, 'y': 3}, {'name': 'Padeira', 'x': 8, 'y': 5},
        {'name': 'Lenhador', 'x': 15, 'y': 6},
        {'name': 'Curandeiro Ancião', 'x': 25, 'y': 6}, {'name': 'Salazar', 'x': 22, 'y': 3},
        {'name': 'Vargan', 'x': 35, 'y': 4},
        {'name': 'Cliente Suspeito', 'x': 34, 'y': 12}, {'name': 'A Hóspede', 'x': 35, 'y': 15},
        {'name': 'O Mudo', 'x': 33, 'y': 23},
        {'name': 'Beberrão', 'x': 4, 'y': 13}, {'name': 'Camponês', 'x': 14, 'y': 21}, {'name': 'Aldeã', 'x': 14, 'y': 17},
        {'name': 'Louca da Vela', 'x': 28, 'y': 25},
        {'name': 'Neera', 'x': 7, 'y': 21}, {'name': 'Voss', 'x': 23, 'y': 19}, {'name': 'Aldric', 'x': 7, 'y': 17}, {'name': 'Malrec', 'x': 21, 'y': 21},
    ]
    extra = [
        {'name': 'Soldado de Passagem', 'classId': 'soldier', 'x': 21, 'y': 23, 'level': 3,
         'dialog': talk('npc-inn-soldado', 'Soldado de Passagem', 'soldier',
                        'Três dias de estrada e a primeira mesa que não balança. Se vier briga, eu durmo por cima dela.')},
        {'name': 'Piqueiro da Escolta', 'classId': 'basePikeman', 'x': 21, 'y': 25, 'level': 3,
         'dialog': talk('npc-inn-piqueiro', 'Piqueiro da Escolta', 'BasePikeman',
                        'A gente escolta carroça de sal até a ponte. Na volta, a carroça volta vazia e a gente volta com sede.')},
        {'name': 'Besteiro Veterano', 'classId': 'brigand', 'x': 18, 'y': 23, 'level': 3,
         'dialog': talk('npc-inn-besteiro', 'Besteiro Veterano', 'brigand',
                        'Besta não erra. Quem erra é o dedo, e o meu dedo já bebeu duas.')},
        {'name': 'Capitão da Companhia', 'classId': 'captain', 'x': 22, 'y': 22, 'level': 3,
         'dialog': talk('npc-inn-capitao', 'Capitão da Companhia', 'captain',
                        'Pago a rodada se você não perguntar quem nos contratou. Pergunta, e a rodada é sua.')},
    ]
    sprites = {'Vargan': ('swordsman', None), 'A Hóspede': ('mothKeeper', 'mothKeeper')}
    return build('estalagem007', 'estalagem008', 8, cols, rows, runs, windows, doors, doorways, sconces, f, npcs, (15, 25), extra, sprites)


def andar():
    cols, rows = 41, 27
    runs = [('h', 11, 1, 39), ('h', 15, 1, 39),                         # the corridor's walls (rows 12-14 between)
            ('v', 10, 1, 10), ('v', 20, 1, 10), ('v', 29, 1, 10),      # the rooms above
            ('v', 11, 16, 25), ('v', 21, 16, 25), ('v', 30, 16, 25)]   # the rooms below (the stairs room x 31-39)
    windows = [(5, 0), (15, 0), (24, 0), (33, 0), (37, 0), (5, 26), (16, 26), (25, 26), (35, 26), (0, 13), (40, 13)]
    doors = [(5, 11, 2), (15, 11, 2), (24, 11, 2), (34, 11, 2), (6, 15, 0), (16, 15, 0), (25, 15, 0)]
    doorways = [(35, 15, 0)]
    sconces = [(3, 0), (12, 0), (18, 0), (27, 0), (31, 0), (39, 0),     # the rooms above (on the outer wall)
               (3, 26), (9, 26), (13, 26), (19, 26), (23, 26), (28, 26), (38, 26),  # the rooms below
               (9, 11), (19, 11), (29, 11), (38, 11), (2, 15), (11, 15), (21, 15), (30, 15)]  # the corridor (the seven torches, inside)
    BED, STOOL, SHELF, RUG = 'ember-interior-simple-bed-v02', 'ember-interior-stool', 'ember-interior-bookshelf', 'ember-interior-worn-rug'
    CHEST, CHEST2, SCREEN = 'ember-treasure-plain-chest', 'ember-treasure-iron-banded-chest', 'ember-interior-folding-screen'
    f = []

    def bedroom(x0, x1, top, beds, extras):
        # beds against the outer wall, a stool and a candle beside each, a chest at the foot; the room's own extras
        out = []
        y = 1 if top else rows - 2
        for bx in beds:
            out += [p(BED, bx, y), p(STOOL, bx + 2, y), p('light-candle', bx - 1, y) if bx - 1 >= x0 else p('light-candle', bx + 3, y)]
            out.append(p(CHEST, bx, y + (1 if top else -1)))
        return out + extras

    f += bedroom(1, 9, True, [2], [p(RUG, 3, 6), p('ember-tavern-table-small', 6, 8), chair(8, 8, 'w', True), p('ember-interior-wooden-mug', 1, 9)])
    f += bedroom(11, 19, True, [12], [p(SHELF, 18, 1), p(RUG, 13, 6), p('ember-tavern-table-small', 16, 8), chair(15, 8, 'e', True)])
    f += bedroom(21, 28, True, [22], [p(SCREEN, 26, 2), p(RUG, 23, 6), p('ember-interior-kitchen-cabinet', 28, 9)])
    f += bedroom(30, 39, True, [31, 36], [p(SHELF, 30, 9), p(RUG, 33, 6), p('ember-tavern-table-small', 36, 8), chair(35, 8, 'e', True),
                                          p(CHEST2, 39, 9)])
    f += bedroom(1, 10, False, [2, 7], [p(RUG, 4, 20), p('ember-interior-cauldron', 10, 17), p('ember-treasure-ceramic-jars', 1, 17)])
    f += bedroom(12, 20, False, [13], [p(SCREEN, 18, 24), p(RUG, 14, 20), p('ember-tavern-table-small', 16, 17), chair(18, 17, 'w', True)])
    f += bedroom(22, 29, False, [23], [p(SHELF, 29, 17), p(RUG, 24, 20), p(CHEST2, 22, 17)])
    # the stairs room (x 31-39, y 16-25): the inn's linen store
    f += [p('inn-stairs-down', 36, 23, mirrorX=True), p('floor-connector', 36, 23, targetMapId='estalagem', returnConnector=True),
          p('city-sack-pile', 31, 16), p('city-sack-pile', 31, 17), p('city-crate-stack', 39, 16), p('city-covered-crate', 39, 17),
          p('ember-props-barrel-v02', 31, 25), p('city-basket', 33, 25), p('city-bucket', 39, 25), p('ember-props-crate-v02', 31, 21)]
    # the corridor (rows 12-14): benches against the walls, a brazier at each end, barrels
    f += [p('light-brazier-bowl', 1, 13), p('light-brazier-bowl', 39, 13), p('ember-interior-long-bench-v02', 12, 12),
          p('ember-interior-long-bench-v02', 26, 14), p('ember-props-barrel-v02', 1, 12), p('ember-props-barrel-v02', 39, 14),
          p('city-firewood-pile', 1, 14), p('city-barrels', 39, 12)]
    npcs = [
        {'name': 'Tomas, o Febril', 'x': 5, 'y': 3}, {'name': 'Dorminhoco', 'x': 14, 'y': 3}, {'name': 'Nara', 'x': 17, 'y': 6},
        {'name': 'Dona Anselma', 'x': 24, 'y': 4}, {'name': 'Garrick, o Caçador', 'x': 34, 'y': 3}, {'name': 'Erasmo', 'x': 31, 'y': 7},
        {'name': 'Berta da Cozinha', 'x': 5, 'y': 22}, {'name': 'Nessa', 'x': 8, 'y': 19},
        {'name': 'O Peregrino', 'x': 15, 'y': 22}, {'name': 'Ada', 'x': 13, 'y': 18},
        {'name': 'Homem de Capuz', 'x': 26, 'y': 22},
        {'name': 'Isa, a Camareira', 'x': 9, 'y': 13}, {'name': 'Guarda de Folga', 'x': 33, 'y': 13},
    ]
    sprites = {'Guarda de Folga': ('baseCrossbowman', 'BaseCrossbowman')}
    return build('estalagem-andar-2006', 'estalagem-andar-2007', 7, cols, rows, runs, windows, doors, doorways, sconces, f, npcs, (4, 13), [], sprites)


def show(name, decos, walls, cols, rows, file):
    grid = [['.'] * cols for _ in range(rows)]
    for (x, y) in walls:
        grid[y][x] = '#'
    marks = {WINDOW: 'W', DOOR: 'D', DOORWAY: 'O', SCONCE: 'L'}
    for d in decos:
        if d['id'] in marks:
            grid[d['y']][d['x']] = marks[d['id']]
        elif d['id'] not in ARCH:
            for (x, y) in cells_of(d):
                if grid[y][x] == '.':
                    grid[y][x] = 'c' if d['id'] == 'light-candle' else 'S' if d['id'] in STAIRS else 'o'
    for s in file['draft']['neutralSpawns']:
        grid[s['y']][s['x']] = 'N'
    k = file['draft']['playerSpawns'][0]
    grid[k['y']][k['x']] = 'K'
    print(f'== {name}')
    for y, row in enumerate(grid):
        print(f'{y:2d} ' + ''.join(row))


def main():
    made = []
    for make in (salao, andar):
        file, path, reach, decos, walls = make()
        made.append((file, path))
        show(os.path.basename(path), decos, walls, file['draft']['cols'], file['draft']['rows'], file)
        print(f'{os.path.relpath(path, ROOT)}: {len(decos)} pieces, {reach} hexes reachable on foot:',
              dict(Counter(d['id'] for d in decos).most_common()))
    for file, path in made:
        with open(path, 'w', encoding='utf-8') as f:
            json.dump(file, f, ensure_ascii=False)


if __name__ == '__main__':
    main()
