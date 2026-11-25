# EmberEngineB project safeguards

## REGRA MÁXIMA: o vídeo inteiro em toda animação, o maior laço possível em tudo
Acima de todas as outras. O usuário, 2026-11-25: "vai usar o caralho inteiro do video em todas as animações sem exceção, salvar como
regra máxima"; "você fazia o menor loop possível, agora vai fazer o maior loop possível para tudo". Vale para todo agente e toda
unidade (herói, monstro, animal, invocação), em Alta, 64, 36 e Cinemática.

- Toda animação usa o vídeo inteiro dela, cada quadro, do primeiro ao último (num vídeo com duas animações, cada uma o trecho inteiro
  dela). Nunca um pedaço.
- Todo laço (andar, parada, pairar, qualquer pose em `loop`) é o **maior possível: o trecho inteiro**. No spec, a pose em `loop` não
  leva `loopMin` nem `loopMax`: sem eles o `video_sprite.py` toca todos os quadros de `first` a `last` como o laço. O corte não escolhe
  um pedaço "de emenda mais limpa": toca tudo.
- Proibido: `loopMin`/`loopMax` em qualquer spec, laço curto, sugerir laço menor, encurtar o trecho para a emenda ficar melhor.
- Laço curto encontrado no jogo: recortar com o maior laço e avisar o usuário.
- **Trava na ferramenta (2026-11-25, o usuário: "salve esse padrão, nunca mais regredir"):** `tools/scripts/sprites/video_sprite.py`
  recusa qualquer spec que ainda tenha `loopMin`/`loopMax` nos clipes a cortar, antes de gravar um quadro. Ninguém tira essa trava.

## REGRA PERMANENTE: modo de teste = mesmo jogo, só recursos infinitos
Ler antes de tocar em qualquer código do jogo. Vale para todo agente de código (Claude, Codex, Cursor, Copilot, qualquer outro).

Existe **um único sistema**. Não há variação de caminhos entre modo normal e modo de teste. O modo de teste não é um sistema paralelo: é só um valor de recurso. Nada mais, nada menos.

**Proibido no código:**
- Qualquer `if (testMode)` (ou equivalente: `debug`, `dev`, `sandbox`, flag, define, variável de ambiente) que altere fluxo, ordem de execução, regras, validação, fórmula, desenho ou estado.
- Qualquer função, classe, cena/stage, prefab, tabela, mapa de dados ou configuração duplicada com sufixo ou versão `_test`, `_debug`, `_dev` — ou uma cópia própria do modo de teste de algo que o jogo já faz.
- Qualquer atalho que pule etapas: desbloqueio instantâneo, pular requisito, bypass de condição, pular custo.
- Qualquer item, receita, construção, unidade, magia ou mecânica que exista só no teste.
- Qualquer diferença de salvamento, serialização ou formato de dados.

**Permitido — e só isso:**
- Quantidade inicial de recursos elevada ou que não diminui.

O teste joga o jogo real com o estoque cheio. Toda aquisição, custo, requisito e consumo passa pelos mesmos métodos. Se um custo é debitado no modo normal, é debitado no teste também — a diferença é só que o saldo não acaba.

**Implementação correta: um único ponto de origem do valor, nunca um ramo de lógica.**

```
// errado
if (test) { build(); } else { if (canAfford()) { pay(); build(); } }

// certo
resources.grant(TEST_STOCK);   // só no boot, só quantidade
// o resto do jogo não sabe que o modo de teste existe
```

O código do jogo não deve conter "teste"/"test" em nenhuma condicional. A única menção vive no boot, concedendo estoque.

Se encontrar código existente que viole esta regra: não copie o padrão nem acrescente outro ramo. Una as cópias numa função só que o jogo e o teste usam, e avise o usuário.

## REGRA PERMANENTE: o pipeline de sprites que funciona — não mudar
**Os sprites são o aspecto mais importante do jogo.** Todo sprite, de toda unidade, passa pelo mesmo pipeline abaixo, sem exceção e sem atalho,
e o resultado tem que ser **perfeito**: nada menos que perfeito é aceitável (o usuário, 2026-11-21). Parte cortada, cauda/asa/arma que some
ou pisca, quadro tremido, tamanho ou luz diferente entre clipes = não está pronto, não entra no jogo, não vai para commit.
Refazer uma unidade não reaproveita nada da anterior; os arquivos antigos (quadros, mattes, cópias `*-before-*`) são apagados assim que os novos
estão no jogo.

**Antes de qualquer sprite, abrir `docs/SPRITE_STANDARD.md`**: o padrão campo por campo (o do Kael, que o usuário aprova), a luz, as
conferências e o commit. Todo conjunto segue esse arquivo.

Vale para todo agente (Claude Opus/Sonnet/Haiku, Codex, qualquer outro). É o caminho com que todos os sprites bons do jogo foram feitos.
Ninguém troca, "melhora" ou substitui uma ferramenta desta lista sem o usuário pedir com todas as letras.

1. **Matte:** `python -I tools/scripts/sprites/video_matte.py <vídeo> <pasta> <primeiro> <último> --threads 12 --device cpu [--crop x0,y0,x1,y1]`
   (BiRefNet general 1024, ONNX na CPU). Um matte por vez, 12 núcleos, nunca a placa de vídeo. Grava `src-NNNN.png` e `alpha-NNNN.png`.
   **NUNCA `hr_matte.py`** (BiRefNet HR 2048): apaga cabeça de cajado, brilho e luz de magia (Voss, cast, quadro 48, 2026-11-20:
   a antiga mantém o cajado e o brilho, a HR some com os dois). Ela é 8x mais lenta (99 s por quadro contra 12 s).
