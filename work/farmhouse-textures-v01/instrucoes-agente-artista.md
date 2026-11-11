# Instruções para o agente artista: texturas das casas do Farmlands (kit `farmhouse` v01)

Cole o bloco abaixo na sessão do agente artista.

```
Tarefa: terminar de aplicar as texturas fotográficas nos objetos 3D das casas do Farmlands (kit farmhouse v01).
Responda em português.

REGRAS (valem em todo passo)
- Leia AGENTS.md primeiro (o chão do Bosque Morto é travado).
- Nunca sobrescreva nem apague arquivo existente: mudança = versão nova (-v02, -v03...).
- Não rode o jogo (Sandbox.exe) nem capturas. PC fraco: Blender uma peça por vez com -t 2, nada em paralelo, sem compilar.
- Mostre ao usuário uma imagem do resultado e diga o que não foi conferido.
- Commit só dos seus arquivos; não troque de branch, não use stash/reset.

O QUE JÁ ESTÁ PRONTO (commit 05e36df3)
- content/models/textures/farmhouse-<material>-v01-{color,normal,roughness}.jpg para seis materiais:
  wood, dark-wood, iron, linen, clay, herbs. JPG 1024 x 1024, emenda nos 4 lados, cor sRGB,
  normal tangente OpenGL (+Y para cima), roughness em cinza (branco = áspero).
- tools/scripts/models/create_farmhouse_kit_v01.py já usa esses seis sozinho (função farm()); as 13 peças
  v01 em content/models/farmhouse/ foram geradas com eles. Para esses seis não há nada a fazer.
- Escala (uma repetição): wood, dark-wood, herbs = 1,2 unidade; iron, linen, clay = 0,6 unidade
  (o UV_SCALE do kit já aplica). Metal: iron 0,8; os outros 0.
- Folha de contato: work/farmhouse-textures-v01/contact-sheet.png. Conferência por material:
  work/farmhouse-textures-v01/check-<material>.png.

O QUE SABER AO USAR OS SEIS
- wood: 8 tábuas de 0,15 unidade empilhadas no V, veio no U, com juntas de topo. Em peça fina (perna, cabo,
  moldura) use dark-wood, que não tem divisa de tábua.
- herbs: são só folhas secas; os talos do molho vêm da malha.
- iron: as facetas de martelo estão só no normal; se a peça ficar "amassada" demais, baixe a força do normal.
- linen: três remendos por repetição; em pano grande eles se repetem a cada 0,6 unidade.
- clay: vidrado castanho-esverdeado com lascas; para pão e barro cru continue usando o terracotta puro.

O QUE FALTA: straw (palha trançada / vime)
Hoje a palha é um substituto (wt-burlap.jpg tingido) em três peças: herb-rack (cesto), spinning-wheel (cesto)
e cradle (colchão de palha). A fotografia ainda não existe; o prompt para gerá-la está em
work/farmhouse-textures-v01/falta-palha-prompt.md.

A. Quando a foto chegar em Attachments/Farmhouse/farmhouse-straw-v01-color.jpg (2048 x 2048):
   1. python work/farmhouse-textures-v01/make_farmhouse_textures.py straw
      Gera work/farmhouse-textures-v01/out/farmhouse-straw-v01-{color,normal,roughness}.jpg e
      work/farmhouse-textures-v01/check-straw.png.
   2. Abra check-straw.png (o primeiro painel é a cor 2 x 2). A trama tem de continuar na emenda.
      Se o script disser "does not tile by itself" e a trama quebrar na faixa misturada, NÃO use:
      peça outra foto ao usuário.
   3. python work/farmhouse-textures-v01/make_farmhouse_textures.py publish
      Copia para content/models/textures/ (nunca sobrescreve).
   4. python work/farmhouse-textures-v01/make_farmhouse_textures.py sheet
      Refaz a folha de contato, agora com os sete.

B. No kit (tools/scripts/models/create_farmhouse_kit_v01.py):
   1. A palha repete a cada 0,6 unidade: acrescente 'straw' ao UV_SCALE e reveja os us=/vs= das chamadas de
      'straw' (cestos do herb_rack e do spinning_wheel, colchão do cradle), que foram ajustados para o substituto.
   2. Gere de novo SÓ as três peças, como versão nova (v02). Os v01 já estão no commit: não use
      --replace-unreleased neles. Uma peça por vez:
      blender -b -t 2 -P tools/scripts/models/create_farmhouse_kit_v01.py -- herb-rack
      (depois spinning-wheel, depois cradle)
   3. Registre as v02 (register_farmhouse_kit_v01.py lê build/models-tmp/farmhouse/manifest.json) e aponte as
      casas para elas. Confira no manifest que stand_ins ficou vazio.
   4. Mostre ao usuário as três peças (render de prévia como em work/farmhouse-kit-v01-preview/) e a folha de
      contato nova.

SE UM MATERIAL PRECISAR MUDAR
A receita de cada um está em work/farmhouse-textures-v01/make_farmhouse_textures.py (uma função por material).
Mudança = arquivos -v02 novos; os -v01 ficam como estão. Créditos das fotos: fim de content/models/CREDITS.md.
```
