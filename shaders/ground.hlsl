// Placeholder floor for the sprite test stage: a dark plane at y = 0 with a 1 m grid.
#include "common.hlsli"

struct Push {
  float4x4 viewProjection;
  float halfExtent;
};
[[vk::push_constant]] Push push;

struct VSOut {
  float4 position : SV_Position;
  float2 world : TEXCOORD0;
};

VSOut VSMain(uint vertex : SV_VertexID) {
  const float2 corners[6] = { float2(-1, -1), float2(1, -1), float2(1, 1), float2(-1, -1), float2(1, 1), float2(-1, 1) };
  const float2 p = corners[vertex] * push.halfExtent;
  VSOut o;
  o.position = mul(float4(p.x, 0.0, p.y, 1.0), push.viewProjection);  // -Zpr: glm's column-major matrix reads as its transpose
  o.world = p;
  return o;
}

float4 PSMain(VSOut i) : SV_Target0 {
  const float2 cell = abs(frac(i.world - 0.5) - 0.5) / fwidth(i.world);
  const float grid = 1.0 - saturate(min(cell.x, cell.y));
  const float3 base = float3(0.035, 0.038, 0.045);
  return float4(base + grid * 0.06, 1.0);
}
