# Atmosfera volumétrica (v1): auditoria, plano, integração

**REGRA FIXA INQUEBRÁVEL DO JOGO (o usuário, 2026-10-23): "só existe uma atmosfera no jogo, não importa qual o mapa o céu é o
mesmo, sempre sempre sempre só existe um único céu nesse jogo nunca vai existir excessão".** Os perfis de clima deste documento
mudam a névoa no ar à frente do céu; o céu (tabelas da atmosfera, sol, lua, estrelas, nuvens, exposição) é um só para todos os
mapas, pelo mesmo código, sem variante, escala, flag ou caminho por mapa.

Pedido do usuário (2026-10-16): um sistema atmosférico cinematográfico (névoa volumétrica 3D, luz espalhada no ar,
clima regional, partículas, perfis por região), versionado, sem tocar nos assets e sistemas existentes, com OFF e
rollback; identidade visual Luminous, referência técnica UE5 ("algo que me cause orgulho"). Este documento é a
Fase 1 (auditoria + plano) e acompanha a implementação (seções 5+ crescem a cada fase).

## 1. O renderer, como está (verificado no código, 2026-10-16)

- **Vulkan 1.3**, `engine/rhi`: bindless (um set: sampled images, samplers, storage images, storage buffers; 128 B
  de push constants em todos os estágios), dynamic rendering, sync2, `GpuTimer` (timestamps por passe), VMA.
  Device: RX 6400 (RDNA2, 4 GB). Shaders HLSL via DXC (`cmake/Shaders.cmake`, `ember_shaders(...)`), um `.spv`
  por entry point em `build/<config>/bin/shaders/`.
- **Alvos**: `sceneColor_` RGBA16F, `depth_` reversed-Z infinito (depth = near / distância), `output_` RGBA8;
  `visibility_` R32 (visibility buffer das malhas), `hiz_` (pirâmide de profundidade), shadow maps (sol 4096 em
  cache + cards por quadro). Sem motion vectors, sem normal buffer, sem AO.
- **Ordem dos passes** (`Renderer::record`): constantes do quadro → upload da máscara do fog of war → clusters de
  luz (16×9×24, até 63 luzes/cluster) → tabelas da atmosfera Hillaire + SH ambiente (todo quadro) → sombra do sol
  (chão + malhas, em cache) → sombra dos cards (todo quadro) → **névoa por froxels antiga** (`volume.hlsl`,
  160×90×64, só no modo de luz 0) → malhas (cull, visibility, resolve → cor HDR; fundo = céu / pintura / preto) →
  passe de cena (chão, água, água 3D, cards; luz via `lighting.hlsli shade()`) → efeitos (flipbooks + VFX
  portados, aditivos, soft particles) → **banco de névoa do fog of war** (`fog_lookNNN.hlsl`: march em meia
  resolução + composite sobre a cor) → tonemap (ACES do three.js + bloom do Engine2, 24 amostras) → UI → cópia.
- **Luz**: `lighting.hlsli` = o contrato. Sol com dois shadow maps, luzes locais por cluster (falloff do
  three.js, decay 2 no modo "real"), ambiente = SH9 do céu físico (ou hemisfério), exposição multiplicada nas
  luzes (`f.exposure`, `f.sunIntensity`, SH). Modo "real" (padrão): sol 128 000 lux × transmitância, lâmpadas em
  lumens → candela, câmera física (EV100 do medidor incidente, adaptação 1,5/s, curva noturna).
- **Como a névoa chega às superfícies hoje**: `applyVolume()` dentro de `shade()`: cada superfície (chão, malhas,
  cards, água 3D) lê o volume integrado **na sua própria profundidade** (trilinear nas fatias). É a composição
  consciente de profundidade do método de froxels (Wronski 2014 / Frostbite 2015 / **Volumetric Fog da UE**):
  não há halos nem upsample. Efeitos aditivos, VFX e o fundo **não** leem o volume (ficam "colados" na frente da
  névoa): ponto de integração mínimo a abrir (seção 4).
- **Fog of war (R7)**: `world/FogOfWar` (estados do Engine2; **linha de visão nossa, por oclusão**, raio 7);
  máscara RGBA16F 8 texels/raio de hex: r escuridão (0 / 0,6 / 1), g borda do nunca visto (blur), b altura do
  chão (média sobre o nunca visto: não revela forma), a vale. Toda luz, sombra, card e efeito passa por
  `tacticalSight()`: nunca visto = preto e não desenhado; lembrado = ×0,4 sem luz local. Luzes locais fora de
  vista são descartadas na CPU. O volume antigo já obedece: froxel nunca visto não espalha nada (só absorve).
  **A spec colada pede "raio 7, sem linha de visão"; o nosso fog of war é por oclusão, escolha do usuário
  (2026-10-15): fica como está.**
