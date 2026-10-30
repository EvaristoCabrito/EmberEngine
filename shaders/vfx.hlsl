// Ember's spell effects (its three.js VFX classes, ported in render/vfx/*): every draw is a mesh the CPU built
// (render/VfxDraw.h, VfxGeometry.h) with one of Ember's materials, its GLSL translated line for line below. Drawn
// over the finished scene in the effects pass, never writing depth. Each material computes what its GLSL writes to
// gl_FragColor (c = rgb, a = alpha); three's blending is then reproduced in one premultiplied pipeline (ONE,
// ONE_MINUS_SRC_ALPHA): NormalBlending (src x srcAlpha + dst x (1 - srcAlpha)) as (c x a, a), AdditiveBlending
// (src x srcAlpha + dst) as (c x a, 0). Brightness is display-relative like the flipbook effects (Ember's materials
// are toneMapped: false); the CPU's `brightness` is the light mode's glow rule.
#include "lighting.hlsli"

// Matches ember::VfxVertex (32 bytes), ember::VfxInstance (112 bytes) and ember::VfxDrawGpu (160 bytes).
struct VfxVertex {
  float3 position;
  float u;
  float3 normal;
  float v;
};
struct VfxDraw {
  float4x4 model;
  float4 params0;
  float4 params1;
  float4 color;
  uint firstVertex;
  uint vertexCount;
  uint firstInstance;
  uint material;
  float3 sightAt;
  uint flags;  // 1 additive
  uint texture;
  uint3 pad;
};
struct VfxInstance {
  float4x4 matrix;
  float4 a;
  float4 b;
  float4 c;
};

static const uint kMaterialBasic = 0, kMaterialMissileCore = 1, kMaterialMissileShell = 2, kMaterialMissileTrail = 3,
                  kMaterialStandard = 4, kMaterialFlame = 5, kMaterialImpactParticle = 6, kMaterialImpactFlash = 7,
                  kMaterialBlessWave = 8, kMaterialSpriteSheet = 9, kMaterialVenomSmoke = 10, kMaterialPfTendril = 11, kMaterialPfCore = 12,
                  kMaterialPfShell = 13, kMaterialWebThread = 14, kMaterialFlipbookParticle = 15, kMaterialHoly = 16,
                  kMaterialCombat = 17, kMaterialIceStorm = 18, kMaterialTextured = 19;

struct Push {
  uint64_t vertices;   // VfxVertex[]
  uint64_t instances;  // VfxInstance[]
  uint64_t draws;      // VfxDraw[]
  uint64_t frame;      // MeshFrame
  uint drawIndex;
  float brightness;
  uint samplerIndex;
  uint pad;
};
[[vk::push_constant]] Push push;

VfxDraw loadDraw() { return vk::RawBufferLoad<VfxDraw>(push.draws + uint64_t(push.drawIndex) * 160); }
VfxInstance loadInstance(uint i) { return vk::RawBufferLoad<VfxInstance>(push.instances + uint64_t(i) * 112); }

// OldFireBall NOISE_GLSL (Ashima's 3D simplex noise + a 4-octave fbm), line for line.
float3 mod289(float3 x) { return x - floor(x * (1.0 / 289.0)) * 289.0; }
float4 mod289(float4 x) { return x - floor(x * (1.0 / 289.0)) * 289.0; }
float4 permute(float4 x) { return mod289(((x * 34.0) + 1.0) * x); }
float4 taylorInvSqrt(float4 r) { return 1.79284291400159 - 0.85373472095314 * r; }
float snoise(float3 v) {
  const float2 C = float2(1.0 / 6.0, 1.0 / 3.0);
  const float4 D = float4(0.0, 0.5, 1.0, 2.0);
  float3 i = floor(v + dot(v, C.yyy));
  const float3 x0 = v - i + dot(i, C.xxx);
  const float3 g = step(x0.yzx, x0.xyz);
  const float3 l = 1.0 - g;
  const float3 i1 = min(g.xyz, l.zxy);
  const float3 i2 = max(g.xyz, l.zxy);
  const float3 x1 = x0 - i1 + C.xxx;
  const float3 x2 = x0 - i2 + C.yyy;
  const float3 x3 = x0 - D.yyy;
  i = mod289(i);
  const float4 p = permute(permute(permute(i.z + float4(0.0, i1.z, i2.z, 1.0)) + i.y + float4(0.0, i1.y, i2.y, 1.0)) + i.x + float4(0.0, i1.x, i2.x, 1.0));
  const float n_ = 0.142857142857;
  const float3 ns = n_ * D.wyz - D.xzx;
  const float4 j = p - 49.0 * floor(p * ns.z * ns.z);
  const float4 x_ = floor(j * ns.z);
  const float4 y_ = floor(j - 7.0 * x_);
  const float4 x = x_ * ns.x + ns.yyyy;
  const float4 y = y_ * ns.x + ns.yyyy;
  const float4 h = 1.0 - abs(x) - abs(y);
  const float4 b0 = float4(x.xy, y.xy);
  const float4 b1 = float4(x.zw, y.zw);
  const float4 s0 = floor(b0) * 2.0 + 1.0;
  const float4 s1 = floor(b1) * 2.0 + 1.0;
  const float4 sh = -step(h, 0.0);
  const float4 a0 = b0.xzyw + s0.xzyw * sh.xxyy;
  const float4 a1 = b1.xzyw + s1.xzyw * sh.zzww;
  float3 p0 = float3(a0.xy, h.x);
  float3 p1 = float3(a0.zw, h.y);
  float3 p2 = float3(a1.xy, h.z);
  float3 p3 = float3(a1.zw, h.w);
  const float4 norm = taylorInvSqrt(float4(dot(p0, p0), dot(p1, p1), dot(p2, p2), dot(p3, p3)));
  p0 *= norm.x;
  p1 *= norm.y;
  p2 *= norm.z;
  p3 *= norm.w;
  float4 m = max(0.6 - float4(dot(x0, x0), dot(x1, x1), dot(x2, x2), dot(x3, x3)), 0.0);
  m = m * m;
  return 42.0 * dot(m * m, float4(dot(p0, x0), dot(p1, x1), dot(p2, x2), dot(p3, x3)));
}
float fbm(float3 p) {
  float f = 0.0;
  float a = 0.5;
  for (int i = 0; i < 4; i++) {
    f += a * snoise(p);
    p *= 2.03;
    a *= 0.5;
  }
  return f;
}
// FireballImpactVFX's hash and value noise.
float hash2(float2 p) { return frac(sin(dot(p, float2(127.1, 311.7))) * 43758.5453); }
float noise2(float2 p) {
  const float2 i = floor(p);
  float2 f = frac(p);
  f = f * f * (3.0 - 2.0 * f);
  return lerp(lerp(hash2(i), hash2(i + float2(1, 0)), f.x), lerp(hash2(i + float2(0, 1)), hash2(i + float2(1, 1)), f.x), f.y);
}

// A 4x4 fx sheet's cell (Ember: frame 0 top left, uv v up) inside its cooked clip (VfxFlipbook): params1 = the cell's
// size and the crop in pixels, color.xy = 1 / the texture's size.
float2 cellToTexture(VfxDraw d, float2 uv) { return (float2(uv.x, 1.0 - uv.y) * d.params1.xy - d.params1.zw) * d.color.xy; }

