#include "cloudsvol.hlsli"
#include "clouds006_lighting.hlsli"
struct Push{uint64_t frame;uint destination,extra,source,mip,width,height;};
[[vk::push_constant]] Push push;
float hash002(float2 p){return frac(sin(dot(p,float2(127.1,311.7)))*43758.5453);}
float noise002(float2 p){float2 i=floor(p),f=frac(p);f=f*f*(3-2*f);
 return lerp(lerp(hash002(i),hash002(i+float2(1,0)),f.x),lerp(hash002(i+float2(0,1)),hash002(i+1),f.x),f.y);}
float fbm002(float2 p){return noise002(p)*.57+noise002(p*2.03+11.3)*.28+noise002(p*4.09+31.1)*.15;}
[numthreads(8,8,1)]
void CSWeather(uint2 pixel:SV_DispatchThreadID){
 if(any(pixel>=uint2(push.width,push.height)))return;
 const MeshFrame f=loadFrame(push.frame);const Cloud006Frame s=cloud006Frame(f);
 const float2 uv=(float2(pixel)+.5)/float2(push.width,push.height);
 const float n=fbm002(uv*24),m=fbm002(uv*17+73.1);
 const float low=saturate((n-(1-s.low.z))/.32);
 const float middle=saturate((m-(1-s.upper.x))/.32);
 gStorageImagesRg16f[NonUniformResourceIndex(push.destination)][pixel]=float2(low,middle);
}
[numthreads(8,8,1)]
void CSHeight(uint2 pixel:SV_DispatchThreadID){
 if(any(pixel>=uint2(push.width,push.height)))return;
 const MeshFrame f=loadFrame(push.frame);const Cloud006Frame s=cloud006Frame(f);
 const float h=(float(pixel.y)+.5)/128;
 const float row=saturate(s.low.w+s.upper.z*.25)*11;
 const float2 uv=float2(h,(row+.5)/12);
 const float4 profile=gTextures[NonUniformResourceIndex(s.fields.w)].SampleLevel(gSamplers[f.volumeSampler],uv,0);
 gStorageImagesRgba16f[NonUniformResourceIndex(push.destination)][pixel]=profile;
}
[numthreads(8,8,1)]
void CSEsm(uint2 pixel:SV_DispatchThreadID){
 if(any(pixel>=uint2(push.width,push.height)))return;
 const MeshFrame f=loadFrame(push.frame);const Cloud006Frame s=cloud006Frame(f);
 const uint phase=s.settings.y==0?0:(s.settings.x&15u);
 if(((pixel.x&3u)+((pixel.y&3u)<<2u))!=phase)return;
 const float3 light=normalize(f.sunDirection);float moment=exp(8.0);
 if(light.y>.01){
  const float top=s.low.x+s.low.y*2.5;
  const float2 at=s.rect.xy+((float2(pixel)+.5)/float2(push.width,push.height))/s.rect.zw;
  const float3 origin=float3(at.x,0,at.y)+light*(top/light.y);
  const uint steps=max(8u,s.settings.z/2u);const float dt=(top-s.low.x)/(light.y*float(steps));
  float t=1;moment=0;
  for(uint i=0;i<steps;++i){
   const float3 p=origin-light*((float(i)+.5)*dt);
   const float alpha=1-exp(-cloud006Density(f,p,true)*.035*dt);
   moment+=t*alpha*exp(8*saturate((top-p.y)/max(top,1)));t*=1-alpha;
  }moment+=t*exp(8.0);
 }
 gStorageImagesR16f[NonUniformResourceIndex(push.destination)][pixel]=moment;
 if(s.settings.y==0)for(uint y=0;y<4;++y)for(uint x=0;x<4;++x)
  gStorageImagesR16f[NonUniformResourceIndex(push.destination)][pixel+uint2(x,y)]=moment;
}
[numthreads(8,8,1)]
void CSFilter(uint2 pixel:SV_DispatchThreadID){
 if(any(pixel>=uint2(push.width,push.height)))return;
 const MeshFrame f=loadFrame(push.frame);
 const float2 uv=(float2(pixel)+.5)/float2(push.width,push.height);
 const float2 texel=(push.mip==0?1.0:.5)/float2(push.width,push.height);
 float sum=0;for(int y=-1;y<=1;++y)for(int x=-1;x<=1;++x){
  const float weight=float((x==0?2:1)*(y==0?2:1));
  sum+=weight*gTextures[NonUniformResourceIndex(push.source)].SampleLevel(gSamplers[f.volumeSampler],uv+float2(x,y)*texel,push.mip==0?0:push.mip-1).r;
 }gStorageImagesR16f[NonUniformResourceIndex(push.destination)][pixel]=sum/16;
}
[numthreads(8,8,1)]
void CSMarch(uint2 pixel:SV_DispatchThreadID){
 if(any(pixel>=uint2(push.width,push.height)))return;
 const MeshFrame f=loadFrame(push.frame);const AtmosphereFrame a=loadAtmosphere(f);const Cloud006Frame s=cloud006Frame(f);
 const uint stride=s.sampling.x;
 const bool trace=s.settings.y==0 || (pixel.x%stride+(pixel.y%stride)*stride)==s.settings.x%(stride*stride);
 float4 result=float4(0,0,0,-1);float depth=0;
 if(trace){
  const float2 uv=(float2(pixel)+.5)/float2(push.width,push.height);
  const float3 dir=skyDirection(f,a,uv*f.screenSize);
  const uint2 bluePixel=(pixel+uint2(s.settings.x*17u,s.settings.x*29u))&63u;
  const uint index=bluePixel.x+bluePixel.y*64u;
  const uint64_t address=(uint64_t(s.extras2.z)<<32u)|uint64_t(s.extras2.y);
  const uint word=vk::RawBufferLoad<uint>(address+(index/4u)*4u);
  const float jitter=(float((word>>((index&3u)*8u))&255u)+.5)/256;
  result=cloud006March(f,a,f.cameraPosition,dir,s.settings.z,jitter,depth);
 }
 gStorageImagesRgba16f[NonUniformResourceIndex(push.destination)][pixel]=result;
 gStorageImagesRgba16f[NonUniformResourceIndex(push.extra)][pixel]=float4(depth,0,0,1);
}
[numthreads(8,8,1)]
void CSResolve(uint2 pixel:SV_DispatchThreadID){
 if(any(pixel>=uint2(push.width,push.height)))return;
 const MeshFrame f=loadFrame(push.frame);const AtmosphereFrame a=loadAtmosphere(f);const Cloud006Frame s=cloud006Frame(f);
 const float2 uv=(float2(pixel)+.5)/float2(push.width,push.height);
 float4 current=gTextures[NonUniformResourceIndex(s.history.z)].Load(int3(pixel,0));
 float depth=gTextures[NonUniformResourceIndex(s.history.w)].Load(int3(pixel,0)).r;
 float4 lo=float4(1e20,1e20,1e20,1),hi=float4(0,0,0,0),sum=0;float count=0,depthSum=0;
 const int radius=1;
 for(int y=-radius;y<=radius;++y)for(int x=-radius;x<=radius;++x){
  const int2 q=clamp(int2(pixel)+int2(x,y),int2(0,0),int2(push.width,push.height)-1);
  const float4 neighbour=gTextures[NonUniformResourceIndex(s.history.z)].Load(int3(q,0));
  if(neighbour.a<0)continue;
  const float sampleDepth=gTextures[NonUniformResourceIndex(s.history.w)].Load(int3(q,0)).r;
  if(depth>0&&abs(depth-sampleDepth)>max(30,depth*.05))continue;
  lo=min(lo,neighbour);hi=max(hi,neighbour);sum+=neighbour;depthSum+=sampleDepth;++count;
 }
 if(current.a<0){current=count>0?sum/count:float4(0,0,0,1);depth=count>0?depthSum/count:0;}
 if(s.settings.y!=0){
  const float3 dir=skyDirection(f,a,uv*f.screenSize);
  const float historyDepth=gTextures[NonUniformResourceIndex(s.history.y)].SampleLevel(gSamplers[f.volumeSampler],uv,0).r;
  const float d=depth>0?depth:max(historyDepth,1000);
  const float3 world=f.cameraPosition+dir*d+float3(s.windTime.x,0,s.windTime.y)*s.windTime.w;
  const float4 clip=mul(float4(world,1),s.previousVP);
  const float2 previous=clip.xy/max(clip.w,1e-5)*.5+.5;
  if(clip.w>0&&all(previous>=0)&&all(previous<=1)){
   const float4 old=gTextures[NonUniformResourceIndex(s.history.x)].SampleLevel(gSamplers[f.volumeSampler],previous,0);
   const float priorDepth=gTextures[NonUniformResourceIndex(s.history.y)].SampleLevel(gSamplers[f.volumeSampler],previous,0).r;
   const bool accept=abs(priorDepth-d)<max(40,d*.04)&&abs(old.a-current.a)<.2;
   const float weight=accept?(count>0?.86:1):0;
   current=lerp(current,count>0?clamp(old,lo,hi):old,weight);
   if(depth<=0&&accept)depth=priorDepth;
  }
 }
 gStorageImagesRgba16f[NonUniformResourceIndex(push.destination)][pixel]=current;
 gStorageImagesRgba16f[NonUniformResourceIndex(push.extra)][pixel]=float4(depth,0,0,1);
}

