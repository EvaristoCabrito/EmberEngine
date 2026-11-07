# CENÁRIO 4 — STONE BRIDGE (versão cinematográfica)

MCs: KAEL · NEEERA · VOSS · Escolta: LIO · Antagonistas: THE BUTCHER · VORA, o Cão de Guerra

Zonas: [A] Saída do Poço → [B] Cabeceira Sul (estátuas sem cabeça) → [C] Vão Central (arco rachado, sem parapeito) →
[D] Torre de Pedágio → [E] Cabeceira Norte (bloqueada). Embaixo: um rio de cinza líquida, laranja por dentro, correndo devagar.

## O que muda em relação aos cenários 1–3

Nos três anteriores (Vau, Bosque Morto, Aldeia) a câmera é a do jogador, com `camera: actors`: o pivô desliza até o meio do grupo e
pronto. Vê-se tudo de cima, de longe, da mesma altura, o tempo inteiro. Funciona para apresentar, não para ameaçar.

Aqui o cenário é uma linha (a ponte), um buraco (o abismo) e uma luz vinda de baixo (o rio laranja). A câmera trabalha com isso:

1. **A ponte é um fio.** Os planos gerais são sempre **ao longo** da ponte, nunca de lado. A neblina fecha o fundo do quadro: o norte
   não existe até o Butcher aparecer nele.
2. **O abismo está sempre no quadro.** Nas cenas de borda a câmera fica **do lado de fora** da ponte, sobre o vazio, olhando para
   dentro: a borda sem parapeito é a base do quadro e o laranja do rio ilumina os rostos por baixo.
3. **Escala pela entrada no quadro.** Vora nunca ganha um plano "de apresentação". Ela entra em quadros enquadrados para humanos e
   não cabe. A câmera recua, ela ainda não cabe.
4. **A câmera para quando alguém decide.** Desde o primeiro CLINK a câmera não fica parada um segundo. Ela só trava três vezes: na
   promessa de Kael, no "Não interessa mais" e no plano fixo do "mais tarde".
5. **Só duas vezes olhamos de cima.** Quando Vora cai (o corpo atravessado na ponte) e quando o Butcher está pendurado (a escolha).
   O resto é altura de peito ou mais baixo. "Por cima" vira um evento, não o padrão.
6. **Barras de cinema** (letterbox 2.35:1) do primeiro ao último plano: a cena se declara filme.

## Gramática usada (e o que ela é no engine)

Toda câmera abaixo é a câmera de ação do MapStage (`ShotKey`: pivô, giro, inclinação, distância, elevação, chave de tempo), a mesma
dos golpes. Nada aqui pede um engine novo, só que a cutscene possa escrever essas chaves.

| Termo no roteiro | O que é | No engine |
|---|---|---|
| GERAL | a ponte inteira ou o vão, figuras pequenas | distância longa (≈12 ou o máximo do mapa), inclinação média |
| MÉDIO | 2–4 figuras do joelho para cima | distância ≈6, elevação 0,4 da carta |
| CLOSE | uma carta a 80% da altura do quadro | `closeDistance` (≈3,5), elevação 0,38 da carta (o peito) |
| OTS | por cima do ombro de A olhando B | `twoShot`: pivô entre os dois, giro com `kShotShoulder` |
| RASANTE | câmera baixa, olhando para cima | inclinação no mínimo do mapa, pivô nos pés, elevação 0 |
| DE CIMA | plongée | inclinação no máximo (`kMaxPitch`) |
| DOLLY-IN / OUT | aproxima / afasta sem cortar | distância × `kShotPushIn` (ou o inverso) numa chave de tempo |
| TRAVELLING | o pivô viaja | pivô de [x,y] a [x,y] ao longo de N s, giro e inclinação fixos |
| PAN | gira no lugar | só o giro muda entre duas chaves |
| CORTE | troca instantânea de plano | chave sem deslize (o `cam_` salta para o goal) — **não existe ainda** |
| DESLIZA | a transição normal, suave | o glide de sempre do `cam_` |
| TRAVA | câmera parada, nenhuma chave nova | nada: é a ausência de movimento, e vale como direção |
| SHAKE n | tremor da tela por n s | um offset aleatório no pivô — **não existe ainda** |
| TINT n% | escurece a imagem | — **não existe ainda** |
| BARRAS | letterbox | — **não existe ainda** |
| FADE | preto para a cena e volta | a cortina do mapa, já existe no load |