- **Câmera**: orbital (MapStage.h topo); **Tática** = olho reto acima do pivô, lente de 1°, distância × tan15°/
  tan0,5° (~30× a distância normal), near = distância − 200, cards deitados (`kSpriteFlat`), sem sombras, sem
  perspectiva aérea nem haze. O `Camera` é o mesmo struct para os dois modos: o sistema novo usa as matrizes
  reais e segue o plano near (como os clusters de luz já fazem: `clusterNear = max(2, near)`).
- **Partículas existentes**: wisps + motes do Engine2 (ImGui, aditivos, sem profundidade, `MapStage` ~l. 2766);
  VFX dos feitiços com `ParticleEmitter` próprio. Nenhum sistema de partículas de ambiente no GPU.
- **Limites encontrados**: fragment shaders não escrevem storage buffers (sem fragmentStoresAndAtomics: ler via
  device address); DXC: passar `AtmosphereFrame` por valor lê campos errados (passar campo a campo); sem sombras
  das luzes locais (nenhum atlas); o look 008 (névoa do próprio mapa) perdeu o GPU (`VK_ERROR_DEVICE_LOST`) e foi
  removido sem causa achada: qualquer passe novo entra com validação em debug antes do release.

## 2. Base medida (release, 60 Hz pacado, 1600×900, aldeia015 ao crepúsculo, 14 unidades, 12 luzes, fog of war)

| Modo de luz | GPU mediana / 99º / pior | Passes (média, ms) |
|---|---|---|
| 3 real (padrão) | 11,70 / 12,30 / 12,34 ms | **banco de névoa 6,44**, chão 2,06, tonemap+bloom 1,70, atmosfera 0,48, resolve 0,45 |
| 0 protótipo (haze por froxels ligada) | 11,62 / 14,55 / 32,14 ms (3 quadros > 16,7) | banco 6,42, chão 2,20, tonemap 1,73, **froxels 0,39** |

Thread principal: mediana 0,93 ms. Leitura: a névoa do fog of war (look 003, march de 40 passos em meia
resolução sem filtro temporal) é **55 % do quadro**; o bloom de 24 amostras custa mais que a atmosfera inteira; a
grade de froxels 160×90×64 lida pelo sol, SH e clusters custa 0,4 ms. Orçamento do subsistema novo: 3–5 ms
(spec); meta própria: ≤ 2 ms no perfil "alta" nesta GPU, e substituir o custo do banco, não somar a ele.

Captura de referência: `captures/atmos/baseline-aldeia015-real.png`. Sequências em um só processo (quadros
consecutivos com a câmera girando): `Sandbox ... --capture <prefixo> --capture-sequence FROM:EVERY:COUNT`
(novo, `main.cpp`), para julgar ghosting, cintilação e histórico temporal.

## 3. Desenho do subsistema (v1)

Um módulo, uma versão, OFF por escolha, nada em cima dos arquivos existentes:

- **`engine/render/Volumetrics.h/.cpp`** (classe `Volumetrics`, `kVersion = 1`): dono das suas imagens 3D (duas
  grades de froxels para o histórico + a integrada, tamanho por nível de qualidade), dos seus pipelines
  (`volumetrics_v1.CSInject`, `volumetrics_v1.CSIntegrate`), do seu registro de quadro (`VolumetricsFrame`, um
  buffer por quadro em voo) e do seu estado (perfil atual, perfil alvo e tempo de transição, qualidade, histórico
  válido). Ciclo de vida próprio: `enable/disable`, `setQuality`, `setProfile(profile, seconds)`,
  `invalidateHistory()` (corte de câmera, Tática), `reportGpuMs()` (qualidade adaptativa com histerese).
- **`shaders/volumetrics_v1.hlsl`**: grade de froxels no espaço da câmera (fatias logarítmicas do near ao alcance
  do perfil), **campo de densidade 3D**: extinção σt = `3.912 / visibilidade` (Koschmieder) × queda exponencial
  com a altura sobre o chão × ruído fbm (vento, escala do perfil) × (1 + vale) + **volumes locais** (até 16
  caixas com feather: cavernas, clareiras, corredores) + **banco do nunca visto** (canal g da máscara) opcional.
  **Luz no ar**: sol × shadow map (+ cards) × fase Henyey-Greenstein de dois lobos; céu = termo DC do SH
  (E/π, isotrópico); luzes locais do cluster do froxel (mesmo falloff das superfícies) × fase × **oclusão pelo
  relevo** (3 amostras contra a altura do chão da máscara a caminho da luz: uma lâmpada atrás de um barranco não
  acende a névoa do outro lado); tudo × regras do fog of war (nunca visto: nada espalhado; lembrado: ×0,4 sem luz
  local). **Temporal**: reprojeção do froxel na matriz do quadro anterior, jitter Halton nas fatias, peso do
  histórico por perfil de qualidade, histórico zerado em cortes. **Integração** front-to-back energeticamente
  conservativa (Hillaire 2015) para (luz espalhada rgb, transmitância a) por fatia.
