"""The Interlude's city, 25 years before it became the Ruins (docs/INTERLUDE.md § Step 3): the extra-large walled city the historical
party (Isolde, Salazar Jovem, Selene) walks, and the same city as the chapter's first mission. Built from the game's own pieces (the
castle wall kit, the temple wall kit, the house cards, the city / structures / props 3D kits, the 2D decorations); the real art of
the city (panorama, ground photos) is the user's and comes later.

  python -I tools/scripts/maps/make_interlude_city.py

Writes (never overwrites: the next serial of each id; the campaign plays the highest):
  content/maps/ember/interlude-city<NNN>.json          60 x 45, free roam: the three at the south gate, townsfolk about, no foes;
                                                      the north gate's passage (floor connector) leads to the mission
  content/maps/ember/interlude-placeholder<NNN>.json   the same city as a rout mission: the party inside the north gate, the
                                                      enemies holding the avenue, the plaza and the temple
  content/interlude/city-plan.json                     the geography (walls, gates, streets, plaza, districts, landmarks): the
                                                      present-day Ruins are laid over the same plan later

The plan (odd-r hexes, x = column, y = row):
  walls     a battlemented ring (castle-3d-battlement) on column 1 / 58 and row 1 / 43, square towers at the corners and the midpoints,
            two open gates: south (29-30, 43: the party's entry) and north (29-30, 1: the passage out)
  streets   "nave" stone: the ring road inside the walls (col 3 / 56, row 3 / 41), the N-S avenue (cols 28-31), the E-W avenue
            (rows 21-23), side streets at cols 13-14 / 44-45 and rows 11-12 / 32-33
  plaza     cols 25-34, rows 18-26: the great fountain, benches, lamps, the notice board, two stalls
  NW        the market: a stone square (cols 15-27, rows 13-20) with stalls, carts, barrels and the well; the forge and storehouses
  NE        the noble quarter: the Salazar family's house (the biggest card) facing a garden with its fountain, knight statues at its
            door, trees, benches; the other fine houses
  SW        the poor quarter: stone huts packed close, firewood, troughs, sacks, a broken wheel
  SE        the temple: the twin spires inside an enclosure of temple walls (temple-3d-plain) opening west onto the avenue, stone
            columns at the mouth, penitent statues, ritual braziers, a stone floor
  lamps     along both avenues every five hexes
Blocking: only architecture (the walls, the temple's walls) and barricades block a hex (the user, 2026-10-25); houses and props do not.
Checks: every spawn, every foe and the passage are reachable from the party's spawn; no spawn stands inside a wall or on a prop.
"""
import json, os, random, sys, time
from collections import deque

sys.stdout.reconfigure(encoding='utf-8')
ROOT = 'C:/EmberEngine'
COLS, ROWS = 60, 45
rng = random.Random(20261025)
catalog = json.load(open(f'{ROOT}/content/data/editor-catalog.json', encoding='utf-8'))['decorations']
rules = json.load(open(f'{ROOT}/content/data/battle-rules.json', encoding='utf-8'))
base = json.load(open(f'{ROOT}/content/maps/ember/cruzadotest001.json', encoding='utf-8'))['draft']  # lighting / mist / music settings

tiles = ['plains'] * (COLS * ROWS)
def at(c, r): return r * COLS + c
def put(c, r, t):
    if 0 <= c < COLS and 0 <= r < ROWS: tiles[at(c, r)] = t
def rect(c0, r0, c1, r1, t):
    for r in range(r0, r1 + 1):
        for c in range(c0, c1 + 1): put(c, r, t)
def tile(c, r): return tiles[at(c, r)]

