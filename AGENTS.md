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

## Bosque Morto ground lock
- Keep the approved ground mask `content/ground/bosque-morto-ground-mask-v02.png` and the `bosque-morto-v04` ground mapping unchanged.
- Do not recolor, replace, regenerate, or remap that mask or tile. Preserve the active map version and all non-ground content when making ground-only fixes.
- Change this locked ground only when the user explicitly asks for a different ground.
