#ifndef EMBER_CLOUDS002_DATA
#define EMBER_CLOUDS002_DATA
struct Cloud006Frame {
  float4x4 previousVP;
  float4 rect;
  float4 low;
  float4 upper;
  float4 windTime;
  uint4 fields; // cloud map, height LUT, ESM, unused
  uint4 history; // colour, depth, raw colour, raw depth
  uint4 settings; // serial, history valid, march steps, light steps
  uint4 sampling; // stride, width, height, unused
  uint4 extras; // SH grid, cube, water reflection, water history
  uint4 extras2; // displacement, blue noise address low/high, resolved cloud depth
};
bool clouds006On(MeshFrame f){return (f.cloudFlags&8u)!=0 && f.atmosphere!=0;}
Cloud006Frame cloud006Frame(MeshFrame f){
  const uint64_t address=vk::RawBufferLoad<uint64_t>(f.atmosphere+512u);
  return vk::RawBufferLoad<Cloud006Frame>(address);
}
float cloud006Coverage(MeshFrame f,float2 xz,uint layer){
  const Cloud006Frame s=cloud006Frame(f);
  const float2 uv=frac((xz+s.windTime.xy*s.windTime.z-s.rect.xy)*s.rect.zw);
  const float4 map=gTextures[NonUniformResourceIndex(s.fields.x)].SampleLevel(gSamplers[f.volumeSampler],uv,0);
  return layer==0?map.r:map.g;
}
// Identical density evaluation for the viewport, cloud shadow, fog and reflection.
float cloud006Density(MeshFrame f,float3 p,bool cheap){
  const Cloud006Frame s=cloud006Frame(f);
  const CloudVolume v=loadCloudVolume(f);
  float density=0;
  for(uint layer=0;layer<2;++layer){
    const float base=s.low.x+(layer==0?0:s.low.y*1.8);
    const float thickness=max(1,s.low.y*(layer==0?1:.45));
    const float h=(p.y-base)/thickness;
    if(h<=0||h>=1)continue;
    const float cover=cloud006Coverage(f,p.xz,layer);
    const float4 profile=gTextures[NonUniformResourceIndex(s.fields.y)].SampleLevel(gSamplers[f.volumeSampler],float2(.5,h),0);
    const float body=smoothstep(profile.x,profile.y,cover);
    if(body<=.0001)continue;
    const float3 drift=float3(s.windTime.x,0,s.windTime.y)*s.windTime.z;
    const float3 q=(p+drift)/max(v.scale*2,1);
    const float2 d0=gTextures[NonUniformResourceIndex(s.extras2.x)].SampleLevel(gSamplers[f.volumeSampler],frac(q.xy*.21),0).rg;
    const float2 d1=gTextures[NonUniformResourceIndex(s.extras2.x)].SampleLevel(gSamplers[f.volumeSampler],frac(q.xz*.19+.37),0).rg;
    const float3 displaced=q+float3(d0.x,d0.y+d1.x,d1.y)*.13;
    const float4 noise=cloudNoiseBaseAt(v,displaced);
    const float carve=(1-noise.r)*profile.z;
    float shaped=saturate((body-carve)/max(1-carve,.001));
    if(!cheap&&shaped>0){
      const float4 detail=cloudNoiseDetailAt(v,displaced*4.1);
      const float erosion=(detail.r*.625+detail.g*.25+detail.b*.125)*profile.z*.35;
      shaped=saturate((shaped-erosion)/max(1-erosion,.001));
    }
    density+=shaped*profile.w;
  }
  return saturate(density);
}
float cloud006RainFog(MeshFrame f,float3 p){
  const Cloud006Frame s=cloud006Frame(f);
  if(s.upper.w<=0||p.y>=s.low.x)return 0;
  const float distance=max(0,s.low.x-p.y);
  return cloud006Coverage(f,p.xz,0)*s.upper.w*.002*exp(-distance/max(s.low.y*1.5,1));
}
float cloud006Shadow(MeshFrame f,float3 p){
  const Cloud006Frame s=cloud006Frame(f);
  const float3 light=normalize(f.sunDirection);
  if(light.y<=.01)return 1;
  const float farHeight=s.low.x+s.low.y*2.5;
  const float2 at=p.xz-light.xz*(p.y/max(light.y,.01));
  const float2 uv=(at-s.rect.xy)*s.rect.zw;
  if(any(uv<0)||any(uv>1))return 1;
  const float receiver=saturate((farHeight-p.y)/max(farHeight,1));
  const float moment=gTextures[NonUniformResourceIndex(s.fields.z)].SampleLevel(gSamplers[f.volumeSampler],uv,1).r;
  return saturate(moment*exp(-8*receiver));
}
// Two Wrenninge octaves and the extinction-controlled Lambertian backscatter floor.
float cloud006Phase(float cosine,float optical,float extinction,float g){
  const float pi=3.14159265359;
  float sum=0,weight=1,attenuation=1;
  for(uint octave=0;octave<2;++octave){
    const float gg=g*g;
    const float hg=(1-gg)/(4*pi*pow(max(1+gg-2*g*cosine,1e-4),1.5));
    const float back=saturate((extinction-.001)/.015)/pi;
    sum+=weight*max(hg,back)*exp(-optical*attenuation);
    weight*=.5;attenuation*=.5;g*=.5;
  }
  return sum;
}
float3 cloud006ProbeIrradiance(MeshFrame f,float3 p,float3 n){
 const Cloud006Frame s=cloud006Frame(f);const float2 uv=saturate((p.xz-s.rect.xy)*s.rect.zw);
 const float b[9]={.282095,.488603*n.y,.488603*n.z,.488603*n.x,1.092548*n.x*n.y,
   1.092548*n.y*n.z,.315392*(3*n.z*n.z-1),1.092548*n.x*n.z,.546274*(n.x*n.x-n.y*n.y)};
 float3 light=0;for(uint c=0;c<9;++c){
  const float3 coordinate=float3(uv,(float(c)+.5)/9);
  light+=gTextures3D[NonUniformResourceIndex(s.extras.x)].SampleLevel(gSamplers[f.volumeSampler],coordinate,0).rgb*b[c];
 }return max(light,0);
}
#endif