plan = {
  'format': 'ember-city-plan-1', 'cols': COLS, 'rows': ROWS,
  'walls': {'west': 1, 'east': 58, 'north': 1, 'south': 43, 'southGate': [[29, 43], [30, 43]], 'northGate': [[29, 1], [30, 1]], 'passage': [29, 2]},
  'streets': {'ringRoad': {'west': 3, 'east': 56, 'north': 3, 'south': 41}, 'avenueNS': [28, 31], 'avenueEW': [21, 23],
              'sideStreetsCols': [[13, 14], [44, 45]], 'sideStreetsRows': [[11, 12], [32, 33]]},
  'plaza': [25, 18, 34, 26],
  'districts': {'market': [4, 4, 27, 20], 'noble': [32, 4, 55, 20], 'poor': [4, 24, 27, 40], 'temple': [32, 24, 55, 40]},
  'landmarks': {'plazaFountain': [29, 22], 'marketSquare': [15, 13, 27, 20], 'well': [20, 16], 'forge': [7, 8],
                'salazarHouse': [46, 7], 'salazarGarden': [45, 9, 51, 12], 'temple': [36, 26, 52, 39], 'templeSpires': [43, 31]},
}

# --- streets, plaza, squares ----------------------------------------------------------------------------------------------------
rect(3, 3, 56, 3, 'nave'); rect(3, 41, 56, 41, 'nave'); rect(3, 3, 3, 41, 'nave'); rect(56, 3, 56, 41, 'nave')
rect(28, 2, 31, 42, 'nave'); rect(2, 21, 57, 23, 'nave')
rect(13, 3, 14, 41, 'nave'); rect(44, 3, 45, 41, 'nave'); rect(3, 11, 56, 12, 'nave'); rect(3, 32, 56, 33, 'nave')
rect(25, 18, 34, 26, 'nave')                     # the plaza
rect(15, 13, 27, 20, 'nave')                     # the market square
rect(36, 26, 52, 39, 'nave')                     # the temple's floor
rect(45, 9, 51, 12, 'woods')                     # the Salazar garden
# the walls' own cells and the outside ring: void beyond the walls (the map ends there)
rect(0, 0, 59, 0, 'void'); rect(0, 44, 59, 44, 'void'); rect(0, 0, 0, 44, 'void'); rect(59, 0, 59, 44, 'void')

decorations, used = [], {}
# hexes kept clear for the people (townsfolk, the party, the foes, the passage): no prop lands on them
reserved = {(18, 15), (24, 17), (31, 19), (8, 30), (20, 36), (6, 26), (33, 24), (48, 13), (29, 41), (28, 42), (31, 41), (29, 4), (28, 5), (31, 4),
            (27, 15), (32, 15), (29, 20), (27, 24), (32, 24), (38, 33), (44, 33), (29, 2)}
def cells_of(id, c, r):
    fp = catalog[id].get('footprint') or [{'dx': 0, 'dy': 0}]
    return [(c + f.get('dx', 0), r + f.get('dy', 0)) for f in fp]
def free(id, c, r, margin=0):
    for x, y in cells_of(id, c, r):
        if not (1 <= x <= 58 and 1 <= y <= 43) or (x, y) in used or (x, y) in reserved: return False
        if margin:
            for dx in range(-margin, margin + 1):
                for dy in range(-margin, margin + 1):
                    if (x + dx, y + dy) in used: return False
    return True
def place(id, c, r, rot=None, margin=0, **extra):
    if id not in catalog: raise SystemExit('unknown decoration ' + id)
    if not free(id, c, r, margin): return False
    d = {'id': id, 'x': c, 'y': r}
    if rot is not None: d['rot'] = rot
    d.update(extra)
    decorations.append(d)
    for x in cells_of(id, c, r): used[x] = id
    return True
def on_street(c, r): return tile(c, r) == 'nave' or tile(c, r) == 'void'
def house_fits(id, c, r, margin=1):
    cells = cells_of(id, c, r)
    for x, y in cells:
        if not (2 <= x <= 57 and 2 <= y <= 42) or on_street(x, y) or tile(x, y) == 'woods': return False
    return free(id, c, r, margin)

# --- the walls ------------------------------------------------------------------------------------------------------------------
GATES = {(29, 43), (30, 43), (29, 1), (30, 1)}
TOWERS = {(1, 1), (58, 1), (1, 43), (58, 43), (1, 22), (58, 22), (15, 1), (44, 1), (15, 43), (44, 43)}
for c in range(1, 59):
    for r in (1, 43):
        if (c, r) in GATES: place('castle-3d-gate-open', c, r)
        elif (c, r) in TOWERS: place('castle-3d-thick-tower', c, r)
        else: place('castle-3d-battlement', c, r)
