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
float4 param(uint i) { return vk::RawBufferLoad<float4>((uint64_t(push.paramsHi) << 32 | uint64_t(push.paramsLo)) + uint64_t(i) * 16); }

uint quality() { return (push.flags >> 1) & 3u; }
bool underworld() { return (push.flags & 1u) != 0u; }
// A presença da máscara / do revelado vem dos flags: push.mask / push.reveal são SEMPRE índices válidos (sem eles, o ruído), porque
// o compilador executa a leitura de um ramo não tomado e gTextures[kNoTexture] derruba a GPU (DEVICE_LOST, 2026-10-27).
bool hasMask() { return (push.flags & 8u) != 0u; }
bool hasReveal() { return (push.flags & 16u) != 0u; }

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
float4 overworld(float2 uv, float2 pixel, float2 texel) {
  const float4 tint = param(0);
  const float4 light = param(2);
  const float4 fogColor = param(4), fogShape = param(5), lowColor = param(6), lowShape = param(7);
  const float4 src = mapAt(uv);
  float3 c = srgbToLinear(src.rgb);
  // Luz por hora do dia: a cor da luz multiplica a pintura (realça em vez de cobrir), o relevo responde à direção da luz.
  const float r = relief(uv, texel, light.xy) * light.z;
  c *= tint.rgb * tint.w * (1.0 + r);
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

float4 PSMain(FullscreenOut i) : SV_Target0 {
  const float2 pixel = i.position.xy;
  const float cover = coverAt(pixel);
  if (cover <= 0.0) discard;
  const float2 uv = (pixel - push.rect.xy) / push.rect.zw;
  uint w, h;
  gTextures[push.map].GetDimensions(w, h);
  const float2 texel = 1.0 / float2(max(w, 1u), max(h, 1u));
  const float4 c = underworld() ? underworldAt(uv, pixel, texel) : overworld(uv, pixel, texel);
  const float a = cover * c.a;
  return float4(linearToSrgb(c.rgb) * a, a);  // pré-multiplicado, sobre a janela
}