2. **Spec e luz:** o `cut.json` do conjunto (clipes, quadros, `figurePx` por vídeo) e `match_light.py` para igualar a luz dos vídeos.
3. **Corte:** `tools/scripts/sprites/video_sprite.py <cut.json>` para Alta (todos os quadros, tamanho do vídeo), `cut-64-media.json` (64 quadros
   a ~610 px) e `cut-36-media.json` (36 a ~610 px): sempre os três.
4. **No jogo:** entradas em `content/sprite-overrides.json`, `apply_sprite_overrides.py` (pelo passo apply dos scripts `build/video-cut/*-after-matte.py`,
   guardando só os conjuntos mudados), cozinha (`Cooker.exe sprites`), prévia.
5. **Progresso na conversa (parte do procedimento, sempre), NUNCA janela:** o usuário, 2026-11-25: "CHEGA DE JANELAS SEM INFORMAÇÃO".
   Nenhuma janela PowerShell/Terminal é aberta na tela dele. O agente roda `build/video-cut/progresso-refazer.ps1 -Once` e cola na conversa
   a tabela (cada etapa com barra, quadros feitos / total, o que roda agora, previsão só com o tempo medido), em todo relato.
6. **Conferir antes de dizer pronto:** para cada clipe, o quadro com arma, cajado, mão e efeito, em tamanho grande, sobre verde forte:
   vídeo | matte | sprite do jogo, lado a lado. O usuário não consegue achar esses momentos no jogo: quem confere é o agente.

## REGRA PERMANENTE: toda animação toca inteira, na sua velocidade — nunca acelerar, espremer ou cortar
O jogo é feito para ser a apresentação mais cinematográfica possível. O usuário, 2026-11-25: "he can take all the time in the world doing
his moves, that is exactly whats wrong with the game everything runs fast or gets cut short"; "Its meant to make for the most cinematic
presentation possible" e os agentes "keep nerfing everything".

Vale para todo agente (Claude Opus/Sonnet/Haiku, Codex, qualquer outro) e para toda unidade, chefe ou não.

**Proibido:**
- Espremer um clipe num tempo menor que o dele (ex.: o abrir do Birolho comprimido em dois hexes), tocar mais rápido que o vídeo, pular
  quadros para "ganhar tempo".
- Deixar a próxima ação, o próximo turno, o idle ou outra pose cortar uma animação antes do fim. Quem espera é o jogo: o turno, a troca de
  golpes e a caminhada só terminam quando a animação terminou.
- Tocar uma animação "às vezes". Se a unidade tem a pose (abrir, fechar, a pose que segue uma habilidade), ela toca sempre.
- Sugerir encurtar um laço, regravar mais curto, ou apresentar o tempo de espera como custo, problema ou ponto negativo.

**Como fazer:** ao ligar uma pose (abrir / fechar, a pose que segue uma habilidade — `"then"` na pose do roster —, conjuração, dano, queda,
morte), somar o tamanho inteiro dela ao tempo que o jogo espera. Código existente que acelera ou corta um clipe: consertar para tocar
inteiro e avisar o usuário. Por dado na pose, nunca por nome de unidade.

**Método definitivo: a cadeia de uma animação até o jogo.** O usuário, 2026-11-25: "we need a definitive method, any step that gets
skipped ruins the whole chain". Uma animação só existe no jogo se TODOS os elos abaixo estão feitos, nesta ordem. Um elo pulado = a
animação não aparece, aparece às vezes ou aparece cortada. Nenhum agente diz "pronto" com um elo faltando; o relato lista os oito, cada
um como feito (com a prova) ou NÃO FEITO, e o que falta vem na primeira linha.

1. **Clipe:** a pose está no `cut.json` do conjunto com o clipe inteiro (o pipeline de sprites acima; Alta, 64 e 36).
2. **Dado:** a entrada da pose em `content/sprite-overrides.json`, com o que a liga às outras (`"then"`: a pose que a segue; `"reversed"`:
   a pose que é outra ao contrário).
3. **Roster:** o passo apply rodou e a pose está em `build/scenes/roster.json` com `pace` não vazio e os mesmos campos do elo 2.
   Prova: abrir o roster e ler a pose.
4. **Cozinha:** `Cooker.exe sprites` rodou depois do apply e o clipe está no `battle.pak`. Prova: o log do jogo sem
   "not drawn: no sprite clip" para essa unidade.
5. **Motor:** o código toca a pose sempre e o jogo espera o tamanho inteiro dela (caminhada: `Walk::hold` / `foldUntil`; habilidade: o
   `x.end` da troca soma o `"then"`). Pose nova de um tipo que o motor ainda não espera = código novo, por dado, antes de dizer pronto.
6. **Build:** `Sandbox.exe` linkado depois da última mudança de código (o jogo fechado para linkar). Prova: a hora do arquivo.
7. **Conferência:** quem confere é o agente, elo por elo, com as provas acima; o que só o usuário pode ver no jogo é dito assim, com a
   unidade e a ação exata para ele testar.
8. **Commit:** o que é do agente, commitado; o que ficou fora, dito com o motivo.

## Bosque Morto ground lock
- Keep the approved ground mask `content/ground/bosque-morto-ground-mask-v02.png` and the `bosque-morto-v04` ground mapping unchanged.
- Do not recolor, replace, regenerate, or remap that mask or tile. Preserve the active map version and all non-ground content when making ground-only fixes.
- Change this locked ground only when the user explicitly asks for a different ground.