// Blizzard2D's sheets, cooked whole as one picture: Ember's uv (v down) over the whole sheet.
float4 iceSheet(VfxDraw d, float2 e2uv) {
  const float2 tex = saturate(cellToTexture(d, float2(e2uv.x, 1.0 - e2uv.y)));
  return gTextureArrays[NonUniformResourceIndex(d.texture)].SampleLevel(gSamplers[push.samplerIndex], float3(tex, 0.0), 0.0);
}

// CausticVenomVFX VENOM_SMOKE_FRAGMENT's noise.
float hash31(float3 p) {
  p = frac(p * 0.1031);
  p += dot(p, p.yzx + 33.33);
  return frac((p.x + p.y) * p.z);
}
float noise3(float3 p) {
  const float3 i = floor(p);
  float3 f = frac(p);
  f = f * f * (3.0 - 2.0 * f);
  return lerp(lerp(lerp(hash31(i), hash31(i + float3(1.0, 0.0, 0.0)), f.x), lerp(hash31(i + float3(0.0, 1.0, 0.0)), hash31(i + float3(1.0, 1.0, 0.0)), f.x), f.y),
              lerp(lerp(hash31(i + float3(0.0, 0.0, 1.0)), hash31(i + float3(1.0, 0.0, 1.0)), f.x), lerp(hash31(i + float3(0.0, 1.0, 1.0)), hash31(i + float3(1.0, 1.0, 1.0)), f.x), f.y),
              f.z);
}
float smokeField(float3 p, float t) {
  float n = noise3(p * 2.2) * 0.55;
  n += noise3(p * 4.6 + float3(0.0, 0.0, t * 0.2)) * 0.3;
  n += noise3(p * 8.0 - float3(t * 0.25, 0.0, 0.0)) * 0.15;
  return n;
}

struct VSOut {
  float4 position : SV_Position;
  float3 local : TEXCOORD0;  // GLSL vP (the displaced, scaled object-space position)
  float2 uv : TEXCOORD1;
  float3 world : TEXCOORD2;
  float3 normal : TEXCOORD3;
  nointerpolation float sight : TEXCOORD4;
  float heat : TEXCOORD5;                        // Flame: vHeat
  nointerpolation float4 particleB : TEXCOORD6;  // ImpactParticle: rotation, frame, alpha, intensity
  nointerpolation float4 particleC : TEXCOORD7;  // ImpactParticle: tint, kind
};

