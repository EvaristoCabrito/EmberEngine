// The atmosphere's tables, rebuilt every frame (atmosphere.hlsli; Hillaire 2020):
//   CSTransmittance    the air's transmittance to its top, by height and zenith angle
//   CSMultiScattering  higher-order scattering, an isotropic geometric series per height and sun angle
//   CSSkyView          the sky from the board (key light in), for the background and the ambient light
//   CSAerial           in-scattered light and transmittance from the camera, per froxel (aerial perspective)
//   CSAmbient          the sky's (and the lit ground's) irradiance as 9 spherical harmonics, scaled so an
//                      upward surface receives the map's ambient strength (the user's tuned value)
#include "atmosphere.hlsli"

struct Push {
  uint64_t frame;       // MeshFrame
  uint64_t atmosphere;  // AtmosphereFrame
};
[[vk::push_constant]] Push push;

AtmosphereFrame loadAtmo() { return vk::RawBufferLoad<AtmosphereFrame>(push.atmosphere); }

// Light from the key light reaching a point (planet-centred), through the air, unless the planet hides it.
float3 sunReaching(AtmosphereFrame a, float3 p) {
  const float r = length(p);
  const float3 up = p / r;
  const float mu = dot(up, a.sunDirection);
  if (raySphere(p, a.sunDirection, a.bottomRadius) >= 0.0) return 0.0;
  return transmittanceToTop(a, r, mu);
}

[numthreads(8, 8, 1)]
void CSTransmittance(uint3 id : SV_DispatchThreadID) {
  if (any(id.xy >= kTransmittanceSize)) return;
  const AtmosphereFrame a = loadAtmo();
  float r, mu;
  transmittanceFromUv(a, (float2(id.xy) + 0.5) / float2(kTransmittanceSize), r, mu);
  const float3 o = float3(0.0, r, 0.0), d = float3(sqrt(max(1.0 - mu * mu, 0.0)), mu, 0.0);
  const float span = max(raySphere(o, d, a.topRadius), 0.0);
  static const uint kSteps = 40;
  float3 depth = 0.0;
  for (uint i = 0; i < kSteps; ++i) {
    const float3 p = o + d * ((float(i) + 0.5) / kSteps * span);
    depth += mediumAt(a, max(0.0, length(p) - a.bottomRadius)).extinction * (span / kSteps);
  }
  gStorageImagesRgba16f[NonUniformResourceIndex(a.transmittanceStorage)][id.xy] = float4(exp(-depth), 1.0);
}

// Spherical Fibonacci directions: n roughly uniform points on the sphere.
float3 sphereDirection(uint i, uint n) {
  const float y = 1.0 - (float(i) + 0.5) / float(n) * 2.0, r = sqrt(max(1.0 - y * y, 0.0));
  const float phi = float(i) * 2.39996323;  // golden angle
  return float3(cos(phi) * r, y, sin(phi) * r);
}

[numthreads(8, 8, 1)]
void CSMultiScattering(uint3 id : SV_DispatchThreadID) {
  if (any(id.xy >= kMultiScatteringSize)) return;
  AtmosphereFrame a = loadAtmo();
  const float2 uv = float2(id.xy) / (float2(kMultiScatteringSize) - 1.0);
  const float muSun = uv.x * 2.0 - 1.0, r = a.bottomRadius + uv.y * (a.topRadius - a.bottomRadius);
  a.sunDirection = float3(sqrt(max(1.0 - muSun * muSun, 0.0)), muSun, 0.0);
  const float3 o = float3(0.0, r, 0.0);
  static const uint kDirections = 64, kSteps = 20;
  float3 second = 0.0, transfer = 0.0;
  for (uint k = 0; k < kDirections; ++k) {
    const float3 d = sphereDirection(k, kDirections);
    const float ground = raySphere(o, d, a.bottomRadius), top = raySphere(o, d, a.topRadius);
    const float span = ground >= 0.0 ? ground : max(top, 0.0);
    const float dt = span / kSteps;
    float3 throughput = 1.0, light = 0.0, fms = 0.0;
    for (uint i = 0; i < kSteps; ++i) {
      const float3 p = o + d * ((float(i) + 0.5) * dt);
      const Medium m = mediumAt(a, max(0.0, length(p) - a.bottomRadius));
      const float3 ext = max(m.extinction, 1e-6), slab = exp(-ext * dt);
      const float3 s = m.scattering * sunReaching(a, p) / (4.0 * kPi);  // isotropic, sun illuminance 1
      light += throughput * (s - s * slab) / ext;
      fms += throughput * (m.scattering - m.scattering * slab) / ext;
      throughput *= slab;
    }
    if (ground >= 0.0) {  // the lit ground below reflects (Lambert)
      const float3 p = o + d * ground, n = normalize(p);
      light += throughput * sunReaching(a, p + n * 1e-3) * saturate(dot(n, a.sunDirection)) * a.groundAlbedo / kPi;
    }
    second += light / kDirections;
    transfer += fms / kDirections;
  }
  const float3 psi = second / max(1.0 - transfer, 1e-3);  // all orders: a geometric series
  gStorageImagesRgba16f[NonUniformResourceIndex(a.multiScatteringStorage)][id.xy] = float4(psi, 1.0);
}

