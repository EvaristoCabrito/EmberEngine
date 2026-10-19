// Visibility-buffer resolve: one thread per pixel. The pixel's (cluster-list slot, triangle) gives the
// triangle; its three vertices are re-projected, perspective-correct barycentrics and their screen-space
// derivatives are computed analytically (so textures still get correct mip selection and anisotropic
// filtering), attributes are interpolated and the material is shaded into the HDR scene colour.
//
// Lighting here is provisional (one sun + sky/ground ambient, physically based materials): the frame's
// real lighting model (clustered lights, shadows, how 2D cards and 3D share light) is decided by
// Demonstration 1, see docs/ARCHITECTURE.md §7.1.
#include "atmosphere.hlsli"

struct Push {
  uint64_t frame;
  uint visibilityImage;  // R32_UINT storage image
  uint colorImage;       // RGBA16F storage image (scene colour)
  float4 clearColor;
  uint background;       // kNoTexture: sky / clearColor; kBlackBackground: black; else a painted backdrop
  uint backgroundMode;   // with a backdrop: 0 cover-fit over the whole screen, 1 the painting up to the world's horizon,
                         // the real sky above it (the user, 2026-10-19: "a imagem de fundo pode terminar no horizonte")
  uint pad1, pad2;
};
// Backdrop mode 1: the painting's own horizon sits at this share of its height (the row the world's horizon is pinned
// to), and the painting reaches its bottom row this many degrees below the horizon.
static const float kBackdropHorizon = 0.5;
static const float kBackdropDownDegrees = 30.0;
[[vk::push_constant]] Push push;
static const uint kBlackBackground = 0xFFFFFFFEu;

// What shows where nothing was drawn: the painted backdrop (fitted to cover the screen, like a photo behind a
// stage), black, the sky, or the clear colour.
float4 backgroundAt(MeshFrame f, uint2 pixel) {
  if (f.debugView >= kDebugVolumeFirst && f.debugView <= kDebugVolumeLast) return float4(volumeOverBackground(f, 0.0, pixel), 1.0);  // the volume's debug views
  if (f.debugView == kDebugClouds && f.atmosphere != 0) {  // the low sheet's cover where the sky shows
    const AtmosphereFrame a = loadAtmosphere(f);
    const float3 dir = skyDirection(f, a, float2(pixel) + 0.5);
    if (dir.y <= 0.002 || (f.cloudFlags & kCloudLayer) == 0) return float4(0.0, 0.0, 0.3, 1.0);
    const CloudParams c = loadCloudParams(f);
    const float t = (c.altitude - f.cameraPosition.y) / dir.y;
    return float4(cloudCoverEroded(c, f.time, f.cameraPosition.xz + dir.xz * t).xxx, 1.0);
  }
  if (f.debugView != 0 || push.background == kBlackBackground) return float4(0.0, 0.0, 0.0, 1.0);
  if (push.background != kNoTexture) {
    uint w, h;
    gTextures[NonUniformResourceIndex(push.background)].GetDimensions(w, h);
    float2 uv = (float2(pixel) + 0.5) / f.screenSize;
    const float screenAspect = f.screenSize.x / f.screenSize.y, imageAspect = float(w) / float(h);
    if (screenAspect > imageAspect) uv.y = 0.5 + (uv.y - 0.5) * imageAspect / screenAspect;
    else uv.x = 0.5 + (uv.x - 0.5) * screenAspect / imageAspect;
    if (push.backgroundMode == 1 && f.atmosphere != 0) {
      // The painting up to the world's horizon, the sky above it: the painting's horizon row is pinned to the horizon and
      // its rows below follow the view's elevation (so it moves with the camera's tilt like a far landscape); a narrow band
      // of sky fades in over the horizon.
      const AtmosphereFrame a = loadAtmosphere(f);
      const float3 dir = skyDirection(f, a, float2(pixel) + 0.5);
      const float elevation = asin(clamp(dir.y, -1.0, 1.0));
      uv.y = kBackdropHorizon + saturate(-elevation / radians(kBackdropDownDegrees)) * (1.0 - kBackdropHorizon);
      const float3 painting = gTextures[NonUniformResourceIndex(push.background)].SampleLevel(gSamplers[f.materialSampler], uv, 0).rgb;
      const float sky = smoothstep(-0.004, 0.012, elevation);
      const float3 color = sky > 0.0 ? lerp(painting, skyBackground(f, pixel), sky) : painting;
      return float4(volumeOverBackground(f, color, pixel), 1.0);
    }
    // Atmosfera volumétrica v1: the fog lies between the camera and the painting (nothing when it is off).
    return float4(volumeOverBackground(f, gTextures[NonUniformResourceIndex(push.background)].SampleLevel(gSamplers[f.materialSampler], uv, 0).rgb, pixel), 1.0);
  }
  if (f.atmosphere != 0) return float4(volumeOverBackground(f, skyBackground(f, pixel), pixel), 1.0);
  return push.clearColor;
}

