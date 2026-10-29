// Mapa-múndi e Submundo: a luz e a névoa das duas telas do mapa (apps/sandbox/OverworldStage.h, render/WorldMapFx.h).
// A pintura 2D é desenhada por este passe no lugar da imagem da UI, dentro do próprio passe da UI (um callback do ImGui),
// com a luz da hora (mapa-múndi) ou dos seres que brilham (Submundo), a névoa de guerra viva, a névoa baixa / de caverna e
// o acabamento de cor. Nada disto toca a névoa, o céu ou a atmosfera das batalhas: é um sistema só destas duas telas.
// Os valores ajustáveis vêm de content/worldmap/*.json pelo buffer de parâmetros (WorldMapParams, render/WorldMapFx.h).
// O ruído vem de uma textura repetível feita uma vez na CPU (WorldMapFx: R = fbm de 5 oitavas, G = 3, B = 2, A = fino),
// uma leitura por campo em vez de dezenas de hashes por pixel.
#include "common.hlsli"

struct Push {
  float4 rect;      // a pintura na tela, em pixels do alvo: x, y, largura, altura
  float4 view;      // o viewport do mapa, em pixels: x, y, largura, altura (a vinheta é relativa a ele)
  uint map;         // a pintura
  uint mask;        // a máscara pintada (R = onde a névoa baixa / de caverna se acumula, G = onde há seres que brilham); kNoTexture = heurística
  uint reveal;      // mapa-múndi: o revelado (R, 1 = explorado) sobre a pintura inteira; kNoTexture = sem névoa de guerra
  uint samplerIndex;  // clamp
  uint flags;       // bit 0: Submundo; bits 1-2: qualidade (0 baixa, 1 média, 2 alta)
  float time;       // segundos (o vento)
  float rounding;   // os cantos arredondados da pintura, em pixels
  float glow;       // Submundo: o ponto do ciclo dos seres, 0 (dormindo) .. 1 (auge)
  uint paramsLo, paramsHi;  // WorldMapParams (float4 x 16), endereço de dispositivo
  uint noise;       // a textura de ruído repetível
  uint repeatSampler;
  // Mapa-múndi, o look 2D (o usuário, 2026-10-28): as LUTs de cor da hora de antes e de depois, a máscara de luzes (pintada
  // pelo usuário: preto, salvo onde algo brilha) e a sua cópia borrada (feita ao carregar), o mapa de altura (cinza: claro =
  // alto), a máscara da água e a da névoa. Sempre índices válidos (o ruído quando ausentes); a presença vem dos flags.
  // tex[]: mapa-múndi: 0 luzes, 1 luzes borradas, 2 altura, 3 água, 4 névoa. Submundo: 0 brilho fúngico (R = idade / força),
  // 1 o mesmo borrado, 2 plantas que se alimentam do brilho, 3 água, 4 zonas de esporos, 5 janelas, 6 ember (R = força), 7 ember
  // borrado, 8 tremor de calor.
  uint lutA, lutB;
  uint tex0, tex1, tex2, tex3, tex4, tex5, tex6, tex7, tex8;
};
[[vk::push_constant]] Push push;

#define kNoTexture 0xFFFFFFFFu

