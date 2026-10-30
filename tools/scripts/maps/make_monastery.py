"""O Mosteiro das Cinzas, the ground floor (2026-10-29; the user: "start working on the map for a area called the Monastery, its going
to be part exploration / adventure, Talk investigation, part dungeon ... villagers claim people are going missing all too often near
that road, its a spooky place with Ashen Devouts and Penitents, they try to keep the fassade, but with some conversation and
investigations you manage to go underground"; "comece com o mapa use tudo de placeholder mais apropriado que tiver").
2026-10-30, the user: "dobre a área cercada do monastério, está tudo muito apertado, deixe os corredores mais espaçosos, não precisa
aumentar o tamanho dos quartos, mas precisa remover aqueles penitentes da entrada não pode revelar esse monstro assim", "coloque novos
NPCs no lugar deles, preciso de diálogos mais compridos e mais bem escritos de acordo com o cenário do jogo".

  python -I tools/scripts/maps/make_monastery.py

Writes content/maps/ember/mosteiro<NNN>.json (the next serial; never overwrites). Free roam (explore, no fog of war, no foe): the
party arrives on the road south of the precinct wall and walks in through the gate. The upper floors: make_monastery_upper.py.

The plan (odd-r hexes, x = column, y = row; 73 x 63): every building keeps its size; the precinct is twice the area of the first
version (inside 70 x 57, was 50 x 41) and the buildings stand apart, so the lanes and yards between them are wide. Each building's
interior is the first version's moved by its own offset (Block below).
  precinct   the muralha (ember-arch-mosteiro-rampart*) on x 1 / 71 and y 1 / 58; the 4-hex gate at x 35-38, y 58; the road outside
             (rows 59-62)
  church     x 29-43, y 4-21: the nave, the chancel; its door south (35-36, 21); the plaza in front of it
  sacristy   x 51-63, y 4-10, door north onto the back alley (56, 4); the crypt stair x 51-63, y 10-16 below it, the LOCKED 3D
             door between them (56, 10: "Arrombar" opens it), the stairs down + the connector to mosteiro-masmorras
  scriptorium x 5-18, y 4-10, door south (11, 10)
  cloister   x 6-17, y 16-31: pillars round an open garden
  refectory  x 5-18, y 38-46, door north (11, 38)
  dormitory  x 51-63, y 23-30, door south (56, 30); the STAIRS UP to Andar II (61, 28)
  infirmary  x 51-63, y 38-46, door north (56, 38)
  cemetery   x 52-64, y 52-56
  work yard  x 4-18, y 52-56 (the penitents' yard of the first version: its chains and cages stay; the three penitents left: the
             monster is not shown at the gate; Bento, Gaspar and Silvano stand there, the yard's tradesmen)
The lanes: the back alley (y 2-3), the east lane (x 45-49), the west lane (x 21-25), the path from the gate to the church (x 35-38).
Blocking: only architecture (walls, doors) blocks; props never (the user, 2026-10-25).
The first version's primitive props left the catalog (700391d8): their places take the nearest props it still has (Poly Haven,
the 2D city / wilds / dungeon pieces).
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from monastery_lib import (DOOR, LOCKED, PORTAL, RAMPART_GATE, STAIRS_DOWN, STAIRS_UP, Floor, L, R, base_draft, hwall, ring, tree, vwall)  # noqa: E402

PEW, LECTERN, VOTIVE, SAINT, BELL, DESK = ('ember-mosteiro-pew', 'ember-mosteiro-lectern', 'ember-mosteiro-votive-rack',
                                           'ember-mosteiro-saint-statue', 'ember-mosteiro-bell-cote', 'ember-mosteiro-scriptorium-desk')
f = Floor('mosteiro', 73, 63)


class Block:
    """A building of the first version, moved by (dx, dy): its walls, openings, floors, props and people in the old coordinates."""

    def __init__(self, dx, dy):
        self.dx, self.dy = dx, dy

    def at(self, x, y):
        return x + self.dx, y + self.dy

    def cells(self, cells):
        return {self.at(x, y) for x, y in cells}

    def ring(self, x0, y0, x1, y1):
        return ring(x0 + self.dx, y0 + self.dy, x1 + self.dx, y1 + self.dy)

    def rect(self, x0, y0, x1, y1, t):
        f.rect(x0 + self.dx, y0 + self.dy, x1 + self.dx, y1 + self.dy, t)

    def p(self, ident, x, y, *args, **kw):
        return f.place(ident, *self.at(x, y), *args, **kw)


CH, SA, SC, CL, RE, DO, IN, CE, YA = Block(10, 1), Block(16, 1), Block(1, 1), Block(1, 6), Block(1, 12), Block(16, 6), Block(16, 12), Block(16, 16), Block(1, 16)

# --- walls --------------------------------------------------------------------------------------------------------------------------
f.walls |= ring(1, 1, 71, 58)            # the precinct: the muralha (openAir pieces: the yard inside stays under the sky)
f.ramparts = ring(1, 1, 71, 58)
f.walls |= CH.ring(19, 3, 33, 20)
f.walls |= SA.ring(35, 3, 47, 15) | SA.cells(hwall(9, 35, 47))      # sacristy + crypt stair
f.walls |= SC.ring(4, 3, 17, 9)
f.walls |= RE.ring(4, 26, 17, 34)
f.walls |= DO.ring(35, 17, 47, 24) | DO.cells(vwall(39, 18, 21)) | DO.cells(vwall(43, 18, 21))  # the cells' partitions (corridor y 22-23)
f.walls |= IN.ring(35, 26, 47, 34)
f.opening(RAMPART_GATE, 36, 58)
for b, piece, x, y in ((CH, DOOR, 25, 20), (SA, PORTAL, 40, 3), (SA, LOCKED, 40, 9), (SC, PORTAL, 10, 9), (RE, DOOR, 10, 26), (DO, PORTAL, 40, 24),
                       (IN, DOOR, 40, 26)):
    f.opening(piece, *b.at(x, y))
f.windows = (CH.cells({(21, 3), (24, 3), (28, 3), (31, 3), (19, 7), (19, 11), (19, 15), (33, 7), (33, 11), (33, 15), (22, 20), (30, 20)})
             | SA.cells({(37, 3), (44, 3), (47, 6), (47, 12)}) | DO.cells({(47, 20), (35, 20)}) | IN.cells({(47, 30), (35, 30), (38, 26), (44, 26), (38, 34), (44, 34)})
             | SC.cells({(7, 3), (14, 3), (4, 6), (17, 6), (7, 9), (14, 9)}) | RE.cells({(7, 26), (14, 26), (4, 30), (17, 30), (7, 34), (14, 34)}))
f.slits = (SA.cells({(35, 6), (35, 12), (38, 15), (44, 15)}) | DO.cells({(38, 17), (44, 17), (38, 24), (44, 24)}) | SC.cells({(4, 4), (17, 4)})
           | RE.cells({(4, 28), (17, 28)}))
f.torches = (CH.cells({(20, 3), (26, 3), (32, 3), (19, 5), (19, 9), (19, 13), (19, 17), (33, 5), (33, 9), (33, 13), (33, 17), (21, 20), (29, 20)})
             | SA.cells({(36, 3), (46, 9), (36, 15), (46, 15), (36, 9), (42, 9)}) | SC.cells({(5, 3), (10, 3), (16, 3), (5, 9), (15, 9)})
             | RE.cells({(5, 26), (15, 26), (10, 34), (4, 32), (17, 32)}) | DO.cells({(36, 17), (41, 17), (46, 17), (37, 24), (46, 24)})
             | IN.cells({(36, 26), (46, 26), (36, 34), (46, 34), (41, 34)})
             | {(29, 58), (44, 58), (14, 58), (57, 58), (1, 28), (1, 48), (71, 28), (71, 48), (20, 1), (52, 1)})  # the precinct, inside faces
f.lay_walls()

# --- floors -------------------------------------------------------------------------------------------------------------------------
CH.rect(20, 4, 32, 19, 'nave'); SA.rect(36, 4, 46, 14, 'nave'); SC.rect(5, 4, 16, 8, 'nave'); RE.rect(5, 27, 16, 33, 'nave')
DO.rect(36, 18, 46, 23, 'nave'); IN.rect(36, 27, 46, 33, 'nave')
f.rect(35, 22, 38, 57, 'nave'); f.rect(35, 59, 38, 62, 'nave')                     # the path from the gate and the road
f.rect(30, 22, 42, 24, 'nave')                                                         # the plaza before the church door
f.rect(20, 2, 64, 3, 'nave'); f.rect(45, 4, 49, 57, 'nave'); f.rect(21, 11, 25, 50, 'nave')  # the back alley, the east and west lanes
f.rect(26, 47, 34, 49, 'nave')                                                         # the west lane to the path
CL.rect(5, 10, 16, 10, 'nave'); CL.rect(5, 25, 16, 25, 'nave'); CL.rect(5, 11, 5, 24, 'nave'); CL.rect(16, 11, 16, 24, 'nave')  # the cloister walk
CL.rect(6, 12, 15, 23, 'woods'); CE.rect(36, 36, 48, 40, 'ruins'); YA.rect(3, 36, 17, 40, 'ruins')

# --- props --------------------------------------------------------------------------------------------------------------------------
# the church: the chancel at its north end, the aisle in the middle
CH.p(SAINT, 26, 4); CH.p('ember-dungeon-altar', 25, 6); CH.p('ember-ember-ancient-reliquary', 29, 5)
CH.p('wilds-brazier-tripod', 22, 5); CH.p('wilds-brazier-tripod', 30, 6)
CH.p(LECTERN, 23, 8); CH.p(VOTIVE, 29, 8); CH.p('wilds-incense-burner', 28, 6); CH.p('light-candle', 24, 5); CH.p('light-candle', 27, 7)
for y in (9, 13, 17):
    CH.p('ember-dungeon-pillar-v02', 20, y); CH.p('ember-dungeon-pillar-v02', 32, y)
for y in (11, 13, 15, 17):
    CH.p(PEW, 21, y); CH.p(PEW, 23, y); CH.p(PEW, 27, y); CH.p(PEW, 29, y)
CH.p('light-candle', 31, 11); CH.p('light-candle', 31, 18); CH.p('city-shrineCandle', 20, 19)
# sacristy
SA.p('ember-polyhaven-wooden-table-02', 40, 6); SA.p('ember-polyhaven-wooden-bookshelf-worn', 36, 4); SA.p('ember-polyhaven-wooden-bookshelf-worn', 37, 4)
SA.p('ember-treasure-iron-banded-chest', 46, 4); SA.p('ember-treasure-ceramic-jars', 46, 8); SA.p('ember-interior-cloth-curtain', 44, 4)
SA.p('light-candle', 38, 7); SA.p('ember-interior-table-candelabra', 42, 5); SA.p('dungeon-funerary-urn', 36, 8)
# crypt stair
f.stairs(STAIRS_DOWN, *SA.at(41, 12), 'mosteiro-masmorras', 'down')
SA.p('ember-dungeon-chains', 36, 13); SA.p('wilds-brazier-tripod', 37, 10); SA.p('wilds-brazier-tripod', 45, 10)
SA.p('dungeon-funerary-urn', 46, 14); SA.p('dungeon-penitent-statue', 36, 10); SA.p('dungeon-penitent-statue', 46, 11)
# scriptorium
for x in (5, 7, 9, 11, 13, 15):
    SC.p('ember-polyhaven-wooden-bookshelf-worn', x, 4)
for x, y in ((6, 6), (10, 6), (14, 6), (8, 8), (12, 8)):
    SC.p(DESK, x, y)
SC.p(LECTERN, 16, 7); SC.p('light-candle', 7, 7); SC.p('light-candle', 11, 7); SC.p('light-candle', 15, 5)
SC.p('ember-interior-bookshelf', 5, 8); SC.p('city-rope-coil', 16, 5)
# cloister
for x in range(6, 16, 2):
    CL.p('ember-dungeon-pillar-v02', x, 11); CL.p('ember-dungeon-pillar-v02', x, 24)
for y in range(13, 23, 3):
    CL.p('ember-dungeon-pillar-v02', 6, y); CL.p('ember-dungeon-pillar-v02', 15, y)
CL.p('ember-city-well', 10, 17); CL.p('tree-3d-dead-oak-v02', 8, 14); CL.p('dead-tree', 13, 20)
CL.p('ember-city-bench', 9, 19); CL.p('ember-city-bench', 13, 16); CL.p('wilds-candle-menhir', 11, 21)
CL.p('wilds-mushroom-cluster', 7, 18); CL.p('ember-polyhaven-periwinkle-plant-01', 13, 13)
# refectory
RE.p('ember-tavern-stone-hearth', 5, 27); RE.p('ember-interior-cauldron', 7, 27); RE.p('ember-interior-kitchen-cabinet', 9, 27)
RE.p('ember-treasure-ceramic-jars', 16, 27); RE.p('ember-props-sacks-v02', 16, 28); RE.p('ember-props-barrel-v02', 16, 33)
for y in (30, 32):
    RE.p('ember-tavern-table-messhall', 8, y); RE.p('ember-tavern-table-messhall', 13, y)
for x, y in ((7, 29), (12, 29), (7, 33), (12, 33)):
    RE.p('ember-interior-long-bench-v02', x, y)
RE.p('light-candle', 10, 31); RE.p('light-candle', 15, 31); RE.p('ember-polyhaven-carved-wooden-plate', 5, 33)
# dormitory: three cells off the corridor; the stairs up at the corridor's east end
for cx in (36, 40, 44):
    DO.p('ember-interior-simple-bed-v02', cx, 18); DO.p('ember-interior-stool', cx + 2, 19); DO.p('light-candle', cx, 20)
DO.p('ember-treasure-plain-chest', 46, 20); DO.p('ember-interior-worn-rug', 40, 21)
DO.p('ember-interior-long-bench-v02', 37, 23)
f.stairs(STAIRS_UP, *DO.at(45, 22), 'mosteiro-andar-2', 'up')
# infirmary
for bx in (36, 39, 42):
    IN.p('ember-interior-simple-bed-v02', bx, 28)
IN.p('ember-interior-stool', 45, 28); IN.p('ember-treasure-ceramic-jars', 46, 27); IN.p('inn-herb-shelf', 36, 27)
IN.p('ember-interior-folding-screen', 38, 31); IN.p('ember-interior-folding-screen', 42, 31)
IN.p('ember-polyhaven-wooden-table-02', 39, 33); IN.p('ember-dungeon-iron-cage', 44, 33); IN.p('ember-dungeon-chains', 36, 33); IN.p('torture-chain-pile', 46, 32)
IN.p('light-candle', 41, 29); IN.p('ember-interior-worn-rug', 40, 30)
# cemetery
for x, y in ((37, 36), (40, 36), (43, 36), (46, 36), (38, 38), (44, 38)):
    CE.p('wilds-gothic-gravestone', x, y)
CE.p('dungeon-sarcophagus', 47, 39); CE.p('wilds-broken-cross', 36, 40)
for x, y in ((40, 40), (42, 38), (45, 40)):
    CE.p('wilds-grave-mound', x, y)
CE.p('dungeon-penitent-statue', 48, 37); CE.p('city-brazier', 36, 38); CE.p('dead-tree', 48, 40)
# the work yard (the penitents' yard: its chains and cages; the penitents are gone from the gate)
YA.p('wilds-chained-pillar', 9, 38); YA.p('dungeon-hanging-cage', 4, 37); YA.p('ember-dungeon-iron-cage', 15, 40)
YA.p('ember-dungeon-chains', 6, 40); YA.p('torture-chain-pile', 12, 36); YA.p('torture-brazier', 16, 36)
YA.p('city-stone-trough-fountain', 3, 40); YA.p('ember-props-sacks-v02', 14, 37); YA.p('city-firewood-pile', 4, 39)
# the yards, the path, the gate
f.place(BELL, 30, 51)
for y in (27, 37, 47):
    f.place('city-brazier', 34, y); f.place('city-brazier', 39, y)
f.place('wilds-ruined-wayside-shrine', 41, 55); f.place('city-shrineCandle', 33, 23)
f.place('tree-3d-dead-oak-v02', 29, 34); f.place('tree-3d-dead-oak-v02', 42, 42); f.place('dead-tree', 43, 30); f.place('dead-tree', 30, 43)
f.place('wooden-cart', 28, 54); f.place('ember-props-barrel-v02', 43, 54); f.place('city-firewood-stack-new', 27, 45)
# the road outside the wall (rows 59-62)
f.place('dead-tree', 25, 60); f.place('dead-tree', 47, 61); f.place('tree-3d-dead-oak-v02', 20, 61); f.place('tree-3d-dead-oak-v02', 53, 60)
f.place('wilds-ruined-wayside-shrine', 32, 61); f.place('wilds-weathered-signpost', 41, 60)
f.place('ember-polyhaven-namaqualand-boulder', 29, 60)  # the user's own (their editor save mosteiro005)

# --- the people ---------------------------------------------------------------------------------------------------------------------
ABADE = tree('npc-mosteiro-abade', 'Abade Orsino', [
    L('l1', 'A Cinza acolhe quem chega pela estrada. Sejam bem-vindos ao Mosteiro, viajantes. A igreja está aberta; o claustro também. '
            'As alas dos irmãos, não: lá se dorme e se reza.', replies=[
        R('Os aldeões dizem que gente desaparece nesta estrada.', 'l2'),
        R('Para que servem as correntes no pátio?', 'l4'),
        R('Vi sepulturas frescas no cemitério.', 'l5'),
        R('Posso ver a cripta?', 'l6'),
        R('Só vim rezar.', 'l7')]),
    L('l2', 'Dizem. A estrada é longa, a floresta é funda e os lobos não confessam. Quem chega aqui ferido, nós cuidamos. Quem não chega... '
            'rezamos por ele.', replies=[R('Mas alguns chegaram aqui e não voltaram.', 'l3'), R('Entendo.', None)]),
    L('l3', 'Peregrinos partem antes do sol. Não acordamos os irmãos para cada despedida. Se um nome lhe importa, o Irmão Escriba guarda o '
            'livro de hóspedes. Pergunte a ele. E depois, siga a sua estrada.', None),
    L('l4', 'Para os penitentes. Vêm de livre vontade, pedir à Cinza o que o mundo lhes negou, e as correntes são deles: ninguém as fecha '
            'por eles. Hoje estão recolhidos, em oração. Não os procure. Quebra a penitência.', None),
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
            'canto que não é reza. O rapaz da cama ao lado, o Tomé... levaram anteontem. E hoje cedo, pela janela, vi passar um acorrentado '
            'novo rumo à sacristia. Com a cara dele.', None),
])
MULHER = tree('npc-mosteiro-mulher', 'Mulher da Estrada', [
    L('l1', 'Senhores... meu filho Tomé veio rezar à Santa há dez dias. Não voltou. Os irmãos dizem que ele nunca chegou. Mas eu lavei a '
            'roupa dele no riacho ali, e vi ele entrar por esse portão.', replies=[R('Vamos procurá-lo.', 'l2'), R('Não podemos ajudar.', None)]),
    L('l2', 'A Cinza os pague. Se ele estiver lá dentro... tragam o que sobrou dele. Eu aguento.', None),
])
# The work yard's three, in the penitents' place (the user, 2026-10-30): the tradesmen who serve the house from outside its rule, each
# with one piece of what the house hides (the furnaces below, the night bell, the coin of the missing) and a way to tie it to the rest.
BENTO = tree('npc-mosteiro-bento', 'Bento, Catador de Cogumelos', [
    L('l1', 'Hm. Gente de espada no pátio dos irmãos. Não é coisa que se vê todo dia. Eu sou o Bento. Trago cogumelo da mata pra cozinha '
            'deles, uma cesta por semana, e eles me pagam em pão e em silêncio. O pão é bom. O silêncio, eu como junto.', replies=[
        R('Você conhece bem o Mosteiro?', 'l2'),
        R('Que tipo de cogumelo eles compram?', 'l4'),
        R('Viu alguém desaparecer por aqui?', 'l6'),
        R('Até mais, Bento.', None)]),
    L('l2', 'Conheço o lado de fora, que é o que deixam a gente conhecer. A cozinha, o pátio, a horta do claustro. Mas lhe digo uma '
            'coisa que quem anda na mata aprende: a terra conta o que a casa esconde.', replies=[R('Conta o quê?', 'l3'), R('Fale claro.', 'l3')]),
    L('l3', 'Atrás da sacristia, onde a água da chuva escorre, o chão é morno. Morno no inverno, senhor. E o cogumelo que nasce ali não '
            'é cogumelo de sombra: brilha. Um brilho de brasa, por dentro do chapéu. Tem fogo debaixo deste chão, e não é fogo de cozinha.',
      replies=[R('Fogo debaixo da sacristia?', 'l8'), R('Você colheu algum?', 'l9')]),
    L('l4', 'Antes era o de sempre: orelha-de-pau, cogumelo de faia, o que se põe no caldo. Faz umas luas, o Irmão Cozinheiro me pediu '
            'outra coisa. "Traga o que nasce perto de cinza", disse. "O que cresce onde houve fogueira." Eu nunca tinha ouvido ninguém pedir isso.',
      replies=[R('E para que serviria?', 'l5'), R('Você levou?', 'l5')]),
    L('l5', 'Levei, que freguês é freguês. Mas nunca vi esse cogumelo no caldo do refeitório. Vai pra outro lugar. Vi um irmão sair da '
            'cozinha com a minha cesta e entrar na sacristia, que é o caminho errado pra quem vai comer. Ninguém come na sacristia.', None),
    L('l6', 'Desaparecer, não vi. Gente que chega e não sai, isso eu vi. Conto as carroças na estrada quando venho, é mania de quem '
            'cata coisa pequena. Entra mais gente do que sai. Toda semana a conta fecha errado.', replies=[R('Quantos, você diria?', 'l7')]),
    L('l7', 'Desde a lua de inverno, uns doze. Moços, quase todos. Os velhos e as crianças, esses o Porteiro manda embora com pão e bênção. '
            'Pense nisso: que casa de caridade recusa justamente quem mais precisa?', None),
    L('l8', 'Embaixo dela, sim. A porta de pedra que eles trancam não guarda túmulo de santo. Túmulo não esquenta. Se for descer, desça '
            'com a mão na espada e o nariz atento: o ar de lá cheira a ferro e a resina queimada.', None),
    L('l9', 'Colhi um, uma vez. Brilhou na minha mão a noite inteira e me deu sonhos que eu não conto nem pra mulher. Joguei no rio. '
            'O rio ficou quieto três dias, sem sapo nenhum. Não colho mais.', None),
])
GASPAR = tree('npc-mosteiro-gaspar', 'Gaspar, Caçador dos Brejos', [
    L('l1', 'Fique onde está até eu ver suas mãos. ...Bom. Desculpe a desconfiança, forasteiro, mas neste pátio é melhor desconfiar '
            'cedo do que se arrepender tarde. Gaspar. Vendo couro e carne aos irmãos, e armadilha a quem precisar.', replies=[
        R('Por que tanta desconfiança?', 'l2'),
        R('O que os irmãos compram de você?', 'l4'),
        R('Você ouve o sino à noite?', 'l6'),
        R('Não vou incomodar.', None)]),
    L('l2', 'Porque há três semanas uma das minhas armadilhas, lá no brejo do lado norte, pegou um homem. Não um bicho: um homem, '
            'descalço, de corrente no tornozelo. A corrente arrebentada. Ele tinha fugido daqui.', replies=[R('E o que houve com ele?', 'l3')]),
    L('l3', 'Morreu antes do sol, nos meus braços. Tinha as veias acesas, forasteiro, como brasa por baixo da pele, e um cheiro de '
            'forja. Repetia um nome: "o Infusor, o Infusor". Enterrei no brejo e não contei a ninguém. Até agora.',
      replies=[R('Infusor? Quem é?', 'l8'), R('Por que não contou?', 'l9')]),
    L('l4', 'Couro, carne seca, sebo. E corrente. Corrente de prender urso, das grossas, que eu mesmo trago da ferraria do Vau. Num '
            'mosteiro de oração, me diga você pra que se quer corrente de urso.', replies=[R('Para os penitentes, diz o Abade.', 'l5')]),
    L('l5', 'Penitente se acorrenta com corda e vergonha, e isso basta. O que eles prendem com corrente de urso não pede perdão a ninguém. '
            'Eu vi os elos que voltam pra conserto: dobrados por dentro. Coisa de força que gente não tem.', None),
    L('l6', 'Ouço. Três batidas, perto da meia-noite, e nunca há missa a essa hora. Na noite seguinte, alguém some da enfermaria. Já contei: '
            'toda vez. O sino é sinal pra quem trabalha embaixo do chão. Pra eles, quer dizer "desce mais um".', replies=[R('Quem toca o sino?', 'l7')]),
    L('l7', 'Um noviço, sempre o mesmo, magro, de olho fundo. Ele não sobe ao campanário: puxa a corda no pátio, olhando a porta da '
            'sacristia como cachorro olha a do dono. Se quiser a verdade desta casa, siga quem toca, não quem reza.', None),
    L('l8', 'Não sei, e espero não saber. Mas um nome assim não se dá a cozinheiro. Quem enche alguém de fogo até a pele acender sabe o '
            'que está fazendo, e faz de propósito. Seja o que for, mora embaixo da sacristia. Os fugidos correm sempre de lá.', None),
    L('l9', 'Porque os irmãos me pagam, porque o Porteiro tem olho de falcão, e porque quem fala some. Agora o senhor sabe, e eu durmo '
            'mais leve. Faça bom uso. E se descer lá embaixo... não deixe a corrente encostar na pele de ninguém.', None),
])
SILVANO = tree('npc-mosteiro-silvano', 'Silvano, Mercador de Velas', [
    L('l1', 'Velas de sebo, velas de cera, incenso de três resinas! ...Ah, desculpe, é o costume. O senhor não tem cara de quem compra '
            'vela. Silvano, mercador. Abasteço o Mosteiro há onze anos, e nunca vendi tanto como neste inverno.', replies=[
        R('Tanto quanto?', 'l2'),
        R('Como eles pagam?', 'l4'),
        R('O que mudou nesta casa?', 'l6'),
        R('Boas vendas, Silvano.', None)]),
    L('l2', 'Dez vezes o de um ano bom. Dez! E não é a igreja que gasta, a igreja tem as mesmas velas de sempre, eu conto. É o sebo que '
            'some. Sebo puro, em barrica, e óleo de queimar, do mais forte. Uma casa de vinte irmãos não queima isso nem em dez invernos.',
      replies=[R('Para onde vai tudo isso?', 'l3')]),
    L('l3', 'Pra baixo. As barricas descem pela sacristia; eu mesmo ajudei a rolar uma, até a porta de pedra, e o irmão me mandou parar '
            'ali. Por baixo da porta saía um calor de forno e uma luz que não é de vela. Alguém lá embaixo mantém uma fornalha acesa dia e noite.', None),
    L('l4', 'Em moeda, e bem. Bem demais. É isso que me tira o sono. Moeda de cobre do Vau, moeda de prata da Aldeia, uma ou outra de '
            'Mordávia ainda com a efígie do velho conde... moeda que peregrino traz no bolso. Não moeda de mosteiro.',
      replies=[R('Moeda de peregrino?', 'l5')]),
    L('l5', 'O senhor me entende. Peregrino entra com a bolsa cheia e, dizem, parte de madrugada. Mas a bolsa dele fica, e vem parar na '
            'minha mão em troca de sebo. Eu vendo, porque vender é o que eu sei. Mas guardo uma de cada no bolso de dentro. Pra lembrar.',
      replies=[R('Posso ver?', 'l9')]),
    L('l6', 'O Abade Orsino voltou de uma viagem ao Sul com uma carroça coberta e dois homens que ninguém conhecia. Desde então, o '
            'canto mudou, o Escriba parou de sorrir e começaram os penitentes. Antes eram irmãos de oração; agora parecem guardas de cárcere.',
      replies=[R('Os dois homens ainda estão aqui?', 'l7')]),
    L('l7', 'Nunca mais vi a cara deles à luz do dia. Mas um deles comprou de mim, uma vez, vela de cera negra, a mais cara, e pediu '
            'que eu não contasse ao Abade. Tinha as mãos queimadas até o cotovelo, e não reclamava da dor. Disse que trabalhava "com brasa viva".',
      replies=[R('Brasa viva?', 'l8')]),
    L('l8', 'Foi o que ele disse. E riu. Já vi muito ferreiro rir de queimadura, mas aquele riso não era de ferreiro. Se o senhor procura '
            'quem some nesta estrada, procure quem compra fogo demais. Eu vendo o fogo. Não quero saber o que acendem com ele.', None),
    L('l9', 'Aqui. Cobre do Vau, prata da Aldeia... e esta, de chumbo, com um furo no meio, que os carvoeiros da mata usam de amuleto. '
            'Nenhum carvoeiro paga vela com amuleto. Alguém tirou isto do pescoço de um homem. Fique com ela. A mim, já pesa demais.', None),
])
f.person('Abade Orsino', 'cultistV2', *CH.at(27, 8), 12, ABADE)
f.person('Irmão Porteiro', 'cultist', 39, 56, 5, PORTEIRO)
f.person('Irmão Escriba', 'shadowScholar', *SC.at(9, 7), 3, ESCRIBA)
f.person('Irmã Hospitaleira', 'soupLady', *IN.at(40, 29), 3, HOSPITALEIRA)
f.person('Irmão Coveiro', 'ratCatcher', *CE.at(42, 39), 3, COVEIRO)
f.person('Peregrino Febril', 'lostCourier', *IN.at(44, 29), 1, PEREGRINO)
f.person('Mulher da Estrada', 'villagerF1', 40, 61, 1, MULHER)
f.person('Bento, Catador de Cogumelos', 'mushroomForager', *YA.at(8, 37), 3, BENTO)
f.person('Gaspar, Caçador dos Brejos', 'marshTrapper', *YA.at(12, 39), 4, GASPAR)
f.person('Silvano, Mercador de Velas', 'travelingMerchant', *YA.at(6, 36), 3, SILVANO)
f.person('Noviço', 'cultist', *CL.at(11, 15), 2)
f.person('Noviço', 'cultist', *CH.at(26, 18), 2)
f.person('Devoto da Cinza', 'cultistV2', *SA.at(38, 6), 10)
SPAWN = (36, 61)
f.person('Kael', 'kaelFinal', *SPAWN, 15, player=True)

seen = f.check(SPAWN, sealed=[SA.at(40, 9), SA.at(41, 9)], beyond=[SA.at(40, 12), SA.at(41, 12), SA.at(42, 12)])
if (45, 2) not in seen or SA.at(40, 2) not in seen:
    raise SystemExit('the back alley to the sacristy is cut')
f.write(base_draft(), seen,
        title='O Mosteiro das Cinzas', place='Mosteiro dos Devotos da Cinza, na estrada dos desaparecidos',
        briefing='Na estrada que passa pelo Mosteiro, gente some. Os Devotos da Cinza recebem peregrinos, curam febris e enterram os seus mortos '
                 'com honra, dizem eles. Entre pelo portão, fale com os irmãos, olhe o que não querem mostrar. A cripta é selada: só os '
                 'irmãos descem.',
        objective='Investigue o Mosteiro e encontre o caminho para baixo', fog=False, timeOfDay='dusk', environment='indoor', aboveGround=True,
        baseTile='plains',  # the Tower's lighting (the user, 2026-10-29: "mapas externos ... sistema similar ao da Torre")
        # as the live version (mosteiro006: no fog of war; 007: the user's tiles and the ground outside)
        emberTiles='mosteiro-v01', emberFloor='mosteiro-ground-v01.jpg', emberFloorDepth=0.0, emberFloorTile=120.0)