// In-scattered light along a ray from `o` (planet-centred) for `length` km, and its transmittance.
float4 integrate(AtmosphereFrame a, float3 o, float3 d, float span, uint steps) {
  const float cosTheta = dot(d, a.sunDirection);
  const float pr = phaseRayleigh(cosTheta), pm = phaseMie(cosTheta, a.mieG);
  const float dt = span / steps;
  float3 throughput = 1.0, light = 0.0;
  for (uint i = 0; i < steps; ++i) {
    const float3 p = o + d * ((float(i) + 0.3) * dt);
    const float r = length(p);
    const Medium m = mediumAt(a, max(0.0, r - a.bottomRadius));
    const float3 ext = max(m.extinction, 1e-6), slab = exp(-ext * dt);
    const float muSun = dot(p / r, a.sunDirection);
    const float3 s = (sunReaching(a, p) * (m.rayleigh * pr + m.mie * pm) + multiScatteringAt(a, r, muSun) * m.scattering) * a.sunIlluminance;
    light += throughput * (s - s * slab) / ext;
    throughput *= slab;
  }
  return float4(light, dot(throughput, 1.0 / 3.0));
}

[numthreads(8, 8, 1)]
void CSSkyView(uint3 id : SV_DispatchThreadID) {
  if (any(id.xy >= kSkyViewSize)) return;
  AtmosphereFrame a = loadAtmo();
  float viewZenith, azimuthCos;
  skyViewFromUv(a, (float2(id.xy) + 0.5) / float2(kSkyViewSize), viewZenith, azimuthCos);
  // The sun in the x-y plane; the view at that azimuth from it.
  const float muSun = a.sunDirection.y;
  a.sunDirection = float3(sqrt(max(1.0 - muSun * muSun, 0.0)), muSun, 0.0);
  const float sz = sin(viewZenith);
  const float3 d = float3(sz * azimuthCos, cos(viewZenith), sz * sqrt(max(1.0 - azimuthCos * azimuthCos, 0.0)));
  const float3 o = float3(0.0, a.viewHeight, 0.0);
  const float ground = raySphere(o, d, a.bottomRadius), top = raySphere(o, d, a.topRadius);
  const float span = ground >= 0.0 ? ground : max(top, 0.0);
  float3 light = integrate(a, o, d, span, 30).rgb;
  // The 24 h cycle's moon, a second light opposite the sun in azimuth (the table is laid out around the sun's azimuth).
  const float3 moonLight = atmoMoonLight(push.atmosphere);
  if (dot(moonLight, 1.0) > 0.0) {
    AtmosphereFrame m = a;
    const float muMoon = atmoMoonDirection(push.atmosphere).y;
    m.sunDirection = float3(-sqrt(max(1.0 - muMoon * muMoon, 0.0)), muMoon, 0.0);
    m.sunIlluminance = moonLight;
    light += integrate(m, o, d, span, 30).rgb;
  }
  gStorageImagesRgba16f[NonUniformResourceIndex(a.skyViewStorage)][id.xy] = float4(light, 1.0);
}

[numthreads(8, 8, 1)]
void CSAerial(uint3 id : SV_DispatchThreadID) {
  if (any(id.xy >= kAerialSize)) return;
  const AtmosphereFrame a = loadAtmo();
  const MeshFrame f = loadFrame(push.frame);
  const float3 moonLight = atmoMoonLight(push.atmosphere);
  AtmosphereFrame m = a;  // the 24 h cycle's moon as a second light
  m.sunDirection = atmoMoonDirection(push.atmosphere);
  m.sunIlluminance = moonLight;
  const bool moonOn = dot(moonLight, 1.0) > 0.0;
  const float2 uv = (float2(id.xy) + 0.5) / float(kAerialSize);
  const float4 h = mul(float4(uv * 2.0 - 1.0, 1.0, 1.0), a.inverseViewProjection);
  const float3 d = normalize(h.xyz / h.w - f.cameraPosition);
  // The camera above the board's ground, in the air's kilometres (the artistic scale).
  const float3 o = float3(0.0, a.viewHeight + max(f.cameraPosition.y, 0.0) * a.kmPerUnit, 0.0);
  RWTexture3D<float4> volume = gStorageVolumes[NonUniformResourceIndex(a.aerialStorage)];
  float3 light = 0.0;
  float throughput = 1.0;  // the mean over r, g, b (one channel left in the volume)
  float previous = 0.0;
  for (uint z = 0; z < kAerialSize; ++z) {
    const float t = (float(z) + 1.0) / kAerialSize * a.aerialMaxKm;
    const float4 slab = integrate(a, o + d * previous, d, t - previous, 2);
    light += throughput * slab.rgb;
    if (moonOn) light += throughput * integrate(m, o + d * previous, d, t - previous, 2).rgb;
    throughput *= slab.a;
    previous = t;
    volume[uint3(id.xy, z)] = float4(light, throughput);
  }
}

