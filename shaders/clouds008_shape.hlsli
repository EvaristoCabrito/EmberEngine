// Independent cloud look 008. Existing looks remain selectable and unchanged.
// Nuvens RDR2 (2026-11-04; the user's pasted prompt "nuvens volumétricas 3D no estilo Red Dead Redemption 2, qualidade máxima"
// and the order "faz"): the cloud model of Rockstar's SIGGRAPH 2019 talk "Creating the Atmospheric World of Red Dead Redemption
// 2" (Fabian Bauer), built as the game's clouds. This file is the SHAPE (what a point of the sky is made of) and the SHADOW
// the layer throws on the board; cloudsvol_rdr2.hlsli lights it and marches it, clouds_rdr2.hlsl holds the compute passes.
//
// The pieces, as the talk has them:
//   weather map   a top-down 2D image over the world (CSWeather, 1024 x 1024 on Ultra): r = the low layer's coverage,
//                 g = its type (0 stratus .. 1 cumulus), b = the cirrus layer's coverage, a = vertical growth (towards the
//                 cumulonimbus, our own requirement)
//   height LUT    the profile of each type along the column (rdr2HeightProfile: stratus low and flat, cumulus tall and
//                 rounded, the cumulonimbus tower with its anvil), mixed continuously so a change of type never pops
//   base density  a smoothstep of the weather value, carved by the 3D Perlin-Worley body (the noise volumes cloudsvol.hlsl
//                 already makes: 128^3 Perlin-Worley + Worley, 32^3 Worley detail)
//   detail        the Worley detail eroding the edges, displaced by a 2D displacement field (xz and xy) and the wind; the
//                 edge's softness follows the type (the LUT's z channel in the talk)
//   cirrus        a high thin sheet at ~8 km made of a 2D density (streaks along the wind), lit with the same model
// Everything of the RDR2 clouds is read from its own record (Rdr2Frame, engine/render/CloudsRdr2.h) whose device address sits
// in the AtmosphereFrame at byte 512; every field is read by byte offset, one at a time (the DXC gotcha of structs passed by
// value, CLAUDE.md). MeshFrame.cloudFlags carries kCloudRdr2 (8) when this is the frame's cloud.
#ifndef EMBER_CLOUDS_RDR2_HLSLI
#define EMBER_CLOUDS_RDR2_HLSLI

#include "clouds.hlsli"

static const uint kCloudRdr2 = 8u;            // MeshFrame.cloudFlags: the RDR2 clouds are this frame's clouds
static const uint kRdr2RecordOffset = 512u;   // AtmosphereFrame: uint64 device address of the Rdr2Frame record

// The record's layout (CloudsRdr2.h Rdr2Frame, static_asserted there).
static const uint kR2WeatherMap = 0u;      // uint  sampled 2D: the weather map
static const uint kR2NoiseBase = 4u;       // uint  sampled 3D: Perlin-Worley body (cloudsvol.hlsl CSNoiseBase)
static const uint kR2NoiseDetail = 8u;     // uint  sampled 3D: Worley detail (CSNoiseDetail)
static const uint kR2SamplerRepeat = 12u;  // uint  linear, repeating
static const uint kR2SamplerClamp = 16u;   // uint  linear, clamped
static const uint kR2March = 20u;          // uint  sampled 2D: this frame's raw march (rgb radiance raw, a transmittance)
static const uint kR2History = 24u;        // uint  sampled 2D: last frame's resolved clouds (the temporal history)
static const uint kR2BlueNoise = 28u;      // uint  sampled 2D: 64 x 64 blue noise (r)
static const uint kR2Layer = 32u;          // float4: base, top, anvil top (the cumulonimbus's), cirrus height (world y)
static const uint kR2Weather = 48u;        // float4: low coverage, cirrus coverage, type (0 stratus .. 1 cumulus), growth (0 .. 1)
static const uint kR2Wind = 64u;           // float4: wind x, wind z (units / s), time (s), rain (0 .. 1: darker, denser)
static const uint kR2MarchInfo = 80u;      // float4: march width, march height, frame index, quality (0 ultra .. 3 low)
static const uint kR2PreviousViewProj = 96u;   // float4x4: last frame's view-projection (the reprojection)
static const uint kR2Steps = 160u;         // float4: march steps, light steps, scattering octaves, lightning (0 .. 1: how often)
static const uint kR2WorldRect = 176u;     // float4: the weather map's world rect: x0, z0, 1 / width, 1 / height
static const uint kR2Flash = 192u;         // float4: the lightning flash this frame: xyz world, w strength (0 = none)
static const uint kR2Screen = 208u;        // float4: screen width, screen height, history valid (1 / 0), mean ground y
static const uint kR2Shadow = 224u;        // float4: shadow rect x0, z0, 1 / width, 1 / height (over the board)
static const uint kR2ShadowInfo = 240u;    // float4: shadow map size, shadow strength, shadow height (world y), light steps
static const uint kR2Textures=272u, kR2Maps=288u, kR2Indirect=304u, kR2Temporal=320u,
                  kR2Middle=336u, kR2ProbeRect=352u, kR2Probes=368u, kR2Ambient=376u,
                  kR2Flashes=384u, kR2Controls=448u;
