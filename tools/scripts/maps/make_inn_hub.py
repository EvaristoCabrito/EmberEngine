"""A Estalagem, both floors, the first town hub: much larger (2026-11-06; the user: "ainda estou achando a taverna toda muito apertada,
claustrofóbica, esses cenários precisam ser maiores, para os jogadores quererem ficar lá explorando por um tempo", "preciso de mais um
extreme makeover com muito mais espaço", "imagine que o INN é o primeiro hub de cidade por um longo tempo ... vai ter vários tipos de
serviços como descanso, comida e cura").

  python -I tools/scripts/maps/make_inn_hub.py

Writes estalagem012 (from 011) and estalagem-andar-2010 (from 009), "Interna acima do solo" like their sources (Luz das janelas + the
light probes); every older version untouched; refuses to overwrite. Same validation as make_inn_v8.py (its build()): nothing on a
wall, another piece, an NPC or a doorway; every NPC, door and the stairs reachable on foot; every lantern on a wall facing a room.

Salão 64 x 44 (was 40 x 28, 2.5 x the area): the great hall (x 1-45, y 14-42) with the long bar, two Meshy hearths, four rows of
refectory tables and a quarter of small tables; the north wing: the kitchen, the store behind the bar, the healer's infirmary (four
beds), Vargan's armoury, the bath house; the east wing: the back room, the game room (the mercenaries), the stairs room.
Andar II 60 x 40 (was 41 x 27): a five-hex corridor between ten rooms: four bedrooms, the guests' lounge with a hearth and books, the
private suite above; the dormitory, two bedrooms, the bath room and the linen store with the stairs below.
"""
import json
import os
import sys
from collections import Counter

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from make_inn_v8 import ROOT, build, chair, p, show  # noqa: E402

WINE, BARRELS, CRATE, TCHEST = ('ember-polyhaven-wine-barrel-01', 'ember-polyhaven-wooden-barrels-01', 'ember-polyhaven-wooden-crate',
                                'ember-polyhaven-treasure-chest')
PBENCH, WSHELF, BUCKET, BUCKET2 = ('ember-polyhaven-painted-bench', 'ember-polyhaven-wooden-bookshelf-worn', 'ember-polyhaven-wooden-bucket',
                                   'ember-polyhaven-wooden-bucket-02')
GTABLE, GCHAIR, KITE, LADDER = 'ember-polyhaven-gallinera-table', 'ember-polyhaven-gallinera-chair', 'ember-polyhaven-kite-shield', 'ember-polyhaven-wooden-ladder-02'
BED, STOOL, SHELF, RUG = 'ember-interior-simple-bed-v02', 'ember-interior-stool', 'ember-interior-bookshelf', 'ember-interior-worn-rug'
CHEST, CHEST2, SCREEN, CURTAIN = 'ember-treasure-plain-chest', 'ember-treasure-iron-banded-chest', 'ember-interior-folding-screen', 'ember-interior-cloth-curtain'
SMALL, MESS, BENCH, HEARTH = 'ember-tavern-table-small', 'ember-tavern-table-messhall', 'ember-interior-long-bench-v02', 'ember-tavern-stone-hearth'
CANDLE = 'light-candle'


def small_table(x, y):
    """A small table (x, x + 1) with a chair at each end."""
    return [p(SMALL, x, y), chair(x - 1, y, 'e', True), chair(x + 2, y, 'w', True)]


def mess_table(x, y):
    """A refectory table (x - 1 .. x + 1), benches north and south, a chair at each end."""
    return [p(MESS, x, y), p(BENCH, x - 1, y - 1), p(BENCH, x - 1, y + 1), chair(x - 2, y, 'e'), chair(x + 2, y, 'w')]


