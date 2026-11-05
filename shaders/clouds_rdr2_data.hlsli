// Consumer side of the extended RDR2 record. No density or atmosphere dependency here.
#ifndef EMBER_RDR2_CONSUMERS
#define EMBER_RDR2_CONSUMERS
bool rdr2On(MeshFrame f){return f.atmosphere!=0 && (f.cloudFlags&8u)!=0;}
uint64_t rdr2Data(MeshFrame f){return vk::RawBufferLoad<uint64_t>(f.atmosphere+512u);}
float4 rdr2Composite(MeshFrame f,float2 uv,uint colour) {
  const uint64_t r=rdr2Data(f);
  const uint4 textures=vk::RawBufferLoad<uint4>(r+272u);
  const float4 info=vk::RawBufferLoad<float4>(r+80u);
  const float2 texel=1/max(info.xy,1);
  const float centreDepth=gTextures[NonUniformResourceIndex(textures.z)].SampleLevel(gSamplers[f.volumeSampler],uv,0).r;
  const float radius=lerp(1.1,.35,saturate(centreDepth/9000));
  const uint64_t blue=vk::RawBufferLoad<uint64_t>(r+256u);
  const uint2 pixel=uint2(uv*f.screenSize);
  const uint index=(pixel.x&63u)+(pixel.y&63u)*64u;
  const float noise=frac(vk::RawBufferLoad<float>(blue+uint64_t(index)*4u)+info.z*.61803398875);
  const float angle=noise*6.2831853;
  const float2x2 rotation=float2x2(cos(angle),-sin(angle),sin(angle),cos(angle));
  float4 sum=0,nearest=float4(0,0,0,1);float count=0,closest=1e25;
  [unroll]for(uint i=0;i<4;++i) {
    const float2 tapOffset=float2((i&1u)?1:-1,(i&2u)?1:-1)*.5;
    const float2 at=uv+mul(rotation,tapOffset)*texel*radius;
    const float d=gTextures[NonUniformResourceIndex(textures.z)].SampleLevel(gSamplers[f.volumeSampler],at,0).r;
    const float4 value=gTextures[NonUniformResourceIndex(colour)].SampleLevel(gSamplers[f.volumeSampler],at,0);
    const float difference=abs(d-centreDepth);
    if(difference<closest){closest=difference;nearest=value;}
    if(difference<=max(40,centreDepth*.08)){sum+=value;++count;}
  }
  return count>0?sum/count:nearest;
}
float rdr2EsmShadow(MeshFrame f,float3 p,uint map) {
  const uint64_t r=rdr2Data(f);
  const float4 rect=vk::RawBufferLoad<float4>(r+224u),info=vk::RawBufferLoad<float4>(r+240u);
  const float4 layer=vk::RawBufferLoad<float4>(r+32u),middle=vk::RawBufferLoad<float4>(r+336u);
  const float3 light=normalize(f.sunDirection);
  if(light.y<=.01)return 1;
  const float2 projected=p.xz-light.xz*((p.y-info.z)/light.y);
  const float2 uv=(projected-rect.xy)*rect.zw;
  if(any(uv<0)||any(uv>1))return 1;
  const float upper=max(layer.z,middle.y);
  const float receiver=saturate((upper-p.y)/max(upper-info.z,1));
  // One shadow footprint per ~2 world units, filtered through the six-level ESM chain.
  const float footprint=2*max(rect.z,rect.w)*info.x;
  const float lod=clamp(log2(max(footprint,1)),0,5);
  const float moment=gTextures[NonUniformResourceIndex(map)].SampleLevel(gSamplers[f.volumeSampler],uv,lod).r;
  const float visibility=saturate(moment*exp(-8*receiver));
  return lerp(1,visibility,saturate(info.y)*smoothstep(.01,.08,light.y));
}
float3 rdr2ProbeIrradiance(MeshFrame f,float3 p,float3 n,bool raw) {
  const uint64_t r=rdr2Data(f),buffer=vk::RawBufferLoad<uint64_t>(r+368u);
  const float4 rect=vk::RawBufferLoad<float4>(r+352u);
  const float2 grid=clamp((p.xz-rect.xy)*rect.zw*32-.5,0,31);
  const int2 base=int2(floor(grid));const float2 t=frac(grid);
  const float b[9]={.282095,.488603*n.y,.488603*n.z,.488603*n.x,1.092548*n.x*n.y,
    1.092548*n.y*n.z,.315392*(3*n.z*n.z-1),1.092548*n.x*n.z,.546274*(n.x*n.x-n.y*n.y)};
  float3 result=0;
  [unroll]for(uint y=0;y<2;++y)[unroll]for(uint x=0;x<2;++x) {
    const uint2 at=min(uint2(base)+uint2(x,y),31u);
    const float w=(x==0?1-t.x:t.x)*(y==0?1-t.y:t.y);
    const uint64_t probe=buffer+uint64_t(at.x+at.y*32u)*144u;
    [unroll]for(uint j=0;j<9;++j)result+=vk::RawBufferLoad<float4>(probe+j*16u).rgb*b[j]*w;
  }
  // Scaling is supplied by lighting.hlsli; the record always stores unexposed irradiance.
  return max(result,0);
}
float3 rdr2CubeSample(MeshFrame f,float3 d) {
  const uint64_t r=rdr2Data(f);
  const uint cube=vk::RawBufferLoad<uint4>(r+304u).x;
  const float3 ad=abs(d);float2 q;uint face;
  if(ad.x>=ad.y&&ad.x>=ad.z){face=d.x>=0?0:1;q=float2(d.x>=0?-d.z:d.z,-d.y)/max(ad.x,1e-5);}
  else if(ad.y>=ad.z){face=d.y>=0?2:3;q=float2(d.x,d.y>=0?d.z:-d.z)/max(ad.y,1e-5);}
  else{face=d.z>=0?4:5;q=float2(d.z>=0?d.x:-d.x,-d.y)/max(ad.z,1e-5);}
  return gTextureArrays[NonUniformResourceIndex(cube)].SampleLevel(gSamplers[f.volumeSampler],float3(q*.5+.5,face),0).rgb;
}
float3 rdr2WaterSample(MeshFrame f,float3 d) {
  const uint water=vk::RawBufferLoad<uint4>(rdr2Data(f)+288u).w;
  const float2 uv=float2(atan2(d.z,d.x)/6.2831853+.5,clamp(acos(clamp(d.y,-1,1))/3.14159265,.5/256,1-.5/256));
  return gTextures[NonUniformResourceIndex(water)].SampleLevel(gSamplers[f.materialSampler],uv,0).rgb;
}
#endif