// WorldMapParams (render/WorldMapFx.h, static_asserted): 16 float4, já interpolados pela hora na CPU.
//  0 tint       rgb: a cor da luz da hora, w: exposição
//  1 grade      x contraste, y saturação, z lift (o mínimo da noite), w gama
//  2 light      xy: direção da luz na pintura (unitária), z força do relevo, w força dos raios (0 = sem)
//  3 vignette   x força, y raio, z suavidade, w ombro (0 = o branco pode estourar)
//  4 fogColor   rgb cor da névoa de guerra (linear), w densidade (alfa máximo)
//  5 fogShape   x escala do ruído (repetições sobre a pintura), y vento (unidades/s), z largura da borda, w sombreado de volume
//  6 lowColor   rgb cor da névoa baixa, w força
//  7 lowShape   x escala, y velocidade, z altura (quanto da máscara), w limiar da heurística
//  8 glowColor  Submundo: rgb cor da luz dos seres, w alcance do halo (em unidades da pintura)
//  9 dark       Submundo: rgb cor ambiente no escuro, w nível de escuro (0 = preto)
// 10 caveColor  Submundo: rgb cor da névoa de caverna apagada, w densidade
// 11 caveShape  x escala, y velocidade, z quanto a névoa pega a cor dos seres, w limiar da heurística
// 12 extra0     Submundo: x luz ambiente no auge (quanto os seres clareiam a caverna inteira)
// Mapa-múndi, o look 2D (2026-10-28), já interpolados pela hora na CPU (WorldMapFx::overworldParams):
// 13 hourMix    x mistura entre lutA e lutB (0 = A), y noite (0 dia .. 1 noite), z dia (0 noite .. 1 dia), w névoa da hora (0..1)
// 14 sun        xyz direção do sol (x, y na pintura, z para cima; unitária), w força do relevo
// 15 lightColor rgb cor das luzes à noite, w força
// 16 clouds     x força das sombras de nuvem, y escala (repetições sobre a pintura), z velocidade, w tremor das luzes (0..1)
// 17 mistShape  x escala, y velocidade, z limiar, w a rampa das luzes (0 de dia .. 1 à noite; sobem no entardecer)
// 18 mistColor  rgb cor da névoa, w força
// 19 water      x brilho de dia, y reflexo da lua à noite, z escala, w velocidade
// 20 party      xy posição do grupo na pintura (uv), z raio da luz (uv), w força do escuro em volta (0 = sem)
// 21 moon       rgb cor do reflexo na água pela luz principal da hora (sol / entardecer / lua), w força da LUT (0 = sem, 1 = inteira)
float4 param(uint i) { return vk::RawBufferLoad<float4>((uint64_t(push.paramsHi) << 32 | uint64_t(push.paramsLo)) + uint64_t(i) * 16); }

uint quality() { return (push.flags >> 1) & 3u; }
bool underworld() { return (push.flags & 1u) != 0u; }
// A presença da máscara / do revelado vem dos flags: push.mask / push.reveal são SEMPRE índices válidos (sem eles, o ruído), porque
// o compilador executa a leitura de um ramo não tomado e gTextures[kNoTexture] derruba a GPU (DEVICE_LOST, 2026-10-27).
bool hasMask() { return (push.flags & 8u) != 0u; }
bool hasReveal() { return (push.flags & 16u) != 0u; }
bool hasLut() { return (push.flags & 32u) != 0u; }
bool hasTex(uint i) { return (push.flags & (64u << i)) != 0u; }  // bits 6..14: tex0..tex8 presentes
bool partyLight() { return (push.flags & 32768u) != 0u; }
// mapa-múndi
bool hasLight() { return hasTex(0); }
bool hasHeight() { return hasTex(2); }
bool hasWater() { return hasTex(3); }
bool hasMist() { return hasTex(4); }
float4 texAt(uint t, float2 uv) { return gTextures[t].SampleLevel(gSamplers[push.samplerIndex], uv, 0); }

FullscreenOut VSMain(uint id : SV_VertexID) { return fullscreenTriangle(id); }

// --- ruído: a textura repetível (p em "células": uma volta da textura = 8 células) ------------------------------------
float4 noiseAt(float2 p) { return gTextures[push.noise].SampleLevel(gSamplers[push.repeatSampler], p / 8.0, 0); }
// fbm pela qualidade: baixa = 2 oitavas (B), média = 3 (G), alta = 5 (R).
float fbmQ(float2 p) {
  const float4 n = noiseAt(p);
  return quality() == 0u ? n.b : quality() == 1u ? n.g : n.r;
}
float fbm2(float2 p) { return noiseAt(p).b; }
float fine(float2 p) { return noiseAt(p).a; }
float hash21(float2 p) {
  float3 p3 = frac(float3(p.xyx) * 0.1031);
  p3 += dot(p3, p3.yzx + 33.33);
  return frac((p3.x + p3.y) * p3.z);
}

float3 srgbToLinear(float3 c) { return pow(max(c, 0.0), 2.2); }
float3 linearToSrgb(float3 c) { return pow(max(c, 0.0), 1.0 / 2.2); }

