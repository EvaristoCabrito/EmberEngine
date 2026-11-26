#include "render/vfx/LightingSpellFx.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

#include <glm/gtc/matrix_transform.hpp>

#include "render/VfxGeometry.h"
#include "render/vfx/ThreeMath.h"
#include "world/LightingSpells.h"

namespace ember {

namespace {

constexpr float kPiF = std::numbers::pi_v<float>;
constexpr float kTau = 2.0f * kPiF;
constexpr glm::vec3 kUp{0.0f, 1.0f, 0.0f};

constexpr float kModeHolyShaft = 0.0f;
constexpr float kModeCombatSphere = 2.0f;
constexpr float kModeCombatTrail = 4.0f;

float easeSmooth(float edge0, float edge1, float x) {
  const float t = std::clamp((x - edge0) / std::max(edge1 - edge0, 1e-5f), 0.0f, 1.0f);
  return t * t * (3.0f - 2.0f * t);
}

float easeOutCubic(float edge0, float edge1, float x) {
  const float t = std::clamp((x - edge0) / std::max(edge1 - edge0, 1e-5f), 0.0f, 1.0f);
  const float u = 1.0f - t;
  return 1.0f - u * u * u;
}

float hash01(uint32_t seed, uint32_t i, uint32_t salt) {
  uint32_t h = seed ^ (i * 0x9e3779b9u) ^ (salt * 0x85ebca6bu);
  h ^= h >> 16;
  h *= 0x7feb352du;
  h ^= h >> 15;
  h *= 0x846ca68bu;
  h ^= h >> 16;
  return static_cast<float>(h & 0xFFFFFFu) / static_cast<float>(0x1000000u);
}

glm::vec3 safeFlatNormalize(glm::vec3 v, glm::vec3 fallback) {
  v.y = 0.0f;
  const float len2 = glm::dot(v, v);
  return len2 > 1e-6f ? (v / std::sqrt(len2)) : fallback;
}

const std::vector<VfxVertex>& sphereGeometry() {
  static const std::vector<VfxVertex> g = vfxgeo::sphere(1.0f, 20, 14);
  return g;
}

const std::vector<VfxVertex>& crystalGeometry() {
  static const std::vector<VfxVertex> g = vfxgeo::octahedron(1.0f, 1);
  return g;
}

const std::vector<VfxVertex>& moteGeometry() {
  static const std::vector<VfxVertex> g = vfxgeo::octahedron(1.0f, 0);
  return g;
}

const std::vector<VfxVertex>& haloHorizontalGeometry() {
  static const std::vector<VfxVertex> g = [] {
    std::vector<VfxVertex> ring = vfxgeo::torus(1.0f, 0.055f, 10, 48, kTau);
    const glm::mat4 flat = glm::rotate(glm::mat4(1.0f), -kPiF * 0.5f, glm::vec3(1.0f, 0.0f, 0.0f));
    for (VfxVertex& v : ring) {
      v.position = glm::vec3(flat * glm::vec4(v.position, 1.0f));
      v.normal = glm::vec3(flat * glm::vec4(v.normal, 0.0f));
    }
    return ring;
  }();
  return g;
}

const std::vector<VfxVertex>& ringVerticalGeometry() {
  static const std::vector<VfxVertex> g = vfxgeo::torus(1.0f, 0.045f, 10, 44, kTau);
  return g;
}

const std::vector<VfxVertex>& columnGeometry() {
  static const std::vector<VfxVertex> g = [] {
    constexpr int kAround = 32, kUpSegs = 12;
    auto vert = [](int i, int j) {
      const float a = static_cast<float>(i) / kAround * kTau;
      const float y = static_cast<float>(j) / kUpSegs;
      return VfxVertex{{std::cos(a), y, std::sin(a)}, static_cast<float>(i) / kAround, {std::cos(a), 0.0f, std::sin(a)}, y};
    };
    std::vector<VfxVertex> out;
    out.reserve(kAround * kUpSegs * 6);
    for (int i = 0; i < kAround; ++i) {
      for (int j = 0; j < kUpSegs; ++j) {
        out.insert(out.end(), {vert(i, j), vert(i + 1, j), vert(i, j + 1), vert(i + 1, j), vert(i + 1, j + 1), vert(i, j + 1)});
      }
    }
    return out;
  }();
  return g;
}

// Genuine 3D open elliptical conical frustum along +Z with analytical 3D outward surface normals.
// Shaded with VfxMaterial::Holy mode 0.0f so Fresnel viewing angle cleanly feathers silhouette edges.
std::vector<VfxVertex> buildBeamConeVolumeGeometry(float length, float halfAngleRad) {
  constexpr int kAround = 32, kAlong = 12;
  const float z0 = 0.25f;
  const float z1 = std::max(length, 1.5f);
  const float tanHalf = std::tan(std::clamp(halfAngleRad, glm::radians(10.0f), glm::radians(62.0f)));
  const float drx = tanHalf * 0.60f;
  const float ryRate = 0.10f;
  const float ryMax = 0.85f;

  auto vert = [&](int i, int j) {
    const float u = static_cast<float>(i) / kAround;
    const float v = static_cast<float>(j) / kAlong;
    const float a = u * kTau;
    const float z = glm::mix(z0, z1, v);
    const float rx = 0.14f + z * drx;
    const float rawRy = 0.14f + z * ryRate;
    const float ry = std::min(rawRy, ryMax);
    const float dry = (rawRy < ryMax) ? ryRate : 0.0f;

    const float ca = std::cos(a), sa = std::sin(a);
    const glm::vec3 pos{ca * rx, sa * ry, z};
    const glm::vec3 ta{-rx * sa, ry * ca, 0.0f};
    const glm::vec3 tz{drx * ca, dry * sa, 1.0f};
    const glm::vec3 c = glm::cross(ta, tz);
    const glm::vec3 normal = glm::dot(c, c) > 1e-8f ? glm::normalize(c) : glm::vec3(ca, sa, 0.0f);
    return VfxVertex{pos, u, normal, v};
  };

  std::vector<VfxVertex> tris;
  tris.reserve(kAround * kAlong * 6);
  for (int i = 0; i < kAround; ++i) {
    for (int j = 0; j < kAlong; ++j) {
      tris.insert(tris.end(), {vert(i, j), vert(i + 1, j), vert(i, j + 1), vert(i + 1, j), vert(i + 1, j + 1), vert(i, j + 1)});
    }
  }
  return tris;
}

struct Mote3D {
  glm::vec3 pos{0.0f};
  float radius = 0.03f;
  float rotY = 0.0f;
  float rotX = 0.0f;
};

void push3dMotes(VfxFrame& frame, uint32_t& moteFirst, uint32_t& moteCount, bool& addedMote,
                 std::vector<Mote3D>& batch, float time, glm::vec3 tint, float intensity, float opacity, glm::vec3 sightAt) {
  if (batch.empty() || opacity <= 0.002f) {
    batch.clear();
    return;
  }
  if (!addedMote) {
    moteFirst = frame.addVertices(moteGeometry());
    moteCount = static_cast<uint32_t>(moteGeometry().size());
    addedMote = true;
  }
  const uint32_t firstInst = static_cast<uint32_t>(frame.instances.size());
  uint32_t emitted = 0;
  for (const Mote3D& m : batch) {
    if (m.radius < 0.004f) continue;
    VfxInstance inst{};
    glm::mat4 mat = glm::translate(glm::mat4(1.0f), m.pos);
    mat = glm::rotate(mat, m.rotY, kUp);
    mat = glm::rotate(mat, m.rotX, glm::vec3(1.0f, 0.0f, 0.0f));
    mat = glm::scale(mat, glm::vec3(m.radius));
    inst.matrix = mat;
    frame.instances.push_back(inst);
    ++emitted;
  }
  batch.clear();
  if (emitted == 0) return;

  VfxDraw d;
  d.gpu.model = glm::mat4(1.0f);
  d.gpu.params0 = {time, kModeCombatSphere, intensity, opacity};
  d.gpu.params1 = glm::vec4(1.0f, 0.99f, 0.96f, 0.85f);
  d.gpu.color = glm::vec4(tint, 0.0f);
  d.gpu.firstVertex = moteFirst;
  d.gpu.vertexCount = moteCount;
  d.gpu.firstInstance = firstInst;
  d.gpu.material = static_cast<uint32_t>(VfxMaterial::Combat);
  d.gpu.sightAt = sightAt;
  d.instances = emitted;
  d.blend = VfxBlend::Additive;
  d.depthTest = true;
  frame.draws.push_back(d);
}

void pushCombatSurface(VfxFrame& frame, uint32_t firstVert, uint32_t vertCount, const glm::mat4& model,
                       float time, float intensity, float opacity, glm::vec3 tint, float coreWeight,
                       float boilAmp, glm::vec3 sightAt, std::optional<glm::vec3> cardAt = std::nullopt) {
  if (opacity <= 0.002f) return;
  VfxDraw d;
  d.gpu.model = model;
  d.gpu.params0 = {time, kModeCombatSphere, intensity, opacity};
  d.gpu.params1 = glm::vec4(1.0f, 0.99f, 0.97f, coreWeight);
  d.gpu.color = glm::vec4(tint, boilAmp);
  d.gpu.firstVertex = firstVert;
  d.gpu.vertexCount = vertCount;
  d.gpu.firstInstance = 0xFFFFFFFFu;
  d.gpu.material = static_cast<uint32_t>(VfxMaterial::Combat);
  d.gpu.sightAt = sightAt;
  d.blend = VfxBlend::Additive;
  d.depthTest = true;
  if (cardAt) d.cardAt = *cardAt;
  frame.draws.push_back(d);
}

void pushTubeCurve(VfxFrame& frame, const std::vector<glm::vec3>& ptsIn, int tubularSegs, float tubeRadius, int radialSegs,
                   float time, glm::vec3 color, glm::vec3 core, float intensity, float opacity, glm::vec3 sightAt) {
  if (opacity <= 0.002f || ptsIn.size() < 3) return;
  std::vector<glm::vec3> pts;
  pts.reserve(ptsIn.size());
  pts.push_back(ptsIn.front());
  float totalLen = 0.0f;
  for (size_t i = 1; i < ptsIn.size(); ++i) {
    const float d = glm::length(ptsIn[i] - pts.back());
    if (d > 0.03f) {
      totalLen += d;
      pts.push_back(ptsIn[i]);
    }
  }
  if (pts.size() < 3 || totalLen < 0.08f) return;

  const std::vector<VfxVertex> tubeVerts = vfxgeo::tube(three::CatmullRom{pts}, tubularSegs, tubeRadius, radialSegs);
  if (tubeVerts.empty()) return;

  VfxDraw d;
  d.gpu.model = glm::mat4(1.0f);
  d.gpu.params0 = {time, kModeCombatTrail, intensity, opacity};
  d.gpu.params1 = glm::vec4(core, 0.0f);
  d.gpu.color = glm::vec4(color, 0.0f);
  d.gpu.firstVertex = frame.addVertices(tubeVerts);
  d.gpu.vertexCount = static_cast<uint32_t>(tubeVerts.size());
  d.gpu.firstInstance = 0xFFFFFFFFu;
  d.gpu.material = static_cast<uint32_t>(VfxMaterial::Combat);
  d.gpu.sightAt = sightAt;
  d.blend = VfxBlend::Additive;
  d.depthTest = true;
  frame.draws.push_back(d);
}

}  // namespace

void LightingSpellCastFx::cast(const std::string& kind, int look, glm::vec3 casterFeet, float casterHeight,
                               glm::vec3 targetFeet, float start, float impact) {
  Cast c;
  c.kind = kind;
  c.look = std::clamp(look <= 0 ? 2 : look, 1, 2);
  c.casterFeet = casterFeet;
  c.casterHeight = std::clamp(casterHeight > 0.0f ? casterHeight : 2.0f, 1.2f, 4.0f);
  c.facing = safeFlatNormalize(targetFeet - casterFeet, glm::vec3(0.0f, 0.0f, 1.0f));
  c.origin = (kind == kSpellArcaneBeam)    ? casterFeet + c.facing * 0.35f + kUp * 1.25f
           : (kind == kSpellAnchoredLight) ? targetFeet + kUp * 0.85f
                                           : casterFeet;
  c.targetPos = (kind == kSpellAnchoredLight) ? (targetFeet + kUp * 0.85f) : targetFeet;
  c.start = start;
  c.impact = std::max(impact, start + 0.08f);
  c.seed = 0x71ab3u + 7919u * ++seedSerial_;

  // Match the newly cast LightSpellInstance so cast animation geometry aligns with persistent power, range & direction.
  const auto& instances = LightingSpellRegistry::get().instances();
  const glm::vec3 refPos = (kind == kSpellAnchoredLight) ? targetFeet : casterFeet;
  for (auto it = instances.rbegin(); it != instances.rend(); ++it) {
    if (it->spellKind != kind) continue;
    const glm::vec2 dxz{it->position.x - refPos.x, it->position.z - refPos.z};
    if (glm::dot(dxz, dxz) <= 3.5f * 3.5f) {
      c.power = it->power;
      c.origin = it->position;
      if (kind == kSpellArcaneBeam) {
        c.facing = safeFlatNormalize(it->facing, c.facing);
      } else if (kind == kSpellAnchoredLight) {
        c.targetPos = it->position;
      }
      c.range = std::max(it->range, 2.0f);
      c.coneAngleDeg = it->coneAngleDeg;
      break;
    }
  }

  c.end = c.impact + (kind == kSpellArcaneBeam ? 0.75f : 0.85f);
  casts_.push_back(std::move(c));
}

bool LightingSpellCastFx::active(float now) const {
  return std::any_of(casts_.begin(), casts_.end(), [&](const Cast& c) { return c.end > now; });
}

float LightingSpellCastFx::latestEnd() const {
  float e = -1.0f;
  for (const Cast& c : casts_) e = std::max(e, c.end);
  return e;
}

void LightingSpellCastFx::build(float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) {
  (void)camera;
  std::erase_if(casts_, [&](const Cast& c) { return now >= c.end; });
  if (casts_.empty()) return;

  uint32_t sphereFirst = 0, sphereCount = 0;
  bool addedSphere = false;
  auto ensureSphere = [&]() {
    if (addedSphere) return;
    sphereFirst = frame.addVertices(sphereGeometry());
    sphereCount = static_cast<uint32_t>(sphereGeometry().size());
    addedSphere = true;
  };

  uint32_t crystalFirst = 0, crystalCount = 0;
  bool addedCrystal = false;
  auto ensureCrystal = [&]() {
    if (addedCrystal) return;
    crystalFirst = frame.addVertices(crystalGeometry());
    crystalCount = static_cast<uint32_t>(crystalGeometry().size());
    addedCrystal = true;
  };

  uint32_t haloHFirst = 0, haloHCount = 0;
  bool addedHaloH = false;
  auto ensureHaloH = [&]() {
    if (addedHaloH) return;
    haloHFirst = frame.addVertices(haloHorizontalGeometry());
    haloHCount = static_cast<uint32_t>(haloHorizontalGeometry().size());
    addedHaloH = true;
  };

  uint32_t ringVFirst = 0, ringVCount = 0;
  bool addedRingV = false;
  auto ensureRingV = [&]() {
    if (addedRingV) return;
    ringVFirst = frame.addVertices(ringVerticalGeometry());
    ringVCount = static_cast<uint32_t>(ringVerticalGeometry().size());
    addedRingV = true;
  };

  uint32_t colFirst = 0, colCount = 0;
  bool addedCol = false;
  auto ensureColumn = [&]() {
    if (addedCol) return;
    colFirst = frame.addVertices(columnGeometry());
    colCount = static_cast<uint32_t>(columnGeometry().size());
    addedCol = true;
  };

  uint32_t moteFirst = 0, moteCount = 0;
  bool addedMote = false;
  std::vector<Mote3D> motes;

  for (const Cast& c : casts_) {
    if (now < c.start || now >= c.end) continue;
    const float tPower = std::clamp(static_cast<float>(c.power - 1) / 29.0f, 0.0f, 1.0f);
    const float age = now - c.start;

    if (c.kind == kSpellArcaneBeam) {
      ensureSphere();
      ensureCrystal();
      ensureRingV();
      const glm::vec3 fwd = safeFlatNormalize(c.facing, glm::vec3(0.0f, 0.0f, 1.0f));
      const glm::vec3 right = glm::normalize(glm::cross(kUp, fwd));
      const float yaw = std::atan2(fwd.x, fwd.z);
      const float halfRad = glm::radians(c.coneAngleDeg * 0.5f);
      const glm::vec3 col = kColorArcaneBeam;

      if (now < c.impact) {
        const float gatherDur = std::max(c.impact - c.start, 0.05f);
        const float q = std::clamp(age / gatherDur, 0.0f, 1.0f);
        const float g = easeOutCubic(0.0f, 1.0f, q);

        // Condensing 3D crystal prism at hand focal point
        const float coreR = 0.04f + 0.08f * g;
        glm::mat4 mCore = glm::translate(glm::mat4(1.0f), c.origin);
        mCore = glm::rotate(mCore, age * 4.2f, kUp);
        mCore = glm::rotate(mCore, age * 2.5f, right);
        mCore = glm::scale(mCore, glm::vec3(coreR));
        pushCombatSurface(frame, crystalFirst, crystalCount, mCore, age, 1.6f + 0.4f * g, 0.85f * g, col, 0.85f, 0.0f, c.casterFeet);

        // Two counter-rotating 3D aperture rings aligned with beam axis
        for (int rIdx = 0; rIdx < 2; ++rIdx) {
          const float rr = glm::mix(0.85f, 0.20f, g) * (rIdx == 0 ? 1.0f : 0.72f);
          const float spin = age * (rIdx == 0 ? 3.5f : -4.8f) + static_cast<float>(rIdx) * 1.1f;
          glm::mat4 mRing = glm::translate(glm::mat4(1.0f), c.origin + fwd * (0.14f + 0.14f * static_cast<float>(rIdx)));
          mRing = glm::rotate(mRing, yaw, kUp);
          mRing = glm::rotate(mRing, spin, glm::vec3(0.0f, 0.0f, 1.0f));
          mRing = glm::scale(mRing, glm::vec3(rr));
          pushCombatSurface(frame, ringVFirst, ringVCount, mRing, age, 1.35f, 0.55f * g, col, 0.78f, 0.0f, c.casterFeet);
        }

        // 12 instanced 3D octahedral shards spiralling inward
        for (int i = 0; i < 12; ++i) {
          const float phase = std::clamp(q * 1.15f - 0.12f * hash01(c.seed, static_cast<uint32_t>(i), 1), 0.0f, 1.0f);
          const float rad = 1.15f * (1.0f - easeSmooth(0.0f, 1.0f, phase));
          const float a = hash01(c.seed, static_cast<uint32_t>(i), 2) * kTau + phase * 5.5f;
          const float back = -0.35f * (1.0f - phase);
          const glm::vec3 p = c.origin + right * (std::cos(a) * rad) + kUp * (std::sin(a) * rad * 0.75f) + fwd * back;
          const float fade = easeSmooth(0.0f, 0.2f, phase) * (1.0f - easeSmooth(0.8f, 1.0f, phase));
          motes.push_back({p, (0.042f + 0.02f * tPower) * fade, age * 2.5f + a, a * 0.7f});
        }
        push3dMotes(frame, moteFirst, moteCount, addedMote, motes, age, col, 1.7f, 0.85f, c.casterFeet);

        lights.push_back({c.origin, col, 4.5f * g, 4.2f});
      } else {
        const float relDur = std::max(c.end - c.impact, 0.1f);
        const float since = now - c.impact;
        const float q = std::clamp(since / relDur, 0.0f, 1.0f);
        const float out = 1.0f - easeSmooth(0.25f, 1.0f, q);

        // Surging 3D volumetric beam cone
        const float surgeLen = std::min(c.range * 0.82f, 24.0f) * easeOutCubic(0.0f, 0.45f, q);
        if (surgeLen > 0.6f) {
          const std::vector<VfxVertex> coneVerts = buildBeamConeVolumeGeometry(surgeLen, halfRad);
          glm::mat4 mCone = glm::translate(glm::mat4(1.0f), c.origin);
          mCone = glm::rotate(mCone, yaw, kUp);
          VfxDraw dCone;
          dCone.gpu.model = mCone;
          dCone.gpu.params0 = {age, kModeHolyShaft, c.look == 1 ? 0.90f : 0.72f, (c.look == 1 ? 0.18f : 0.11f) * out};
          dCone.gpu.params1 = glm::vec4(0.95f, 0.98f, 1.0f, 0.0f);
          dCone.gpu.color = glm::vec4(col, 1.0f);
          dCone.gpu.firstVertex = frame.addVertices(coneVerts);
          dCone.gpu.vertexCount = static_cast<uint32_t>(coneVerts.size());
          dCone.gpu.firstInstance = 0xFFFFFFFFu;
          dCone.gpu.material = static_cast<uint32_t>(VfxMaterial::Holy);
          dCone.gpu.sightAt = c.casterFeet;
          dCone.blend = VfxBlend::Additive;
          dCone.depthTest = true;
          frame.draws.push_back(dCone);
        }

        // Thin 3D Fresnel release sphere at focal aperture
        const float burstFade = 1.0f - easeSmooth(0.0f, 0.48f, q);
        if (burstFade > 0.01f) {
          const float burstR = glm::mix(0.24f, 1.10f, easeOutCubic(0.0f, 0.48f, q));
          pushCombatSurface(frame, sphereFirst, sphereCount, glm::scale(glm::translate(glm::mat4(1.0f), c.origin), glm::vec3(burstR)),
                            age, 1.5f, 0.46f * burstFade, col, 0.0f, 0.03f, c.casterFeet);
        }

        // 3 expanding 3D torus wave rings gliding forward down the beam
        const float tanHalf = std::tan(std::clamp(halfRad, glm::radians(12.0f), glm::radians(58.0f)));
        for (int w = 0; w < 3; ++w) {
          const float wq = std::clamp((q - static_cast<float>(w) * 0.13f) / 0.72f, 0.0f, 1.0f);
          if (wq <= 0.001f || wq >= 1.0f) continue;
          const float distZ = 0.35f + wq * std::min(c.range * 0.52f, 11.0f);
          const float ringR = std::clamp(0.22f + distZ * tanHalf * 0.38f, 0.22f, 3.0f);
          const float wAlpha = (c.look == 1 ? 0.56f : 0.42f) * (1.0f - easeSmooth(0.15f, 1.0f, wq));
          glm::mat4 mWave = glm::translate(glm::mat4(1.0f), c.origin + fwd * distZ);
          mWave = glm::rotate(mWave, yaw, kUp);
          mWave = glm::scale(mWave, glm::vec3(ringR, std::min(ringR * 0.65f, 1.15f), ringR));
          pushCombatSurface(frame, ringVFirst, ringVCount, mWave, age, 1.35f, wAlpha, col, 0.80f, 0.0f, c.casterFeet);
        }

        const float decay = std::pow(1.0f - q, 1.4f);
        lights.push_back({c.origin, col, (c.look == 1 ? 6.5f : 5.5f) * decay, 5.0f});
      }

    } else if (c.kind == kSpellAnchoredLight) {
      ensureSphere();
      ensureCrystal();
      ensureHaloH();
      const glm::vec3 startPos = c.casterFeet + kUp * (c.casterHeight * 0.58f);
      const glm::vec3 anchorPos = c.targetPos;
      const glm::vec3 col = kColorAnchoredLight;

      auto arcPos = [&](float t) {
        const glm::vec3 base = glm::mix(startPos, anchorPos, t);
        const float arc = 0.85f * 4.0f * t * (1.0f - t);
        return base + kUp * arc;
      };

      if (now < c.impact) {
        const float flyDur = std::max(c.impact - c.start, 0.08f);
        const float q = std::clamp(age / flyDur, 0.0f, 1.0f);
        const glm::vec3 pos = arcPos(q);
        const float env = easeSmooth(0.0f, 0.12f, q);

        // Flying 3D pearl orb + soft corona
        const float orbR = (c.look == 1 ? 0.18f : 0.13f) + 0.04f * tPower;
        pushCombatSurface(frame, sphereFirst, sphereCount, glm::scale(glm::translate(glm::mat4(1.0f), pos), glm::vec3(orbR)),
                          age, 1.85f, 0.85f * env, col, 0.84f, 0.03f, pos, pos);
        pushCombatSurface(frame, sphereFirst, sphereCount, glm::scale(glm::translate(glm::mat4(1.0f), pos), glm::vec3(orbR * 1.9f)),
                          age, 1.15f, 0.22f * env, col, 0.18f, 0.03f, pos, pos);

        // Real 3D Catmull-Rom tube trail along the parabolic arc
        if (q > 0.06f) {
          std::vector<glm::vec3> trailPts;
          const float tMin = std::max(0.0f, q - 0.38f);
          constexpr int kSamples = 8;
          for (int s = 0; s < kSamples; ++s) {
            const float f = static_cast<float>(s) / static_cast<float>(kSamples - 1);
            trailPts.push_back(arcPos(glm::mix(tMin, q, f)));
          }
          pushTubeCurve(frame, trailPts, 16, c.look == 1 ? 0.052f : 0.038f, 8, age, col, glm::vec3(1.0f, 0.99f, 1.0f),
                        1.45f, 0.65f * env, pos);
        }

        // 10 instanced 3D octahedral sparks trailing the orb
        for (int i = 0; i < 10; ++i) {
          const float back = (static_cast<float>(i) + 0.5f) / 10.0f * 0.36f;
          const float tSample = q - back;
          if (tSample <= 0.01f) continue;
          const float a = age * 4.5f + static_cast<float>(i) * 1.7f + hash01(c.seed, static_cast<uint32_t>(i), 3) * kTau;
          const float rad = 0.08f + 0.14f * (back / 0.36f);
          const glm::vec3 sp = arcPos(tSample) + glm::vec3(std::cos(a) * rad, std::sin(a * 1.3f) * rad, std::sin(a) * rad);
          const float fade = 1.0f - (back / 0.36f);
          motes.push_back({sp, (0.042f + 0.02f * tPower) * fade * env, a, a * 0.6f});
        }
        push3dMotes(frame, moteFirst, moteCount, addedMote, motes, age, col, 1.75f, 0.85f, pos);

        lights.push_back({pos, col, 5.5f * env, 4.2f});
      } else {
        const float setDur = std::max(c.end - c.impact, 0.1f);
        const float since = now - c.impact;
        const float q = std::clamp(since / setDur, 0.0f, 1.0f);

        // Expanding thin 3D Fresnel energy shell on anchor settle
        const float shellMax = (c.look == 1 ? 1.75f : 1.30f) + 0.40f * tPower;
        const float shellR = glm::mix(0.18f, shellMax, easeOutCubic(0.0f, 0.85f, q));
        const float shellAlpha = 0.52f * std::pow(1.0f - q, 1.6f);
        pushCombatSurface(frame, sphereFirst, sphereCount, glm::scale(glm::translate(glm::mat4(1.0f), anchorPos), glm::vec3(shellR)),
                          age, 1.45f, shellAlpha, col, 0.0f, 0.03f, anchorPos);

        // Tilted 3D torus ring ripple(s)
        const int ringCount = (c.look == 1) ? 2 : 1;
        for (int rIdx = 0; rIdx < ringCount; ++rIdx) {
          const float rq = std::clamp((q - static_cast<float>(rIdx) * 0.12f) / 0.78f, 0.0f, 1.0f);
          if (rq <= 0.001f || rq >= 1.0f) continue;
          const float rRadius = glm::mix(0.16f, c.look == 1 ? 0.62f : 0.45f, easeOutCubic(0.0f, 1.0f, rq));
          glm::mat4 mRing = glm::translate(glm::mat4(1.0f), anchorPos);
          mRing = glm::rotate(mRing, 0.42f * (rIdx == 0 ? 1.0f : -1.0f), glm::vec3(1.0f, 0.0f, 0.4f));
          mRing = glm::rotate(mRing, age * (rIdx == 0 ? 2.2f : -2.6f), kUp);
          mRing = glm::scale(mRing, glm::vec3(rRadius));
          pushCombatSurface(frame, haloHFirst, haloHCount, mRing, age, 1.4f, 0.55f * (1.0f - rq), col, 0.80f, 0.0f, anchorPos);
        }

        // 12 instanced 3D octahedral sparks blooming outward
        for (int i = 0; i < 12; ++i) {
          const float u = hash01(c.seed, static_cast<uint32_t>(i), 11);
          const float v = hash01(c.seed, static_cast<uint32_t>(i), 12);
          const float z = 2.0f * u - 1.0f;
          const float rho = std::sqrt(std::max(0.0f, 1.0f - z * z));
          const float phi = v * kTau;
          const glm::vec3 dir{rho * std::cos(phi), z, rho * std::sin(phi)};
          const float dist = (0.20f + 0.95f * easeOutCubic(0.0f, 1.0f, q)) * (0.75f + 0.5f * hash01(c.seed, static_cast<uint32_t>(i), 13));
          const float fade = 1.0f - easeSmooth(0.25f, 1.0f, q);
          motes.push_back({anchorPos + dir * dist, (0.044f + 0.02f * tPower) * fade, age * 2.0f + phi, phi * 0.5f});
        }
        push3dMotes(frame, moteFirst, moteCount, addedMote, motes, age, col, 1.75f, 0.85f, anchorPos);

        lights.push_back({anchorPos, col, 6.8f * std::pow(1.0f - q, 1.4f), 5.2f});
      }

    } else if (c.kind == kSpellLuminousHalo) {
      ensureSphere();
      ensureColumn();
      const glm::vec3 feet = c.casterFeet;
      const glm::vec3 chest = feet + kUp * (c.casterHeight * 0.55f);
      const glm::vec3 col = kColorLuminousHalo;

      if (now < c.impact) {
        const float riseDur = std::max(c.impact - c.start, 0.05f);
        const float q = std::clamp(age / riseDur, 0.0f, 1.0f);
        const float env = easeSmooth(0.0f, 0.25f, q);

        // Translucent 3D open vertical light column centered on caster
        const float shaftR = 0.62f * (1.15f - 0.35f * q);
        const float shaftH = (c.casterHeight + 1.2f) * easeOutCubic(0.0f, 0.85f, q);
        if (shaftH > 0.1f) {
          glm::mat4 mCol = glm::translate(glm::mat4(1.0f), feet);
          mCol = glm::scale(mCol, glm::vec3(shaftR, shaftH, shaftR));
          VfxDraw dCol;
          dCol.gpu.model = mCol;
          dCol.gpu.params0 = {age, kModeHolyShaft, 0.85f, (c.look == 1 ? 0.16f : 0.11f) * env};
          dCol.gpu.params1 = glm::vec4(1.0f, 0.99f, 0.94f, 0.0f);
          dCol.gpu.color = glm::vec4(col, 1.0f);
          dCol.gpu.firstVertex = colFirst;
          dCol.gpu.vertexCount = colCount;
          dCol.gpu.firstInstance = 0xFFFFFFFFu;
          dCol.gpu.material = static_cast<uint32_t>(VfxMaterial::Holy);
          dCol.gpu.sightAt = feet;
          dCol.blend = VfxBlend::Additive;
          dCol.depthTest = true;
          frame.draws.push_back(dCol);
        }

        // Two rising 3D helical light tubes spiraling upward around caster
        for (int h = 0; h < 2; ++h) {
          const float hOff = static_cast<float>(h) * kPiF;
          std::vector<glm::vec3> pts;
          constexpr int kPts = 12;
          const float maxH = c.casterHeight * (0.25f + 0.85f * q);
          for (int k = 0; k < kPts; ++k) {
            const float f = static_cast<float>(k) / static_cast<float>(kPts - 1);
            const float y = glm::mix(0.15f, std::max(maxH, 0.35f), f);
            const float r = glm::mix(0.76f, 0.36f, f * 0.7f + 0.3f * q);
            const float ang = hOff + f * kTau * 1.15f + age * 2.8f;
            pts.push_back(feet + glm::vec3(std::cos(ang) * r, y, std::sin(ang) * r));
          }
          pushTubeCurve(frame, pts, 20, 0.036f, 8, age, col, glm::vec3(1.0f, 0.99f, 0.95f), 1.35f, 0.52f * env, feet);
        }

        // 14 instanced 3D octahedral warm-gold motes spiralling upward
        for (int i = 0; i < 14; ++i) {
          const float frac = static_cast<float>(i) / 14.0f;
          const float a = age * 2.2f + frac * kTau + hash01(c.seed, static_cast<uint32_t>(i), 21) * 1.5f;
          const float y = (0.15f + c.casterHeight * 1.05f * std::fmod(q * 0.9f + frac, 1.0f));
          const float rad = 0.45f + 0.35f * (1.0f - q * 0.5f);
          motes.push_back({feet + glm::vec3(std::cos(a) * rad, y, std::sin(a) * rad), (0.040f + 0.015f * tPower) * env, a, a * 0.4f});
        }
        push3dMotes(frame, moteFirst, moteCount, addedMote, motes, age, col, 1.65f, 0.85f, feet);

        lights.push_back({chest, col, 5.0f * q, 4.5f});
      } else {
        const float bloomDur = std::max(c.end - c.impact, 0.1f);
        const float since = now - c.impact;
        const float q = std::clamp(since / bloomDur, 0.0f, 1.0f);

        // Expanding thin 3D warm-white Fresnel shell around caster
        const float shellMax = (c.look == 1 ? 2.05f : 1.70f) + 0.35f * tPower;
        const float shellR = glm::mix(0.55f, shellMax, easeOutCubic(0.0f, 0.85f, q));
        const float shellAlpha = 0.46f * std::pow(1.0f - q, 1.6f);
        pushCombatSurface(frame, sphereFirst, sphereCount, glm::scale(glm::translate(glm::mat4(1.0f), chest), glm::vec3(shellR)),
                          age, 1.40f, shellAlpha, col, 0.0f, 0.03f, feet);

        // V1 only: overhead 3D golden torus ring settling cleanly into the persistent halo (V2 has zero halo/holy ring)
        if (c.look == 1) {
          ensureHaloH();
          const float ringR = glm::mix(0.92f, 0.32f + 0.09f * tPower, easeOutCubic(0.0f, 0.75f, q));
          const glm::vec3 haloAt = feet + kUp * (c.casterHeight + 0.15f + 0.25f * (1.0f - q));
          glm::mat4 mHalo = glm::translate(glm::mat4(1.0f), haloAt);
          mHalo = glm::rotate(mHalo, age * 1.4f, kUp);
          mHalo = glm::scale(mHalo, glm::vec3(ringR));
          pushCombatSurface(frame, haloHFirst, haloHCount, mHalo, age, 1.45f, 0.65f * (1.0f - easeSmooth(0.6f, 1.0f, q)),
                            col, 0.82f, 0.0f, feet);
        }

        // 14 instanced 3D octahedral warm-white motes expanding softly around caster
        for (int i = 0; i < 14; ++i) {
          const float frac = static_cast<float>(i) / 14.0f;
          const float a = age * 1.4f + frac * kTau;
          const float rad = (0.55f + 0.65f * easeOutCubic(0.0f, 1.0f, q)) * (0.85f + 0.3f * tPower);
          const float y = (0.35f + (c.casterHeight * 0.75f) * hash01(c.seed, static_cast<uint32_t>(i), 31)) + 0.35f * q;
          const float fade = 1.0f - easeSmooth(0.2f, 1.0f, q);
          motes.push_back({feet + glm::vec3(std::cos(a) * rad, y, std::sin(a) * rad), (0.042f + 0.018f * tPower) * fade, a, a * 0.5f});
        }
        push3dMotes(frame, moteFirst, moteCount, addedMote, motes, age, col, 1.65f, 0.85f, feet);

        lights.push_back({chest, col, 6.5f * std::pow(1.0f - q, 1.4f), 5.0f});
      }
    }
  }
}

void appendLightingSpellVfx(
    VfxFrame& frame,
    float time,
    const Camera& camera,
    const std::string& currentFloor,
    const std::string& currentLogicalMap,
    int64_t currentMinute,
    const std::function<std::optional<glm::vec3>(const std::string& casterId)>& getCasterPos,
    const std::function<std::optional<glm::vec3>(const std::string& casterId)>& getCasterHead) {
  (void)camera;
  const auto& instances = LightingSpellRegistry::get().instances();
  if (instances.empty()) return;

  uint32_t sphereFirst = 0, sphereCount = 0;
  bool addedSphere = false;
  auto ensureSphere = [&]() {
    if (addedSphere) return;
    sphereFirst = frame.addVertices(sphereGeometry());
    sphereCount = static_cast<uint32_t>(sphereGeometry().size());
    addedSphere = true;
  };

  uint32_t crystalFirst = 0, crystalCount = 0;
  bool addedCrystal = false;
  auto ensureCrystal = [&]() {
    if (addedCrystal) return;
    crystalFirst = frame.addVertices(crystalGeometry());
    crystalCount = static_cast<uint32_t>(crystalGeometry().size());
    addedCrystal = true;
  };

  uint32_t haloHFirst = 0, haloHCount = 0;
  bool addedHaloH = false;
  auto ensureHaloH = [&]() {
    if (addedHaloH) return;
    haloHFirst = frame.addVertices(haloHorizontalGeometry());
    haloHCount = static_cast<uint32_t>(haloHorizontalGeometry().size());
    addedHaloH = true;
  };

  uint32_t ringVFirst = 0, ringVCount = 0;
  bool addedRingV = false;
  auto ensureRingV = [&]() {
    if (addedRingV) return;
    ringVFirst = frame.addVertices(ringVerticalGeometry());
    ringVCount = static_cast<uint32_t>(ringVerticalGeometry().size());
    addedRingV = true;
  };

  uint32_t moteFirst = 0, moteCount = 0;
  bool addedMote = false;
  std::vector<Mote3D> motes;

  for (size_t idx = 0; idx < instances.size(); ++idx) {
    const LightSpellInstance& inst = instances[idx];
    if (inst.isExpired(currentMinute)) continue;
    if (!currentLogicalMap.empty() && inst.logicalMapId != currentLogicalMap) continue;
    const float seedOffset = static_cast<float>(idx) * 1.37f + static_cast<float>(inst.power) * 0.53f;
    const float tPower = std::clamp(static_cast<float>(inst.power - 1) / 29.0f, 0.0f, 1.0f);

    if (inst.spellKind == kSpellAnchoredLight) {
      if (inst.floorId != currentFloor) continue;
      ensureSphere();

      if (inst.look == 1) {
        // V1 pure 3D: hovering luminous 3D sphere + soft corona + tilted precessing 3D torus ring + 3D octahedral motes (no 2D floor disc)
        ensureHaloH();
        const float bob = 0.055f * std::sin(time * 2.3f + seedOffset);
        const float pulse = 1.0f + 0.045f * std::sin(time * 3.6f + seedOffset);
        const float sphereRadius = (0.16f + 0.08f * tPower) * pulse;
        const glm::vec3 orbPos = inst.position + glm::vec3(0.0f, bob, 0.0f);

        pushCombatSurface(frame, sphereFirst, sphereCount, glm::scale(glm::translate(glm::mat4(1.0f), orbPos), glm::vec3(sphereRadius)),
                          time, 1.65f, 0.82f, kColorAnchoredLight, 0.80f, 0.03f, inst.position, orbPos);
        pushCombatSurface(frame, sphereFirst, sphereCount, glm::scale(glm::translate(glm::mat4(1.0f), orbPos), glm::vec3(sphereRadius * 1.65f)),
                          time, 1.0f, 0.16f, kColorAnchoredLight, 0.18f, 0.03f, inst.position, orbPos);

        glm::mat4 mRing = glm::translate(glm::mat4(1.0f), orbPos);
        mRing = glm::rotate(mRing, time * 0.45f + seedOffset, kUp);
        mRing = glm::rotate(mRing, glm::radians(24.0f), glm::vec3(1.0f, 0.0f, 0.35f));
        mRing = glm::scale(mRing, glm::vec3(0.30f + 0.12f * tPower));
        pushCombatSurface(frame, haloHFirst, haloHCount, mRing, time, 1.25f, 0.36f, kColorAnchoredLight, 0.78f, 0.0f, inst.position, orbPos);

        const int moteCountN = 8 + static_cast<int>(std::round(6.0f * tPower));
        for (int m = 0; m < moteCountN; ++m) {
          const float a = time * 0.45f + seedOffset + static_cast<float>(m) * (kTau / static_cast<float>(moteCountN));
          const float r = (0.48f + 0.50f * tPower) + 0.18f * std::sin(time * 0.9f + static_cast<float>(m) * 1.7f);
          const float y = 0.18f * std::sin(time * 1.15f + static_cast<float>(m) * 2.1f);
          motes.push_back({orbPos + glm::vec3(std::cos(a) * r, y, std::sin(a) * r), 0.028f + 0.014f * tPower, a * 1.3f, a * 0.6f});
        }
        push3dMotes(frame, moteFirst, moteCount, addedMote, motes, time, kColorAnchoredLight, 1.55f, 0.80f, inst.position);
      } else {
        // V2 pure 3D: real 3D lighting primary + compact 3D faceted pearl crystal core, soft 3D corona & orbiting 3D motes
        ensureCrystal();
        const float breathe = 0.025f * std::sin(time * 2.1f + seedOffset);
        const glm::vec3 orbPos = inst.position + glm::vec3(0.0f, breathe, 0.0f);
        const float coreRadius = 0.075f + 0.045f * tPower;
        const float coronaRadius = 0.15f + 0.09f * tPower;

        glm::mat4 mCore = glm::translate(glm::mat4(1.0f), orbPos);
        mCore = glm::rotate(mCore, time * 0.65f + seedOffset, kUp);
        mCore = glm::rotate(mCore, time * 0.35f, glm::vec3(1.0f, 0.0f, 0.6f));
        mCore = glm::scale(mCore, glm::vec3(coreRadius));
        pushCombatSurface(frame, crystalFirst, crystalCount, mCore, time, 1.60f, 0.80f, kColorAnchoredLight, 0.82f, 0.0f, inst.position, orbPos);

        pushCombatSurface(frame, sphereFirst, sphereCount, glm::scale(glm::translate(glm::mat4(1.0f), orbPos), glm::vec3(coronaRadius)),
                          time, 1.0f, 0.18f, kColorAnchoredLight, 0.20f, 0.03f, inst.position, orbPos);

        const int moteCountN = 8 + static_cast<int>(std::round(6.0f * tPower));
        for (int m = 0; m < moteCountN; ++m) {
          const float a = time * 0.42f + seedOffset + static_cast<float>(m) * (kTau / static_cast<float>(moteCountN));
          const float r = (0.45f + 0.55f * tPower) + 0.22f * std::sin(time * 0.9f + static_cast<float>(m) * 1.7f);
          const float y = 0.18f * std::sin(time * 1.15f + static_cast<float>(m) * 2.1f);
          motes.push_back({orbPos + glm::vec3(std::cos(a) * r, y, std::sin(a) * r), 0.026f + 0.012f * tPower, a * 1.1f, a * 0.7f});
        }
        push3dMotes(frame, moteFirst, moteCount, addedMote, motes, time, kColorAnchoredLight, 1.50f, 0.78f, inst.position);
      }

    } else if (inst.spellKind == kSpellArcaneBeam) {
      if (inst.floorId != currentFloor) continue;

      const glm::vec3 fwd = safeFlatNormalize(inst.facing, glm::vec3(0.0f, 0.0f, 1.0f));
      const glm::vec3 right = glm::normalize(glm::cross(kUp, fwd));
      const float yaw = std::atan2(fwd.x, fwd.z);
      const float halfRad = glm::radians(inst.coneAngleDeg * 0.5f);
      ensureCrystal();

      if (inst.look == 1) {
        // V1 pure 3D: 3D focal crystal + thin 3D aperture ring + true 3D volumetric cone + 3D drifting motes (NO 2D ground sector!)
        ensureRingV();
        const float coreR = 0.085f + 0.035f * tPower;
        glm::mat4 mCore = glm::translate(glm::mat4(1.0f), inst.position);
        mCore = glm::rotate(mCore, time * 0.8f + seedOffset, kUp);
        mCore = glm::rotate(mCore, time * 0.4f, right);
        mCore = glm::scale(mCore, glm::vec3(coreR));
        pushCombatSurface(frame, crystalFirst, crystalCount, mCore, time, 1.55f, 0.72f, kColorArcaneBeam, 0.82f, 0.0f, inst.position);

        glm::mat4 mRing = glm::translate(glm::mat4(1.0f), inst.position + fwd * 0.22f);
        mRing = glm::rotate(mRing, yaw, kUp);
        mRing = glm::rotate(mRing, time * 0.45f, glm::vec3(0.0f, 0.0f, 1.0f));
        mRing = glm::scale(mRing, glm::vec3(0.20f + 0.08f * tPower));
        pushCombatSurface(frame, ringVFirst, ringVCount, mRing, time, 1.25f, 0.40f, kColorArcaneBeam, 0.78f, 0.0f, inst.position);

        const std::vector<VfxVertex> coneVerts = buildBeamConeVolumeGeometry(inst.range * 0.78f, halfRad);
        glm::mat4 mCone = glm::translate(glm::mat4(1.0f), inst.position);
        mCone = glm::rotate(mCone, yaw, kUp);

        VfxDraw dCone;
        dCone.gpu.model = mCone;
        dCone.gpu.params0 = {time, kModeHolyShaft, 0.72f, 0.115f};
        dCone.gpu.params1 = glm::vec4(0.95f, 0.98f, 1.0f, 0.0f);
        dCone.gpu.color = glm::vec4(kColorArcaneBeam, 1.0f);
        dCone.gpu.firstVertex = frame.addVertices(coneVerts);
        dCone.gpu.vertexCount = static_cast<uint32_t>(coneVerts.size());
        dCone.gpu.firstInstance = 0xFFFFFFFFu;
        dCone.gpu.material = static_cast<uint32_t>(VfxMaterial::Holy);
        dCone.gpu.sightAt = inst.position;
        dCone.blend = VfxBlend::Additive;
        dCone.depthTest = true;
        frame.draws.push_back(dCone);

        const int moteCountN = 10 + static_cast<int>(std::round(12.0f * tPower));
        const float maxDist = std::max(inst.range * 0.88f, 3.0f);
        for (int m = 0; m < moteCountN; ++m) {
          const float phase = std::fmod(time * 0.14f + static_cast<float>(m) / static_cast<float>(moteCountN) + seedOffset * 0.1f, 1.0f);
          const float dist = 0.55f + phase * (maxDist - 0.55f);
          const float spread = std::sin(static_cast<float>(m) * 2.3f + seedOffset) * std::tan(std::clamp(halfRad * 0.82f, 0.15f, 1.05f)) * 0.65f;
          const float heightOff = -0.35f + 0.35f * std::sin(time * 0.85f + static_cast<float>(m) * 1.9f);
          const glm::vec3 p = inst.position + fwd * dist + right * (dist * spread) + glm::vec3(0.0f, heightOff, 0.0f);
          const float edgeFade = std::sin(kPiF * phase);
          motes.push_back({p, (0.028f + 0.014f * tPower) * edgeFade, time * 0.9f + static_cast<float>(m), static_cast<float>(m) * 0.7f});
        }
        push3dMotes(frame, moteFirst, moteCount, addedMote, motes, time, kColorArcaneBeam, 1.50f, 0.78f, inst.position);
      } else {
        // V2 pure 3D: Real 3D persistent spotlight drives world lighting + restrained 3D crystal prism, gentle 3D volumetric Tyndall cone & 3D dust motes
        const float coreR = 0.065f + 0.025f * tPower;
        glm::mat4 mCore = glm::translate(glm::mat4(1.0f), inst.position);
        mCore = glm::rotate(mCore, time * 0.7f + seedOffset, kUp);
        mCore = glm::rotate(mCore, time * 0.35f, right);
        mCore = glm::scale(mCore, glm::vec3(coreR));
        pushCombatSurface(frame, crystalFirst, crystalCount, mCore, time, 1.35f, 0.58f, kColorArcaneBeam, 0.80f, 0.0f, inst.position);

        const std::vector<VfxVertex> coneVerts = buildBeamConeVolumeGeometry(inst.range * 0.70f, halfRad);
        glm::mat4 mCone = glm::translate(glm::mat4(1.0f), inst.position);
        mCone = glm::rotate(mCone, yaw, kUp);

        VfxDraw dCone;
        dCone.gpu.model = mCone;
        dCone.gpu.params0 = {time, kModeHolyShaft, 0.55f, 0.065f};
        dCone.gpu.params1 = glm::vec4(0.95f, 0.98f, 1.0f, 0.0f);
        dCone.gpu.color = glm::vec4(kColorArcaneBeam, 1.0f);
        dCone.gpu.firstVertex = frame.addVertices(coneVerts);
        dCone.gpu.vertexCount = static_cast<uint32_t>(coneVerts.size());
        dCone.gpu.firstInstance = 0xFFFFFFFFu;
        dCone.gpu.material = static_cast<uint32_t>(VfxMaterial::Holy);
        dCone.gpu.sightAt = inst.position;
        dCone.blend = VfxBlend::Additive;
        dCone.depthTest = true;
        frame.draws.push_back(dCone);

        const int moteCountN = std::clamp(static_cast<int>(std::round(inst.range * 0.65f)), 10, 24);
        const float maxDist = std::max(inst.range * 0.90f, 3.0f);
        for (int m = 0; m < moteCountN; ++m) {
          const float phase = std::fmod(time * 0.14f + static_cast<float>(m) / static_cast<float>(moteCountN) + seedOffset * 0.1f, 1.0f);
          const float dist = 0.6f + phase * (maxDist - 0.6f);
          const float spread = std::sin(static_cast<float>(m) * 2.3f + seedOffset) * std::tan(std::clamp(halfRad * 0.80f, 0.15f, 1.05f)) * 0.65f;
          const float heightOff = -0.35f + 0.35f * std::sin(time * 0.85f + static_cast<float>(m) * 1.9f);
          const glm::vec3 p = inst.position + fwd * dist + right * (dist * spread) + glm::vec3(0.0f, heightOff, 0.0f);
          const float edgeFade = std::sin(kPiF * phase);
          motes.push_back({p, (0.025f + 0.012f * tPower) * edgeFade, time * 0.8f + static_cast<float>(m), static_cast<float>(m) * 0.5f});
        }
        push3dMotes(frame, moteFirst, moteCount, addedMote, motes, time, kColorArcaneBeam, 1.45f, 0.75f, inst.position);
      }

    } else if (inst.spellKind == kSpellLuminousHalo) {
      const std::optional<glm::vec3> casterPos = getCasterPos ? getCasterPos(inst.casterId) : std::nullopt;
      const glm::vec3 feet = casterPos ? *casterPos : glm::vec3(inst.position.x, inst.groundY, inst.position.z);

      if (inst.look == 1) {
        // V1 pure 3D: overhead golden 3D torus halo + orbiting 3D motes (NO flat 2D ground disc!)
        ensureHaloH();
        const float haloRadius = 0.30f + 0.09f * tPower;
        const float bob = 0.03f * std::sin(time * 2.4f + seedOffset);
        const glm::vec3 haloBase =
            (getCasterHead ? getCasterHead(inst.casterId) : std::nullopt).value_or(feet + glm::vec3(0.0f, 2.55f, 0.0f));
        const glm::vec3 haloPos = haloBase + glm::vec3(0.0f, 0.10f + bob, 0.0f);
        glm::mat4 mHalo = glm::translate(glm::mat4(1.0f), haloPos);
        mHalo = glm::rotate(mHalo, time * 0.35f, kUp);
        mHalo = glm::scale(mHalo, glm::vec3(haloRadius));

        pushCombatSurface(frame, haloHFirst, haloHCount, mHalo, time, 1.55f, 0.78f, kColorLuminousHalo, 0.82f, 0.0f, feet);

        const glm::vec3 chest = feet + glm::vec3(0.0f, 1.25f, 0.0f);
        const int moteCountN = 8 + static_cast<int>(std::round(6.0f * tPower));
        for (int m = 0; m < moteCountN; ++m) {
          const float a = time * 0.40f + seedOffset + static_cast<float>(m) * (kTau / static_cast<float>(moteCountN));
          const float r = 0.52f + 0.50f * tPower;
          const float y = 0.22f * std::sin(time * 1.1f + static_cast<float>(m) * 1.6f);
          motes.push_back({chest + glm::vec3(std::cos(a) * r, y, std::sin(a) * r), 0.026f + 0.012f * tPower, a * 1.2f, a * 0.5f});
        }
        push3dMotes(frame, moteFirst, moteCount, addedMote, motes, time, kColorLuminousHalo, 1.50f, 0.78f, feet);
      } else {
        // V2 pure 3D: Follows caster across floors; real 3D lantern light + gentle warm 3D wisp crystal + orbiting 3D motes (NO holy halo/disc)
        ensureCrystal();
        ensureSphere();
        const glm::vec3 chest = feet + glm::vec3(0.0f, 1.25f, 0.0f);
        const float wispAngle = time * 0.55f + seedOffset;
        const glm::vec3 wispPos = chest + glm::vec3(std::cos(wispAngle) * 0.55f, 0.20f * std::sin(wispAngle * 1.3f), std::sin(wispAngle) * 0.55f);
        const float wispR = 0.045f + 0.02f * tPower;

        glm::mat4 mWisp = glm::translate(glm::mat4(1.0f), wispPos);
        mWisp = glm::rotate(mWisp, time * 0.9f, kUp);
        mWisp = glm::rotate(mWisp, time * 0.5f, glm::vec3(1.0f, 0.0f, 0.5f));
        mWisp = glm::scale(mWisp, glm::vec3(wispR));
        pushCombatSurface(frame, crystalFirst, crystalCount, mWisp, time, 1.35f, 0.60f, kColorLuminousHalo, 0.80f, 0.0f, feet, wispPos);
        pushCombatSurface(frame, sphereFirst, sphereCount, glm::scale(glm::translate(glm::mat4(1.0f), wispPos), glm::vec3(wispR * 2.2f)),
                          time, 0.95f, 0.14f, kColorLuminousHalo, 0.18f, 0.02f, feet, wispPos);

        const int moteCountN = 10 + static_cast<int>(std::round(6.0f * tPower));
        for (int m = 0; m < moteCountN; ++m) {
          const float a = time * 0.38f + seedOffset + static_cast<float>(m) * (kTau / static_cast<float>(moteCountN));
          const float r = 0.55f + 0.55f * tPower;
          const float y = 0.22f * std::sin(time * 1.1f + static_cast<float>(m) * 1.6f);
          motes.push_back({chest + glm::vec3(std::cos(a) * r, y, std::sin(a) * r), 0.024f + 0.012f * tPower, a * 1.1f, a * 0.6f});
        }
        push3dMotes(frame, moteFirst, moteCount, addedMote, motes, time, kColorLuminousHalo, 1.45f, 0.75f, feet);
      }
    }
  }
}

}  // namespace ember
