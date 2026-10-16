// Engine2's spell effects (its three.js VFX classes, ported in render/vfx/*): every draw is a mesh the CPU built
// (render/VfxDraw.h, VfxGeometry.h) with one of Engine2's materials, its GLSL translated line for line below. Drawn
// over the finished scene in the effects pass, never writing depth. Each material computes what its GLSL writes to
// gl_FragColor (c = rgb, a = alpha); three's blending is then reproduced in one premultiplied pipeline (ONE,
// ONE_MINUS_SRC_ALPHA): NormalBlending (src x srcAlpha + dst x (1 - srcAlpha)) as (c x a, a), AdditiveBlending
// (src x srcAlpha + dst) as (c x a, 0). Brightness is display-relative like the flipbook effects (Engine2's materials
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
                  kMaterialStandard = 4, kMaterialFlame = 5, kMaterialImpactParticle = 6, kMaterialImpactFlash = 7;

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
  if (d.material == kMaterialImpactParticle) {
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
  if (d.material == kMaterialFlame) {
    // flameMaterial's vertex: the boiling surface (the noise scrolls up +z and churns), pushed along the normal.
    const float t = d.params0.x, amp = d.params0.y;
    const float3 q = v.position * 1.6 + float3(0.0, 0.0, -t * 1.4);
    const float n = fbm(q + float3(t * 0.35, t * 0.2, 0.0));
    o.heat = n;
    p = v.position + v.normal * n * amp;
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
    const float4x4 inst = loadInstance(d.firstInstance + instance).matrix;
    world = mul(world, inst);
    normal = mul(normal, inst);
  }
  world = mul(world, d.model);
  normal = mul(normal, d.model);
  o.position = mul(world, f.viewProjection);
  o.local = p;
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
      // The atlas cell (Engine2: frame 0 top left) is a layer of the cooked clip: params1 maps the cell's uv (v down)
      // into the trimmed texture (xy: cell size in pixels, zw: the crop), color.xy = 1 / texture size.
      const float frame = floor(fmod(frameValue, 16.0));
      const float2 cell = clamp(uv, 0.015, 0.985);
      const float2 tex = (float2(cell.x, 1.0 - cell.y) * d.params1.xy - d.params1.zw) * d.color.xy;
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
  } else if (d.material == kMaterialImpactFlash) {
    const float opacity = d.params0.x, intensity = d.params0.y;
    const float dd = length((i.uv - 0.5) * float2(1.0, 1.15));
    const float core = exp(-dd * 18.0), halo = exp(-dd * 5.0);
    a = (core + halo * 0.36) * opacity;
    if (a < 0.005) discard;
    c = lerp(float3(1.0, 0.46, 0.04), d.color.rgb, core) * a * intensity;
  } else if (d.material == kMaterialStandard) {
    // MeshStandardMaterial: lit by the scene like any surface, plus its emissive (display-relative, as Engine2's glow).
    const MeshFrame f = loadFrame(push.frame);
    float3 n = normalize(i.normal);
    if (dot(n, f.cameraPosition - i.world) < 0.0) n = -n;
    Surface s = defaultSurface(i.world, n, d.color.rgb);
    s.roughness = d.params0.x;
    s.metallic = d.params0.y;
    s.receivesCardShadow = 0.0;
    s.sightAt = d.sightAt;
    const Sight sight = tacticalSight(f, d.sightAt);
    c = shadeInSight(f, s, sight) + d.params1.rgb * d.params1.a * push.brightness;
    return float4(c * i.sight, 1.0);
  } else {
    // MeshBasicMaterial / LineBasicMaterial
    c = d.color.rgb;
    a = d.color.a;
  }
  c *= push.brightness * i.sight;
  a = max(a, 0.0);
  if (d.flags & 1u) return float4(c * a, 0.0);  // AdditiveBlending: src x srcAlpha + dst
  return float4(c * a, a);
}