for r in range(2, 43):
    for c in (1, 58):
        place('castle-3d-thick-tower' if (c, r) in TOWERS else 'castle-3d-battlement', c, r)
gate_def = rules['decorations'].get('castle-3d-gate-open', {})
assert gate_def.get('model3d') not in ('wall', 'door', 'secretDoor'), 'the open gate must not block'

# --- the plaza ------------------------------------------------------------------------------------------------------------------
place('wilds-fountain', 29, 22)
for c, r in ((26, 19), (33, 19), (26, 25), (33, 25)): place('ember-city-bench', c, r)
for c, r in ((25, 18), (34, 18), (25, 26), (34, 26)): place('ember-city-street-lamp', c, r)
place('city-notice-board-2', 27, 18); place('city-market-stall-2', 32, 25); place('ember-structures-vendor-cart', 26, 23)

# --- the market (NW) --------------------------------------------------------------------------------------------------------------
place('ember-city-well', 20, 16)
for c, r in ((16, 14), (19, 14), (23, 14), (16, 19), (19, 19), (23, 19), (25, 15), (25, 18)): place('ember-city-market-stall', c, r)
place('ember-structures-vendor-cart', 22, 17); place('city-covered-wagon', 17, 17)
for c, r in ((15, 13), (26, 13), (15, 20), (26, 20)): place('ember-structures-street-brazier', c, r)
for i in range(10):
    c, r = rng.randrange(16, 27), rng.randrange(14, 20)
    place(rng.choice(['ember-props-barrel', 'ember-props-crate', 'ember-props-sacks', 'ember-treasure-wicker-baskets', 'ember-treasure-ceramic-jars']), c, r)
place('city-forge', 7, 8); place('ember-treasure-weapon-rack', 9, 9); place('ember-props-barrel', 6, 9)
for c, r in ((6, 5), (10, 5), (17, 6), (21, 6), (25, 6), (6, 16), (9, 17)):
    place(rng.choice(['farmland-photographic-014-store-house', 'small-house']), c, r) if house_fits('farmland-photographic-014-store-house', c, r) else place('small-house', c, r) if house_fits('small-house', c, r) else None

# --- the noble quarter (NE) -------------------------------------------------------------------------------------------------------
place('farmland-photographic-013-inn-house', 46, 7)        # the Salazar family's house (the biggest card today; the user's art later)
place('stone-fountain', 48, 11)
place('wilds-knight-statue', 45, 8); place('wilds-knight-statue', 50, 8)
for c, r in ((45, 10), (51, 10), (47, 12), (50, 12)): place('tree-3d-broadleaf', c, r)
place('ember-city-bench', 46, 11); place('ember-city-bench', 50, 11)
for c, r in ((46, 13), (51, 13), (45, 6), (51, 6)): place('ember-city-street-lamp', c, r)
for c, r in ((34, 6), (38, 6), (34, 15), (38, 15), (47, 16), (52, 16), (53, 5), (41, 9)):
    for id in ('farmland-photographic-013-tavern-house', 'farmland-photographic-014-villagers-house', 'small-house'):
        if house_fits(id, c, r): place(id, c, r); break
place('wilds-ivy-statue', 36, 10); place('ember-structures-hanging-sign-post', 35, 14)

# --- the poor quarter (SW) --------------------------------------------------------------------------------------------------------
for r in range(26, 41, 4):
    for c in range(5, 27, 4):
        if house_fits('stone-hut', c, r, margin=0): place('stone-hut', c, r)
for i in range(14):
    c, r = rng.randrange(4, 27), rng.randrange(24, 41)
    if not on_street(c, r): place(rng.choice(['ember-extra-firewood-stack', 'ember-structures-animal-trough', 'ember-props-sacks', 'ember-extra-broken-cart-wheel', 'ember-extra-wooden-bucket', 'ember-props-handcart']), c, r)
place('city-stone-trough-fountain', 9, 37)

