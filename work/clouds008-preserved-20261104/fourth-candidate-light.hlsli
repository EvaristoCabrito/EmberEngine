// Independent cloud look 008. Existing looks remain selectable and unchanged.
// Nuvens RDR2: the LIGHT and the MARCH (Bauer, SIGGRAPH 2019; clouds_rdr2.hlsli holds the shape). What a sample of cloud does
// with the light, as the talk has it:
//   scattering     single scattering with one transmittance for every colour (Beer: T = exp(-extinction x distance)), plus
//                  Wrenninge's multiple-scattering octaves: each octave repeats the light with the absorption, the phase's g
//                  and the weight halved (3 identical octaves in every preset)
//   phase          Henyey-Greenstein in octaves (g 0.8 then halved), floored by a Lambert 1 / pi x rescale(backMin, backMax,
//                  extinction): HG gives no convincing back-scatter, so a dense cloud lit from behind the eye still reads
//                  (the talk's visual adjustment, not physics)
//   visibility     a short march towards the sun or the moon at every sample (rdr2LightDepthFrom), quadratically spaced
//   ambient        the clear sky's mean from above (the raw up irradiance CSAmbient stored) weighted towards the key light's
//                  side of the sky, the lit ground from below: bright tops, grey bases, never black
//   lightning      a flash = a point light inside the layer with an exponential falloff, HG phase and the local absorption
//                  as its shadow (the cloud lights up from inside)
//   aerial         far clouds haze into the sky's own colour through the atmosphere's aerial-perspective transmittance
// cloudsvol_rdr2 needs the sky tables (atmosphere.hlsli).
#ifndef EMBER_CLOUDSVOL_RDR2_HLSLI
#define EMBER_CLOUDSVOL_RDR2_HLSLI

#include "atmosphere.hlsli"
#include "clouds008_shape.hlsli"

float hgRdr2(float cosTheta, float g) {
  const float g2 = g * g;
  return (1.0 - g2) / (4.0 * kPi * pow(max(1.0 + g2 - 2.0 * g * cosTheta, 1e-4), 1.5));
}

// The phase in octaves (g 0.8, halved each one, mixed by the octave's weight) floored by the Lambert back-scatter term.
float rdr2Phase(float cosTheta, float extinction, uint octaves, uint octave) {
  const float g = 0.8 * pow(0.5, float(octave));
  const float hg = hgRdr2(cosTheta, g);
  // BACK_SCATTER_MIN / MAX in extinction (per unit): a thin wisp keeps its forward lobe, a dense body goes diffuse.
  const float back = (1.0 / kPi) * rescale(0.02, 0.28, extinction);
  return max(hg, back);
}

// The in-scattered key light at a sample: light depth towards it already marched (`depth`, optical).
float rdr2Scatter(float depth, float cosTheta, float extinction, uint octaves) {
  float sum = 0.0;
  float a = 1.0, w = 1.0;
  for (uint o = 0u; o < octaves; ++o) {
    sum += w * rdr2Phase(cosTheta, extinction, octaves, o) * exp(-depth * a);
    a *= 0.5;
    w *= 0.5;
  }
  return sum * 4.0 * kPi * 0.55;  // energy near a white Lambert top for a dense cloud (E / pi after the slab integration)
}

struct Rdr2Hit {
  float3 color;         // radiance (raw: the sky's scale, before exposure)
  float transmittance;  // 1 = nothing in the way
  float depth;          // distance to the cloud's mean (0 = none)
};