Passos de `ember-cutscene-1` usados que já existem: `move`, `face`, `say`, `wait`, `enter`, `exit`, `speaker: "???"`.
Passos que este roteiro pede e o formato não tem: `shot` (as chaves acima), `cut`, `shake`, `tint`, `bars`, `sfx`, `bgm`, `emote`,
`status` (a caixa vermelha/azul). Estão listados no fim.

Convenção de tempo: `[2,0 s]` = o plano dura isso antes do próximo. Sem marcação = dura o que a fala durar.

---

## CENA DE ABERTURA — "O Pedágio"

**P01 · PRETO** `[3,0 s]`
Tela preta, BARRAS entram. SFX: escada de ferro rangendo, longe, com eco de poço. Uma respiração (Voss). Depois ferro contra
pedra: o alçapão. Nenhuma imagem ainda. O jogador só ouve.

**P02 · DE CIMA, o rio** `[4,0 s]`
FADE para a primeira imagem, e ela não é o grupo: é o rio. Câmera no máximo de inclinação, apontada para o abismo ao lado de [A],
distância média. Cinza líquida escorrendo, laranja por dentro, lenta. Sem música. SFX: o rio, um borbulhar grave, de longe.
O quadro inteiro é cinza e laranja. Nada vivo.

**P03 · GRUA, o alçapão** `[5,0 s]`
Sem corte: a inclinação baixa e o pivô desliza do abismo para o alçapão em [A]. A pedra se abre. O grupo sai, **pequeno** no canto
inferior do quadro; atrás deles, a ponte entra em quadro pela primeira vez: um fio de pedra que some na neblina.
A câmera não os espera.

**P04 · GERAL, "o fio"** `[4,0 s]`
PAN lento de [A] para o norte, ao longo da ponte. A neblina fecha o quadro antes da metade. A cabeceira norte não existe.
SFX: vento baixo, o rio embaixo. BGM entra: só um bordão grave, sem melodia.

**P05 · MÉDIO, Voss**
DESLIZA de volta para o grupo, à altura do peito, por trás de Kael (Kael é um ombro desfocado na esquerda do quadro).

VOSS: (se apoia nos joelhos)
Escada, túnel, mais escada… Se eu nunca mais vir um degrau na vida, tá ótimo.

**P06 · RASANTE pelo lado de fora, Lio na borda**
Lio corre até a borda. CORTE: a câmera está **sobre o vazio**, baixa, olhando para cima. Lio aparece do peito para cima contra o céu
cinza; o laranja do rio ilumina o rosto dela por baixo. Emote "!".

LIO: O rio… está correndo. Cinza correndo como água.

NEEERA (fora de quadro): Sai da borda, Lio. Aquilo não devolve nada que cai.

**P07 · CLOSE, Lio** `[1,5 s de silêncio antes da fala]`
DOLLY-IN leve. Ela não se mexe.

LIO: Será que levaram eles por aqui? A minha mãe e o Tavi?

**P08 · MÉDIO, os quatro** `[3,0 s]`
Ninguém responde. A câmera recua para um médio de lado, a ponte atravessando o quadro. Neera entra, pega o braço de Lio e a traz
para longe da borda, devagar. O plano segura mais do que o confortável.

**P09 · GERAL, Kael entre as estátuas** `[a caminhada]`
Kael sai do quadro. CORTE para [B]: câmera baixa, no eixo da ponte, as duas estátuas sem cabeça enquadrando o centro como um
portal. Kael entra nesse centro, caminhando para a câmera, e para entre elas. Simetria exata.

