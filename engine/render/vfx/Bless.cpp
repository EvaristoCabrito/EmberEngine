#include "render/vfx/Bless.h"

#include <algorithm>
#include <cmath>
#include <numbers>

#include <glm/gtc/matrix_transform.hpp>

#include "render/VfxGeometry.h"
#include "render/vfx/IceParts.h"
#include "render/vfx/SigilParts.h"

namespace ember {

namespace {

constexpr float kPi = std::numbers::pi_v<float>;
// DEFAULT_BLESS_VFX_SETTINGS (Legacy)
constexpr float kWaveSpeed = 1, kWaveRadius = 1, kWaveHeight = 0.42f, kWaveTurbulence = 0.22f;
constexpr int kCasterParticles = 32, kAllyParticles = 28, kStrandCount = 8;
constexpr float kStrandThickness = 0.025f, kStrandHeight = 0.85f, kStrandCurvature = 0.42f, kAbsorptionSpeed = 1, kEmissive = 4.5f;
constexpr float kCasterLight = 8, kCasterLightRadius = 5.2f, kAllyLight = 3.5f, kAllyLightRadius = 2.4f, kLightDecay = 1;
constexpr uint32_t kSeed = 31037;
constexpr float kWaveTravel = std::max(0.08f, (Bless::kDuration - Bless::kWaveRelease - 0.65f) / kWaveSpeed);

// Modern Holy settings
constexpr glm::vec3 kUp{0.0f, 1.0f, 0.0f};
constexpr glm::vec3 kHolyCore{1.0f, 0.96f, 0.85f};
constexpr glm::vec3 kHolyGold{1.0f, 0.78f, 0.35f};
constexpr glm::vec3 kHolyWarm{0.95f, 0.60f, 0.20f};
constexpr float kPool = 1.0f, kMote = 2.0f, kLine = 4.0f, kRay = 5.0f, kBloom = 7.0f, kBeam = 8.0f;

float easeOut(float edge0, float edge1, float x) {
  const float t = std::clamp((x - edge0) / std::max(edge1 - edge0, 1e-5f), 0.0f, 1.0f);
  return 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t);
}

VfxInstance dot(glm::vec3 at, float size, float alpha, glm::vec3 tint, float intensity = 1.0f) {
  VfxInstance inst;
  inst.a = glm::vec4(at, size);
  inst.b = {0.0f, 0.0f, alpha, intensity};
  inst.c = glm::vec4(tint, 0.0f);
  return inst;
}

// The legacy wave ring: a thin tube (160 x 8) around the unit circle in the xy plane.
const std::vector<VfxVertex>& ringGeometry() {
  static const std::vector<VfxVertex> g = [] {
    constexpr int segments = 160, sides = 8;
    std::vector<glm::vec3> p;
    for (int i = 0; i <= segments; ++i) {
      const float a = static_cast<float>(i) / segments * kPi * 2.0f;
      for (int j = 0; j <= sides; ++j) {
        const float b = static_cast<float>(j) / sides * kPi * 2.0f;
        const float r = 1.0f + 0.07f * std::cos(b);
        p.push_back({std::cos(a) * r, std::sin(a) * r, 0.07f * std::sin(b)});
      }
    }
    std::vector<VfxVertex> out;
    auto v = [&](int k) { return VfxVertex{p[k], 0, {0, 0, 1}, 0}; };
    for (int i = 0; i < segments; ++i)
      for (int j = 0; j < sides; ++j) {
        const int k = i * (sides + 1) + j;
        out.insert(out.end(), {v(k), v(k + sides + 1), v(k + 1), v(k + 1), v(k + sides + 1), v(k + sides + 2)});
      }
    return out;
  }();
  return g;
}
const std::vector<VfxVertex>& coreGeometry() { static const auto g = vfxgeo::icosahedron(0.18f, 2); return g; }
const std::vector<VfxVertex>& moteGeometry() { static const auto g = vfxgeo::icosahedron(0.045f, 1); return g; }
// rebuildStrands: one tube per strand index, the same around every ally.
const std::vector<VfxVertex>& strandGeometry(int i) {
  static const std::vector<std::vector<VfxVertex>> g = [] {
    std::vector<std::vector<VfxVertex>> all;
    const int count = std::min(10, std::max(1, kStrandCount));
    for (int k = 0; k < count; ++k) {
      const float a = static_cast<float>(k) / kStrandCount * kPi * 2.0f;
      three::CatmullRom curve{{{std::cos(a) * 0.05f, 0.0f, std::sin(a) * 0.05f},
                               {std::cos(a + kStrandCurvature) * 0.2f, -kStrandHeight * 0.35f, std::sin(a + kStrandCurvature) * 0.2f},
                               {std::cos(a + kPi) * 0.17f, -kStrandHeight * 0.72f, std::sin(a + kPi) * 0.17f},
                               {std::cos(a + kPi * 1.5f) * 0.1f, -kStrandHeight, std::sin(a + kPi * 1.5f) * 0.1f}}};
      all.push_back(vfxgeo::tube(curve, 14, kStrandThickness, 5));
    }
    return all;
  }();
  return g[static_cast<size_t>(i)];
}

}  // namespace

