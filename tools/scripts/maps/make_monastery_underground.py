"""O Mosteiro das Cinzas, the two underground levels (2026-10-30; the user: "design the two underground levels of the monastery",
"there are going to be sub solo dungeon levels"; the brief of 2026-10-29: "underground were they not only have dungeons with prisoners
but they also have a lab of sorts for creating horrors and Ember Infused Soldiers such as the chainbound peninents").

  python -I tools/scripts/maps/make_monastery_underground.py [1|2]

Writes content/maps/ember/mosteiro-masmorras<NNN>.json (level -1) and mosteiro-laboratorio<NNN>.json (level -2): the next serials,
never an overwrite. Both are Subterrâneo (environment indoor + underground: no sun, no moon, only the torches, braziers and the
Ember's own glow), fog of war on, free roam until a foe is seen (the monastery kit's walls; the catalog as it is today: the crypt /
mines kits are gone, so the dungeon dresses in the torture, cave, dungeon, ember and Poly Haven pieces).

Masmorras (44 x 34), under the sacristy: the ground floor's crypt stair lands in the stair hall (NW). Band A (y 2-9): the stair hall
(x 2-12), the warden's room (x 14-24: his desk, the keys, the second ledger's twin, a chest), the store (x 26-42: the prisoners'
belongings in crates, barrels, the smith's hammer). Band B (y 11-13): the corridor, two guards and a war dog. Band C (y 15-21): five
cells (x 2-20, y 15-17) behind LOCKED stone doors opening south onto the aisle (y 19-21), the torture room east of the aisle (x 22-30:
the pillory, the wall shackles, the brazier, the Carrasco), the cistern (x 32-42: the underground pool, bones). Band D (y 23-31): the
penitents' pit (x 2-20: chained pillars, cages, three Penitentes and their keeper) and, behind a LOCKED stone door from the cistern,
the descent (x 22-42: the stairs down to the laboratory, its guard). Prisoners in the cells talk: Tomé, the smith of Vau, the
woodcutter, Irmão Davi (the brother who asked). The fifth cell holds bones.
Laboratório (38 x 30), the bottom: the arrival room (x 2-10, y 2-8) with the stairs up; the master's study (x 12-24: books, skulls,
the pedestal) and the specimen store (x 26-36: cages, a giant beetle, jars); the great infusion hall (x 2-36, y 10-21: rows of Ember
containment tanks between pillars, the altar with the reliquary, blue crystals and geodes, horrors loose); the pens (x 2-18, y 23-28:
penitents being made, the Ash golem that guards them); the Mestre Infusor's chamber (x 20-36, y 23-28) behind a LOCKED stone door.
Placeholder monsters (the user: "tem um monte de monstros nunca usados para colocar no laboratorio depois troco"): minorHorror, horror,
emberedWraith, apparition, ancientGolem, giantBeetle, theButcher; the guards cultist / cultistV2 / chainBoundPenitent / wardog2.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from monastery_lib import (DOOR, LOCKED, PORTAL, STAIRS_DOWN, STAIRS_UP, Floor, L, R, base_draft, hwall, ring, tree, vwall)  # noqa: E402

UNDER = dict(fog=True, timeOfDay='night', environment='indoor', underground=True, baseTile='nave')
CANDLE, CHAINS, CAGE, PILLAR = 'light-candle', 'ember-dungeon-chains', 'ember-dungeon-iron-cage', 'ember-dungeon-pillar-v02'
BRAZIER, WALLBRAZIER, TORCHSTAND = 'torture-brazier', 'cave-wall-brazier', 'cave-candle-stand'
BONES, OSSUARY, RUBBLE = 'dungeon-bones', 'dungeon-ossuary', 'dungeon-stone-rubble'
TANK, EMBERLAMP, RELIQUARY = 'ember-ember-containment-box', 'ember-ember-ember-lantern', 'ember-ember-ancient-reliquary'


# =================================================================================================== Masmorras (-1)
def masmorras():
    f = Floor('mosteiro-masmorras', 44, 34, 'nave')
    f.walls |= ring(1, 1, 43, 33)
    f.walls |= hwall(10, 1, 43) | hwall(14, 1, 43) | hwall(22, 1, 43)          # the four bands
    f.walls |= vwall(13, 1, 10) | vwall(25, 1, 10)                              # band A: stair hall | warden | store
    for x in (5, 9, 13, 17, 21):                                                 # band C: the cells' partitions (y 15-17)
        f.walls |= vwall(x, 14, 18)
    f.walls |= hwall(18, 1, 21)                                                  # the cells' south wall (their doors)
    f.walls |= vwall(31, 14, 22)                                                 # torture room | cistern
    f.walls |= vwall(21, 22, 33)                                                 # band D: the pit | the descent
    for piece, x, y in ((PORTAL, 6, 10), (DOOR, 18, 10), (PORTAL, 33, 10),      # band A onto the corridor
                        (PORTAL, 26, 14), (PORTAL, 36, 14),                      # the corridor into the torture room / the cistern
                        (LOCKED, 2, 18), (LOCKED, 6, 18), (LOCKED, 10, 18), (LOCKED, 14, 18), (LOCKED, 18, 18),  # the five cells
                        (PORTAL, 10, 22),                                        # the aisle down into the pit
                        (LOCKED, 36, 22)):                                       # the cistern down into the descent
        f.opening(piece, x, y)
    f.torches = {(3, 1), (9, 1), (16, 1), (22, 1), (28, 1), (34, 1), (40, 1), (1, 5), (43, 5), (13, 5), (25, 5),
                 (3, 10), (11, 10), (22, 10), (29, 10), (39, 10), (1, 12), (43, 12),
                 (23, 14), (30, 14), (33, 14), (41, 14), (31, 17), (31, 20), (4, 18), (8, 18), (12, 18), (16, 18), (20, 18),
                 (1, 20), (3, 22), (15, 22), (26, 22), (32, 22), (41, 22), (21, 25), (21, 30), (1, 27), (43, 27), (8, 33), (14, 33), (28, 33), (38, 33)}
    f.lay_walls()
    p = f.place
    # --- band A ---
    f.stairs(STAIRS_UP, 6, 5, 'mosteiro', 'up', back=True)                     # the crypt stair: back up to the sacristy
    p(RUBBLE, 2, 2); p(OSSUARY, 11, 2); p(BRAZIER, 3, 8); p(BRAZIER, 11, 8); p('cave-hanging-chains', 9, 2); p(CANDLE, 2, 7)
    p('ember-polyhaven-woodentable-01', 18, 5); p('ember-polyhaven-woodenchair-01', 17, 5); p('cave-rusty-keys', 20, 5)        # the warden
    p('ember-treasure-iron-banded-chest', 24, 2); p('ember-polyhaven-gothicbed-01', 14, 2); p('ember-polyhaven-wooden-bookshelf-worn', 22, 2)
    p('ember-interior-table-candelabra', 19, 7); p('ember-polyhaven-wine-barrel-01', 24, 8); p('ember-interior-worn-rug', 18, 4); p('cave-rusty-lantern', 15, 7)
    p('ember-polyhaven-wooden-barrels-01', 27, 2); p('ember-polyhaven-wooden-crate', 30, 2); p('ember-polyhaven-crate-chest', 33, 2)   # the store
    p('ember-props-sacks-v02', 36, 2); p('ember-props-crate-v02', 39, 2); p('ember-props-barrel-v02', 41, 3); p('ember-polyhaven-wooden-crate', 42, 6)
    p('ember-polyhaven-ornate-war-hammer', 28, 6); p('ember-polyhaven-kite-shield', 31, 6); p('ember-polyhaven-wooden-axe', 34, 6)      # the smith's hammer, the lumbermen's axe
    p('ember-polyhaven-wooden-bucket', 38, 7); p('ember-treasure-ceramic-jars', 41, 8); p('ember-props-sacks-v02', 27, 8); p(CANDLE, 36, 5)
    # --- band B: the corridor ---
    p(BRAZIER, 2, 12); p(BRAZIER, 42, 12); p('ember-interior-long-bench-v02', 20, 11); p(TORCHSTAND, 32, 13)
    # --- band C: the cells (x 2-4, 6-8, 10-12, 14-16, 18-20; y 15-17) ---
    for cx in (2, 6, 10, 14, 18):
        p('ember-props-sacks-v02', cx, 15)                                       # the straw
    p('ember-polyhaven-wooden-bucket-02', 4, 17); p(CHAINS, 8, 15); p('ember-polyhaven-wooden-bucket', 12, 17); p(CHAINS, 16, 15)
    p(BONES, 19, 16); p(OSSUARY, 18, 17); p('cave-hanging-chains', 20, 17)      # the fifth cell: whoever was there
    p(CANDLE, 23, 19); p(RUBBLE, 2, 21)                                          # the aisle
    # the torture room x 22-30, y 15-21
    p('torture-pillory', 25, 15); p('torture-wall-shackles', 29, 15); p(BRAZIER, 23, 16); p('torture-chain-pile', 29, 17)
    p('wilds-iron-cage', 28, 20); p('ember-treasure-workbench-v02', 24, 20); p(BONES, 29, 21); p('cave-hanging-chains', 26, 18)
    # the cistern x 32-42, y 15-21
    p('cave-underground-pool', 36, 17); p('cave-stalagmites', 33, 20); p('cave-stalactite', 41, 15); p(BONES, 41, 20); p(RUBBLE, 33, 15)
    p('cave-glowing-mushrooms', 41, 19); p(TORCHSTAND, 33, 17)
    # --- band D: the penitents' pit x 2-20, y 23-31 ---
    p('wilds-chained-pillar', 6, 25); p('wilds-chained-pillar', 14, 25); p('wilds-chained-pillar', 10, 29)
    p('dungeon-hanging-cage', 3, 30); p(CAGE, 18, 30); p(CAGE, 19, 23); p('torture-chain-pile', 2, 23); p(BRAZIER, 10, 26)
    p(CHAINS, 16, 28); p(BONES, 4, 28); p('ember-dungeon-bone-pile', 12, 31); p('torture-wall-shackles', 6, 31)
    # the descent x 22-42, y 23-31
    f.stairs(STAIRS_DOWN, 36, 28, 'mosteiro-laboratorio', 'down')
    p(BRAZIER, 23, 24); p(BRAZIER, 41, 24); p('dungeon-penitent-statue', 24, 30); p('dungeon-penitent-statue', 41, 30)
    p(EMBERLAMP, 30, 26); p('cave-blue-crystals', 42, 27); p('dungeon-funerary-urn', 23, 31); p(PILLAR, 28, 24); p(PILLAR, 28, 31)

    TOME = tree('npc-masmorras-tome', 'Tomé', [
        L('l1', '...quem? Vocês não são deles. Não são, né? Me tira daqui. Minha mãe... ela está na estrada, ela ia me esperar.', replies=[
            R('Sua mãe está no portão. Ela nos mandou.', 'l2'), R('O que fazem com vocês aqui?', 'l3'), R('Aguente. Voltamos.', None)]),
        L('l2', 'Ela está viva... Então eu aguento. Diz pra ela que eu não rezei pra Santa nenhuma. Que eu gritei o nome dela.', None),
        L('l3', 'Levam um por vez. Pela escada que desce, lá do fundo, depois da água. Quem volta, volta de ferro, com os olhos cheios de brasa, e '
                'não conhece mais ninguém. O ferreiro foi ontem. Me disseram que amanhã sou eu.', None),
    ])
    FERREIRO = tree('npc-masmorras-ferreiro', 'Ferreiro de Vau', [
        L('l1', 'Olha só. Visita. O carcereiro não avisou. ...Levaram meu martelo pro depósito, viram? Um martelo que fez trezentas ferraduras. '
                'Agora fazem correntes com ele.', replies=[R('Correntes para quem?', 'l2'), R('Vamos tirá-lo daqui.', 'l3')]),
        L('l2', 'Pros penitentes. Eu sei o peso certo: dois elos por braço, um por pescoço. Eu ouço eles forjando a noite toda lá embaixo. Não é '
                'ferro só. Tem brasa dentro. Ember. Eu sei o cheiro.', None),
        L('l3', 'Então abram o poço primeiro. Os três acorrentados lá eram gente que eu conhecia. Se eu sair, saio com o martelo na mão.', None),
    ])
    LENHADOR = tree('npc-masmorras-lenhador', 'Lenhador', [
        L('l1', 'Meu irmão... levaram meu irmão primeiro. Eu ouvi ele lá embaixo. Duas noites. Depois parou.', replies=[
            R('O que você ouviu?', 'l2'), R('Fique quieto. Vamos descer.', None)]),
        L('l2', 'Um canto. Depois ele gritando o meu nome. Depois um sino de ferro, três vezes. Depois ele não era mais ele. Eu reconheço a '
                'voz do meu irmão, moço. Aquilo não era a voz dele.', None),
    ])
    DAVI = tree('npc-masmorras-davi', 'Irmão Davi', [
        L('l1', 'Eu perguntei. Só isso. Eu perguntei ao Prior o que descia pela sacristia. E agora estou aqui, com os que não perguntam.', replies=[
            R('O que desce pela sacristia?', 'l2'), R('O Noviço lá em cima falou de você.', 'l3')]),
        L('l2', 'O Abade. Toda lua nova. Com a Santa. Não a estátua: o que está no sarcófago da capela do topo. Ele leva o coração dela para o '
                'laboratório e volta sem ele. E os fornos acendem.', None),
        L('l3', 'O Trêmulo? Então ainda tem um que lembra. Digam a ele que eu não voltei de olhos de cinza. Ainda não. Por favor, ainda não.', None),
    ])
    f.person('Tomé', 'peasant1', 3, 16, 2, TOME)
    f.person('Ferreiro de Vau', 'woodsman', 7, 16, 4, FERREIRO)
    f.person('Lenhador', 'charcoalBurner', 11, 16, 2, LENHADOR)
    f.person('Irmão Davi', 'cultist', 15, 16, 3, DAVI)
    # the guards (enemies: the fight starts when the party sees them)
    f.person('Carcereiro', 'cultistV2', 20, 6, 12, enemy=True)
    f.person('Guarda da Cinza', 'cultist', 14, 12, 8, enemy=True)
    f.person('Guarda da Cinza', 'cultist', 30, 12, 8, enemy=True)
    f.person('Cão de guerra', 'wardog2', 28, 13, 6, enemy=True)
    f.person('Carrasco', 'cultistV2', 26, 17, 12, enemy=True)
    f.person('Guarda da Cinza', 'cultist', 38, 19, 8, enemy=True)
    f.person('Guardião do Poço', 'cultistV2', 9, 24, 12, enemy=True)
    f.person('Penitente Acorrentado', 'chainBoundPenitent', 5, 27, 9, enemy=True)
    f.person('Penitente Acorrentado', 'chainBoundPenitent', 13, 29, 9, enemy=True)
    f.person('Penitente Acorrentado', 'chainBoundPenitent', 17, 25, 9, enemy=True)
    f.person('Guardião da Descida', 'cultistV2', 36, 25, 13, enemy=True)
    f.person('Penitente Acorrentado', 'chainBoundPenitent', 30, 28, 10, enemy=True)
    SPAWN = (8, 6)
    f.person('Kael', 'kaelFinal', *SPAWN, 15, player=True)
    seen = f.check(SPAWN, sealed=[(2, 18), (3, 18), (6, 18), (7, 18), (10, 18), (11, 18), (14, 18), (15, 18), (18, 18), (19, 18), (36, 22), (37, 22)],
                   beyond=[(3, 16), (7, 16), (11, 16), (15, 16), (19, 16), (36, 26)])
    return f.write(base_draft(), seen, title='O Mosteiro das Cinzas · Masmorras', place='Sob a sacristia: as celas, a tortura, o poço dos penitentes',
                   briefing='A escada da cripta não desce a uma cripta. Desce a celas trancadas, a uma sala de ferros, a um poço onde os '
                            'penitentes são acorrentados antes de descer mais. Os guardas não recebem peregrinos aqui. Quem está nas celas '
                            'ainda tem a própria voz.',
                   objective='Liberte quem puder e encontre a descida para o laboratório', **UNDER)


# =================================================================================================== Laboratório (-2)
def laboratorio():
    f = Floor('mosteiro-laboratorio', 38, 30, 'nave')
    f.walls |= ring(1, 1, 37, 29)
    f.walls |= hwall(9, 1, 37) | hwall(22, 1, 37)                                # the upper rooms | the hall | the lower rooms
    f.walls |= vwall(11, 1, 9) | vwall(25, 1, 9)                                 # arrival | study | specimen store
    f.walls |= vwall(19, 22, 29)                                                 # the pens | the Mestre's chamber
    for piece, x, y in ((PORTAL, 6, 9), (DOOR, 17, 9), (PORTAL, 30, 9), (PORTAL, 8, 22), (LOCKED, 27, 22)):
        f.opening(piece, x, y)
    f.torches = {(3, 1), (9, 1), (14, 1), (21, 1), (28, 1), (34, 1), (1, 5), (37, 5), (11, 4), (25, 4),
                 (3, 9), (12, 9), (22, 9), (26, 9), (35, 9), (1, 13), (1, 18), (37, 13), (37, 18),
                 (2, 22), (13, 22), (22, 22), (33, 22), (19, 25), (1, 26), (37, 26), (6, 29), (14, 29), (24, 29), (32, 29)}
    f.lay_walls()
    p = f.place
    # --- arrival x 2-10, y 2-8 ---
    f.stairs(STAIRS_UP, 4, 4, 'mosteiro-masmorras', 'up', back=True)
    p(BRAZIER, 2, 8); p(BRAZIER, 10, 8); p(RUBBLE, 2, 2); p('cave-hanging-chains', 8, 2); p(EMBERLAMP, 9, 6)
    # --- the master's study x 12-24, y 2-8 ---
    for x in (12, 14, 16):
        p('ember-polyhaven-wooden-bookshelf-worn', x, 2)
    p('ember-polyhaven-gothiccabinet-01', 23, 2); p('ember-polyhaven-woodentable-01', 18, 5); p('ember-polyhaven-woodenchair-01', 20, 6)
    p('ember-smithsonian-homo-floresiensis-cranium', 17, 5); p('ember-smithsonian-gorilla-cranium', 16, 6); p('ember-interior-table-candelabra', 21, 4)
    p('ember-treasure-iron-banded-chest', 24, 8); p('ember-interior-worn-rug', 17, 7); p(CANDLE, 13, 7); p('ember-polyhaven-brass-candleholder-01', 23, 5)
    p(RELIQUARY, 13, 4)
    # --- the specimen store x 26-36, y 2-8 ---
    p(CAGE, 27, 2); p(CAGE, 30, 2); p('wilds-iron-cage', 36, 3); p('dungeon-hanging-cage', 35, 7); p('ember-treasure-ceramic-jars', 26, 8)
    p('ember-treasure-ceramic-jars', 29, 8); p(BONES, 33, 2); p('ember-dungeon-bone-pile', 36, 5); p('cave-amethyst-geode', 32, 8)
    p('ember-smithsonian-chimpanzee-fibula-right', 28, 5); p('ember-smithsonian-cast-iron-cauldron', 34, 4)
    # --- the great infusion hall x 2-36, y 10-21 ---
    for x in (6, 14, 22, 30):
        p(PILLAR, x, 11); p(PILLAR, x, 20)
    for x in (4, 9, 17, 25, 33):                                                 # the tanks, two rows
        p(TANK, x, 13); p(TANK, x, 18)
    p('ember-dungeon-altar', 18, 15); p(RELIQUARY, 20, 16); p(EMBERLAMP, 16, 14); p(EMBERLAMP, 21, 14); p(EMBERLAMP, 11, 16); p(EMBERLAMP, 27, 16)
    p('cave-blue-crystals', 2, 12); p('cave-blue-crystals', 36, 19); p('cave-amethyst-geode', 35, 11); p('cave-glowing-mushrooms', 2, 20)
    p(CHAINS, 12, 11); p(CHAINS, 24, 20); p('torture-chain-pile', 31, 14); p('ember-treasure-workbench-v02', 7, 16); p('ember-interior-cauldron', 29, 20)
    p(BRAZIER, 2, 16); p(BRAZIER, 36, 15); p(OSSUARY, 2, 10); p(BONES, 33, 21); p('ember-smithsonian-cast-iron-cauldron', 13, 20)
    # --- the pens x 2-18, y 23-28 ---
    p(CAGE, 3, 23); p(CAGE, 7, 23); p(CAGE, 11, 23); p('wilds-chained-pillar', 15, 24); p('torture-wall-shackles', 17, 28); p(CHAINS, 2, 28)
    p(BRAZIER, 9, 28); p(TANK, 5, 27); p(EMBERLAMP, 13, 27); p('ember-dungeon-bone-pile', 17, 23)
    # --- the Mestre Infusor's chamber x 20-36, y 23-28 ---
    p('dungeon-sarcophagus', 28, 24); p('dungeon-penitent-statue', 22, 23); p('dungeon-penitent-statue', 35, 23); p(RELIQUARY, 31, 24)
    p(BRAZIER, 21, 28); p(BRAZIER, 35, 28); p(EMBERLAMP, 24, 26); p(EMBERLAMP, 33, 26); p('ember-polyhaven-wooden-bookshelf-worn', 36, 24)
    p('ember-treasure-iron-banded-chest', 36, 27); p(CHAINS, 20, 26); p('cave-blue-crystals', 30, 28)

    f.person('Devoto Infusor', 'cultistV2', 14, 13, 13, enemy=True)
    f.person('Devoto Infusor', 'cultistV2', 23, 19, 13, enemy=True)
    f.person('Horror Menor', 'minorHorror', 10, 14, 10, enemy=True)
    f.person('Horror Menor', 'minorHorror', 28, 17, 10, enemy=True)
    f.person('Espectro de Brasa', 'emberedWraith', 19, 18, 12, enemy=True)
    f.person('Aparição', 'apparition', 6, 19, 10, enemy=True)
    f.person('Horror', 'horror', 32, 12, 14, enemy=True)
    f.person('Besouro Gigante', 'giantBeetle', 31, 5, 10, enemy=True)
    f.person('Penitente Acorrentado', 'chainBoundPenitent', 4, 25, 10, enemy=True)
    f.person('Penitente Acorrentado', 'chainBoundPenitent', 8, 26, 10, enemy=True)
    f.person('Penitente Acorrentado', 'chainBoundPenitent', 12, 25, 10, enemy=True)
    f.person('Golem de Cinza', 'ancientGolem', 16, 26, 15, enemy=True)
    f.person('Mestre Infusor', 'cultistV2', 28, 26, 18, enemy=True)
    f.person('O Açougueiro', 'theButcher', 33, 25, 15, enemy=True)
    f.person('Horror', 'horror', 24, 24, 14, enemy=True)
    SPAWN = (6, 5)
    f.person('Kael', 'kaelFinal', *SPAWN, 15, player=True)
    seen = f.check(SPAWN, sealed=[(27, 22), (28, 22)], beyond=[(28, 25)])
    return f.write(base_draft(), seen, title='O Mosteiro das Cinzas · Laboratório', place='O fundo: onde a Cinza refaz o que se perde',
                   briefing='Abaixo das masmorras, o salão de infusão: tanques de Ember entre pilares, um altar com o relicário, horrores '
                            'soltos entre as cubas. Nos cercados, os próximos penitentes; atrás da porta de pedra, o Mestre Infusor e o que ele '
                            'chama de Santa.',
                   objective='Atravesse o laboratório e derrote o Mestre Infusor', **UNDER)


if __name__ == '__main__':
    only = sys.argv[1:]
    if not only or '1' in only:
        masmorras()
    if not only or '2' in only:
        laboratorio()