// The lights that reach the top of the layer (through the air above the board), fading as they set.
struct Rdr2Lights {
  float3 sun, moon, sunDir, moonDir, skyUp, skySun, groundUp;
  bool moonOn;
};
Rdr2Lights rdr2Lights(MeshFrame f, AtmosphereFrame a) {
  Rdr2Lights l;
  l.sunDir = a.sunDirection;
  l.moonDir = atmoMoonDirection(f.atmosphere);
  const float3 moonLight = a.sunDisc==0?a.sunIlluminance:atmoMoonLight(f.atmosphere);
  if(a.sunDisc==0) l.moonDir=a.sunDirection;
  l.moonOn = dot(moonLight, 1.0) > 0.0 && l.moonDir.y > -0.1;
  l.sun = (a.sunDisc==0?float3(0,0,0):a.sunIlluminance) * transmittanceToTop(a, a.viewHeight, l.sunDir.y) * saturate(l.sunDir.y * 8.0 + 0.2);
  l.moon = l.moonOn ? moonLight * transmittanceToTop(a, a.viewHeight, l.moonDir.y) * saturate(l.moonDir.y * 8.0 + 0.2) : 0.0;
  const uint64_t ambient=vk::RawBufferLoad<uint64_t>(rdr2Record(f)+kR2Ambient);
  l.skyUp=vk::RawBufferLoad<float4>(ambient).rgb;
  // The ambient weighted towards the key light's side (the talk's paraboloid mean, weighted to the sun / moon): the sky's
  // radiance 25 degrees up on the key light's azimuth, on the raw scale.
  const float3 key = l.sunDir.y > -0.05 ? l.sunDir : l.moonDir;
  const float2 az = normalize(key.xz + float2(1e-4, 0.0));
  const float3 side = normalize(float3(az.x * 0.9, 0.42, az.y * 0.9));
  l.skySun = vk::RawBufferLoad<float4>(ambient+16u).rgb;  // irradiance-like, as skyUp is
  const float3 sunOnGround = a.sunIlluminance * transmittanceToTop(a, a.viewHeight, l.sunDir.y) * saturate(l.sunDir.y);
  l.groundUp = a.groundAlbedo * (l.skyUp + sunOnGround);
  return l;
}

