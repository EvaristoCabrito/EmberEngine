"""O Mosteiro das Cinzas, the ground floor (2026-10-29; the user: "start working on the map for a area called the Monastery, its going
to be part exploration / adventure, Talk investigation, part dungeon ... villagers claim people are going missing all too often near
that road, its a spooky place with Ashen Devouts and Penitents, they try to keep the fassade, but with some conversation and
investigations you manage to go underground"; "comece com o mapa use tudo de placeholder mais apropriado que tiver").

  python -I tools/scripts/maps/make_monastery.py

Writes content/maps/ember/mosteiro<NNN>.json (the next serial; never overwrites). Free roam (explore, fog of war, no foe in sight):
the party arrives on the road south of the precinct wall and walks in through the gate. The upper floors: make_monastery_upper.py.

The plan (odd-r hexes, x = column, y = row; 52 x 46), the shared builder in monastery_lib.py:
  precinct   the monastery kit's walls (ember-arch-mosteiro-*, the watchtower's geometry on the gothic stone until the user's
             texture arrives) on x 1 / 50 and y 1 / 41; the 4-hex gate at x 24-27, y 41; the road outside (rows 42-45)
  church     x 19-33, y 3-20: the nave (pews, the aisle, pillars, wall torches), the chancel (altar, the saint's statue, the
             reliquary, braziers, the lectern, the votive rack); its door south (25-26, 20)
  sacristy   x 35-47, y 3-9, door north onto the back alley (40-41, 3); the second ledger's table, chests, jars
  crypt stair x 35-47, y 9-15 (below the sacristy): the LOCKED stone door between them (dungeon-stone-door at 40-41, 9:
             "Arrombar" opens it), the stairs down + the floor connector to the next map (mosteiro-masmorras)
  scriptorium x 4-17, y 3-9, door south (10-11, 9): ancient bookshelves, writing desks, candles
  cloister   x 5-16, y 11-24: pillars round an open garden, the well, dead trees, benches, herb beds
  refectory  x 4-17, y 26-34, door north (10-11, 26): long tables, benches, the hearth, the kitchen corner
  dormitory  x 35-47, y 17-24, door south (40-41, 24): three cells of beds off a corridor; the STAIRS UP to Andar II at its
             east end (45-46, 22)
  infirmary  x 35-47, y 26-34, door north (40-41, 26): beds for the "febris", a screen hiding a ritual table and a cage
  cemetery   x 35-48, y 36-40: graves, fresh mounds, the gravedigger
  penitents' yard x 3-17, y 36-40: the chained pillar, cages, chains; three Penitentes Acorrentados at work
The Devotos (Cultista Ancestral art, the Abbot) and the brothers (placeholder NPC sprites) carry the investigation in their
conversations; the fallen pilgrim in the infirmary, the gravedigger and a penitent let the truth slip.
Blocking: only architecture (walls, doors) blocks; props never (the user, 2026-10-25).
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from monastery_lib import (DOOR, LOCKED, PORTAL, RAMPART_GATE, STAIRS_DOWN, STAIRS_UP, Floor, L, R, base_draft, hwall, ring, tree, vwall)  # noqa: E402

PEW, LECTERN, VOTIVE, SAINT, BELL, DESK = ('ember-mosteiro-pew', 'ember-mosteiro-lectern', 'ember-mosteiro-votive-rack',
                                           'ember-mosteiro-saint-statue', 'ember-mosteiro-bell-cote', 'ember-mosteiro-scriptorium-desk')
f = Floor('mosteiro', 52, 46)

# --- walls --------------------------------------------------------------------------------------------------------------------------
f.walls |= ring(1, 1, 50, 41)            # the precinct: the muralha (openAir pieces: the yard inside stays under the sky)
f.ramparts = ring(1, 1, 50, 41)
f.walls |= ring(19, 3, 33, 20)           # the church
f.walls |= ring(35, 3, 47, 15) | hwall(9, 35, 47)   # sacristy (y 3-9) + crypt stair (y 9-15)
f.walls |= ring(4, 3, 17, 9)             # scriptorium
f.walls |= ring(4, 26, 17, 34)           # refectory
f.walls |= ring(35, 17, 47, 24) | vwall(39, 18, 21) | vwall(43, 18, 21)  # dormitory, the cells' partitions (corridor y 22-23)
f.walls |= ring(35, 26, 47, 34)          # infirmary
for piece, x, y in ((RAMPART_GATE, 25, 41), (DOOR, 25, 20), (PORTAL, 40, 3), (LOCKED, 40, 9), (PORTAL, 10, 9), (DOOR, 10, 26), (PORTAL, 40, 24), (DOOR, 40, 26)):
    f.opening(piece, x, y)
f.windows = {(21, 3), (24, 3), (28, 3), (31, 3), (19, 7), (19, 11), (19, 15), (33, 7), (33, 11), (33, 15), (22, 20), (30, 20),  # church
             (37, 3), (44, 3), (47, 6), (47, 12), (47, 20), (47, 30), (35, 20), (35, 30), (38, 26), (44, 26), (38, 34), (44, 34),
             (7, 3), (14, 3), (4, 6), (17, 6), (7, 9), (14, 9), (7, 26), (14, 26), (4, 30), (17, 30), (7, 34), (14, 34)}
f.slits = {(35, 6), (35, 12), (38, 15), (44, 15), (38, 17), (44, 17), (38, 24), (44, 24), (4, 4), (17, 4), (4, 28), (17, 28)}
f.torches = {(20, 3), (26, 3), (32, 3), (19, 5), (19, 9), (19, 13), (19, 17), (33, 5), (33, 9), (33, 13), (33, 17), (21, 20), (29, 20),  # church
             (36, 3), (46, 9), (36, 15), (46, 15), (36, 9), (42, 9),                 # sacristy / crypt stair
             (5, 3), (10, 3), (16, 3), (5, 9), (15, 9),                              # scriptorium
             (5, 26), (15, 26), (10, 34), (4, 32), (17, 32),                         # refectory
             (36, 17), (41, 17), (46, 17), (37, 24), (46, 24),                       # dormitory
             (36, 26), (46, 26), (36, 34), (46, 34), (41, 34),                       # infirmary
             (20, 41), (31, 41), (1, 20), (50, 20), (1, 38), (50, 38), (10, 41), (40, 41)}  # the precinct, inside faces
f.lay_walls()

# --- floors -------------------------------------------------------------------------------------------------------------------------
f.rect(20, 4, 32, 19, 'nave'); f.rect(36, 4, 46, 14, 'nave'); f.rect(5, 4, 16, 8, 'nave'); f.rect(5, 27, 16, 33, 'nave')
f.rect(36, 18, 46, 23, 'nave'); f.rect(36, 27, 46, 33, 'nave')
f.rect(24, 21, 27, 40, 'nave'); f.rect(24, 42, 27, 45, 'nave')                                         # the path and the road
f.rect(5, 10, 16, 10, 'nave'); f.rect(5, 25, 16, 25, 'nave'); f.rect(5, 11, 5, 24, 'nave'); f.rect(16, 11, 16, 24, 'nave')  # the cloister walk
f.rect(18, 10, 18, 25, 'nave'); f.rect(34, 2, 34, 40, 'nave'); f.rect(34, 2, 48, 2, 'nave'); f.rect(18, 2, 33, 2, 'nave')    # lanes, the back alley
f.rect(6, 12, 15, 23, 'woods'); f.rect(36, 36, 48, 40, 'ruins'); f.rect(3, 36, 17, 40, 'ruins')

# --- props --------------------------------------------------------------------------------------------------------------------------
p = f.place
# the church: nave x 20-32, y 4-19; the chancel y 4-8, the aisle x 25-26
p(SAINT, 26, 4); p('ember-dungeon-altar', 25, 6); p('ember-ember-ancient-reliquary', 29, 5)
p('ember-crypt-ritual-brazier', 22, 5); p('ember-crypt-ritual-brazier', 30, 6)
p(LECTERN, 23, 8); p(VOTIVE, 29, 8); p('wilds-incense-burner', 28, 6); p('light-candle', 24, 5); p('light-candle', 27, 7)
for y in (9, 13, 17):
    p('ember-dungeon-pillar-v02', 20, y); p('ember-dungeon-pillar-v02', 32, y)
for y in (11, 13, 15, 17):
    p(PEW, 21, y); p(PEW, 23, y); p(PEW, 27, y); p(PEW, 29, y)
p('light-candle', 31, 11); p('light-candle', 31, 18); p('city-shrineCandle', 20, 19)
# sacristy x 36-46, y 4-8
p('ember-crypt-ritual-table', 40, 6); p('ember-crypt-ancient-bookshelf', 36, 4); p('ember-crypt-ancient-bookshelf', 37, 4)
p('ember-treasure-iron-banded-chest', 46, 4); p('ember-treasure-ceramic-jars', 46, 8); p('ember-interior-cloth-curtain', 44, 4)
p('light-candle', 38, 7); p('ember-interior-table-candelabra', 42, 5); p('dungeon-funerary-urn', 36, 8)
# crypt stair x 36-46, y 10-14
f.stairs(STAIRS_DOWN, 41, 12, 'mosteiro-masmorras', 'down')
p('ember-dungeon-chains', 36, 13); p('ember-crypt-ritual-brazier', 37, 10); p('ember-crypt-ritual-brazier', 45, 10)
p('ember-crypt-funeral-urns', 46, 14); p('dungeon-penitent-statue', 36, 10); p('dungeon-penitent-statue', 46, 11)
# scriptorium x 5-16, y 4-8
for x in (5, 7, 9, 11, 13, 15):
    p('ember-crypt-ancient-bookshelf', x, 4)
for x, y in ((6, 6), (10, 6), (14, 6), (8, 8), (12, 8)):
    p(DESK, x, y)
p(LECTERN, 16, 7); p('light-candle', 7, 7); p('light-candle', 11, 7); p('light-candle', 15, 5)
p('ember-interior-bookshelf', 5, 8); p('ember-treasure-coiled-rope', 16, 5)
# cloister x 5-16, y 10-25
for x in range(6, 16, 2):
    p('ember-dungeon-pillar-v02', x, 11); p('ember-dungeon-pillar-v02', x, 24)
for y in range(13, 23, 3):
    p('ember-dungeon-pillar-v02', 6, y); p('ember-dungeon-pillar-v02', 15, y)
p('ember-city-well', 10, 17); p('tree-3d-dead-oak-v02', 8, 14); p('dead-tree', 13, 20); p('ember-nature-dry-bush', 12, 14); p('ember-nature-dry-bush', 8, 21)
p('ember-city-bench', 9, 19); p('ember-city-bench', 13, 16); p('wilds-candle-menhir', 11, 21)
p('ember-nature-wild-mushrooms', 7, 18); p('ember-nature-leaf-litter', 13, 13)
# refectory x 5-16, y 27-33
p('ember-tavern-stone-hearth', 5, 27); p('ember-interior-cauldron', 7, 27); p('ember-interior-kitchen-cabinet', 9, 27)
p('ember-treasure-ceramic-jars', 16, 27); p('ember-props-sacks-v02', 16, 28); p('ember-props-barrel-v02', 16, 33)
for y in (30, 32):
    p('ember-tavern-table-messhall', 8, y); p('ember-tavern-table-messhall', 13, y)
p('ember-interior-long-bench-v02', 7, 29); p('ember-interior-long-bench-v02', 12, 29); p('ember-interior-long-bench-v02', 7, 33); p('ember-interior-long-bench-v02', 12, 33)
p('light-candle', 10, 31); p('light-candle', 15, 31); p('ember-interior-wooden-plate', 5, 33)
# dormitory x 36-46, y 18-23: three cells off the corridor y 22-23; the stairs up at the corridor's east end
for cx in (36, 40, 44):
    p('ember-interior-simple-bed-v02', cx, 18); p('ember-interior-stool', cx + 2, 19); p('light-candle', cx, 20)
p('ember-treasure-plain-chest', 46, 20); p('ember-interior-worn-rug', 40, 21)
p('ember-interior-long-bench-v02', 37, 23)
f.stairs(STAIRS_UP, 45, 22, 'mosteiro-andar-2', 'up')
# infirmary x 36-46, y 27-33
for bx in (36, 39, 42):
    p('ember-interior-simple-bed-v02', bx, 28)
p('ember-interior-stool', 45, 28); p('ember-treasure-ceramic-jars', 46, 27); p('inn-herb-shelf', 36, 27)
p('ember-interior-folding-screen', 38, 31); p('ember-interior-folding-screen', 42, 31)
p('ember-crypt-ritual-table', 39, 33); p('ember-dungeon-iron-cage', 44, 33); p('ember-dungeon-chains', 36, 33); p('torture-chain-pile', 46, 32)
p('light-candle', 41, 29); p('ember-interior-worn-rug', 40, 30)
# cemetery x 36-48, y 36-40
for x, y in ((37, 36), (40, 36), (43, 36), (46, 36), (38, 38), (44, 38)):
    p('ember-crypt-ancient-tombstone-v02', x, y)
p('ember-crypt-stone-tomb-v02', 47, 39); p('wilds-broken-cross', 36, 40)
for x, y in ((40, 40), (42, 38), (45, 40)):
    p('wilds-grave-mound', x, y)
p('dungeon-penitent-statue', 48, 37); p('ember-structures-street-brazier', 36, 38); p('dead-tree', 48, 40)
# penitents' yard x 3-17, y 36-40
p('wilds-chained-pillar', 9, 38); p('dungeon-hanging-cage', 4, 37); p('ember-dungeon-iron-cage', 15, 40)
p('ember-dungeon-chains', 6, 40); p('torture-chain-pile', 12, 36); p('torture-brazier', 16, 36)
p('ember-structures-animal-trough', 3, 40); p('ember-props-sacks-v02', 14, 37); p('city-firewood-pile', 4, 39)
# the yard, the path, the gate
p(BELL, 21, 38)
for y in (24, 30, 36):
    p('ember-structures-street-brazier', 23, y); p('ember-structures-street-brazier', 28, y)
p('wilds-ruined-wayside-shrine', 29, 39); p('city-shrineCandle', 22, 23)
p('tree-3d-dead-oak-v02', 20, 28); p('dead-tree', 31, 33); p('dead-tree', 32, 23); p('tree-3d-dead-oak-v02', 30, 28)
p('ember-props-handcart', 19, 36); p('ember-props-barrel-v02', 33, 38); p('ember-extra-firewood-stack', 18, 33)
# the road outside the wall (rows 42-45)
p('dead-tree', 20, 43); p('dead-tree', 31, 44); p('tree-3d-dead-oak-v02', 15, 44); p('tree-3d-dead-oak-v02', 37, 43)
p('wilds-ruined-wayside-shrine', 22, 44); p('ember-extra-broken-cart-wheel', 30, 42); p('ember-structures-road-marker', 28, 43)

# --- the people ---------------------------------------------------------------------------------------------------------------------
ABADE = tree('npc-mosteiro-abade', 'Abade Orsino', [
    L('l1', 'A Cinza acolhe quem chega pela estrada. Sejam bem-vindos ao Mosteiro, viajantes. A igreja está aberta; o claustro também. '
            'As alas dos irmãos, não: lá se dorme e se reza.', replies=[
        R('Os aldeões dizem que gente desaparece nesta estrada.', 'l2'),
        R('Quem são os homens acorrentados no pátio?', 'l4'),
        R('Vi sepulturas frescas no cemitério.', 'l5'),
        R('Posso ver a cripta?', 'l6'),
        R('Só vim rezar.', 'l7')]),
    L('l2', 'Dizem. A estrada é longa, a floresta é funda e os lobos não confessam. Quem chega aqui ferido, nós cuidamos. Quem não chega... '
            'rezamos por ele.', replies=[R('Mas alguns chegaram aqui e não voltaram.', 'l3'), R('Entendo.', None)]),
    L('l3', 'Peregrinos partem antes do sol. Não acordamos os irmãos para cada despedida. Se um nome lhe importa, o Irmão Escriba guarda o '
            'livro de hóspedes. Pergunte a ele. E depois, siga a sua estrada.', None),
    L('l4', 'Penitentes. Vieram de livre vontade, pedir à Cinza o que o mundo lhes negou. As correntes são deles; ninguém as fechou por eles. '
            'Não fale com eles. Quebra a penitência.', None),
    L('l5', 'A febre da estrada leva muitos. Enterramos com honra, no dia da morte, como manda a regra. O Irmão Coveiro cuida disso. Não há '
            'mistério em terra remexida.', None),
    L('l6', 'A cripta é selada. Só os irmãos descem, e só nos dias santos. Não insista: há coisas que a Cinza guarda para si.', None),
    L('l7', 'Então reze. A Santa ouve melhor quem fala baixo.', None),
])
PORTEIRO = tree('npc-mosteiro-porteiro', 'Irmão Porteiro', [
    L('l1', 'Devagar. O portão está aberto, mas a casa tem regra: a igreja e o claustro são de todos; o resto é dos irmãos. A ala leste está '
            'fechada a peregrinos. Fechada, ouviu?', replies=[R('Por que fechada?', 'l2'), R('E os andares de cima?', 'l3'), R('Está bem.', None)]),
    L('l2', 'Doentes. Febre da estrada. Não quer pegar, e eles não querem ser vistos. A Irmã Hospitaleira cuida. Siga para a igreja.', None),
    L('l3', 'A biblioteca e os quartos dos hóspedes. Peregrino que paga dorme lá em cima. O terceiro andar é do Abade e do conselho: '
            'trancado. Nem eu subo.', None),
])
ESCRIBA = tree('npc-mosteiro-escriba', 'Irmão Escriba', [
    L('l1', 'Shh. Aqui se copia, não se fala. ...O que quer?', replies=[
        R('O livro de hóspedes. Procuro um rapaz chamado Tomé.', 'l2'), R('O que vocês copiam aqui?', 'l5'), R('Nada. Desculpe.', None)]),
    L('l2', 'Tomé... Tomé, filho da estrada, chegou na lua passada. Aqui: "partiu de madrugada". Letra minha. Eu escrevi o que me mandaram '
            'escrever.', replies=[R('Você o viu partir?', 'l3'), R('Quem mandou?', 'l4')]),
    L('l3', 'Não vi ninguém partir. Nunca. Chegam, dormem na enfermaria, e de manhã o Abade dita: "partiu". Há um segundo livro, na sacristia, '
            'que eu não copio. Não fui eu que lhe disse isso.', None),
    L('l4', 'O Abade dita. O Abade sempre dita. Pergunte a ele, se tiver coragem. Eu não tenho. Tenho mãos, e gosto delas onde estão.', None),
    L('l5', 'As Lamentações da Cinza. Trinta vezes. O mesmo versículo: "o que arde não se perde, o que se perde é refeito". Antes copiávamos '
            'salmos. Mudou quando o Abade trouxe os... os acorrentados.', None),
])
HOSPITALEIRA = tree('npc-mosteiro-hospitaleira', 'Irmã Hospitaleira', [
    L('l1', 'Baixo. Eles dormem. A febre quebra ao terceiro dia, se a Cinza quiser. Vocês não deviam estar aqui.', replies=[
        R('Por que há correntes ao lado das camas?', 'l2'), R('Posso falar com um doente?', 'l3'), R('Vamos sair.', None)]),
    L('l2', 'Para os febris. Delírio. Já tive um que mordeu a própria língua. As correntes protegem. ...E o biombo é para o pudor. Não '
            'passe dele.', None),
    L('l3', 'Fale, se ele acordar. Mas o que a febre diz não é testemunho. Lembre-se disso quando ele falar da noite.', None),
])
COVEIRO = tree('npc-mosteiro-coveiro', 'Irmão Coveiro', [
    L('l1', 'Sete este mês. A terra está mole, nem precisa de pá. Vai falar comigo ou vai me olhar cavar?', replies=[
        R('Sete mortos de febre?', 'l2'), R('As covas parecem rasas.', 'l3'), R('Deixo você trabalhar.', None)]),
    L('l2', 'Febre, diz o Abade. Eu só cavo. Mas lhe digo: nunca enterrei um caixão. Só terra por cima de terra. Pesa pouco um caixão vazio... '
            'pesa menos um que não existe.', None),
    L('l3', 'Rasas. Porque não tem nada embaixo. O que a casa guarda, guarda lá embaixo, pela sacristia, e não sou eu que desço. Eles '
            'descem. Não sobem. Agora vá, antes que o Porteiro me veja falando.', None),
])
PEREGRINO = tree('npc-mosteiro-peregrino', 'Peregrino Febril', [
    L('l1', '...vocês são de fora? De fora, de fora? Chegue perto. Não deixe a Irmã ver.', replies=[R('O que aconteceu aqui?', 'l2'), R('Descanse.', None)]),
    L('l2', 'À noite levam um de nós. Sempre o mais forte. Pela sacristia, por uma porta de pedra. Eu ouvi correntes embaixo do chão, e um '
            'canto que não é reza. O rapaz da cama ao lado, o Tomé... levaram anteontem. E hoje tem um acorrentado novo no pátio. Com a cara '
            'dele.', None),
])
MULHER = tree('npc-mosteiro-mulher', 'Mulher da Estrada', [
    L('l1', 'Senhores... meu filho Tomé veio rezar à Santa há dez dias. Não voltou. Os irmãos dizem que ele nunca chegou. Mas eu lavei a '
            'roupa dele no riacho ali, e vi ele entrar por esse portão.', replies=[R('Vamos procurá-lo.', 'l2'), R('Não podemos ajudar.', None)]),
    L('l2', 'A Cinza os pague. Se ele estiver lá dentro... tragam o que sobrou dele. Eu aguento.', None),
])
PENITENTE = tree('npc-mosteiro-penitente', 'Penitente Acorrentado', [
    L('l1', '...cinza entra. cinza fica. ...', replies=[R('Quem fez isso com você?', 'l2'), R('Afastar-se.', None)]),
    L('l2', 'lá embaixo... fazem mais de nós. o fogo por dentro. não dói mais. ...não me olhe. não me olhe.', None),
])
f.person('Abade Orsino', 'cultistV2', 27, 8, 12, ABADE)
f.person('Irmão Porteiro', 'cultist', 28, 39, 5, PORTEIRO)
f.person('Irmão Escriba', 'shadowScholar', 9, 7, 3, ESCRIBA)
f.person('Irmã Hospitaleira', 'soupLady', 40, 29, 3, HOSPITALEIRA)
f.person('Irmão Coveiro', 'ratCatcher', 42, 39, 3, COVEIRO)
f.person('Peregrino Febril', 'lostCourier', 44, 29, 1, PEREGRINO)
f.person('Mulher da Estrada', 'villagerF1', 29, 44, 1, MULHER)
f.person('Penitente Acorrentado', 'chainBoundPenitent', 8, 37, 8, PENITENTE)
f.person('Penitente Acorrentado', 'chainBoundPenitent', 13, 39, 8)
f.person('Penitente Acorrentado', 'chainBoundPenitent', 6, 36, 8)
f.person('Noviço', 'cultist', 11, 15, 2)
f.person('Noviço', 'cultist', 26, 18, 2)
f.person('Devoto da Cinza', 'cultistV2', 38, 6, 10)
SPAWN = (25, 44)
f.person('Kael', 'kaelFinal', *SPAWN, 15, player=True)

seen = f.check(SPAWN, sealed=[(40, 9), (41, 9)], beyond=[(40, 12), (41, 12), (42, 12)])
if (34, 2) not in seen or (40, 2) not in seen:
    raise SystemExit('the back alley to the sacristy is cut')
f.write(base_draft(), seen,
        title='O Mosteiro das Cinzas', place='Mosteiro dos Devotos da Cinza, na estrada dos desaparecidos',
        briefing='Na estrada que passa pelo Mosteiro, gente some. Os Devotos da Cinza recebem peregrinos, curam febris e enterram os seus mortos '
                 'com honra, dizem eles. Entre pelo portão, fale com os irmãos, olhe o que não querem mostrar. A cripta é selada: só os '
                 'irmãos descem.',
        objective='Investigue o Mosteiro e encontre o caminho para baixo', fog=True, timeOfDay='dusk', environment='indoor', aboveGround=True, baseTile='plains')  # the Tower's lighting (the user, 2026-10-29: "mapas externos ... sistema similar ao da Torre")