# --- the temple (SE) --------------------------------------------------------------------------------------------------------------
for c in range(36, 53):
    place('temple-3d-plain', c, 26); place('temple-3d-plain', c, 39)
for r in range(27, 39):
    place('temple-3d-plain', 52, r)
    if r not in (32, 33): place('temple-3d-plain', 36, r)   # the west mouth, onto the side street (cols 44-45 run inside; the avenue is west)
place('twin-spires', 43, 31)
for c, r in ((37, 31), (37, 34)): put(c, r, 'column')          # stone columns at the mouth (impassable, a line of sight break)
place('dungeon-penitent-statue', 40, 29); place('dungeon-penitent-statue', 40, 36); place('dungeon-penitent-statue', 48, 29); place('dungeon-penitent-statue', 48, 36)
for c, r in ((39, 32), (39, 33), (47, 32), (47, 33)): place('ember-crypt-ritual-brazier', c, r)
for c, r in ((42, 28), (46, 28), (42, 37), (46, 37)): place('ember-dungeon-pillar', c, r)
for c, r in ((33, 27), (33, 37), (54, 27), (54, 37)):
    for id in ('small-house', 'farmland-photographic-013-apothecary-house'):
        if house_fits(id, c, r): place(id, c, r); break

# --- lamps along the avenues -------------------------------------------------------------------------------------------------------
for r in range(5, 41, 5):
    place('ember-city-street-lamp', 27, r); place('ember-city-street-lamp', 32, r)
for c in range(5, 56, 5):
    place('ember-city-street-lamp', c, 20); place('ember-city-street-lamp', c, 24)

# --- the passage out (the north gate) and the party ---------------------------------------------------------------------------------
PASSAGE = (29, 2)
decorations.append({'id': 'floor-connector', 'x': PASSAGE[0], 'y': PASSAGE[1], 'targetMapId': 'interlude-placeholder'})
party_city = [{'name': 'Isolde', 'classId': 'conjurer', 'x': 29, 'y': 41, 'level': 1}, {'name': 'Salazar Jovem', 'classId': 'crusader', 'x': 28, 'y': 42, 'level': 5},
              {'name': 'Selene', 'classId': 'conjurer', 'x': 31, 'y': 41, 'level': 1}]
# townsfolk: neutrals about the city (no conversation yet: they stand; the user's people come later)
folk = [('Padeira', 'breadLady', 18, 15), ('Mercador', 'travelingMerchant', 24, 17), ('Aldeã', 'villagerF1', 31, 19), ('Camponês', 'peasant1', 8, 30),
        ('Lenhador', 'woodsman', 20, 36), ('Carvoeiro', 'charcoalBurner', 6, 26), ('Bêbado', 'beberrao', 33, 24), ('Aldeã', 'villagerF1', 48, 13)]
neutrals = [{'name': n, 'classId': c, 'x': x, 'y': y} for n, c, x, y in folk]

# --- checks ---------------------------------------------------------------------------------------------------------------------------
PASSABLE = {'plains', 'woods', 'ruins', 'nave', 'hill', 'snow'}
ARCH_BLOCK = {(x, y) for (x, y), id in used.items() if rules['decorations'].get(id, {}).get('model3d') in ('wall', 'door', 'secretDoor')}
def neighbors(c, r):
    for dc, dr in ([(1, 0), (-1, 0), (1, -1), (0, -1), (1, 1), (0, 1)] if r & 1 else [(1, 0), (-1, 0), (0, -1), (-1, -1), (0, 1), (-1, 1)]):
        n = (c + dc, r + dr)
        if 0 <= n[0] < COLS and 0 <= n[1] < ROWS: yield n
def reach(start):
    seen = {start}; q = deque([start])
    while q:
        c, r = q.popleft()
        for n in neighbors(c, r):
            if n not in seen and tile(*n) in PASSABLE and n not in ARCH_BLOCK: seen.add(n); q.append(n)
    return seen
r0 = reach((29, 41))
for p in party_city + neutrals + [{'name': 'passage', 'x': PASSAGE[0], 'y': PASSAGE[1]}]:
    assert (p['x'], p['y']) in r0, ('unreachable', p['name'], p['x'], p['y'])
    assert (p['x'], p['y']) not in used, ('standing on a prop', p['name'])
