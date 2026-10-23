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
  uint ceilingTexture;   // modes 2-4 and 6: a room's drawn ceiling where the sky would show (kNoTexture = the sky)
  float ceilingHeight;   // ... a flat plane this far above the eye
  float ceilingTile;     // ... world units per repeat
};
// Backdrop mode 1: the painting's own horizon sits at this share of its height (the row the world's horizon is pinned
// to), and the painting reaches its bottom row this many degrees below the horizon.
static const float kBackdropHorizon = 0.5;
static const float kBackdropDownDegrees = 30.0;
[[vk::push_constant]] Push push;
static const uint kBlackBackground = 0xFFFFFFFEu;

// The real sky with the volumetric air over it only near the horizon: the air's haze fades out over the first few degrees of elevation, so it
// never hides the sun, the clouds or the stars above (the user, 2026-10-20: "onde está o sol": the froxel haze, thick along the board, turned the whole
// sky in the sun's direction into an orange wash and attenuated the disc to nothing).
float3 skyWithAir(MeshFrame f, uint2 pixel) {
  const AtmosphereFrame a = loadAtmosphere(f);
  const float3 dir = skyDirection(f, a, float2(pixel) + 0.5);
  const float3 sky = skyBackground(f, pixel);
  const float elevation = asin(clamp(dir.y, -1.0, 1.0));
  return lerp(sky, volumeOverBackground(f, sky, pixel), exp(-max(elevation, 0.0) / radians(2.5)));
}

// A room's drawn ceiling (the user, 2026-10-22: "Tavern Ceiling ... add it to the ceiling on both levels"; CLAUDE.md: "Internos também
// ficam sem o céu, terão um teto desenhado"): where the sky would show, the image on a flat plane ceilingHeight above the eye (the game's
// camera stands far above the board, so a plane at the room's own height would never be seen from it), ceilingTile world units across,
// mirrored at its edges, its beams converging toward the horizon; it darkens into the room's depth, black at the horizon.
float3 ceilingAt(MeshFrame f, float3 dir, float pixelAngle) {
  if (dir.y <= 1e-4) return 0.0;
  const float t = push.ceilingHeight / dir.y;
  const float2 hit = f.cameraPosition.xz + dir.xz * t;
  Texture2D tile = gTextures[NonUniformResourceIndex(push.ceilingTexture)];
  uint tw, th;
  tile.GetDimensions(tw, th);
  const float2 size = float2(push.ceilingTile, push.ceilingTile * float(th) / max(float(tw), 1.0));
  float2 m = hit / size + 0.5;
  m -= 2.0 * floor(m * 0.5);  // mirrored repeat: 0..2 -> 0..1..0
  m = float2(m.x <= 1.0 ? m.x : 2.0 - m.x, m.y <= 1.0 ? m.y : 2.0 - m.y);
  const float footprint = t * pixelAngle / max(dir.y, 0.05);
  const float lod = max(log2(footprint / (size.x / float(max(tw, 1u)))), 0.0);
  return tile.SampleLevel(gSamplers[f.materialSampler], m, lod).rgb;  // no darkening (the user, 2026-10-22: no black line)
}
float3 ceilingAt(MeshFrame f, AtmosphereFrame a, float3 dir, uint2 pixel) {
  return ceilingAt(f, dir, length(skyDirection(f, a, float2(pixel) + float2(1.5, 0.5)) - dir));
}

// The view's direction through a pixel from the camera alone (no atmosphere needed: a room keeps the Atmosfera off). MeshFrame.view
// maps world to view space looking down -z (scene.hlsli sphereVisibleHiZ), Vulkan's NDC y points down.
float3 viewDirection(MeshFrame f, float2 pixel) {
  const float2 ndc = pixel / f.screenSize * 2.0 - 1.0;
  const float3 v = normalize(float3(ndc.x / f.projectionX, -ndc.y / f.projectionY, -1.0));
  return normalize(mul((float3x3)f.view, v));
}

