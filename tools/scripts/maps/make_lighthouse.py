"""O Farol (the Light House), five maps (2026-11-11; the user: "create a new dungeon the Light House complete with new 3D Objects,
underground levels and a boss talk scene at the top of the lighthouse"; picked: the Embered Wraith as the boss, a film cutscene and
then the fight, five maps).

  python -I tools/scripts/maps/make_lighthouse.py [ground|2|top|cellars|cave ...]

Writes the next serial of each id in content/maps/ember (never an overwrite), on the Mosteiro's builder (monastery_lib.py) with the
Light House's own wall kit (`ember-arch-farol-*`) and its 3D kit (`ember-lighthouse-*`):

  farol          the headland: the sea north and west, the dock (boats, nets, pots, anchors), the keeper's cottage, the fog bell, the
                 round tower's ground floor (the hall; the stair up; the LOCKED door to the cellar stair). 48 x 42, under the sky.
  farol-andar-2  Floor II: the keeper's room, the oil store (the apprentice), the machine hall with the clockwork. 26 x 25.
  farol-topo     the lantern room: the great lamp burning Ember, O Faroleiro (the Embered Wraith) and his drowned watch. A battle
                 map: the scenes farol-01 (opening) and farol-02 (victory) play here (content/cutscenes). 22 x 21.
  farol-poroes   level -1, the cellars: stair hall, oil cellar, the wreckers' LOCKED store, the cistern gallery, the hold with the
                 castaway, the LOCKED descent. 36 x 26, Subterrâneo.
  farol-gruta    level -2, the sea cave under the cliff: the inlet, the wreckers' landing, the hoard and what guards it. 42 x 30,
                 Subterrâneo, no built walls: rock (void) and water.
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from monastery_lib import Floor, L, R, base_draft, hwall, ring, tree, vwall  # noqa: E402

KIT = 'farol'
LOC = 'lighthouse'
NIGHT = dict(timeOfDay='brightNight', sunIntensity=0.6, ambientIntensity=0.95)
INDOOR = dict(fog=True, environment='indoor', aboveGround=True, baseTile='nave', emberTiles='torre-v01', locationId=LOC, **NIGHT)
UNDER = dict(fog=True, timeOfDay='night', environment='indoor', underground=True, locationId=LOC)
LAMP, CLOCK, TANK, BELL = 'ember-lighthouse-lamp', 'ember-lighthouse-clockwork', 'ember-lighthouse-oil-tank', 'ember-lighthouse-fog-bell'
BOAT, ANCHOR, POST, NET, POTS = ('ember-lighthouse-rowboat', 'ember-lighthouse-anchor', 'ember-lighthouse-mooring-post',
                                 'ember-lighthouse-net-rack', 'ember-lighthouse-lobster-pots')
CANDLE, BRAZIER, BARREL, CRATE, SACKS = 'light-candle', 'torture-brazier', 'ember-props-barrel-v02', 'ember-props-crate-v02', 'ember-props-sacks-v02'
BARRELS, WCRATE, CHEST, IRONCHEST = 'ember-polyhaven-wooden-barrels-01', 'ember-polyhaven-wooden-crate', 'ember-polyhaven-treasure-chest', 'ember-treasure-iron-banded-chest'
EMBERLAMP, BONES, RUBBLE = 'ember-ember-ember-lantern', 'cave-bones', 'cave-stone-rubble'


def floor(map_id, cols, rows, base='nave'):
    f = Floor(map_id, cols, rows, base, KIT)
    f.S = lambda role: f.k[role]
    return f


def hex_neighbours(x, y):
    odd = y & 1
    return ((x + 1, y), (x - 1, y), (x + odd, y - 1), (x + odd - 1, y - 1), (x + odd, y + 1), (x + odd - 1, y + 1))


def disc(cx, cy, radius):
    """The round tower's plan (the user, 2026-11-11: "you need to make round walls for this one, only square walls go underground"):
    the hexes whose centres lie within `radius` units of the centre hex's (odd-r: a column is sqrt(3) wide, a row 1.5 deep) and its
    wall, the ring of them with a neighbour outside: a one-hex chain in which every piece has exactly two neighbours, so the kit's
    round wall (ArchitectureDef::round) draws it as one smooth curve."""
    def centre(x, y):
        return (x + 0.5 * (y & 1)) * math.sqrt(3), y * 1.5
    c = centre(cx, cy)
    reach = int(radius) + 2
    inside = {(x, y) for y in range(cy - reach, cy + reach + 1) for x in range(cx - reach, cx + reach + 1) if math.dist(centre(x, y), c) <= radius}
    wall = {h for h in inside if any(n not in inside for n in hex_neighbours(*h))}
    for (x, y) in wall:  # the engine's joins (MapStage: the four grid neighbours, a diagonal one when no wall stands on either side of the step)
        joins = sum((a, b) in wall for a, b in ((x - 1, y), (x + 1, y), (x, y - 1), (x, y + 1)))
        joins += sum((a, b) in wall and (a, y) not in wall and (x, b) not in wall for a, b in ((x - 1, y - 1), (x + 1, y - 1), (x - 1, y + 1), (x + 1, y + 1)))
        if joins != 2:
            raise SystemExit(f'round tower at {cx},{cy} radius {radius}: the wall piece at {x},{y} would have {joins} neighbours')
    return inside, wall


def tower(f, cx, cy, radius, outside=None):
    inside, wall = disc(cx, cy, radius)
    if outside:
        f.tiles = [outside] * (f.cols * f.rows)
    for (x, y) in inside:
        f.put(x, y, 'nave')
    f.walls |= wall
    f.round |= wall
    return inside, wall


# ============================================================================================================ the headland
def ground():
    f = floor('farol', 48, 42, 'plains')
    p = f.place
    # the sea north and west, the rock shore along it, the stone path from the road to the tower's door and the dock
    for y in range(f.rows):
        for x in range(f.cols):
            north = 3.2 + 1.6 * math.sin(x * 0.45) + 0.8 * math.sin(x * 1.3)
            west = 4.2 + 1.8 * math.sin(y * 0.38 + 1) + 0.7 * math.sin(y * 1.1)
            if y < north or x < west or (x + y) < 11:
                f.put(x, y, 'water')
            elif y < north + 2 or x < west + 2.4 or (x + y) < 14:
                f.put(x, y, 'ruins')
    f.rect(25, 26, 26, 41, 'nave')
    f.rect(8, 27, 26, 28, 'nave')
    f.rect(27, 30, 40, 31, 'nave')
    f.rect(6, 14, 11, 31, 'ruins')
    for x, y in ((33, 5), (34, 5), (38, 9), (39, 9), (40, 10), (14, 34), (15, 34), (15, 35), (42, 20), (43, 20), (43, 21), (12, 10), (13, 10)):
        f.put(x, y, 'hill')
    # the tower: round, its ground floor cut by a wall into the hall (south) and the two stair rooms (north)
    tower(f, 26, 16, 14.3)
    f.walls |= hwall(13, 18, 33) | vwall(26, 7, 13)
    # the keeper's cottage east of the path
    f.walls |= ring(34, 23, 43, 29)
    f.rect(35, 24, 42, 28, 'nave')
    for piece, x, y in ((f.S('DOOR'), 25, 25), (f.S('PORTAL'), 21, 13), (f.S('LOCKED'), 29, 13), (f.S('DOOR'), 38, 29)):
        f.opening(piece, x, y)
    # windows only where the round wall runs straight (the top row); the round pieces carry the torches
    f.windows = {(24, 7), (28, 7), (36, 23), (41, 23), (34, 26), (43, 26)}
    f.torches = {(24, 13), (32, 13), (20, 22), (32, 22), (18, 16), (34, 16), (26, 10), (36, 29), (41, 29)}
    f.lay_walls()
    # --- inside the tower ---
    f.stairs(f.S('STAIRS_UP'), 22, 10, 'farol-andar-2', 'up')
    f.stairs(f.S('STAIRS_DOWN'), 28, 10, 'farol-poroes', 'down')
    p(CANDLE, 21, 11); p(BARREL, 24, 9); p(RUBBLE, 27, 8); p('cave-hanging-chains', 31, 11)
    p(TANK, 20, 16); p(ANCHOR, 32, 16); p('ember-tavern-table-v02', 25, 18); p('ember-tavern-chair-v04', 24, 18); p('ember-polyhaven-wooden-bookshelf-worn', 30, 14)
    p(BRAZIER, 21, 21); p(BRAZIER, 31, 21); p(CRATE, 20, 19); p(BARRELS, 32, 19); p('ember-interior-worn-rug', 25, 21); p('ember-polyhaven-wooden-ladder-02', 22, 14)
    p('ember-polyhaven-lantern-01', 27, 18); p(SACKS, 23, 23); p('ember-polyhaven-wooden-bucket', 29, 23)
    # --- the cottage ---
    p('ember-interior-simple-bed-v02', 35, 24); p('ember-tavern-table-small', 39, 25); p('ember-polyhaven-wooden-stool', 41, 25); p('ember-interior-iron-pot', 42, 24)
    p('ember-interior-pantry-shelf', 38, 24); p(CANDLE, 37, 27); p('ember-interior-worn-rug', 39, 27); p('ember-polyhaven-wooden-bucket-02', 42, 28)
    # --- the dock on the west shore ---
    p(BOAT, 7, 16); p(BOAT, 8, 22); p(POST, 6, 19); p(POST, 6, 25); p(POST, 10, 14); p(NET, 9, 18); p(NET, 9, 25); p(POTS, 11, 21); p(POTS, 7, 29)
    p(POTS, 11, 16); p(ANCHOR, 11, 24); p(BARREL, 10, 29); p(WCRATE, 11, 30); p(CRATE, 8, 30); p('ember-polyhaven-wooden-bucket', 6, 22); p(SACKS, 11, 27)
    p('wilds-lantern-post', 12, 27); p('wilds-lantern-post', 24, 30)
    # --- the cliff: the fog bell, rocks along the shore, dead trees, the graves by the cottage ---
    p(BELL, 38, 11); p('ember-polyhaven-coast-rocks', 15, 6); p('ember-polyhaven-coast-rocks', 43, 6); p('ember-polyhaven-coast-rocks', 8, 11)
    p('ember-polyhaven-coast-rocks', 37, 6); p('ember-polyhaven-coast-rocks', 10, 36); p('ember-polyhaven-boulder-01', 14, 12); p('ember-polyhaven-namaqualand-boulder', 40, 16)
    p('ember-polyhaven-rock-moss-set-02-rock08', 17, 9); p('ember-polyhaven-rock-moss-set-01-rock03', 44, 12); p('ember-polyhaven-boulder-01', 16, 37)
    p('dead-tree', 15, 22); p('dead-tree', 41, 36); p('dead-tree', 18, 31); p('wilds-dead-sapling', 44, 32); p('wilds-dry-bush', 31, 34); p('wilds-dry-bush', 14, 39)
    p('wilds-dry-bush', 36, 18); p('wilds-grave-mound', 44, 24); p('wilds-grave-mound', 44, 27); p('wilds-broken-cross', 46, 25); p('wilds-gothic-gravestone', 46, 28)
    p('wilds-abandoned-cart', 29, 37); p('ember-polyhaven-rusted-spade', 45, 30); p('wilds-broken-wheel', 21, 36)
    for x, y, g in ((20, 33, 'tall-a'), (30, 28, 'mid-b'), (33, 36, 'large-a'), (17, 25, 'tall-c'), (38, 33, 'mid-a'), (22, 39, 'large-c'), (45, 17, 'tall-b'), (36, 14, 'mid-c')):
        p(f'ember-polyhaven-grass-medium-01-{g}', x, y)

    MARTA = tree('npc-farol-marta', 'Marta', [
        L('l1', 'Não suba. Ninguém que sobe desce igual. ...O barco do meu homem seguiu aquela luz, há três luas. A luz estava no lugar errado.', replies=[
            R('No lugar errado?', 'l2'), R('Quem acende o farol?', 'l3'), R('Vamos subir mesmo assim.', None)]),
        L('l2', 'Um farol diz onde a pedra está. Este chama. A luz anda de noite, de um lado para o outro da torre, e os barcos vão atrás '
                'dela como mariposa. De manhã o mar devolve a madeira. Os homens, não. Esses voltam andando.', None),
        L('l3', 'O Anselmo. Faroleiro por quarenta anos. Enterramos o Anselmo no inverno passado, ali, ao lado da casa. A luz acendeu de novo '
                'na mesma noite. Laranja. Farol nenhum queima laranja.', None),
    ])
    RUBEN = tree('npc-farol-pescador', 'Velho Pescador', [
        L('l1', 'Feche a porta. O cheiro de maresia não é o problema: é o que vem com ela. Eu fico aqui porque sei quais tábuas rangem.', replies=[
            R('O que tem debaixo da torre?', 'l2'), R('A porta do porão está trancada.', 'l3'), R('Fique aqui dentro.', None)]),
        L('l2', 'Porão de óleo, cisterna, e mais embaixo a gruta. Maré cheia, entra bote. Antes do Anselmo era ninho de saqueador: apagavam o '
                'farol, acendiam um falso na pedra, e catavam a carga de manhã. O Anselmo acabou com isso. Agora é o farol que saqueia.', None),
        L('l3', 'Trancada por dentro, e não fui eu. Quem tem gazua abre. Mas escute: o que sobe daquela escada sobe molhado, e não sobe para '
                'conversar. Se for descer, desça com fogo.', None),
    ])
    f.person('Marta', 'villagerF1', 27, 38, 2, MARTA)
    f.person('Velho Pescador', 'swampFerryman', 40, 27, 5, RUBEN)
    f.person('Afogado', 'zombie2', 9, 20, 9, enemy=True)
    f.person('Afogado', 'zombie2', 10, 26, 9, enemy=True)
    f.person('Cão da Maré', 'zombieDog', 13, 18, 8, enemy=True)
    f.person('Cão da Maré', 'zombieDog', 36, 12, 8, enemy=True)
    f.person('Aparição do Sino', 'apparition', 40, 12, 10, enemy=True)
    f.person('Afogado', 'zombie2', 24, 16, 10, enemy=True)
    f.person('Afogado', 'zombie2', 28, 20, 10, enemy=True)
    SPAWN = (25, 40)
    f.person('Kael', 'kaelFinal', *SPAWN, 15, player=True)
    seen = f.check(SPAWN, sealed=[(29, 13), (30, 13)], beyond=[(28, 11), (29, 11), (30, 11), (28, 10)])
    for must in ((8, 20), (39, 26), (37, 12), (22, 11)):
        if must not in seen:
            raise SystemExit(f'farol: {must} cannot be reached from the road')
    return f.write(base_draft(), seen, title='O Farol', place='O promontório do Farol: o cais, a casa do faroleiro, a torre',
                   briefing='O farol da costa de Mordávia voltou a acender na noite em que enterraram o faroleiro. A luz é laranja e anda. Os '
                            'barcos que a seguem dão nas pedras, e as tripulações voltam à praia andando. A torre está aberta; o porão, trancado '
                            'por dentro.',
                   objective='Atravesse o promontório, entre na torre e encontre a escada para o topo',
                   fog=False, environment='indoor', aboveGround=True, baseTile='plains', locationId=LOC, **NIGHT)


# ============================================================================================================ Floor II
def andar2():
    f = floor('farol-andar-2', 26, 25)
    p = f.place
    tower(f, 13, 12, 14.3, 'void')
    f.walls |= hwall(11, 5, 20) | vwall(13, 3, 11)
    for piece, x, y in ((f.S('PORTAL'), 8, 11), (f.S('DOOR'), 16, 11)):
        f.opening(piece, x, y)
    f.windows = {(11, 3), (15, 3), (11, 21), (14, 21)}
    f.torches = {(7, 11), (11, 11), (19, 11), (13, 7), (5, 13), (20, 13), (7, 18), (19, 18)}
    f.lay_walls()
    # the keeper's room (north-west)
    p('ember-polyhaven-gothicbed-01', 8, 7); p('ember-polyhaven-gothiccabinet-01', 10, 5); p('ember-tavern-table-small', 7, 9); p('ember-tavern-chair-v04', 9, 9)
    p('ember-polyhaven-wooden-bookshelf-worn', 12, 6); p('ember-interior-worn-rug', 10, 8); p(CANDLE, 7, 10); p('ember-polyhaven-brass-candleholder-02', 12, 10)
    p(IRONCHEST, 11, 4)
    # the oil store (north-east)
    p(TANK, 15, 5); p(TANK, 17, 7); p(BARRELS, 15, 8); p(BARREL, 19, 9); p(WCRATE, 14, 10); p('ember-polyhaven-wooden-bucket', 18, 8); p(SACKS, 14, 4)
    p('ember-polyhaven-lantern-01', 14, 7)
    # the machine hall (south): the clockwork under the lamp, its weights' shaft, the two stairs
    p(CLOCK, 13, 15); p(BRAZIER, 9, 13); p(BRAZIER, 17, 13); p('ember-interior-long-bench-v02', 12, 20); p('ember-treasure-workbench-v02', 17, 15)
    p('cave-hanging-chains', 13, 17); p(CRATE, 8, 15); p('ember-polyhaven-wooden-stool-02', 14, 19); p(EMBERLAMP, 11, 14); p(EMBERLAMP, 15, 14)
    p('ember-treasure-weapon-rack-v02', 7, 14)
    f.stairs(f.S('STAIRS_DOWN'), 9, 17, 'farol', 'down', back=True)
    f.stairs(f.S('STAIRS_UP'), 16, 17, 'farol-topo', 'up')

    APRENDIZ = tree('npc-farol-aprendiz', 'Aprendiz do Faroleiro', [
        L('l1', 'Não abram a porta de cima! ...Vocês estão vivos. Secos. Faz tempo que eu não vejo ninguém seco.', replies=[
            R('O que aconteceu com o faroleiro?', 'l2'), R('O que queima na lanterna?', 'l3'), R('Fique escondido.', None)]),
        L('l2', 'Mestre Anselmo nunca deixou a luz apagar. Nem na tempestade de cinza, quando o óleo acabou. Ele desceu à gruta e voltou com '
                'uma pedra que brilhava. Pôs na lanterna. A luz ficou laranja. Ele ficou... magro. Depois ficou claro. Depois eu enxergava '
                'a parede através dele.', None),
        L('l3', 'Ember. E ele. Os dois queimam juntos. A corda do relógio eu ainda dou, toda noite, porque se a lente parar ele desce para '
                'ver por quê. Se forem subir: ele não apaga, e não deixa apagar. Quebrem a lanterna e ele não tem onde ficar.', None),
    ])
    f.person('Aprendiz do Faroleiro', 'peasant1', 16, 6, 3, APRENDIZ)
    f.person('Vigia Afogado', 'apparition', 11, 16, 11, enemy=True)
    f.person('Vigia Afogado', 'apparition', 15, 19, 11, enemy=True)
    f.person('Afogado', 'zombie2', 13, 13, 10, enemy=True)
    f.person('Horror Menor', 'minorHorror', 10, 7, 11, enemy=True)
    SPAWN = (10, 18)
    f.person('Kael', 'kaelFinal', *SPAWN, 15, player=True)
    seen = f.check(SPAWN)
    return f.write(base_draft(), seen, title='O Farol · Andar II', place='O quarto do faroleiro, o depósito de óleo e a sala das máquinas',
                   briefing='O andar onde o faroleiro vivia. A cama está feita. O relógio que gira a lente ainda tem corda: alguém a dá toda '
                            'noite. Do teto desce uma luz laranja pelas frestas do assoalho, e ela não treme como fogo.',
                   objective='Encontre a escada para a sala da lanterna', **INDOOR)


# ============================================================================================================ the lantern room
def topo():
    f = floor('farol-topo', 22, 21)
    p = f.place
    tower(f, 11, 10, 11.6, 'void')
    f.windows = {(9, 3), (10, 3), (11, 3), (12, 3), (9, 17), (10, 17), (11, 17), (12, 17)}  # the glazing where the wall runs straight
    f.torches = {(5, 8), (17, 8), (5, 12), (17, 12)}
    f.lay_walls()
    p(LAMP, 11, 8)
    p(CLOCK, 8, 7); p(TANK, 14, 7); p(BRAZIER, 6, 11); p(BRAZIER, 16, 11); p(EMBERLAMP, 9, 5); p(EMBERLAMP, 13, 5)
    p('cave-hanging-chains', 6, 8); p('cave-hanging-chains', 16, 8); p(BONES, 15, 13); p(BONES, 7, 13); p('ember-polyhaven-wooden-ladder-02', 14, 5)
    p(BARREL, 8, 5)
    f.stairs(f.S('STAIRS_DOWN'), 10, 16, 'farol-andar-2', 'down', back=True)
    boss = f.person('O Faroleiro', 'emberedWraith', 11, 10, 26, enemy=True)
    boss['holdsPosition'] = True
    f.person('Vigia Afogado', 'apparition', 7, 9, 9, enemy=True)
    f.person('Vigia Afogado', 'apparition', 15, 9, 9, enemy=True)
    f.person('Afogado', 'zombie2', 8, 12, 11, enemy=True)
    f.person('Afogado', 'zombie2', 14, 12, 11, enemy=True)
    SPAWN = (11, 14)
    f.person('Kael', 'kaelFinal', *SPAWN, 15, player=True)
    # the whole roster has its hex, off the lane Kael walks in the opening scene ((11, 14) -> (11, 12)): the Cooker's prepare-map puts
    # whoever has no spawn on the nearest free hexes, which were the lane's
    for name, cls, x, y in (('Voss', 'voss', 9, 14), ('Neera', 'neera', 13, 14), ('Salazar', 'salazar', 14, 14), ('Aldric', 'aldric', 8, 14),
                            ('Malrec', 'conjurer', 9, 15), ('Isolde', 'conjurer', 13, 15), ('Selene', 'conjurer', 12, 16)):
        f.person(name, cls, x, y, 15, player=True)
    seen = f.check(SPAWN)
    return f.write(base_draft(), seen, title='O Farol · A Lanterna', place='O topo do Farol: a sala da lanterna',
                   briefing='A sala de vidro no alto da torre. No meio, a grande lente gira sobre o seu relógio, e dentro dela não há chama: há '
                            'Ember. Diante da lanterna, de costas para a porta, o faroleiro olha o mar. Dá para ver o mar através dele.',
                   objective='Derrote O Faroleiro e apague a luz que chama os barcos',
                   win='rout', explore=False, autoTactics=False, music='BossBattleTragicArqTruePersona01-Balanced-Medium.mp3',
                   **dict(INDOOR, fog=False))


# ============================================================================================================ the cellars (-1)
def poroes():
    f = floor('farol-poroes', 36, 26)
    p = f.place
    f.walls |= ring(1, 1, 35, 25) | hwall(9, 1, 35) | hwall(17, 1, 35)
    f.walls |= vwall(12, 1, 9) | vwall(24, 1, 9) | vwall(18, 17, 25)
    for piece, x, y in ((f.S('PORTAL'), 6, 9), (f.S('DOOR'), 17, 9), (f.S('LOCKED'), 29, 9), (f.S('PORTAL'), 8, 17), (f.S('LOCKED'), 27, 17)):
        f.opening(piece, x, y)
    f.torches = {(3, 1), (9, 1), (15, 1), (21, 1), (27, 1), (33, 1), (1, 5), (35, 5), (12, 5), (24, 5), (3, 9), (10, 9), (14, 9), (21, 9), (26, 9), (33, 9),
                 (1, 13), (35, 13), (4, 17), (13, 17), (22, 17), (31, 17), (18, 21), (1, 21), (35, 21), (6, 25), (13, 25), (24, 25), (31, 25)}
    f.lay_walls()
    # band A: the stair hall | the oil cellar | the wreckers' store (locked)
    f.stairs(f.S('STAIRS_UP'), 5, 4, 'farol', 'up', back=True)
    p(RUBBLE, 2, 2); p(BRAZIER, 3, 8); p(BRAZIER, 10, 8); p('cave-hanging-chains', 9, 2); p(BARREL, 11, 3)
    p(TANK, 14, 3); p(TANK, 17, 3); p(TANK, 21, 3); p(BARRELS, 14, 7); p('ember-polyhaven-wine-barrel-01', 22, 7); p(BARREL, 21, 6); p('ember-polyhaven-wooden-bucket-02', 19, 8)
    p('ember-interior-barrel-rack-v02', 18, 6); p(CANDLE, 23, 5)
    p(CHEST, 26, 2); p(IRONCHEST, 29, 2); p('chest-large', 32, 2); p('ember-polyhaven-cannon-01', 26, 5); p(ANCHOR, 33, 5); p(WCRATE, 31, 7); p(CRATE, 34, 8)
    p('ember-polyhaven-kite-shield', 25, 7); p('ember-polyhaven-wooden-handle-saber', 28, 7); p('ember-polyhaven-brass-goblet-02', 30, 4); p(SACKS, 34, 3)
    p('ember-polyhaven-crate-chest', 28, 4); p(EMBERLAMP, 32, 6)
    # band B: the cistern gallery
    p('cave-underground-pool', 17, 13); p('ember-dungeon-pillar-v02', 8, 11); p('ember-dungeon-pillar-v02', 8, 15); p('ember-dungeon-pillar-v02', 27, 11)
    p('ember-dungeon-pillar-v02', 27, 15); p(BRAZIER, 2, 13); p(BRAZIER, 34, 13); p('cave-stalagmites', 13, 15); p('cave-glowing-mushrooms', 22, 11); p(BONES, 31, 15)
    p(POTS, 4, 15); p(NET, 11, 11); p('cave-stalactite', 20, 15); p('ember-polyhaven-wooden-bucket', 15, 11)
    # band C: the hold (west) | the descent (east, locked)
    p('ember-dungeon-iron-cage', 3, 19); p('wilds-iron-cage', 15, 23); p('torture-wall-shackles', 11, 24); p('torture-chain-pile', 2, 23); p(SACKS, 13, 19)
    p(BONES, 16, 20); p(CANDLE, 6, 22); p('ember-interior-simple-bed-v02', 9, 19); p('ember-polyhaven-wooden-bucket-02', 5, 24); p(BARREL, 16, 24)
    f.stairs(f.S('STAIRS_DOWN'), 27, 22, 'farol-gruta', 'down')
    p(BRAZIER, 20, 19); p(BRAZIER, 33, 19); p(POST, 22, 23); p(BOAT, 30, 20); p('cave-blue-crystals', 34, 23); p(RUBBLE, 20, 24); p(EMBERLAMP, 24, 21)

    NAUFRAGO = tree('npc-farol-naufrago', 'Náufrago', [
        L('l1', 'Baixo. Falem baixo. Eles ouvem pela água. ...Eu era o cozinheiro da Andorinha. Batemos na pedra faz seis noites. Eu nadei '
                'para a luz. Burro. Todo mundo nada para a luz.', replies=[
            R('E a tripulação?', 'l2'), R('Como você veio parar no porão?', 'l3'), R('Aguente. A gente volta.', None)]),
        L('l2', 'Lá embaixo, na gruta. Eu vi o capitão levantar da areia com a boca cheia de água e subir a escada sem olhar para mim. Eles '
                'sobem quando a luz passa. Descem quando ela vira. Feito maré.', None),
        L('l3', 'A gruta dá no mar. Subi pela escada, achei esta jaula aberta e fechei por dentro. Jaula é o lugar mais seguro daqui. Atrás '
                'da porta trancada do fundo é a descida. Tem um que fica nela. Ele ainda usa o apito de contramestre.', None),
    ])
    f.person('Náufrago', 'lostCourier', 4, 21, 2, NAUFRAGO)
    f.person('Afogado', 'zombie2', 16, 5, 10, enemy=True)
    f.person('Afogado', 'zombie2', 10, 13, 10, enemy=True)
    f.person('Afogado', 'zombie2', 24, 14, 10, enemy=True)
    f.person('Cão da Maré', 'zombieDog', 21, 13, 9, enemy=True)
    f.person('Lodo da Cisterna', 'corrosiveSlime', 14, 12, 9, enemy=True)
    f.person('Lodo da Cisterna', 'corrosiveSlime', 19, 15, 9, enemy=True)
    f.person('Aranha do Porão', 'giantSpider', 30, 5, 11, enemy=True)
    f.person('Afogado', 'zombie2', 12, 21, 10, enemy=True)
    f.person('Contramestre Afogado', 'zombie', 27, 20, 13, enemy=True)
    f.person('Afogado', 'zombie2', 32, 22, 11, enemy=True)
    SPAWN = (7, 6)
    f.person('Kael', 'kaelFinal', *SPAWN, 15, player=True)
    seen = f.check(SPAWN, sealed=[(29, 9), (30, 9), (27, 17), (28, 17)], beyond=[(29, 5), (27, 21), (28, 21)])
    return f.write(base_draft(), seen, title='O Farol · Porões', place='Sob a torre: o óleo, a cisterna, o depósito dos saqueadores',
                   briefing='O porão do farol é mais velho que a torre. Tanques de óleo vazios, uma cisterna de água parada, e uma porta que '
                            'alguém trancou por dentro: o depósito do que o mar trouxe. No fundo, outra porta trancada e o som da maré '
                            'subindo por uma escada.',
                   objective='Atravesse os porões e encontre a descida para a gruta', baseTile='nave', emberTiles='watchtower-foundations-v01', **UNDER)


# ============================================================================================================ the sea cave (-2)
def gruta():
    f = floor('farol-gruta', 42, 30, 'void')
    p = f.place

    def blob(cx, cy, rx, ry, t='plains'):
        for y in range(f.rows):
            for x in range(f.cols):
                wob = 1 + 0.12 * math.sin(x * 1.7 + y * 0.9) + 0.08 * math.sin(y * 2.3 - x)
                if ((x - cx) / rx) ** 2 + ((y - cy) / ry) ** 2 <= wob and 1 <= x < f.cols - 1 and 1 <= y < f.rows - 1:
                    f.put(x, y, t)

    blob(33, 6, 6, 4)            # the arrival chamber under the cellars
    blob(27, 9, 4, 3)            # the throat down
    blob(21, 15, 13, 7.5)        # the great cavern
    blob(10, 23, 7, 4.5)         # the south-west beach chamber
    blob(15, 20, 4, 3)
    blob(34, 23, 5.5, 4)         # the hoard
    blob(30, 19, 3, 2.5)
    blob(13, 14, 7.5, 4.2, 'snow')   # the wet margin round the inlet
    blob(9, 13, 8.5, 3.6, 'water')   # the inlet from the sea (west)
    for y in range(11, 16):
        for x in range(0, 4):
            f.put(x, y, 'water')
    blob(22, 13, 3, 2, 'nave')       # the wreckers' old paving at the landing
    blob(29, 14, 2.5, 2, 'hill')
    blob(20, 19, 3, 1.6, 'hill')
    blob(34, 24, 3, 2, 'nave')
    f.stairs(f.S('STAIRS_UP'), 33, 4, 'farol-poroes', 'up', back=True)
    p(BRAZIER, 30, 5); p(BRAZIER, 37, 6); p('cave-stalagmites', 36, 8); p(RUBBLE, 30, 8); p('cave-stalactite', 28, 7)
    # the landing: boats drawn up, posts, pots, nets, the loot waiting to go up
    p(BOAT, 18, 12); p(BOAT, 15, 17); p(POST, 19, 14); p(POST, 18, 15); p(ANCHOR, 20, 16); p(POTS, 23, 11); p(POTS, 18, 18); p(NET, 24, 16)
    p(WCRATE, 22, 13); p(BARRELS, 24, 13); p(CRATE, 21, 12); p(BARREL, 25, 12); p(EMBERLAMP, 23, 14); p('cave-rusty-lantern', 21, 15)
    p('cave-basalt-006-shore', 13, 17); p('cave-basalt-006-shore', 17, 10)
    # the cavern
    p('cave-stalagmites', 27, 17); p('cave-blue-crystals', 31, 13); p('cave-glowing-mushrooms', 26, 20); p('cave-boulder', 28, 12); p(BONES, 25, 18)
    p('cave-limestone-spires-001', 22, 20); p('cave-amethyst-geode', 32, 16); p('cave-mossy-rocks', 14, 19); p('cave-rock-pile', 30, 17); p('cave-stalactite', 18, 20)
    p('cave-hanging-bat', 24, 9); p('cave-glowing-mushrooms', 11, 19); p('cave-cracked-stone', 26, 14)
    # the beach chamber
    p(BOAT, 8, 22); p(BONES, 12, 25); p('cave-stalagmites', 6, 24); p(POTS, 11, 21); p('cave-glowing-mushrooms', 14, 24); p('cave-ammonite-fossil', 9, 25)
    p('dungeon-bones', 6, 21); p(ANCHOR, 13, 22)
    # the hoard
    p('chest-large', 34, 22); p(CHEST, 36, 24); p(IRONCHEST, 32, 24); p('cave-webbed-chest', 37, 22); p('ember-polyhaven-cannon-01', 35, 26); p('cave-blue-crystals', 38, 24)
    p('ember-polyhaven-brass-vase-03', 35, 25); p('ember-polyhaven-kite-shield', 31, 22); p(EMBERLAMP, 35, 21); p(BRAZIER, 31, 20)

    f.person('Marinheiro Afogado', 'zombie2', 21, 14, 11, enemy=True)
    f.person('Marinheiro Afogado', 'zombie2', 18, 16, 11, enemy=True)
    f.person('Marinheiro Afogado', 'zombie2', 26, 16, 11, enemy=True)
    f.person('Capitão da Andorinha', 'zombie', 23, 17, 14, enemy=True)
    f.person('Fera da Maré', 'jacare', 16, 16, 12, enemy=True)
    f.person('Fera da Maré', 'jacare', 9, 21, 12, enemy=True)
    f.person('Aranha da Gruta', 'giantSpider', 29, 10, 11, enemy=True)
    f.person('Aranha da Gruta', 'giantSpider', 12, 24, 11, enemy=True)
    f.person('Lodo da Maré', 'corrosiveSlimeLarge', 28, 19, 12, enemy=True)
    f.person('Troll da Gruta', 'troll', 35, 23, 15, enemy=True)
    SPAWN = (33, 6)
    f.person('Kael', 'kaelFinal', *SPAWN, 15, player=True)
    seen = f.check(SPAWN)
    for must in ((34, 24), (9, 23), (20, 13)):
        if must not in seen:
            raise SystemExit(f'farol-gruta: {must} cannot be reached from the stairs')
    return f.write(base_draft(), seen, title='O Farol · A Gruta', place='Sob o penhasco: a enseada dos saqueadores',
                   briefing='A escada do porão acaba em pedra molhada. A gruta abre para o mar por uma boca que só a maré cheia cobre: botes na '
                            'areia, redes, a carga de vinte naufrágios. É daqui que as tripulações sobem quando a luz passa. E foi daqui que o '
                            'faroleiro trouxe a pedra que queima na lanterna.',
                   objective='Limpe a gruta e encontre o tesouro dos saqueadores', baseTile='void', emberTiles='cave-v01', **UNDER)


FLOORS = {'ground': ground, '2': andar2, 'top': topo, 'cellars': poroes, 'cave': gruta}

if __name__ == '__main__':
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    if '--dry' in sys.argv:  # the checks and the picture, no file
        Floor.write = lambda self, base, seen, **fields: (self.show(), print(f'{self.id}: ok, {len(self.decorations)} pieces, {len(seen)} hexes reachable'))
    for key in args or list(FLOORS):
        FLOORS[key]()