assert (43, 32) in r0, 'the temple floor must be reachable through its mouth'

def draft(id, title, explore, players, enemies, neutral, briefing, objective, serial):
    d = dict(base)
    d.update({'id': id, 'title': title, 'place': 'A cidade, 25 anos antes', 'briefing': briefing, 'objective': objective,
              'win': 'rout', 'hub': False, 'explore': explore, 'fog': False, 'locationId': 'ruins', 'cols': COLS, 'rows': ROWS, 'timeOfDay': 'day',
              'tiles': list(tiles), 'tileVariants': [0] * (COLS * ROWS), 'terrainElevations': [0] * (COLS * ROWS), 'waterLevels': [None] * (COLS * ROWS), 'tileRots': [0] * (COLS * ROWS),
              'decorations': [dict(x) for x in decorations if explore or x['id'] != 'floor-connector'], 'elementalFx': [],
              'playerSpawns': players, 'enemySpawns': enemies, 'neutralSpawns': neutral})
    return {'serial': serial, 'savedAt': int(time.time() * 1000), 'draft': d}

def next_serial(id):
    n = 0
    for f in os.listdir(f'{ROOT}/content/maps/ember'):
        if f.startswith(id) and len(f) == len(id) + 8 and f[len(id):-5].isdigit(): n = max(n, int(f[len(id):-5]))
    return n + 1

s1 = next_serial('interlude-city')
out1 = f'{ROOT}/content/maps/ember/interlude-city{s1:03}.json'
json.dump(draft('interlude-city', 'A cidade, 25 anos antes', True, party_city, [], neutrals,
                'A cidade ainda de pé: muralhas, o mercado, o bairro nobre com a casa dos Salazar, o bairro pobre e o templo. Os três entram pelo portão sul; a passagem no portão norte leva adiante.',
                'Atravesse a cidade até o portão norte', s1), open(out1, 'w', encoding='utf-8'), ensure_ascii=False)
# the mission: the party inside the north gate; the enemies hold the avenue, the plaza and the temple
party_fight = [dict(p, x=x, y=y) for p, (x, y) in zip(party_city, ((29, 4), (28, 5), (31, 4)))]
foes = [{'name': 'Besteiro', 'classId': 'baseCrossbowman', 'x': 27, 'y': 15}, {'name': 'Besteiro', 'classId': 'baseCrossbowman', 'x': 32, 'y': 15},
        {'name': 'Cultista Ancestral', 'classId': 'cultistV2', 'x': 29, 'y': 20}, {'name': 'Zumbi', 'classId': 'zombie', 'x': 27, 'y': 24},
        {'name': 'Zumbi', 'classId': 'zombie', 'x': 32, 'y': 24}, {'name': 'Cão de guerra', 'classId': 'wardog2', 'x': 38, 'y': 33},
        {'name': 'Cultista Ancestral', 'classId': 'cultistV2', 'x': 44, 'y': 33}]
rf = reach((29, 4))
for f in foes + party_fight:
    assert (f['x'], f['y']) in rf and (f['x'], f['y']) not in used, ('fight spawn', f['name'], f['x'], f['y'])
s2 = next_serial('interlude-placeholder')
out2 = f'{ROOT}/content/maps/ember/interlude-placeholder{s2:03}.json'
json.dump(draft('interlude-placeholder', 'A cidade tomada', False, party_fight, foes, [],
                'A mesma cidade: cultistas, mortos-vivos e besteiros tomaram a avenida, a praça e o templo. Limpe as ruas.',
                'Derrote todos os inimigos', s2), open(out2, 'w', encoding='utf-8'), ensure_ascii=False)
os.makedirs(f'{ROOT}/content/interlude', exist_ok=True)
json.dump(plan, open(f'{ROOT}/content/interlude/city-plan.json', 'w', encoding='utf-8'), ensure_ascii=False, indent=1)
print(f'wrote {out1}\n      {out2}\n      {len(decorations)} decorations, {len(neutrals)} townsfolk, {len(r0)} hexes reachable from the south gate')
