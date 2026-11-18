"""Torre de Vigia v3: every floor on the ground floor's outline (watchtower-gate-floor010, the user's approved Térreo, never touched).

  python -I tools/scripts/maps/make_watchtower_v3.py

The user, 2026-11-17: the tower is the wide octagon of the Térreo, not a round one; "change everything else"; no zombies in a
watchtower; the questline of saving Aldric at the bottom of the dungeon belongs there.

Writes new versions (the highest serial of each floor is the one the game opens; the Térreo stays pinned at 010 by active.json):
  watchtower-barracks007  1º Andar, Caserna e Arsenal    (barracks005's rooms, the tower's own garrison)
  watchtower-command006   2º Andar, Câmara do Comandante (command004's rooms, the tower's own garrison)
  watchtower-beacon007    3º Andar, O Farol Apagado      (beacon005's rooms, the tower's own garrison)
  watchtower-undercroft004 Subsolo 1, Salões das Fundações: the Térreo's outline dug out below it
  watchtower-prison007     Subsolo 2, A Prisão Silenciosa: the same outline; Aldric locked in the far cell
and registers `ember-arch-watchtower-door-locked` (the tower's door with a lock, as the Mosteiro / Farol locked doors).
"""
import copy
import json
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
MAPS = ROOT / 'content/maps/ember'
ARCH = ROOT / 'content/models/architecture.json'
CATALOG = ROOT / 'content/data/editor-catalog.json'

WALL = 'ember-arch-watchtower-wall'
TORCH = 'ember-arch-watchtower-wall-torch'
DOOR = 'ember-arch-watchtower-door'
LOCKED = 'ember-arch-watchtower-door-locked'
WALLISH = {WALL, TORCH, 'ember-arch-watchtower-window', 'ember-arch-watchtower-slit'}


def load(stem):
    return json.loads((MAPS / f'{stem}.json').read_text(encoding='utf-8'))


def save(stem, serial, draft):
    out = {'serial': serial, 'savedAt': int(time.time() * 1000), 'draft': draft}
    path = MAPS / f'{stem}.json'
    if path.exists():
        raise SystemExit(f'{path.name} already exists: not overwritten')
    path.write_text(json.dumps(out, ensure_ascii=False, indent=1) + '\n', encoding='utf-8')
    print('wrote', path.name, len(draft['decorations']), 'pieces,', len(draft['enemySpawns']), 'enemies')


# --- the locked tower door ----------------------------------------------------------------------------------------------------------
def register_locked_door():
    arch = json.loads(ARCH.read_text(encoding='utf-8'))
    if not any(p['id'] == LOCKED for p in arch['pieces']):
        door = next(p for p in arch['pieces'] if p['id'] == DOOR)
        q = copy.deepcopy(door)
        q.update({'id': LOCKED, 'name': 'Porta trancada da Torre de Vigia 3D · 2 hexes', 'doorStyle': 'mosteiro-locked', 'locked': True})
        at = arch['pieces'].index(door) + 1
        arch['pieces'].insert(at, q)
        text = ARCH.read_text(encoding='utf-8')
        ARCH.write_text(json.dumps(arch, ensure_ascii=False, indent=2) + ('\n' if text.endswith('\n') else ''), encoding='utf-8')
    catalog = json.loads(CATALOG.read_text(encoding='utf-8'))
    editor = catalog['editor']
    if LOCKED not in editor['architectureDefs']:
        for table in (catalog['decorations'], editor['decoMeta'], editor['decorationInfo'], editor['architectureDefs']):
            e = copy.deepcopy(table[DOOR])
            if 'name' in e:
                e['name'] = 'Porta trancada da Torre de Vigia 3D · 2 hexes'
            if table is editor['decoMeta']:
                e['doorStyle'] = 'mosteiro-locked'
            if table is editor['architectureDefs']:
                e['locked'] = True
            table[LOCKED] = e
        order = editor['architectureOrder']
        order.insert(order.index(DOOR) + 1, LOCKED)
        CATALOG.write_text(json.dumps(catalog, ensure_ascii=False, separators=(',', ':')), encoding='utf-8')


