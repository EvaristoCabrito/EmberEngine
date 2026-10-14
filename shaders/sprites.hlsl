// Character sprites: upright cards in the 3D scene that turn about the vertical axis to face the
// camera. Each clip is a texture array (one layer per frame), stored with straight alpha in sRGB BC7;
// colour is premultiplied after sampling and blended premultiplied, so edges never fringe.
// Unlit for now; scene lighting and shadows arrive with the lighting milestone.
#include "common.hlsli"

// Matches ember::SpriteInstance (render/SpriteDraw.h). 48 bytes.
struct SpriteInstance {
  float3 origin;  // world position of the picture's bottom centre
  float width;
  float height;
  uint textureIndex;
  uint layer;
  uint flags;     // bit 0: mirrored
  float2 uvScale; // the trimmed picture's share of the texture (the rest is block padding)
  float2 pad;
};

struct Push {
  float4x4 viewProjection;
  float3 cameraRight;
  uint samplerIndex;
  uint64_t instances;  // device address of SpriteInstance[]
};
[[vk::push_constant]] Push push;

struct VSOut {
  float4 position : SV_Position;
  float2 uv : TEXCOORD0;
  nointerpolation uint textureIndex : TEXCOORD1;
  nointerpolation uint layer : TEXCOORD2;
};

VSOut VSMain(uint vertex : SV_VertexID, uint instance : SV_InstanceID) {
  const float2 corners[6] = { float2(0, 0), float2(1, 0), float2(1, 1), float2(0, 0), float2(1, 1), float2(0, 1) };
  const SpriteInstance s = vk::RawBufferLoad<SpriteInstance>(push.instances + instance * 48);
  const float2 c = corners[vertex];
  const float3 world = s.origin + push.cameraRight * ((c.x - 0.5) * s.width) + float3(0, 1, 0) * (c.y * s.height);
  VSOut o;
  o.position = mul(float4(world, 1.0), push.viewProjection);  // -Zpr: glm's column-major matrix reads as its transpose
  const float u = (s.flags & 1u) ? 1.0 - c.x : c.x;
  o.uv = float2(u, 1.0 - c.y) * s.uvScale;
  o.textureIndex = s.textureIndex;
  o.layer = s.layer;
  return o;
}

float4 PSMain(VSOut i) : SV_Target0 {
  float4 c = gTextureArrays[NonUniformResourceIndex(i.textureIndex)].Sample(gSamplers[push.samplerIndex], float3(i.uv, i.layer));
  clip(c.a - 1.0 / 255.0);
  c.rgb *= c.a;
  return c;
}