// The lit layer along the ray from `origin` towards `dir`: up to `steps` samples (adaptive: long steps through clear air,
// the step halved on entering a cloud, long again after a run of empty samples). `clear` = the sky's radiance in `dir` on the
// sky's scale (the haze colour). `dither` (0..1): the blue-noise offset of this pixel this frame. `octaves` / `lightTaps`:
// the preset's. `uv`: the pixel on screen (the aerial table).
Rdr2Hit rdr2March(MeshFrame f, AtmosphereFrame a, Rdr2Clouds c, float3 origin, float3 dir, float3 clear, uint steps, uint lightTaps,
                  uint octaves, float dither, float2 uv, bool perspective=true, bool simple=false) {
  Rdr2Hit none;
  none.color = 0.0;
  none.transmittance = 1.0;
  none.depth = 0.0;
  if (dir.y <= 0.003) return none;
  const float tBase = max((c.base - origin.y) / dir.y, 0.0);
  const float tTop = (max(c.anvilTop,c.middle.y) - origin.y) / dir.y;
  static const float kFar = 9000.0;
  const float t0 = tBase, t1 = min(tTop, kFar);

  const Rdr2Lights l = rdr2Lights(f, a);
  const float ext1 = rdr2Extinction(c);
  const float cosSun = dot(dir, l.sunDir), cosMoon = dot(dir, l.moonDir);
  const float layer = max(c.anvilTop - c.base, 1.0);
  float3 light = 0.0;
  float transmittance = 1.0;
  float depthSum = 0.0, depthWeight = 0.0;
  // Stable quadrature: one small blue-noise offset per ray, never a new random light ray at each sample.
  const float fineStep=max((t1-t0)/max(float(steps),1),layer/512.0);
  float t=t0;
  uint empty=3u;
  for(uint i=0u;i<steps && t<t1;++i){
    const float r=.5+(dither-.5)*.12;
    const float fine=fineStep;
    const float coarse=fine*4;
    float dt=min(empty>=3u?coarse:fine,t1-t);
    float3 p=origin+dir*(t+dt*r);
    if (empty >= 3u) {
      // Clear air so far: test cheaply; on a hit, step back and take the fine step from there.
      if (rdr2Density(c, p, true, 0.0) <= 0.0) {
        t += dt;
        continue;
      }
      dt = min(fine, t1 - t);
      p = origin + dir * (t + dt * r);
    }
    t += dt;
    const float lod = saturate((t - 900.0) / 5500.0);
    const float density = rdr2Density(c, p, simple, lod);
    if (density <= 0.002) {
      ++empty;
      continue;
    }
    empty = 0u;
    const float ext = density * ext1;
    const float h = saturate((p.y - c.base) / layer);
    // Direct: the sun (and the moon) through the cloud towards this point.
    const float sunDepth = rdr2LightDepthFrom(c, p, l.sunDir, lightTaps, .5);
    float3 direct = l.sun * rdr2Scatter(sunDepth, cosSun, ext, octaves);
    if (l.moonOn) direct += l.moon * rdr2Scatter(rdr2LightDepthFrom(c, p, l.moonDir, max(lightTaps / 2u, 2u), .5), cosMoon, ext, octaves);
    // Ambient: the sky from above, the key light's side of the sky, the ground from below; thinned deep inside.
    const float inside = exp(-sunDepth * 0.12);
    const float3 ambient = (l.skyUp * lerp(0.3, 0.9, h) + l.skySun * 0.18 * lerp(0.5, 1.0, h)) * lerp(0.55, 1.0, inside) +
                           l.groundUp * lerp(0.32, 0.04, h);
    // A list of simultaneous flashes, supplied by weather; high-frequency flashes bypass history in resolve.
    float3 flash=0;
    const uint flashCount=simple?0u:uint(vk::RawBufferLoad<float4>(c.record+kR2Temporal).w);
    for(uint j=0;j<flashCount;++j) {
      const float4 bolt=vk::RawBufferLoad<float4>(c.record+kR2Flashes+j*16u);
      const float3 toFlash=bolt.xyz-p;
      const float d=length(toFlash);
      const float falloff=exp(-d/320.0)/max(d*d*1e-4,1.0);
      flash+=float3(.78,.84,1)*bolt.w*falloff*exp(-density*ext1*min(d,60.0)*.5)*
          hgRdr2(dot(dir,toFlash/max(d,1e-3)),.3)*4*kPi;
    }
    const float3 radiance=(direct+ambient/kPi+flash)*lerp(.97,.82,c.rain)*
        lerp(1.0,.3,saturate(vk::RawBufferLoad<float>(f.atmosphere+300u)));
    const float slab = exp(-ext * dt);
    light += transmittance * (radiance - radiance * slab);
    depthSum += length(p-origin) * transmittance * (1.0 - slab);
    depthWeight += transmittance * (1.0 - slab);
    transmittance *= slab;
    if (transmittance < 0.008) break;
  }
  // The cirrus sheet beyond the layer: one thin, lit sample.
  const float tc = (c.cirrusHeight - origin.y) / dir.y;
  if (tc > 0.0 && tc < 40000.0 && transmittance > 0.01) {
    const float2 at = origin.xz + dir.xz * tc;
    const float cirrus = rdr2Cirrus(c, at);
    if (cirrus > 0.002) {
      const float od = cirrus * 0.45;
      const float cover = 1.0 - exp(-od);
      // Thin ice: strong forward lobe, lit by the sun straight through it, the sky's fill from both sides.
      const float3 p = float3(at.x, c.cirrusHeight, at.y);
      float3 direct = l.sun * (hgRdr2(cosSun, 0.85) * 0.7 + hgRdr2(cosSun, 0.2) * 0.3) * 4.0 * kPi * 0.6;
      if (l.moonOn) direct += l.moon * hgRdr2(cosMoon, 0.6) * 4.0 * kPi * 0.6;
      const float3 radiance = direct + (l.skyUp * 0.6 + l.skySun * 0.2) / kPi;
      light += transmittance * radiance * cover;
      depthSum += tc * transmittance * cover;
      depthWeight += transmittance * cover;
      transmittance *= 1.0 - cover;
    }
  }
  Rdr2Hit hit;
  hit.transmittance = transmittance;
  hit.depth = depthWeight > 0.0 ? depthSum / depthWeight : 0.0;
  // Aerial perspective: the far cloud hazes into the sky's own colour there, by the atmosphere's own transmittance for that
  // distance (the aerial table's far slices).
  const float covered = 1.0 - transmittance;
  if (perspective && depthWeight > 0.0 && a.kmPerUnit>0.0) {
    const float w = hit.depth * a.kmPerUnit / a.aerialMaxKm;
    const float3 uvw = float3(uv, saturate(w) - 0.5 / 32.0);
    const float4 air=gTextures3D[NonUniformResourceIndex(a.aerial)].SampleLevel(gSamplers[a.linearSampler],saturate(uvw),0);
    hit.color=light*air.a+air.rgb*a.skyScale*covered;
  } else {
    hit.color = light;
  }
  // Towards the horizon the layer thins into the haze (the slab's far cap).
  const float horizon = smoothstep(0.003, 0.025, dir.y);
  hit.color *= horizon;
  hit.transmittance = lerp(1.0, hit.transmittance, horizon);
  return hit;
}

#endif
