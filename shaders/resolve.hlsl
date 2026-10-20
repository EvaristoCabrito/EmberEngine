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
                         // the real sky above it (the user, 2026-10-19: "a imagem de fundo pode terminar no horizonte"),
                         // 2 the painting aligned to the sky (content/engine2/backdrop-fit.json; fitA / fitB)
  float planeY;          // mode 2: height of the flat ground plane under the board
  float4 fitA;           // mode 2: horizon row, bottom row kept (1 = the whole painting), degrees the painting's height spans above
                         // the horizon, degrees below the horizon where the bottom row lands
  float4 fitB;           // mode 2: centre azimuth (radians, the painting's glow under the real sun), sky-top row, degrees its width spans,
                         // the elevation (degrees, usually negative) where the painting's horizon row sits (the user: "o cenário está muito alto")
  uint planeTexture;     // mode 2: tileable top-down ground for the plane (kNoTexture = no plane: the painting all the way down)
  float planeTile;       // ... world units per repeat
  float planeFar;        // ... beyond this distance along the ray the painting takes over
  float planeFrom;       // ... and within this many degrees of the horizon (the plane fully in from 2.5 x this)
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
    // mode 5 = the regular image, whole and opaque (the user, 2026-10-20: "the regular image static at the line of the horizon, NOT melting,
    // no transparency"): its horizon row on the world's horizon line, its width spanning the screen (the image's own proportions, no
    // stretch), nothing of the real sky mixed into it; it moves with the camera's tilt only, as a far backdrop does. The painting's own
    // colours, with the volumetric air in front of it like the plain cover-fit.
    if (push.backgroundMode == 5 && f.atmosphere != 0) {
      const AtmosphereFrame a = loadAtmosphere(f);
      const float3 dir = skyDirection(f, a, float2(pixel) + 0.5);
      const float elevation = asin(clamp(dir.y, -1.0, 1.0));
      const float3 dl = skyDirection(f, a, float2(0.5, f.screenSize.y * 0.5)), dr = skyDirection(f, a, float2(f.screenSize.x - 0.5, f.screenSize.y * 0.5));
      const float imageHeightRad = acos(clamp(dot(dl, dr), -1.0, 1.0)) * float(h) / max(float(w), 1.0);
      // The rows keep the image's own scale near the horizon and compress smoothly toward the zenith / the nadir, so the last row lands exactly
      // there (one continuous image at any tilt: no clamped smear). A painting is never repeated or cut.
      const float horizonRow = clamp(push.fitA.x, 0.02, 0.98);
      float v;
      if (elevation >= 0.0) {
        const float span = horizonRow * imageHeightRad, toZenith = (1.5707963 / max(span, 1e-3));
        v = horizonRow * (1.0 - (1.0 - exp(-elevation / max(span, 1e-3))) / max(1.0 - exp(-toZenith), 1e-3));
      } else {
        const float span = (1.0 - horizonRow) * imageHeightRad, toNadir = (1.5707963 / max(span, 1e-3));
        v = horizonRow + (1.0 - horizonRow) * (1.0 - exp(elevation / max(span, 1e-3))) / max(1.0 - exp(-toNadir), 1e-3);
      }
      v = saturate(v);
      const float u = (float(pixel.x) + 0.5) / f.screenSize.x;
      const float3 painting = gTextures[NonUniformResourceIndex(push.background)].SampleLevel(gSamplers[f.materialSampler], float2(u, v), 0).rgb;
      return float4(volumeOverBackground(f, painting, pixel), 1.0);
    }
    // mode 3 = mode 2 keeping the painting's own sky (aldeia's fire panorama: its smoke sky is the look), the real sky only
    // above the painting's top edge, faded in over its top rows.
    // mode 4 = mode 2 for a painting whose sky is truly transparent (its alpha: the user's "Paisagem Devastada", 2026-10-19): the
    // real sky shows wherever the image is clear, through its smoke where it is half clear; no matte by colour.
    if ((push.backgroundMode == 2 || push.backgroundMode == 3 || push.backgroundMode == 4) && f.atmosphere != 0) {
      // The painting aligned to the sky (the user, 2026-10-19: "alinhar essas imagens de fundo com o céu ... cortando a parte
      // inferior da imagem para a parte com fumaça ficar visível abaixo do sol"): its horizon row pinned to the world's horizon,
      // its rows at their own angular scale (no stretch), the band below the horizon cut at `bottom` (the cracked ground goes;
      // below it the bottom row fades into the sky's horizon colour), its width spanning `span` degrees of azimuth turned so
      // its own sun glow sits under the real sun (mirrored beyond its edges, so the camera may turn all the way round), and its
      // own sky matted out by colour against its top rows (towers and smoke rising above the ridge stay).
      const AtmosphereFrame a = loadAtmosphere(f);
      const float3 dir = skyDirection(f, a, float2(pixel) + 0.5);
      // The painting's horizon row sits at `fitB.w` degrees of elevation (below the real horizon: the scenery lower than the board's
      // eye line); rows above it at the painting's angular scale; below it, the kept band (to `bottom`) reaches `bottomDegrees`
      // further down (the user, 2026-10-19: "use the whole image and just add atmosphere and sky on the top layer": bottom = 1).
      const float realElevation = asin(clamp(dir.y, -1.0, 1.0));
      const float elevation = realElevation - radians(push.fitB.w);
      const float horizon = push.fitA.x, bottom = push.fitA.y, skyTop = push.fitB.y;
      // Below the horizon, a painting kept whole reaches its last row exactly at the nadir, one continuous image at any tilt (no
      // repeat, no seam: the user's "remenda" at the game's angle): near the horizon its rows keep the `bottomDegrees` scale (the
      // low views the user approved), then the ground compresses smoothly down to the nadir. A cut painting keeps the linear map.
      float below = -elevation / radians(max(push.fitA.w, 1.0));
      if (bottom >= 0.999) {
        const float toNadir = radians(90.0 + push.fitB.w) / radians(max(push.fitA.w, 1.0));
        below = (1.0 - exp(-below)) / max(1.0 - exp(-toNadir), 1e-3);
      }
      const float v = elevation >= 0.0 ? horizon - elevation / radians(max(push.fitA.z, 1.0)) : horizon + below * (bottom - horizon);
      // What fills the painting's matted sky: the real sky above the real horizon; below it (down to the painting's ridge) the
      // sky's own horizon colour, never the sky table's dark ground.
      const float3 horizonDir = normalize(float3(dir.x, 0.002, dir.z));
      const float3 skyHorizon = skyRadiance(a, horizonDir) * a.skyScale * f.exposure;
      const float3 sky = lerp(skyHorizon, skyBackground(f, pixel), smoothstep(-0.004, 0.012, realElevation));
      if (v < 0.0) return float4(volumeOverBackground(f, sky, pixel), 1.0);
      float x = atan2(dir.z, dir.x) - push.fitB.x;
      x -= 2.0 * kPi * round(x / (2.0 * kPi));
      x = 0.5 + x / radians(max(push.fitB.z, 1.0));
      x -= 2.0 * floor(x * 0.5);  // mirrored repeat: 0..2 -> 0..1..0
      const float u = x <= 1.0 ? x : 2.0 - x;
      Texture2D painting = gTextures[NonUniformResourceIndex(push.background)];
      SamplerState s = gSamplers[f.materialSampler];
      const float2 puv = float2(u, min(v, min(bottom + 0.15, 1.0)));
      const float4 paintSample = painting.SampleLevel(s, puv, 0);
      const float3 paint = paintSample.rgb;
      // The painting's own sky at this column (its top rows): what is close to it, above the ridge, is sky.
      const float3 skyRef = (painting.SampleLevel(s, float2(u, skyTop * 0.5), 0).rgb + painting.SampleLevel(s, float2(u, skyTop), 0).rgb +
                             painting.SampleLevel(s, float2(u, skyTop + 0.04), 0).rgb) / 3.0;
      const float distance = length(paint - skyRef) / max(dot(skyRef, float3(0.2126, 0.7152, 0.0722)), 0.03);
      float scene = smoothstep(0.18, 0.45, distance);
      scene = lerp(scene, 1.0, smoothstep(horizon - 0.04, horizon + 0.01, v));  // from the ridge down: all painting
      scene *= smoothstep(0.0, max(skyTop, 0.01), v);                             // the very top: all sky
      if (push.backgroundMode == 3) scene = smoothstep(0.0, 0.08, v);              // its own sky kept, top rows fade
      if (push.backgroundMode == 4) scene = paintSample.a;                         // its own alpha: clear sky = the real sky
      float3 color = lerp(sky, paint, scene);
      // Below the kept band: the bottom row fades into the sky's own horizon colour; a thin haze on the seam.
      // (a painting kept whole ends at its last row and stays there; a cut one fades into the horizon colour)
      if (bottom < 0.999 && v > bottom - 0.08) color = lerp(color, skyHorizon, smoothstep(bottom - 0.08, bottom + 0.15, v));
      color = lerp(color, skyHorizon, 0.3 * exp(-abs(elevation) / radians(1.0)));
      // The ground around the board: a real flat plane under it (planeY) carrying the map's top-down ground image (planeTexture:
      // the user's own art, e.g. content/backdrops/aldeia-ground-v01.jpg; the user: "AldeiaGround ... for the top view"), its
      // width `planeTile` world units, mirrored at its edges so copies meet without a seam; the painting takes over where the
      // plane lies far away, towards the horizon (the approved low views keep it). So no view looks into a pit (the user: "visão
      // do inferno não aceito") and nothing folds at the game's angle (the "remenda").
      if (realElevation < 0.0 && push.planeTexture != kNoTexture) {
        const float t = (push.planeY - f.cameraPosition.y) / min(dir.y, -1e-4);
        if (t > 0.0) {
          const float2 hit = f.cameraPosition.xz + dir.xz * t;
          Texture2D tile = gTextures[NonUniformResourceIndex(push.planeTexture)];
          uint tw, th;
          tile.GetDimensions(tw, th);
          const float2 size = float2(push.planeTile, push.planeTile * float(th) / max(float(tw), 1.0));
          float2 m = hit / size + 0.5;
          m -= 2.0 * floor(m * 0.5);  // mirrored repeat: 0..2 -> 0..1..0
          m = float2(m.x <= 1.0 ? m.x : 2.0 - m.x, m.y <= 1.0 ? m.y : 2.0 - m.y);
          // The mip that matches this pixel's footprint on the plane (one pixel's angle, stretched by the grazing view).
          const float pixelAngle = length(skyDirection(f, a, float2(pixel) + float2(1.5, 0.5)) - dir);
          const float footprint = t * pixelAngle / max(-dir.y, 0.05);
          const float lod = max(log2(footprint / (size.x / float(max(tw, 1u)))), 0.0);
          float3 ground = tile.SampleLevel(s, m, lod).rgb;
          // Distance haze into the horizon colour, and the hand-over to the painting where the plane lies far.
          // (Tática, planeFar huge: the camera stands far above, the image is seen as it is, no haze)
          if (push.planeFar < 1.0e8) ground = lerp(ground, skyHorizon, 0.55 * (1.0 - exp(-t / 1200.0)));
          // Only where the view looks clearly down (10-25 deg): the low views the user approved, whose rays below the horizon
          // stay within a few degrees of it, keep the painting exactly as it was.
          const float far = max(smoothstep(push.planeFar * 0.55, push.planeFar, t), 1.0 - smoothstep(radians(push.planeFrom), radians(push.planeFrom * 2.5), -realElevation));
          color = lerp(ground, color, far);
        }
      }
      // The volumetric air only above the horizon: a ray looking down runs under the board's ground in the froxel volume and
      // turns opaque (the flat tan band in the corner at the game's angle, the user's "vazio"); the painting carries its own air.
      return float4(lerp(color, volumeOverBackground(f, color, pixel), smoothstep(-0.03, 0.0, realElevation)), 1.0);
    }
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
