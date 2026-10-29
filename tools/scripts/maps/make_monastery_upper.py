"""O Mosteiro das Cinzas, the upper floors (2026-10-29; the user: "continua a fazer o andar de cima do monasterio, um pouco menor mas
não muito, e um terceiro ainda menor, esses são andares superiores onde as coisas ainda aparentam ser normais, exceto no topo que
está trancado e tem a diretoria macabra do monasterio").

  python -I tools/scripts/maps/make_monastery_upper.py

Writes content/maps/ember/mosteiro-andar-2<NNN>.json and mosteiro-andar-3<NNN>.json (the next serials; never overwrites). Both are
"Interna acima do solo" (environment indoor + aboveGround: the one sky's light through the windows), free roam, no foe in sight.
The ground floor's stairs up (make_monastery.py, the dormitory corridor) lead to Andar II; Andar II's stairs up to Andar III; each
floor's stairs down return (returnConnector).

Andar II (44 x 36): a corridor (rows 16-18) across the building; north of it the library (x 2-15), the chapter house (x 17-28) and
  the guests' quarters (x 30-41: pilgrims' beds, some with the belongings left and nobody in them); south of it five monks' cells
  (x 2-20), the treasurer's office (x 22-31) and the stairs hall (x 33-41: down to the ground floor, up to Andar III).
Andar III (34 x 28): the arrival hall (rows 19-25, the stairs down, a brother on guard) and, beyond the LOCKED stone door
  (dungeon-stone-door at 16-17, 18: "Arrombar"), the council of the Devotos: the chapter of the Ash (x 2-14: the council table,
  the Prior and the Master of Novices, the relic on its pedestal, chains and bones) and the Abbot's private chapel (x 16-31: the
  altar under penitent statues, the hanging cage, the sarcophagus of the "Saint", the Mother Superior); two Penitentes stand guard.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from monastery_lib import (DOOR, LOCKED, PORTAL, STAIRS_DOWN, STAIRS_UP, Floor, L, R, base_draft, hwall, ring, tree, vwall)  # noqa: E402

PEW, LECTERN, VOTIVE, SAINT, DESK = ('ember-mosteiro-pew', 'ember-mosteiro-lectern', 'ember-mosteiro-votive-rack', 'ember-mosteiro-saint-statue',
                                     'ember-mosteiro-scriptorium-desk')
BED, STOOL, CANDLE, RUG, SHELF, OLDSHELF = ('ember-interior-simple-bed-v02', 'ember-interior-stool', 'light-candle', 'ember-interior-worn-rug',
                                            'ember-interior-bookshelf', 'ember-crypt-ancient-bookshelf')
CHEST, CHEST2, BENCH, BRAZIER, CANDELABRA = ('ember-treasure-plain-chest', 'ember-treasure-iron-banded-chest', 'ember-interior-long-bench-v02',
                                              'ember-crypt-ritual-brazier', 'ember-interior-table-candelabra')
INDOOR = dict(fog=True, timeOfDay='dusk', environment='indoor', aboveGround=True, baseTile='nave')


def chair(f, x, y, face):
    # ember-tavern-chair-v04: the back toward -z at rot 0; FACES as make_inn_v8.py (v02 faces n/e/s/w at rot 0..3, v04 the other way)
    f.place('ember-tavern-chair-v04', x, y, rot=({'n': 0, 'e': 1, 's': 2, 'w': 3}[face] + 2) % 4)


# =================================================================================================== Andar II
def andar2():
    f = Floor('mosteiro-andar-2', 44, 36, 'nave')
    f.walls |= ring(1, 1, 42, 34)
    f.walls |= hwall(15, 1, 42) | hwall(19, 1, 42)                       # the corridor's walls (rows 16-18 between)
    f.walls |= vwall(16, 1, 15) | vwall(29, 1, 15)                       # library | chapter house | guests
    f.walls |= vwall(5, 19, 34) | vwall(9, 19, 34) | vwall(13, 19, 34) | vwall(17, 19, 34) | vwall(21, 19, 34)  # the cells
    f.walls |= vwall(32, 19, 34)                                         # office | stairs hall
    for piece, x, y in ((PORTAL, 8, 15), (PORTAL, 22, 15), (DOOR, 35, 15),                 # library, chapter house, guests
                        (DOOR, 2, 19), (DOOR, 6, 19), (DOOR, 10, 19), (DOOR, 14, 19), (DOOR, 18, 19),  # the cells
                        (DOOR, 26, 19), (PORTAL, 36, 19)):                                  # the office, the stairs hall
        f.opening(piece, x, y)
    f.windows = {(4, 1), (8, 1), (12, 1), (19, 1), (23, 1), (27, 1), (33, 1), (38, 1), (1, 5), (1, 10), (42, 5), (42, 10), (1, 24), (1, 30), (42, 24), (42, 30),
                 (3, 34), (7, 34), (11, 34), (15, 34), (19, 34), (25, 34), (29, 34), (36, 34), (40, 34)}
    f.slits = {(1, 17), (42, 17), (16, 1), (29, 1)}
    f.torches = {(2, 15), (12, 15), (18, 15), (27, 15), (32, 15), (40, 15), (4, 19), (8, 19), (12, 19), (16, 19), (20, 19), (23, 19), (30, 19), (34, 19), (40, 19),
                 (6, 1), (10, 1), (21, 1), (25, 1), (31, 1), (36, 1), (40, 1), (1, 3), (1, 12), (42, 3), (42, 12), (16, 8), (29, 8),
                 (5, 27), (9, 27), (13, 27), (17, 27), (21, 27), (32, 27), (1, 27), (42, 27), (24, 34), (37, 34), (33, 34)}
    f.lay_walls()
    p = f.place
    # the library x 2-15, y 2-14: shelves on three walls, reading desks, two lecterns, a rug, candelabra
    for x in range(2, 15, 2):
        p(OLDSHELF, x, 2)
    for y in (4, 6, 8, 10, 12):
        p(OLDSHELF, 2, y); p(OLDSHELF, 15, y)
    for x, y in ((5, 6), (9, 6), (12, 6), (6, 10), (10, 10)):
        p(DESK, x, y)
    p(LECTERN, 8, 4); p(LECTERN, 13, 12); p(RUG, 7, 8); p(CANDELABRA, 4, 4); p(CANDLE, 11, 4); p(CANDLE, 5, 12); p(CANDLE, 11, 12)
    p('ember-treasure-leather-satchel', 14, 14); p('ember-treasure-coiled-rope', 3, 14)
    # the chapter house x 17-28, y 2-14: the reader's lectern north, benches in rows facing it, a penitent statue, braziers
    p(LECTERN, 22, 3); p('dungeon-penitent-statue', 18, 2); p('dungeon-penitent-statue', 27, 2); p(VOTIVE, 25, 3)
    for y in (6, 8, 10, 12):
        p(PEW, 18, y); p(PEW, 20, y); p(PEW, 24, y); p(PEW, 26, y)
    p(BRAZIER, 17, 5); p(BRAZIER, 28, 5); p(CANDLE, 22, 14); p('city-shrineCandle', 28, 14)
    # the guests' quarters x 30-41, y 2-14: beds along the north and east walls, their owners' things left beside empty ones
    for bx in (30, 33, 36, 39):
        p(BED, bx, 2); p(STOOL, bx + 2, 3)
    p(BED, 40, 6); p(BED, 40, 10)
    p('ember-treasure-leather-satchel', 31, 4); p('ember-treasure-leather-satchel', 37, 4); p('ember-props-sacks-v02', 34, 4)
    p('ember-treasure-wicker-baskets', 30, 13); p(CHEST, 30, 9); p(RUG, 34, 8); p('ember-tavern-table-small', 34, 11)
    chair(f, 33, 11, 'e'); chair(f, 36, 11, 'w'); p(CANDLE, 35, 6); p(CANDLE, 31, 13); p('ember-interior-wooden-mug', 39, 13)
    p('ember-interior-cloth-curtain', 37, 13); p('ember-interior-folding-screen', 38, 8)
    # the monks' cells x 2-20, y 20-33 (five cells of three hexes between the partitions): a bed, a stool, a candle, a chest
    for cx in (2, 6, 10, 14, 18):
        p(BED, cx, 32); p(STOOL, cx + 2, 31); p(CANDLE, cx, 30); p(CHEST, cx + 2, 33) if cx != 10 else p(SHELF, cx + 2, 33)
    p(RUG, 10, 22); p('ember-interior-wooden-plate', 18, 24); p('city-shrineCandle', 2, 21); p('ember-treasure-unlit-torch', 14, 21)
    # the treasurer's office x 22-31, y 20-33: the desk with the ledgers, shelves, strongboxes, a rug, the candelabra
    p('ember-crypt-ritual-table', 25, 25); chair(f, 26, 27, 'n'); p(OLDSHELF, 22, 20); p(OLDSHELF, 23, 20); p(SHELF, 30, 20)
    p(CHEST2, 31, 32); p(CHEST2, 29, 33); p('ember-treasure-metal-safe', 22, 33); p(CANDELABRA, 27, 24); p(RUG, 25, 30); p(CANDLE, 22, 26)
    p('ember-treasure-coin-pile', 30, 31); p('ember-treasure-ceramic-jars', 31, 22)
    # the stairs hall x 33-41, y 20-33
    f.stairs(STAIRS_DOWN, 39, 30, 'mosteiro', 'down', back=True)
    f.stairs(STAIRS_UP, 34, 30, 'mosteiro-andar-3', 'up')
    p(BRAZIER, 33, 20); p(BRAZIER, 41, 20); p(BENCH, 36, 22); p('dungeon-penitent-statue', 37, 33); p(CANDLE, 33, 24); p('ember-props-crate-v02', 41, 33)
    # the corridor y 16-18: braziers at both ends, benches, a shrine candle
    p(BRAZIER, 2, 17); p(BRAZIER, 41, 17); p(BENCH, 12, 16); p(BENCH, 28, 18); p('city-shrineCandle', 20, 17); p(RUG, 21, 17)

    BIBLIO = tree('npc-mosteiro-biblio', 'Irmão Bibliotecário', [
        L('l1', 'Silêncio é a primeira regra desta sala. A segunda é não tocar nos livros da estante do fundo.', replies=[
            R('O que há nos livros do fundo?', 'l2'), R('Procuro um peregrino, Tomé.', 'l3'), R('Só estou olhando.', None)]),
        L('l2', 'As Lamentações, nas cópias antigas. O Abade manda copiar sempre o mesmo verso, e queimar as cópias velhas no braseiro do '
                'conselho, lá em cima. Nunca vi o que escrevem nas margens. Nunca quis ver.', None),
        L('l3', 'Os hóspedes dormem no fim do corredor. Veja as camas: quem partiu de madrugada deixou a sacola, as botas, o bordão. '
                'Peregrino não parte sem o bordão. Eu só digo isso: não parte.', None),
    ])
    PEREGRINA = tree('npc-mosteiro-peregrina', 'Peregrina Insone', [
        L('l1', 'Vocês também não conseguem dormir? Eu não durmo desde que cheguei. Tem um canto, à noite, que vem do teto. Do andar de cima, '
                'onde ninguém pode subir.', replies=[R('Que canto?', 'l2'), R('Quem dormia nas camas vazias?', 'l3'), R('Durma. Boa noite.', None)]),
        L('l2', 'Não é reza. Reza sobe. Isso desce. E para no meio, como se alguém engolisse. Depois um sino que não é o sino da sineira. '
                'Depois nada. De manhã falta alguém.', None),
        L('l3', 'Um ferreiro de Vau, um rapaz forte chamado Tomé, dois irmãos lenhadores. "Partiram antes do sol." Deixaram tudo. O '
                'ferreiro deixou até o martelo. Quem deixa o martelo?', None),
    ])
    TESOUREIRO = tree('npc-mosteiro-tesoureiro', 'Irmão Tesoureiro', [
        L('l1', 'Este escritório é dos irmãos. Se vieram pagar a hospedagem, moeda na mesa; se vieram perguntar, a porta é essa.', replies=[
            R('Pagar pelo quê? As camas estão vazias.', 'l2'), R('Para onde vai o dinheiro dos peregrinos?', 'l3'), R('Já vou.', None)]),
        L('l2', 'Vazias são as que já pagaram. O Mosteiro não devolve. Está na regra. Capítulo nove.', None),
        L('l3', 'Para o óleo das lâmpadas, o pão, o ferro... o ferro das correntes dos penitentes. Pedem muito ferro, os de cima. Mais ferro '
                'do que um homem precisa.', None),
    ])
    NOVICO = tree('npc-mosteiro-novico', 'Noviço Trêmulo', [
        L('l1', 'Não, não entre, a cela é minha, é só minha... Vocês não são irmãos. Vocês não são.', replies=[R('Do que você tem medo?', 'l2'), R('Calma. Já vamos.', None)]),
        L('l2', 'Da porta de pedra lá em cima. Levaram o Irmão Davi para o conselho porque ele perguntou. Voltou sem perguntar. Voltou sem... '
                'voltou com os olhos de cinza. Não pergunte nada. Não pergunte.', None),
    ])
    f.person('Irmão Bibliotecário', 'bellCollector', 9, 12, 4, BIBLIO)
    f.person('Peregrina Insone', 'villagerF1', 33, 7, 1, PEREGRINA)
    f.person('Irmão Tesoureiro', 'shadyPatron', 24, 27, 5, TESOUREIRO)
    f.person('Noviço Trêmulo', 'cultist', 7, 23, 2, NOVICO)
    f.person('Irmão Varredor', 'peasant1', 16, 17, 2)
    f.person('Devoto da Cinza', 'cultistV2', 22, 7, 10)
    f.person('Devoto da Cinza', 'cultistV2', 3, 17, 10)
    SPAWN = (38, 32)
    f.person('Kael', 'kaelFinal', *SPAWN, 15, player=True)
    seen = f.check(SPAWN)
    return f.write(base_draft(), seen, title='O Mosteiro das Cinzas · Andar II', place='Biblioteca, sala do capítulo e quartos dos hóspedes',
                   briefing='O andar dos hóspedes. Tudo parece em ordem: a biblioteca copia, o capítulo reza, os peregrinos pagam a cama. '
                            'Mas há camas vazias com as sacolas ao lado, e um canto que vem do teto. O terceiro andar é do conselho: trancado.',
                   objective='Fale com os hóspedes e os irmãos; encontre a escada para o topo', **INDOOR)


# =================================================================================================== Andar III
def andar3():
    f = Floor('mosteiro-andar-3', 34, 28, 'nave')
    f.walls |= ring(1, 1, 32, 26)
    f.walls |= hwall(18, 1, 32)                                          # the hall (rows 19-25) | the council's floor
    f.walls |= hwall(13, 1, 32) | vwall(15, 1, 13)                        # the antechamber (rows 14-17) | the chapter (x 2-14), the chapel (x 16-31)
    for piece, x, y in ((LOCKED, 16, 18), (PORTAL, 7, 13), (PORTAL, 23, 13)):
        f.opening(piece, x, y)
    f.windows = {(5, 26), (11, 26), (21, 26), (27, 26), (1, 22), (32, 22), (1, 9), (32, 9), (4, 1), (10, 1), (19, 1), (25, 1), (29, 1)}
    f.slits = {(1, 15), (32, 15), (15, 1)}
    f.torches = {(3, 18), (9, 18), (13, 18), (20, 18), (25, 18), (30, 18), (1, 20), (32, 20), (8, 26), (24, 26), (16, 26),
                 (2, 13), (12, 13), (18, 13), (28, 13), (15, 5), (15, 10), (1, 4), (32, 5), (7, 1), (22, 1), (31, 1)}
    f.lay_walls()
    p = f.place
    # the arrival hall x 2-31, y 19-25: the stairs down, benches, braziers, the statue of the Saint watching the locked door
    f.stairs(STAIRS_DOWN, 27, 23, 'mosteiro-andar-2', 'down', back=True)
    p(BRAZIER, 2, 20); p(BRAZIER, 31, 20); p(BENCH, 5, 24); p(BENCH, 20, 24); p(SAINT, 12, 20); p(VOTIVE, 9, 20)
    p('ember-interior-cloth-curtain', 22, 20); p(CANDLE, 16, 25); p(RUG, 15, 23)
    # the antechamber y 14-17: chains on the walls, urns, a hook chain, the smell of the braseiro
    p('ember-dungeon-chains', 2, 16); p('ember-dungeon-chains', 31, 16); p('ember-crypt-funeral-urns', 2, 14); p('ember-crypt-hook-chain', 31, 14)
    p(BRAZIER, 15, 15); p('dungeon-penitent-statue', 10, 15); p('dungeon-penitent-statue', 20, 15)
    # the chapter of the Ash x 2-14, y 2-12: the council table with chairs, the relic on its pedestal, bones, chains, the old books
    p('ember-crypt-ritual-table', 6, 6); p('ember-crypt-ritual-table', 8, 6)
    chair(f, 5, 6, 'e'); chair(f, 10, 6, 'w'); chair(f, 6, 4, 's'); chair(f, 8, 4, 's'); chair(f, 6, 8, 'n'); chair(f, 8, 8, 'n')
    p('ember-crypt-artifact-pedestal', 13, 3); p('ember-ember-ancient-reliquary', 13, 5); p('ember-dungeon-bone-pile', 2, 11); p('ember-dungeon-chains', 2, 3)
    p(OLDSHELF, 3, 2); p(OLDSHELF, 5, 2); p(OLDSHELF, 7, 2); p(CHEST2, 13, 11); p(CANDELABRA, 7, 10); p(BRAZIER, 2, 7); p(BRAZIER, 12, 8)
    p('torture-chain-pile', 11, 11); p('dungeon-funerary-urn', 13, 7); p(CANDLE, 4, 11)
    # the Abbot's chapel x 16-31, y 2-12: the altar between penitent statues, the sarcophagus of the "Saint", the cage, pews
    p('ember-dungeon-altar', 23, 3); p('dungeon-penitent-statue', 21, 2); p('dungeon-penitent-statue', 26, 2); p('ember-crypt-sarcophagus-v02', 28, 4)
    p('dungeon-hanging-cage', 17, 3); p(VOTIVE, 19, 4); p('wilds-incense-burner', 25, 5); p(LECTERN, 20, 6)
    for y in (8, 10):
        p(PEW, 18, y); p(PEW, 22, y); p(PEW, 26, y)
    p('ember-crypt-secret-stone-door', 30, 8); p('ember-dungeon-iron-cage', 30, 11); p(BRAZIER, 16, 11); p(CANDLE, 24, 12); p('dungeon-funerary-urn', 17, 12)
    p('ember-dungeon-chains', 31, 3); p(RUG, 22, 6)

    GUARDIAO = tree('npc-mosteiro-guardiao', 'Irmão Guardião', [
        L('l1', 'Até aqui. O topo é do Abade e do conselho. A porta de pedra não se abre para hóspede, nem para irmão, nem para rei.', replies=[
            R('O que o conselho faz lá dentro?', 'l2'), R('Ouvimos um canto à noite.', 'l3'), R('Vamos descer.', None)]),
        L('l2', 'Delibera. Reza. Lê as Lamentações nas margens. E escolhe. Não me pergunte o que escolhe.', None),
        L('l3', 'Então ouviram errado. Aqui não se canta. Aqui se cumpre. Desçam, e não digam a ninguém que subiram.', None),
    ])
    PRIOR = tree('npc-mosteiro-prior', 'Prior Vânio', [
        L('l1', 'Ora. Hóspedes no capítulo. A porta de pedra tinha um guardião... tinha. Sentem-se. Não? Então fiquem de pé, como os '
                'penitentes.', replies=[R('Onde está Tomé?', 'l2'), R('O que vocês fazem com os peregrinos?', 'l3'), R('Isto acaba hoje.', 'l4')]),
        L('l2', 'Tomé. Forte, o Tomé. Bom pulmão. Desceu para servir a Cinza de um jeito que a mãe dele não entenderia. Vocês viram o '
                'pátio? Viram, sim. Ele os viu também. Não os reconheceu. A Cinza é misericordiosa assim.', None),
        L('l3', '"O que arde não se perde; o que se perde é refeito." Nós refazemos. Um homem que chega à nossa porta é cinza que ainda não '
                'sabe. Nós ensinamos. Por baixo da sacristia, onde o fogo não apaga.', None),
        L('l4', 'Acaba? Meu filho, isto começou antes do seu avô. A Cinza tem paciência. Desçam pela sacristia, se têm pressa. Lá embaixo '
                'o Abade recebe melhor do que eu.', None),
    ])
    MESTRE = tree('npc-mosteiro-mestre', 'Mestre dos Noviços', [
        L('l1', 'Noviços novos? Não. Hóspedes. Hóspedes que sobem demais. Deixem-me olhar os seus olhos... Não. Ainda não. Pena.', replies=[
            R('O que você faz com os noviços?', 'l2'), R('Afaste-se.', None)]),
        L('l2', 'Ensino a aguentar. Primeiro o jejum, depois a corrente, depois a cinza na boca. Quem aguenta, sobe. Quem não aguenta, '
                'desce, e aguenta de outro jeito. O Irmão Davi aguentou. Agora nem lembra que perguntou.', None),
    ])
    SUPERIORA = tree('npc-mosteiro-superiora', 'Irmã Superiora', [
        L('l1', 'Shhh. A Santa dorme. Não a acordem com perguntas. Ela já respondeu tudo, há duzentos anos, quando ardeu e não se perdeu.', replies=[
            R('Quem está no sarcófago?', 'l2'), R('Por que a gaiola?', 'l3'), R('Deixe-a dormir.', None)]),
        L('l2', 'Ela. O que sobrou. Cinza e um pouco de osso, e um coração que ainda está quente. Encoste a mão, se duvida. ...Não? Ninguém '
                'encosta. Só o Abade.', None),
        L('l3', 'Para quem canta errado. Um dia e uma noite sem pão e sem chão, e o canto sai certo. Eu mesma cantei errado uma vez. Olhe '
                'para mim: sai certo.', None),
    ])
    f.person('Irmão Guardião', 'cultist', 14, 21, 6, GUARDIAO)
    f.person('Prior Vânio', 'cultistV2', 7, 5, 14, PRIOR)
    f.person('Mestre dos Noviços', 'shadowScholar', 4, 8, 12, MESTRE)  # one talking NPC per look on a map (BattleExport uniqueNeutralNpcSpawns): not a second cultistV2
    f.person('Irmã Superiora', 'crazyLady', 24, 7, 10, SUPERIORA)
    f.person('Penitente Acorrentado', 'chainBoundPenitent', 11, 16, 9)
    f.person('Penitente Acorrentado', 'chainBoundPenitent', 21, 16, 9)
    SPAWN = (25, 24)
    f.person('Kael', 'kaelFinal', *SPAWN, 15, player=True)
    seen = f.check(SPAWN, sealed=[(16, 18), (17, 18)], beyond=[(16, 16), (7, 6), (23, 6)])
    return f.write(base_draft(), seen, title='O Mosteiro das Cinzas · Andar III', place='O topo trancado: o capítulo da Cinza e a capela do Abade',
                   briefing='O último andar. Uma sala de chegada, um irmão de guarda e uma porta de pedra que não se abre para hóspedes. '
                            'Atrás dela delibera o conselho dos Devotos: o Prior, o Mestre dos Noviços, a Irmã Superiora. E a Santa, que dorme.',
                   objective='Abra a porta de pedra e enfrente o conselho com a verdade', **INDOOR)


if __name__ == '__main__':
    only = sys.argv[1:]  # "2" / "3": one floor; nothing: both
    if not only or '2' in only:
        andar2()
    if not only or '3' in only:
        andar3()
