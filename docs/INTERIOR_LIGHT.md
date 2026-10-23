# Luz interna: como Dragon Age: The Veilguard faz, e o que isso muda aqui (2026-10-23)

Pedido do usuário: "estudar como funciona a luz interna em jogos como Dragon Age e aplicar as novas portas e janelas sem precisar
alterar nada nas paredes de fora". Estudo primeiro (fontes no fim), depois o mapa para o que já existe no EmberEngine
(`engine/world/ProbeBake.h`, MapStage `startProbeBake`, `buildRoof`, as saias e o plinto: a "Luz das janelas" de 2026-10-21/22).

## 1. O que a BioWare fez no Veilguard (Frostbite, REAC 2025, Kleber Garcia / Darrin Stewart / Navjot Garg)

- **Tudo iluminado por sondas** ("Runtime: fully probe lit, streamable volumetric probe system"), colocadas automaticamente
  (GiGrid), com lightmaps só onde a sonda vazaria luz demais ("Lightmap for objects that could just leak too much"). Equipe de 15
  pessoas de conteúdo: "no content capacity to do much lighting", por isso nada de luz artesanal por sala.
- **Densidade: até 0,5 m** entre sondas na resolução máxima; orçamento de 500 MB em streaming; "do as much in probes".
- **Vazamento de luz pelas paredes: resolvido na arquitetura, não no shader.** "Light leaking: build chunky architecture.
  Thinnest wall .5m." Ou seja: parede mais fina que meia sonda vaza; a regra do kit é parede grossa.
- **O que a sonda guarda:** irradiância em harmônicos esféricos (Flux, o path tracer da Frostbite) e **visibilidade do céu
  ("skyvis")** por sonda; o skyvis vai para o G-buffer e é lido por materiais e névoa. Lição deles: ler iluminação dentro do grafo
  de material custa caro; eles pré-amostram o skyvis numa cascata 3D ao redor da câmera (1 tap).
- **Costuras entre sondas:** vêm de bakes não determinísticos entre máquinas e da estratégia de amostrar faces traseiras (sonda
  dentro de parede); a resposta é marcar e descartar essas sondas.
- **Bake:** 10-15 min por nível, distribuído; a névoa volumétrica lê as mesmas sondas.

## 2. O que a Frostbite faz com janelas (GDC 2018, Yuriy O'Donnell, "Precomputed GI in Frostbite")

- Lightmaps e volumes de irradiância guardam **só a luz indireta e a do céu**; sol e luzes locais são dinâmicos.
- **Não existe "luz de janela" especial.** A luz que entra por uma abertura é o path tracer do bake vendo a abertura: "Indoor scenes
  require more rays ... the probability of a light path bouncing several times and then hitting a window is quite low, while the
  lighting contribution of such low probability path may be extremely high." Por isso interiores recebem muito mais raios.
- Resultado: a janela ou a porta ilumina a sala porque a geometria tem o buraco; nada é acrescentado à parede.

## 3. O EmberEngine já faz isso, e um pouco mais

| Veilguard / Frostbite | EmberEngine hoje (Luz das janelas) |
|---|---|
| Sondas SH auto-colocadas, até 0,5 m | Grade de sondas da sala, 1,0 x 0,75 x 1,0 unidades (hex = 1,73 x 1,5), `kProbeSpacing` |
| Irradiância cozida uma vez para um céu fixo | **Transferência PRT 9x9 por sonda**: o céu real da hora reacende as sondas a cada frame (FFXV) |
| Sol dinâmico, sua quicada não | Tabela da quicada do sol/lua por 64 direções, interpolada na direção real |
| skyvis por sonda | Está dentro da transferência (o que a parede e o teto escondem sai da matriz) |
| Sondas dentro de parede descartadas | `ProbeBakeOutput::valid` + máscara da sala a 0,25 (`kProbeMaskSpacing`) |
| Parede mínima 0,5 m contra vazamento | Paredes do kit 0,61-1,3; **vazamento é tratado pela máscara e pelas saias** |
| Janela = buraco na geometria que o bake vê | Igual: janela, seteira, porta e portal são peças com o vão; bake e shadow map veem o vão |
| Lightmaps onde vaza | Nenhum (não precisamos: a máscara zera a luz de fora fora da sala) |
| Fogo estático quicado nas sondas | `ProbeBakeInput::Fire`: tochas e braseiros quicam nas sondas |

Conclusão: **a mecânica "porta e janela trazem a luz de fora, que nunca é desenhada" já é a do Veilguard e da Frostbite**, com a
vantagem de reacender pelo relógio. Não há técnica deles a importar para isso funcionar; há três ajustes que eles fazem e nós não:

1. **Densidade 0,5 m.** Nossa grade é ~1 unidade. Descer para 0,5 x 0,5 x 0,5 unidades = 8x sondas (o bake da torre: 14.640 sondas
   hoje → ~117k; o bake usa cache). Ganho: feixe de janela mais nítido no chão perto das paredes, menos "mancha".
2. **Portas animadas.** Veilguard não trata (bake estático). Aqui a porta aberta deixa passar o sol (o shadow map vê a folha
   girada) mas a quicada do céu nas sondas é a do bake, feito com a porta **fechada ou aberta como estava na cena**. Opção
   barata: cozinhar a transferência duas vezes por porta animada (aberta / fechada) e misturar pelo ângulo da folha.
3. **Skyvis pré-amostrado para a névoa.** Se a Atmosfera voltar a ser ligada em salas, a névoa deve ler as sondas (eles leem),
   não o céu inteiro.

## 4. "Aplicar as novas portas e janelas sem alterar nada nas paredes de fora"

As janelas e portas do kit (`ember-arch-*-window / -slit / -door / -portal / -gate`, `inn-window / inn-door / inn-doorway`)
**são** peças de parede com o vão; aplicá-las a um mapa é trocar a peça de parede pela peça de janela na célula (Map Editor:
"TORRE DE VIGIA" / "ESTALAGEM", Traçar parede). Nada da parede ao lado muda; as saias (cópias só-sombra do lado de fora) e o
plinto são gerados sozinhos a partir do anel externo, por isso "sem alterar as paredes de fora" já vale: o mapa é a única
coisa editada.

O que está travado: **a Estalagem** (seu kit, seu mapa 005 com 16 janelas e 7 portas) só muda com a palavra secreta. A Torre de
Vigia não está travada: os ajustes 1 e 2 podem ser feitos nela.

## Fontes

- BioWare, "Dragon Age: The Veilguard - GI, RT, Character Creator and other systems", REAC 2025:
  https://enginearchitecture.org/downloads/REAC_2025_Bioware.pdf (resumo: https://cs.fit.edu/~pkc/dept/seminar/garcia.html)
- Yuriy O'Donnell, "Precomputed Global Illumination in Frostbite", GDC 2018:
  https://media.contentapi.ea.com/content/dam/eacom/frostbite/files/gdc2018-precomputedgiobalilluminationinfrostbite.pdf
- Elcott et al., "Rendering Techniques of Final Fantasy XV", SIGGRAPH 2016 Talks (a base das nossas sondas: docs em ProbeBake.h).