// Mode 8 = a room's panorama (the tavern's floors, the user, 2026-10-22: "Tavern Ceiling ... on both levels", InnsecondFloor "for making a
// panorama"): the painting from its horizon row down (mode 5's mapping), the map's ground image on a plane around the board (planeTexture,
// the painting beyond planeFar), the drawn ceiling above the horizon; nothing of the sky or the air, so it works with the Atmosfera off.
float4 roomBackground(MeshFrame f, uint2 pixel) {
  const float3 dir = viewDirection(f, float2(pixel) + 0.5);
  const float pixelAngle = length(viewDirection(f, float2(pixel) + float2(1.5, 0.5)) - dir);
  uint w, h;
  gTextures[NonUniformResourceIndex(push.background)].GetDimensions(w, h);
  SamplerState s = gSamplers[f.materialSampler];
  const float elevation = asin(clamp(dir.y, -1.0, 1.0));
  const float3 dl = viewDirection(f, float2(0.5, f.screenSize.y * 0.5)), dr = viewDirection(f, float2(f.screenSize.x - 0.5, f.screenSize.y * 0.5));
  const float imageHeightRad = acos(clamp(dot(dl, dr), -1.0, 1.0)) * float(h) / max(float(w), 1.0);
  const float horizonRow = clamp(push.fitA.x, 0.02, 0.98);
  if (elevation >= 0.0) {
    // The whole painting above the horizon too, its own ceiling included, at its own scale (the user, 2026-10-22: "leave the full image",
    // "we cant have this black line"); above its top edge the drawn ceiling, the painting's top rows fading into it.
    const float v = horizonRow - elevation / max(imageHeightRad, 1e-3);
    const float3 ceiling = push.ceilingTexture != kNoTexture ? ceilingAt(f, dir, pixelAngle) : 0.0;
    if (v <= 0.0) return float4(ceiling, 1.0);
    const float3 top = gTextures[NonUniformResourceIndex(push.background)].SampleLevel(s, float2((float(pixel.x) + 0.5) / f.screenSize.x, v), 0).rgb;
    return float4(push.ceilingTexture != kNoTexture ? lerp(ceiling, top, smoothstep(0.0, 0.06, v)) : top, 1.0);
  }
  const float span = (1.0 - horizonRow) * imageHeightRad, toNadir = (1.5707963 / max(span, 1e-3));
  const float pv = saturate(horizonRow + (1.0 - horizonRow) * (1.0 - exp(elevation / max(span, 1e-3))) / max(1.0 - exp(-toNadir), 1e-3));
  const float3 painting = gTextures[NonUniformResourceIndex(push.background)].SampleLevel(s, float2((float(pixel.x) + 0.5) / f.screenSize.x, pv), 0).rgb;
  if (push.planeTexture == kNoTexture) return float4(painting, 1.0);
  const float t = (push.planeY - f.cameraPosition.y) / min(dir.y, -1e-4);
  if (t <= 0.0) return float4(painting, 1.0);
  const float2 hit = f.cameraPosition.xz + dir.xz * t;
  Texture2D tile = gTextures[NonUniformResourceIndex(push.planeTexture)];
  uint tw, th;
  tile.GetDimensions(tw, th);
  const float2 size = float2(push.planeTile, push.planeTile * float(th) / max(float(tw), 1.0));
  float2 m = hit / size + 0.5;
  m -= 2.0 * floor(m * 0.5);  // mirrored repeat: 0..2 -> 0..1..0
  m = float2(m.x <= 1.0 ? m.x : 2.0 - m.x, m.y <= 1.0 ? m.y : 2.0 - m.y);
  const float footprint = t * pixelAngle / max(-dir.y, 0.05);
  const float lod = max(log2(footprint / (size.x / float(max(tw, 1u)))), 0.0);
  const float3 ground = tile.SampleLevel(s, m, lod).rgb;
  return float4(lerp(ground, painting, smoothstep(push.planeFar * 0.55, push.planeFar, t)), 1.0);
}

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
  if (push.background != kNoTexture && push.backgroundMode == 8) return roomBackground(f, pixel);
  if (push.background != kNoTexture) {
    uint w, h;
    gTextures[NonUniformResourceIndex(push.background)].GetDimensions(w, h);
    float2 uv = (float2(pixel) + 0.5) / f.screenSize;
    const float screenAspect = f.screenSize.x / f.screenSize.y, imageAspect = float(w) / float(h);
    if (screenAspect > imageAspect) uv.y = 0.5 + (uv.y - 0.5) * imageAspect / screenAspect;
    else uv.x = 0.5 + (uv.x - 0.5) * screenAspect / imageAspect;
    // mode 5 = the regular image, opaque, from its horizon row down (the user, 2026-10-20: "the regular image static at the line of the horizon,
    // NOT melting, no transparency"): its horizon row on the world's horizon line, its width spanning the screen (the image's own proportions,
    // no stretch), the real sky above the line, nothing blended; it moves with the camera's tilt only, as a far backdrop does. The painting's own
    // colours, with the volumetric air in front of it like the plain cover-fit.
    // mode 6 = the map's painting laid flat far under the board as its ground (A Ponte de Pedra 023, the user, 2026-10-21: "use the current
    // background image as a Ground Floor Image": the gorge under the bridge): a real plane at planeY carrying the painting, planeTile world units
    // wide, mirrored at its edges; the real sky above the horizon; where the plane lies far the regular painting of mode 5 takes over on the
    // horizon. Its own branch: mode 5 is unchanged.
    // mode 7 = an interior's ground (Luz das janelas, the user, 2026-10-21: "não renderizar nada do lado de fora ... exceto pelo ground and
    // ceiling images"; the watchtower's background "usar ele de imagem de GROUND"): the map's painting on a real plane at planeY, planeTile world
    // units wide, mirrored at its edges, its own colours; nothing else outside: black above the horizon, no sky, no haze.
    if (push.backgroundMode == 7) {
      if (f.atmosphere == 0 || push.planeTexture == kNoTexture) return float4(0.0, 0.0, 0.0, 1.0);
      const AtmosphereFrame a = loadAtmosphere(f);
      const float3 dir = skyDirection(f, a, float2(pixel) + 0.5);
      if (dir.y >= -1e-4) return float4(0.0, 0.0, 0.0, 1.0);
      const float t = (push.planeY - f.cameraPosition.y) / dir.y;
      if (t <= 0.0) return float4(0.0, 0.0, 0.0, 1.0);
      const float2 hit = f.cameraPosition.xz + dir.xz * t;
      Texture2D tile = gTextures[NonUniformResourceIndex(push.planeTexture)];
      uint tw, th;
      tile.GetDimensions(tw, th);
      const float2 size = float2(push.planeTile, push.planeTile * float(th) / max(float(tw), 1.0));
      float2 m = hit / size + 0.5;
      m -= 2.0 * floor(m * 0.5);  // mirrored repeat: 0..2 -> 0..1..0
      m = float2(m.x <= 1.0 ? m.x : 2.0 - m.x, m.y <= 1.0 ? m.y : 2.0 - m.y);
      const float pixelAngle = length(skyDirection(f, a, float2(pixel) + float2(1.5, 0.5)) - dir);
      const float footprint = t * pixelAngle / max(-dir.y, 0.05);
      const float lod = max(log2(footprint / (size.x / float(max(tw, 1u)))), 0.0);
      return float4(tile.SampleLevel(gSamplers[f.materialSampler], m, lod).rgb, 1.0);
    }
    if (push.backgroundMode == 6 && f.atmosphere != 0) {
      const AtmosphereFrame a = loadAtmosphere(f);
      const float3 dir = skyDirection(f, a, float2(pixel) + 0.5);
      const float elevation = asin(clamp(dir.y, -1.0, 1.0));
      // a room (a panorama with a drawn ceiling, the tavern's floors): the ceiling above the horizon, never the sky
      if (elevation >= 0.0) return float4(push.ceilingTexture != kNoTexture ? ceilingAt(f, a, dir, pixel) : skyWithAir(f, pixel), 1.0);
      SamplerState s = gSamplers[f.materialSampler];
      // the regular painting from its horizon row down (mode 5's mapping), for the far band
      const float3 dl = skyDirection(f, a, float2(0.5, f.screenSize.y * 0.5)), dr = skyDirection(f, a, float2(f.screenSize.x - 0.5, f.screenSize.y * 0.5));
      const float imageHeightRad = acos(clamp(dot(dl, dr), -1.0, 1.0)) * float(h) / max(float(w), 1.0);
      const float horizonRow = clamp(push.fitA.x, 0.02, 0.98);
      const float span = (1.0 - horizonRow) * imageHeightRad, toNadir = (1.5707963 / max(span, 1e-3));
      const float pv = saturate(horizonRow + (1.0 - horizonRow) * (1.0 - exp(elevation / max(span, 1e-3))) / max(1.0 - exp(-toNadir), 1e-3));
      const float3 farPainting = gTextures[NonUniformResourceIndex(push.background)].SampleLevel(s, float2((float(pixel.x) + 0.5) / f.screenSize.x, pv), 0).rgb;
      float3 color = farPainting;
      if (push.planeTexture != kNoTexture) {
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
          const float pixelAngle = length(skyDirection(f, a, float2(pixel) + float2(1.5, 0.5)) - dir);
          const float footprint = t * pixelAngle / max(-dir.y, 0.05);
          const float lod = max(log2(footprint / (size.x / float(max(tw, 1u)))), 0.0);
          float3 ground = tile.SampleLevel(s, m, lod).rgb;
          const float3 horizonDir = normalize(float3(dir.x, 0.002, dir.z));
          const float3 skyHorizon = skyRadiance(a, horizonDir) * a.skyScale * f.exposure;
          ground = lerp(ground, skyHorizon, 0.45 * (1.0 - exp(-t / 900.0)));
          color = lerp(ground, farPainting, smoothstep(push.planeFar * 0.55, push.planeFar, t));
        }
      }
      return float4(color, 1.0);
    }
    if (push.backgroundMode == 5 && f.atmosphere != 0) {
      const AtmosphereFrame a = loadAtmosphere(f);
      const float3 dir = skyDirection(f, a, float2(pixel) + 0.5);
      const float elevation = asin(clamp(dir.y, -1.0, 1.0));
      const float3 dl = skyDirection(f, a, float2(0.5, f.screenSize.y * 0.5)), dr = skyDirection(f, a, float2(f.screenSize.x - 0.5, f.screenSize.y * 0.5));
      const float imageHeightRad = acos(clamp(dot(dl, dr), -1.0, 1.0)) * float(h) / max(float(w), 1.0);
      // The real sky above the world's horizon (the sun, the clouds, the stars: the user, 2026-10-20: "I NEED THE SKY ON ALL OUTDOORS MAPS"), the
      // painting from its horizon row down, a clean cut on the horizon line. Below it the rows keep the image's own scale near the horizon and
      // compress smoothly toward the nadir, so the last row lands exactly there (one continuous image at any tilt: no clamped smear).
      if (elevation >= 0.0) return float4(skyWithAir(f, pixel), 1.0);
      const float horizonRow = clamp(push.fitA.x, 0.02, 0.98);
      const float span = (1.0 - horizonRow) * imageHeightRad, toNadir = (1.5707963 / max(span, 1e-3));
      const float v = saturate(horizonRow + (1.0 - horizonRow) * (1.0 - exp(elevation / max(span, 1e-3))) / max(1.0 - exp(-toNadir), 1e-3));
      const float u = (float(pixel.x) + 0.5) / f.screenSize.x;
      const float3 painting = gTextures[NonUniformResourceIndex(push.background)].SampleLevel(gSamplers[f.materialSampler], float2(u, v), 0).rgb;
      return float4(volumeOverBackground(f, painting, pixel), 1.0);
    }
    // mode 3 = mode 2 keeping the painting's own sky (aldeia's fire panorama: its smoke sky is the look), the real sky only
    // above the painting's top edge, faded in over its top rows.
    // mode 4 = mode 2 for a painting whose sky is truly transparent (its alpha: the user's "Paisagem Devastada", 2026-10-19): the
    // real sky shows wherever the image is clear, through its smoke where it is half clear; no matte by colour.
    // A room with a drawn ceiling (the tavern: the Atmosfera OFF, the user, 2026-10-22: "é para essa merda se comportar como todos os outros
    // mapas") takes this same path without the sky: the view direction from the camera, the ceiling where the sky would be, no air.
    if ((push.backgroundMode == 2 || push.backgroundMode == 3 || push.backgroundMode == 4) && (f.atmosphere != 0 || push.ceilingTexture != kNoTexture)) {
      // The painting aligned to the sky (the user, 2026-10-19: "alinhar essas imagens de fundo com o céu ... cortando a parte
      // inferior da imagem para a parte com fumaça ficar visível abaixo do sol"): its horizon row pinned to the world's horizon,
      // its rows at their own angular scale (no stretch), the band below the horizon cut at `bottom` (the cracked ground goes;
      // below it the bottom row fades into the sky's horizon colour), its width spanning `span` degrees of azimuth turned so
      // its own sun glow sits under the real sun (mirrored beyond its edges, so the camera may turn all the way round), and its
      // own sky matted out by colour against its top rows (towers and smoke rising above the ridge stay).
      const bool air = f.atmosphere != 0;
      AtmosphereFrame a = (AtmosphereFrame)0;
      float3 dir, dirNext;
      if (air) {
        a = loadAtmosphere(f);
        dir = skyDirection(f, a, float2(pixel) + 0.5);
        dirNext = skyDirection(f, a, float2(pixel) + float2(1.5, 0.5));
      } else {
        dir = viewDirection(f, float2(pixel) + 0.5);
        dirNext = viewDirection(f, float2(pixel) + float2(1.5, 0.5));
      }
      const float pixelAngle = length(dirNext - dir);
      const float airShare = air ? 1.0 : 0.0;  // the sky's horizon haze: none without the air
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
      float3 skyHorizon = 0.0, sky = 0.0;
      if (air) {
        const float3 horizonDir = normalize(float3(dir.x, 0.002, dir.z));
        skyHorizon = skyRadiance(a, horizonDir) * a.skyScale * f.exposure;
        sky = lerp(skyHorizon, skyBackground(f, pixel), smoothstep(-0.004, 0.012, realElevation));
      }
      if (push.ceilingTexture != kNoTexture) sky = ceilingAt(f, dir, pixelAngle);  // a room: its drawn ceiling, never the sky
      if (v < 0.0) {
        if (push.ceilingTexture != kNoTexture) return float4(sky, 1.0);
        return float4(volumeOverBackground(f, sky, pixel), 1.0);
      }
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
      if (bottom < 0.999 && v > bottom - 0.08) color = lerp(color, skyHorizon, airShare * smoothstep(bottom - 0.08, bottom + 0.15, v));
      color = lerp(color, skyHorizon, airShare * 0.3 * exp(-abs(elevation) / radians(1.0)));
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
          const float footprint = t * pixelAngle / max(-dir.y, 0.05);
          const float lod = max(log2(footprint / (size.x / float(max(tw, 1u)))), 0.0);
          float3 ground = tile.SampleLevel(s, m, lod).rgb;
          // Distance haze into the horizon colour, and the hand-over to the painting where the plane lies far.
          // (Tática, planeFar huge: the camera stands far above, the image is seen as it is, no haze)
          if (push.planeFar < 1.0e8) ground = lerp(ground, skyHorizon, airShare * 0.55 * (1.0 - exp(-t / 1200.0)));
          // Only where the view looks clearly down (10-25 deg): the low views the user approved, whose rays below the horizon
          // stay within a few degrees of it, keep the painting exactly as it was.
          const float far = max(smoothstep(push.planeFar * 0.55, push.planeFar, t), 1.0 - smoothstep(radians(push.planeFrom), radians(push.planeFrom * 2.5), -realElevation));
          color = lerp(ground, color, far);
        }
      }
      // The volumetric air only above the horizon: a ray looking down runs under the board's ground in the froxel volume and
      // turns opaque (the flat tan band in the corner at the game's angle, the user's "vazio"); the painting carries its own air.
      if (!air) return float4(color, 1.0);
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
  if (f.atmosphere != 0) return float4(skyWithAir(f, pixel), 1.0);
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