**P10 · CLOSE, Kael**
DOLLY-IN **muito** lento que atravessa as três falas seguintes sem cortar. O maxilar travado. O laranja do rio na borda do rosto.

NEEERA (fora de quadro): Kael… você não abriu a boca desde o poço.

KAEL: Estou contando.

NEEERA (fora de quadro): Contando o quê?

KAEL: Os ganchos daquela praça. Trinta e dois.
(pausa `[1,5 s]`; o dolly continua)
Odrin falou em cento e quatro pessoas. Sobraram onze. E eu deixei ele descer por uma grade.

**P11 · OTS Voss → Kael**

VOSS: Ninguém deixou nada. O teto caiu em cima dele.

**P12 · CLOSE, Kael** (o mesmo dolly, já no limite)

KAEL: E ele levantou. Da próxima vez eu não confio em teto.

**P13 · GERAL, a neblina** `[2,5 s]`
SFX: **CLINK… CLINK…**
CORTE seco para o norte: a ponte vazia entrando na neblina, câmera à altura do peito, no eixo. Nada no quadro. Só neblina e o som.
A câmera **não** mostra os heróis reagindo primeiro; mostra o lugar de onde vem o som.

**P14 · MÉDIO, os quatro de frente** `[2,0 s]`
CORTE de volta: os quatro numa linha, de frente para a câmera, olhando para além dela (para o norte). Emote "!" em todos, ao mesmo
tempo. A câmera está onde o som está. Eles olham para nós.

LIO: (sussurrando, apertando a pétala)
…Não. Não pode ser.

**P15 · TRAVELLING, "a ponte se abre"** `[8,0 s]`
O plano assinatura. A câmera **larga o grupo**: pivô à altura do peito, no eixo da ponte, viaja de [B] para [C] em linha reta e
lenta, giro fixo, inclinação baixa. BGM: o bordão sobe meio tom. A neblina se abre em camadas conforme o pivô avança: primeiro o
arco rachado, depois o bloco de pedra, depois um vulto sentado nele, com o cutelo apoiado nos joelhos. O travelling termina a uns
quatro hexes dele e **para**. Ele ainda não se mexeu.

**P16 · CLOSE, The Butcher**
DOLLY-IN lento.

THE BUTCHER: (sem levantar)
Demoraram. Achei que o poço tinha ficado com algum.
(vira o rosto para a câmera. Está sem máscara: só a sombra do capuz e um brilho laranja onde deviam estar os olhos)
Ainda são quatro. Que bom. Eu odeio refazer conta.

**P17 · OTS Kael → Butcher** `[a caminhada]`
CORTE para trás de Kael. Ele avança 2 tiles e a câmera vai junto, colada no ombro dele; o Butcher cresce no quadro.

KAEL: Você estava morto.

**P18 · OTS Butcher → Kael** (o contracampo exato)

THE BUTCHER: Eu estava debaixo. Não é a mesma coisa. Você vai entender isso cedo demais.

**P19 · MÉDIO, Lio passa à frente**
A câmera desliza para o lado: Lio entra no quadro pela esquerda, passando à frente de Neera, e para à frente de Kael.

LIO: (a voz tremendo)
Cadê a minha mãe?

**P20 · CLOSE, Butcher**
Ele inclina a cabeça. O brilho laranja dos olhos "foca" em Lio.

THE BUTCHER: Ah. A menina que correu. Ainda segurando a flor.
(pausa `[1,5 s]`)
Ela está onde eu deixei. Esperando, como todos os escolhidos esperam.

**P21 · CLOSE, Lio**

LIO: ONDE?!

**P22 · OTS Butcher → Lio**

THE BUTCHER: Do outro lado desta ponte. É só passar por mim.

NEEERA: (segura Lio pelo ombro, entrando no quadro)
Ele está te provocando. Não escuta.

**P23 · RASANTE, o Butcher levanta** `[SHAKE 0,15 s]`
CORTE para uma câmera baixa, nos pés dele. Ele levanta e sai do quadro por cima: a câmera não consegue acompanhar a altura dele.
A ponte treme 1 frame. SFX: pedra assentando.