float4 mapAt(float2 uv) { return gTextures[push.map].SampleLevel(gSamplers[push.samplerIndex], uv, 0); }
float4 maskAt(float2 uv) { return gTextures[push.mask].SampleLevel(gSamplers[push.samplerIndex], uv, 0); }
float revealAt(float2 uv) { return gTextures[push.reveal].SampleLevel(gSamplers[push.samplerIndex], uv, 0).r; }

float luma(float3 c) { return dot(c, float3(0.299, 0.587, 0.114)); }

// Um relevo tirado da própria pintura (ela não tem altura): o gradiente da luminância, lido como uma inclinação. Baixa: nada.
float relief(float2 uv, float2 texel, float2 lightDir) {
  if (quality() == 0u) return 0.0;
  const float2 d = texel * 2.0;
  const float l = luma(mapAt(uv - float2(d.x, 0)).rgb), r = luma(mapAt(uv + float2(d.x, 0)).rgb);
  const float u = luma(mapAt(uv - float2(0, d.y)).rgb), b = luma(mapAt(uv + float2(0, d.y)).rgb);
  const float2 g = float2(r - l, b - u);
  return dot(g, -lightDir);  // a face virada para a luz clareia, a oposta escurece
}

// O acabamento de cor comum às duas telas: contraste em torno do cinza médio, saturação, o lift da noite, um ombro suave em vez do
// branco estourado, a vinheta relativa ao viewport.
float3 grade(float3 c, float2 pixel) {
  const float4 g = param(1);
  const float4 v = param(3);
  float3 lin = c;
  const float l = luma(lin);
  lin = lerp(l.xxx, lin, g.y);                  // saturação
  lin = (lin - 0.18) * g.x + 0.18;              // contraste em torno do cinza médio (linear)
  lin = max(lin, 0.0);
  lin = lerp(lin, 1.0 - exp(-lin * 1.6), v.w);  // o ombro: o branco do papel nunca estoura
  lin = lin * (1.0 - g.z) + g.z * 0.08;         // lift: a noite guarda um mínimo legível
  lin = pow(max(lin, 0.0), g.w);
  // vinheta: cantos levemente escurecidos, relativa ao viewport (não à pintura inteira, que rola)
  const float2 q = (pixel - push.view.xy) / max(push.view.zw, 1.0) * 2.0 - 1.0;
  const float dist = length(q * float2(1.0, push.view.w / max(push.view.z, 1.0)) * 0.85);
  const float vig = 1.0 - v.x * smoothstep(v.y, v.y + v.z, dist);
  return lin * vig;
}

// Os cantos arredondados da pintura (o mesmo raio da imagem da UI que isto substitui).
float coverAt(float2 pixel) {
  const float2 half = push.rect.zw * 0.5;
  const float2 d = abs(pixel - push.rect.xy - half) - half + push.rounding;
  const float sd = length(max(d, 0.0)) + min(max(d.x, d.y), 0.0) - push.rounding;
  return 1.0 - saturate(sd + 0.5);
}

// --- mapa-múndi ------------------------------------------------------------------------------------------------------------
// A LUT de uma hora: uma faixa de 32 x 32 x 32 cores (1024 x 32: x = r + 32 b, y = g), lida em sRGB, duas fatias de azul misturadas.
float3 lutAt(uint lut, float3 c) {
  c = saturate(c);
  const float b = c.b * 31.0;
  const float b0 = floor(b), b1 = min(b0 + 1.0, 31.0), f = b - b0;
  const float2 base = float2((c.r * 31.0 + 0.5) / 1024.0, (c.g * 31.0 + 0.5) / 32.0);
  const float3 s0 = gTextures[lut].SampleLevel(gSamplers[push.samplerIndex], base + float2(b0 * 32.0 / 1024.0, 0.0), 0).rgb;
  const float3 s1 = gTextures[lut].SampleLevel(gSamplers[push.samplerIndex], base + float2(b1 * 32.0 / 1024.0, 0.0), 0).rgb;
  return lerp(s0, s1, f);
}

