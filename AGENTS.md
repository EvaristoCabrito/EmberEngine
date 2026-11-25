# EmberEngineB project safeguards

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
5. **Janela de progresso (parte do procedimento, sempre):** toda fila de sprites tem a sua janela PowerShell aberta na tela do usuário com a lista
   inteira (cada vídeo com barra, quadros feitos / total, o que roda agora, previsão só com o tempo medido), como
   `build/video-cut/progresso-refazer.ps1` (abrir com o `.cmd` do lado). Fila nova = janela nova ou a mesma atualizada, antes de começar.
6. **Conferir antes de dizer pronto:** para cada clipe, o quadro com arma, cajado, mão e efeito, em tamanho grande, sobre verde forte:
   vídeo | matte | sprite do jogo, lado a lado. O usuário não consegue achar esses momentos no jogo: quem confere é o agente.

## Bosque Morto ground lock
- Keep the approved ground mask `content/ground/bosque-morto-ground-mask-v02.png` and the `bosque-morto-v04` ground mapping unchanged.
- Do not recolor, replace, regenerate, or remap that mask or tile. Preserve the active map version and all non-ground content when making ground-only fixes.
- Change this locked ground only when the user explicitly asks for a different ground.