THE BUTCHER: Isto aqui é pedágio, cinzomante. Todo mundo passa. Mas ninguém passa inteiro.

**P24 · MÉDIO, Voss**
SFX: CLACK.

VOSS: (arma o canhão)
Somos quatro contra um. Faz essa conta aí.

**P25 · CLOSE, Butcher** `[2,0 s depois da fala]`
Ele parece sorrir debaixo do capuz pela primeira vez.

THE BUTCHER: …Um?

Ele bate duas vezes no chão com o cabo do cutelo. **TOC. TOC.**
A BGM corta no segundo TOC. Silêncio total. O rio para de se ouvir.

**P26 · CLOSE, Butcher (TRAVA)** `[5,0 s]`
**A câmera não corta.** Fica no rosto dele. SFX: uma respiração funda, molhada, grande demais, vindo de trás dele.
TINT 30%. Atrás da cabeça do Butcher, fora de foco, na neblina, dois pontos laranja se acendem. Depois mais dois, acima.
Não são dois animais. São dois pares de olhos na mesma cabeça. O Butcher não se vira. Ele sabe o que tem atrás.

**P27 · MÉDIO, Lio cai**
CORTE para Lio: ela cai sentada, emote suor, o sprite tremendo.

LIO: K-Kael…

**P28 · DOLLY-OUT, "ela não cabe"** `[7,0 s]`
CORTE para o eixo da ponte, atrás do Butcher, câmera à altura do peito dele, **enquadrada para um homem**. SFX: garras na pedra.
SHAKE 0,2 s a cada passo. Da neblina sai VORA, e ela não cabe no quadro: primeiro a gaiola do focinho, depois as placas de ferro
rebitadas no couro. A câmera **recua** (distância ×1,6 numa chave de 4 s) e ela ainda não cabe. Correntes saem da coleira e se
enrolam no braço do Butcher. Os quatro olhos queimam em brasa. Vora abre a mandíbula dentro da gaiola: a cinza que cai à frente
dela se acende em faíscas com o bafo.
Só na última chave o DOLLY-OUT para, e ela cabe: do tamanho de uma carroça, o Butcher na altura do peito dela.

**P29 · CLOSE, Voss** `[zoom lento, 3,0 s]`
DOLLY-IN lento no rosto de Voss. O canhão treme no canto do quadro.

VOSS: (baixinho)
…Eu retiro a conta.

**P30 · MÉDIO, o Butcher e o cão**
Ele acaricia a cabeça dela. A câmera está baixa, do lado deles, Vora enchendo o fundo inteiro do quadro.

THE BUTCHER: Essa é a Vora. Achei ela filhote, comendo o que sobrava dos ganchos da aldeia. Achei bonito. Ela não escolhe.
(a corrente afrouxa um elo. SFX: um elo de ferro)
Eu escolho por ela.

**P31 · GERAL ao longo da ponte, o uivo** `[SHAKE 1,2 s]`
Vora dá um passo à frente e uiva. CORTE para o GERAL de P04 invertido (do norte para o sul): a tela inteira treme, a cinza do ar é
soprada para a câmera.
**Inserto** `[0,6 s]`: CLOSE das estátuas de [B] rachando, pó caindo.
Caixa vermelha: 〔 NEEERA, VOSS, LIO estão AMEDRONTADOS 〕

**P32 · CLOSE, Neera**

NEEERA: (com a mão no peito)
Eu não consigo… minhas mãos não param…

**P33 · RASANTE, Kael passa à frente** `[a caminhada, lenta]`
Câmera baixa, à frente do grupo, olhando para trás (para o sul): Vora está **atrás da câmera**. Kael entra no quadro andando para
nós, um tile de cada vez, sem recuar, e passa à frente de todos. Os três ficam atrás dele, pequenos.

KAEL: Olhem pra mim. Não pra ele. Pra mim.

**P34 · CLOSE, Kael (TRAVA)**
O retrato de Kael ocupa a tela. **Primeira vez que a câmera para desde o CLINK.** Nenhuma chave nova até o fim da promessa.

