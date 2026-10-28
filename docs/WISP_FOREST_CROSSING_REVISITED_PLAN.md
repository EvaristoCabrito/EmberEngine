# Wisp Forest Crossing Revisited

## Cronograma de produção

| Fase | Duração estimada | Entrega | Estado |
|---|---:|---|---|
| 1. Identificar a área grande ativa | 0,25 dia | Partir de `wisp-forest-crossing012`, preservar `013` e manter a arena do chefão separada | Concluída |
| 2. Reaproveitar o kit Revisited | 0,25 dia | Nove árvores mortas v02 e três props 3D cadastrados | Concluída |
| 3. Adaptar texturas para os claros da floresta | 0,5 dia | Aplicar folhas, raízes e cinza às casas `plains`/`woods` sem mudar regras de terreno | Concluída |
| 4. Compor a revisão grande | 1 dia | Revisão 44 × 36 com máscara irregular, 1.146 casas jogáveis, todos os objetos, spawns e passagem preservados | Concluída |
| 5. Validar catálogo e preparar terreno | 0,5 dia | Exportar cena sem itens ignorados e cozinhar pacote de terreno versionado | Concluída; revisão 017 exporta 93 cards, 12 modelos e 33 unidades sem itens ignorados |
| 6. Ligar à campanha | 0,25 dia | Fazer a missão Wisp Forest Crossing usar `wisp-forest-crossing017.json` | Concluída |
| 7. Revisar visualmente | 1 dia | Render offline no Blender e capturar com Playwright; conferir a sessão real do jogo quando disponível | Prévia Blender/Playwright concluída; captura dentro do jogo não confirmada |

## Revisão ativa

- `content/maps/ember/wisp-forest-crossing017.json` é a versão ligada à campanha. Ela deriva da 016; as revisões 012–016 e `wisp-forest-crossing-boss005.json` permanecem preservadas.
- A máscara irregular 44 × 36, os 1.146 hexes jogáveis, 33 spawns, o rio, as ruínas, os baús, as saídas e os marcadores que encaminham ao covil foram mantidos. Os 120 props repetidos ou destoantes foram retirados da nova revisão; as versões anteriores continuam intactas.
- As nove árvores mortas 3D v02 e três props 3D — toco quebrado, tronco oco caído e raízes com fungos — permanecem na revisão 017. Cada árvore fica ao menos quatro hexes das outras novas.
- A névoa de guerra voltou a `true`, como na revisão 015. O terreno, as variantes e a distribuição de spawns não foram alterados durante a limpeza.
- As variantes são visuais: os terrenos continuam `plains`, `woods`, `ruins`, `nave` e `water`, com a mesma passagem e distribuição de spawns. Folhas, raízes e cinza cobrem as 888 casas de floresta (`plains` e `woods`); as ruínas e a água mantêm suas superfícies.
- O mapa conserva iluminação, horário, ambiente, wisps e efeitos da revisão ativa `012`. Nenhuma configuração de luz, céu, shader ou VFX foi alterada.
- A campanha agora referencia o mapa `017`; o mapa separado do chefão continua em `wisp-forest-crossing-boss005.json`.

## Materiais e pacotes

- As texturas fotográficas de folhas mortas, solo enraizado e cinza foram reutilizadas do kit Wisp Forest Revisited. Seus mapas normal/roughness foram registrados para `plains` em chaves próprias; os assets de `woods` continuam iguais.
- Variantes aplicadas em `015`: 647 células `plains` com folhas, 111 com raízes e 18 com cinza; 79 células `woods` com folhas e 33 com raízes.
- `content/data/board.json` contém 46 layers e corresponde ao export do Engine2. O cooker exportou 213 cards, 12 modelos 3D e 33 unidades, sem itens ignorados.
- `build/ground/wisp-crossing-revisited-ground-v01.pak` cozinhou 248 texturas sem falhas. Os meshes v02 reutilizam `build/meshes/wisp-forest-revisited-v02.pak`, que já contém as nove árvores v02 e os três props.

## Captura e validação pendente

A prévia de composição da revisão 017 foi renderizada offline no Blender e capturada em seguida com Playwright/Chrome. A cena exportada reconhece os 12 modelos novos e as 33 unidades sem ignorar itens. A imagem confirma a distribuição visual dos props e das superfícies, mas não simula névoa nem substitui uma captura da partida.

## Arquivos principais

- Recomposição: `tools/scripts/maps/recompose_wisp_crossing_revisited.py`
- Variantes de superfície: `tools/scripts/maps/apply_wisp_crossing_surface_variants.py`
- Prévia Blender: `tools/scripts/models/render_wisp_crossing_preview.py`
- Mapa ligado à campanha: `content/maps/ember/wisp-forest-crossing017.json`
- Campanha: `content/data/campaign.json`
