#include "render/vfx/PhantasmalForce.h"

#include <algorithm>
#include <cmath>
#include <numbers>

#include <glm/gtc/matrix_transform.hpp>

#include "render/VfxGeometry.h"

namespace ember {

namespace {

constexpr float kPi = std::numbers::pi_v<float>;
// DEFAULT_PHANTASMAL_FORCE_SETTINGS
constexpr float kRadius = 1.8f, kTendrilThickness = 0.035f, kTendrilTurbulence = 0.18f, kSplineNoise = 0.22f;
constexpr int kTendrilCount = 8, kParticleCount = 260;
constexpr float kParticleAttraction = 2.3f, kSpiralStrength = 1.35f, kCompressionDuration = 0.2f, kCoreSize = 0.12f, kCoreEmission = 7;
constexpr float kBuildupLight = 2.4f, kImpactLight = 22, kLightRadius = 4.8f, kShellRadius = 2, kShellSpeed = 10, kDistortion = 0.16f;
constexpr float kResidualLifetime = 0.5f;
constexpr uint32_t kSeed = 42017;
constexpr int kTendrilVerts = 18, kTendrilSides = 6;

// tubeGeometry(seed, thickness): a five-point curve from a random point to the centre, tapered, along = row / 18 in u.
std::vector<VfxVertex> tendrilGeometry(uint32_t seed, float thickness) {
  three::Mulberry random{seed};
  const glm::vec3 start{(random.next() * 2 - 1) * 0.8f, (random.next() * 2 - 1) * 0.8f, (random.next() * 2 - 1) * 0.8f};
  const glm::vec3 middle{(random.next() * 2 - 1) * 0.65f, (random.next() * 2 - 1) * 0.65f, (random.next() * 2 - 1) * 0.65f};
  const three::CatmullRom curve{{start * 1.05f, glm::mix(start, middle, 0.38f), middle, middle * 0.48f, glm::vec3(0.0f)}};
  std::vector<glm::vec3> tangents, normals, binormals;
  three::frenetFrames(curve, kTendrilVerts, tangents, normals, binormals);
  const std::vector<float> lengths = curve.getLengths();
  std::vector<VfxVertex> grid;
  for (int row = 0; row <= kTendrilVerts; ++row) {
    const float t = static_cast<float>(row) / kTendrilVerts;
    const glm::vec3 centre = three::getPointAt(curve, t, lengths);
    const float taper = thickness * (0.13f + 0.87f * std::pow(1.0f - t, 0.72f));
    for (int side = 0; side <= kTendrilSides; ++side) {
      const float angle = static_cast<float>(side) / kTendrilSides * kPi * 2.0f;
      const glm::vec3 radial = normals[row] * std::cos(angle) + binormals[row] * std::sin(angle);
      grid.push_back({centre + radial * taper, t, radial, 0.0f});
    }
  }
  std::vector<VfxVertex> out;
  for (int row = 0; row < kTendrilVerts; ++row)
    for (int side = 0; side < kTendrilSides; ++side) {
      const int a = row * (kTendrilSides + 1) + side, b = a + kTendrilSides + 1;
      out.insert(out.end(), {grid[a], grid[b], grid[a + 1], grid[b], grid[b + 1], grid[a + 1]});
    }
  return out;
}
const std::vector<VfxVertex>& tendril(int i) {
  static const std::vector<std::vector<VfxVertex>> all = [] {
    std::vector<std::vector<VfxVertex>> v;
    for (int k = 0; k < kTendrilCount; ++k) v.push_back(tendrilGeometry(kSeed + k * 137, kTendrilThickness));
    return v;
  }();
  return all[static_cast<size_t>(i)];
}
const std::vector<VfxVertex>& coreGeometry() { static const auto g = vfxgeo::icosahedron(0.5f, 3); return g; }
const std::vector<VfxVertex>& shellGeometry() { static const auto g = vfxgeo::icosahedron(1.0f, 4); return g; }
const std::vector<VfxVertex>& moteGeometry() { static const auto g = vfxgeo::icosahedron(1.0f, 0); return g; }

}  // namespace

PhantasmalForce::PhantasmalForce(glm::vec3 target, float start) : target_(vfxToEmber(target)), start_(start) {
  // restartAt: one seed per mote.
  three::Mulberry random{kSeed};
  for (int i = 0; i < kParticleCount; ++i) {
    const float z = random.next() * 2 - 1;
    const float angle = random.next() * kPi * 2;
    const float planar = std::sqrt(std::max(0.0f, 1 - z * z));
    const glm::vec3 direction = three::normalize({planar * std::cos(angle), planar * std::sin(angle), z});
    const glm::vec3 tangent = three::normalize({-direction.y, direction.x, (random.next() - 0.5f) * 0.8f});
    Seed s;
    s.direction = direction;
    s.tangent = tangent;
    s.phase = random.next() * kPi * 2;
    s.speed = 0.55f + random.next() * 1.35f;
    s.size = 0.012f + random.next() * 0.032f;
    s.delay = random.next() * 0.2f;
    seeds_.push_back(s);
  }
}

void PhantasmalForce::build(float now, const Camera&, VfxFrame& frame, std::vector<VfxLight>& lights) {
  const float t = now - start_;
  if (t < 0.0f || done(now)) return;
  const glm::mat4 group = vfxEmberToWorld() * glm::translate(glm::mat4(1.0f), target_);
  const glm::vec3 sight = vfxFromEmber(target_);
  auto draw = [&](VfxMaterial material, uint32_t first, size_t count, const glm::mat4& model, glm::vec4 p0, glm::vec4 p1, glm::vec4 color,
                  VfxBlend blend, uint32_t firstInstance = 0xFFFFFFFFu, uint32_t instances = 1) {
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

  const float collapseStart = 0.55f, collapseEnd = std::min(0.79f, collapseStart + std::max(0.08f, kCompressionDuration));
  const float collapse = three::smoothstep(t, collapseStart, collapseEnd);
  const bool impacted = t >= kImpactAt;
  const float release = std::max(0.0f, t - kImpactAt);

  // --- 260 motes (MeshStandardMaterial, per-instance HSL colour; three's opaque pass: first) ---
  {
    const float radius = kRadius * (1.0f - collapse * 0.94f);
    const glm::vec3 colour = three::hsl(0.72f + collapse * 0.06f, 0.78f, 0.5f + collapse * 0.28f);
    std::vector<VfxInstance> motes;
    for (const Seed& p : seeds_) {
      const float age = std::max(0.0f, t - p.delay);
      const float orbitAngle = p.phase + age * kSpiralStrength * p.speed;
      glm::vec3 position = p.direction * (radius * (1.0f - std::min(0.98f, age * kParticleAttraction * (0.4f + collapse * 7.0f)))) +
                           p.tangent * (std::sin(orbitAngle) * radius * 0.3f);
      if (impacted) {
        position += p.direction * std::min(kShellRadius * 0.72f, release * 2.2f * p.speed);
        position.y += release * release * 1.5f;
      }
      const float lifetime = impacted ? std::max(0.0f, 1.0f - release / std::max(0.1f, kResidualLifetime)) : 1.0f;
      const float size = p.size * (0.55f + collapse * 0.7f) * lifetime;
      if (size <= 0.0f) continue;
      VfxInstance inst;
      inst.matrix = glm::translate(glm::mat4(1.0f), position) * three::eulerXYZ(orbitAngle, orbitAngle * 0.7f, orbitAngle * 0.37f) *
                    glm::scale(glm::mat4(1.0f), glm::vec3(size));
      inst.c = glm::vec4(colour, 1.0f);
      motes.push_back(inst);
    }
    if (!motes.empty()) {
      const uint32_t firstInstance = static_cast<uint32_t>(frame.instances.size());
      frame.instances.insert(frame.instances.end(), motes.begin(), motes.end());
      draw(VfxMaterial::Standard, frame.addVertices(moteGeometry()), moteGeometry().size(), group, {0.5f, 0.1f, 0, 0}, glm::vec4(three::hex(0x8660ff), 4.0f),
           glm::vec4(three::hex(0x9f78ff), 1.0f), VfxBlend::Alpha, firstInstance, static_cast<uint32_t>(motes.size()));
    }
  }

  // --- The tendrils (renderOrder 6) ---
  const float tendrilOpacity = t < 0.55f ? std::min(1.0f, t / 0.15f) * 0.9f
                                         : 0.9f * (1.0f - collapse * 0.75f) *
                                               std::max(0.0f, 1.0f - std::max(0.0f, t - 0.98f) / std::max(0.08f, kResidualLifetime));
  if (tendrilOpacity > 0.005f)
    for (int i = 0; i < std::max(1, std::min(16, kTendrilCount)); ++i)
      draw(VfxMaterial::PfTendril, frame.addVertices(tendril(i)), tendril(i).size(), group * glm::scale(glm::mat4(1.0f), glm::vec3(kRadius)),
           {t, collapse, kTendrilTurbulence, kSplineNoise}, {tendrilOpacity, 0, 0, 0}, {}, VfxBlend::Additive);

  // --- The shell (renderOrder 7): expanding from the burst ---
  if (impacted) {
    const float shellRadius = std::max(0.01f, std::min(kShellRadius, release * kShellSpeed));
    const float opacity = std::max(0.0f, 0.72f * (1.0f - shellRadius / std::max(0.01f, kShellRadius)));
    if (release < kShellRadius / std::max(0.1f, kShellSpeed) && opacity > 0.005f)
      draw(VfxMaterial::PfShell, frame.addVertices(shellGeometry()), shellGeometry().size(), group * glm::scale(glm::mat4(1.0f), glm::vec3(shellRadius)),
           {t, kDistortion, 0, 0}, {opacity, 0, 0, 0}, {}, VfxBlend::Additive);
  }

  // --- The core (renderOrder 8) ---
  if (t >= 0.12f && t < kEndAt) {
    const float coreScale = kCoreSize * (0.35f + collapse * 1.9f + (impacted ? 0.18f : 0.0f));
    draw(VfxMaterial::PfCore, frame.addVertices(coreGeometry()), coreGeometry().size(),
         group * glm::translate(glm::mat4(1.0f), {0.0f, 0.0f, 0.015f}) * glm::scale(glm::mat4(1.0f), glm::vec3(coreScale)),
         {t, 0.055f + collapse * 0.13f, 0, 0}, {kCoreEmission * (0.35f + collapse * 1.1f + (impacted ? 0.3f : 0.0f)), 0, 0, 0}, {}, VfxBlend::Additive);
  }

  // --- The light: building, a spike at the burst, fading ---
  const float buildup = three::smoothstep(t, 0.02f, 0.5f);
  const float spike = impacted ? std::max(0.0f, 1.0f - release / 0.14f) : 0.0f;
  const float fade = t > kImpactAt ? std::max(0.0f, 1.0f - release / std::max(0.08f, kResidualLifetime)) : 1.0f;
  const float intensity = (kBuildupLight * buildup * (1.0f - collapse * 0.45f) + kImpactLight * spike) * fade;
  if (intensity > 0.0f) lights.push_back({sight, three::hex(0x9b75ff), intensity, kLightRadius});
}

}  // namespace ember
