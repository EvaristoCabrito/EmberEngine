# O padrão de um conjunto de sprites (abrir antes de qualquer sprite)

O usuário, 2026-11-25: o andar, as animações e o recorte do **Kael** (`content/sprites/kael-v3/cut.json`) são a qualidade que ele aprova;
todo conjunto novo ou refeito segue este arquivo, campo por campo. As ferramentas são as do pipeline do `AGENTS.md` (matte com
`video_matte.py`, corte com `video_sprite.py`); aqui está **como** usá-las. Na dúvida, abrir o `cut.json` do Kael e fazer igual.

## 1. Os vídeos e o matte
- Um matte por vídeo inteiro, `--crop` no lugar onde a figura fica (a caixa dela + margem), 12 núcleos, CPU, um por vez.
- Antes do corte, medir e **dizer ao usuário** quais quadros encostam na borda do vídeo (figura cortada).

## 2. O spec (`content/sprites/<set>/cut.json`)
| Campo | O padrão (Kael) |
|---|---|
| `figureUnits` | a altura do personagem, cabeça à bota: humanos 2.58–2.70 (Kael 2.58, Aldric 2.64, Isolde 2.66, Voss 2.69) |
| `figurePx` | **não usar**: o corte mede a altura de cada clipe (a mediana da cabeça à bota), todos do mesmo tamanho |
| `edgeKey` | `true` (a borda lida entre a cor da figura e a da parede: sem franja cinza) |
| `bodyAt` | em **toda** pose em pé: a posição da cabeça na parada (lida da parada, como o −0.1902 do Kael); a parada também. Sem isso ele pula de lado ao trocar de pose |
| parada (`base`) | o vídeo de parada inteiro em `pingpong`, ou um loop fechado com a emenda onde ele está parado (`loopMin`/`loopMax`); nunca cortar no meio de um movimento |
| andar (`walk`, `walkLeft`, `walkUp`, `walkDown`) | `kind: walk`, `mode: loop`, o vídeo inteiro daquela vista e `loopMin`/`loopMax` largos (Kael: 96–239, 40–142), para o corte achar a melhor emenda; esquerda e direita / cima e baixo do mesmo vídeo combinado |
| ataques e especiais | `impactFrame` no quadro do golpe; efeito de luz que o matte apaga (arma dentro do brilho): `wallKey` na região do efeito, só nos quadros dele |
| queda | `standUntil` no último quadro em pé; o último quadro fica |
| `version` | versão nova a cada refação (`r01`, `v1121`...); Média `media64-`, Leve `media36-`, Cinemática `cine-` com a mesma |

## 3. A luz (`match_set_light.py`, como o Kael em 2026-11-21)
- Todos os vídeos do conjunto igualados a **uma** referência (os quadros em pé de um vídeo de estúdio), nos quadros em pé de cada vídeo
  (`light.videos`), e por cima **uma** correção do conjunto (`light.grade`).
- A correção é ajustada até o conjunto medir como os outros heróis (linear): **rosto 0.23–0.25** (Isolde 0.237, Aldric 0.250), **corpo
  (mediana) ~0.014** (Aldric 0.0137), r/b perto de 1.3–1.5.
- Conferir com `match_set_light.py <cut.json> --preview out.png`: um quadro de cada clipe com o número do rosto; nenhum clipe a mais de 5% da parada.

## 4. O corte e o jogo
- Sempre Alta (o tamanho do vídeo) + Média 64 + Leve 36 (~610 px) + Cinemática (feita pelo `video_sprite.py`).
- Entradas em `content/sprite-overrides.json`, aplicar só os conjuntos mudados, cozinhar.
- O tamanho que o jogo usa (`build/scenes/roster.json`) tem que ser o do corte (`cut-result.json`): conferir depois de aplicar.

## 5. Conferir antes de dizer pronto (quem confere é o agente)
- Para cada clipe, os quadros com arma, mão, cabelo, efeito: vídeo | matte | sprite, grandes, sobre verde forte.
- Os rostos de todos os clipes lado a lado (a mesma luz).
- O personagem ao lado dos outros heróis, no tamanho do jogo, na mesma linha de chão (`build/video-cut/lineup_roster.py`).
- A janela de progresso aberta durante toda a fila (`progresso-refazer`).

## 6. O commit
- Só os arquivos do conjunto, por um índice particular do git (`GIT_INDEX_FILE`): outras sessões usam o mesmo índice.
- Depois que o novo está no jogo e conferido: apagar os quadros, mattes e cópias `*-before-*` antigos daquele conjunto.