struct Barycentrics {
  float3 lambda;
  float3 ddx;  // per pixel to the right
  float3 ddy;  // per pixel down
};

// Perspective-correct barycentrics of `ndc` inside the triangle (clip-space corners), with derivatives.
// After "Visibility Buffer Rendering with Material Graphs" (Schied, 2021) and The Forge's CalcFullBary;
// Vulkan NDC (+y down), so no y flip.
Barycentrics barycentrics(float4 p0, float4 p1, float4 p2, float2 ndc, float2 screenSize) {
  Barycentrics b;
  const float3 invW = 1.0 / float3(p0.w, p1.w, p2.w);
  const float2 n0 = p0.xy * invW.x, n1 = p1.xy * invW.y, n2 = p2.xy * invW.z;
  const float invDet = 1.0 / determinant(float2x2(n2 - n1, n0 - n1));
  float3 ddx = float3(n1.y - n2.y, n2.y - n0.y, n0.y - n1.y) * invDet * invW;
  float3 ddy = float3(n2.x - n1.x, n0.x - n2.x, n1.x - n0.x) * invDet * invW;
  float ddxSum = dot(ddx, 1.0), ddySum = dot(ddy, 1.0);
  const float2 delta = ndc - n0;
  const float interpInvW = invW.x + delta.x * ddxSum + delta.y * ddySum;
  const float interpW = 1.0 / interpInvW;
  b.lambda = interpW * float3(invW.x + delta.x * ddx.x + delta.y * ddy.x, delta.x * ddx.y + delta.y * ddy.y,
                              delta.x * ddx.z + delta.y * ddy.z);
  ddx *= 2.0 / screenSize.x;
  ddy *= 2.0 / screenSize.y;
  ddxSum *= 2.0 / screenSize.x;
  ddySum *= 2.0 / screenSize.y;
  const float wx = 1.0 / (interpInvW + ddxSum), wy = 1.0 / (interpInvW + ddySum);
  b.ddx = wx * (b.lambda * interpInvW + ddx) - b.lambda;
  b.ddy = wy * (b.lambda * interpInvW + ddy) - b.lambda;
  return b;
}

