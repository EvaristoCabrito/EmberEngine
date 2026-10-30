# O Mosteiro das Cinzas (2026-10-29)

A área do Mosteiro (o user: "part exploration / adventure, Talk investigation, part dungeon ... villagers claim people are going
missing all too often near that road, its a spooky place with Ashen Devouts and Penitents, they try to keep the fassade, but with
some conversation and investigations you manage to go underground were they not only have dungeons with prisoners but they also
have a lab of sorts for creating horrors and Ember Infused Soldiers such as the chainbound peninents"; depois: "o andar de cima do
monasterio, um pouco menor mas não muito, e um terceiro ainda menor, esses são andares superiores onde as coisas ainda aparentam ser
normais, exceto no topo que está trancado e tem a diretoria macabra do monasterio").

| id | o quê | tamanho | estado |
|---|---|---|---|
| `mosteiro` | térreo: pátio, igreja, claustro, sacristia, escritório, refeitório, dormitório, enfermaria, cemitério, pátio dos penitentes; a porta de pedra trancada da cripta desce; a escada do dormitório sobe | 52 x 46, externo | **feito** (`mosteiro002`) |
| `mosteiro-andar-2` | Andar II: biblioteca, sala do capítulo, quartos dos hóspedes, celas, escritório do tesoureiro, sala das escadas | 44 x 36, Interna acima do solo | **feito** (`mosteiro-andar-2001`) |
| `mosteiro-andar-3` | Andar III, o topo: sala de chegada, o irmão de guarda, a PORTA DE PEDRA TRANCADA; atrás dela o capítulo da Cinza (a diretoria) e a capela do Abade | 34 x 28, Interna acima do solo | **feito** (`mosteiro-andar-3002`) |
| `mosteiro-masmorras` | nível −1, sob a sacristia: a sala da escada, o quarto do carcereiro, o depósito (os pertences dos presos), o corredor, cinco celas TRANCADAS com os prisioneiros, a sala de tortura, a cisterna, o poço dos penitentes e, atrás de outra porta de pedra, a descida | 44 x 34, Subterrâneo | **feito** (`mosteiro-masmorras001`) |
| `mosteiro-laboratorio` | nível −2: a chegada, o escritório do Mestre, o depósito de espécimes, o grande salão de infusão (tanques de Ember entre pilares, o altar com o relicário, horrores soltos), os cercados dos penitentes com o Golem de Cinza e, atrás de porta trancada, a câmara do Mestre Infusor | 38 x 30, Subterrâneo | **feito** (`mosteiro-laboratorio001`; monstros nunca usados como placeholder: `minorHorror`, `horror`, `emberedWraith`, `apparition`, `ancientGolem`, `giantBeetle`, `theButcher`; `asherah`, `undeadOx`, `plagueBearingCattle`, `troll2`, `punisher` ficam para trocar depois) |

Geradores: `tools/scripts/maps/make_monastery.py` (térreo), `make_monastery_upper.py [2|3]` (os andares), sobre `monastery_lib.py`
(hexes odd-r, as paredes do kit, os footprints do catálogo, as pessoas com as conversas, as checagens: nada sobre parede / pessoa /
outra peça, todo NPC, porta e escada alcançável a pé com as portas trancadas abertas, e com elas fechadas o que selam fica selado;
o arquivo escrito como o próximo serial do id, nunca por cima). Só arquitetura bloqueia (paredes, a porta de pedra); as portas do
kit (`-door`) abrem para quem chega; portais e portão são vãos. Free roam com névoa de guerra, nenhum inimigo: exploração.

## Térreo (x = coluna, y = linha)

```
 precinto   muralha do kit em x 1 / 50 e y 1 / 41; portão de 4 hexes em x 24-27, y 41; a estrada fora (linhas 42-45), o grupo chega
            em (25, 44), a Mulher da Estrada ao lado
 igreja     x 19-33, y 3-20: nave (bancos, corredor central x 25-26, pilares, tochas), presbitério (altar, a Santa, relicário,
            braseiros, estante de leitura, grade de velas); porta sul (25-26, 20)
 sacristia  x 35-47, y 3-9, porta norte para o beco de trás (40-41, 3); mesa de ritual (o segundo livro), baús, jarros
 escada da cripta x 35-47, y 9-15: a PORTA DE PEDRA TRANCADA entre sacristia e antecâmara (dungeon-stone-door em 40-41, 9:
            "Arrombar" abre, como qualquer fechadura), a escada que desce + floor-connector para `mosteiro-masmorras`
 escritório x 4-17, y 3-9, porta sul (10-11, 9); claustro x 5-16, y 11-24 (pilares, poço, árvores mortas, bancos)
 refeitório x 4-17, y 26-34, porta norte (10-11, 26); dormitório x 35-47, y 17-24, porta sul (40-41, 24), três celas e o corredor
            y 22-23 com a ESCADA QUE SOBE para o Andar II em (45-46, 22)
 enfermaria x 35-47, y 26-34, porta norte (40-41, 26): camas dos "febris", um biombo escondendo a mesa de ritual e a jaula
 cemitério  x 35-48, y 36-40; pátio dos penitentes x 3-17, y 36-40 (pilar acorrentado, jaulas, correntes, três Penitentes)
```

## Andar II

Corredor nas linhas 16-18 atravessando o prédio. Ao norte: a biblioteca (x 2-15: estantes antigas em três paredes, escrivaninhas,
duas estantes de leitura), a sala do capítulo (x 17-28: a estante do leitor, bancos em fileiras, estátuas penitentes, braseiros), os
quartos dos hóspedes (x 30-41: camas, algumas com a sacola e as coisas do dono ao lado e ninguém nelas). Ao sul: cinco celas dos
monges (x 2-20, uma cama, um banquinho, uma vela, um baú), o escritório do tesoureiro (x 22-31: a mesa dos livros, cofres, moedas), a
sala das escadas (x 33-41: a que desce para o térreo em (39-40, 30), a que sobe para o Andar III em (34-35, 30)).

## Andar III (o topo)

A sala de chegada (linhas 19-25: a escada que desce em (27-28, 23), bancos, braseiros, a Santa olhando a porta, o Irmão Guardião) e a
PORTA DE PEDRA TRANCADA em (16-17, 18). Atrás dela a antecâmara (linhas 14-17: correntes, urnas, dois Penitentes de guarda) e os
dois salões: o capítulo da Cinza (x 2-14: a mesa do conselho com seis cadeiras, o relicário no pedestal, ossos, correntes, os livros
antigos; o Prior Vânio e o Mestre dos Noviços) e a capela do Abade (x 16-31: o altar entre estátuas penitentes, o sarcófago da
"Santa", a gaiola suspensa, bancos, a porta secreta de pedra no fundo; a Irmã Superiora).

## Os níveis subterrâneos (2026-10-30; o user: "design the two underground levels", "sub solo dungeon levels")

`tools/scripts/maps/make_monastery_underground.py [1|2]` (a mesma biblioteca, agora com `person(..., enemy=True)` → `enemySpawns`).
Os dois são **Subterrâneo** (`environment: indoor` + `underground: true`: sem sol nem lua; só tochas, braseiros e o brilho do Ember),
névoa de guerra ligada: exploração livre até um inimigo ser avistado (a regra do free roam), aí combate. O kit do Mosteiro nas
paredes; a decoração com o que o catálogo tem hoje (os kits cripta / minas saíram: `700391d8`): tortura (`torture-*`), caverna
(`cave-*`), ossários (`dungeon-*`), as peças de Ember (`ember-ember-containment-box`, `-ember-lantern`, `-ancient-reliquary`), jaulas,
Poly Haven (mesas, estantes, barris, caixas, o martelo, o machado) e os crânios do Smithsonian na mesa do Mestre.

**Masmorras (−1, 44 x 34)**, em quatro faixas: A (y 2-9) sala da escada que sobe (x 2-12; `returnConnector` para o térreo), quarto
do Carcereiro (x 14-24: mesa, chaves, baú, cama), depósito (x 26-42: caixas e barris com os pertences dos presos: o martelo do
ferreiro, o machado dos lenhadores, um escudo); B (y 11-13) o corredor (dois guardas, um cão de guerra); C (y 15-21) cinco celas de
3 hexes (x 2-20, y 15-17) atrás de portas de pedra TRANCADAS que abrem para o sul, para o corredor das celas (y 19-21); a leste a
sala de tortura (x 22-30: pelourinho, grilhões, braseiro, jaula, o Carrasco) e a cisterna (x 32-42: a poça subterrânea, ossos,
cogumelos); D (y 23-31) o poço dos penitentes (x 2-20: pilares acorrentados, jaulas, três Penitentes Acorrentados e o Guardião do
Poço) e a descida (x 22-42), atrás de porta TRANCADA na cisterna (36-37, 22): a escada para o laboratório, o Guardião da Descida e um
Penitente. Presos que falam: **Tomé** (cela 1), **Ferreiro de Vau** (2), **Lenhador** (3), **Irmão Davi** (4); a cela 5 só tem ossos.
Inimigos: 2 cultistV2 guardas-chefe, 3 cultist, 1 wardog2, 4 chainBoundPenitent, o Carrasco e o Guardião da Descida (cultistV2).

**Laboratório (−2, 38 x 30)**: a chegada (x 2-10, y 2-8: escada que sobe, `returnConnector`), o escritório do Mestre (x 12-24:
estantes, mesa com crânios, o relicário, o cofre), o depósito de espécimes (x 26-36: jaulas, o Besouro Gigante, ossos, caldeirão); o
grande salão de infusão (x 2-36, y 10-21: oito pilares, dez tanques de Ember em duas fileiras, o altar com o relicário, lanternas de
Ember, cristais azuis e geodos, correntes; dois Devotos Infusores, dois Horrores Menores, um Espectro de Brasa, uma Aparição, um
Horror); os cercados (x 2-18, y 23-28: jaulas, pilar acorrentado, um tanque, três Penitentes sendo feitos e o Golem de Cinza);
a câmara do Mestre Infusor (x 20-36), atrás de porta TRANCADA (27-28, 22): o sarcófago da "Santa", estátuas penitentes, o relicário,
o Mestre Infusor (cultistV2, nível 18), O Açougueiro e um Horror. Sem NPC neutro.

Limites: libertar um preso = abrir a cela e falar (nenhuma mecânica de resgate / escolta existe); o chefe não encerra o mapa (os mapas
de exploração terminam só num waypoint: a escada); os monstros são placeholders do user ("depois troco por mais apropriados").

**Portas trancadas em 3D (2026-10-30, o user: "we have real 3D doors, stop using 2D props"):** a porta de pedra 2D
(`dungeon-stone-door`) saiu dos cinco mapas; no lugar, a peça nova do kit `ember-arch-mosteiro-door-locked` (architecture.json
`"locked": true`, par de hexes, a mesma porta animada do kit), uma fechadura: o exportador a lista entre os `locks` do combate
("Arrombar" ao lado dela, kind doorDecor, no hex âncora: BattleExport `lockedKitDoors`, lidos do catálogo do editor), e o MapStage
mantém as folhas fechadas e as células sólidas até a Gazua abri-la (`DoorState::locked`, `updateDoors` / `lockOpenedAt`); daí em diante
abre para quem chega, como as outras. Mapas: `mosteiro008`, `mosteiro-andar-3004`, `mosteiro-masmorras002`, `mosteiro-laboratorio002`
(`tools/scripts/maps/swap_monastery_locked_doors.py`: o serial mais novo de cada id com as portas trocadas, nada mais; o Andar II não
tinha porta trancada). `monastery_lib.LOCKED` é a peça nova. Limite: a fechadura fica só no hex âncora (o da esquerda) da porta de
dois hexes; o editor não tem Abrir porta / Fechar passagem para ela (sem doorVariants).

**Escadas em combate (2026-10-30, o user: "não seja possível usar escadas no meio do combate, apenas warp", "deve ter uma mensagem
avisando"):** em todo mapa, com os turnos rodando (`!battle_->roaming()`), parar num conector de andar abre só o aviso "Você não pode
continuar em meio ao combate." com um Ok (`MapStage::drawFinishPrompt`); no movimento livre a pergunta Sim / Não de sempre. Provado nas
Masmorras: `captures/mosteiro/stairs-turns.png` (Kael em (6, 5) com o Carcereiro à vista) e `stairs-roam.png` (`--fog 1`, movimento livre).

## A investigação (as conversas, nos próprios mapas: `neutralSpawns[].dialog`)

| quem | arte (placeholder) | onde | o que deixa escapar |
|---|---|---|---|
| Abade Orsino | `cultistV2` | térreo, presbitério | a fachada: lobos, febre, penitentes voluntários; "a cripta é selada, só os irmãos descem" |
| Irmão Porteiro | `cultist` | térreo, portão | a ala leste é fechada; lá em cima a biblioteca e os hóspedes; "o terceiro andar é do Abade e do conselho: trancado" |
| Irmão Escriba | `shadowScholar` | térreo, escritório | o livro de hóspedes ditado pelo Abade; nunca viu ninguém partir; o segundo livro na sacristia |
| Irmã Hospitaleira | `soupLady` | térreo, enfermaria | as correntes "para os febris"; "não passe do biombo" |
| Irmão Coveiro | `ratCatcher` | térreo, cemitério | sete este mês, nunca um caixão; "guardam lá embaixo, pela sacristia; descem, não sobem" |
| Peregrino Febril | `lostCourier` | térreo, enfermaria | levam o mais forte pela sacristia, por uma porta de pedra; o Tomé, e um acorrentado novo "com a cara dele" |
| Mulher da Estrada | `villagerF1` | fora do portão | o filho Tomé entrou por esse portão há dez dias |
| Penitente Acorrentado | `chainBoundPenitent` | térreo, pátio | "lá embaixo fazem mais de nós" |
| Irmão Bibliotecário | `bellCollector` | Andar II, biblioteca | as cópias velhas queimam no braseiro do conselho; "peregrino não parte sem o bordão" |
| Peregrina Insone | `villagerF1` | Andar II, hóspedes | o canto que desce do teto à noite, o sino que não é o da sineira; o ferreiro deixou o martelo |
| Irmão Tesoureiro | `shadyPatron` | Andar II, escritório | "vazias são as que já pagaram"; "pedem muito ferro, os de cima" |
| Noviço Trêmulo | `cultist` | Andar II, cela | o Irmão Davi perguntou e voltou com os olhos de cinza |
| Irmão Guardião | `cultist` | Andar III, sala | "aqui não se canta, aqui se cumpre" |
| Prior Vânio | `cultistV2` | Andar III, capítulo | a máscara cai: Tomé "desceu para servir a Cinza"; "nós refazemos ... por baixo da sacristia" |
| Mestre dos Noviços | `shadowScholar` | Andar III, capítulo | o jejum, a corrente, a cinza na boca; quem não aguenta desce |
| Irmã Superiora | `crazyLady` | Andar III, capela | o sarcófago: "um coração que ainda está quente"; a gaiola para quem canta errado |

Regra do exportador: **um NPC com fala por aparência em cada mapa** (BattleExport `uniqueNeutralNpcSpawns`: o segundo NPC com fala e
o mesmo sprite é descartado; foi o que tirou o Mestre dos Noviços do `andar-3001` enquanto era um segundo `cultistV2`).

Limites de hoje: as conversas não gravam nada (nenhuma bandeira, nenhuma pista contada); as portas de pedra abrem só pela Gazua
(Arrombar). Um gate por pista (a porta abre depois de N conversas, ou alguém entrega a chave) é código novo: só com o "faz".

## O kit 3D do Mosteiro (`content/models/mosteiro`, `blender/kit_mosteiro.py`, `render_review.py`; nomes em `names.json`)

| id | o quê | v02 (tris) |
|---|---|---|
| `ember-mosteiro-pew` | banco de igreja: trilho moldurado, painéis de ponta com quadrifólio, assento de tábuas, genuflexório (par) | 20 208 |
| `ember-mosteiro-lectern` | estante de leitura: base octogonal, coluna torneada, tampo inclinado, o livro aberto | 15 608 |
| `ember-mosteiro-votive-rack` | grade de ferro forjado em três degraus, montantes torcidos, velas votivas pingadas, acesas (luz própria) | 20 420 |
| `ember-mosteiro-saint-statue` | a Santa da Cinza: figura velada em pedra sobre plinto octogonal, dobras, mãos abertas | 20 734 |
| `ember-mosteiro-bell-cote` | sineira: pilares de blocos de calcário, viga de carvalho com cintas de ferro, o sino de bronze, a corda (par) | 107 788 |
| `ember-mosteiro-scriptorium-desk` | escrivaninha inclinada com juntas cavilhadas, prateleira de livros, folhas, tinteiro, penas, vela | 8 480 |

Materiais: as fotos do user (`textures/wt-limestone`, `wt-dark-oak`, `wt-iron`, `wt-pillar`) com relevo da própria foto. Renders de
revisão: `captures/models/mosteiro-review/<name>-v02-{three-quarter,front,detail,base}.png`.
**O user recusou os v01 (apagados, 2026-10-29) e os v02 como arte final ("awful low poly trash ... you know how to do much better
3D"): os v02 ficam de placeholder nos mapas até as versões finais.** Caminhos para a arte final (perguntado, sem resposta ainda):
Meshy (texto / imagem → malha, o caminho da lareira de pedra: `export_hearth.py`), TripoSR (`import_triposr.py`, imagem → malha
no CPU), ou uma biblioteca aberta (Poly Haven, CC0). **Ainda não cozinhados num pack de meshes** (JOGAR monta `props-v12.pak`; o
staging `build/meshes/src-v2/ember/mosteiro/` já tem os GLBs): até o cook, os mapas avisam "model ... is not in the mesh pack" e as
peças não aparecem (o mesmo aviso para `ritual-brazier-v02`, `dead-oak-v02`, `funeral-urns-v02`: props do catálogo que faltam no v12).

Arquitetura: `ember-arch-mosteiro-wall / -window / -slit / -wall-torch / -portal / -door / -stairs-up / -stairs-down / -gate`
(`content/models/architecture.json`): a geometria da Torre de Vigia com a textura `gothic-stone-v01.jpg` até a do Mosteiro chegar
(abaixo). Nenhum recook do architecture.pak foi preciso.

## A iluminação da Torre com o pátio ao céu (2026-10-29; o user: "esses são mapas externos e devem usar um sistema similar ao da
Torre ... sistema de iluminação", depois "pode fazer assim, vai ser util na cidade e fazenda")

Os três andares são "Interna acima do solo" (`environment: indoor` + `aboveGround`): a atmosfera única em cima, a luz do sol e da
lua só pelas janelas, seteiras e portas, os probes, a laje de sombra que nunca se desenha (a ilusão de corte da Torre). No térreo o
pátio, o claustro e o cemitério ficam ao céu:

- **`openAir`** (ArchitectureDef, `architecture.json`): uma **muralha** (`ember-arch-mosteiro-rampart`, `-rampart-torch`,
  `-rampart-gate`): junta-se às vizinhas, bloqueia e faz sombra como qualquer parede, mas **não fecha sala**: o que ela cerca fica
  fora (sem laje em cima, hexes fora da sala dos probes), e ela não ganha rodapé de sombra. O precinto do térreo é dela
  (`Floor.ramparts` em `monastery_lib.py`).
- **Uma laje por sala** (`MapStage::buildRoof`): as salas são os grupos 4-conexos dos hexes fechados por paredes de prédio
  (`roomCells_`, o flood a partir da borda do tabuleiro passando por cima das muralhas); cada grupo recebe a sua laje (o
  retângulo dos hexes, mais uma fila de paredes, mais `kRoofMargin`). Um mapa de uma sala só (a Torre) tem uma laje, como antes;
  um mapa sem sala fechada (caverna) tem a laje sobre todas as peças, como antes. `roofRect_` (o volume dos probes) é a união.
  Limite: uma sala em L recebe a laje do seu retângulo (um canto de pátio a mais sob a laje).
- **Fora da sala, a luz de qualquer mapa** (`lighting.hlsli`): o portão dos probes (`probeRoom`) antes zerava o sol e o céu em tudo
  que estava fora da sala ("100% interior"); agora fora da sala o sol entra com as suas sombras (na Torre os rodapés e o plinto
  continuam a tirar o sol das faces externas) e o céu ilumina (a irradiância do próprio céu, o segundo registro do CSAmbient, à
  exposição do frame: o ambiente dos passes é 0 num mapa com sala). Consequência na Torre: as faces externas da muralha e os lados
  do tabuleiro, antes pretos, recebem o ambiente do céu (captura `captures/mosteiro/watchtower-gate-floor010-day14.png`); o
  interior não muda. O user julga.
- A mesma marca serve à cidade e à fazenda: muralha = `openAir`, casas = paredes do kit.
- Capturas às 14 h (`--game-hour 14`; `--hour` é outra opção): `captures/mosteiro/mosteiro004-day14.png` (pátio, claustro e
  cemitério ao sol, salas escuras com as tochas, 6 lajes, 599 hexes de sala) e a Torre acima.

## Texturas e imagens que o user gera (prompts)

Entregar em `Attachments/mosteiro/` com estes nomes (JPG, qualidade máxima). Depois: copiar para `content/textures/walls/` (a parede)
e `content/ground/` (o piso), trocar o `wallTexture` das nove peças `ember-arch-mosteiro-*` para
`/ember/textures/walls/mosteiro-v01.jpg`, rodar `python -I tools/scripts/models/catalog_models.py`, cozinhar o architecture.pak;
o piso vira um conjunto `emberTiles` (make_tile.py → `kEmberGroundTiles` → o draft, nova versão do mapa); o panorama entra em
backdrop-fit.json como entrada do mapa (regra do panorama: locked, só com a ordem).

| arquivo | onde vai | tamanho | mapeamento |
|---|---|---|---|
| `mosteiro-wall-v01.jpg` | todas as paredes, janelas, seteiras, tochas, portais, portão e escadas do kit | 2048 x 2048, **seamless nos 4 lados** | projeção por face alinhada ao mundo: o quadrado cobre 3,2 x 3,2 unidades e repete; as emendas entre peças caem no meio da imagem: desgaste uniforme, sem gradiente de cima para baixo |
| `mosteiro-floor-v01.jpg` | o piso da igreja e das salas (tile `emberTiles`), nos três andares | 2048 x 2048, **seamless nos 4 lados** | um hex de chão ≈ 1,73 unidades; o tile cobre ~3,2 unidades e repete |
| `mosteiro-panorama-v01.jpg` | o panorama do térreo (atrás do tabuleiro, o horizonte na linha de 0,5 da altura) | 8192 x 2048 (4:1) ou 6144 x 2048 | céu pintado (o motor usa o modo com a linha do horizonte); o chão da pintura some atrás do board |

### 1 · Pedra das paredes — `mosteiro-wall-v01.jpg`

```
A photograph of a square of the outer wall of an old mountain monastery, photographed square-on with the lens axis perpendicular to the wall, the frame filled edge to edge by the stonework alone, the courses running horizontally. Coursed ashlar of grey-brown limestone, each block a different length, the faces dressed with a fine diagonal chisel tooling softened by centuries, the joints narrow and recessed in a pale lime mortar gone grey, a few blocks replaced long ago in a slightly darker stone. Photographed with a 100mm macro lens on a full-frame sensor at f/11, focus stacked, lit by a large diffused softbox from the upper left standing in for the last cold light of dusk, with a faint warm fill from the lower right as if from a torch below. Construction and history: a thin veil of grey ash has settled into every joint and tooling groove and darkened the lower halves of the blocks evenly; black soot tongues rise from nowhere in particular, faint, as if from fires that burned too close; lichen in grey-green rosettes on two blocks; a hairline crack crossing one block and stopping at the joint; old iron pin holes rusted orange; a carved mason's mark, a small circle with a dot, on one block; no plants, no windows, no edges of the wall. Surface truth at close range: the limestone's fossil specks and grain, sand standing proud of the mortar, dust caught in every recess, the high points of the blocks rubbed smooth and faintly polished. Natural sensor grain, true-to-life colour response, gentle highlight rolloff, raw file neutral grade, the frame reading as a photograph of a real wall. The image is a seamless tile: its left edge continues into its right edge and its top edge into its bottom edge, every block, joint and stain that reaches a border carrying on from the opposite border at the same place, the courses meeting exactly, the tone one even level across the whole square with the same density of marks everywhere.
```

### 2 · Piso da igreja — `mosteiro-floor-v01.jpg`

```
A photograph of a square of the stone floor of an old monastery church, photographed straight down with the lens axis perpendicular to the floor, the frame filled edge to edge by the paving alone. Large worn flagstones of grey limestone laid in irregular rectangular courses, each slab a different size, the joints narrow and dark, the slabs hollowed by centuries of feet and knees into shallow dishes, the edges rounded; two slabs are old grave slabs set flush into the floor, their inscriptions worn to ghosts of letters and a faint carved cross. Photographed with a 100mm macro lens on a full-frame sensor at f/11, focus stacked so the whole square stays sharp, lit by a large diffused softbox from the upper left standing in for the dim light through tall windows, with a faint warm fill from the right as if from candles. Construction and history: a thin even film of grey ash over everything, swept into the joints and the hollows; drips of old candle wax gone grey in three places; a dark stain in one corner that was scrubbed and never came out; a crack across one slab held by the mortar; the wear even across the whole square. Surface truth at close range: the limestone's grain and fossil specks, grit in the joints, the polish of the worn tops catching the key light, the dishes matte with dust. Natural sensor grain, true-to-life colour response, gentle highlight rolloff, raw file neutral grade, the frame reading as a photograph of a real floor. The image is a seamless tile: its left edge continues into its right edge and its top edge into its bottom edge, every slab and joint that reaches a border carrying on from the opposite border at the same place, with the same density of slabs and wear everywhere and no visible repetition.
```

### 3 · Panorama do Mosteiro — `mosteiro-panorama-v01.jpg`

```
A photograph, very wide panoramic format 4:1, of the landscape around an old mountain monastery at dusk, seen from the monastery's own courtyard at eye level, the horizon exactly at the vertical centre of the frame, no part of the monastery itself in view. Beyond the near ground the land falls away into a valley of dead grey grass and ash-covered scrub, a dirt road winding down into it and out of sight; dark pine forest climbs the far slopes, burnt in patches to black standing trunks; a line of bare grey mountains closes the horizon, their tops lost in low cloud; a thin column of smoke rises somewhere in the valley. The sky is the last of a cold overcast dusk, slate grey-blue with a dull amber band low on the western side, no sun disc, no stars; a few crows. Photographed with a 35mm lens on a full-frame sensor at f/8, the whole depth in focus, the exposure set for the sky so the land sits a stop dark, ash-grey and desaturated, with the amber of the dusk catching only the edges of the clouds and the far ridges. Natural sensor grain, true-to-life colour response, gentle highlight rolloff, raw file neutral grade, the frame reading as a photograph of a real place. The foreground ground at the bottom of the frame is flat trodden earth with grey ash, so it can sit under a game board; the left and right edges of the image continue into each other seamlessly.
```

## Como abrir os mapas (hoje)

Ainda não estão no mapa-múndi (campaign.json / overworld: sem localização, sem missão). No jogo: Modo teste → Map Editor → abrir
`mosteiro002` / `mosteiro-andar-2001` / `mosteiro-andar-3002` → Testar. Por linha de comando (arquivos de jogo feitos com o Cooker;
`<m>` = um desses três):

```
build\release\bin\Cooker.exe prepare-map content\maps\ember\<m>.json build\test-maps\maps\<m>.json
build\release\bin\Cooker.exe export-scene content\data\board.json content\data\editor-catalog.json content\assets build\test-maps\maps\<m>.json build\test-maps\scenes\<m>.scene.json
build\release\bin\Cooker.exe export-battle build\test-maps\maps\<m>.json build\test-maps\battles\<m>.battle.json --player-level 15
Sandbox.exe --map build/test-maps/maps/<m>.json --scene build/test-maps/scenes/<m>.scene.json --battle-rules build/test-maps/battles/<m>.battle.json --ground build/ground/ground-v55.pak --sprites build/sprites/battle.pak --meshes build/meshes/props-v12.pak --heroes build/scenes/roster.json --game-assets content/assets
```

**No mapa-múndi (2026-10-29; o user: duas casas à direita da Fazenda):** hex **(3, 5)**, pino x 30,31 y 37,5, acesso livre
(`openAccess`), os andares como `submaps` (floor 2 / 3). Escrito por `tools/scripts/data/add_monastery_location.py` (idempotente) em
`content/data/overworld.json` (o pino), `content/data/campaign.json` (a localização + as missões `mosteiro`, `mosteiro-andar-2`,
`mosteiro-andar-3`, índice 64, textos dos drafts mais novos, `mapFile` = o serial mais alto na hora: o jogo abre sempre o mais novo)
e `content/data/editor-catalog.json` (`editor.locations` + `editor.locais.submaps`: a lista Locais do editor). No Modo teste RPG o
pino abre a missão (captura `captures/mosteiro/worldmap-location.png`). `mosteiro005` é um save do user no editor (03:50): é o que
o jogo abre. Não rodado: `Cooker check-overworld` (o Cooker.exe não estava em `build/release/bin` nessa hora: build de outra sessão).

Verificado (2026-10-29, release, `captures/mosteiro/`): os três abrem (térreo `456 architecture pieces (456 built)`, `14 of 14 units in
play`; Andar II `330 built`, `8 of 8`, window light + probes; Andar III `180 built`, `7 of 7`, window light + probes). Não verificado:
as conversas na janela, as portas trancadas abertas com a Gazua, as escadas entre andares em jogo (só o caminho do código), o conector
da cripta (o mapa das masmorras não existe: o jogo só avisa no log), o look (o user julga).