static const uint kR2BlueAddress = 256u;   // uint64: device address of the 64 x 64 blue noise (floats)

uint64_t rdr2Record(MeshFrame f) { return vk::RawBufferLoad<uint64_t>(f.atmosphere + kRdr2RecordOffset); }

struct Rdr2Clouds {
  uint weatherMap, noiseBase, noiseDetail, samplerRepeat, samplerClamp, march, history, blueNoise;
  float base, top, anvilTop, cirrusHeight;
  float coverage, cirrusCoverage, kind, growth;
  float2 wind;
  float time, rain;
  float4 worldRect;
  float3 flash;
  float flashStrength;
  float meanGround;
  uint heightMap, displacement, middleMap;
  float4 middle;
  uint64_t record;
};
Rdr2Clouds loadRdr2(MeshFrame f) {
  const uint64_t r = vk::RawBufferLoad<uint64_t>(f.atmosphere + kRdr2RecordOffset);
  Rdr2Clouds c;
  c.weatherMap = vk::RawBufferLoad<uint>(r + kR2WeatherMap);
  c.noiseBase = vk::RawBufferLoad<uint>(r + kR2NoiseBase);
  c.noiseDetail = vk::RawBufferLoad<uint>(r + kR2NoiseDetail);
  c.samplerRepeat = vk::RawBufferLoad<uint>(r + kR2SamplerRepeat);
  c.samplerClamp = vk::RawBufferLoad<uint>(r + kR2SamplerClamp);
  c.march = vk::RawBufferLoad<uint>(r + kR2March);
  c.history = vk::RawBufferLoad<uint>(r + kR2History);
  c.blueNoise = vk::RawBufferLoad<uint>(r + kR2BlueNoise);
  const float4 layer = vk::RawBufferLoad<float4>(r + kR2Layer);
  c.base = layer.x, c.top = layer.y, c.anvilTop = layer.z, c.cirrusHeight = layer.w;
  const float4 weather = vk::RawBufferLoad<float4>(r + kR2Weather);
  c.coverage = weather.x, c.cirrusCoverage = weather.y, c.kind = weather.z, c.growth = weather.w;
  const float4 wind = vk::RawBufferLoad<float4>(r + kR2Wind);
  c.wind = wind.xy, c.time = wind.z, c.rain = wind.w;
  c.worldRect = vk::RawBufferLoad<float4>(r + kR2WorldRect);
  const float4 flash = vk::RawBufferLoad<float4>(r + kR2Flash);
  c.flash = flash.xyz, c.flashStrength = flash.w;
  c.meanGround = vk::RawBufferLoad<float4>(r + kR2Screen).w;
  c.record=r;
  c.heightMap=vk::RawBufferLoad<uint4>(r+kR2Textures).w;
  const uint4 maps=vk::RawBufferLoad<uint4>(r+kR2Maps);
  c.displacement=maps.y;c.middleMap=maps.z;
  c.middle=vk::RawBufferLoad<float4>(r+kR2Middle);
  return c;
}

float rescale(float a, float b, float v) { return saturate((v - a) / max(b - a, 1e-5)); }