// --- The ambient light: spherical harmonics of the sky (and the lit ground below the horizon) -------------

static const uint kAmbientThreads = 256, kAmbientDirections = 4096;
groupshared float3 gSh[kAmbientThreads][9];
groupshared float3 gUp[kAmbientThreads];

void shBasis(float3 n, out float b[9]) {
  b[0] = 0.282095;
  b[1] = 0.488603 * n.y;
  b[2] = 0.488603 * n.z;
  b[3] = 0.488603 * n.x;
  b[4] = 1.092548 * n.x * n.y;
  b[5] = 1.092548 * n.y * n.z;
  b[6] = 0.315392 * (3.0 * n.z * n.z - 1.0);
  b[7] = 1.092548 * n.x * n.z;
  b[8] = 0.546274 * (n.x * n.x - n.y * n.y);
}

[numthreads(kAmbientThreads, 1, 1)]
void CSAmbient(uint thread : SV_GroupIndex) {
  const AtmosphereFrame a = loadAtmo();
  const float weight = 4.0 * kPi / kAmbientDirections;
  // First the irradiance on the ground from the sky, so the ground below the horizon can be lit by it.
  float3 up = 0.0;
  for (uint k = thread; k < kAmbientDirections; k += kAmbientThreads) {
    const float3 d = sphereDirection(k, kAmbientDirections);
    if (d.y > 0.0) up += skyRadiance(a, d) * d.y * weight;
  }
  gUp[thread] = up;
  GroupMemoryBarrierWithGroupSync();
  for (uint s = kAmbientThreads / 2; s > 0; s >>= 1) {
    if (thread < s) gUp[thread] += gUp[thread + s];
    GroupMemoryBarrierWithGroupSync();
  }
  float3 sunOnGround = a.sunIlluminance * transmittanceToTop(a, a.viewHeight, a.sunDirection.y) * saturate(a.sunDirection.y);
  const float3 moonDir = atmoMoonDirection(push.atmosphere);
  sunOnGround += atmoMoonLight(push.atmosphere) * transmittanceToTop(a, a.viewHeight, moonDir.y) * saturate(moonDir.y);
  const float3 groundRadiance = a.groundAlbedo / kPi * (gUp[0] + sunOnGround);
  float3 sh[9];
  [unroll] for (uint c = 0; c < 9; ++c) sh[c] = 0.0;
  for (uint k = thread; k < kAmbientDirections; k += kAmbientThreads) {
    const float3 d = sphereDirection(k, kAmbientDirections);
    const float3 radiance = d.y > 0.0 ? skyRadiance(a, d) : groundRadiance;
    float b[9];
    shBasis(d, b);
    [unroll] for (uint c = 0; c < 9; ++c) sh[c] += radiance * b[c] * weight;
  }
  [unroll] for (uint c = 0; c < 9; ++c) gSh[thread][c] = sh[c];
  GroupMemoryBarrierWithGroupSync();
  for (uint s = kAmbientThreads / 2; s > 0; s >>= 1) {
    if (thread < s) [unroll] for (uint c = 0; c < 9; ++c) gSh[thread][c] += gSh[thread + s][c];
    GroupMemoryBarrierWithGroupSync();
  }
  if (thread != 0) return;
  // Irradiance (cosine lobe: pi, 2pi/3, pi/4 per band), scaled so an upward surface receives the map's ambient.
  static const float kBand[9] = {kPi, 2.0943951, 2.0943951, 2.0943951, 0.785398, 0.785398, 0.785398, 0.785398, 0.785398};
  float3 e[9];
  [unroll] for (uint c = 0; c < 9; ++c) e[c] = gSh[0][c] * kBand[c];
  float bUp[9];
  shBasis(float3(0, 1, 0), bUp);
  float3 upIrradiance = 0.0;
  [unroll] for (uint c = 0; c < 9; ++c) upIrradiance += e[c] * bUp[c];
  // ambientTarget >= 0: scaled so an upward surface receives that luminance; < 0: the sky's own irradiance x -target.
  const float scale = a.ambientTarget >= 0.0 ? a.ambientTarget / max(dot(upIrradiance, float3(0.2126, 0.7152, 0.0722)), 1e-8)
                                            : -a.ambientTarget;
  // The w's carry what else reads the sky: the scale (first), then the raw sky irradiance on an upward face (the cloud
  // sheet's fill light, the reflection map's ground), both read by skyReflection / cloudsOverSky.
  const float extra[4] = {scale, gUp[0].r, gUp[0].g, gUp[0].b};
  [unroll] for (uint c = 0; c < 9; ++c) vk::RawBufferStore<float4>(a.ambient + c * 16, float4(e[c] * scale, c < 4 ? extra[c] : 0.0));
}
