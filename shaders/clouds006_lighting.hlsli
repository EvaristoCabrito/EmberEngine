#ifndef EMBER_CLOUDS002_LIGHTING
#define EMBER_CLOUDS002_LIGHTING
float cloud006OpticalDepth(MeshFrame f,float3 p,float3 light,uint steps){
 const Cloud006Frame s=cloud006Frame(f);
 const float boundary=light.y>0?s.low.x+s.low.y*2.5:s.low.x;
 const float span=clamp((boundary-p.y)/(abs(light.y)>.01?light.y:.01),0,s.low.y*8);
 float optical=0;
 for(uint i=0;i<steps;++i){
  const float a=float(i)/float(steps),b=float(i+1)/float(steps);
  const float lo=span*a*a,hi=span*b*b;
  optical+=cloud006Density(f,p+light*((lo+hi)*.5),true)*(hi-lo)*.035;
 }
 return optical;
}
float3 cloud006Source(MeshFrame f,AtmosphereFrame a,float3 p,float3 dir,float density){
 const Cloud006Frame s=cloud006Frame(f);
 const float3 key=a.sunDirection;
 const float optical=cloud006OpticalDepth(f,p,key,s.settings.w);
 const float3 keyLight=a.sunIlluminance*transmittanceToTop(a,a.viewHeight,key.y);
 float3 direct=keyLight*cloud006Phase(dot(dir,key),optical,density*.035,.8);
 const float3 moon=atmoMoonDirection(f.atmosphere),moonLight=atmoMoonLight(f.atmosphere);
 if(dot(moonLight,1)>0 && a.sunDisc!=0)
  direct+=moonLight*transmittanceToTop(a,a.viewHeight,moon.y)*cloud006Phase(dot(dir,moon),cloud006OpticalDepth(f,p,moon,s.settings.w),density*.035,.8);
 const float3 sky=float3(vk::RawBufferLoad<float4>(a.ambient+16).w,vk::RawBufferLoad<float4>(a.ambient+32).w,vk::RawBufferLoad<float4>(a.ambient+48).w);
 const float h=saturate((p.y-s.low.x)/max(s.low.y,1));
 const float3 fill=sky*lerp(.12,.8,h)*lerp(.35,1,exp(-optical*.15))/3.14159265;
 // The Map Editor's "Tom cinzento" (AtmosphereFrame.cloudGray, byte 300; 2026-11-04): 0 white .. 1 dark grey, as every look.
 return (direct+fill)*.96*a.skyScale*lerp(1.0,0.3,saturate(vk::RawBufferLoad<float>(f.atmosphere+300u)));
}
float4 cloud006March(MeshFrame f,AtmosphereFrame a,float3 origin,float3 dir,uint steps,float jitter,out float depth,bool perspective=true){
 const Cloud006Frame s=cloud006Frame(f);depth=0;
 if(dir.y<=.003)return float4(0,0,0,1);
 const float start=max(160,(s.low.x-origin.y)/dir.y);
 const float end=min(18000,(s.low.x+s.low.y*2.5-origin.y)/dir.y);
 if(end<=start)return float4(0,0,0,1);
 float t=start;float3 colour=0;float transmission=1,weight=0,weightedDepth=0;
 const float coarse=max((end-start)/max(float(steps)*.5,1),1);
 float dt=coarse;uint empty=0;
 for(uint i=0;i<steps&&t<end;++i){
  const float span=min(max(dt,(end-t)/max(float(steps-i),1)),end-t);const float3 p=origin+dir*(t+span*jitter);
  const float base=cloud006Density(f,p,true);
  if(base>.001 && dt>coarse*.251){dt*=.5;continue;}
  const float density=base>.001?cloud006Density(f,p,false):0;
  if(density>.0001){
   const float slab=exp(-density*.035*span),w=transmission*(1-slab);
   colour+=w*cloud006Source(f,a,p,dir,density);weightedDepth+=(t+span*.5)*w;weight+=w;transmission*=slab;empty=0;
  }else if(++empty>=3)dt=min(dt*2,coarse);
  t+=span;if(transmission<.005)break;
 }
 depth=weight>0?weightedDepth/weight:0;
 // Read the existing atmosphere at the transmittance-weighted cloud depth once.
 if(perspective && weight>0 && a.kmPerUnit>0){
  const float4 clip=mul(float4(origin+dir*depth,1),f.viewProjection);
  const float3 uvw=saturate(float3(clip.xy/max(clip.w,1e-5)*.5+.5,depth*.001/a.aerialMaxKm-.5/32));
  const float4 air=gTextures3D[NonUniformResourceIndex(a.aerial)].SampleLevel(gSamplers[a.linearSampler],uvw,0);
  colour=colour*air.a+air.rgb*a.skyScale*(1-transmission);
 }
 return float4(colour,transmission);
}
#endif
