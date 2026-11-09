# CENÁRIO 5 — WISP FOREST · abertura "As Vozes Vermelhas"

MCs: KAEL · NEEERA · VOSS · Escolta: LIO · Antagonistas: THE BUTCHER (de volta do rio) · Irmã Cinza e os guardas do santuário

Continua de onde a Ponte de Pedra parou: Lio vai na frente e não fala com Kael desde a torre de pedágio; a marca de Tavi ficou
para trás; a corrente cortada balançou sobre o abismo "como se alguém lá embaixo tivesse puxado". Esta cena paga essa corrente.

Mapa: `content/maps/ember/wisp-forest012.json` (id do rascunho `wisp-forest-revisited-v03`, o gatilho usa esse id). Só Kael, Neera
e Voss do grupo (`partyHeroes`), Lio como convidada (`guestHeroes`). O grupo entra pelo sul (y 16-19, sob o carvalho morto gigante);
a clareira abre para o norte; a pedra-guia em (16,7), o santuário em (16,12), a lanterna vermelha na borda oeste (3,9), o salgueiro
no extremo norte. Todos os inimigos esperam fora do tabuleiro e entram no fim, menos o Vigia de Pedra (o golem), que fica no norte
parado como uma estátua até virar a cabeça.

Cena: `content/cutscenes/wisp-forest-01-as-vozes-vermelhas.json` (ao abrir o mapa). Os planos usam os passos de filme do
`ember-cutscene-1` (docs/CUTSCENES.md); "n" olha para dentro da floresta, "s" de volta para a entrada.

| Plano | O que a câmera faz | O que acontece |
|---|---|---|
| W01 | — | "Horas depois da ponte." Um CLINK distante, passos em folha seca. |
| W02 | de cima, a clareira vazia, desce devagar | wisps vermelhos sobre as árvores; ninguém ainda. |
| W03 | baixa no carvalho, olhando o norte | Lio entra primeiro, sozinha. |
| W04 | geral do norte | os três atrás dela, Kael por último, um vão entre eles. |
| W05 | por cima do ombro de Kael | Neera: "Ela não fala com você desde a torre." Kael: "Precisa chegar viva." |
| W06 | Voss, depois Neera | as árvores mortas; "wisp vermelho é alma que ainda não terminou de cair"; Voss: "Mais gente que não terminou de cair." |
| W07 | close em Kael, dolly-in, sem fala | a frase acerta. |
| W08 | close na pedra-guia | a marca de novo, a letra de Tavi e uma seta para o norte. |
| W09 | a lanterna vermelha na borda, ninguém no quadro | uma voz: "Lio…" (a tela escurece 20%). |
| W10 | close em Lio | "…Tavi?" A voz pede para ela vir. |
| W11 | geral | Lio corre para a borda; Neera: "Wisp imita voz!" |
| W12 | rasante da borda, o vazio atrás da câmera | Kael corre e a segura. |
| W13 | os dois, por cima do ombro dele | "É a voz que você quer ouvir." / "Você não decide isso!" / "Hoje decido." |
| W14 | close em Lio, a câmera para | "Igual ele disse na ponte. Você escolhe pelos outros." |
| W15 | do lado dela, Kael **de costas** (pose `turnBack`) | "…Então escolhe você. Mas escolhe olhando." |
| W16 | a lanterna, depois Lio | a voz erra: "como todos os escolhidos esperam" (as palavras do Butcher); Lio percebe e volta sozinha. |
| W17 | geral, o grupo junto | Voss odeia a floresta "de um jeito pessoal"; Neera: os wisps repetem o que a gente carrega. |
| W18 | o norte vazio; depois os quatro de frente | CLINK. CLINK. A música corta. Todos congelam. |
| W19-W20 | close em Kael, a câmera para | "Contando de novo, Vanguarda?" — "…Não." |
| W21 | rasante, a névoa abre (escurece 30%) | o Butcher sai da floresta arrastando a corrente: "cinza não engole o que já é cinza." |
| W22 | contracampo | "Eu vi você entrar." / "Depois eu subi pela corrente que você deixou pendurada." |
| W23 | Lio dá um passo à frente | "Cadê eles?" O Butcher achou Tavi. |
| W24 | geral do santuário | a Irmã Cinza e os guardas saem das árvores: "O santuário pediu quatro. O pedágio mandou quatro." |
| W25 | rasante no golem | a "estátua" vira a cabeça; a tela treme. |
| W26 | Kael entre eles e Lio | "Desta vez quem escolhe é você." Lio: "Então ele vive até dizer onde estão. Depois é seu." "Combinado." |
| W27 | close no Butcher | "Ela aprendeu a fazer conta." Golpe, flash, começa a luta. |

O arco: na ponte Kael escolheu por Lio e cortou a corrente; aqui a floresta devolve a escolha para ela, e o acordo do fim (o Butcher
vive até dizer onde estão) é a primeira coisa que os dois decidem juntos.

Os passos de tela cheia (barras, fade, escurecer, flash, legendas) não desenham nada desde 2026-11-07 ("Keep every on-map cutscene
in the normal game view"); ficam no roteiro como marcação e voltam sozinhos se esse desenho voltar.