// --- Samples, each in its own helper with the coordinate finished first (the DXC gotcha of volumeSampleAt) ----------------
float4 rdr2WeatherSample(Rdr2Clouds c, float2 uv) {
  return gTextures[NonUniformResourceIndex(c.weatherMap)].SampleLevel(gSamplers[NonUniformResourceIndex(c.samplerClamp)], uv, 0);
}
float4 rdr2BodySample(Rdr2Clouds c, float3 uvw) {
  const uint image=vk::RawBufferLoad<uint>(c.record+kR2NoiseBase);
  const uint sampler=vk::RawBufferLoad<uint>(c.record+kR2SamplerRepeat);
  return gTextures3D[NonUniformResourceIndex(image)].SampleLevel(gSamplers[NonUniformResourceIndex(sampler)],uvw,0);
}
float4 rdr2DetailSample(Rdr2Clouds c, float3 uvw) {
  const uint image=vk::RawBufferLoad<uint>(c.record+kR2NoiseDetail);
  const uint sampler=vk::RawBufferLoad<uint>(c.record+kR2SamplerRepeat);
  return gTextures3D[NonUniformResourceIndex(image)].SampleLevel(gSamplers[NonUniformResourceIndex(sampler)],uvw,0);
}
float rdr2BlueNoise(MeshFrame f, uint2 pixel, uint frame) {
  // The 64 x 64 tile (a float per texel in the record's buffer), the frame's golden-ratio rotation on top (a different
  // offset every frame: the temporal pass converges).
  const uint64_t blue = vk::RawBufferLoad<uint64_t>(rdr2Record(f) + kR2BlueAddress);
  const uint index = (pixel.y & 63u) * 64u + (pixel.x & 63u);
  const float n = vk::RawBufferLoad<float>(blue + uint64_t(index) * 4u);
  return frac(n + 0.61803398875 * float(frame & 255u));
}

// --- The weather over a world point: the map's four channels, with the top-down clearing far from the world's origin ----------
// (the boards stand near the origin; the layer is drawn 9000 units out: the far field thins so the horizon band keeps its
// clearings for the low sun, the moon and the stars, and the camera's grazing view never closes into a ceiling)
float4 rdr2Weather(Rdr2Clouds c, float2 xz) {
  const float2 uv = (xz - c.worldRect.xy) * c.worldRect.zw;
  float4 w = rdr2WeatherSample(c, saturate(uv));
  const float farClear = 1.0 - smoothstep(4500.0, 9000.0, length(xz));
  w.r *= farClear;
  w.b *= lerp(0.6, 1.0, farClear);
  return w;
}

// --- The height LUT as a function: each type's profile along the column (h = 0 at the base .. 1 at the top), mixed by `kind`
// and lifted into a cumulonimbus by `growth` (the tower keeps its body to the top, the anvil spreads flat under the cap).
float rdr2HeightProfile(float h, float kind, float growth) {
  const float stratus = smoothstep(0.0, 0.07, h) * (1.0 - smoothstep(0.26, 0.42, h));
  const float cumulus = smoothstep(0.0, 0.11, h) * (1.0 - smoothstep(0.58, 0.98, h)) * lerp(0.75, 1.0, smoothstep(0.1, 0.4, h));
  const float altocumulus = smoothstep(0.3, 0.42, h) * (1.0 - smoothstep(0.6, 0.78, h)) * 0.7;
  float profile = lerp(stratus, cumulus, smoothstep(0.0, 1.0, kind));
  profile = max(profile, altocumulus * (1.0 - abs(kind - 0.5) * 2.0) * 0.6);
  if (growth > 0.0) {
    const float tower = smoothstep(0.0, 0.09, h) * (1.0 - smoothstep(0.84, 0.99, h)) * lerp(1.0, 0.7, smoothstep(0.5, 0.85, h));
    const float anvil = smoothstep(0.74, 0.88, h) * (1.0 - smoothstep(0.93, 0.995, h)) * 1.3;
    profile = lerp(profile, max(tower, anvil), growth);
  }
  return saturate(profile);
}
// The edge's softness by type (the LUT's z channel): stratus wispy and soft, cumulus crisp.
float rdr2EdgeSoftness(float kind, float h) { return lerp(0.55, 0.28, kind) * lerp(1.35, 0.85, saturate(h * 1.6)); }

// --- The displacement field (two samples of a 2D map, xz and xy planes in the talk; here the drifting 2D value noise) -------
float3 rdr2Displace(Rdr2Clouds c, float3 p, float scale) {
  const float2 q1=frac((p.xz+c.wind*c.time*.35)/scale);
  const float2 q2=frac((p.xy+float2(c.wind.x*c.time*.2,0))/scale);
  const float2 d0=gTextures[NonUniformResourceIndex(c.displacement)].SampleLevel(gSamplers[c.samplerRepeat],q1,0).rg;
  const float2 d1=gTextures[NonUniformResourceIndex(c.displacement)].SampleLevel(gSamplers[c.samplerRepeat],q2,0).rg;
  return float3(d0.x,d1.x*.5,d0.y)*scale*.45;
}

