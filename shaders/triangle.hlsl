// Bring-up test: one spinning HDR triangle (colours above 1.0 exercise the tonemapper).
#include "common.hlsli"

struct Push {
  float time;
  float aspect;
};
[[vk::push_constant]] Push push;

struct VSOut {
  float4 position : SV_Position;
  float3 color : COLOR0;
};

VSOut VSMain(uint id : SV_VertexID) {
  const float2 corners[3] = { float2(0.0, -0.62), float2(0.56, 0.42), float2(-0.56, 0.42) };
  const float3 colors[3] = { float3(6.0, 0.6, 0.15), float3(0.25, 3.0, 0.5), float3(0.2, 0.6, 6.0) };
  const float s = sin(push.time * 0.6), c = cos(push.time * 0.6);
  float2 p = corners[id];
  p = float2(p.x * c - p.y * s, p.x * s + p.y * c);
  p.x /= push.aspect;
  VSOut o;
  o.position = float4(p, 0.5, 1.0);
  o.color = colors[id];
  return o;
}

float4 PSMain(VSOut i) : SV_Target0 {
  return float4(i.color, 1.0);
}
