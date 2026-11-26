# Andar com profundidade: estudo do motor e plano

O usuário, 2026-11-25: "está uma bosta total esses angulos nos quais os personagens andam", "analise o engine crie um plano e estudo de
como coordenar angulos e cameras para dar mais profundidade aos movimentos". Este arquivo é o estudo e o plano. Nenhum número de câmera
aqui é palpite: onde falta um número, está escrito que é ele quem escolhe, vendo no jogo.

## 1. Como o motor anda hoje (lido no código)

| Peça | Onde | O que faz |
|---|---|---|
| Tabuleiro | `engine/world/HexMovement.h` | Hex de ponta para cima. Um passo só existe em seis direções do tabuleiro: 0, 60, 120, 180, 240 e 300 graus. Não existe passo reto "para cima" nem "para baixo" do tabuleiro. |
| Caminho | `findTurnPenalizedHexPath` | A* que cobra 0,15 por curva, para preferir trechos retos. Uma reta que não cai numa das seis direções vira zigue-zague de hexes. |
| Posição | `HexStepMover`, `MapStage::walkPoint` | A unidade vai de centro de hex a centro de hex. Desde hoje as quinas são arredondadas (curva pelo meio dos lados do hex). |
| Câmera | `MapStage` (`yaw_`, `pitch_`, `distance_`, `target_`) | Uma câmera só, em órbita. Parada de fábrica: girada 30 graus, inclinada 20. O usuário gira de 15 em 15 graus pelos botões e livre pelo mouse. Os golpes têm os seus enquadramentos (`content/camera.json`: 12 a 30 graus de inclinação). |
| Figura | `MapStage::buildCards`, `addCard` | Cada unidade é uma carta 2D em pé, sempre de frente para a câmera. Espelhar é virar a carta em volta do hex (os pés não saem do lugar). |
| Clipe de andar | `MapStage::walkPose` | Quatro vistas possíveis: `walk` (direita), `walkLeft`, `walkUp` (de costas), `walkDown` (de frente). |
| Velocidade | `MapStage::feetSpeed` | Uma velocidade só para todas as unidades e direções; os quadros do andar seguem o chão percorrido. |

### O que cada unidade tem (build/scenes/roster.json, 126 unidades)

| Clipes de andar | Unidades | O que se vê |
|---|---|---|
| nenhum | 47 | desliza parada na pose de idle |
| só `walk` | 27 | o mesmo andar espelhado para a esquerda; nunca de costas nem de frente |
| `walk` + `walkLeft` | 28 | os dois lados; nunca de costas nem de frente |
| `walk` + `walkUp` + `walkDown` | 12 | lado espelhado, costas e frente |
| os quatro | 9 (Kael, Voss, Aldric, Isolde, Vespera, Nekomata, Troll, Birolho Legs 1 e 2) | tudo que existe hoje |
| `walk` + `walkLeft` + `walkDown` | 2 | sem costas |
| `walk` + `walkDown` | 1 | sem costas |

Ou seja: 102 das 126 unidades não têm vista de costas ou de frente. Nelas nenhum código faz o andar "entrar" na tela: é arte que falta.

## 2. Por que os ângulos ficam ruins (as causas, em ordem de peso)

1. **A vista era escolhida pelo tabuleiro, não pela tela.** Com a câmera girada, o "para cima" do tabuleiro não é o da tela.
   Corrigido hoje: cada passo é lido como a câmera o vê.
2. **A vista era escolhida hex por hex.** Num zigue-zague a unidade trocava de lado, de clipe e de espelho a cada hex.
   Corrigido hoje: vale o rumo (o passo anterior, este e o próximo juntos), e o clipe que já está tocando se segura perto da linha de troca.
3. **O caminho tinha quina em todo centro de hex.** Corrigido hoje: quinas arredondadas.
4. **Só existem quatro vistas, e as seis direções não caem nelas.** É o que sobra, e é o maior. Um passo a 60 graus da horizontal
   mostra a vista de costas reta (30 graus errada) ou a de lado (60 graus errada). Nenhum código resolve: ou a câmera para onde as
   quatro vistas servem, ou entram vistas novas. É o item 3 abaixo.
5. **A câmera não participa do andar.** Ela fica parada enquanto a unidade anda; profundidade vem de a câmera acompanhar. É o item 4.

## 3. Coordenar o ângulo da câmera com as vistas (o centro do plano)