def salao():
    cols, rows = 64, 44
    runs = [('h', 13, 0, 63),                                                  # the north wing's wall
            ('v', 14, 1, 12), ('v', 28, 1, 12), ('v', 42, 1, 12), ('v', 52, 1, 12),  # kitchen | store | infirmary | armoury | baths
            ('v', 46, 14, 42),                                                 # the east wing's wall
            ('h', 24, 47, 63), ('h', 33, 47, 63)]                              # back room | game room | stairs room
    windows = [(4, 0), (10, 0), (18, 0), (24, 0), (32, 0), (38, 0), (46, 0), (50, 0), (56, 0), (60, 0),
               (0, 4), (0, 9), (0, 17), (0, 22), (0, 27), (0, 32), (0, 37),
               (6, 43), (12, 43), (30, 43), (36, 43), (40, 43), (52, 43), (58, 43),
               (63, 4), (63, 9), (63, 18), (63, 28), (63, 38)]
    doors = [(7, 13, 2), (21, 13, 2), (35, 13, 2), (47, 13, 2), (57, 13, 2),  # the north wing's rooms (the leaf swings into the room)
             (22, 43, 2)]                                                       # the front door (swings into the hall)
    doorways = [(46, 18, 1), (46, 28, 1), (46, 38, 1)]                          # the back room, the game room, the stairs room
    sconces = [(2, 0), (7, 0), (12, 0), (16, 0), (21, 0), (26, 0), (30, 0), (35, 0), (40, 0), (44, 0), (48, 0), (54, 0), (58, 0), (62, 0),
               (3, 13), (11, 13), (17, 13), (25, 13), (31, 13), (39, 13), (50, 13), (60, 13),
               (0, 2), (0, 7), (0, 11), (0, 15), (0, 20), (0, 25), (0, 30), (0, 35), (0, 40),
               (3, 43), (9, 43), (15, 43), (19, 43), (26, 43), (33, 43), (43, 43), (49, 43), (55, 43), (61, 43),
               (63, 2), (63, 7), (63, 11), (63, 16), (63, 21), (63, 27), (63, 31), (63, 36), (63, 41),
               (46, 16), (46, 22), (46, 26), (46, 32), (46, 36), (46, 41),
               (55, 24), (55, 33)]
    f = []
    # -- the great hall (x 1-45, y 14-42) --
    # the bar: four counters (16-23) on row 16, Brue behind it, the shelves and the barrels against the wing's wall
    f += [p('ember-tavern-counter', x, 16) for x in (16, 18, 20, 22)]
    f += [p('ember-interior-drink-shelf', 16, 14), p('ember-interior-barrel-rack-v02', 18, 14), p('ember-interior-drink-shelf', 23, 14),
          p('ember-tavern-bottles-jugs', 24, 14), p('inn-provisions-display', 15, 14), p(WINE, 25, 15), p(WINE, 14, 15)]
    f += [p(STOOL, x, 17) for x in (16, 18, 20, 22, 23)]
    # the north-west hearth corner
    f += [p(HEARTH, 2, 14), p(RUG, 2, 16), p(BENCH, 2, 18), chair(5, 16, 'w'), chair(1, 17, 'e'), p('city-firewood-pile', 5, 14),
          p('ember-interior-fireplace-grate', 4, 15)]
    # the west hearth, half-way down the hall
    f += [p(HEARTH, 1, 29), p(RUG, 2, 31), p(PBENCH, 4, 30), p(PBENCH, 4, 32), p('city-firewood-stack-new', 1, 27)]
    # four rows of refectory tables, three to a row
    for y in (21, 26, 31, 36):
        for x in (8, 16, 24):
            f += mess_table(x, y)
    # the quarter of small tables (x 30-41)
    for y in (20, 25, 30, 35):
        f += small_table(31, y) + small_table(38, y)
    # the entrance: barrels and benches by the door, a coat corner, benches along the south wall
    f += [p(BARRELS, 19, 42), p(BARRELS, 25, 42), p(PBENCH, 10, 42), p(PBENCH, 14, 42), p(PBENCH, 30, 42), p(PBENCH, 34, 42),
          p(WINE, 1, 42), p(WINE, 45, 42), p(WINE, 45, 14), p('city-barrels', 44, 14), p(SCREEN, 41, 41), p('ember-props-barrel-v02', 2, 41)]
    f += [p(CANDLE, x, y) for x, y in ((12, 23), (20, 23), (12, 33), (20, 33), (28, 28), (35, 22), (35, 32), (42, 27), (27, 18), (9, 40))]
    # iron braziers in the aisles: the middle of a 45 x 29 hall is far from every window
    f += [p('light-brazier-bowl', x, y) for x, y in ((12, 28), (20, 28), (28, 23), (28, 33), (4, 24), (4, 37), (35, 40), (42, 33), (42, 18))]
    # -- the kitchen (x 1-13, y 1-12) --
    f += [p('ember-interior-cauldron', 2, 2), p('ember-interior-kitchen-cabinet', 4, 1), p('ember-interior-kitchen-cabinet', 5, 1),
          p('ember-interior-pantry-shelf', 8, 1), p('ember-interior-pantry-shelf', 9, 1), p('ember-treasure-ceramic-jars', 12, 1),
          p('ember-treasure-workbench-v02', 3, 6), p('ember-treasure-workbench-v02', 9, 6), p('ember-interior-iron-pot', 6, 3),
          p('city-provisions', 12, 4), p('ember-props-sacks-v02', 1, 5), p(BUCKET, 1, 10), p('city-basket', 12, 11), p(CANDLE, 5, 10),
          p(SMALL, 9, 9), chair(8, 9, 'e', True), chair(11, 9, 'w', True), p('city-barrels', 12, 7), p('city-firewood-pile', 1, 12)]
    # -- the store behind the bar (x 15-27) --
    f += [p('ember-interior-barrel-rack-v02', 15, 1), p('ember-interior-barrel-rack-v02', 19, 1), p('ember-interior-barrel-rack-v02', 23, 1),
          p('city-crate-stack', 27, 1), p('city-covered-crate', 27, 3), p('city-sack-pile', 15, 5), p('city-sack-pile', 15, 6),
          p('ember-props-crate-v02', 15, 10), p(CRATE, 27, 10), p('ember-props-barrel-v02', 27, 7), p('ember-treasure-open-barrel', 26, 11),
          p(WINE, 19, 6), p(WINE, 23, 6), p(BARRELS, 25, 8), p(CANDLE, 21, 4), p(LADDER, 17, 3)]
    # -- the healer's infirmary (x 29-41): four beds behind screens, the herbs, the books --
    f += [p('inn-herb-shelf', 29, 1), p('inn-herb-shelf', 30, 1), p(SHELF, 32, 1), p(WSHELF, 33, 1),
          p(BED, 36, 2), p(BED, 39, 2), p(BED, 36, 10), p(BED, 39, 10), p(SCREEN, 36, 4), p(SCREEN, 36, 8),
          p(SMALL, 31, 6), chair(33, 6, 'w', True), p('ember-interior-table-candelabra', 29, 11), p('ember-treasure-ceramic-jars', 41, 6),
          p(RUG, 31, 9), p(BUCKET2, 29, 8), p(CANDLE, 41, 4)]
    # -- Vargan's armoury (x 43-51) --
    f += [p('ember-treasure-weapon-rack-v02', 43, 1), p('ember-treasure-weapon-rack-v02', 45, 1), p('ember-treasure-weapon-rack-v02', 47, 1),
          p('ember-treasure-shield-stand-v02', 49, 1), p(KITE, 51, 3), p('ember-treasure-workbench-v02', 44, 6), p(CHEST2, 51, 6),
          p('ember-props-crate-v02', 43, 11), p(TCHEST, 51, 10), p(CANDLE, 49, 11), p('ember-treasure-shield-stand-v02', 43, 9)]
    # -- the bath house (x 53-62): tubs, buckets, screens --
    f += [p('ember-treasure-open-barrel', 54, 2), p('ember-treasure-open-barrel', 58, 2), p('ember-treasure-open-barrel', 61, 5),
          p(BUCKET, 53, 1), p(BUCKET2, 56, 1), p(BUCKET, 62, 1), p(SCREEN, 54, 5), p(CURTAIN, 58, 8), p('city-basket', 53, 10),
          p(PBENCH, 62, 10), p('city-sack-pile', 53, 7), p(CANDLE, 57, 11)]
    # -- the back room (x 47-62, y 14-23) --
    f += small_table(53, 18) + [p(CURTAIN, 48, 14), p(SHELF, 62, 14), p(WSHELF, 61, 14), p(CHEST, 62, 22), p(TCHEST, 61, 22),
                                p(RUG, 53, 20), p(CANDLE, 49, 22), p(GTABLE, 58, 16), p(GCHAIR, 59, 16)]
    # -- the game room (x 47-62, y 25-32): the mercenaries' tables --
    f += small_table(50, 27) + small_table(57, 27) + small_table(50, 31) + small_table(57, 31)
    f += [p('ember-interior-barrel-rack-v02', 61, 25), p(BARRELS, 62, 29), p(CANDLE, 54, 29), p(PBENCH, 47, 25)]
    # -- the stairs room (x 47-62, y 34-42) --
    f += [p('inn-stairs-up', 57, 40), p('floor-connector', 57, 40, targetMapId='estalagem-andar-2'),
          p('ember-props-crate-v02', 48, 34), p('ember-props-barrel-v02', 62, 34), p('city-sack-pile', 48, 42), p('ember-props-sacks-v02', 62, 42),
          p(BENCH, 50, 36), p(CRATE, 62, 38)]
    npcs = [
        {'name': 'Brue', 'x': 20, 'y': 15},
        {'name': 'Velha da Sopa', 'x': 4, 'y': 3}, {'name': 'Padeira', 'x': 6, 'y': 8},
        {'name': 'Lenhador', 'x': 21, 'y': 9},
        {'name': 'Curandeiro Ancião', 'x': 34, 'y': 6}, {'name': 'Salazar', 'x': 31, 'y': 3},
        {'name': 'Vargan', 'x': 47, 'y': 5},
        {'name': 'Cliente Suspeito', 'x': 53, 'y': 17}, {'name': 'A Hóspede', 'x': 58, 'y': 20},
        {'name': 'O Mudo', 'x': 52, 'y': 38},
        {'name': 'Beberrão', 'x': 6, 'y': 18}, {'name': 'Camponês', 'x': 16, 'y': 23}, {'name': 'Aldeã', 'x': 9, 'y': 28},
        {'name': 'Louca da Vela', 'x': 59, 'y': 6},
        {'name': 'Neera', 'x': 12, 'y': 34}, {'name': 'Voss', 'x': 35, 'y': 27}, {'name': 'Aldric', 'x': 9, 'y': 23}, {'name': 'Malrec', 'x': 28, 'y': 34},
        {'name': 'Soldado de Passagem', 'x': 52, 'y': 28}, {'name': 'Piqueiro da Escolta', 'x': 55, 'y': 30},
        {'name': 'Besteiro Veterano', 'x': 59, 'y': 28},
    ]
    return build('estalagem011', 'estalagem012', 12, cols, rows, runs, windows, doors, doorways, sconces, f, npcs, (22, 40), [], {})


