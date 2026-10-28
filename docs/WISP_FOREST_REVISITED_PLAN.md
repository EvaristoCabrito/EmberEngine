# Wisp Forest Revisited

## Cronograma de produção

| Fase | Duração estimada | Entrega | Estado |
|---|---:|---|---|
| 1. Preservar e ler a revisão atual | 0,5 dia | Fonte `wisp-forest007` preservada; conferir grade, contorno irregular, spawns, marcadores e objetos existentes | Concluída |
| 2. Direção das nove árvores | 0,5 dia | Nove silhuetas reconhecíveis: salgueiro, carvalho, tronco oco, árvore arrancada, árvore partida, espiral, pinheiro queimado, copa quebrada e arco de raízes | Concluída |
| 3. Texturas PBR | 1 dia | Casca morta, folhas úmidas, solo enraizado e folhas com cinza; mapas de cor, normal e rugosidade em arquivos novos | Concluída |
| 4. Modelagem 3D no Blender | 1–2 dias | Fontes `.blend` e exportações `.glb` das nove árvores, três tiles hexagonais e três props complementares | Concluída |
| 5. Revisão independente do mapa | 1 dia | Revisões aditivas 20 × 20; contorno irregular, bosque escuro, clareira assimétrica, nove árvores novas e três props; posicionamento ajustado na `wisp-forest010` | Concluída |
| 6. Catálogos e superfícies | 0,5 dia | 24 entradas novas (9 árvores v01, 9 v02 e 6 tiles/props), três variantes de chão com mapas normal/roughness e mapas `wisp-forest008`, `009` e `010` | Concluída |
| 7. Cozinhar o pacote 3D | 0,5 dia | Pacotes versionados de meshes v01/v02 e pacote versionado de terreno; materiais compartilhados e níveis de detalhe gerados | Concluída |
| 8. Inspeção visual na superfície real | 1 dia | Abrir a revisão no editor/jogo com Playwright, conferir leitura da clareira, colisões, alturas, tiles e capture de tela | Pendente: Playwright não está disponível neste ambiente; nenhum servidor de prévia foi iniciado |
| 9. Ajuste após a captura real | 0,5–1 dia | Corrigir apenas problemas visíveis na captura e cozinhar novo pacote versionado | Pendente da fase 8 |

## Conteúdo desta revisão

- Os originais ficam guardados: revisões `content/maps/ember/wisp-forest008.json`, `009.json` e `010.json` são aditivas; `wisp-forest007.json` não foi modificada. `010` é a composição escolhida para revisão visual.
- Os 20 × 20 hexes, o contorno irregular, os seis spawns e todos os nove itens decorativos anteriores foram preservados. Os 334 hexes jogáveis recebem chão de bosque; a clareira central usa cinza e folhas e as raízes se concentram junto às árvores e ao santuário.
- As nove árvores usam casca fotográfica e volumes de tronco, raízes, galhos e ramificações. As formas v02 ampliaram e diferenciaram as silhuetas após revisão do primeiro render; os arquivos v01 e v02 foram preservados.
- Os três tiles são folhas mortas, solo enraizado e folhas com cinza. Os props novos são toco partido, tronco oco caído e raízes com fungos silvestres.
- A revisão usa as wisps e a atmosfera existentes. Nenhuma luz, sombra, shader, céu, sol, lua ou efeito foi alterado.
- A exportação da cena da revisão `010` resolveu nove cards preservados, doze modelos 3D e dezesseis unidades sem itens ignorados. O mapa contém 21 decorações ao somar nove árvores novas, três props e os nove objetos decorativos anteriores.
- A prévia Blender offline do mapa e a folha de árvores são estudos de composição/modelagem; não substituem a inspeção visual na interface/jogo.

## Arquivos principais

- Blender: `content/models/blender/kit_wisp_revisited.py`
- Modelos e fontes: `content/models/wisp-forest-revisited/`
- Texturas de materiais: `content/models/textures/wisp-*-v01.*`
- Texturas de tiles: `content/assets/game/ground-variant-tiles/wisp-forest-*-v01.png`
- Mapas: `content/maps/ember/wisp-forest008.json`, `009.json`, `010.json`
- Definições das variantes: `content/game/src/ember/tileVariants.ts`
- Catálogo dos modelos: `content/models/models.json` e `content/data/editor-catalog.json`
- Pacotes cozidos: `build/meshes/wisp-forest-revisited-v01.pak`, `build/meshes/wisp-forest-revisited-v02.pak`, `build/ground/wisp-forest-revisited-ground-v01.pak`

## Validação pendente

O pacote de meshes v02 inclui quinze modelos (nove árvores v02 e seis tiles/props), 28.982 triângulos de origem, 1.359 clusters de geometria, 55.209 triângulos somados nos LODs e oito texturas compartilhadas. O pacote de terreno v01 cozinhou 238 texturas sem falhas. A cena exportada de `wisp-forest010` confirmou resolução dos modelos e objetos.

A captura automatizada com Playwright/Chrome foi feita para a prévia Blender offline. Ainda falta inspeção visual dentro do editor/jogo: o navegador empacotado pelo Playwright não está instalado e nenhuma sessão do jogo/editor ou servidor de prévia foi iniciada. A aprovação visual final aguarda abrir o mapa na superfície real e capturar essa tela com Playwright.