VSOut VSMain(uint vertex : SV_VertexID, uint instance : SV_InstanceID) {
  const VfxDraw d = loadDraw();
  const MeshFrame f = loadFrame(push.frame);
  const VfxVertex v = vk::RawBufferLoad<VfxVertex>(push.vertices + uint64_t(d.firstVertex + vertex) * 32);
  const Sight sight = tacticalSight(f, d.sightAt);
  VSOut o;
  o.sight = sight.local;
  o.heat = 0.0;
  o.particleB = 0.0;
  o.particleC = 0.0;
  o.uv = float2(v.u, v.v);
  float3 p = v.position;
  if (d.material == kMaterialSpriteSheet) {
    // three's Sprite: a screen-aligned quad around the sprite's centre point, shifted by `center` (0.5, 0 = standing on
    // its point), sized and turned in view space. a.xyz = the point, b = rotation, frame, opacity, colour scale,
    // c = width, height, center y.
    const VfxInstance inst = loadInstance(d.firstInstance + instance);
    const float2 aligned = (v.position.xy - (float2(0.5, inst.c.z) - 0.5)) * inst.c.xy;
    const float cs = cos(inst.b.x), sn = sin(inst.b.x);
    const float2 q = float2(cs * aligned.x - sn * aligned.y, sn * aligned.x + cs * aligned.y);
    const float3 right = float3(f.view[0].x, f.view[1].x, f.view[2].x), up = float3(f.view[0].y, f.view[1].y, f.view[2].y);
    const float3 w = mul(float4(inst.a.xyz, 1.0), d.model).xyz + right * q.x + up * q.y;
    o.position = mul(float4(w, 1.0), f.viewProjection);
    o.local = v.position;
    o.world = w;
    o.normal = float3(0, 1, 0);
    o.particleB = inst.b;
    if (sight.local <= 0.0) o.position = float4(0, 0, -1, 1);
    return o;
  }
  if (d.material == kMaterialImpactParticle || d.material == kMaterialFlipbookParticle || (d.material == kMaterialHoly && d.params0.y > 1.5 && d.params0.y < 2.5)) {
    // A camera-facing quad (PlaneGeometry 1x1) around the particle's centre, turned and sized in view space.
    const VfxInstance inst = loadInstance(d.firstInstance + instance);
    const float cs = cos(inst.b.x), sn = sin(inst.b.x);
    const float2 q = float2(v.position.x * cs - v.position.y * sn, v.position.x * sn + v.position.y * cs) * inst.a.w;
    const float3 right = float3(f.view[0].x, f.view[1].x, f.view[2].x), up = float3(f.view[0].y, f.view[1].y, f.view[2].y);
    const float3 w = mul(float4(inst.a.xyz, 1.0), d.model).xyz + right * q.x + up * q.y;
    o.position = mul(float4(w, 1.0), f.viewProjection);
    o.local = v.position;
    o.world = w;
    o.normal = float3(0, 1, 0);
    o.particleB = inst.b;
    o.particleC = inst.c;
    if (sight.local <= 0.0) o.position = float4(0, 0, -1, 1);
    return o;
  }
  if (d.material == kMaterialIceStorm && ((d.params0.y > 0.5 && d.params0.y < 1.5) || (d.params0.y > 4.5 && d.params0.y < 6.5))) {
    // render/vfx/IceStorm's billboards (snow, ice dust, Blizzard2D's sprites): a camera-facing quad around a.xyz, c.xy wide
    // and tall, turned by b.x. Never thinner than 1.5 pixels (a snowflake is smaller than one); what is widened is dimmed by
    // as much, so it carries the same light.
    const VfxInstance inst = loadInstance(d.firstInstance + instance);
    const float3 right = float3(f.view[0].x, f.view[1].x, f.view[2].x), up = float3(f.view[0].y, f.view[1].y, f.view[2].y);
    const float3 centre = mul(float4(inst.a.xyz, 1.0), d.model).xyz;
    const float pixel = 2.0 * length(centre - f.cameraPosition) / max(f.projectionY * f.screenSize.y, 1.0);
    const float2 size = max(inst.c.xy, pixel * 1.5);
    const float kept = (inst.c.x * inst.c.y) / max(size.x * size.y, 1e-8);
    const float cs = cos(inst.b.x), sn = sin(inst.b.x);
    const float2 l = v.position.xy * size;
    const float3 w = centre + right * (cs * l.x - sn * l.y) + up * (sn * l.x + cs * l.y);
    o.position = mul(float4(w, 1.0), f.viewProjection);
    o.local = v.position;
    o.world = w;
    o.normal = float3(0, 1, 0);
    o.particleB = float4(inst.b.x, inst.b.y, inst.b.z * kept, inst.b.w);
    o.particleC = inst.c;
    if (sight.local <= 0.0) o.position = float4(0, 0, -1, 1);
    return o;
  }
  if (d.material == kMaterialCombat && d.params0.y < 1.5) {
    // Ours (render/vfx/CombatFx): a streak or a dust puff, one instance each, around its centre a.xyz.
    //   streak (mode 0): stretched along b.xyz (its motion, tail -> head) and a.w wide, turned about that axis to face the
    //                    eye; shorter than its width = a round dot of a.w; b.w = alpha, c = tint + intensity;
    //   puff (mode 1):   a camera-facing quad of a.w, turned by b.x; b.y = its noise seed, b.z = alpha; c = its colour.
    const VfxInstance inst = loadInstance(d.firstInstance + instance);
    const float3 right = float3(f.view[0].x, f.view[1].x, f.view[2].x), up = float3(f.view[0].y, f.view[1].y, f.view[2].y);
    const float3 centre = mul(float4(inst.a.xyz, 1.0), d.model).xyz;
    float3 w;
    if (d.params0.y < 0.5) {
      // Never thinner than kMinPixels on screen (a thinner streak shimmers as loose pixels); what is widened is dimmed
      // by as much, so it carries the same light.
      static const float kMinPixels = 3.0;
      const float pixel = 2.0 * length(centre - f.cameraPosition) / max(f.projectionY * f.screenSize.y, 1.0);  // world size of a pixel there
      const float width = max(inst.a.w, pixel * kMinPixels);
      const float3 axis = inst.b.xyz;
      const float len = length(axis);
      float dotShape = 0.0;
      if (len < width * 0.75) {
        w = centre + (right * v.position.x + up * v.position.y) * width;
        dotShape = 1.0;
      } else {
        float3 across = cross(axis / len, normalize(f.cameraPosition - centre));
        across = dot(across, across) > 1e-8 ? normalize(across) : right;
        w = centre + axis * v.position.y + across * (v.position.x * width);
      }
      const float kept = inst.a.w / width;
      o.particleB = float4(inst.b.w * (dotShape > 0.5 ? kept * kept : kept), dotShape, 0.0, 0.0);
    } else {
      const float cs = cos(inst.b.x), sn = sin(inst.b.x);
      const float2 q = float2(v.position.x * cs - v.position.y * sn, v.position.x * sn + v.position.y * cs) * inst.a.w;
      w = centre + right * q.x + up * q.y;
      o.particleB = inst.b;
    }
    o.position = mul(float4(w, 1.0), f.viewProjection);
    o.local = v.position;
    o.world = w;
    o.normal = float3(0, 1, 0);
    o.particleC = inst.c;
    if (sight.local <= 0.0) o.position = float4(0, 0, -1, 1);
    return o;
  }
  if (d.material == kMaterialCombat && d.params0.y > 1.5 && d.params0.y < 2.5) {
    // An energy sphere: its surface boils (color.a = how far); heat = the noise.
    const float n = snoise(p * 3.0 + float3(0.0, d.params0.x * 2.2, d.params0.x * 1.3));
    p += v.normal * n * d.color.a;
    o.heat = n;
  }
  if (d.material == kMaterialFlame) {
    // flameMaterial's vertex: the boiling surface (the noise scrolls up +z and churns), pushed along the normal.
    const float t = d.params0.x, amp = d.params0.y;
    const float3 q = v.position * 1.6 + float3(0.0, 0.0, -t * 1.4);
    const float n = fbm(q + float3(t * 0.35, t * 0.2, 0.0));
    o.heat = n;
    p = v.position + v.normal * n * amp;
  }
  if (d.material == kMaterialBlessWave) {
    // BlessVFX WAVE_VERTEX: the ring's radius ripples with its angle, its height waves; vEnergy rides in `heat`.
    const float t = d.params0.x, noise = d.params0.y, height = d.params0.z, radius = d.params0.w;
    const float a = atan2(v.position.y, v.position.x);
    const float n = sin(a * 11.0 + t * 7.0) * 0.48 + sin(a * 23.0 - t * 4.0) * 0.22 + cos(a * 37.0 + t * 3.0) * 0.12;
    p.xy *= radius + n * noise;
    p.z += sin(a * 9.0 - t * 6.0) * noise * 0.16 + sin(a * 7.0 + t * 3.0) * height * 0.12;
    o.heat = 0.52 + 0.48 * sin(a * 13.0 - t * 8.0 + n * 4.0);
  }
  if (d.material == kMaterialPfTendril) {
    // TENDRIL_VERTEX (aAlong = uv.x): waves along the strand, all of it drawn in as it collapses. vNoise rides in heat.
    const float t = d.params0.x, collapse = d.params0.y, turbulence = d.params0.z, splineNoise = d.params0.w;
    const float along = v.u;
    const float wave = sin(t * 3.4 + along * 18.0 + v.position.z * 4.0);
    const float wave2 = cos(t * 2.1 - along * 13.0 + v.position.x * 5.0);
    o.heat = wave * 0.5 + wave2 * 0.5;
    p.xy += float2(wave, wave2) * turbulence * (0.25 + along);
    p.xz += float2(wave2, wave) * splineNoise * sin(along * 3.14159);
    p *= lerp(1.0, 0.07, collapse);
  }
  float webPulse = 0.0, webVisible = 0.0;
  if (d.material == kMaterialWebThread) {
    // THREAD_VERTEX: a thread grows in from its delay, waves, tightens, carries a travelling pulse. aAlong = u;
    // aDelay / aPhase unpacked from v.
    const float t = d.params0.x, pulseSpeed = d.params0.y, formation = d.params0.z, tightening = d.params0.w, displacement = d.params1.x;
    const float along = v.u;
    const float k = floor(v.v / 2.0);
    const float delay = v.v - 2.0 * k, phase = k / 1000.0 * 6.2831853;
    webVisible = smoothstep(delay, delay + 0.18, formation - along * 0.2);
    const float wave = sin(t * 2.7 + along * 17.0 + phase) * displacement;
    const float wave2 = cos(t * 1.9 - along * 12.0 + phase * 1.7) * displacement;
    p.x += wave * (0.25 + along);
    p.y += wave2 * (0.35 + along);
    p.z += sin(t * 2.1 + along * 9.0 + phase) * displacement * 1.6;
    p.xy *= lerp(1.0, 1.0 - tightening, smoothstep(0.68, 0.92, formation));
    const float pulse = frac(t * pulseSpeed * 0.27 + along + phase);
    webPulse = smoothstep(0.0, 0.07, pulse) * (1.0 - smoothstep(0.11, 0.19, pulse));
  }
  if (d.material == kMaterialPfCore || d.material == kMaterialPfShell) {
    // CORE_VERTEX / SHELL_VERTEX: the surface pushed along its normal by a moving interference; vWarp rides in heat.
    const float t = d.params0.x, distortion = d.params0.y;
    const float n = d.material == kMaterialPfCore ? sin(p.x * 18.0 + t * 9.0) * cos(p.y * 15.0 - t * 7.0) * sin(p.z * 17.0 + t * 5.0)
                                                  : sin(p.x * 9.0 + t * 13.0) * cos(p.y * 8.0 - t * 9.0) * sin(p.z * 10.0 + t * 7.0);
    p += v.normal * n * distortion;
    o.heat = n;
  }
  if (d.material == kMaterialMissileCore || d.material == kMaterialMissileShell) {
    // BOLT_VERTEX
    const float t = d.params0.x, distort = d.params0.y, scale = d.params0.z;
    const float n = sin(v.position.x * 14.0 + t * 8.0) * cos(v.position.y * 17.0 - t * 6.0) + sin(v.position.z * 23.0 + t * 4.0) * 0.35;
    p += normalize(v.position + 0.001) * n * distort;
    p.x *= 0.55;
    p *= scale;
  }
  // -Zpr: mul(v, M) is glm's M * v, so world = model * instance * p.
  float4 world = float4(p, 1.0);
  float4 normal = float4(v.normal, 0.0);
  if (d.firstInstance != 0xFFFFFFFFu) {
    const VfxInstance inst = loadInstance(d.firstInstance + instance);
    world = mul(world, inst.matrix);
    normal = mul(normal, inst.matrix);
    o.particleC = inst.c;  // instanceColor
  }
  world = mul(world, d.model);
  normal = mul(normal, d.model);
  o.position = mul(world, f.viewProjection);
  o.local = d.material == kMaterialWebThread ? float3(webVisible, webPulse, 0.0) : p;  // THREAD: vVisible, vPulse
  o.world = world.xyz;
  o.normal = normalize(normal.xyz + 1e-6);
  if (sight.local <= 0.0) o.position = float4(0, 0, -1, 1);  // out of sight: nothing (an effect is light)
  return o;
}