KAEL: Butcher.

THE BUTCHER (fora de quadro): Vanguarda.

KAEL: Na aldeia eu deixei você descer. Foi erro meu.
(ergue a lâmina: ela entra no quadro por baixo e reflete o laranja do rio no rosto dele)
Hoje você não desce. Não sobe. Não some. Eu fico nesta ponte até ter certeza de que acabou.
Você não sai daqui vivo.

**P35 · MÉDIO, os três atrás** `[2,0 s]`
O status AMEDRONTADO some de Neera e Voss com um brilho azul. Em Lio ele pisca e fica.

LIO: (baixinho, ainda no chão)
…Kael. Se ele morrer, quem vai dizer onde ela está?

**P36 · CLOSE, a nuca de Kael** `[2,0 s]`
Câmera atrás dele. Ele não responde. Não olha para trás. Ao fundo, fora de foco, Vora e o Butcher.

**P37 · MÉDIO, a corrente cai**
SFX: a corrente inteira na pedra.

THE BUTCHER: (solta a corrente)
Ouviu, garoto? Promessa tem preço. Vora. Come.

**P38 · CLOSE, os quatro olhos** `[0,8 s]`
CORTE seco. Só os olhos. Eles se movem para a câmera.

**P39 · TRANSIÇÃO**
Vora avança. A tela se parte ao meio como pedra rachando. Transição de batalha. BGM: "Pedágio de Ferro e Dente".
As BARRAS saem com a transição (a batalha é do jogador).

---

## CENA DE FIM DE COMBATE — "A Promessa"

**P40 · o golpe final**
BARRAS voltam no último golpe. O tiro de Voss acerta a coleira: câmera de ação normal (`rangedShot`, atrás de Voss, então o pivô
em Vora no impacto). As placas de ferro estouram. SHAKE 0,8 s.

**P41 · DE CIMA, o corpo atravessado** `[6,0 s]`
**Primeira vez de cima.** Inclinação no máximo, sobre [C]: Vora cai de lado e o corpo dela atravessa a ponte de borda a borda. Ela
tenta levantar duas vezes. Na terceira, os quatro olhos se apagam, um par de cada vez. SFX: cada par é um estalo de brasa molhada.
Silêncio. O rio volta a se ouvir.

**P42 · GERAL, o Butcher pequeno** `[4,0 s]`
Sem corte: a inclinação desce devagar até o médio, mas a distância fica longa. O Butcher parado, olhando. Pela primeira vez o
sprite dele é pequeno no quadro.

THE BUTCHER: …Vora.

**P43 · MÉDIO baixo, ele se ajoelha**
DESLIZA para perto. Ele se ajoelha ao lado do cão. A mão grande passa na cabeça dela, devagar. A câmera fica na altura da cabeça de
Vora, de modo que a mão dele é a coisa mais próxima da lente.

**P44 · OTS Lio → Neera**

LIO: (baixinho)
Ele… está de luto?

NEEERA: Até monstro tem coisa que ama. É isso que deixa tudo pior.

**P45 · RASANTE, ele levanta** `[SHAKE 0,15 s]`
Repete o P23: câmera nos pés dele, ele levanta e sai do quadro por cima, pegando o cutelo. Os olhos de brasa estão maiores: dois
pontos laranja fora de quadro iluminam o alto da imagem.

THE BUTCHER: Eu só queria tirar o que ela não precisava. Agora vou tirar tudo.

**P46 · TRAVELLING lateral, a corrida** `[2,0 s]`
Ele corre na direção de Lio. A câmera acompanha de lado, de fora da ponte, o vazio embaixo: ele atravessa o quadro da esquerda para
a direita. Kael entra pela direita.

**P47 · OTS, o choque** `[SHAKE contínuo]`
Os dois se chocam no meio do [C]: cutelo contra lâmina. `twoShot` com push-in no impacto, faíscas. A tela não para de tremer.

