#include "render/vfx/Bless.h"

#include <algorithm>
#include <cmath>
#include <numbers>

#include <glm/gtc/matrix_transform.hpp>

#include "render/VfxGeometry.h"

namespace ember {

namespace {

constexpr float kPi = std::numbers::pi_v<float>;
// DEFAULT_BLESS_VFX_SETTINGS
constexpr float kWaveSpeed = 1, kWaveRadius = 1, kWaveHeight = 0.42f, kWaveTurbulence = 0.22f;
constexpr int kCasterParticles = 32, kAllyParticles = 28, kStrandCount = 8;
constexpr float kStrandThickness = 0.025f, kStrandHeight = 0.85f, kStrandCurvature = 0.42f, kAbsorptionSpeed = 1, kEmissive = 4.5f;
constexpr float kCasterLight = 8, kCasterLightRadius = 5.2f, kAllyLight = 3.5f, kAllyLightRadius = 2.4f, kLightDecay = 1;
constexpr uint32_t kSeed = 31037;
constexpr float kWaveTravel = std::max(0.08f, (Bless::kDuration - Bless::kWaveRelease - 0.65f) / kWaveSpeed);

// The wave ring: a thin tube (160 x 8) around the unit circle in the xy plane.
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

Bless::Bless(glm::vec3 center, float radiusWorld, std::vector<Ally> allies, float start)
    : center_(vfxToEmber(center)), radius_(radiusWorld), start_(start) {
  for (const Ally& a : allies) receptions_.push_back({vfxToEmber(a.position), reachSeconds(a.distanceHexes)});
  // castSpell: one seed per mote, from the settings' seed.
  three::Lcg rng;
  rng.state = kSeed;
  seeds_.resize(std::max(1, kCasterParticles + kAllyParticles * 8));
  for (float& s : seeds_) s = rng.next();
}

void Bless::build(float now, const Camera&, VfxFrame& frame, std::vector<VfxLight>& lights) {
  const float t = now - start_;
  if (t < 0.0f || done(now)) return;
  const glm::mat4 toWorld = vfxEmberToWorld();
  const glm::mat4 group = toWorld * glm::translate(glm::mat4(1.0f), center_);
  const glm::vec3 worldCenter = vfxFromEmber(center_);
  auto draw = [&](VfxMaterial material, uint32_t first, size_t count, const glm::mat4& model, glm::vec4 p0, glm::vec4 p1, glm::vec4 color,
                  glm::vec3 sight, VfxBlend blend, uint32_t firstInstance = 0xFFFFFFFFu, uint32_t instances = 1) {
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
  };

  const float release = std::max(0.0f, t - kWaveRelease);
  const float arrive = three::smoothstep(release, 0.0f, kWaveTravel);
  const float fade = t > kDuration ? std::max(0.0f, 1.0f - (t - kDuration) / 0.25f) : 1.0f;
  const float buildup = three::smoothstep(t, 0.0f, 0.28f);
  const float releasePulse = t >= kWaveRelease ? std::max(0.0f, 1.0f - (t - kWaveRelease) / 0.36f) : 0.0f;
  const float lightFade = fade;

  // --- Motes (InstancedMesh, MeshStandardMaterial; three's opaque pass, so first): rising at the caster, orbiting and
  // drawn into each ally ---
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
      const glm::vec3 allyLocal = receptions_[r].position - center_;
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

  // --- The wave ring (renderOrder 9), 0.025 above the board ---
  if (t >= kWaveRelease && arrive > 0.001f) {
    const float waveRadius = radius_ * kWaveRadius * arrive;
    const glm::mat4 m = group * glm::translate(glm::mat4(1.0f), {0.0f, 0.0f, 0.025f}) * glm::scale(glm::mat4(1.0f), {waveRadius, waveRadius, 1.0f});
    draw(VfxMaterial::BlessWave, frame.addVertices(ringGeometry()), ringGeometry().size(), m, {t * kWaveSpeed, kWaveTurbulence, kWaveHeight, 1.0f},
         {fade * 0.88f, kEmissive, 0, 0}, {}, worldCenter, VfxBlend::Additive);
  }

  // --- The gathering core (renderOrder 10) ---
  if (t < kDuration + 0.2f) {
    const float scale = 0.3f + buildup * 1.1f + releasePulse * 0.42f;
    const glm::mat4 m = group * glm::translate(glm::mat4(1.0f), {0.0f, -0.3f + std::sin(t * 8.0f) * 0.035f, 0.06f}) * glm::scale(glm::mat4(1.0f), glm::vec3(scale));
    draw(VfxMaterial::Standard, frame.addVertices(coreGeometry()), coreGeometry().size(), m, {0.32f, 0.08f, 0, 0},
         glm::vec4(three::hex(0xffb83e), kEmissive * (0.45f + releasePulse)), glm::vec4(three::hex(0xffd77e), 1.0f), worldCenter, VfxBlend::Alpha);
  }
  lights.push_back({vfxFromEmber(center_ + glm::vec3(0.0f, -0.38f, 0.2f)), three::hex(0xffbd50), kCasterLight * (buildup * 0.24f + releasePulse) * lightFade,
                    kCasterLightRadius});

  // --- Each ally: its light, its eight strands ---
  std::vector<uint32_t> strandFirst;
  for (size_t r = 0; r < receptions_.size(); ++r) {
    const Reception& rec = receptions_[r];
    const float local = t >= rec.reachAt ? t - rec.reachAt : -1.0f;
    const float localProgress = local * kAbsorptionSpeed;
    const float localFade = local < 0.0f ? 0.0f : std::max(0.0f, 1.0f - localProgress / 0.78f);
    // Eight lights are pooled (index % 8): a later ally takes over an earlier one's.
    if (r + 8 >= receptions_.size() && local >= 0.0f)
      lights.push_back({vfxFromEmber(rec.position + glm::vec3(0.0f, -0.35f, 0.0f)), three::hex(0xffc85c),
                        kAllyLight * std::min(1.0f, local * 7.0f) * localFade * kLightDecay, kAllyLightRadius});
    if (local < 0.0f || localProgress >= 0.78f) continue;
    if (strandFirst.empty())
      for (int k = 0; k < std::min(10, std::max(1, kStrandCount)); ++k) strandFirst.push_back(frame.addVertices(strandGeometry(k)));
    const float absorb = three::smoothstep(localProgress, 0.48f, 0.78f);
    const float scale = std::max(0.025f, 1.0f - absorb * 0.95f);
    const float opacity = std::max(0.0f, localFade * (0.85f - absorb * 0.45f));
    for (int k = 0; k < static_cast<int>(strandFirst.size()); ++k) {
      const float phase = static_cast<float>(k) / kStrandCount * kPi * 2.0f;
      const glm::mat4 m = toWorld * glm::translate(glm::mat4(1.0f), rec.position) * three::eulerXYZ(0.0f, t * 0.3f + phase, 0.0f) *
                          glm::scale(glm::mat4(1.0f), glm::vec3(scale));
      draw(VfxMaterial::Standard, strandFirst[static_cast<size_t>(k)], strandGeometry(k).size(), m, {0.36f, 0.05f, 0, 0},
           glm::vec4(three::hex(0xffbf4c), kEmissive * (0.65f + std::sin(t * 9.0f + phase) * 0.18f)), glm::vec4(three::hex(0xffd374), opacity),
           vfxFromEmber(rec.position), VfxBlend::Alpha);
    }
  }
}

}  // namespace ember
