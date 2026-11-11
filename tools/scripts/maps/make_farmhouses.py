"""The Farmlands houses, rebuilt (2026-11-11; the user: "você precisa habilitar luzes e probes vindos das janelas e entradas das casas
do farmlands, não consigo nem sair nem enxergar nada quando entro em uma casa", "crie janelas e coloque objetos e iluminação
aumente o tamanho de todas as casas", "a casa precisa seguir a luminosidade do relogio assim como a torre").

  python -I tools/scripts/maps/make_farmhouses.py [smith|apothecary|store|inn|villagers ...] [--dry]

Writes the next serial of each id in content/maps/ember (never an overwrite; `--dry` runs the checks and prints the plan only), on
the Mosteiro's builder (monastery_lib.py) with the houses' own wall kit (`ember-arch-fazenda-*`, register_farmhouse_architecture.py)
and their 3D kit (`ember-farmhouse-*`, create_farmhouse_kit_v01.py).

What a house was: 18 x 14, a ring of plain plaster wall with a two-hex gap for a door, "environment": "indoor" and nothing else: no
window, no door piece, no lamp. An indoor map that is not "aboveGround" has no light from outside at all (MapStage darkIndoor), so
the room was black; and the gap left it open to the board's edge, so it had no room, no roof and no light probes either.

What a house is now: "Interna acima do solo" ("aboveGround": true), like the Torre de Vigia: the sun and the moon follow the
game's clock and come in through the windows and the open doorway (each opening its own light, the room its light probes); oil
lanterns, the hearth and the forge carry the night. Every house is larger. Each keeps its own title, texts, people (with their
conversations), its plank floor and the photographed furniture it had; the way back to the fields is the connector in the doorway.

  farmlands-smith           Ferraria de Duarte      22 x 16   the forge, the anvil, the quench tub, the tool rack, the grindstone
  farmlands-apothecary      Botica de Lia           22 x 16   the counter, the remedy shelves, the herb racks
  farmlands-general-store   Armazém dos Campos      22 x 16   the counter, the provision tables, shelves, barrels, crates, sacks
  farmlands-inn             A Pequena Pousada       26 x 18   two bedrooms behind the common room with its hearth and long table
  farmlands-villagers       Casa dos Moradores      24 x 17   the hearth, the long table, the spinning wheel, the cradle
"""
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from monastery_lib import MAPS, Floor, hwall, ring, vwall  # noqa: E402

KIT = 'fazenda'
FLOOR_PLANKS = 18  # the nave tile's variant the houses stand on (hex-ground-013-farm-planks)
ANVIL, TUB, TOOLS, GRINDSTONE = 'ember-farmhouse-anvil', 'ember-farmhouse-quench-tub', 'ember-farmhouse-tool-rack', 'ember-farmhouse-grindstone'
COUNTER, REMEDIES, HERBS, TABLE = 'ember-farmhouse-shop-counter', 'ember-farmhouse-remedy-shelf', 'ember-farmhouse-herb-rack', 'ember-farmhouse-farm-table'
WHEEL, CRADLE, LANTERN, WASHSTAND, CHEST = ('ember-farmhouse-spinning-wheel', 'ember-farmhouse-cradle', 'ember-farmhouse-oil-lantern',
                                            'ember-farmhouse-washstand', 'ember-farmhouse-linen-chest')
FORGE, PHOTO_BED, PHOTO_TABLE, PHOTO_CABINET = ('farmland-photographic-013-forge', 'farmland-photographic-014-bed',
                                                'farmland-photographic-014-table', 'farmland-photographic-013-cabinet')
BARREL, CRATE, SACKS, RACK = 'ember-props-barrel-v02', 'ember-props-crate-v02', 'ember-props-sacks-v02', 'ember-interior-barrel-rack-v02'
BED, HEARTH, RUG, STOOL, CHAIR = ('ember-interior-simple-bed-v02', 'ember-tavern-stone-hearth', 'ember-interior-worn-rug', 'ember-interior-stool',
                                  'ember-tavern-chair-v04')
PANTRY, DRINKS, CAULDRON, SMALL_TABLE, JUGS, POT = ('ember-interior-pantry-shelf', 'ember-interior-drink-shelf', 'ember-interior-cauldron',
                                                    'ember-tavern-table-small', 'ember-tavern-bottles-jugs', 'ember-interior-iron-pot')
WEAPONS, WORKBENCH, BUCKET = 'ember-treasure-weapon-rack-v02', 'ember-treasure-workbench-v02', 'ember-polyhaven-wooden-bucket'


def newest(map_id):
    """The house as it is played today: the newest saved file of its id."""
    serial = max(int(f[len(map_id):-5]) for f in os.listdir(MAPS) if f.startswith(map_id) and len(f) == len(map_id) + 8 and f[len(map_id):-5].isdigit())
    return json.load(open(os.path.join(MAPS, f'{map_id}{serial:03}.json'), encoding='utf-8'))['draft']