**P48 · TRAVELLING lateral, a borda** `[5,0 s]`
A câmera **do lado de fora**, baixa, o rio laranja ocupando o terço de baixo do quadro. O cutelo empurra Kael para trás tile a tile;
o pivô recua com ele. A cada tile a borda sem parapeito chega mais perto do pé de trás de Kael. A luz laranja no rosto dele cresce
conforme ele chega à borda.

NEEERA (fora de quadro): KAEL!

**P49 · CLOSE, os dois rostos**

THE BUTCHER: (fazendo força)
Você prometeu. Pra quem? Pros ganchos? Eles não estão ouvindo.

**P50 · RASANTE, o pé de trás** `[1,0 s]`
**Inserto**: câmera no nível da pedra, de fora. O pé de trás de Kael, já no ar sobre o abismo. Embaixo, o laranja.

KAEL: Prometi pra mim.

**P51 · CLOSE, a mão solta a lâmina** `[0,5 s]`
Inserto: a mão abre. A lâmina cai. SFX: aço na pedra.

**P52 · MÉDIO, o giro** `[1,5 s]`
O cutelo passa sem resistência e o Butcher perde o equilíbrio para a frente. Kael gira por baixo do braço dele, agarra a corrente
presa ao pulso e a enrola duas vezes no pescoço do Butcher. A câmera gira junto (PAN de 90°, rápido).
**FLASH BRANCO** `[0,2 s]`.

**P53 · CLOSE, Kael**

KAEL: Desta vez eu fico olhando.

**P54 · RASANTE pelo lado de fora, "o pendurado"** `[4,0 s]`
Kael puxa e empurra o Butcher para fora da borda. CORTE para a câmera **embaixo da ponte**, sobre o rio, olhando para cima: os pés do
Butcher se debatendo contra o céu cinza, a metros do laranja; acima deles a corrente esticada; no alto do quadro, a borda e Kael
preso à pedra, segurando a outra ponta. SFX: a corrente rangendo, o rio.

**P55 · MÉDIO, Voss corre**

VOSS: (correndo)
Kael, solta! Ele vai te puxar junto!

**P56 · CLOSE, Kael segurando**
O sprite vibrando de esforço. A corrente sai do quadro por baixo.

KAEL: Se eu soltar, ele cai. Se cair, some. Se sumir, volta.

**P57 · MÉDIO, Lio na borda**
Lio corre até a borda, entrando no quadro de P54 visto do outro lado.

LIO: (aos gritos)
NÃO! Ele sabe onde ela está! Kael, ele SABE!

**P58 · DE CIMA, a escolha** `[o diálogo inteiro]`
**Segunda vez de cima.** Inclinação no máximo, sobre a borda: Kael e Lio no alto do quadro, pequenos; a corrente descendo; o Butcher
pendurado no meio; o rio laranja ocupando a metade de baixo. Tudo o que importa cabe numa linha vertical.

THE BUTCHER: (com a voz engasgada, rindo)
…Ouviu, garoto? A menina quer que eu viva. Puxa. Me sobe. Eu levo ela até a mãe.
(levanta o rosto para Kael)
Ou corta. E escolhe por ela. Igual eu escolhia pela Vora.

Pausa longa `[3,0 s]`. Os retratos de Neera, Voss e Lio se voltam para Kael.

NEEERA: (baixo)
Kael… é ele que está escolhendo agora. Não você.

THE BUTCHER: E antes de decidir… me diz…
o que vocês vieram buscar.

**P59 · CLOSE, Kael (TRAVA)** `[3,0 s de caixa vazia]`
**Segunda vez que a câmera para.** O retrato de Kael aparece. A caixa fica vazia por 3 segundos. Nenhum som além do rio.

KAEL: (baixo)
Não interessa mais.

**P60 · CLOSE, a lâmina** `[0,8 s]`
Inserto: a outra mão pega a lâmina caída ao lado. O corte na corrente. SFX: um único elo partindo.

LIO (fora de quadro): NÃÃÃO!