float4 PSMain(VSOut i) : SV_Target0 {
  const VfxDraw d = loadDraw();
  float3 c = 0.0;
  float a = 1.0;
  if (d.material == kMaterialMissileCore) {
    // CORE_FRAGMENT
    const float t = d.params0.x, glow = d.params0.w;
    const float center = 1.0 - smoothstep(0.015, 0.16, length(i.local.xy));
    const float pulse = 0.84 + 0.16 * sin(t * 19.0 + i.local.z * 20.0);
    c = lerp(float3(0.17, 0.045, 0.55), float3(0.72, 0.46, 1.0), center);
    c = lerp(c, float3(0.97, 0.91, 1.0), pow(center, 3.0));
    c *= glow * pulse;
    a = 0.98;
  } else if (d.material == kMaterialMissileShell) {
    // SHELL_FRAGMENT
    const float t = d.params0.x, glow = d.params0.w, opacity = d.params1.x;
    const float3 q = i.local;
    const float n = sin(q.x * 39.0 + t * 11.0) * sin(q.y * 31.0 - t * 9.0) * sin(q.z * 27.0 + t * 8.0);
    const float bands = 0.44 + 0.56 * abs(sin(atan2(q.y, q.x) * 6.0 + t * 4.0));
    a = clamp((0.48 + n * 0.3) * bands * opacity, 0.0, 0.82);
    c = lerp(float3(0.13, 0.025, 0.43), float3(0.65, 0.35, 0.98), bands) * glow;
  } else if (d.material == kMaterialMissileTrail) {
    // TRAIL_FRAGMENT
    const float t = d.params0.x, opacity = d.params0.y, breakup = d.params0.z;
    const float u = i.uv.x, v = i.uv.y;
    const float tip = smoothstep(0.0, 0.25, u);
    const float fade = pow(max(u, 0.0), 1.7);
    const float noise = sin(u * 72.0 + t * 8.0) * sin(u * 31.0 - t * 5.0);
    const float cuts = smoothstep(-0.4 + breakup * 0.5, 0.48 + breakup * 0.5, noise);
    const float core = 1.0 - smoothstep(0.22, 0.5, abs(v - 0.5));
    c = lerp(float3(0.12, 0.025, 0.46), float3(0.75, 0.52, 1.0), tip);
    c = lerp(c, float3(0.97, 0.92, 1.0), core * tip);
    a = opacity * fade * cuts * (0.35 + 0.65 * core);
  } else if (d.material == kMaterialFlame) {
    // flameMaterial's fragment: facing the viewer = the hot core, grazing = the thin flaring edge.
    const MeshFrame f = loadFrame(push.frame);
    const float opacity = d.params0.z;
    const bool edgeFade = d.params0.w > 0.5;
    const float facing = saturate(dot(normalize(i.normal), normalize(f.cameraPosition - i.world)));
    const float heat = saturate(facing * 0.85 + i.heat * 0.6 + 0.15);
    const float3 red = float3(0.55, 0.06, 0.01), orange = float3(1.0, 0.42, 0.05), yellow = float3(1.0, 0.82, 0.35), white = float3(1.0, 0.97, 0.88);
    float3 col = lerp(red, orange, smoothstep(0.0, 0.45, heat));
    col = lerp(col, yellow, smoothstep(0.45, 0.75, heat));
    col = lerp(col, white, smoothstep(0.8, 1.0, heat));
    a = opacity * (edgeFade ? smoothstep(0.0, 0.55, facing) : 1.0);
    c = col * a;
  } else if (d.material == kMaterialImpactParticle) {
    // FireballImpactVFX's fragment: kind 0 the fire flipbook, 1 a procedural flame, 2 smoke.
    const float2 uv = i.uv;
    const float frameValue = i.particleB.y, alpha = i.particleB.z, intensity = i.particleB.w, kind = i.particleC.w;
    float3 color = i.particleC.rgb;
    if (kind < 0.5) {
      // The atlas cell (Ember: frame 0 top left) is a layer of the cooked clip: params1 maps the cell's uv (v down)
      // into the trimmed texture (xy: cell size in pixels, zw: the crop), color.xy = 1 / texture size.
      const float frame = floor(fmod(frameValue, 16.0));
      const float2 tex = cellToTexture(d, clamp(uv, 0.015, 0.985));
      const float4 texel = gTextureArrays[NonUniformResourceIndex(d.texture)].Sample(gSamplers[push.samplerIndex], float3(tex, frame));
      a = texel.a * alpha;
      color *= texel.rgb;
    } else if (kind < 1.5) {
      const float2 q = (uv - 0.5) * float2(1.0, 1.8);
      const float dd = length(q);
      a = (1.0 - smoothstep(0.02, 0.5, dd)) * alpha;
      color *= lerp(float3(1.0, 0.98, 0.78), float3(1.0, 0.18, 0.015), smoothstep(0.05, 0.42, dd));
    } else {
      const float2 q = (uv - 0.5) * float2(1.0, 1.35);
      const float n = noise2(uv * 5.3 + float2(frameValue * 0.008, -frameValue * 0.013)) * 0.55 + noise2(uv * 11.0 + frameValue * 0.004) * 0.25;
      const float dd = length(q);
      const float cloud = 1.0 - smoothstep(0.18 + n * 0.12, 0.58 + n * 0.2, dd);
      a = cloud * alpha;
      color *= 0.62 + n * 0.55;
    }
    if (a < 0.004) discard;
    c = color * intensity;
  } else if (d.material == kMaterialBlessWave) {
    // BlessVFX WAVE_FRAGMENT
    const float t = d.params0.x, opacity = d.params1.x, emissive = d.params1.y;
    const float streak = smoothstep(0.26, 0.88, i.heat);
    const float3 amber = float3(1.0, 0.28, 0.025), gold = float3(1.0, 0.67, 0.16), ivory = float3(1.0, 0.91, 0.62);
    c = lerp(amber, gold, streak);
    c = lerp(c, ivory, pow(streak, 5.0));
    const float flicker = 0.76 + 0.24 * sin(t * 15.0 + i.heat * 8.0);
    c *= emissive * flicker;
    a = opacity * (0.35 + streak * 0.65);
  } else if (d.material == kMaterialPfTendril) {
    // TENDRIL_FRAGMENT
    const float t = d.params0.x, opacity = d.params1.x;
    const float along = i.uv.x;
    float pulse = smoothstep(0.0, 0.22, frac(t * 0.9 + along));
    pulse *= 1.0 - smoothstep(0.28, 0.55, frac(t * 0.9 + along));
    const float3 indigo = float3(0.12, 0.08, 0.72), violet = float3(0.42, 0.22, 1.0), lavender = float3(0.77, 0.63, 1.0);
    float3 color = lerp(indigo, violet, smoothstep(0.0, 0.65, along));
    color = lerp(color, lavender, saturate(pulse * 0.82 + smoothstep(0.78, 1.0, along) * 0.35));
    const float flicker = 0.72 + 0.28 * sin(t * 11.0 + i.heat * 4.0);
    a = opacity * flicker * (0.48 + pulse * 0.52);
    c = color * (1.5 + pulse * 2.5);
  } else if (d.material == kMaterialPfCore) {
    // CORE_FRAGMENT
    const float t = d.params0.x, emission = d.params1.x;
    const float3 q = i.local;
    const float turbulence = 0.5 + 0.5 * sin(q.x * 24.0 + t * 11.0) * cos(q.y * 22.0 - t * 8.0) * sin(q.z * 25.0 + t * 6.0);
    float3 color = lerp(float3(0.33, 0.13, 1.0), float3(0.76, 0.61, 1.0), turbulence);
    color = lerp(color, float3(1.0, 0.96, 1.0), smoothstep(0.75, 1.0, turbulence));
    c = color * emission;
    a = 0.98;
  } else if (d.material == kMaterialPfShell) {
    // SHELL_FRAGMENT: brighter toward the silhouette (the view-space normal's z).
    const MeshFrame f = loadFrame(push.frame);
    const float opacity = d.params1.x, distortion = d.params0.y;
    const float viewZ = mul(float4(normalize(i.normal), 0.0), f.view).z;
    const float edge = pow(1.0 - abs(viewZ), 1.5);
    c = lerp(float3(0.28, 0.16, 0.96), float3(0.86, 0.75, 1.0), saturate(edge + abs(i.heat) * distortion)) * 2.3;
    a = opacity * (0.2 + edge * 0.8);
  } else if (d.material == kMaterialWebThread) {
    // THREAD_FRAGMENT: indigo to lavender along the thread, a bevel and a rim in view space.
    const MeshFrame f = loadFrame(push.frame);
    const float energy = d.params1.y, opacity = d.params1.z;
    const float along = i.uv.x, visible = i.local.x, pulse = i.local.y;
    const float3 indigo = float3(0.12, 0.075, 0.68), violet = float3(0.42, 0.18, 0.94), lavender = float3(0.78, 0.66, 1.0),
                 cyanLavender = float3(0.57, 0.78, 1.0);
    float3 color = lerp(indigo, violet, smoothstep(0.02, 0.72, along));
    color = lerp(color, lavender, saturate(pulse * 0.92 + smoothstep(0.76, 1.0, along) * 0.16));
    color = lerp(color, cyanLavender, pulse * 0.12);
    const float3 viewNormal = normalize(mul(float4(normalize(i.normal), 0.0), f.view).xyz);
    const float3 viewDirection = normalize(-mul(float4(i.world, 1.0), f.view).xyz);
    const float bevel = 0.42 + 0.58 * abs(dot(viewNormal, normalize(float3(-0.36, 0.48, 0.8))));
    const float rim = pow(1.0 - abs(dot(viewNormal, viewDirection)), 2.0);
    color = color * bevel + lavender * rim * 0.62;
    a = opacity * visible * (0.48 + pulse * 0.5);
    c = color * energy * (0.72 + pulse * 1.6);
  } else if (d.material == kMaterialSpriteSheet) {
    // SpriteMaterial (map = the sheet's cell, colour = a scalar, opacity, alphaTest 0.02), additive.
    const float4 texel = gTextureArrays[NonUniformResourceIndex(d.texture)].Sample(gSamplers[push.samplerIndex], float3(cellToTexture(d, i.uv), i.particleB.y));
    a = texel.a * i.particleB.z;
    if (a < 0.02) discard;
    c = texel.rgb * i.particleB.w;
  } else if (d.material == kMaterialVenomSmoke) {
    // VENOM_SMOKE_FRAGMENT: ragged smoke lobes from the sphere's own surface, no texture.
    const float t = d.params0.x, opacity = d.params0.y, seed = d.params0.z;
    // Fixed 2026-10-21 (the user: no green smoke at all): every fragment lies on the sphere's surface (radius 1), past the ragged
    // edge (0.72 + field x 0.44), so almost all were discarded, and the colour was near black. Now a soft lobe: dense where it faces
    // the eye, thinning to nothing at its rim, torn by the same noise, an acid green.
    const float3 sp = i.local;
    const float3 p = sp + float3(seed * 2.7, seed * 0.83, t * 0.18);
    const float field = smokeField(p, t);
    const MeshFrame f = loadFrame(push.frame);
    const float facing = abs(dot(normalize(i.normal), normalize(f.cameraPosition - i.world)));
    const float body = smoothstep(0.05, 0.75, facing + (field - 0.5) * 0.5);
    const float pockets = 0.45 + 0.55 * smoothstep(0.2, 0.7, field);
    a = body * pockets * opacity;
    if (a < 0.018) discard;
    const float variation = smoothstep(0.28, 0.8, field);
    c = lerp(float3(0.12, 0.4, 0.06), float3(0.36, 0.8, 0.15), variation);
  } else if (d.material == kMaterialFlipbookParticle) {
    // ParticleEmitter's fragment: the sheet's cell, its uv rippled a little, x tint x emissive x uIntensity.
    const float frameValue = i.particleB.y, alpha = i.particleB.z, emissive = i.particleB.w * d.params0.x;
    const float frame = floor(fmod(frameValue, 16.0));
    float2 localUv = clamp(i.uv, 0.02, 0.98);
    const float turbulence = sin(localUv.y * 13.0 + frameValue * 0.37) * sin(localUv.x * 9.0 - frameValue * 0.29);
    localUv += float2(turbulence * 0.014, turbulence * 0.006);
    const float4 flame = gTextureArrays[NonUniformResourceIndex(d.texture)].Sample(gSamplers[push.samplerIndex],
                                                                                   float3(cellToTexture(d, clamp(localUv, 0.02, 0.98)), frame));
    a = flame.a * alpha;
    if (a < 0.006) discard;
    c = flame.rgb * i.particleC.rgb * emissive;
  } else if (d.material == kMaterialImpactFlash) {
    const float opacity = d.params0.x, intensity = d.params0.y;
    const float dd = length((i.uv - 0.5) * float2(1.0, 1.15));
    const float core = exp(-dd * 18.0), halo = exp(-dd * 5.0);
    a = (core + halo * 0.36) * opacity;
    if (a < 0.005) discard;
    c = lerp(float3(1.0, 0.46, 0.04), d.color.rgb, core) * a * intensity;
  } else if (d.material == kMaterialHoly) {
    // Ours (render/vfx/HolyLight): the cures', the meal's and the potions' light. params0 = time, mode, intensity,
    // opacity; color.rgb = its colour; params1.rgb = the hot core's colour, params1.a = the ground ring's radius.
    const MeshFrame f = loadFrame(push.frame);
    const float t = d.params0.x, mode = d.params0.y, intensity = d.params0.z, opacity = d.params0.w;
    if (mode < 0.5) {
      // A shaft of light (an open cylinder, u around, v up): seen through as a volume, brightest where it faces the eye,
      // faded toward its top, with rays rising through it.
      const float facing = abs(dot(normalize(i.normal), normalize(f.cameraPosition - i.world)));
      const float body = pow(facing, 1.6);
      const float v = i.uv.y;
      const float along = smoothstep(0.0, 0.1, v) * (1.0 - smoothstep(0.35, 1.0, v));
      const float rays = noise2(float2(i.uv.x * 14.0, v * 3.0 - t * 1.6)) * 0.6 + noise2(float2(i.uv.x * 31.0, v * 6.0 - t * 2.7)) * 0.4;
      a = body * along * (0.45 + 0.9 * smoothstep(0.35, 0.85, rays)) * opacity;
      c = lerp(d.color.rgb, d.params1.rgb, saturate(body * body * body * 0.6)) * intensity;
    } else if (mode < 1.5) {
      // The ground's glow (a disc, u around, v out): a soft pool and a sparkling ring at params1.a of its radius.
      const float r = i.uv.y, around = i.uv.x;
      const float pool = pow(saturate(1.0 - r), 2.2) * 0.55;
      const float ring = exp(-pow((r - d.params1.a) / 0.12, 2.0)) * 0.6;
      const float sparkle = 0.65 + 0.35 * noise2(float2(around * 40.0 + t * 3.0, r * 6.0 - t * 2.0));
      a = (pool + ring * sparkle) * opacity;
      c = lerp(d.color.rgb, d.params1.rgb, ring * 0.5) * intensity;
    } else if (mode < 2.5) {
      // A mote: a soft dot of light facing the camera (instance b = rotation, -, alpha, intensity; c = its tint).
      const float dd = length(i.uv - 0.5);
      const float core = exp(-dd * dd * 90.0), halo = exp(-dd * dd * 18.0) * 0.45;
      a = (core + halo) * i.particleB.z * opacity;
      if (a < 0.004) discard;
      c = lerp(i.particleC.rgb, d.params1.rgb, core) * i.particleB.w * intensity;
    } else {
      // A ribbon of light along a tube or a ring (u along): brightest where it faces the eye, faded at both ends, a
      // current running along it.
      const float facing = abs(dot(normalize(i.normal), normalize(f.cameraPosition - i.world)));
      const float ends = mode < 3.5 ? smoothstep(0.0, 0.18, i.uv.x) * (1.0 - smoothstep(0.82, 1.0, i.uv.x)) : 1.0;
      const float flow = 0.7 + 0.3 * sin(i.uv.x * 24.0 - t * 9.0);
      a = (0.25 + 0.75 * facing) * ends * opacity;
      c = lerp(d.color.rgb, d.params1.rgb, facing * facing * 0.5) * intensity * flow;
    }
  } else if (d.material == kMaterialCombat) {
    // Ours (render/vfx/CombatFx): the blows' and the Arcano bolt's light. params0 = time, mode, intensity, opacity;
    // color.rgb = the energy's colour; params1.rgb = the hot core's colour, params1.a = the mode's own number.
    const MeshFrame f = loadFrame(push.frame);
    const float t = d.params0.x, mode = d.params0.y, intensity = d.params0.z, opacity = d.params0.w;
    if (mode < 0.5) {
      // A streak of light (u across, v tail -> head: brightest at the head, a white-hot thread down its middle), or a
      // round dot.
      const float2 q = (i.uv - 0.5) * 2.0;
      float shape, hot;
      if (i.particleB.y > 0.5) {
        const float r2 = dot(q, q);
        shape = exp(-r2 * 4.5);
        hot = exp(-r2 * 22.0);
      } else {
        const float across = q.x * q.x, along = i.uv.y;
        shape = exp(-across * 5.0) * (0.2 + 0.8 * along) * smoothstep(0.0, 0.2, along) * (1.0 - smoothstep(0.85, 1.0, along));
        hot = exp(-across * 30.0) * along;
      }
      a = shape * i.particleB.x * opacity;
      if (a < 0.003) discard;
      c = lerp(i.particleC.rgb, d.params1.rgb, saturate(hot)) * i.particleC.w * intensity;
    } else if (mode < 1.5) {
      // A puff of dust: a ragged soft disc, lit by the scene's own light like the ground it came from (dark at night,
      // coloured by a spell's light), alpha-blended.
      const float2 q = (i.uv - 0.5) * 2.0;
      const float n = noise2(i.uv * 3.5 + i.particleB.y) * 0.6 + noise2(i.uv * 7.0 - i.particleB.y) * 0.4;
      const float shape = saturate(1.0 - smoothstep(0.3, 1.0, length(q) + (n - 0.5) * 0.55));
      const float alpha = shape * i.particleB.z * opacity;
      if (alpha < 0.003) discard;
      const float3 toEye = normalize(f.cameraPosition - i.world);
      Surface s = defaultSurface(i.world, normalize(toEye * 0.55 + float3(0.0, 0.45, 0.0)), i.particleC.rgb);
      s.roughness = 1.0;
      s.metallic = 0.0;
      s.receivesCardShadow = 0.0;
      s.sightAt = d.sightAt;
      const Sight sight = tacticalSight(f, d.sightAt);
      const float3 lit = shadeInSight(f, s, sight) * (0.85 + 0.3 * n);
      return float4(applyVolume(f, lit, i.world) * i.sight * alpha, alpha);
    } else if (mode < 2.5) {
      // An energy sphere: a hot heart where it faces the eye (params1.a = how much of it), a rim at its edge, boiling.
      const float facing = abs(dot(normalize(i.normal), normalize(f.cameraPosition - i.world)));
      const float coreWeight = d.params1.a;
      const float core = pow(facing, 2.5) * coreWeight;
      // A shell (no heart) keeps only a thin edge, so it reads as a wave in the air, never as a glass ball.
      const float rim = pow(1.0 - facing, lerp(5.0, 2.2, coreWeight));
      const float boil = 0.72 + 0.28 * snoise(i.local * 5.0 + float3(0.0, t * 4.0, t * 2.0));
      a = (core + rim * (1.0 - 0.4 * coreWeight)) * boil * opacity;
      c = lerp(d.color.rgb, d.params1.rgb, saturate(core * 1.2)) * intensity;
    } else if (mode < 3.5) {
      // A ring of light on the ground (a disc, u around, v out): a thin bright ring at params1.a of its radius, a faint
      // pool inside, sparkling.
      const float r = i.uv.y, around = i.uv.x;
      const float ring = exp(-pow((r - d.params1.a) / 0.07, 2.0));
      const float pool = pow(saturate(1.0 - r), 2.5) * 0.25;
      const float sparkle = 0.7 + 0.3 * noise2(float2(around * 48.0 + t * 4.0, r * 8.0 - t * 3.0));
      a = (ring * sparkle + pool) * opacity;
      c = lerp(d.color.rgb, d.params1.rgb, ring * 0.5) * intensity;
    } else if (mode < 4.5) {
      // A trail (a tube, u from its tail to its head): brightest at the head and where it faces the eye, a current in it.
      const float facing = abs(dot(normalize(i.normal), normalize(f.cameraPosition - i.world)));
      const float along = i.uv.x;
      a = (0.3 + 0.7 * pow(facing, 1.5)) * pow(along, 1.6) * opacity * (0.75 + 0.25 * sin(along * 30.0 - t * 20.0));
      c = lerp(d.color.rgb, d.params1.rgb, facing * facing * along) * intensity;
    } else if (mode >= 5.5) {
      // Lightning (render/vfx/Lightning, Relâmpago 02): a jagged tube from the sky to the body (u from the top to the foot). A
      // white-hot thread down its middle where it faces the eye, the energy's colour in the shell around it, a hard crackle
      // along its length; params1.a = how much of the bolt is lit (a flash that dies from the top), color.a = unused.
      const float facing = abs(dot(normalize(i.normal), normalize(f.cameraPosition - i.world)));
      const float along = i.uv.x;
      const float crackle = 0.78 + 0.22 * sin(along * 140.0 + t * 130.0 + i.uv.y * 6.0);
      const float lit = smoothstep(0.0, 0.08, d.params1.a - along);
      a = (0.35 + 0.65 * pow(facing, 1.2)) * opacity * crackle * lit;
      if (a < 0.003) discard;
      c = lerp(d.color.rgb, d.params1.rgb, saturate(pow(facing, 3.0) * 1.3)) * intensity;
    } else {
      // A blade's cut (a sheet along the swing, u along its path, v from its inner side to the cutting edge): only the
      // part between the tail (color.a) and the head (params1.a) shows; a white-hot edge, a thin steel wash behind it,
      // brightest near the head; streaked along its length like light caught on a moving edge.
      const float s = i.uv.x, v = i.uv.y, head = d.params1.a, tail = d.color.a;
      if (s > head || s < tail) discard;
      const float span = max(head - tail, 1e-3);
      const float fromTail = smoothstep(0.0, 0.35, (s - tail) / span);
      const float atHead = exp(-pow((head - s) / 0.1, 2.0));
      const float edge = exp(-pow((1.0 - v) / 0.1, 2.0));
      const float wash = pow(v, 2.2) * 0.32;
      const float streaks = 0.75 + 0.25 * noise2(float2(s * 40.0, v * 3.0));
      a = (edge + wash * streaks) * fromTail * (0.65 + 0.7 * atHead) * opacity;
      if (a < 0.003) discard;
      c = lerp(d.color.rgb, d.params1.rgb, saturate(edge * 0.8 + atHead * 0.4)) * intensity;
    }
  } else if (d.material == kMaterialIceStorm) {
    // render/vfx/IceStorm (Tempestade de Gelo). params0 = time, mode, intensity, opacity. Alpha-blended, as Blizzard2D's pass.
    const MeshFrame f = loadFrame(push.frame);
    const float t = d.params0.x, mode = d.params0.y, intensity = d.params0.z, opacity = d.params0.w;
    if (mode < 0.5) {
      // Ours: the frosted floor (a hex, local xz = its own coordinates), spreading from the middle out to params1.x; a
      // frost of crystal veins in world space (adjacent hexes join), glints twinkling in it.
      const float2 wp = i.world.xz;
      const float n = noise2(wp * 1.7) * 0.6 + noise2(wp * 4.1) * 0.4;
      const float edge = 1.0 - smoothstep(d.params1.x - 0.35, d.params1.x, length(i.local.xz) + (n - 0.5) * 0.5);
      const float vein = pow(1.0 - abs(noise2(wp * 6.0 + 3.1) * 2.0 - 1.0), 6.0) * 0.7 + pow(1.0 - abs(noise2(wp * 13.0 - 1.7) * 2.0 - 1.0), 8.0) * 0.5;
      const float2 grain = floor(wp * 30.0);
      const float glint = step(0.985, hash2(grain)) * (0.5 + 0.5 * sin(t * 6.0 + hash2(grain + 7.0) * 40.0));
      a = saturate(0.08 + 0.3 * n * n + vein * 0.45 + glint) * edge * opacity;
      c = (lerp(float3(0.45, 0.66, 0.85), float3(0.9, 0.96, 1.0), saturate(vein + n * 0.4)) + glint) * intensity;
    } else if (mode < 1.5) {
      // Ours: a snowflake, a soft white dot.
      const float dd = length((i.uv - 0.5) * 2.0);
      a = (1.0 - smoothstep(0.15, 1.0, dd)) * i.particleB.z * opacity;
      if (a < 0.004) discard;
      c = lerp(float3(0.78, 0.92, 1.0), float3(1.0, 1.0, 1.0), exp(-dd * dd * 8.0)) * intensity;
    } else if (mode < 2.5) {
      // Ours: a band of wind (u along, v across): streaks flowing along it, soft at its ends and edges.
      const float u = i.uv.x, v = i.uv.y, seed = i.particleC.y;
      const float across = 1.0 - pow(abs(v * 2.0 - 1.0), 1.6);
      const float ends = smoothstep(0.0, 0.25, u) * (1.0 - smoothstep(0.6, 1.0, u));
      const float streak = noise2(float2(u * 3.0 - t * 1.4 + seed * 17.0, v * 9.0 + seed * 5.0)) * 0.65 + noise2(float2(u * 9.0 - t * 2.3, v * 23.0 + seed * 11.0)) * 0.35;
      a = across * ends * smoothstep(0.35, 0.85, streak) * i.particleC.x * opacity;
      if (a < 0.004) discard;
      c = lerp(float3(0.55, 0.78, 0.95), float3(0.9, 0.97, 1.0), streak) * intensity;
    } else if (mode < 3.5) {
      // Ours: an ice crystal, flat faces: clear in the middle, bright at its rim, a glint where a face catches the sky.
      const float3 toEye = normalize(f.cameraPosition - i.world);
      const float3 n = normalize(i.normal);
      const float facing = abs(dot(n, toEye));
      const float rim = pow(1.0 - facing, 2.0);
      const float glint = pow(saturate(dot(reflect(-toEye, n), normalize(float3(0.3, 0.9, 0.3)))), 24.0);
      a = saturate(0.12 + 0.5 * rim + glint * 0.8) * i.particleC.x * opacity;
      c = (lerp(float3(0.32, 0.58, 0.86), float3(0.85, 0.95, 1.0), rim) + glint) * intensity;
    } else if (mode < 4.5) {
      // Blizzard2D's floor: the wind sheet's cell (1, 2) laid over the field (Ember tiled it over the screen, here over the
      // ground), mixed toward ice blue, 0.32 strong.
      const float2 tile = frac(i.world.xz * float2(0.26, 0.3));
      const float4 ice = iceSheet(d, (float2(1.0, 2.0) + tile) / 4.0);
      a = ice.a * 0.32 * opacity;
      c = lerp(ice.rgb, float3(0.72, 0.9, 0.96), 0.45) * intensity;
    } else if (mode < 5.5) {
      // Blizzard2D's FS, line for line (vUv = uv with v down, p = vUv x 2 - 1); b = rotation, frame, alpha, kind.
      const float2 e = float2(i.uv.x, 1.0 - i.uv.y);
      const float2 p = e * 2.0 - 1.0;
      const float kind = i.particleB.w, frame = floor(i.particleB.y);
      const float2 cell = float2(fmod(frame, 4.0), floor(frame / 4.0));
      float4 tex;
      if (kind < 0.5) {
        float2 uv = e;
        uv.x += sin(uv.y * 9.0 + t * 1.3 + frame) * 0.045;
        uv.y += sin(uv.x * 7.0 - t * 0.8) * 0.055;
        tex = iceSheet(d, (cell + clamp(uv, 0.005, 0.995)) / 4.0);
        const float ribbon = pow(max(0.0, 1.0 - p.x * p.x), 0.5) * pow(max(0.0, 1.0 - abs(p.y)), 0.7);
        tex.a *= ribbon;
        tex.rgb = lerp(tex.rgb, float3(0.13, 0.58, 0.83), 0.25);
      } else if (kind < 1.5) {
        const float2 uv = float2(0.49 + e.x * 0.12, 0.035 + e.y * 0.66);
        tex = iceSheet(d, (cell + uv) / 4.0);
        tex.a *= 1.0 - smoothstep(0.82, 0.99, e.y + sin(e.x * 13.0) * 0.025);
      } else if (kind < 2.5) {
        tex = float4(0.76, 0.94, 1.0, 1.0 - smoothstep(0.1, 0.85, length(p)));
      } else {
        tex = iceSheet(d, (cell + clamp(e, 0.002, 0.998)) / 4.0);
      }
      a = tex.a * i.particleB.z * opacity;
      if (a < 0.006) discard;
      c = tex.rgb * intensity;
    } else if (mode < 6.5) {
      // Ours: a puff of ice dust, a ragged soft disc.
      const float2 q = (i.uv - 0.5) * 2.0;
      const float n = noise2(i.uv * 4.0 + i.particleC.w) * 0.6 + noise2(i.uv * 8.0 - i.particleC.w) * 0.4;
      a = saturate(1.0 - smoothstep(0.2, 1.0, length(q) + (n - 0.5) * 0.5)) * i.particleB.z * opacity;
      if (a < 0.004) discard;
      c = lerp(float3(0.7, 0.86, 0.97), float3(0.95, 0.99, 1.0), n) * intensity;
    } else {
      // Ours: a frost ring on the ground (a disc, u around, v out), glittering.
      const float r = i.uv.y;
      const float ring = exp(-pow((r - 0.85) / 0.09, 2.0)) + pow(saturate(1.0 - r), 3.0) * 0.15;
      const float sparkle = 0.6 + 0.4 * noise2(float2(i.uv.x * 40.0 + i.particleC.y * 9.0, r * 7.0 - t * 2.0));
      a = ring * sparkle * i.particleC.x * opacity;
      if (a < 0.004) discard;
      c = float3(0.82, 0.95, 1.0) * intensity;
    }
  } else if (d.material == kMaterialStandard) {
    // MeshStandardMaterial: lit by the scene like any surface, plus its emissive (display-relative, as Ember's glow);
    // color.a = its opacity (a transparent material: NormalBlending).
    const MeshFrame f = loadFrame(push.frame);
    float3 n = normalize(i.normal);
    if (dot(n, f.cameraPosition - i.world) < 0.0) n = -n;
    const float3 albedo = d.color.rgb * (i.particleC.w > 0.5 ? i.particleC.rgb : 1.0);  // instanceColor x colour
    Surface s = defaultSurface(i.world, n, albedo);
    s.roughness = d.params0.x;
    s.metallic = d.params0.y;
    s.receivesCardShadow = 0.0;
    s.sightAt = d.sightAt;
    const Sight sight = tacticalSight(f, d.sightAt);
    c = shadeInSight(f, s, sight) + d.params1.rgb * d.params1.a * push.brightness;
    const float opacity = saturate(d.color.a);
    return float4(applyVolume(f, c, i.world) * i.sight * opacity, opacity);
  } else if (d.material == kMaterialTextured) {
    // Ours: a cooked mesh's own material (MeshScene::addEffect, the Nuvem de Adagas' dagger), read as resolve.hlsl reads it, lit
    // like Standard. The instance colour scales the colour and the glow (a dagger materializing or dissolving).
    const MeshFrame f = loadFrame(push.frame);
    SamplerState smp = gSamplers[f.materialSampler];
    const uint normalMap = asuint(d.params1.x), ormMap = asuint(d.params1.y), emissiveMap = asuint(d.params1.z);
    const float3 tint = i.particleC.w > 0.5 ? i.particleC.rgb : 1.0;
    float3 n = normalize(i.normal);
    if (dot(n, f.cameraPosition - i.world) < 0.0) n = -n;
    const float3 albedo = gTextures[NonUniformResourceIndex(d.texture)].Sample(smp, i.uv).rgb * d.color.rgb * tint;
    Surface s = defaultSurface(i.world, n, albedo);
    if (ormMap != kNoTexture) {
      const float3 orm = gTextures[NonUniformResourceIndex(ormMap)].Sample(smp, i.uv).rgb;
      s.occlusion = orm.r;
      s.roughness = orm.g;
      s.metallic = orm.b;
    }
    if (normalMap != kNoTexture) {
      // An effect's vertices carry no tangent: the frame comes from the screen derivatives of the position and the uv. glTF's
      // uv runs down the image while its normal maps' +Y points up it: the bitangent is the one along -v.
      const float3 dp1 = ddx(i.world), dp2 = ddy(i.world);
      const float2 duv1 = ddx(i.uv), duv2 = ddy(i.uv);
      const float3 dp2perp = cross(dp2, n), dp1perp = cross(n, dp1);
      const float3 t = dp2perp * duv1.x + dp1perp * duv2.x;
      const float3 b = dp2perp * duv1.y + dp1perp * duv2.y;
      const float scale = rsqrt(max(max(dot(t, t), dot(b, b)), 1e-20));
      const float2 xy = gTextures[NonUniformResourceIndex(normalMap)].Sample(smp, i.uv).rg * 2.0 - 1.0;
      const float z = sqrt(saturate(1.0 - dot(xy, xy)));
      s.normal = normalize(t * (scale * xy.x) - b * (scale * xy.y) + n * z);
    }
    s.receivesCardShadow = 0.0;
    s.sightAt = d.sightAt;
    const Sight sight = tacticalSight(f, d.sightAt);
    c = shadeInSight(f, s, sight);
    if (emissiveMap != kNoTexture)
      c += gTextures[NonUniformResourceIndex(emissiveMap)].Sample(smp, i.uv).rgb * tint * d.params1.w * push.brightness;
    const float opacity = saturate(d.color.a);
    return float4(applyVolume(f, c, i.world) * i.sight * opacity, opacity);
  } else {
    // MeshBasicMaterial / LineBasicMaterial
    c = d.color.rgb;
    a = d.color.a;
  }
  // Atmosfera volumétrica v1: the effect's light crosses the fog (1 when the volume does not ask for it).
  c *= push.brightness * i.sight * volumeTransmittance(loadFrame(push.frame), i.world);
  a = max(a, 0.0);
  if (d.flags & 1u) return float4(c * a, 0.0);  // AdditiveBlending: src x srcAlpha + dst
  return float4(c * a, a);
}