float Bless::reachSeconds(float distanceHexes) { return kWaveRelease + std::clamp(distanceHexes / 3.0f, 0.0f, 1.0f) * kWaveTravel; }

Bless::Bless(glm::vec3 center, float radiusWorld, std::vector<Ally> allies, float start, bool modern)
    : center_(center), emberCenter_(vfxToEmber(center)), radius_(radiusWorld), start_(start), modern_(modern) {
  for (const Ally& a : allies) {
    receptions_.push_back({a.position, vfxToEmber(a.position), reachSeconds(a.distanceHexes)});
  }
  three::Lcg rng;
  rng.state = kSeed;
  seeds_.resize(std::max(1, kCasterParticles + kAllyParticles * 8));
  for (float& s : seeds_) s = rng.next();
}

void Bless::build(float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) {
  const float t = now - start_;
  if (t < 0.0f || done(now)) return;
  if (modern_) {
    buildModern(now, camera, frame, lights);
  } else {
    buildLegacy(now, camera, frame, lights);
  }
}

// --- Bênção 02 (Brand new modern holy remake) ---------------------------------------------------------------------------------

void Bless::buildModern(float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) {
  const float t = now - start_;
  if (t < 0.0f || done(now)) return;

  auto draw = [&](VfxMaterial material, uint32_t first, size_t count, const glm::mat4& model, glm::vec4 p0, glm::vec4 p1, glm::vec4 color,
                  glm::vec3 sight, VfxBlend blend, uint32_t firstInstance = 0xFFFFFFFFu, uint32_t instances = 1) -> VfxDraw& {
    VfxDraw d;
    d.gpu.model = model;
    d.gpu.params0 = p0;
    d.gpu.params1 = p1;
    d.gpu.color = color;
    d.gpu.firstVertex = first;
    d.gpu.vertexCount = static_cast<uint32_t>(count);
    d.gpu.firstInstance = firstInstance;
    d.gpu.material = static_cast<uint32_t>(material);
    d.gpu.sightAt = sight;
    d.instances = instances;
    d.blend = blend;
    d.depthTest = true;
    frame.draws.push_back(d);
    return frame.draws.back();
  };

  auto strip = [&](float mode, glm::vec3 from, glm::vec3 to, float w0, float w1, glm::vec3 colour, glm::vec3 core, float intensity, float opacity, float seed) -> VfxDraw* {
    const glm::vec3 along = to - from;
    glm::vec3 across = glm::cross(along, camera.position - (from + to) * 0.5f);
    if (glm::dot(across, across) < 1e-8f || opacity <= 0.003f) return nullptr;
    across = glm::normalize(across);
    const glm::vec3 n = glm::normalize(glm::cross(along, across));
    const VfxVertex a{from - across * (w0 * 0.5f), 0, n, 0}, b{from + across * (w0 * 0.5f), 1, n, 0},
                    c{to + across * (w1 * 0.5f), 1, n, 1}, d{to - across * (w1 * 0.5f), 0, n, 1};
    const uint32_t first = frame.addVertices({a, b, c, a, c, d});
    VfxDraw& vd = draw(VfxMaterial::Holy, first, 6, glm::mat4(1.0f), {t, mode, intensity, opacity}, {core, seed}, glm::vec4(colour, 1.0f), from, VfxBlend::Additive);
    return &vd;
  };

  auto circle = [&](glm::vec3 at, float rad, float cover, glm::vec3 colour, glm::vec3 core, float intensity, float opacity) {
    if (opacity <= 0.003f || cover <= 0.003f) return;
    const float r = rad * (0.35f + 0.65f * std::min(cover, 1.0f));
    const glm::mat4 base = glm::translate(glm::mat4(1.0f), at + glm::vec3(0.0f, 0.04f, 0.0f));
    const size_t fromIdx = frame.draws.size();
    draw(VfxMaterial::Holy, frame.addVertices(sigil::ringGeometry()), sigil::ringGeometry().size(),
         base * glm::rotate(glm::mat4(1.0f), t * 0.25f, kUp) * glm::scale(glm::mat4(1.0f), glm::vec3(r)),
         {t, kLine, 2.5f * intensity, opacity}, {core, 0.0f}, glm::vec4(colour, 1.0f), at, VfxBlend::Additive);
    draw(VfxMaterial::Holy, frame.addVertices(sigil::starGeometry()), sigil::starGeometry().size(),
         base * glm::rotate(glm::mat4(1.0f), -t * 0.45f, kUp) * glm::scale(glm::mat4(1.0f), glm::vec3(r)),
         {t, kLine, 2.8f * intensity, opacity}, {core, 0.0f}, glm::vec4(glm::mix(colour, core, 0.5f), 1.0f), at, VfxBlend::Additive);
    draw(VfxMaterial::Holy, frame.addVertices(icefx::discGeometry()), icefx::discGeometry().size(),
         base * glm::scale(glm::mat4(1.0f), {r, 1.0f, r}),
         {t, kPool, 0.5f * intensity, 0.8f * opacity}, {core, 0.85f}, glm::vec4(colour, 1.0f), at, VfxBlend::Additive);
    for (size_t k = fromIdx; k < frame.draws.size(); ++k) frame.draws[k].underCards = true;
  };

  // --- Caster Stage: radiant holy presence, pillar of divine light (behind card) ---
  const float casterFade = 1.0f - icefx::ease(kDuration - 0.35f, kDuration, t);
  const float casterBuildup = easeOut(0.0f, 0.3f, t);
  if (casterFade > 0.003f) {
    // Ground seal under caster
    circle(center_, 1.5f, casterBuildup, kHolyGold, kHolyCore, 1.2f, casterFade);

    // Celestial God-Ray descending upon the caster (behind card)
    if (VfxDraw* d = strip(kRay, center_, center_ + kUp * 6.5f, 1.6f, 3.2f, kHolyGold, kHolyCore, 1.4f, casterFade * casterBuildup, 0.3f)) {
      d->behindCard = true;
      d->cardAt = center_;
    }
    // Focused holy beam core (behind card)
    if (VfxDraw* d = strip(kBeam, center_, center_ + kUp * 7.0f, 0.7f, 1.5f, kHolyWarm, kHolyCore, 1.6f, casterFade * casterBuildup * 0.9f, 0.7f)) {
      d->behindCard = true;
      d->cardAt = center_;
    }

    // Chest bloom
    const glm::vec3 chest = center_ + kUp * 0.9f;
    const VfxInstance bloomInst = dot(chest, 1.2f * casterBuildup, casterFade * 0.9f, kHolyGold, 2.0f);
    const uint32_t firstBloom = static_cast<uint32_t>(frame.instances.size());
    frame.instances.push_back(bloomInst);
    VfxDraw& bDraw = draw(VfxMaterial::Holy, frame.addVertices(icefx::quadGeometry()), icefx::quadGeometry().size(),
                          glm::mat4(1.0f), {t, kBloom, 1.5f, casterFade}, {kHolyCore, 0.0f}, glm::vec4(1.0f), center_,
                          VfxBlend::Additive, firstBloom, 1);
    bDraw.behindCard = true;
    bDraw.cardAt = center_;

    // Rising motes at caster
    std::vector<VfxInstance> casterMotes;
    for (int i = 0; i < 28; ++i) {
      const float seed = seeds_[static_cast<size_t>(i % seeds_.size())];
      const float born = std::fmod(seed * 3.7f, 1.2f);
      const float age = std::fmod(t + born, 1.2f);
      const float u = age / 1.2f;
      const float ang = static_cast<float>(i) * 2.39996f + seed * 2.0f + t * 0.8f;
      const float dist = (0.25f + 0.65f * (1.0f - u)) * (0.6f + 0.4f * casterBuildup);
      const glm::vec3 pos = center_ + glm::vec3(std::cos(ang) * dist, 0.1f + u * 2.2f, std::sin(ang) * dist);
      const float alpha = std::sin(kPi * u) * casterFade * 0.9f;
      if (alpha > 0.01f) {
        VfxInstance inst;
        inst.a = glm::vec4(pos, 0.12f * (1.0f - 0.4f * u));
        inst.b = {ang, 0.0f, alpha, 2.5f};
        inst.c = glm::vec4(kHolyGold, 0.0f);
        casterMotes.push_back(inst);
      }
    }
    if (!casterMotes.empty()) {
      const uint32_t firstMote = static_cast<uint32_t>(frame.instances.size());
      frame.instances.insert(frame.instances.end(), casterMotes.begin(), casterMotes.end());
      VfxDraw& mDraw = draw(VfxMaterial::Holy, frame.addVertices(icefx::quadGeometry()), icefx::quadGeometry().size(),
                            glm::mat4(1.0f), {t, kMote, 1.2f, 1.0f}, {kHolyCore, 0.0f}, glm::vec4(1.0f), center_,
                            VfxBlend::Additive, firstMote, static_cast<uint32_t>(casterMotes.size()));
      mDraw.behindCard = true;
      mDraw.cardAt = center_;
    }

    lights.push_back({center_ + kUp * 0.8f, kHolyGold, 12.0f * casterBuildup * casterFade, 5.0f});
  }

  // --- Expanding Holy Wave Ring (t >= kWaveRelease) ---
  if (t >= kWaveRelease) {
    const float waveAge = t - kWaveRelease;
    constexpr float kWaveDuration = 0.65f;
    if (waveAge < kWaveDuration + 0.35f) {
      const float arrive = easeOut(0.0f, kWaveDuration, waveAge);
      const float waveRadius = radius_ * arrive;
      const float waveFade = 1.0f - icefx::ease(kWaveDuration * 0.5f, kWaveDuration + 0.35f, waveAge);
      if (waveFade > 0.003f && waveRadius > 0.05f) {
        const glm::mat4 base = glm::translate(glm::mat4(1.0f), center_ + glm::vec3(0.0f, 0.045f, 0.0f));
        const size_t fromIdx = frame.draws.size();
        draw(VfxMaterial::Holy, frame.addVertices(icefx::discGeometry()), icefx::discGeometry().size(),
             base * glm::scale(glm::mat4(1.0f), {waveRadius, 1.0f, waveRadius}),
             {t, kPool, 0.8f, 0.75f * waveFade}, {kHolyCore, 0.95f}, glm::vec4(kHolyGold, 1.0f), center_, VfxBlend::Additive);
        draw(VfxMaterial::Holy, frame.addVertices(sigil::ringGeometry()), sigil::ringGeometry().size(),
             base * glm::rotate(glm::mat4(1.0f), t * 1.5f, kUp) * glm::scale(glm::mat4(1.0f), glm::vec3(waveRadius)),
             {t, kLine, 2.2f, waveFade * 0.8f}, {kHolyCore, 0.0f}, glm::vec4(kHolyCore, 1.0f), center_, VfxBlend::Additive);
        for (size_t k = fromIdx; k < frame.draws.size(); ++k) frame.draws[k].underCards = true;
      }
    }
  }

  // --- Ally Reception: each receives a celestial pillar and seal (behind cards) ---
  for (size_t r = 0; r < receptions_.size(); ++r) {
    const Reception& rec = receptions_[r];
    if (t < rec.reachAt) continue;
    const float local = t - rec.reachAt;
    constexpr float kAllyDuration = 1.0f;
    if (local >= kAllyDuration) continue;
    const float allyBuildup = easeOut(0.0f, 0.2f, local);
    const float allyFade = 1.0f - icefx::ease(kAllyDuration - 0.35f, kAllyDuration, local);
    const glm::vec3 pos = rec.worldPosition;

    // 1. Ally holy ground seal (underCards = true)
    circle(pos, 1.1f, allyBuildup, kHolyGold, kHolyCore, 1.0f, allyFade * 0.85f);

    // 2. Divine descending god-ray / beam (behindCard = true so it backlights the ally!)
    if (VfxDraw* d = strip(kBeam, pos, pos + kUp * 6.0f, 0.7f, 1.6f, kHolyGold, kHolyCore, 1.5f, allyFade * allyBuildup, static_cast<float>(r) * 1.3f)) {
      d->behindCard = true;
      d->cardAt = pos;
    }
    if (VfxDraw* d = strip(kRay, pos, pos + kUp * 5.5f, 1.2f, 2.4f, kHolyWarm, kHolyCore, 1.2f, allyFade * allyBuildup * 0.7f, static_cast<float>(r) * 2.7f)) {
      d->behindCard = true;
      d->cardAt = pos;
    }

    // 3. Rotating celestial halo above head (behindCard = true)
    const glm::vec3 crown = pos + kUp * 1.85f;
    VfxDraw& hDraw = draw(VfxMaterial::Holy, frame.addVertices(sigil::ringGeometry()), sigil::ringGeometry().size(),
                          glm::translate(glm::mat4(1.0f), crown) * glm::rotate(glm::mat4(1.0f), t * 1.8f + static_cast<float>(r), kUp) *
                          glm::rotate(glm::mat4(1.0f), 0.25f, glm::vec3(1, 0, 0)) * glm::scale(glm::mat4(1.0f), glm::vec3(0.42f)),
                          {t, kLine, 2.0f, allyFade * allyBuildup * 0.9f}, {kHolyCore, 0.0f}, glm::vec4(kHolyGold, 1.0f), pos, VfxBlend::Additive);
    hDraw.behindCard = true;
    hDraw.cardAt = pos;

    // 4. Ascending golden motes around ally (behindCard = true)
    std::vector<VfxInstance> allyMotes;
    for (int i = 0; i < 20; ++i) {
      const float seed = seeds_[static_cast<size_t>((i + r * 17) % seeds_.size())];
      const float born = std::fmod(seed * 2.9f, 0.8f);
      const float age = std::fmod(local + born, 0.8f);
      const float u = age / 0.8f;
      const float ang = static_cast<float>(i) * 2.39996f + seed * 3.0f + local * 2.2f;
      const float rad = (0.15f + 0.35f * (1.0f - u * 0.5f));
      const glm::vec3 motePos = pos + glm::vec3(std::cos(ang) * rad, 0.1f + u * 1.8f, std::sin(ang) * rad);
      const float alpha = std::sin(kPi * u) * allyFade * 0.85f;
      if (alpha > 0.01f) {
        VfxInstance inst;
        inst.a = glm::vec4(motePos, 0.1f * (1.0f - 0.3f * u));
        inst.b = {ang, 0.0f, alpha, 2.2f};
        inst.c = glm::vec4(kHolyGold, 0.0f);
        allyMotes.push_back(inst);
      }
    }
    if (!allyMotes.empty()) {
      const uint32_t firstMote = static_cast<uint32_t>(frame.instances.size());
      frame.instances.insert(frame.instances.end(), allyMotes.begin(), allyMotes.end());
      VfxDraw& mDraw = draw(VfxMaterial::Holy, frame.addVertices(icefx::quadGeometry()), icefx::quadGeometry().size(),
                            glm::mat4(1.0f), {t, kMote, 1.2f, 1.0f}, {kHolyCore, 0.0f}, glm::vec4(1.0f), pos,
                            VfxBlend::Additive, firstMote, static_cast<uint32_t>(allyMotes.size()));
      mDraw.behindCard = true;
      mDraw.cardAt = pos;
    }

    lights.push_back({pos + kUp * 0.9f, kHolyGold, 6.5f * allyBuildup * allyFade, 3.5f});
  }
}