# --- the Térreo's outline -----------------------------------------------------------------------------------------------------------
GROUND = load('watchtower-gate-floor010')['draft']
C, R = GROUND['cols'], GROUND['rows']
VOID = {(i % C, i // C) for i, t in enumerate(GROUND['tiles']) if t == 'void'}


def near_void(x, y):
    return any((x + dx, y + dy) in VOID or not (0 <= x + dx < C and 0 <= y + dy < R) for dx in (-1, 0, 1) for dy in (-1, 0, 1))


def outer_ring():
    """The Térreo's outer wall, every piece where it stands; the gate's four hexes walled up (below ground there is no way out)."""
    ring = {}
    for d in GROUND['decorations']:
        if d['id'] in WALLISH and near_void(d['x'], d['y']):
            ring[(d['x'], d['y'])] = {'id': TORCH if d['id'] == TORCH else WALL, 'x': d['x'], 'y': d['y'], 'rot': 0, 'wallOrientation': 'horizontal'}
    gate = next(d for d in GROUND['decorations'] if d['id'] == 'ember-arch-watchtower-gate')
    for x in range(gate['x'], gate['x'] + 4):
        ring[(x, gate['y'])] = {'id': WALL, 'x': x, 'y': gate['y'], 'rot': 0, 'wallOrientation': 'horizontal'}
    return ring


class Floor:
    def __init__(self, ring):
        self.pieces = dict(ring)  # (x, y) -> wall piece
        self.props = []
        self.taken = set(ring)

    def inside(self, x, y):
        return 0 <= x < C and 0 <= y < R and (x, y) not in VOID and (x, y) not in self.pieces

    def wall(self, x, y, piece=WALL):
        if (x, y) in VOID or (x, y) in self.pieces:
            return
        self.pieces[(x, y)] = {'id': piece, 'x': x, 'y': y, 'rot': 0, 'wallOrientation': 'horizontal'}
        self.taken.add((x, y))

    def hwall(self, y, gaps=(), doors=()):
        """A wall across the whole row from the outer wall to the outer wall; `gaps` stay open, `doors` = (x, id) two hexes each."""
        held = set(gaps)
        for x, _ in doors:
            held |= {x, x + 1}
        for x in range(C):
            if (x, y) in VOID or (x, y) in self.pieces or x in held:
                continue
            if self._between_ring(x, y):
                self.wall(x, y)
        for x, piece in doors:
            self.pieces[(x, y)] = {'id': piece, 'x': x, 'y': y, 'rot': 0, 'wallOrientation': 'horizontal'}
            self.taken |= {(x, y), (x + 1, y)}

    def _between_ring(self, x, y):
        left = any(((k, y) in self.pieces and k < x) for k in range(x))
        right = any(((k, y) in self.pieces and k > x) for k in range(x + 1, C))
        return left and right

    def vwall(self, x, y0, y1, gaps=()):
        for y in range(y0, y1 + 1):
            if y not in gaps:
                self.wall(x, y)

    def prop(self, pid, x, y, block=True):
        if not self.inside(x, y) or (x, y) in self.taken:
            raise SystemExit(f'{pid} at {x},{y}: not a free floor hex')
        p = {'id': pid, 'x': x, 'y': y}
        if block:
            p['blocksPath'] = True
        self.props.append(p)
        self.taken.add((x, y))

    def stairs(self, x, y, up, target, back):
        self.props.append({'id': 'stone-stairs-up-001' if up else 'stone-stairs-down-001', 'x': x, 'y': y, 'targetMapId': target,
                           'connectorDirection': 'up' if up else 'down', **({'returnConnector': True} if back else {})})
        self.pieces[(x, y)] = {'id': 'ember-arch-watchtower-stairs-up' if up else 'ember-arch-watchtower-stairs-down', 'x': x, 'y': y,
                               'rot': 3, 'wallOrientation': 'vertical'}
        self.taken |= {(x, y), (x + 1, y)}

    def free(self, x, y):
        if not self.inside(x, y) or (x, y) in self.taken:
            raise SystemExit(f'unit at {x},{y}: not a free floor hex')
        self.taken.add((x, y))
        return x, y

    def decorations(self):
        return list(self.pieces.values()) + self.props


def units(floor, side, spec, level):
    out = []
    for name, cls, x, y in spec:
        floor.free(x, y)
        out.append({'name': name, 'classId': cls, 'x': x, 'y': y, 'level': level})
    return out


# --- the tower's garrison (no undead: soldiers, officers, the cult that put the beacon out) -----------------------------------------
GARRISON = {
    'barracks': [('Soldado da Torre', 'soldier'), ('Piqueiro da Torre', 'pikeman'), ('Besteiro da Torre', 'baseCrossbowman'),
                 ('Soldado da Torre', 'soldier'), ('Lanceiro da Torre', 'lancer'), ('Sentinela da Torre', 'sentinel')],
    'command': [('Capitão da Torre', 'captain'), ('Cavaleiro da Guarda', 'heavyKnight'), ('Sentinela da Torre', 'sentinel'),
                ('Besteiro da Torre', 'baseCrossbowman'), ('Soldado da Torre', 'soldier'), ('Piqueiro da Torre', 'pikeman')],
    'beacon': [('Arqueira da Torre', 'archer'), ('Besteiro da Torre', 'baseCrossbowman'), ('Cultista Negro', 'blackCultist'),
               ('Sentinela da Torre', 'sentinel'), ('Arqueira da Torre', 'archer'), ('Lanceiro da Torre', 'lancer')],
}


def upper(stem, src, serial, key):
    d = copy.deepcopy(load(src)['draft'])
    for spawn, (name, cls) in zip(d['enemySpawns'], GARRISON[key]):
        spawn['name'], spawn['classId'] = name, cls
    save(stem, serial, d)


BASEMENT = {k: GROUND[k] for k in ('cols', 'rows', 'tiles', 'tileVariants', 'tileRots')}


def basement(src, **over):
    d = {k: v for k, v in load(src)['draft'].items()}
    d.update(copy.deepcopy(BASEMENT))
    d.update(over)
    d['aboveGround'] = False
    d['neutralSpawns'] = d.get('neutralSpawns') or []
    return d


def heroes(template, spots):
    return [dict(h, x=x, y=y) for h, (x, y) in zip(template, spots)]


def undercroft():
    f = Floor(outer_ring())
    # the way up sits under the Térreo's way down; the way down to the prison in the south-east corner
    f.stairs(10, 29, True, 'watchtower-gate-floor', True)
    f.stairs(28, 29, False, 'watchtower-prison', False)
    # the basalt sanctuary to the north, behind a wall with an arch in the middle
    f.hwall(9, gaps=(19, 20))
    f.prop('ember-dungeon-altar', 19, 5)
    f.prop('ember-fire-v02-candle-shrine', 16, 6)
    f.prop('ember-fire-v02-candle-shrine', 23, 6)
    f.prop('light-brazier-bowl', 13, 7)
    f.prop('light-brazier-bowl', 26, 7)
    # the storeroom to the west and the cistern to the east, each behind a wall with an opening
    f.vwall(9, 11, 23, gaps=(16, 17))
    f.vwall(31, 11, 23, gaps=(16, 17))
    for pid, x, y in (('ember-props-barrel-v02', 6, 12), ('ember-props-barrel-v02', 7, 13), ('ember-interior-barrel-rack-v02', 5, 15),
                      ('ember-polyhaven-wine-barrel-01', 6, 19), ('city-crate-stack', 6, 21), ('ember-props-sacks-v02', 7, 22),
                      ('ember-props-crate-v02', 7, 20)):
        f.prop(pid, x, y)
    f.prop('city-well', 34, 17)
    f.prop('light-brazier-bowl', 33, 13)
    f.prop('light-brazier-bowl', 33, 21)
    f.prop('ember-props-barrel-v02', 35, 14)
    # the arcades: four rows of three pillars carrying the Térreo's floor
    for x in (13, 17, 22, 26):
        for y in (13, 17, 21):
            f.prop('ember-dungeon-pillar-v02', x, y)
    f.prop('light-brazier-bowl', 19, 24)
    f.prop('ember-props-crate-v02', 31, 26)
    src = load('watchtower-undercroft003')['draft']
    party = heroes(src['playerSpawns'], [f.free(*p) for p in ((12, 27), (13, 28), (11, 26), (14, 26))])
    foes = units(f, 'enemy', [('Cultista Negro', 'blackCultist', 19, 7), ('Cultista Branco', 'cultistV2', 15, 15),
                              ('Soldado da Torre', 'soldier', 24, 19), ('Sentinela da Torre', 'sentinel', 7, 16),
                              ('Besteiro da Torre', 'baseCrossbowman', 34, 15), ('Feiticeiro', 'cultist', 20, 13)], 10)
    d = basement('watchtower-undercroft003', decorations=f.decorations(), playerSpawns=party, enemySpawns=foes,
                 briefing='Sob o Térreo, as fundações repetem a planta da torre: arcadas de pilares no salão, depósito a oeste, '
                          'cisterna a leste e um santuário de basalto ao norte. Quem apagou o farol reza aqui embaixo.',
                 objective='Explore o andar.')
    save('watchtower-undercroft004', 4, d)


def prison():
    f = Floor(outer_ring())
    f.stairs(28, 29, True, 'watchtower-undercroft', True)
    # north cells: a row of cells behind the north wall, one door each; Aldric's is the far one (north-west), locked
    f.hwall(11, doors=((8, LOCKED), (14, DOOR), (19, DOOR), (24, DOOR), (29, DOOR)))
    for x in (12, 17, 22, 27):
        f.vwall(x, 3, 10)
    # the long corridor (rows 12-16), then the south cells and the interrogation room
    f.hwall(17, gaps=(29, 30, 31, 32), doors=((6, DOOR), (10, DOOR), (14, DOOR), (18, DOOR), (22, DOOR)))
    for x in (9, 13, 17, 21, 25):
        f.vwall(x, 18, 22)
    f.hwall(23, gaps=(29, 30, 31, 32))
    # below: the ossuary to the west, the guard room with the stairs to the east
    f.vwall(19, 24, 31, gaps=(27, 28))
    for pid, x, y in (('dungeon-ossuary', 9, 27), ('dungeon-bones', 12, 25), ('ember-dungeon-bone-pile', 7, 25),
                      ('ember-dungeon-bone-pile', 14, 29), ('ember-dungeon-bone-pile', 16, 26)):
        f.prop(pid, x, y)
    f.prop('ember-tavern-table-small', 24, 26)
    f.prop('light-brazier-bowl', 22, 29)
    f.prop('light-brazier-bowl', 33, 26)
    # the interrogation room
    f.prop('ember-dungeon-iron-cage', 34, 19)
    f.prop('dungeon-hanging-cage', 27, 21)
    f.prop('ember-tavern-table', 31, 22)
    f.prop('ember-dungeon-chains', 35, 21)
    f.prop('light-brazier-bowl', 33, 15)
    f.prop('light-brazier-bowl', 9, 15)
    # the cells' chains and straw-less floors
    for x, y in ((15, 5), (24, 6), (6, 20), (23, 20)):
        f.prop('ember-dungeon-chains', x, y)
    f.prop('ember-dungeon-bone-pile', 29, 7)
    f.prop('ember-dungeon-chains', 11, 6)
    src = load('watchtower-prison003')['draft']
    party = heroes(load('watchtower-prison006')['draft']['playerSpawns'], [f.free(*p) for p in ((27, 27), (26, 28), (25, 27), (26, 26))])
    foes = units(f, 'enemy', [('Carcereiro', 'captain', 31, 20), ('Cultista Negro', 'blackCultist', 24, 14),
                              ('Soldado da Torre', 'soldier', 15, 14), ('Sentinela da Torre', 'sentinel', 8, 14),
                              ('Feiticeiro', 'cultist', 34, 13), ('Piqueiro da Torre', 'pikeman', 11, 29)], 10)
    aldric = copy.deepcopy(src['neutralSpawns'][0])
    aldric['x'], aldric['y'] = f.free(10, 8)
    aldric['dialogRequiresOpenDoor'] = {'x': 8, 'y': 11}
    d = basement('watchtower-prison006', decorations=f.decorations(), playerSpawns=party, enemySpawns=foes, neutralSpawns=[aldric],
                 briefing='O fundo da torre: um corredor de celas dos dois lados, a sala de interrogatório a leste e o ossário a oeste. '
                          'Na cela mais distante, trancada, alguém ainda está vivo.',
                 objective='Encontre o oficial cativo e abra a cela dele.')
    save('watchtower-prison007', 7, d)


def main():
    register_locked_door()
    upper('watchtower-barracks007', 'watchtower-barracks005', 7, 'barracks')
    upper('watchtower-command006', 'watchtower-command004', 6, 'command')
    upper('watchtower-beacon007', 'watchtower-beacon005', 7, 'beacon')
    undercroft()
    prison()


if __name__ == '__main__':
    main()
