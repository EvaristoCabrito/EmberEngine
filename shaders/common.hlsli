// Shared by every shader: the global bindless tables (see engine/rhi/Bindless.h).
#ifndef EMBER_COMMON_HLSLI
#define EMBER_COMMON_HLSLI

[[vk::binding(0, 0)]] Texture2D gTextures[];
// The same descriptors seen as texture arrays (sprite clips: one layer per frame). Each index is
// read only through the type it was created with.
[[vk::binding(0, 0)]] Texture2DArray gTextureArrays[];
[[vk::binding(1, 0)]] SamplerState gSamplers[];
// The same samplers seen as comparison samplers (shadow maps); only those created with compareEnable.
[[vk::binding(1, 0)]] SamplerComparisonState gComparisonSamplers[];
[[vk::binding(2, 0)]] RWTexture2D<float4> gStorageImages[];
// Typed views of the same storage images (one format per image).
[[vk::binding(2, 0)]] [[vk::image_format("rgba16f")]] RWTexture2D<float4> gStorageImagesRgba16f[];
[[vk::binding(2, 0)]] [[vk::image_format("r32ui")]] RWTexture2D<uint> gStorageImagesR32ui[];
[[vk::binding(2, 0)]] [[vk::image_format("r32f")]] RWTexture2D<float> gStorageImagesR32f[];
// Storage buffers, for atomics and counters (plain reads go through device addresses).
[[vk::binding(3, 0)]] RWByteAddressBuffer gBuffers[];

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
