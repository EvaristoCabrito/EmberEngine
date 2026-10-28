#include "render/FogLooks.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace ember {

namespace {

glm::vec3 srgb(uint32_t hex) {
  const auto c = [](uint32_t v) {
    const float x = static_cast<float>(v) / 255.0f;
    return x <= 0.04045f ? x / 12.92f : std::pow((x + 0.055f) / 1.055f, 2.4f);
  };
  return {c((hex >> 16) & 255), c((hex >> 8) & 255), c(hex & 255)};
}

// --- Looks 001 (176-byte frame), 002 and 003 (192-byte frame): shaders/fog_look001..003.hlsl -----------------

struct Look001Frame {
  glm::mat4 inverseViewProjection;
  glm::vec2 boardCenter;
  float time;
  float baseY;
  float topY;
  float extinction;
  float fadeRadius;
  float rememberedCover;
  glm::vec3 wind;
  float noiseScale;
  glm::vec3 albedo;
  float ambient;
  uint32_t noise, depth, march, marchSampled, color, frameIndex;
  glm::uvec2 halfSize, fullSize, pad;
};
static_assert(sizeof(Look001Frame) == 176);

struct Look003Frame {
  glm::mat4 inverseViewProjection;
  glm::vec2 boardCenter;
  float time;
  float baseY;
  float topY;
  float extinction;
  float fadeRadius;
  float rememberedCover;
  glm::vec3 wind;
  float noiseScale;
  float scaleHeight;
  glm::vec3 pad0;
  glm::vec3 albedo;
  float ambient;
  uint32_t noise, depth, march, marchSampled, color, frameIndex;
  glm::uvec2 halfSize, fullSize, pad;
};
static_assert(sizeof(Look003Frame) == 192);

template <class F>
void common(const FogLookInputs& in, F& f) {
  f.inverseViewProjection = in.inverseViewProjection;
  f.time = in.time;
  f.noise = in.noise;
  f.depth = in.depth;
  f.march = in.march;
  f.marchSampled = in.marchSampled;
  f.color = in.color;
  f.frameIndex = in.frameIndex;
  f.halfSize = in.halfSize;
  f.fullSize = in.fullSize;
}

// 001: the first bank, as it was drawn (thick billows, self-shadowed, bright).
void writeLook001(const FogLookInputs& in, void* out) {
  Look001Frame f{};
  common(in, f);
  f.boardCenter = in.scene.boardCenter;
  f.baseY = in.scene.meanGround - 1.0f;
  f.topY = in.scene.groundHigh + 2.5f;
  f.fadeRadius = in.scene.boardRadius * 2.2f;
  f.extinction = 2.2f;
  f.rememberedCover = 0.35f;
  f.wind = {0.35f, 0.0f, 0.15f};
  f.noiseScale = 0.09f;
  f.albedo = {0.82f, 0.84f, 0.88f};
  f.ambient = 1.0f;
  std::memcpy(out, &f, sizeof(f));
}

// 002: the soft, even grey sheet, as it was drawn.
void writeLook002(const FogLookInputs& in, void* out) {
  Look003Frame f{};
  common(in, f);
  f.boardCenter = in.scene.boardCenter;
  f.baseY = in.scene.meanGround - 1.0f;
  f.topY = in.scene.groundHigh + 4.0f;
  f.fadeRadius = in.scene.boardRadius * 2.2f;
  f.extinction = 0.9f;
  f.scaleHeight = 1.6f;
  f.rememberedCover = 0.3f;
  f.wind = {0.35f, 0.0f, 0.15f};
  f.noiseScale = 0.07f;
  f.albedo = {0.5f, 0.52f, 0.56f};
  f.ambient = 0.8f;
  std::memcpy(out, &f, sizeof(f));
}

// 003: the look the user kept (looks/fog/003/capture.png), as it was drawn.
void writeLook003(const FogLookInputs& in, void* out) {
  Look003Frame f{};
  common(in, f);
  f.boardCenter = in.scene.boardCenter;
  f.baseY = in.scene.meanGround - 1.0f;
  f.topY = in.scene.groundHigh + 3.0f;
  f.fadeRadius = in.scene.boardRadius * 2.2f;
  f.extinction = 0.75f;
  f.scaleHeight = 1.1f;
  f.rememberedCover = 0.3f;
  f.wind = {0.35f, 0.0f, 0.15f};
  f.noiseScale = 0.06f;
  f.albedo = {0.5f, 0.52f, 0.56f};
  f.ambient = 0.8f;
  std::memcpy(out, &f, sizeof(f));
}

// --- Look 008 (192-byte frame): 003 thinning away near the camera (shaders/fog_look008.hlsl) -------------------

struct Look008Frame {
  glm::mat4 inverseViewProjection;
  glm::vec2 boardCenter;
  float time;
  float baseY;
  float topY;
  float extinction;
  float fadeRadius;
  float rememberedCover;
  glm::vec3 wind;
  float noiseScale;
  float scaleHeight;
  float nearFadeStart, nearFadeEnd, pad0;
  glm::vec3 albedo;
  float ambient;
  uint32_t noise, depth, march, marchSampled, color, frameIndex;
  glm::uvec2 halfSize, fullSize, pad;
};
static_assert(sizeof(Look008Frame) == 192);

// 008: 003's settings; the fog along each ray grows from nothing 2 units from the eye to whole at 14 (about 8 hexes),
// so a camera that has come down into the bank no longer sees it over everything (the user's white / yellow veil).
void writeLook008(const FogLookInputs& in, void* out) {
  Look008Frame f{};
  common(in, f);
  f.boardCenter = in.scene.boardCenter;
  f.baseY = in.scene.meanGround - 1.0f;
  f.topY = in.scene.groundHigh + 3.0f;
  f.fadeRadius = in.scene.boardRadius * 2.2f;
  f.extinction = 0.75f;
  f.scaleHeight = 1.1f;
  f.nearFadeStart = 2.0f;
  f.nearFadeEnd = 14.0f;
  f.rememberedCover = 0.3f;
  f.wind = {0.35f, 0.0f, 0.15f};
  f.noiseScale = 0.06f;
  f.albedo = {0.5f, 0.52f, 0.56f};
  f.ambient = 0.8f;
  std::memcpy(out, &f, sizeof(f));
}

// --- Look 010 (192-byte frame): the gorge's wind fog, outside the bridge walls (A Ponte de Pedra 023) -------------

struct Look010Frame {
  glm::mat4 inverseViewProjection;
  glm::vec4 deck;  // x0, z0, x1, z1: the board (its long sides are the bridge walls)
  float time, deckY, floorY, topY;
  glm::vec3 wind;  // the gorge's wind, world units per second
  float gust;      // how strongly the gusts push on top of it
  float extinction, noiseScale, wallMargin, nearFadeEnd;
  glm::vec3 albedo;
  float ambient;
  uint32_t noise, depth, march, marchSampled, color, frameIndex;
  glm::uvec2 halfSize, fullSize, pad;
};
static_assert(sizeof(Look010Frame) == 192);

// 010 (the user, 2026-10-21: "a strong volumetric fog with strong gusts of wind flowing outside of the bridge walls"): the gorge
// filled with fog from its floor up past the parapets, the wind blowing along the gorge (across the bridge, under it), gusts
// sweeping through; the deck itself stays clear.
void writeLook010(const FogLookInputs& in, void* out) {
  Look010Frame f{};
  common(in, f);
  f.deck = in.scene.board;
  f.deckY = in.scene.meanGround;
  f.floorY = in.scene.floorY;
  f.topY = in.scene.groundHigh + 5.0f;
  f.wind = {7.5f, 0.0f, 1.2f};
  f.gust = 1.0f;
  f.extinction = 0.13f;
  f.noiseScale = 0.05f;
  f.wallMargin = 1.2f;
  f.nearFadeEnd = 6.0f;
  f.albedo = {0.74f, 0.77f, 0.83f};
  f.ambient = 0.7f;
  std::memcpy(out, &f, sizeof(f));
}

// --- Looks 004 and 005 (240-byte frame): Ember's mist model, 005 with the swirl ------------------------------

struct Look004Frame {
  glm::mat4 inverseViewProjection;
  glm::vec4 board;
  float time, speed, density, thickness;
  float base, top, pool, heightPow;
  glm::vec3 scale;
  float floorShare;
  glm::vec3 wind;
  float reach;
  glm::vec2 contrast;
  glm::vec2 groundRange;
  glm::vec3 albedo;
  float rememberedCover;
  float ambientShare, sunShare, lightShare, lightReach;
  uint32_t noise, depth, march, marchSampled, color, frameIndex;
  glm::uvec2 halfSize, fullSize, pad;
};
static_assert(sizeof(Look004Frame) == 240);

// 004: Ember's "fog1" mist at full intensity (src/game/mist3d.ts, src/render/volumetricFog.ts).
Look004Frame classicMist(const FogLookInputs& in) {
  Look004Frame f{};
  common(in, f);
  f.board = in.scene.board;
  f.speed = 1.0f;
  f.density = 1.4f;
  f.thickness = 3.0f;
  f.base = in.scene.groundLow;
  f.top = in.scene.groundHigh + f.thickness;
  f.pool = 0.8f;
  f.heightPow = 1.0f;
  f.scale = {0.2f, 0.4f, 0.2f};
  f.floorShare = 0.4f;
  f.wind = {0.04f, 0.01f, 0.03f};
  f.reach = 14.0f;
  f.contrast = {0.25f, 0.8f};
  f.groundRange = {in.scene.groundLow, in.scene.groundHigh};
  f.albedo = srgb(0xa9aaab);
  f.rememberedCover = 0.35f;
  f.ambientShare = 0.55f;
  f.sunShare = 0.12f;
  f.lightShare = 0.006f;
  f.lightReach = 0.45f;
  return f;
}
void writeLook004(const FogLookInputs& in, void* out) {
  const Look004Frame f = classicMist(in);
  std::memcpy(out, &f, sizeof(f));
}

// 005: the swirl (look 004's mist turning slowly round the party's clearing).
struct Look005Frame {
  Look004Frame mist;
  glm::vec2 swirlCenter;
  float swirlRate;    // radians per second at the centre
  float swirlFalloff; // the turn slows as 1 / (1 + r * swirlFalloff)
  float spiral;       // inward drift of the pattern, world units per second
  float period;       // seconds between the two cross-faded phases (no winding up)
  glm::vec2 pad;
};
static_assert(sizeof(Look005Frame) == 272);
void writeLook005(const FogLookInputs& in, void* out) {
  Look005Frame f{};
  f.mist = classicMist(in);
  f.swirlCenter = in.scene.partyCenter;
  f.swirlRate = 0.12f;
  f.swirlFalloff = 0.08f;
  f.spiral = 0.15f;
  f.period = 24.0f;
  std::memcpy(out, &f, sizeof(f));
}

// Ember's mist4 values as on its fog-of-war maps (cemiterio-esquecidos003: intensity 0.3, speed 0.6).
Look004Frame classicMist4(const FogLookInputs& in) {
  Look004Frame f = classicMist(in);
  f.speed = 0.6f;
  f.density = 0.9f * std::pow(0.3f, 1.5f);  // Ember: preset density x intensity^1.5
  f.thickness = 3.5f;
  f.top = in.scene.groundHigh + f.thickness;
  f.scale = {0.18f, 0.35f, 0.18f};
  f.wind = {0.03f, 0.01f, 0.02f};
  f.contrast = {0.3f, 0.75f};
  f.floorShare = 0.2f;
  f.heightPow = 1.3f;
  f.reach = 10.0f;
  return f;
}

// 006: Ember's mist4 as in the user's reference.
struct Look006Frame {
  Look004Frame mist;
  glm::vec2 center;
  float swirl, clearRadius, edgeRadius;
  glm::vec3 pad;
};
static_assert(sizeof(Look006Frame) == 272);
void writeLook006(const FogLookInputs& in, void* out) {
  Look006Frame f{};
  f.mist = classicMist4(in);
  f.center = in.scene.boardCenter;
  const float half = std::min(in.scene.board.z - in.scene.board.x, in.scene.board.w - in.scene.board.y) * 0.5f;
  f.swirl = 0.12f;
  f.clearRadius = half * 0.45f;
  f.edgeRadius = half * 1.1f;
  std::memcpy(out, &f, sizeof(f));
}

// 007: look 005's swirl on mist4's real values, over the fog-of-war coverage.
void writeLook007(const FogLookInputs& in, void* out) {
  Look005Frame f{};
  f.mist = classicMist4(in);
  f.swirlCenter = in.scene.partyCenter;
  f.swirlRate = 0.12f;
  f.swirlFalloff = 0.08f;
  f.spiral = 0.15f;
  f.period = 24.0f;
  std::memcpy(out, &f, sizeof(f));
}

}  // namespace