As seis direções do tabuleiro, vistas pela câmera, dependem só de quanto ela está girada:

| Câmera girada | As seis direções na tela | Com as quatro vistas de hoje |
|---|---|---|
| 30, 90, 150 ... graus (de 60 em 60) | 2 retas em profundidade (costas / frente) + 4 a 30 graus da horizontal | costas e frente **exatas**; as outras quatro usam o lado, 30 graus fora |
| 0, 60, 120 ... graus | 2 retas de lado + 4 a 60 graus da horizontal | lado **exato**; as outras quatro usam costas / frente, 30 graus fora |
| qualquer outro (15, 45 ...) | nenhuma direção cai numa vista | todas fora |

A parada de fábrica (30) já é a boa. O giro de 15 em 15 leva a câmera para paradas onde nada casa.

**Proposta A (câmera, decisão do usuário):** o giro dos botões passa a parar só nas paradas de 60 em 60 que casam com as vistas.
O giro livre do mouse continua existindo; ao soltar, a câmera pousa na parada mais próxima.

**Proposta B (arte, a que dá as seis direções de verdade):** dois vídeos novos por unidade, filmados no ângulo exato que a parada
da câmera mostra: andar **de frente a três quartos** (30 graus da horizontal, vindo) e **de costas a três quartos** (30 graus, indo).
Os outros dois lados são esses espelhados (a arma troca de mão, como no andar esquerdo da Isolde) ou, para quem não aceita a troca,
mais dois vídeos. Com A + B, as seis direções têm cada uma a sua vista exata: costas, frente e quatro três-quartos.
O motor já escolhe por rumo; faltaria só ensinar os dois nomes novos de pose (`walkUpSide`, `walkDownSide`), por dado, para todas as unidades.

Sem a Proposta A, a B ainda melhora, mas a vista exata só acontece quando a câmera está numa parada boa.

## 4. Câmera durante o andar (profundidade)

Hoje a câmera só se mexe sozinha nos golpes e nas cenas. Três coisas dão profundidade ao andar; cada uma é uma decisão dele e entra
uma por vez, para ele ver:

1. **Acompanhar:** o alvo da câmera segue a unidade que anda, com atraso suave, em vez de ficar parado.
2. **Aproximar / afastar com o rumo:** andando para o fundo a câmera recua um pouco e baixa; vindo para a frente, aproxima. É o que faz
   o passo "entrar" na cena.
3. **Inclinação mais baixa no andar longo:** os golpes já usam 12 a 16 graus (`camera.json`); o andar usa a inclinação parada (20).
   Mais baixo = mais horizonte e mais diferença de tamanho entre perto e longe.

Quanto de cada um: ele escolhe no jogo. Nada disso é ligado sem ele pedir (as mudanças de câmera de 2026-11-18 foram todas desfeitas
a mando dele).

## 5. O plano, em ordem

| Fase | O quê | Depende de | Estado |
|---|---|---|---|
| 0 | Vista pela tela; rumo em vez de hex a hex; quinas arredondadas; abrir / fechar inteiros | nada | código feito 2026-11-25, **não visto no jogo** |
| 1 | O som do andar (`walk` / `walkLeft`) escolhido pela tela, como a vista (hoje ainda é pelo tabuleiro) | nada | a fazer |
| 2 | Proposta A: paradas da câmera de 60 em 60 | o sim dele | espera |
| 3 | Proposta B: vídeos de três quartos. Primeiro as 9 unidades que já têm as quatro vistas (heróis), depois as 12 com costas e frente | o sim dele e os vídeos | espera |
| 4 | Costas e frente para as 55 unidades que só andam de lado; andar para as 47 que não têm nenhum | os vídeos | espera |
| 5 | Câmera no andar (acompanhar, aproximar, inclinação), um de cada vez | o sim dele a cada um | espera |

## 6. Como conferir a fase 0 no jogo

Num mapa de teste, com a câmera na parada de fábrica, mandar o Kael (tem as quatro vistas):
1. seis hexes em linha para cada uma das seis direções: duas devem mostrar costas / frente, quatro o andar de lado, sem trocar no meio;
2. uma reta que o tabuleiro faz em zigue-zague: um clipe só do começo ao fim, o corpo numa onda suave, sem quina;
3. um L (três hexes, curva, três hexes): a troca de vista acontece uma vez, na curva.
Depois girar a câmera um toque (15 graus) e repetir o item 1: é onde as vistas deixam de casar, o motivo da Proposta A.