// --- Bênção 01 (Legacy three.js port with card occlusion fixes) ---------------------------------------------------------------

void Bless::buildLegacy(float now, const Camera&, VfxFrame& frame, std::vector<VfxLight>& lights) {
  const float t = now - start_;
  if (t < 0.0f || done(now)) return;
  const glm::mat4 toWorld = vfxEmberToWorld();
  const glm::mat4 group = toWorld * glm::translate(glm::mat4(1.0f), emberCenter_);
  const glm::vec3 worldCenter = center_;
  auto draw = [&](VfxMaterial material, uint32_t first, size_t count, const glm::mat4& model, glm::vec4 p0, glm::vec4 p1, glm::vec4 color,
                  glm::vec3 sight, VfxBlend blend, uint32_t firstInstance = 0xFFFFFFFFu, uint32_t instances = 1) -> VfxDraw& {
    VfxDraw d;
    d.gpu.model = model;
    d.gpu.params0 = p0;
    d.gpu.params1 = p1;
    d.gpu.color = color;
    d.gpu.firstVertex = first;
    d.gpu.vertexCount = static_cast<uint32_t>(count);
    d.gpu.firstInstance = firstInstance;
    d.gpu.material = static_cast<uint32_t>(material);
    d.gpu.sightAt = sight;
    d.instances = instances;
    d.blend = blend;
    d.depthTest = true;
    frame.draws.push_back(d);
    return frame.draws.back();
  };

  const float release = std::max(0.0f, t - kWaveRelease);
  const float arrive = three::smoothstep(release, 0.0f, kWaveTravel);
  const float fade = t > kDuration ? std::max(0.0f, 1.0f - (t - kDuration) / 0.25f) : 1.0f;
  const float buildup = three::smoothstep(t, 0.0f, 0.28f);
  const float releasePulse = t >= kWaveRelease ? std::max(0.0f, 1.0f - (t - kWaveRelease) / 0.36f) : 0.0f;
  const float lightFade = fade;

  // --- Motes ---
  std::vector<VfxInstance> motes;
  const int capCaster = kCasterParticles, perAlly = kAllyParticles;
  for (int i = 0; i < static_cast<int>(seeds_.size()); ++i) {
    glm::vec3 p;
    float s = 0.0f;
    if (i < capCaster) {
      const float a = i * 2.39996f + seeds_[i] * 0.45f;
      const float age = std::fmod(t * 0.72f + static_cast<float>(i % 17) / 17.0f, 1.0f);
      const float radius = (0.18f + 0.7f * (1.0f - age)) * (0.5f + arrive * 0.45f);
      p = {std::cos(a + t * 0.9f) * radius, -0.12f - age * kWaveHeight, std::sin(a + t * 0.9f) * radius * 0.55f};
      s = (1.0f - age) * fade * 0.13f;
    } else if (i < capCaster + perAlly * 8) {
      const int j = i - capCaster;
      const size_t r = static_cast<size_t>(j / std::max(1, perAlly));
      if (r >= receptions_.size() || t < receptions_[r].reachAt) continue;
      const int particleIndex = j % std::max(1, perAlly);
      const float local = std::max(0.0f, t - receptions_[r].reachAt) * kAbsorptionSpeed;
      const float a = particleIndex * 2.39996f + seeds_[i] * 0.45f + local * 1.5f;
      const float age = std::fmod(local * 1.6f + static_cast<float>(particleIndex % 11) / 11.0f, 1.0f);
      const float radius = 0.14f + age * 0.28f;
      const glm::vec3 allyLocal = receptions_[r].emberPosition - emberCenter_;
      p = allyLocal + glm::vec3(std::cos(a) * radius, -0.12f - age * kStrandHeight, std::sin(a) * radius * 0.6f);
      const float absorb = three::smoothstep(local, 0.48f, 0.78f);
      p = glm::mix(p, allyLocal, absorb * 0.75f);
      s = (1.0f - age) * std::max(0.0f, 1.0f - local / 0.78f) * 0.1f;
    } else {
      continue;
    }
    if (s <= 0.0f) continue;
    motes.push_back({glm::translate(glm::mat4(1.0f), p) * three::eulerXYZ(t + i, t * 0.6f + i * 0.37f, t * 0.3f) * glm::scale(glm::mat4(1.0f), glm::vec3(s))});
  }
  if (!motes.empty()) {
    const uint32_t firstInstance = static_cast<uint32_t>(frame.instances.size());
    frame.instances.insert(frame.instances.end(), motes.begin(), motes.end());
    draw(VfxMaterial::Standard, frame.addVertices(moteGeometry()), moteGeometry().size(), group, {0.42f, 0.05f, 0, 0},
         glm::vec4(three::hex(0xffae36), kEmissive), glm::vec4(three::hex(0xffcf67), 1.0f), worldCenter, VfxBlend::Alpha, firstInstance,
         static_cast<uint32_t>(motes.size()));
  }

  // --- The wave ring, 0.025 above the board ---
  if (t >= kWaveRelease && arrive > 0.001f) {
    const float waveRadius = radius_ * kWaveRadius * arrive;
    const glm::mat4 m = group * glm::translate(glm::mat4(1.0f), {0.0f, 0.0f, 0.025f}) * glm::scale(glm::mat4(1.0f), {waveRadius, waveRadius, 1.0f});
    draw(VfxMaterial::BlessWave, frame.addVertices(ringGeometry()), ringGeometry().size(), m, {t * kWaveSpeed, kWaveTurbulence, kWaveHeight, 1.0f},
         {fade * 0.88f, kEmissive, 0, 0}, {}, worldCenter, VfxBlend::Additive);
    frame.draws.back().underCards = true;
  }

  // --- The gathering core ---
  if (t < kDuration + 0.2f) {
    const float scale = 0.3f + buildup * 1.1f + releasePulse * 0.42f;
    const glm::mat4 m = group * glm::translate(glm::mat4(1.0f), {0.0f, -0.3f + std::sin(t * 8.0f) * 0.035f, 0.06f}) * glm::scale(glm::mat4(1.0f), glm::vec3(scale));
    draw(VfxMaterial::Standard, frame.addVertices(coreGeometry()), coreGeometry().size(), m, {0.32f, 0.08f, 0, 0},
         glm::vec4(three::hex(0xffb83e), kEmissive * (0.45f + releasePulse)), glm::vec4(three::hex(0xffd77e), 1.0f), worldCenter, VfxBlend::Alpha);
    frame.draws.back().behindCard = true;
    frame.draws.back().cardAt = center_;
  }
  lights.push_back({center_ + glm::vec3(0.0f, 0.9f, 0.0f), three::hex(0xffbd50), kCasterLight * (buildup * 0.24f + releasePulse) * lightFade,
                    kCasterLightRadius});

  // --- Each ally ---
  std::vector<uint32_t> strandFirst;
  for (size_t r = 0; r < receptions_.size(); ++r) {
    const Reception& rec = receptions_[r];
    const float local = t >= rec.reachAt ? t - rec.reachAt : -1.0f;
    const float localProgress = local * kAbsorptionSpeed;
    const float localFade = local < 0.0f ? 0.0f : std::max(0.0f, 1.0f - localProgress / 0.78f);
    if (r + 8 >= receptions_.size() && local >= 0.0f)
      lights.push_back({rec.worldPosition + glm::vec3(0.0f, 0.9f, 0.0f), three::hex(0xffc85c),
                        kAllyLight * std::min(1.0f, local * 7.0f) * localFade * kLightDecay, kAllyLightRadius});
    if (local < 0.0f || localProgress >= 0.78f) continue;
    if (strandFirst.empty())
      for (int k = 0; k < std::min(10, std::max(1, kStrandCount)); ++k) strandFirst.push_back(frame.addVertices(strandGeometry(k)));
    const float absorb = three::smoothstep(localProgress, 0.48f, 0.78f);
    const float scale = std::max(0.025f, 1.0f - absorb * 0.95f);
    const float opacity = std::max(0.0f, localFade * (0.85f - absorb * 0.45f));
    for (int k = 0; k < static_cast<int>(strandFirst.size()); ++k) {
      const float phase = static_cast<float>(k) / kStrandCount * kPi * 2.0f;
      const glm::mat4 m = toWorld * glm::translate(glm::mat4(1.0f), rec.emberPosition) * three::eulerXYZ(0.0f, t * 0.3f + phase, 0.0f) *
                          glm::scale(glm::mat4(1.0f), glm::vec3(scale));
      draw(VfxMaterial::Standard, strandFirst[static_cast<size_t>(k)], strandGeometry(k).size(), m, {0.36f, 0.05f, 0, 0},
           glm::vec4(three::hex(0xffbf4c), kEmissive * (0.65f + std::sin(t * 9.0f + phase) * 0.18f)), glm::vec4(three::hex(0xffd374), opacity),
           rec.worldPosition, VfxBlend::Alpha);
      frame.draws.back().behindCard = true;
      frame.draws.back().cardAt = rec.worldPosition;
    }
  }
}

}  // namespace ember