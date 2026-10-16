// Spell effects: flipbook frames of the fx clips (Engine2's effect art, sprite clips in set "fx") on quads the CPU
// places (render/EffectDraw.h; the recipes in render/SpellEffects.cpp). Drawn after every surface, depth-tested
// against them without writing depth, blended premultiplied: opacity 1 covers what is behind by the art's alpha,
// 0 only adds light. Two frames are blended for smooth motion; each quad fades where it meets the surface behind it
// (soft particles), so a ring on the ground or a burst at a unit's feet never shows a hard cut.
//
// Brightness is display-relative (the art at 1 is the art as painted at the scene's white), not physical: the CPU
// scales it by the light mode's glow rule, as it does for painted flames. Only what is in sight is drawn (the fog of
// war's local-light term: an effect is light, and memory shows no light).
#include "lighting.hlsli"

// Matches ember::EffectInstance (render/EffectDraw.h). 96 bytes.
struct EffectInstance {
  float3 center;
  float softness;
  float3 axisX;
  uint textureIndex;
  float3 axisY;
  uint layer;
  float3 color;
  float opacity;
  float2 uvScale;
  uint nextLayer;
  float blend;
  float3 sightAt;
  uint flags;  // 1 mirrored, 2 lit (solid art: an arrow takes the scene's light like a card instead of glowing)
};

struct Push {
  uint64_t instances;  // device address of EffectInstance[]
  uint64_t frame;      // MeshFrame
  uint samplerIndex;
  uint depthTexture;   // the scene's depth (reversed-Z, depth = near / distance)
  float brightness;    // the light mode's glow rule, x every effect's colour
  uint pad;
};
[[vk::push_constant]] Push push;

struct VSOut {
  float4 position : SV_Position;
  float2 uv : TEXCOORD0;
  nointerpolation uint instance : TEXCOORD1;
  nointerpolation float sight : TEXCOORD2;
  float3 world : TEXCOORD3;
};

EffectInstance loadEffect(uint i) { return vk::RawBufferLoad<EffectInstance>(push.instances + uint64_t(i) * 96); }

VSOut VSMain(uint vertex : SV_VertexID, uint instance : SV_InstanceID) {
  const float2 corners[6] = { float2(0, 0), float2(1, 0), float2(1, 1), float2(0, 0), float2(1, 1), float2(0, 1) };
  const EffectInstance e = loadEffect(instance);
  const MeshFrame f = loadFrame(push.frame);
  const float2 c = corners[vertex];
  const float3 world = e.center + e.axisX * (c.x * 2.0 - 1.0) + e.axisY * (c.y * 2.0 - 1.0);
  VSOut o;
  o.position = mul(float4(world, 1.0), f.viewProjection);  // -Zpr: glm's column-major matrix reads as its transpose
  const Sight sight = tacticalSight(f, e.sightAt);
  o.sight = sight.local;
  if (sight.local <= 0.0) o.position = float4(0, 0, -1, 1);  // out of sight: nothing
  const float u = (e.flags & 1u) ? 1.0 - c.x : c.x;
  o.uv = float2(u, 1.0 - c.y) * e.uvScale;
  o.instance = instance;
  o.world = world;
  return o;
}

float4 PSMain(VSOut i) : SV_Target0 {
  const EffectInstance e = loadEffect(i.instance);
  const MeshFrame f = loadFrame(push.frame);
  const Texture2DArray art = gTextureArrays[NonUniformResourceIndex(e.textureIndex)];
  const SamplerState s = gSamplers[push.samplerIndex];
  // Straight alpha in the texture; premultiplied before the two frames are mixed, so edges never darken.
  float4 c = art.Sample(s, float3(i.uv, e.layer));
  c.rgb *= c.a;
  if (e.blend > 0.0) {
    float4 n = art.Sample(s, float3(i.uv, e.nextLayer));
    n.rgb *= n.a;
    c = lerp(c, n, e.blend);
  }
  float fade = i.sight;
  if (e.softness > 0.0) {
    // Linear distances from the reversed-Z depths: the surface behind this pixel and the quad itself.
    const float scene = gTextures[NonUniformResourceIndex(push.depthTexture)].Load(int3(i.position.xy, 0)).r;
    const float behind = f.zNear / max(scene, 1e-7), here = f.zNear / max(i.position.z, 1e-7);
    fade *= saturate((behind - here) / e.softness);
  }
  if (c.a * fade <= 1.0 / 1024.0) discard;
  if (e.flags & 2u) {
    // Solid art, lit as a card is (wrap, no specular), facing the camera; never brighter than twice its colour.
    const float3 albedo = c.rgb / max(c.a, 1e-4) * e.color;
    Surface surface = defaultSurface(i.world, normalize(f.cameraPosition - i.world), albedo);
    surface.wrap = 0.85;
    surface.specular = 0.0;
    surface.receivesCardShadow = 0.0;
    const Sight sight = tacticalSight(f, e.sightAt);
    const float3 lit = min(shadeInSight(f, surface, sight), albedo * 2.0 * f.litCap);
    return float4(lit * c.a * fade, c.a * fade * e.opacity);
  }
  return float4(c.rgb * e.color * push.brightness * fade, c.a * fade * e.opacity);
}