// O relevo pelo mapa de altura (cinza: claro = alto): a inclinação pelo gradiente vira uma normal; as encostas viradas para o
// sol clareiam e as opostas escurecem, em relação ao chão plano (que não muda). Sutil: `sun.w` manda.
float heightRelief(float2 uv, float2 texel, float4 sun) {
  const float2 d = texel * 2.0;
  const float l = texAt(push.tex2, uv - float2(d.x, 0)).r;
  const float r = texAt(push.tex2, uv + float2(d.x, 0)).r;
  const float u = texAt(push.tex2, uv - float2(0, d.y)).r;
  const float b = texAt(push.tex2, uv + float2(0, d.y)).r;
  const float3 n = normalize(float3((l - r) * 6.0, (u - b) * 6.0, 1.0));
  const float flat = max(sun.z, 0.15);
  return (dot(n, sun.xyz) - sun.z) / flat * sun.w;
}

float4 overworld(float2 uv, float2 pixel, float2 texel) {
  const float4 tint = param(0);
  const float4 light = param(2);
  const float4 fogColor = param(4), fogShape = param(5), lowColor = param(6), lowShape = param(7);
  const float4 hourMix = param(13), sun = param(14), lightColor = param(15), clouds = param(16);
  const float4 mistShape = param(17), mistColor = param(18), water = param(19), party = param(20), moon = param(21);
  const float4 src = mapAt(uv);
  // 1. A cor da hora: a LUT de antes e a de depois, misturadas (em sRGB, como as LUTs foram feitas), depois a pintura em linear.
  float3 graded = src.rgb;
  if (hasLut()) {
    const float3 a = lutAt(push.lutA, src.rgb);
    const float3 b = lutAt(push.lutB, src.rgb);
    graded = lerp(src.rgb, lerp(a, b, hourMix.x), moon.w);
  }
  float3 c = srgbToLinear(graded);
  // Luz por hora do dia: a cor da luz multiplica a pintura (realça em vez de cobrir), o relevo responde à direção da luz.
  const float r = relief(uv, texel, light.xy) * light.z;
  c *= tint.rgb * tint.w * (1.0 + r);
  // 3. O sol falso: o relevo do mapa de altura, só de dia (à noite a lua não marca encosta), parado quando o sol está baixo.
  if (hasHeight() && quality() != 0u && sun.w > 0.0) c *= saturate(1.0 + clamp(heightRelief(uv, texel, sun), -0.5, 0.5) * hourMix.z);
  // 4a. Sombras de nuvem passando, de dia: o ruído grande deriva devagar e escurece de leve.
  if (clouds.x > 0.0 && hourMix.z > 0.001) {
    const float2 p = uv * clouds.y + float2(push.time * clouds.z, push.time * clouds.z * 0.23);
    const float n = fbmQ(p);
    const float shadow = smoothstep(0.52, 0.78, n) * clouds.x * hourMix.z;
    c *= 1.0 - shadow;
  }
  // 5. A água: de dia um brilho fino que anda; o reflexo tingido pela luz principal da hora (moon.rgb: o sol de dia, o
  // vermelho do entardecer, a lua pálida à noite), nos pontos altos do mesmo ruído.
  if (hasWater() && (water.x > 0.0 || water.y > 0.0)) {
    const float w = texAt(push.tex3, uv).r;
    if (w > 0.002) {
      const float2 p = uv * water.z;
      const float s1 = fine(p + float2(push.time * water.w, push.time * water.w * 0.6));
      const float s2 = fine(p * 1.7 - float2(push.time * water.w * 0.8, push.time * water.w * 0.35));
      const float glint = saturate((s1 * s2 - 0.22) * 4.0);
      c += w * glint * water.x * hourMix.z * tint.rgb * 0.25;
      c += w * pow(glint, 2.0) * water.y * moon.rgb;
    }
  }
  // 2. As luzes: acendem no entardecer (mistShape.w = a rampa das luzes, 0 de dia .. 1 à noite): a máscara nítida mais a sua
  // cópia borrada (o halo), com um tremor lento e próprio em cada ponto.
  if (hasLight() && mistShape.w > 0.001 && lightColor.w > 0.0) {
    const float3 sharp = texAt(push.tex0, uv).rgb;
    const float3 glow = texAt(push.tex1, uv).rgb;
    const float cell = hash21(floor(uv * 160.0));
    const float flicker = 1.0 - clouds.w * (0.5 + 0.5 * sin(push.time * (5.0 + cell * 4.0) + cell * 6.2831853)) * 0.35;
    c += (srgbToLinear(sharp) * 1.4 + srgbToLinear(glow) * 0.5) * lightColor.rgb * lightColor.w * mistShape.w * flicker;
  }
  // 4b. A névoa pálida da madrugada e da noite, deslizando sobre as baixadas e a água (onde a máscara permite).
  if (hasMist() && hourMix.w > 0.001 && mistColor.w > 0.0) {
    const float m = texAt(push.tex4, uv).r;
    if (m > 0.002) {
      const float2 drift = float2(push.time * mistShape.y, push.time * mistShape.y * 0.37);
      const float2 p = uv * mistShape.x + drift;
      const float2 warp = quality() == 0u ? 0.0 : (noiseAt(p * 0.5 - drift * 0.4).gb - 0.5) * 1.2;
      const float n = fbmQ(p + warp);
      const float mist = m * saturate((n - mistShape.z) * 2.4) * mistColor.w * hourMix.w;
      c = lerp(c, mistColor.rgb * tint.rgb, saturate(mist));
    }
  }
  // 6. A luz em volta do grupo à noite (Opções > Gráficos > Luz do grupo, OFF por padrão): um círculo suave de luz onde o grupo
  // está, o resto do mapa mais escuro.
  if (partyLight() && party.w > 0.0 && hourMix.y > 0.001) {
    const float dist = length((uv - party.xy) * float2(1.0, push.rect.w / max(push.rect.z, 1.0)));
    const float inside = 1.0 - smoothstep(party.z * 0.5, party.z, dist);
    const float dark = 1.0 - party.w * hourMix.y;
    c *= lerp(dark, 1.0 + 0.15 * hourMix.y, inside);
    c += inside * lightColor.rgb * 0.06 * hourMix.y;
  }
  // Névoa baixa: rasteira e fina, nas partes baixas (a máscara pintada; sem ela a heurística: o escuro frio da pintura).
  if (lowColor.w > 0.0) {
    float m;
    if (hasMask()) m = maskAt(uv).r;
    else {
      const float3 s = src.rgb;
      const float cold = saturate((max(s.g, s.b) - s.r) * 3.0);  // pântano, ravinas, entre as árvores
      m = saturate((lowShape.w - luma(s)) / max(lowShape.w, 0.01)) * (0.5 + 0.5 * cold);
    }
    m = saturate(m * lowShape.z);
    if (m > 0.002) {
      const float2 drift = float2(push.time * lowShape.y, push.time * lowShape.y * 0.37);
      const float2 p = uv * lowShape.x + drift;
      const float2 warp = quality() == 0u ? 0.0 : (noiseAt(p * 0.5 - drift * 0.4).gb - 0.5) * 1.2;
      const float n = fbmQ(p + warp);
      const float low = m * saturate((n - 0.38) * 2.4) * lowColor.w;
      const float3 lowLit = lowColor.rgb * tint.rgb * tint.w;
      c = lerp(c, lowLit, saturate(low));
    }
  }
  // Fog of war (o usuário, 2026-10-27: "FOG OF WAR NÃO É FOG LITERAL", "EU PEDI ESCURO"): o escuro opaco que esconde o
  // inexplorado, e só isso: sem ruído de nuvem, sem vento, sem sombreado, sem raios. A borda é a de hoje (o revelado dos
  // círculos), apenas levemente irregular e parada (fogShape.z = a largura da transição; 0 = a borda de hoje tal qual).
  // Onde o revelado é 1 não há escuro nenhum: a área revelada é exatamente a mesma.
  if (hasReveal() && fogColor.w > 0.0) {
    const float keep = 1.0 - revealAt(uv);
    if (keep > 0.001) {
      const float n = noiseAt(uv * fogShape.x).g;  // parado: só desenha a borda
      const float edge = fogShape.z > 0.0 ? smoothstep(0.5 - fogShape.z, 0.5 + fogShape.z, keep + (n - 0.5) * 0.6) : keep;
      const float fog = keep * edge * fogColor.w;
      c = lerp(c, fogColor.rgb, saturate(fog));
    }
  }
  return float4(grade(c, pixel), src.a);
}

