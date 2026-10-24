# Farmlands Revisited — produção

## Cronograma

| Fase | Duração estimada | Entrega | Estado |
|---|---:|---|---|
| 1. Leitura do mapa e preservação | 0,5 dia | Revisão isolada, dimensões mantidas, casas e conectores originais preservados | Concluída |
| 2. Direção visual e composição | 0,5 dia | Panorama rural, plano aéreo agrícola e desenho dos cinco talhões | Concluída |
| 3. Materiais de terreno | 1 dia | Texturas de solo arado, trigo e pasto, versionadas | Concluída |
| 4. Objetos 3D em Blender | 1–2 dias | Tile hexagonal de trigo, tile de capim alto e fardos de palha, com fontes `.blend` e exportações `.glb` | Concluída; 336 modelos carregados na cena do jogo |
| 5. Montagem da revisão | 1 dia | Cinco áreas de cultivo, margens de capim e passagens entre as casas | Concluída; 292 tiles de trigo, 40 de capim e 4 agrupamentos de fardos |
| 6. Integração de mapa e panorama | 0,5 dia | Campanha apontando para a revisão, ground image e panorama registrados | Concluída em `farmlands-revisited002`; pacotes `ground-v38` e `props-v7` cozidos |
| 7. Verificação na superfície real | 1 dia | Cozinhar os pacotes e capturar o mapa no jogo em vista ampla e aproximada | Concluída; missão da campanha e cena nativa capturadas |
| 8. Ajuste final | 0,5–1 dia | Ajustar escala, densidade e transições a partir das capturas | Concluída; base do mapa reduzida, luz de entardecer calibrada e ground contínuo selecionado |

## Composição aprovada nesta revisão

- O tabuleiro permanece em 60 × 42 hexes; as casas, árvores, entradas, conectores e spawns originais ficam no lugar.
- Cinco talhões irregulares usam o novo tile 3D de trigo. As margens recebem capim alto esparso, e quatro pequenos agrupamentos de fardos de palha marcam áreas de trabalho.
- O panorama `fields-revisited-panorama-v01.png` e o ground image `fields-revisited-ground-v03.png` têm nomes próprios para este mapa. As versões anteriores do ground e todos os assets anteriores continuam preservados.
- Os modelos de vegetação são passáveis e decorativos; as passagens entre as casas ficam livres.
- Revisões intermediárias de modelos continuam preservadas nos arquivos e não são usadas pela campanha; as versões referenciadas são capim `v02`, trigo `v03` e fardos `v02`.
- A revisão ativa é `content/maps/ember/farmlands-revisited002.json`: a base de terreno está no nível 0 para encurtar a borda vertical e aproximar o tabuleiro do pasto.

## Critérios de aceite visual

1. Abrir a missão real da campanha e confirmar que o tabuleiro 60 × 42, os talhões, as casas e o panorama carregam juntos.
2. Aproximar até os hexes e conferir que folhas e espigas têm volume 3D, não recortes planos.
3. Verificar a junção do pasto e do ground image. A versão v03 e escala de 180 unidades reduzem as manchas de primeiro plano vistas nas capturas intermediárias.
4. Conferir o horizonte do panorama na orientação da câmera e sua continuidade nos lados.
5. Confirmar que as casas, árvores, conectores, spawns e hexes originais permanecem no lugar; wheat e capim estão marcados como decorativos e passáveis.