std::vector<uint8_t> makeFogNoise(FogNoise kind) {
  constexpr int n = static_cast<int>(kFogNoiseSize);
  std::vector<uint8_t> out(static_cast<size_t>(n) * n * n);
  if (kind == FogNoise::WhiteLattice) {
    // Ember's baked lattice (volumetricFog.ts buildNoiseVolume): xorshift values, filtered smoothly in the shader.
    uint32_t s = 0x9e3779b9u;
    for (uint8_t& v : out) {
      s ^= s << 13;
      s ^= s >> 17;
      s ^= s << 5;
      v = static_cast<uint8_t>(s & 255u);
    }
    return out;
  }
  const auto hash = [](int x, int y, int z, int seed) {
    uint32_t h = static_cast<uint32_t>(x) * 73856093u ^ static_cast<uint32_t>(y) * 19349663u ^ static_cast<uint32_t>(z) * 83492791u ^
                 static_cast<uint32_t>(seed) * 2654435761u;
    h ^= h >> 13;
    h *= 0x5bd1e995u;
    h ^= h >> 15;
    return (h & 0xFFFFFFu) / static_cast<float>(0xFFFFFF);
  };
  const auto wrap = [](int v, int period) { return ((v % period) + period) % period; };
  // Periodic value noise with `cells` cells across the volume.
  const auto value = [&](float x, float y, float z, int cells, int seed) {
    const float fx = x * cells, fy = y * cells, fz = z * cells;
    const int ix = static_cast<int>(std::floor(fx)), iy = static_cast<int>(std::floor(fy)), iz = static_cast<int>(std::floor(fz));
    const auto s = [](float t) { return t * t * t * (t * (t * 6 - 15) + 10); };
    const float ux = s(fx - ix), uy = s(fy - iy), uz = s(fz - iz);
    float c[2][2][2];
    for (int a = 0; a < 2; ++a)
      for (int b = 0; b < 2; ++b)
        for (int d = 0; d < 2; ++d) c[a][b][d] = hash(wrap(ix + a, cells), wrap(iy + b, cells), wrap(iz + d, cells), seed);
    const auto lerp = [](float a, float b, float t) { return a + (b - a) * t; };
    return lerp(lerp(lerp(c[0][0][0], c[1][0][0], ux), lerp(c[0][1][0], c[1][1][0], ux), uy),
                lerp(lerp(c[0][0][1], c[1][0][1], ux), lerp(c[0][1][1], c[1][1][1], ux), uy), uz);
  };
  // Periodic cellular noise: distance to the nearest of one jittered point per cell.
  const auto cellular = [&](float x, float y, float z, int cells, int seed) {
    const float fx = x * cells, fy = y * cells, fz = z * cells;
    const int ix = static_cast<int>(std::floor(fx)), iy = static_cast<int>(std::floor(fy)), iz = static_cast<int>(std::floor(fz));
    float best = 1e9f;
    for (int a = -1; a <= 1; ++a)
      for (int b = -1; b <= 1; ++b)
        for (int d = -1; d <= 1; ++d) {
          const int cx = ix + a, cy = iy + b, cz = iz + d;
          const int wx = wrap(cx, cells), wy = wrap(cy, cells), wz = wrap(cz, cells);
          const float px = cx + hash(wx, wy, wz, seed), py = cy + hash(wx, wy, wz, seed + 1), pz = cz + hash(wx, wy, wz, seed + 2);
          best = std::min(best, (px - fx) * (px - fx) + (py - fy) * (py - fy) + (pz - fz) * (pz - fz));
        }
    return std::min(1.0f, std::sqrt(best));
  };
  for (int z = 0; z < n; ++z)
    for (int y = 0; y < n; ++y)
      for (int x = 0; x < n; ++x) {
        const float u = (x + 0.5f) / n, v = (y + 0.5f) / n, w = (z + 0.5f) / n;
        float k;
        if (kind == FogNoise::CellularBillows) {
          // Look 001's noise: inverted cellular billows mixed with value-noise fBm.
          const float fbm = value(u, v, w, 4, 1) * 0.5f + value(u, v, w, 8, 2) * 0.3f + value(u, v, w, 16, 3) * 0.2f;
          const float billows = (1.0f - cellular(u, v, w, 5, 10)) * 0.65f + (1.0f - cellular(u, v, w, 11, 20)) * 0.35f;
          k = std::clamp(billows * 0.6f + fbm * 0.4f, 0.0f, 1.0f);
        } else {
          // Looks 002-003: soft value-noise fBm spread over the range.
          const float fbm = value(u, v, w, 4, 1) * 0.47f + value(u, v, w, 8, 2) * 0.27f + value(u, v, w, 16, 3) * 0.17f +
                            value(u, v, w, 32, 4) * 0.09f;
          k = std::clamp((fbm - 0.5f) * 1.6f + 0.5f, 0.0f, 1.0f);
        }
        out[(static_cast<size_t>(z) * n + y) * n + x] = static_cast<uint8_t>(k * 255.0f + 0.5f);
      }
  return out;
}