class House:
    """One house: the outer ring one hex in from the board's edge (the rim stays void, as before), the doorway in the south wall
    with the way back in it, the party just inside."""

    def __init__(self, map_id, cols, rows):
        self.old = newest(map_id)
        f = self.f = Floor(map_id, cols, rows, 'void', KIT)
        f.rect(1, 1, cols - 2, rows - 2, 'nave')
        f.walls |= ring(1, 1, cols - 2, rows - 2)
        self.door = (cols // 2 - 1, rows - 2)
        f.opening(f.k['PORTAL'], *self.door)
        self.p = f.place

    def windows(self, north=(), south=(), west=(), east=()):
        f = self.f
        f.windows |= {(x, 1) for x in north} | {(x, f.rows - 2) for x in south} | {(1, y) for y in west} | {(f.cols - 2, y) for y in east}

    def torches(self, north=(), south=(), west=(), east=(), inner=()):
        """Wall torches between the windows (an opening's light reaches seven units and a house is deeper than that: the walls'
        own fires and the lanterns carry the middle of the room and the night); `inner`: on the partitions."""
        f = self.f
        f.torches |= {(x, 1) for x in north} | {(x, f.rows - 2) for x in south} | {(1, y) for y in west} | {(f.cols - 2, y) for y in east} | set(inner)

    def lanterns(self, *cells):
        for x, y in cells:
            self.p(LANTERN, x, y)

    def people(self, where):
        """The house's own people, each with the conversation it had, at its new place (`where`: name -> hex)."""
        left = dict(where)
        for s in self.old['neutralSpawns']:
            x, y = left.pop(s['name'])
            self.f.person(s['name'], s['classId'], x, y, s.get('level', 1), s.get('dialog'))
        if left:
            raise SystemExit(f'{self.f.id}: no such person in the house: {sorted(left)}')

    def finish(self, dry):
        f, (dx, dy) = self.f, self.door
        self.p('floor-connector', dx, dy, targetMapId='farmlands', returnConnector=True)
        spawns = ((dx, dy - 2), (dx + 1, dy - 2), (dx, dy - 1), (dx - 1, dy - 1))
        for s, (x, y) in zip(self.old['playerSpawns'], spawns):
            f.person(s['name'], s['classId'], x, y, s.get('level', 1), player=True)
        seen = f.check(spawns[0])
        if (dx, dy) not in seen:
            raise SystemExit(f'{f.id}: the way back at {dx},{dy} cannot be walked to from the door')
        lights = sum(1 for d in f.decorations if d['id'] in (LANTERN, HEARTH, FORGE, f.k['TORCH']))
        print(f'{f.id}: {f.cols} x {f.rows} (was {self.old["cols"]} x {self.old["rows"]}), {len(f.windows)} windows, the doorway at {dx},{dy}, '
              f'{lights} lamps and fires, {len(f.neutrals)} people')
        if dry:
            f.show()
            return None
        variants = [FLOOR_PLANKS if t == 'nave' else 0 for t in f.tiles]
        return f.write(self.old, seen, tileVariants=variants, locationId='farm', environment='indoor', aboveGround=True)


# ================================================================================================= Ferraria de Duarte
def smith(dry):
    h = House('farmlands-smith', 22, 16)
    p = h.p
    h.windows(north=(4, 7, 14, 17), south=(4, 7, 14, 17), west=(4, 8, 11), east=(4, 8, 11))
    h.torches(north=(9, 12), south=(8, 13), west=(6,), east=(6,))
    h.f.lay_walls()
    p(FORGE, 10, 3)                       # the forge against the north wall, the smith between it and the anvil
    p(ANVIL, 9, 6); p(TUB, 12, 6); p(TOOLS, 6, 2); p(TOOLS, 15, 2); p(GRINDSTONE, 16, 6)
    p(WEAPONS, 3, 2); p(WEAPONS, 18, 2); p(WORKBENCH, 4, 6); p(SACKS, 13, 3); p(BUCKET, 8, 4)
    p(BARREL, 2, 12); p(BARREL, 3, 13); p(CRATE, 18, 13); p(CRATE, 19, 12); p(SACKS, 19, 9)
    h.lanterns((3, 4), (18, 4), (6, 9), (15, 9))
    h.people({'Duarte': (10, 5)})
    return h.finish(dry)


# ===================================================================================================== Botica de Lia
def apothecary(dry):
    h = House('farmlands-apothecary', 22, 16)
    p = h.p
    h.windows(north=(5, 10, 16), south=(4, 7, 14, 17), west=(4, 8, 11), east=(4, 8, 11))
    h.torches(south=(8, 13), west=(6,), east=(6,))
    h.f.lay_walls()
    p(PHOTO_CABINET, 2, 3); p(PHOTO_CABINET, 18, 3)
    p(REMEDIES, 7, 2); p(REMEDIES, 8, 2); p(REMEDIES, 12, 2); p(REMEDIES, 13, 2)
    p(COUNTER, 10, 6); p(JUGS, 8, 6); p(STOOL, 13, 6)
    p(HERBS, 3, 7); p(HERBS, 5, 9); p(HERBS, 17, 7); p(CAULDRON, 16, 10); p(POT, 18, 10)
    p(SMALL_TABLE, 4, 12); p(STOOL, 6, 12); p(BARREL, 19, 13); p(SACKS, 2, 10)
    h.lanterns((7, 4), (14, 4), (7, 10), (14, 9))
    h.people({})
    return h.finish(dry)


# ================================================================================================ Armazém dos Campos
def store(dry):
    h = House('farmlands-general-store', 22, 16)
    p = h.p
    h.windows(north=(4, 10, 17), south=(4, 7, 14, 17), west=(4, 8, 11), east=(4, 8, 11))
    h.torches(south=(8, 13), west=(6,), east=(6,))
    h.f.lay_walls()
    p(PANTRY, 6, 2); p(PANTRY, 8, 2); p(DRINKS, 13, 2); p(PANTRY, 15, 2)
    p(COUNTER, 10, 6)
    p(PHOTO_TABLE, 2, 9); p(PHOTO_TABLE, 15, 9)
    p(BARREL, 2, 2); p(BARREL, 3, 3); p(BARREL, 19, 2); p(RACK, 17, 4); p(CRATE, 2, 5); p(CRATE, 3, 6); p(SACKS, 19, 6)
    p(CRATE, 2, 13); p(SACKS, 4, 13); p(BARREL, 19, 13); p(CRATE, 17, 13); p(JUGS, 13, 7); p(BUCKET, 8, 7)
    h.lanterns((7, 4), (13, 4), (8, 11), (13, 11))
    h.people({'Bento': (10, 5)})
    return h.finish(dry)


# ================================================================================================== A Pequena Pousada
def inn(dry):
    h = House('farmlands-inn', 26, 18)
    f, p = h.f, h.p
    # two bedrooms behind the common room: a wall across the house, a wall between them, an open doorway into each
    f.walls |= hwall(7, 1, 24) | vwall(12, 1, 7)
    f.opening(f.k['PORTAL'], 5, 7)
    f.opening(f.k['PORTAL'], 18, 7)
    h.windows(north=(4, 8, 16, 20), south=(4, 8, 17, 21), west=(4, 10, 13), east=(4, 10, 13))
    h.torches(north=(6, 22), south=(10, 15), inner=((3, 7), (9, 7), (15, 7), (22, 7), (12, 4)))
    f.lay_walls()
    # the west bedroom
    p(PHOTO_BED, 2, 3); p(PHOTO_BED, 8, 3); p(BED, 2, 5); p(CHEST, 5, 2); p(WASHSTAND, 10, 5); p(RUG, 5, 4)
    # the east bedroom
    p(PHOTO_BED, 14, 3); p(PHOTO_BED, 21, 3); p(BED, 22, 5); p(CHEST, 18, 2); p(WASHSTAND, 14, 5); p(RUG, 18, 4)
    # the common room: the hearth on the bedrooms' wall, the long table, the inn's own provisions table
    p(HEARTH, 11, 8); p(TABLE, 15, 11); p(PHOTO_TABLE, 3, 10); p(RACK, 21, 8); p(BARREL, 23, 9); p(DRINKS, 2, 8); p(STOOL, 9, 10); p(CHAIR, 14, 9)
    p(CRATE, 2, 14); p(SACKS, 23, 14); p(RUG, 12, 12)
    h.lanterns((7, 5), (16, 5), (8, 13), (20, 13), (8, 9))
    h.people({'Marta': (13, 10), 'Neera': (5, 12), 'Voss': (10, 12), 'Salazar': (16, 13), 'Aldric': (18, 10), 'Malrec': (20, 11)})
    return h.finish(dry)


# ================================================================================================== Casa dos Moradores
def villagers(dry):
    h = House('farmlands-villagers', 24, 17)
    p = h.p
    h.windows(north=(4, 7, 15, 19), south=(4, 7, 16, 19), west=(4, 8, 12), east=(4, 8, 12))
    h.torches(south=(9, 14), west=(6, 10), east=(6, 10))
    h.f.lay_walls()
    p(HEARTH, 10, 2); p(CAULDRON, 13, 3); p(POT, 9, 3); p(STOOL, 8, 4)
    p(TABLE, 10, 7); p(PHOTO_TABLE, 2, 11)
    p(WHEEL, 17, 5); p(CRADLE, 19, 8); p(CHEST, 20, 2); p(BED, 16, 2); p(WASHSTAND, 3, 2); p(HERBS, 5, 2)
    p(RUG, 11, 10); p(BARREL, 2, 6); p(SACKS, 21, 13); p(CRATE, 20, 13); p(STOOL, 16, 9)
    h.lanterns((4, 5), (19, 5), (8, 10), (15, 11))
    h.people({'O Sineiro': (6, 4), 'A Guardiã das Mariposas': (14, 5), 'O Escrevente': (14, 9), 'O Carvoeiro': (6, 8), 'Rui': (11, 9)})
    return h.finish(dry)


HOUSES = {'smith': smith, 'apothecary': apothecary, 'store': store, 'inn': inn, 'villagers': villagers}

if __name__ == '__main__':
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    for name in args or list(HOUSES):
        if name not in HOUSES:
            raise SystemExit(f'unknown house {name}: {", ".join(HOUSES)}')
        HOUSES[name]('--dry' in sys.argv)