- **Composição**: as superfícies continuam lendo `f.volume` em `applyVolume()` (profundidade exata, sem halos).
  O renderer aponta `f.volume` para a grade integrada do v1 quando ele está ligado; a haze antiga segue intacta
  (modo 0) e nunca corre junto. Efeitos aditivos / VFX recebem a transmitância na sua profundidade; o fundo
  pintado recebe a última fatia; ambos só com o v1 ligado (bits novos em `MeshFrame`), para que o OFF devolva
  exatamente a imagem de antes.
- **Perfis regionais** em `content/atmosphere/v1/profiles.json` (versionado): `padrao`, `mordavia` (inverno),
  `bosque-dos-fogos-fatuos` (Wisp Forest), `profundezas`, `templo`, `devastada`, `natural`. Campos: visibilidade,
  altura de escala, albedo (cor), anisotropia, vento, ruído (escala, força), multiplicadores do sol / céu / luzes
  locais, escala na Tática, alcance máximo, banco do nunca visto. Campos de clima e partículas ficam reservados
  (lidos, ainda não usados) até as Fases 5–6. Transição entre perfis: interpolação linear em segundos.
- **Qualidade**: cinematográfica 192×108×96 (sol 4 amostras, oclusão 3), alta 160×90×64 (1, 2), desempenho
  96×54×48 (1, 0, histórico mais pesado). Adaptativa: desce um nível após N quadros acima do orçamento, sobe só
  após M quadros abaixo da metade (histerese), tudo dentro do módulo.
- **Painel "Escolhas"** (Modo teste): "Atmosfera": OFF / cinema / alta / desempenho; "Clima": os perfis. Sem
  teclas. Dev Controls "FX atmosféricos" OFF desliga tudo.

## 4. Integração mínima (cada ponto documentado; rollback = remover a chamada)

| Arquivo | Mudança | Rollback |
|---|---|---|
| `shaders/scene.hlsli` MeshFrame | `pad10` vira `volumeSlices` (nº de fatias do volume ligado) e `volumeFlags` (1 efeitos, 2 fundo) | pad10 de volta; a haze antiga escreve kFroxelZ / 0 |
| `shaders/lighting.hlsli applyVolume` | `0.5 / kFroxelZ` → `0.5 / f.volumeSlices`; função nova `volumeTransmittance()` | idêntico com volumeSlices = kFroxelZ |
| `shaders/effects.hlsl`, `vfx.hlsl`, `resolve.hlsl backgroundAt` | × transmitância / última fatia quando `volumeFlags` manda | bits 0 = como antes |
| `engine/render/Renderer.h/.cpp` | membro `Volumetrics`, 3 chamadas (criar, preparar o quadro, gravar os passes com escopo "volumetrics") | remover as chamadas |
| `apps/sandbox/MapStage.h` | escolhas do painel, perfil do mapa, `invalidateHistory()` ao entrar / sair da Tática | remover |
| `apps/sandbox/CMakeLists.txt`, `engine/CMakeLists.txt` | o shader e a fonte novos | remover |

Nada dos looks salvos (001–007), de `volume.hlsl`, da atmosfera Hillaire, dos sprites, dos feitiços ou dos assets
é editado.

## 5. Riscos e limites declarados

- **Sem sombras das luzes locais**: a oclusão da luz no ar é pelo relevo (altura da máscara), não por paredes e
  casas (sem shadow map local). Luz pode atravessar uma parede no ar como já atravessa nas superfícies. Fase D do
  plano de luz (ARCHITECTURE §7.6) resolve para os dois.
- **Froxels são grossos** (10 px na tela, fatias logarítmicas): feixes de luz são suaves, nunca cortantes. Para
  "god rays" finos é preciso um passe de ray-march por pixel (como o banco faz): fica para avaliar com o custo.
- **Ghosting** com luzes que piscam e feitiços: peso do histórico 0,85–0,9; verificar na sequência.
- **Tática**: a grade segue o near (≈ 700+ unidades); a névoa aparece como véu uniforme visto de cima, escalado
  pelo perfil (legibilidade). Sem perspectiva aérea lá, como antes.