// --- The low layer's column: its local top (the cumulonimbus climbs above the layer's top) and relative height -------------
float rdr2ColumnTop(Rdr2Clouds c, float growthHere) { return lerp(c.top, c.anvilTop, growthHere); }

// The density at a world point, 0 .. 1. `cheap` = no detail erosion (the light march, the shadows, the reflection map's
// march); `lod` 0 near .. 1 far (the fine erosion fades out before the coarse one: far clouds sampled at long steps alias
// into grain otherwise).

// Isotropic 3D gradient noise and cellular cloud bodies. No height-aligned texture slices.
float3 cloud008Hash(int3 p){
  uint n=asuint(p.x)*1597334677u^asuint(p.y)*3812015801u^asuint(p.z)*2798796415u;
  n^=n>>16;n*=2246822519u;n^=n>>13;n*=3266489917u;n^=n>>16;
  return float3(n&1023u,(n>>10)&1023u,(n>>20)&1023u)/1023.0;
}
float cloud008Perlin(float3 p){
  const int3 cell=int3(floor(p));const float3 f=frac(p);
  const float3 u=f*f*f*(f*(f*6-15)+10);
  float values[8];
  [unroll]for(uint i=0;i<8;++i){
    const int3 d=int3(i&1u,(i>>1)&1u,(i>>2)&1u);
    const float3 g=normalize(cloud008Hash(cell+d)*2-1+float3(.001,.002,.003));
    values[i]=dot(g,f-float3(d));
  }
  return lerp(lerp(lerp(values[0],values[1],u.x),lerp(values[2],values[3],u.x),u.y),
              lerp(lerp(values[4],values[5],u.x),lerp(values[6],values[7],u.x),u.y),u.z);
}
float rdr2LayerDensity(Rdr2Clouds c,float3 p,bool cheap,float lod,bool middle){
  const float base=middle?c.middle.x:c.base;
  const float upper=middle?c.middle.y:lerp(c.top,c.anvilTop,c.growth);
  const float thickness=max(upper-base,1);
  const float h=(p.y-base)/thickness;
  if(h<=0 || h>=1)return 0;
  const float kind=middle?.5:c.kind;
  const float spacing=middle?680:1050;
  const float2 drift=c.wind*c.time;
  const float2 world=p.xz+drift;
  const int2 tile=int2(floor(world/spacing));
  float body=0;
  [loop]for(int z=-1;z<=1;++z)[loop]for(int x=-1;x<=1;++x){
    const int2 id=tile+int2(x,z);
    const float3 random=cloud008Hash(int3(id.x,middle?71:19,id.y));
    const float2 centre=(float2(id)+.5+(random.xy-.5)*.72)*spacing;
    const float2 delta=world-centre;
    const float radius=spacing*(.38+random.z*.22);
    if(any(abs(delta)>radius*1.35))continue;
    const float2 weatherUv=(centre-drift-c.worldRect.xy)*c.worldRect.zw;
    const float cover=middle?
      gTextures[NonUniformResourceIndex(c.middleMap)].SampleLevel(gSamplers[c.samplerClamp],saturate(weatherUv),0).r:
      rdr2WeatherSample(c,saturate(weatherUv)).r;
    if(cover<=.004)continue;
    const float height=thickness*lerp(.18,.48,kind)*(.72+random.y*.28);
    const float centreY=base+height*.94;
    const float scale=smoothstep(0,.7,cover);
    const float3 q=float3(delta.x/max(radius*scale,1),
      (p.y-centreY)/max(height*scale,1),delta.y/max(radius*scale*(.8+random.x*.4),1));
    float distance=length(q*float3(1,1.55,1));
    [unroll]for(int lobe=0;lobe<4;++lobe){
      const float3 l=cloud008Hash(int3(id.x*7+lobe,37,id.y*11-lobe));
      const float3 centreLobe=float3((l.x-.5)*1.35,.2+l.y*.45,(l.z-.5)*1.25);
      const float size=.38+l.z*.32;
      const float d=length((q-centreLobe)/float3(size,size*(.9+l.y*.6),size));
      const float blend=saturate(.5+.5*(d-distance)/.22);
      distance=lerp(d,distance,blend)-.22*blend*(1-blend);
    }
    if(distance>1.3)continue;
    float erosion=0;
    if(true){
      const float3 warped=float3(world.x,p.y,world.y)+rdr2Displace(c,p,900)*.12;
      const float3 n=warped/145;
      const float perlin=cloud008Perlin(n)+cloud008Perlin(n.yzx*2.03+13.7)*.38;
      erosion=perlin*.72;
      const float3 bodyUv=warped/1050;
      const float4 volumeNoise=rdr2BodySample(c,bodyUv);
      erosion+=((volumeNoise.r+volumeNoise.g)*.5-.5)*.08;
      // Fine detail fades with distance, with no change of material between presets.
      if(!cheap && lod<.6)erosion+=cloud008Perlin(n.zxy*5.11+29.1)*.1*(1-lod);
    }
    const float d=1-smoothstep(.86,1.06,distance+erosion);
    body=max(body,d);
  }
  const float4 profile=gTextures[NonUniformResourceIndex(c.heightMap)].SampleLevel(gSamplers[c.samplerClamp],float2(.5,h),0);
  // The LUT keeps a flat water-droplet base and morphs the vertical character continuously.
  const float shaped=body*lerp(1,profile.w,.28)*smoothstep(0,.045,h);
  return saturate(shaped)*lerp(.82,1.08,c.rain);
}