def andar():
    cols, rows = 60, 40
    runs = [('h', 17, 1, 58), ('h', 23, 1, 58),                                       # the corridor's walls (rows 18-22 between)
            ('v', 12, 1, 16), ('v', 24, 1, 16), ('v', 36, 1, 16), ('v', 48, 1, 16),  # the rooms above
            ('v', 13, 24, 38), ('v', 25, 24, 38), ('v', 37, 24, 38), ('v', 47, 24, 38)]  # the rooms below (the stairs room x 48-58)
    windows = [(3, 0), (8, 0), (16, 0), (20, 0), (28, 0), (32, 0), (40, 0), (44, 0), (52, 0), (56, 0),
               (4, 39), (9, 39), (17, 39), (21, 39), (29, 39), (33, 39), (41, 39), (51, 39), (55, 39),
               (0, 6), (0, 12), (0, 20), (0, 28), (0, 34), (59, 6), (59, 12), (59, 20), (59, 28), (59, 34)]
    doors = [(6, 17, 2), (18, 17, 2), (30, 17, 2), (42, 17, 2), (53, 17, 2),  # above (the leaf swings north, into the room)
             (7, 23, 0), (19, 23, 0), (31, 23, 0), (42, 23, 0)]               # below (south, into the room)
    doorways = [(53, 23, 0)]                                                     # the stairs room
    sconces = [(5, 0), (10, 0), (14, 0), (22, 0), (26, 0), (34, 0), (38, 0), (46, 0), (50, 0), (57, 0),
               (2, 39), (11, 39), (15, 39), (23, 39), (27, 39), (35, 39), (39, 39), (45, 39), (49, 39), (57, 39),
               (0, 3), (0, 9), (0, 15), (0, 18), (0, 26), (0, 32), (0, 37),
               (59, 3), (59, 9), (59, 15), (59, 18), (59, 26), (59, 32), (59, 37),
               (3, 17), (15, 17), (27, 17), (39, 17), (51, 17), (10, 23), (22, 23), (34, 23), (44, 23), (56, 23)]
    f = []

    def bedroom(y_bed, beds, top):
        """Beds against the outer wall, a stool beside each, a chest at the foot."""
        out = []
        for bx in beds:
            out += [p(BED, bx, y_bed), p(STOOL, bx + 2, y_bed), p(CHEST, bx, y_bed + (1 if top else -1))]
        return out

    # above (y 1-16)
    f += bedroom(1, [2], True) + [p(RUG, 3, 7), p(SMALL, 7, 10), chair(9, 10, 'w', True), p('ember-interior-wooden-mug', 1, 14),
                                  p(WSHELF, 11, 1), p(CANDLE, 1, 1), p(BUCKET, 11, 14), p(SCREEN, 6, 3)]
    f += bedroom(1, [14, 19], True) + [p(SHELF, 23, 1), p(RUG, 16, 8), p(SMALL, 19, 11), chair(18, 11, 'e', True), p(CANDLE, 13, 3),
                                       p(CHEST2, 23, 15)]
    # the guests' lounge (x 25-35): the hearth, books, armchairs, a reading table
    f += [p(HEARTH, 29, 1), p(RUG, 29, 3), p(PBENCH, 27, 4), p(PBENCH, 32, 4), p(SHELF, 25, 1), p(WSHELF, 26, 1), p(SHELF, 35, 1),
          p(WSHELF, 34, 1), p(GTABLE, 30, 9), p(GCHAIR, 29, 9), p(GCHAIR, 31, 9), p(SMALL, 26, 13), chair(25, 13, 'e', True),
          p('ember-interior-table-candelabra', 35, 12), p(CANDLE, 33, 15), p('city-firewood-pile', 25, 4)]
    f += bedroom(1, [38, 43], True) + [p(RUG, 40, 8), p(SMALL, 44, 12), chair(43, 12, 'e', True), p(WSHELF, 47, 1), p(CANDLE, 37, 15),
                                       p('ember-interior-kitchen-cabinet', 47, 15)]
    # the private suite (x 49-58)
    f += bedroom(1, [50, 55], True) + [p(CURTAIN, 50, 4), p(CURTAIN, 55, 4), p(RUG, 52, 8), p(GTABLE, 53, 12), p(GCHAIR, 52, 12),
                                       p(GCHAIR, 54, 12), p(CHEST2, 58, 15), p(TCHEST, 49, 15), p(SHELF, 58, 8),
                                       p('ember-interior-table-candelabra', 49, 8)]
    # below (y 24-38)
    # the dormitory (x 1-12): three beds
    f += bedroom(38, [2, 6], False) + [p(BED, 10, 31), p(STOOL, 9, 30), p(RUG, 4, 30), p('ember-interior-cauldron', 12, 25),
                                       p('ember-treasure-ceramic-jars', 1, 25), p(SMALL, 5, 26), chair(4, 26, 'e', True), p(CANDLE, 1, 33)]
    f += bedroom(38, [15], False) + [p(SCREEN, 21, 36), p(RUG, 16, 30), p(SMALL, 19, 26), chair(21, 26, 'w', True), p(WSHELF, 24, 38),
                                     p(CANDLE, 14, 25), p(CHEST2, 24, 26)]
    f += bedroom(38, [27], False) + [p(SHELF, 36, 38), p(RUG, 28, 30), p(CHEST2, 26, 25), p(SMALL, 32, 27), chair(34, 27, 'w', True),
                                     p(CANDLE, 36, 25)]
    # the bath room (x 38-46): tubs, buckets, screens
    f += [p('ember-treasure-open-barrel', 39, 37), p('ember-treasure-open-barrel', 43, 37), p(BUCKET, 38, 38), p(BUCKET2, 46, 38),
          p(SCREEN, 39, 33), p(PBENCH, 46, 30), p('city-basket', 38, 25), p(CANDLE, 46, 25)]
    # the stairs room (x 48-58): the inn's linen store
    f += [p('inn-stairs-down', 55, 36, mirrorX=True), p('floor-connector', 55, 36, targetMapId='estalagem', returnConnector=True),
          p('city-sack-pile', 48, 24), p('city-sack-pile', 48, 25), p('city-crate-stack', 58, 24), p('city-covered-crate', 58, 25),
          p('ember-props-barrel-v02', 48, 38), p('city-basket', 50, 38), p(BUCKET, 58, 38), p('ember-props-crate-v02', 48, 31), p(CRATE, 58, 31)]
    # the corridor (rows 18-22): braziers at the ends, benches against the walls, barrels
    f += [p('light-brazier-bowl', 1, 20), p('light-brazier-bowl', 58, 20), p(BENCH, 11, 18), p(BENCH, 24, 22), p(BENCH, 36, 18),
          p(BENCH, 47, 22), p('ember-props-barrel-v02', 1, 18), p('ember-props-barrel-v02', 58, 22), p('city-firewood-pile', 1, 22),
          p('city-barrels', 58, 18), p(PBENCH, 29, 18), p(PBENCH, 45, 18)]
    npcs = [
        {'name': 'Tomas, o Febril', 'x': 5, 'y': 3}, {'name': 'Dorminhoco', 'x': 16, 'y': 3}, {'name': 'Nara', 'x': 21, 'y': 8},
        {'name': 'Dona Anselma', 'x': 28, 'y': 7}, {'name': 'Garrick, o Caçador', 'x': 41, 'y': 4}, {'name': 'Erasmo', 'x': 33, 'y': 10},
        {'name': 'Berta da Cozinha', 'x': 8, 'y': 34}, {'name': 'Nessa', 'x': 6, 'y': 29},
        {'name': 'O Peregrino', 'x': 18, 'y': 33}, {'name': 'Ada', 'x': 17, 'y': 27},
        {'name': 'Homem de Capuz', 'x': 30, 'y': 33},
        {'name': 'Isa, a Camareira', 'x': 14, 'y': 20}, {'name': 'Guarda de Folga', 'x': 51, 'y': 20},
    ]
    return build('estalagem-andar-2009', 'estalagem-andar-2010', 10, cols, rows, runs, windows, doors, doorways, sconces, f, npcs, (4, 20), [], {})


def main():
    made = []
    for make in (salao, andar):
        file, path, reach, decos, walls = make()
        made.append((file, path))
        show(os.path.basename(path), decos, walls, file['draft']['cols'], file['draft']['rows'], file)
        print(f'{os.path.relpath(path, ROOT)}: {len(decos)} pieces, {reach} hexes reachable on foot:',
              dict(Counter(d['id'] for d in decos).most_common()))
    if '--dry' in sys.argv:
        return
    for file, path in made:
        with open(path, 'w', encoding='utf-8') as f:
            json.dump(file, f, ensure_ascii=False)


if __name__ == '__main__':
    main()