// --- Submundo ---------------------------------------------------------------------------------------------------------------
float glowOf(float3 s) {
  const float sat = (max(s.r, max(s.g, s.b)) - min(s.r, min(s.g, s.b)));
  const float cold = saturate((max(s.g, s.b) - s.r) * 4.0);
  return saturate((sat - 0.08) * 5.0) * cold * saturate((luma(s) - 0.5) / 0.3);
}

float4 underworldAt(float2 uv, float2 pixel, float2 texel) {
  const float4 glowColor = param(8), dark = param(9), caveColor = param(10), caveShape = param(11), extra0 = param(12);
  const float4 src = mapAt(uv);
  const float3 base = srgbToLinear(src.rgb);
  // Onde há seres que brilham: a máscara pintada (G), senão a heurística (o claro e frio da pintura: fungos azul-esverdeados).
  float g, caveM;
  if (hasMask()) {
    const float4 m = maskAt(uv);
    g = m.g, caveM = m.r;
  } else {
    g = glowOf(src.rgb);
    const float3 s = src.rgb;
    const float blueish = saturate((s.b - s.r * 1.05 + 0.02) * 12.0);
    caveM = saturate((caveShape.w - luma(s)) / max(caveShape.w, 0.01)) * blueish;
  }
  // O halo da luz dos seres: a luz sai de onde eles estão e se espalha pelo entorno (amostras em anel).
  float halo = g;
  const int taps = quality() == 0u ? 4 : quality() == 1u ? 6 : 8;
  [loop] for (int i = 0; i < taps; ++i) {
    const float a = (float(i) + 0.5) / float(taps) * 6.2831853;
    const float2 o = float2(cos(a), sin(a)) * glowColor.w * (0.6 + 0.4 * float(i & 1));
    // if / else, never a ternário: ele avalia os dois lados e lê gTextures[kNoTexture] (DEVICE_LOST, 2026-10-27)
    if (hasMask()) halo += maskAt(uv + o).g;
    else halo += glowOf(mapAt(uv + o).rgb);
  }
  halo /= float(taps + 1);
  // O brilho respira no auge (cada ser no seu ritmo), e cai ao nada quando dormem.
  const float breathe = 0.85 + 0.15 * sin(push.time * 0.9 + hash21(floor(uv * 40.0)) * 6.28);
  const float L = push.glow * breathe;
  // Sem sol, sem lua: a pintura no escuro (dark.w) com a cor ambiente; no auge os seres clareiam a caverna inteira um pouco
  // (extra0.x) e muito onde estão e no halo em volta.
  float3 c = base * (dark.rgb * (dark.w + extra0.x * L));
  c += base * g * L * 2.5 + glowColor.rgb * halo * L * 1.2;
  c += base * halo * L * glowColor.rgb * 2.0;
  // Névoa de caverna: úmida, lenta, entre as galerias, sobre a água e nos abismos; apagada quando os seres dormem, iluminada
  // com a cor deles no auge.
  if (caveColor.w > 0.0 && caveM > 0.002) {
    const float2 drift = float2(push.time * caveShape.y, -push.time * caveShape.y * 0.6);
    const float2 p = uv * caveShape.x + drift;
    const float2 warp = quality() == 0u ? 0.0 : (noiseAt(p * 0.5 + drift * 0.3).gb - 0.5) * 1.2;
    const float n = fbmQ(p + warp);
    const float cave = caveM * saturate((n - 0.32) * 2.2) * caveColor.w;
    const float3 caveLit = caveColor.rgb * (dark.w + 0.1 + extra0.x * L) + glowColor.rgb * halo * L * caveShape.z;
    c = lerp(c, caveLit, saturate(cave));
  }
  return float4(grade(c, pixel), src.a);
}

