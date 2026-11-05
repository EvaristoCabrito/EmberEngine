#ifndef EMBER_CLOUDS008_COMPOSITE
#define EMBER_CLOUDS008_COMPOSITE
// Sky-only reconstruction: deterministic bilinear taps preserve smooth coverage at Low.
// Opaque scene depth is handled by the sky's existing depth test.
float4 cloud008Composite(MeshFrame f,float2 uv,uint image){
  const uint64_t record=rdr2Data(f);
  const float4 info=vk::RawBufferLoad<float4>(record+80u);
  const uint4 textures=vk::RawBufferLoad<uint4>(record+272u);
  const float2 texel=1/max(info.xy,1);
  const float4 centre=gTextures[NonUniformResourceIndex(image)].SampleLevel(gSamplers[f.volumeSampler],uv,0);
  const float depth=gTextures[NonUniformResourceIndex(textures.z)].SampleLevel(gSamplers[f.volumeSampler],uv,0).r;
  float4 sum=centre*4;float weight=4;
  [unroll]for(uint i=0;i<4;++i){
    const float2 offset=i==0?float2(-.5,0):i==1?float2(.5,0):i==2?float2(0,-.5):float2(0,.5);
    const float2 at=uv+offset*texel;
    const float nearby=gTextures[NonUniformResourceIndex(textures.z)].SampleLevel(gSamplers[f.volumeSampler],at,0).r;
    const float w=1-smoothstep(max(40,depth*.08),max(80,depth*.16),abs(nearby-depth));
    const float4 sampleValue=gTextures[NonUniformResourceIndex(image)].SampleLevel(gSamplers[f.volumeSampler],at,0);
    sum+=sampleValue*w;weight+=w;
  }
  return sum/weight;
}
#endif