[numthreads(8,8,1)]
void CSAtlas(uint2 pixel:SV_DispatchThreadID){
 if(any(pixel>=uint2(push.width,push.height)))return;
 const float h=(float(pixel.x)+.5)/128,type=float(pixel.y)/11;
 const float crown=lerp(.30,.98,type);
 const float shoulder=lerp(.68,.48,type);
 const float shape=smoothstep(0,lerp(.05,.14,type),h)*(1-smoothstep(crown*shoulder,crown,h));
 const float threshold=.6-shape*.6;
 gStorageImagesRgba16f[NonUniformResourceIndex(push.destination)][pixel]=float4(threshold,threshold+.25,lerp(.32,.62,type),shape);
}
[numthreads(8,8,1)]
void CSDisplacement(uint2 pixel:SV_DispatchThreadID){
 if(any(pixel>=uint2(push.width,push.height)))return;
 const float2 uv=(float2(pixel)+.5)/float2(push.width,push.height);
 const float2 displacement=float2(fbm002(uv*16+13),fbm002(uv*13+91))*2-1;
 gStorageImagesRg16f[NonUniformResourceIndex(push.destination)][pixel]=displacement;
}
float3 incident002(MeshFrame f,AtmosphereFrame a,float3 origin,float3 dir,uint steps){
 if(dir.y<=.003){
  const float3 sky=float3(vk::RawBufferLoad<float4>(a.ambient+16).w,vk::RawBufferLoad<float4>(a.ambient+32).w,vk::RawBufferLoad<float4>(a.ambient+48).w);
  return a.groundAlbedo*(sky+a.sunIlluminance*transmittanceToTop(a,a.viewHeight,a.sunDirection.y)*saturate(a.sunDirection.y))*a.skyScale/kPi;
 }
 float depth;
 const float4 cloud=cloud006March(f,a,origin,dir,steps,.5,depth,false);
 return skyRadiance(a,dir)*a.skyScale*cloud.a+cloud.rgb;
}
[numthreads(8,8,1)]
void CSProbes(uint2 pixel:SV_DispatchThreadID){
 if(any(pixel>=uint2(push.width,push.height)))return;
 const MeshFrame f=loadFrame(push.frame);const AtmosphereFrame a=loadAtmosphere(f);const Cloud006Frame s=cloud006Frame(f);
 const uint period=s.sampling.w==0?16u:s.sampling.w==1?8u:s.sampling.w==2?4u:1u;
 if(s.settings.y!=0 && ((pixel.x+pixel.y*32u)%period)!=(s.settings.x%period))return;
 const float2 xz=s.rect.xy+((float2(pixel)+.5)/32)/s.rect.zw;
 const uint rays=s.sampling.w<2?8u:16u;
 float3 sh[9];for(uint c=0;c<9;++c)sh[c]=0;
 for(uint i=0;i<rays;++i){
  const float y=(float(i)+.5)/float(rays),phi=float(i)*2.39996323;
  const float3 d=float3(cos(phi)*sqrt(1-y*y),y,sin(phi)*sqrt(1-y*y));
  const float3 radiance=incident002(f,a,float3(xz.x,0,xz.y),d,max(8u,s.settings.z/4u));
  const float b[9]={.282095,.488603*d.y,.488603*d.z,.488603*d.x,1.092548*d.x*d.y,
   1.092548*d.y*d.z,.315392*(3*d.z*d.z-1),1.092548*d.x*d.z,.546274*(d.x*d.x-d.y*d.y)};
  for(uint c=0;c<9;++c)sh[c]+=radiance*b[c]*(2*kPi/float(rays));
 }
 const float band[9]={kPi,2.0943951,2.0943951,2.0943951,.785398,.785398,.785398,.785398,.785398};
 const float scale=vk::RawBufferLoad<float4>(a.ambient).w/max(a.skyScale,1e-6);
 for(uint c=0;c<9;++c)gStorageVolumes[NonUniformResourceIndex(push.destination)][uint3(pixel,c)]=float4(sh[c]*band[c]*scale,1);
}
float3 cubeDirection002(uint face,float2 uv){
 const float2 p=uv*2-1;
 if(face==0)return normalize(float3(1,-p.y,-p.x));if(face==1)return normalize(float3(-1,-p.y,p.x));
 if(face==2)return normalize(float3(p.x,1,p.y));if(face==3)return normalize(float3(p.x,-1,-p.y));
 if(face==4)return normalize(float3(p.x,-p.y,1));return normalize(float3(-p.x,-p.y,-1));
}
[numthreads(8,8,1)]
void CSCube(uint3 id:SV_DispatchThreadID){
 if(any(id.xy>=uint2(push.width,push.height))||id.z>=6)return;
 const MeshFrame f=loadFrame(push.frame);const AtmosphereFrame a=loadAtmosphere(f);const Cloud006Frame s=cloud006Frame(f);
 const uint period=s.sampling.w==0?16u:s.sampling.w==1?8u:s.sampling.w==2?4u:1u;
 if(s.settings.y!=0 && (s.settings.x+id.z)%period!=0)return;
 const float3 dir=cubeDirection002(id.z,(float2(id.xy)+.5)/float2(push.width,push.height));
 const float3 radiance=incident002(f,a,float3(0,0,0),dir,max(8u,s.settings.z/4u));
 gStorageArraysRgba16f[NonUniformResourceIndex(push.destination)][id]=float4(min(radiance,65000.0),1);
}
[numthreads(8,8,1)]
void CSWater(uint2 pixel:SV_DispatchThreadID){
 if(any(pixel>=uint2(push.width,push.height)))return;
 const MeshFrame f=loadFrame(push.frame);const AtmosphereFrame a=loadAtmosphere(f);const Cloud006Frame s=cloud006Frame(f);
 const float2 uv=(float2(pixel)+.5)/float2(push.width,push.height);
 const uint period=s.sampling.w==0?16u:s.sampling.w==1?8u:s.sampling.w==2?4u:1u;
 const float4 old=gTextures[NonUniformResourceIndex(s.extras.w)].SampleLevel(gSamplers[f.volumeSampler],uv,0);
 if(s.settings.y!=0 && (pixel.x+pixel.y*push.width)%period!=s.settings.x%period){
  gStorageImagesRgba16f[NonUniformResourceIndex(push.destination)][pixel]=old;return;
 }
 const float phi=(uv.x-.5)*2*kPi,theta=uv.y*kPi;
 const float3 dir=float3(cos(phi)*sin(theta),cos(theta),sin(phi)*sin(theta));
 const float3 raw=incident002(f,a,f.cameraPosition,dir,max(8u,s.settings.z/4u));
 float4 result=float4(min(raw,65000.0),1);
 if(s.settings.y!=0)result=lerp(result,clamp(old,result*.7,result*1.3+.001),.94);
 gStorageImagesRgba16f[NonUniformResourceIndex(push.destination)][pixel]=result;
}