- **Orçamento**: esperado 0,5–1,5 ms por nível nesta GPU (medido: a grade antiga 0,39 ms); se a cinematográfica
  passar de 2 ms, a adaptativa recua.

## 6. Validação (2026-10-16, o que foi feito e o resultado)

1. **Debug + validação:** capturas de aldeia015 com `--atmos 0/1/2/3` e `--tatica`: saída 0, 0 erros de validação.
2. **Rollback:** `Sandbox.exe --atmos 0` (release novo) contra o exe de antes da mudança (`Sandbox-locked-1432.exe`,
   mesmos shaders novos, mesmo `--scene-time`, sem regras de batalha): **0 pixels diferentes** (o próprio exe antigo
   varia 632 k pixels entre duas execuções suas por causa do ruído por quadro do banco de névoa; a comparação é
   contra a execução idêntica). O OFF devolve a imagem de antes.
3. **Fog of war:** `check_fog.py` com o banco de névoa ligado FALHA já no OFF (o banco pinta sobre o fundo preto, e
   com "imagem do mapa" a pintura aparece no nunca visto: o teste é de 2026-10-14, antes dos looks e do fundo); por
   isso o teste corre com `--mist 0 --background 2` (máscara e atmosfera ligadas, sem o banco), 4 momentos com
   batedor, tocha e feitiço: **OFF 34 pixels não pretos (brilho máximo 10 de 765) num momento, atmosfera "alta" 30
   (máximo 7)**: a mesma fuga mínima de antes (não é da atmosfera: com ela há menos), 0 nos outros três momentos.
   Regra mantida no v1: ar nunca visto não espalha nada (só absorve) e o ar lembrado é escurecido ×0,4 sem luz
   local; luzes locais fora de vista já não chegam ao GPU; o fundo preto do nunca visto não recebe névoa.
4. **Temporal (a tremulação, 2026-10-16):** o usuário mostrou o ângulo (câmera baixa olhando ao longo do tabuleiro, ar
   denso) onde "a fumaça treme sem parar". Medido com `--capture-sequence 70:1:12` e câmera parada (floresta, `--tilt
   -12 --zoom 2`): diferença média entre quadros consecutivos OFF 0,63 (animação das unidades e wisps), névoa com
   jitter 0,77 (terço de cima 0,18 → 0,32); filtrar o ruído ao tamanho da fatia e amostrar a luz no centro fixo da fatia
   não mudaram nada; **sem jitter de profundidade: 0,57 (0,18)**, igual ao OFF. Causa: jitter + histórico exponencial
   (Wronski / Frostbite) mantém para sempre um resíduo de (1 − peso) × a variação da fatia, que numa fatia distante e
   densa é visível. Decisão v1: sem jitter (cena parada = quadros idênticos; 64–96 fatias lidas trilinearmente não
   mostram bandas); o histórico (peso 0,94) fica só para suavizar o movimento da câmera e reinicia sem tremor (peso
   volta em 4 quadros). O ruído filtrado por fatia ficou (é correto e barato). Qualidade adaptativa desligada por
   padrão (as trocas de nível reiniciavam o histórico: "voltou alguns segundos depois"). A confirmar pelo usuário.
5. **Custo (release, 60 Hz pacado, aldeia015, fog of war, regras de batalha):** "volumetrics" alta 0,59 ms (pior
   2,85), cinema 1,86 (pior 7,69); quadro mediano 12,8 ms contra 11,7 de base. Dentro da meta (≤ 2 ms em alta).
6. **Tática:** `--tatica --atmos 2`: captura sai, 0 erros; o véu segue a escala do perfil (`tacticalScale`).
7. **DXC:** a amostra da última fatia com a coordenada montada inline no `float3` lia 0 num caminho e o valor certo
   noutro; corrigido com `volumeFarSample` (coordenada numa variável). Anotado no CLAUDE.md.

## 7. O upgrade gráfico, além da névoa (pedido do usuário; a mesma série versionada)

Candidatos pelo que a base mostrou, em ordem de ganho por custo: (a) **bloom físico** (cadeia de downsample /
upsample de Jimenez 2014: melhor e mais barato que as 24 amostras, 1,7 ms); (b) **banco de névoa do fog of war
dentro do volume** (devolve ~6 ms e unifica a névoa, os looks antigos ficam no menu); (c) **transformação de
display estável em matiz** (AgX) como look novo ao lado do ACES; (d) **GTAO** (oclusão do céu, contato); (e)
**sombras das luzes locais** (atlas); (f) partículas de ambiente no GPU (neve, cinza, poeira) com vento. Cada
um: look numerado, OFF, medido.