**P61 · GRUA para baixo, a queda** `[5,0 s]`
A câmera vai junto: pivô desce com o Butcher, inclinação subindo até olhar para cima enquanto cai. Queda lenta. O capuz abre e o
rosto continua sem aparecer: só o brilho laranja, que agora se confunde com o laranja do rio. Ele bate na cinza e o laranja engole o
corpo. Bolhas. Depois nada. A câmera fica um segundo sobre o rio liso.

**P62 · GRUA para cima, "o que sobrou"** `[6,0 s]`
Sem corte: o pivô sobe, devagar, do rio até a borda. Primeiro a corrente cortada balançando; depois Lio de joelhos na borda; depois
Kael parado ao lado dela. A câmera termina num médio dos dois, de fora da ponte, o vazio embaixo.

LIO: (sem olhar para ele)
Você prometeu pra você. Eu prometi pra ela.

KAEL: Lio…

**P63 · MÉDIO, Lio se afasta** `[a caminhada]`
Ela levanta e se afasta 3 tiles. A câmera **não a segue**: ela sai do quadro e Kael fica sozinho nele.

LIO: Não.

Caixa azul, curta: 〔 Confiança de LIO diminuiu 〕

**P64 · OTS Voss → Neera** (longe de Kael, um plano "de canto")

VOSS: (baixo)
…Ele fez certo?

NEEERA: Ele fez o que tinha prometido. Não sei se é a mesma coisa.

**P65 · RASANTE pelo lado de fora, Kael ajoelha** `[o diálogo]`
A câmera de P06, o mesmo lugar onde Lio olhou o rio na abertura. Kael ajoelha na borda e olha para baixo; o laranja no rosto dele.
O cenário rima consigo mesmo: começou com ela perguntando ao rio, termina com ele.

KAEL: Eu vi ele entrar. Desta vez eu vi.

VOSS (fora de quadro): Ninguém sai daquilo. Nem ele.

KAEL: Vou ficar mais um pouco.

NEEERA (fora de quadro): Quanto tempo?

KAEL: Até o rio parar de borbulhar.

**P66 · O MESMO ENQUADRAMENTO (TRAVA), "…mais tarde"** `[FADE curto, 4,0 s]`
Fade curto para preto, letreiro "…mais tarde", fade de volta **no mesmo plano exato de P65**, sem mover uma chave. Kael continua
sentado na borda, cinza cobrindo os ombros. O rio corre liso. No fundo do quadro, do outro lado da ponte, Lio de costas, sozinha,
pequena. **Terceira trava.**

KAEL: (levanta)
Acabou.

**P67 · GERAL ao longo da ponte, a passagem** `[a caminhada]`
O grupo segue para o norte. Câmera no eixo, à frente, recuando com eles (TRAVELLING de ré). O corpo de Vora atravessa o quadro.

**P68 · CLOSE, a pétala** `[1,5 s]`
Lio para. Tira a PÉTALA DA GUARDIÃ e a segura sobre a cabeça do cão. Câmera baixa, a cabeça de Vora e a mão de Lio no quadro. A
pétala pulsa uma vez e, por 1 frame, ilumina a gaiola de dentro.

LIO: (sussurrando)
Desculpa. Não foi escolha sua.

NEEERA (fora de quadro): Lio. Vem.

**P69 · [D] TORRE DE PEDÁGIO, a marca** `[inserto 2,0 s]`
CLOSE da parede: quatro traços e um quinto atravessando, riscados com ponta de cutelo. O quinto está fresco; a pedra ainda solta
pó. A câmera está tão perto que o pó cai na lente.

VOSS: (para)
Isso foi feito agora há pouco.

NEEERA: Ele ficou esperando a gente aqui. Pode ter feito antes.

VOSS: Antes, a pedra não estaria soltando pó.

**P70 · CLOSE, a mão de Lio na marca** `[DOLLY-IN 2,0 s]`
Lio passa a mão na marca. O dolly desce com a mão dela: embaixo, menor, um nome riscado com letra torta: **TAVI**.

LIO: (emote "!!")
…É a letra dele. É a letra do meu irmão.

