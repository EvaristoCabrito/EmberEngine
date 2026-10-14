// Shared by every shader: the global bindless tables (see engine/rhi/Bindless.h).
#ifndef EMBER_COMMON_HLSLI
#define EMBER_COMMON_HLSLI

[[vk::binding(0, 0)]] Texture2D gTextures[];
[[vk::binding(1, 0)]] SamplerState gSamplers[];
[[vk::binding(2, 0)]] RWTexture2D<float4> gStorageImages[];

// Fullscreen triangle from SV_VertexID (no vertex buffer). uv (0,0) = top-left.
struct FullscreenOut {
  float4 position : SV_Position;
  float2 uv : TEXCOORD0;
};

FullscreenOut fullscreenTriangle(uint id) {
  FullscreenOut o;
  o.uv = float2((id << 1) & 2, id & 2);
  o.position = float4(o.uv * 2.0 - 1.0, 0.0, 1.0);
  return o;
}

#endif