// --- Submundo, o look 2D no ciclo invertido (2026-10-28) ------------------------------------------------------------------
// Os parâmetros (13..21, significado próprio desta tela; WorldMapFx::underworldLookParams):
//  13 hourMix   x mistura entre lutA e lutB, y L = floração (0 dormente de dia .. 1 auge na noite da superfície), z 1 - L, w (livre)
//  14 fungal    rgb cor do brilho fúngico (teal), w força
//  15 plant     rgb cor das plantas que se alimentam (violeta apagado), w força
//  16 windows   rgb cor das janelas das casas nos cogumelos (quente), w força
//  17 ember     rgb cor do ember dos Ferreiros Cinzentos, w força (constante, dia e noite)
//  18 emberS    x pulsação (0..1), y velocidade da pulsação, z amplitude do tremor de calor (texels), w velocidade do tremor
//  19 water     x brilho, y reflexo, z escala, w velocidade
//  20 waterTint rgb cor do reflexo na floração (teal / violeta), w respiração dos cogumelos de dia (0..1)
//  21 spores    x escala (células sobre a pintura), y velocidade, z tamanho, w força; a mesma grade serve às criaturas
//  22 creatures rgb cor das mariposas / vagalumes, w força (0 = sem)
float sporePoint(float2 p, float seed, float size) {
  // uma partícula por célula da grade: um ponto suave que vagueia dentro da célula (as 4 células à volta do ponto: a
  // partícula fica a menos de meia célula do seu centro, então estas bastam)
  const float2 cell = floor(p - 0.5);
  float v = 0.0;
  [unroll] for (int j = 0; j <= 1; ++j)
    [unroll] for (int i = 0; i <= 1; ++i) {
      const float2 c = cell + float2(i, j);
      const float h = hash21(c + seed);
      // o vagueio pela textura de ruído (uma leitura por célula, sem trigonometria por pixel)
      const float2 wander = (noiseAt(c * 0.37 + seed + push.time * 0.03).gb - 0.5) * 0.8;
      const float2 pos = c + 0.5 + wander;
      const float d = length(p - pos);
      v += (1.0 - smoothstep(0.0, size, d)) * (0.6 + 0.4 * h);
    }
  return v;
}

