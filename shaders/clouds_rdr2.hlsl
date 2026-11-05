// RDR2 cloud completion: all consumers share density, illumination and optical integration.
#include "cloudsvol_rdr2.hlsli"
struct Push { uint64_t frame; uint destination,width,height,source,mode; };
[[vk::push_constant]] Push push;
float4 r2Read(uint image,uint sampler,float2 uv) {
  return gTextures[NonUniformResourceIndex(image)].SampleLevel(gSamplers[NonUniformResourceIndex(sampler)],uv,0);
}
[numthreads(8,8,1)]
void CSWeather(uint2 pixel:SV_DispatchThreadID) {
  if(any(pixel>=uint2(push.width,push.height)))return;
  const MeshFrame f=loadFrame(push.frame);const Rdr2Clouds c=loadRdr2(f);
  const float2 xz=c.worldRect.xy+(float2(pixel)+.5)/float2(push.width,push.height)/c.worldRect.zw;
  const float2 drift=c.wind*c.time;
  const float front=cloudFbm((xz+drift*.6)/4200+5.1);
  const float cells=cloudFbm((xz+drift)/920+1.7);
  const float n=saturate((cells-.18)/.62),threshold=1-saturate(c.coverage);
  float cover=smoothstep(threshold-.14,threshold+.24,n)*saturate(c.coverage*8);
  cover*=lerp(smoothstep(.22,.62,front),1,saturate(c.coverage*1.3));
  const float kind=saturate(c.kind+(cloudNoise((xz+drift*.4)/3400+23.1)-.5)*.55);
  const float cirrusField=cloudFbm((xz+drift*2)/5200+43.3);
  const float cirrus=smoothstep(1-c.cirrusCoverage-.2,1-c.cirrusCoverage+.2,saturate((cirrusField-.2)/.6))*c.cirrusCoverage;
  const float growth=smoothstep(.5,.92,cover*(.5+.5*cloudNoise((xz+drift*.8)/1500+61.7)));
  gStorageImagesRgba16f[NonUniformResourceIndex(push.destination)][pixel]=float4(cover,kind,cirrus,growth);
  const float m=cloudFbm((xz+drift*.8)/1700+79.7);
  const float middle=smoothstep(1-c.middle.z-.12,1-c.middle.z+.23,saturate((m-.18)/.62))*saturate(c.middle.z*8);
  const uint middleStorage=push.source;
  gStorageImagesRgba16f[NonUniformResourceIndex(middleStorage)][pixel]=float4(middle,0,0,1);
}
[numthreads(8,8,1)]
void CSAtlas(uint2 pixel:SV_DispatchThreadID) {
  if(any(pixel>=uint2(push.width,push.height)))return;
  const float h=(float(pixel.x)+.5)/128;
  const float kind=pixel.y==0?0:pixel.y==1?.5:1;
  const float profile=rdr2HeightProfile(h,kind,pixel.y==3?1:0);
  gStorageImagesRgba16f[NonUniformResourceIndex(push.destination)][pixel]=float4(1-profile,1-profile+.25,rdr2EdgeSoftness(kind,h),profile);
}
[numthreads(8,8,1)]
void CSHeight(uint2 pixel:SV_DispatchThreadID) {
  if(any(pixel>=uint2(push.width,push.height)))return;
  const MeshFrame f=loadFrame(push.frame);const Rdr2Clouds c=loadRdr2(f);
  const uint atlas=vk::RawBufferLoad<uint4>(c.record+kR2Maps).x;
  const float h=(float(pixel.y)+.5)/128;
  const float row=saturate(c.kind)*2;
  const float2 uv=float2(h,(row+.5)/4);
  const float4 profile=r2Read(atlas,c.samplerClamp,uv);
  gStorageImagesRgba16f[NonUniformResourceIndex(push.destination)][pixel]=profile;
}
[numthreads(8,8,1)]
void CSDisplacement(uint2 pixel:SV_DispatchThreadID) {
  if(any(pixel>=uint2(push.width,push.height)))return;
  // Periodic Fourier field: texture wrapping has no discontinuity at the tile seam.
  const float2 q=(float2(pixel)+.5)/float2(push.width,push.height)*6.2831853;
  const float x=sin(q.x*2+sin(q.y*3))*.55+sin(q.x*5-q.y*4)*.3+sin(q.y*9+q.x*7)*.15;
  const float y=cos(q.y*2+cos(q.x*3))*.55+cos(q.y*5+q.x*4)*.3+cos(q.x*9-q.y*7)*.15;
  gStorageImagesRgba16f[NonUniformResourceIndex(push.destination)][pixel]=float4(x,y,0,1);
}
groupshared float3 r2Ambient[64],r2Weighted[64];
groupshared float r2Weight[64];
[numthreads(64,1,1)]
void CSAmbient(uint thread:SV_GroupIndex) {
  const MeshFrame f=loadFrame(push.frame);const AtmosphereFrame a=loadAtmosphere(f);
  const Rdr2Clouds c=loadRdr2(f);
  const float3 key=a.sunDirection;
  float3 up=0,weighted=0;float weights=0;
  for(uint i=thread;i<512;i+=64) {
    const float y=(float(i)+.5)/512,phi=float(i)*2.39996323;
    const float3 d=float3(cos(phi)*sqrt(1-y*y),y,sin(phi)*sqrt(1-y*y));
    const float3 L=skyRadiance(a,d)*a.skyScale;
    const float w=.25+.75*pow(saturate(dot(d,key)),2);
    up+=L*y*(2*kPi/512);weighted+=L*w;weights+=w;
  }
  r2Ambient[thread]=up;r2Weighted[thread]=weighted;r2Weight[thread]=weights;
  GroupMemoryBarrierWithGroupSync();
  for(uint stride=32;stride>0;stride>>=1) {
    if(thread<stride){r2Ambient[thread]+=r2Ambient[thread+stride];r2Weighted[thread]+=r2Weighted[thread+stride];r2Weight[thread]+=r2Weight[thread+stride];}
    GroupMemoryBarrierWithGroupSync();
  }
  if(thread==0) {
    const uint64_t dst=vk::RawBufferLoad<uint64_t>(c.record+kR2Ambient);
    vk::RawBufferStore<float4>(dst,float4(r2Ambient[0],1));
    vk::RawBufferStore<float4>(dst+16u,float4(r2Weighted[0]/max(r2Weight[0],1e-5)*kPi,1));
  }
}
[numthreads(8,8,1)]
void CSMarch(uint2 pixel:SV_DispatchThreadID) {
  if(any(pixel>=uint2(push.width,push.height)))return;
  const MeshFrame f=loadFrame(push.frame);const AtmosphereFrame a=loadAtmosphere(f);const Rdr2Clouds c=loadRdr2(f);
  const float4 info=vk::RawBufferLoad<float4>(c.record+kR2MarchInfo);
  const float4 temporal=vk::RawBufferLoad<float4>(c.record+kR2Temporal);
  const uint stride=vk::RawBufferLoad<uint4>(c.record+kR2Indirect).w;
  const uint2 tile=pixel/stride;
  const uint phase=(uint(info.z)+((tile.x+tile.y)&1u))%(stride*stride);
  const bool trace=temporal.y<.5 || stride==1 || pixel.x%stride+(pixel.y%stride)*stride==phase;
  float4 result=float4(0,0,0,-1);float depth=0;
  if(trace) {
    const float2 uv=(float2(pixel)+.5)/float2(push.width,push.height);
    const float3 dir=skyDirection(f,a,uv*f.screenSize);
    const float4 steps=vk::RawBufferLoad<float4>(c.record+kR2Steps);
    const Rdr2Hit hit=rdr2March(f,a,c,f.cameraPosition,dir,skyRadiance(a,dir)*a.skyScale,uint(steps.x),uint(steps.y),3u,
                                rdr2BlueNoise(f,pixel,uint(info.z)),uv);
    result=float4(hit.color,hit.transmittance);depth=hit.depth;
  }
  gStorageImagesRgba16f[NonUniformResourceIndex(push.destination)][pixel]=result;
  gStorageImagesRgba16f[NonUniformResourceIndex(push.source)][pixel]=float4(depth,0,0,1);
}
[numthreads(8,8,1)]
void CSResolve(uint2 pixel:SV_DispatchThreadID) {
  if(any(pixel>=uint2(push.width,push.height)))return;
  const MeshFrame f=loadFrame(push.frame);const AtmosphereFrame a=loadAtmosphere(f);const Rdr2Clouds c=loadRdr2(f);
  const uint4 textures=vk::RawBufferLoad<uint4>(c.record+kR2Textures);
  const float4 temporal=vk::RawBufferLoad<float4>(c.record+kR2Temporal);
  const float2 size=float2(push.width,push.height),uv=(float2(pixel)+.5)/size;
  float4 current=gTextures[NonUniformResourceIndex(c.march)].Load(int3(pixel,0));
  float depth=gTextures[NonUniformResourceIndex(textures.x)].Load(int3(pixel,0)).r;
  float4 lo=1e25,hi=-1e25,sum=0;float count=0,depthSum=0;
  float closest=1e25;float4 nearest=float4(0,0,0,1);float nearestDepth=0;
  [unroll]for(int y=-1;y<=1;++y)[unroll]for(int x=-1;x<=1;++x) {
    const int2 q=clamp(int2(pixel)+int2(x,y),int2(0,0),int2(push.width-1,push.height-1));
    const float4 n=gTextures[NonUniformResourceIndex(c.march)].Load(int3(q,0));
    if(n.a<0)continue;
    const float d=gTextures[NonUniformResourceIndex(textures.x)].Load(int3(q,0)).r;
    if(depth>0 && abs(d-depth)>max(40,depth*.08))continue;
    lo=min(lo,n);hi=max(hi,n);sum+=n;depthSum+=d;++count;
    const float delta=depth>0?abs(d-depth):float(x*x+y*y);
    if(delta<closest){closest=delta;nearest=n;nearestDepth=d;}
  }
  const bool traced=current.a>=0;
  if(!traced){current=nearest;depth=nearestDepth;}
  if(temporal.y>.5 && temporal.w<.5) {
    const float3 dir=skyDirection(f,a,uv*f.screenSize);
    const float priorAtPixel=r2Read(textures.y,c.samplerClamp,uv).r;
    const float distance=depth>0?depth:max(priorAtPixel,9000);
    const float3 world=f.cameraPosition+dir*distance+float3(c.wind.x,0,c.wind.y)*temporal.x;
    const float4x4 previous=vk::RawBufferLoad<float4x4>(c.record+kR2PreviousViewProj);
    const float4 clip=mul(float4(world,1),previous);
    const float2 previousUv=clip.xy/max(clip.w,1e-5)*.5+.5;
    if(clip.w>0 && all(previousUv>=0) && all(previousUv<=1)) {
      const float oldDepth=r2Read(textures.y,c.samplerClamp,previousUv).r;
      const float4 old=r2Read(c.history,c.samplerClamp,previousUv);
      const bool compatible=(depth==0&&oldDepth==0) || (depth>0&&oldDepth>0&&abs(oldDepth-depth)<max(60,depth*.1));
      if(compatible && abs(old.a-current.a)<.25) {
        const float4 history=count>0?clamp(old,lo,hi):old;
        const float motion=length(previousUv-uv)*max(size.x,size.y);
        const float weight=traced?lerp(.92,.55,saturate(motion/12)):1;
        current=lerp(current,history,weight);
        if(depth<=0)depth=oldDepth;
      }
    }
  }
  gStorageImagesRgba16f[NonUniformResourceIndex(push.destination)][pixel]=current;
  gStorageImagesRgba16f[NonUniformResourceIndex(push.source)][pixel]=float4(depth,0,0,1);
}
[numthreads(8,8,1)]
void CSShadow(uint2 pixel:SV_DispatchThreadID) {
  if(any(pixel>=uint2(push.width,push.height)))return;
  const MeshFrame f=loadFrame(push.frame);const Rdr2Clouds c=loadRdr2(f);
  const float4 rect=vk::RawBufferLoad<float4>(c.record+kR2Shadow);
  const float4 info=vk::RawBufferLoad<float4>(c.record+kR2ShadowInfo);
  const float4 time=vk::RawBufferLoad<float4>(c.record+kR2Temporal);
  const uint frame=uint(vk::RawBufferLoad<float4>(c.record+kR2MarchInfo).z);
  const uint quality=uint(vk::RawBufferLoad<float4>(c.record+kR2MarchInfo).w);
  // Ultra refreshes every texel. Other presets update a 4x4 tile across sixteen frames after initialization.
  if(time.y>.5 && quality>0 && ((pixel.x&3u)+((pixel.y&3u)*4u))!=(frame&15u))return;
  const float2 xz=rect.xy+(float2(pixel)+.5)/float2(push.width,push.height)/rect.zw;
  const float3 light=normalize(f.sunDirection);
  float moment=exp(8.0);
  if(light.y>.01) {
    const float upper=max(c.anvilTop,c.middle.y),span=(upper-info.z)/light.y;
    const float3 origin=float3(xz.x,info.z,xz.y)+light*span;
    const uint steps=max(uint(info.w)*2u,16u);
    const float dt=span/float(steps);
    float transmission=1;moment=0;
    for(uint i=0;i<steps;++i) {
      const float t=(float(i)+.5)*dt;
      const float3 p=origin-light*t;
      const float slab=exp(-rdr2Density(c,p,true,0)*rdr2Extinction(c)*dt);
      const float weight=transmission*(1-slab);
      moment+=weight*exp(8*saturate((upper-p.y)/max(upper-info.z,1)));
      transmission*=slab;
    }
    const float tc=(c.cirrusHeight-info.z)/light.y;
    const float cirrusT=exp(-rdr2Cirrus(c,float2(xz+light.xz*tc))*.35);
    // Cirrus acts above the low-cloud domain; its missing weight is at the ESM's near boundary.
    moment=(moment+transmission*exp(8.0))*cirrusT+(1-cirrusT);
  }
  gStorageImagesR16f[NonUniformResourceIndex(push.destination)][pixel]=moment;
}
[numthreads(8,8,1)]
void CSShadowBlur(uint2 pixel:SV_DispatchThreadID) {
  if(any(pixel>=uint2(push.width,push.height)))return;
  const Rdr2Clouds c=loadRdr2(loadFrame(push.frame));
  const float2 uv=(float2(pixel)+.5)/float2(push.width,push.height);
  const float2 step=push.mode==0?float2(1.0/push.width,0):float2(0,1.0/push.height);
  static const float weights[5]={.227027027,.194594595,.121621622,.054054054,.016216216};
  float sum=r2Read(push.source,c.samplerClamp,uv).r*weights[0];
  [unroll]for(int i=1;i<5;++i)sum+=(r2Read(push.source,c.samplerClamp,uv+step*i).r+r2Read(push.source,c.samplerClamp,uv-step*i).r)*weights[i];
  gStorageImagesR16f[NonUniformResourceIndex(push.destination)][pixel]=sum;
}
[numthreads(8,8,1)]
void CSShadowMip(uint2 pixel:SV_DispatchThreadID) {
  if(any(pixel>=uint2(push.width,push.height)))return;
  const Rdr2Clouds c=loadRdr2(loadFrame(push.frame));
  const float2 uv=(float2(pixel)+.5)/float2(push.width,push.height);
  const float2 offset=.25/float2(push.width,push.height);
  float sum=0;
  [unroll]for(int y=-1;y<=1;y+=2)[unroll]for(int x=-1;x<=1;x+=2)
    sum+=gTextures[NonUniformResourceIndex(push.source)].SampleLevel(gSamplers[c.samplerClamp],uv+float2(x,y)*offset,push.mode).r;
  gStorageImagesR16f[NonUniformResourceIndex(push.destination)][pixel]=sum*.25;
}
float3 r2Incident(MeshFrame f,AtmosphereFrame a,Rdr2Clouds c,float3 origin,float3 dir,uint steps) {
  if(dir.y<=.003) {
    const Rdr2Lights l=rdr2Lights(f,a);
    return a.groundAlbedo*(l.skyUp+(l.sun*saturate(l.sunDir.y)+l.moon*saturate(l.moonDir.y)))/kPi;
  }
  const float3 clear=skyRadiance(a,dir)*a.skyScale;
  const Rdr2Hit hit=rdr2March(f,a,c,origin,dir,clear,steps,4u,3u,.5,float2(.5,.5),false,true);
  return clear*hit.transmittance+hit.color;
}
[numthreads(8,8,1)]
void CSProbes(uint2 pixel:SV_DispatchThreadID) {
  if(any(pixel>=32u))return;
  const MeshFrame f=loadFrame(push.frame);const AtmosphereFrame a=loadAtmosphere(f);const Rdr2Clouds c=loadRdr2(f);
  const float4 temporal=vk::RawBufferLoad<float4>(c.record+kR2Temporal);
  const uint serial=uint(vk::RawBufferLoad<float4>(c.record+kR2MarchInfo).z),period=uint(temporal.z);
  const uint id=pixel.x+pixel.y*32;
  if(temporal.y>.5 && id%period!=serial%period)return;
  const float4 rect=vk::RawBufferLoad<float4>(c.record+kR2ProbeRect);
  const float2 xz=rect.xy+(float2(pixel)+.5)/32/rect.zw;
  float3 sh[9];[unroll]for(uint j=0;j<9;++j)sh[j]=0;
  const uint rays=32;
  const uint steps=uint(vk::RawBufferLoad<float4>(c.record+kR2Steps).x)/2u;
  for(uint i=0;i<rays;++i) {
    const float y=1-2*(float(i)+.5)/float(rays),phi=float(i)*2.39996323;
    const float3 d=float3(cos(phi)*sqrt(1-y*y),y,sin(phi)*sqrt(1-y*y));
    const float3 radiance=r2Incident(f,a,c,float3(xz.x,c.meanGround,xz.y),d,steps)/max(a.skyScale,1e-6);
    const float b[9]={.282095,.488603*d.y,.488603*d.z,.488603*d.x,1.092548*d.x*d.y,
      1.092548*d.y*d.z,.315392*(3*d.z*d.z-1),1.092548*d.x*d.z,.546274*(d.x*d.x-d.y*d.y)};
    [unroll]for(uint j=0;j<9;++j)sh[j]+=radiance*b[j]*(4*kPi/float(rays));
  }
  static const float band[9]={kPi,2.0943951,2.0943951,2.0943951,.785398,.785398,.785398,.785398,.785398};
  const uint64_t buffer=vk::RawBufferLoad<uint64_t>(c.record+kR2Probes)+uint64_t(id)*144u;
  [unroll]for(uint j=0;j<9;++j)vk::RawBufferStore<float4>(buffer+j*16u,float4(sh[j]*band[j],1));
}
float3 r2CubeDirection(uint face,float2 uv) {
  const float2 p=uv*2-1;
  if(face==0)return normalize(float3(1,-p.y,-p.x));if(face==1)return normalize(float3(-1,-p.y,p.x));
  if(face==2)return normalize(float3(p.x,1,p.y));if(face==3)return normalize(float3(p.x,-1,-p.y));
  if(face==4)return normalize(float3(p.x,-p.y,1));return normalize(float3(-p.x,-p.y,-1));
}
[numthreads(8,8,1)]
void CSCube(uint3 pixel:SV_DispatchThreadID) {
  if(any(pixel.xy>=uint2(push.width,push.height))||pixel.z>=6)return;
  const MeshFrame f=loadFrame(push.frame);const AtmosphereFrame a=loadAtmosphere(f);const Rdr2Clouds c=loadRdr2(f);
  const float4 temporal=vk::RawBufferLoad<float4>(c.record+kR2Temporal);
  const uint serial=uint(vk::RawBufferLoad<float4>(c.record+kR2MarchInfo).z),period=uint(temporal.z);
  if(temporal.y>.5 && (pixel.x+pixel.y*push.width+pixel.z)%period!=serial%period)return;
  const float3 dir=r2CubeDirection(pixel.z,(float2(pixel.xy)+.5)/float2(push.width,push.height));
  const uint steps=uint(vk::RawBufferLoad<float4>(c.record+kR2Steps).x)/2u;
  const float3 radiance=r2Incident(f,a,c,f.cameraPosition,dir,steps)/max(a.skyScale,1e-6);
  gStorageArraysRgba16f[NonUniformResourceIndex(push.destination)][pixel]=float4(min(radiance,65000),1);
}
[numthreads(8,8,1)]
void CSWater(uint2 pixel:SV_DispatchThreadID) {
  if(any(pixel>=uint2(push.width,push.height)))return;
  const MeshFrame f=loadFrame(push.frame);const AtmosphereFrame a=loadAtmosphere(f);const Rdr2Clouds c=loadRdr2(f);
  const float4 temporal=vk::RawBufferLoad<float4>(c.record+kR2Temporal);
  const uint serial=uint(vk::RawBufferLoad<float4>(c.record+kR2MarchInfo).z),period=uint(temporal.z);
  const float2 uv=(float2(pixel)+.5)/float2(push.width,push.height);
  const uint previous=vk::RawBufferLoad<uint4>(c.record+kR2Indirect).y;
  const float4 old=r2Read(previous,c.samplerClamp,uv);
  if(temporal.y>.5 && (pixel.x+pixel.y*push.width)%period!=serial%period) {
    gStorageImagesRgba16f[NonUniformResourceIndex(push.destination)][pixel]=old;return;
  }
  const float phi=(uv.x-.5)*2*kPi,theta=uv.y*kPi;
  const float3 dir=float3(cos(phi)*sin(theta),cos(theta),sin(phi)*sin(theta));
  const uint steps=uint(vk::RawBufferLoad<float4>(c.record+kR2Steps).x)/2u;
  const float3 radiance=r2Incident(f,a,c,f.cameraPosition,dir,steps)/max(a.skyScale,1e-6);
  float4 result=float4(min(radiance,65000),1);
  if(temporal.y>.5 && temporal.w<.5)result=lerp(result,clamp(old,result*.7,result*1.3+.001),.94);
  gStorageImagesRgba16f[NonUniformResourceIndex(push.destination)][pixel]=result;
}
