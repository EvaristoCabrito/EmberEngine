// The board's water: Engine2's water sheet (engine/world/GroundMesh.h buildWater) and its material
// (src/editor/ember/dioramaView.ts): colour #2f86a8, roughness 0.5, metalness 0.05, 74% opaque, a ripple
// normal map (normal scale 0.6) scrolling by (0.02, 0.013) per second, uv = world xz / 4. Engine2 bakes the
// ripple into a 128 px texture from a sum of sines (src/render/textures.ts); the same sines are evaluated
// here directly. Lit through the lighting contract; blended premultiplied, depth-tested, no depth write.
#include "lighting.hlsli"

struct Push {
  uint64_t frame;     // MeshFrame
  uint64_t vertices;  // WaterVertex[] (16 bytes: position, octahedral normal)
  uint64_t indices;   // uint[]
};
[[vk::push_constant]] Push push;

struct VSOut {
  float4 position : SV_Position;
  float3 world : TEXCOORD0;
  float3 normal : TEXCOORD1;
};

VSOut VSMain(uint id : SV_VertexID) {
  const MeshFrame f = loadFrame(push.frame);
  const uint index = vk::RawBufferLoad<uint>(push.indices + uint64_t(id) * 4);
  const uint64_t v = push.vertices + uint64_t(index) * 16;
  VSOut o;
  o.world = vk::RawBufferLoad<float3>(v);
  o.normal = decodeNormal(vk::RawBufferLoad<uint>(v + 12));
  o.position = mul(float4(o.world, 1.0), f.viewProjection);
  return o;
}

// Engine2's ripple height field, in texels of its 128 px tile.
float rippleHeight(float2 p) {
  return sin(p.x * 0.19 + sin(p.y * 0.11) * 2.0) * 0.5 + sin(p.y * 0.23 + p.x * 0.07) * 0.5 + sin((p.x + p.y) * 0.31) * 0.25;
}

float4 PSMain(VSOut i) : SV_Target0 {
  const MeshFrame f = loadFrame(push.frame);
  const float2 uv = i.world.xz / 4.0 + float2(0.02, 0.013) * f.time;
  const float2 p = frac(uv) * 128.0;
  // Engine2's texel normal: (-(dH/dx), -(dH/dy), 2), central differences one texel apart.
  const float3 tn = normalize(float3(-(rippleHeight(p + float2(1, 0)) - rippleHeight(p - float2(1, 0))),
                                     -(rippleHeight(p + float2(0, 1)) - rippleHeight(p - float2(0, 1))), 2.0));
  // On the flat sheet the texture's x runs along world x and its y along world z (uv = xz / 4).
  const float3 n = normalize(i.normal);
  const float3 shading = normalize(n + float3(tn.x, 0.0, tn.y) * 0.6 / max(tn.z, 1e-3));
  Surface s = defaultSurface(i.world, n, srgbToLinear3(float3(0x2f, 0x86, 0xa8) / 255.0));
  s.normal = shading;
  s.roughness = 0.5;
  s.metallic = 0.05;
  const float opacity = 0.74;
  return float4(shade(f, s) * opacity, opacity);
}