NEEERA: Ele passou por aqui. Ele está vivo, Lio.

**P71 · OTS Lio → Kael**
Ela se vira para Kael. Olhos molhados e duros. A câmera por cima do ombro dela: Kael enquadrado, longe, pequeno.

LIO: Então eu vou achar ele. Sem precisar de quem você jogou no rio.

**P72 · MÉDIO, Kael e a marca** `[1,0 s de trava]`
Ela sai na frente, sem esperar, atravessando o quadro. Kael passa pela marca e para por 1 segundo ao lado dela: ele e os cinco
traços no mesmo quadro.

KAEL: (baixo)
Andando.

**P73 · [E] GERAL, o grupo some** `[4,0 s]`
No eixo da ponte, para o norte: o grupo some na neblina. Figuras pequenas, depois nenhuma.

**P74 · TRAVELLING de volta, "a corrente"** `[8,0 s]`
Sem corte: o pivô volta devagar pela ponte, de [E] para [C], **o P15 ao contrário**. Passa pelo corpo de Vora. Para na borda:
a corrente cortada continua presa na pedra, pendurada sobre o abismo.
SFX: **CLINK.**
Ela balança uma vez, como se alguém lá embaixo tivesse puxado.
A câmera inclina 10° para baixo, para o rio. O rio corre liso.
FADE-OUT. BARRAS somem com o preto.

〔 STONE BRIDGE — CONCLUÍDO 〕

---

## Som (resumo por bloco)

| Bloco | BGM | O que se ouve |
|---|---|---|
| P01–P04 | silêncio, depois um bordão grave | escada, alçapão, o rio, vento |
| P05–P12 | o bordão | vozes, o rio ao fundo sempre |
| P13–P25 | o bordão sobe meio tom no CLINK | CLINK, pedra, CLACK |
| P26–P38 | **corta no segundo TOC**; nada até a batalha | respiração, garras, o uivo, a corrente |
| P40–P41 | fim da BGM de batalha com o último tiro | placas estourando, os olhos apagando |
| P42–P61 | nenhuma | o rio, a corrente, aço, o elo partindo |
| P62–P74 | um tema curto, só cordas, entra no P62 e some no P73 | o rio liso; o CLINK final é a última coisa |

Regra: o rio nunca sai da mixagem, exceto entre o segundo TOC e o fim da batalha. A ausência dele é o que avisa que Vora chegou.

## No engine (2026-11-06)

Os passos `shot`, `cut`, `shake`, `tint`, `bars`, `fade`, `sfx`, `music`, `caption`, `emote` e `flash` existem no formato
`ember-cutscene-1` (docs/CUTSCENES.md, "Film steps"). As duas cenas que tocam este roteiro:

- `content/cutscenes/stone-bridge-01-o-pedagio.json` (ao abrir o mapa `thebridge`): P01–P39.
- `content/cutscenes/stone-bridge-02-a-promessa.json` (ao vencer): P40–P74.

O mapa (`content/maps/ember/thebridge030.json`) corre de oeste para leste: [A] a saída do poço em x 2–5, [B] os nichos em x 11,
[C] o vão em x 16–24, [D] a torre de pedágio (os aparelhos de tortura) em x 29–34, [E] a cabeceira em x 36–39; o rio fica além do
muro sul (y 20), então "a borda" é a fileira y 19 e "pelo lado de fora" é a câmera olhando para `n` a partir dela. O "norte" do
roteiro é o `e` do mapa. O que não dá para tocar como escrito: a câmera **por baixo** da ponte (o olho nunca desce do convés:
P54 e P61 são rasantes e uma grua para baixo olhando o rio), o Butcher sentado (não há pose), o golpe final de Voss (é a batalha),
a caixa de diálogo vazia por 3 s (é um `wait`).

Vora morre na batalha e é uma atriz `fallen`: o corpo fica na ponte para a cena de vitória. O Carniceiro é um neutro do mapa
(não luta, não morre) para estar vivo na cena final; o `exit` dele é a queda.