float rdr2Density(Rdr2Clouds c,float3 p,bool cheap,float lod) {
  return saturate(rdr2LayerDensity(c,p,cheap,lod,false)+rdr2LayerDensity(c,p,cheap,lod,true));
}

// Extinction per world unit at density 1 (a unit is ~25 m of air: a cumulus is opaque within a few units; rain thickens it).
float rdr2Extinction(Rdr2Clouds c) { return 0.035 * lerp(1.0, 1.6, c.rain); }

// --- The cirrus sheet: a 2D density at the cirrus height, streaks along the wind, thin (optical depth under 1) ---------------
float rdr2Cirrus(Rdr2Clouds c, float2 xz) {
  const float4 w = rdr2Weather(c, xz);
  if (w.b <= 0.004) return 0.0;
  const float2 along = normalize(c.wind + float2(1e-3, 0.0));
  const float2 across = float2(-along.y, along.x);
  const float2 q = xz + c.wind * c.time * 2.2;
  // Anisotropic streaks: fine across the wind, long along it.
  const float2 s = float2(dot(q, along) / 2600.0, dot(q, across) / 520.0);
  const float streak = cloudFbm(s + 3.7) * 0.6 + cloudNoise(s * float2(3.1, 2.3) + 11.2) * 0.4;
  const float threshold = 1.0 - w.b;
  return smoothstep(threshold - 0.08, threshold + 0.3, streak) * w.b;
}

// --- The shadow the layer throws: the optical depth along the light from `position` up through the low layer (cheap density,
// `taps` samples spaced quadratically: the first ones near the base, where the light leaves the cloud) and the cirrus sheet.
float rdr2LightDepthFrom(Rdr2Clouds c, float3 position, float3 toLight, uint taps, float dither) {
  if (abs(toLight.y)<0.005) return 0.0;
  const float upper=max(c.anvilTop,c.middle.y);
  const float enter=(c.base-position.y)/toLight.y, leave=(upper-position.y)/toLight.y;
  const float t0=max(min(enter,leave),0.0),t1=max(max(enter,leave),0.0);
  const float span = t1 - t0;
  float depth = 0.0;
  for (uint i = 0u; i < taps; ++i) {
    const float u0 = float(i) / float(taps), u1 = float(i + 1u) / float(taps);
    const float from = span * u0 * u0, to = span * u1 * u1;
    const float at = t0 + lerp(from,to,dither);  // v02: the whole tap interval, a new place per tap (v01: +-15 % around the middle, banded)
    const float3 lightPosition=position+toLight*at;
    depth += rdr2Density(c,lightPosition,true,0.0)*(to-from);
  }
  depth *= rdr2Extinction(c);
  // The cirrus sheet above, one thin sample.
  const float tc = (c.cirrusHeight - position.y) / toLight.y;
  if (tc > 0.0) depth += rdr2Cirrus(c, position.xz + toLight.xz * tc) * 0.35;
  return depth;
}

#endif