float4 underworldLook(float2 uv, float2 pixel, float2 texel) {
  const float4 hourMix = param(13), fungalC = param(14), plantC = param(15), winC = param(16), emberC = param(17), emberS = param(18);
  const float4 water = param(19), waterTint = param(20), spores = param(21), creatures = param(22);
  const float L = hourMix.y;
  // Tremor de calor sobre as forjas e respiradouros: a pintura lida com um pequeno desvio ondulante, só dentro da máscara.
  float2 suv = uv;
  if (hasTex(8) && emberS.z > 0.0) {
    const float h = texAt(push.tex8, uv).r;
    if (h > 0.002) {
      const float2 n = noiseAt(uv * 90.0 + float2(0.0, -push.time * emberS.w)).ga - 0.5;
      suv += n * emberS.z * h * texel * 4.0;
    }
  }
  const float4 src = mapAt(suv);
  float3 graded = src.rgb;
  if (hasLut()) graded = lerp(lutAt(push.lutA, src.rgb), lutAt(push.lutB, src.rgb), hourMix.x);
  float3 c = srgbToLinear(graded);
  const float cell = hash21(floor(uv * 160.0));
  // O brilho fúngico: R = idade / força (claro = mais velho, acende antes). A onda: um ponto acende quando L passa de 1 - idade
  // (os gigantes primeiro, os pequenos depois) e apaga na ordem inversa ao amanhecer. O halo borrado ilumina as paredes.
  float haloLit = 0.0;
  if (hasTex(0) && fungalC.w > 0.0) {
    const float age = texAt(push.tex0, uv).r;
    const float hAge = texAt(push.tex1, uv).r;
    const float lit = smoothstep(1.0 - age - 0.18, 1.0 - age + 0.18, L);
    haloLit = smoothstep(1.0 - hAge - 0.25, 1.0 - hAge + 0.25, L) * hAge;
    const float breathe = (1.0 - L) * waterTint.w * (0.5 + 0.5 * sin(push.time * 0.45 + cell * 6.28));
    c += fungalC.rgb * fungalC.w * (age * lit * 1.2 + haloLit * 0.6 + age * breathe * 0.25);
  }
  // As plantas que se alimentam do brilho: acendem só depois dos cogumelos perto delas, mais fracas e violeta.
  if (hasTex(2) && plantC.w > 0.0) {
    const float pl = texAt(push.tex2, uv).r;
    const float feed = smoothstep(0.30, 0.70, haloLit);
    c += plantC.rgb * plantC.w * pl * feed;
  }
  // As janelas das casas dentro dos cogumelos: quentes contra a luz fria, com a floração, tremor leve.
  if (hasTex(5) && winC.w > 0.0) {
    const float3 w = srgbToLinear(texAt(push.tex5, uv).rgb);
    const float flicker = 1.0 - 0.12 * (0.5 + 0.5 * sin(push.time * (4.0 + cell * 3.0) + cell * 6.28));
    c += w * winC.rgb * winC.w * L * flicker;
  }
  // O ember dos Ferreiros Cinzentos: constante, R = força (mais fundo, mais quente), pulsação lenta e pesada de brasa.
  float emberHalo = 0.0;
  if (hasTex(6) && emberC.w > 0.0) {
    const float e = texAt(push.tex6, uv).r;
    emberHalo = texAt(push.tex7, uv).r;
    const float throb = 1.0 - emberS.x * (0.5 + 0.5 * sin(push.time * emberS.y + cell * 1.5));
    c += emberC.rgb * emberC.w * throb * (e * 1.3 + emberHalo * 0.6);
  }
  // A água (o Mar de Vidro): o brilho fino; o reflexo teal / violeta na floração, vermelho onde o ember alcança.
  if (hasTex(3) && (water.x > 0.0 || water.y > 0.0)) {
    const float w = texAt(push.tex3, uv).r;
    if (w > 0.002) {
      const float2 p = uv * water.z;
      const float s1 = fine(p + float2(push.time * water.w, push.time * water.w * 0.6));
      const float s2 = fine(p * 1.7 - float2(push.time * water.w * 0.8, push.time * water.w * 0.35));
      const float glint = saturate((s1 * s2 - 0.22) * 4.0);
      const float3 refl = waterTint.rgb * L + emberC.rgb * emberHalo;
      c += w * glint * water.x * 0.15 * (0.3 + L);
      c += w * pow(glint, 2.0) * water.y * refl;
    }
  }
  // Esporos à deriva sobre as Marchas (a máscara), na floração: partículas suaves, uma por célula, subindo devagar.
  if (hasTex(4) && spores.w > 0.0 && L > 0.001) {
    const float m = texAt(push.tex4, uv).r;
    if (m > 0.002) {
      const float2 p = uv * spores.x + float2(push.time * spores.y * 0.3, -push.time * spores.y);
      const float s = sporePoint(p, 3.0, spores.z);
      c += fungalC.rgb * spores.w * m * s * L;
    }
  }
  // Mariposas e vagalumes: juntam-se nos pontos mais brilhantes à noite (o halo fúngico), dispersam-se ao amanhecer (menos e
  // mais longe dos focos quando L cai).
  if (hasTex(0) && creatures.w > 0.0 && L > 0.001) {
    const float gather = smoothstep(0.15, 0.6, haloLit);
    const float scatter = 1.0 - L;
    const float density = lerp(gather, texAt(push.tex1, uv).r * 0.3, scatter);
    if (density > 0.002) {
      const float2 p = uv * spores.x * 1.6 + float2(sin(push.time * 0.2), cos(push.time * 0.17)) * 0.5;
      const float s = sporePoint(p, 7.0, spores.z * 0.7);
      const float blink = 0.6 + 0.4 * sin(push.time * 3.0 + cell * 6.28);
      c += creatures.rgb * creatures.w * density * s * blink * L;
    }
  }
  return float4(grade(c, pixel), src.a);
}

float4 PSMain(FullscreenOut i) : SV_Target0 {
  const float2 pixel = i.position.xy;
  const float cover = coverAt(pixel);
  if (cover <= 0.0) discard;
  const float2 uv = (pixel - push.rect.xy) / push.rect.zw;
  uint w, h;
  gTextures[push.map].GetDimensions(w, h);
  const float2 texel = 1.0 / float2(max(w, 1u), max(h, 1u));
  // O Submundo com LUTs = o look 2D novo (ciclo invertido); sem elas, o passe antigo (heurística / máscara R-G).
  float4 c;
  if (underworld()) {
    if (hasLut()) c = underworldLook(uv, pixel, texel);
    else c = underworldAt(uv, pixel, texel);
  } else c = overworld(uv, pixel, texel);
  const float a = cover * c.a;
  return float4(linearToSrgb(c.rgb) * a, a);  // pré-multiplicado, sobre a janela
}