std::span<const FogLook> fogLooks() {
  static const FogLook looks[] = {
      {"001", "fog_look001", "thick puffy billows, self-shadowed (user: too puffy, cartoony)", FogNoise::CellularBillows, sizeof(Look001Frame), writeLook001},
      {"002", "fog_look002", "soft even grey sheet (user: looks fake)", FogNoise::SoftFbm, sizeof(Look003Frame), writeLook002},
      {"003", "fog_look003", "soft banks and gaps (user: kept)", FogNoise::SoftFbm, sizeof(Look003Frame), writeLook003},
      {"004", "fog_look004", "Ember's mist model (fog1) - not yet seen", FogNoise::WhiteLattice, sizeof(Look004Frame), writeLook004},
      {"005", "fog_look005", "swirl round the party (Claude's take) - not yet seen", FogNoise::WhiteLattice, sizeof(Look005Frame), writeLook005},
      {"006", "fog_look006", "Ember's mist4 as in the user's reference (vortex, clear centre) - not yet seen", FogNoise::WhiteLattice, sizeof(Look006Frame), writeLook006},
      {"007", "fog_look007", "swirl round the party on mist4's real values, fog-of-war coverage - not yet seen", FogNoise::WhiteLattice, sizeof(Look005Frame), writeLook007},
      {"008", "fog_look008", "003 thinning away near the camera (no veil when the camera comes close)", FogNoise::SoftFbm, sizeof(Look008Frame), writeLook008},
      {"009", "fog_look009", "008 with the background left clear: no mist over the sky or the painting (the user: the sky on every outdoor map)", FogNoise::SoftFbm, sizeof(Look008Frame), writeLook008},
      {"010", "fog_look010", "the gorge's wind fog outside the bridge walls (A Ponte de Pedra 023) - not yet seen", FogNoise::SoftFbm, sizeof(Look010Frame), writeLook010, false},
  };
  return looks;
}

}  // namespace ember