[numthreads(8, 8, 1)]
void CSResolve(uint2 pixel : SV_DispatchThreadID) {
  const MeshFrame f = loadFrame(push.frame);
  if (any(pixel >= uint2(f.screenSize))) return;
  const uint vis = gStorageImagesR32ui[NonUniformResourceIndex(push.visibilityImage)][pixel];
  RWTexture2D<float4> color = gStorageImagesRgba16f[NonUniformResourceIndex(push.colorImage)];
  if (vis == ~0u) {
    // Nothing here (the ground and cards are drawn over this later).
    color[pixel] = backgroundAt(f, pixel);
    return;
  }

  const uint slot = vis >> 6, triangleIndex = vis & 63;
  const uint2 item = work(f).Load2(clusterListOffset(f) + slot * 8);
  const InstanceGpu inst = loadInstance(f, item.x);
  const MeshGpu mesh = loadMesh(f, inst.mesh);
  const MeshCluster cluster = loadCluster(mesh, item.y);
  const uint3 t = loadTriangle(mesh, cluster, triangleIndex);
  MeshVertex v[3];
  float3 world[3];
  float4 clipPos[3];
  [unroll] for (uint k = 0; k < 3; ++k) {
    v[k] = loadVertex(mesh, loadClusterVertex(mesh, cluster.vertexOffset + t[k]));
    world[k] = transformPoint(inst, v[k].position);
    clipPos[k] = mul(float4(world[k], 1.0), f.viewProjection);
  }
  const float2 ndc = (float2(pixel) + 0.5) / f.screenSize * 2.0 - 1.0;
  const Barycentrics b = barycentrics(clipPos[0], clipPos[1], clipPos[2], ndc, f.screenSize);

  const float3 position = world[0] * b.lambda.x + world[1] * b.lambda.y + world[2] * b.lambda.z;
  const float2 uv = v[0].uv * b.lambda.x + v[1].uv * b.lambda.y + v[2].uv * b.lambda.z;
  const float2 uvDx = v[0].uv * b.ddx.x + v[1].uv * b.ddx.y + v[2].uv * b.ddx.z;
  const float2 uvDy = v[0].uv * b.ddy.x + v[1].uv * b.ddy.y + v[2].uv * b.ddy.z;
  float3 normal = 0.0;
  float4 tangent = 0.0;
  float4 vertexColor = 0.0;
  [unroll] for (uint k = 0; k < 3; ++k) {
    normal += decodeNormal(v[k].normal) * b.lambda[k];
    tangent += decodeTangent(v[k].tangent) * b.lambda[k];
    vertexColor += decodeColor(v[k].color) * b.lambda[k];
  }
  // Rotation part of the instance transform (uniform scale assumed; non-uniform needs the inverse transpose).
  normal = normalize(transformDirection(inst, normal));
  const float3 tangentWorld = normalize(transformDirection(inst, tangent.xyz));

  const MaterialGpu m = loadMaterial(f, mesh.materialBase + clusterMaterial(cluster));
  SamplerState s = gSamplers[f.materialSampler];
  float4 baseColor = m.baseColor * vertexColor;
  if (m.baseColorTexture != kNoTexture)
    baseColor *= gTextures[NonUniformResourceIndex(m.baseColorTexture)].SampleGrad(s, uv, uvDx, uvDy);
  float occlusion = 1.0, roughness = m.roughness, metallic = m.metallic;
  if (m.ormTexture != kNoTexture) {
    const float3 orm = gTextures[NonUniformResourceIndex(m.ormTexture)].SampleGrad(s, uv, uvDx, uvDy).rgb;
    occlusion = lerp(1.0, orm.r, m.occlusionStrength);
    roughness *= orm.g;
    metallic *= orm.b;
  }
  float3 n = normal;
  if (m.normalTexture != kNoTexture) {
    float2 xy = gTextures[NonUniformResourceIndex(m.normalTexture)].SampleGrad(s, uv, uvDx, uvDy).rg * 2.0 - 1.0;
    xy *= m.normalScale;
    const float3 tn = float3(xy, sqrt(saturate(1.0 - dot(xy, xy))));
    const float3 bitangent = cross(normal, tangentWorld) * tangent.w;
    n = normalize(tangentWorld * tn.x + bitangent * tn.y + normal * tn.z);
  }
  float3 emissive = m.emissive;
  if (m.emissiveTexture != kNoTexture)
    emissive *= gTextures[NonUniformResourceIndex(m.emissiveTexture)].SampleGrad(s, uv, uvDx, uvDy).rgb;

  // Provisional lighting (lighting.hlsli): sun and a sky/ground hemisphere.
  const float3 view = normalize(f.cameraPosition - position);
  if (dot(n, view) < 0.0 && (m.flags & kMaterialDoubleSided)) n = -n;
  if (f.debugView == 1) {
    color[pixel] = float4(normal * 0.5 + 0.5, 1.0);
    return;
  }
  if (f.debugView == 2) {
    color[pixel] = float4(n * 0.5 + 0.5, 1.0);
    return;
  }
  Surface surface = defaultSurface(position, normal, baseColor.rgb);
  surface.normal = n;
  surface.roughness = roughness;
  surface.metallic = metallic;
  surface.occlusion = occlusion;
  surface.emissive = emissive;
  surface.sightAt = instanceAnchor(inst);  // a piece takes the fog of war of where it stands, whole (like a card)
  color[pixel] = float4(shade(f, surface), 1.0);
}

// A frame without 3D meshes (no resolve): the background alone (backdrop, black or sky) behind the ground and cards.
[numthreads(8, 8, 1)]
void CSSky(uint2 pixel : SV_DispatchThreadID) {
  const MeshFrame f = loadFrame(push.frame);
  if (any(pixel >= uint2(f.screenSize))) return;
  RWTexture2D<float4> color = gStorageImagesRgba16f[NonUniformResourceIndex(push.colorImage)];
  color[pixel] = backgroundAt(f, pixel);
}
