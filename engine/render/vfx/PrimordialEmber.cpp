#include "render/vfx/PrimordialEmber.h"

#include <algorithm>
#include <cmath>
#include <numbers>

#include <glm/gtc/matrix_transform.hpp>

#include "render/VfxGeometry.h"
#include "render/vfx/ThreeMath.h"

namespace ember {

namespace {

constexpr float kPi = std::numbers::pi_v<float>;
constexpr float kTau = 2.0f * kPi;
constexpr float kHex = 1.7320508f;
constexpr float kRingDelay = 0.055f;
constexpr float kFlameLife = 1.3f;
constexpr float kAirFoot = 0.6f;

constexpr glm::vec3 kEmberGlow{1.0f, 0.44f, 0.14f};
constexpr glm::vec3 kCore{1.0f, 0.92f, 0.82f};

// The Combat material's modes.
constexpr float kStreak = 0.0f, kRing = 3.0f;

const std::vector<VfxVertex>& quadGeometry() {
  static const std::vector<VfxVertex> g = [] {
    const VfxVertex a{{-0.5f, -0.5f, 0}, 0, {0, 0, 1}, 0}, b{{0.5f, -0.5f, 0}, 1, {0, 0, 1}, 0}, c{{0.5f, 0.5f, 0}, 1, {0, 0, 1}, 1},
        d{{-0.5f, 0.5f, 0}, 0, {0, 0, 1}, 1};
    return std::vector<VfxVertex>{a, b, c, a, c, d};
  }();
  return g;
}

const std::vector<VfxVertex>& discGeometry() {
  static const std::vector<VfxVertex> g = [] {
    constexpr int around = 48, out = 8;
    auto v = [](int i, int j) {
      const float a = static_cast<float>(i) / around * kTau, r = static_cast<float>(j) / out;
      return VfxVertex{{std::cos(a) * r, 0.0f, std::sin(a) * r}, static_cast<float>(i) / around, {0.0f, 1.0f, 0.0f}, r};
    };
    std::vector<VfxVertex> tris;
    for (int i = 0; i < around; ++i)
      for (int j = 0; j < out; ++j) tris.insert(tris.end(), {v(i, j), v(i, j + 1), v(i + 1, j), v(i + 1, j), v(i, j + 1), v(i + 1, j + 1)});
    return tris;
  }();
  return g;
}

float smooth(float a, float b, float x) {
  const float t = std::clamp((x - a) / std::max(b - a, 1e-5f), 0.0f, 1.0f);
  return t * t * (3.0f - 2.0f * t);
}
float easeOut(float edge0, float edge1, float x) {
  const float t = std::clamp((x - edge0) / std::max(edge1 - edge0, 1e-5f), 0.0f, 1.0f);
  return 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t);
}

}  // namespace

PrimordialEmber::PrimordialEmber(glm::vec3 casterFeet, glm::vec3 casterBody, float casterHeight,
                                 std::vector<HexSpot> hexes, float start, float impact, uint32_t seed)
    : casterFeet_(casterFeet), casterBody_(casterBody), casterHeight_(std::clamp(casterHeight, 1.0f, 4.0f)),
      hexes_(std::move(hexes)), start_(start), impact_(std::max(impact, start + 0.1f)), seed_(seed) {
  if (casterBody_ == glm::vec3(0.0f)) casterBody_ = casterFeet_ + glm::vec3(0.0f, casterHeight_ * 0.6f, 0.0f);
  if (hexes_.empty()) hexes_.push_back({casterFeet_, casterBody_, casterHeight_});

  centre_ = hexes_.front().feet;
  three::Lcg rand;
  rand.state = seed_;

  for (const HexSpot& h : hexes_) {
    const float dist = glm::length(glm::vec2(h.feet.x - centre_.x, h.feet.z - centre_.z));
    reach_ = std::max(reach_, dist);
  }

  const float preLead = std::min(0.85f, std::max(impact_ - start_ - 0.05f, 0.3f));

  // Central colossal plume
  Plume corePlume;
  corePlume.foot = centre_;
  corePlume.delay = 0.0f;
  corePlume.life = 1.7f;
  corePlume.height = 5.4f;
  corePlume.radius = 1.35f;
  corePlume.seed = rand.next() * 73.0f;
  plumes_.push_back(corePlume);

  // Plumes across all hexes
  for (const HexSpot& h : hexes_) {
    const float dist = glm::length(glm::vec2(h.feet.x - centre_.x, h.feet.z - centre_.z));
    const float ring = std::round(dist / kHex);

    Plume p;
    p.foot = h.feet + glm::vec3((rand.next() - 0.5f) * 0.3f, 0.0f, (rand.next() - 0.5f) * 0.3f);
    p.delay = ring * kRingDelay + rand.next() * 0.04f;
    p.life = kFlameLife * (0.9f + rand.next() * 0.3f);
    p.height = (ring < 0.5f ? 3.6f : 2.5f) * (0.85f + rand.next() * 0.3f);
    p.radius = 0.58f + rand.next() * 0.14f;
    p.seed = rand.next() * 91.0f;
    plumes_.push_back(p);

    // Rising pre-impact embers across the hex before the blast
    for (int i = 0; i < 4; ++i) {
      Particle pt{};
      pt.kind = 1;
      pt.at = -preLead + rand.next() * preLead * 0.85f;
      pt.life = 0.6f + rand.next() * 0.5f;
      pt.size = 0.09f + rand.next() * 0.07f;
      pt.spin = (rand.next() - 0.5f) * 3.0f;
      pt.rotation = rand.next() * kTau;
      pt.from = h.feet + glm::vec3((rand.next() - 0.5f) * 1.2f, 0.05f, (rand.next() - 0.5f) * 1.2f);
      pt.velocity = {(rand.next() - 0.5f) * 0.4f, 0.8f + rand.next() * 0.9f, (rand.next() - 0.5f) * 0.4f};
      pt.gravity = -0.15f;
      pt.drag = 0.5f;
      pt.color = glm::vec3(1.0f, 0.65f + rand.next() * 0.25f, 0.35f);
      particles_.push_back(pt);
    }

    // Explosive fountain of incandescent sparks/embers when each hex erupts
    for (int i = 0; i < 12; ++i) {
      Particle pt{};
      pt.kind = 1;
      pt.at = p.delay + 0.02f + rand.next() * 0.10f;
      pt.life = 0.55f + rand.next() * 0.55f;
      pt.size = 0.08f + rand.next() * 0.06f;
      pt.spin = (rand.next() - 0.5f) * 4.0f;
      pt.rotation = rand.next() * kTau;
      const float a = rand.next() * kTau;
      pt.from = p.foot + glm::vec3(0.0f, 0.4f + rand.next() * 1.2f, 0.0f);
      pt.velocity = glm::vec3(std::cos(a) * 1.8f, 2.8f + rand.next() * 2.8f, std::sin(a) * 1.8f) * (0.6f + rand.next() * 0.6f);
      pt.gravity = 6.2f;
      pt.drag = 0.85f;
      pt.color = glm::vec3(1.0f, 0.8f - rand.next() * 0.3f, 0.4f);
      particles_.push_back(pt);
    }

    // Billowing smoke as the hex burns down
    for (int i = 0; i < 3; ++i) {
      Particle pt{};
      pt.kind = 2;
      pt.at = p.delay + p.life * (0.45f + rand.next() * 0.35f);
      pt.life = 1.3f + rand.next() * 0.8f;
      pt.size = 0.85f + rand.next() * 0.6f;
      pt.spin = (rand.next() - 0.5f) * 0.6f;
      pt.rotation = rand.next() * kTau;
      pt.from = p.foot + glm::vec3((rand.next() - 0.5f) * 0.5f, 1.0f + rand.next() * 1.0f, (rand.next() - 0.5f) * 0.5f);
      pt.velocity = {(rand.next() - 0.5f) * 0.3f, 0.6f + rand.next() * 0.5f, (rand.next() - 0.5f) * 0.3f};
      pt.gravity = -0.15f;
      pt.drag = 0.45f;
      particles_.push_back(pt);
    }
  }

  duration_ = 0.0f;
  for (const Plume& p : plumes_) duration_ = std::max(duration_, p.delay + p.life + 0.3f);
  for (const Particle& pt : particles_) duration_ = std::max(duration_, pt.at + pt.life);
}

void PrimordialEmber::buildCaster(float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) const {
  const float release = impact_ + 0.15f;
  if (now >= release) return;
  (void)camera;

  const float f = smooth(start_, impact_, now), fade = 1.0f - smooth(impact_, release, now);
  const uint32_t disc = frame.addVertices(discGeometry());

  // Fiery ember ring on the ground at the caster's feet
  VfxDraw d;
  d.gpu.model = glm::translate(glm::mat4(1.0f), casterFeet_ + glm::vec3(0.0f, 0.04f, 0.0f)) * glm::scale(glm::mat4(1.0f), glm::vec3(0.45f + 0.75f * f));
  d.gpu.params0 = {now, kRing, 2.0f, (0.4f + 0.6f * f) * fade};
  d.gpu.params1 = glm::vec4(kCore, 0.88f);
  d.gpu.color = glm::vec4(kEmberGlow, 0.0f);
  d.gpu.firstVertex = disc, d.gpu.vertexCount = static_cast<uint32_t>(discGeometry().size()), d.gpu.firstInstance = 0xFFFFFFFFu;
  d.gpu.material = static_cast<uint32_t>(VfxMaterial::Combat);
  d.gpu.sightAt = casterFeet_;
  d.blend = VfxBlend::Additive;
  d.depthTest = true;
  frame.draws.push_back(d);

  lights.push_back({casterBody_, kEmberGlow, (4.0f + 14.0f * f) * fade, 3.5f});
}

void PrimordialEmber::build(float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) const {
  const float ti = now - impact_;
  if (now < start_ || done(now)) return;

  buildCaster(now, camera, frame, lights);

  const uint32_t quad = frame.addVertices(quadGeometry());
  const uint32_t disc = frame.addVertices(discGeometry());

  // Expanding fiery primordial shockwave ring on the ground
  if (ti >= 0.0f && ti < 0.85f) {
    const float ringAge = ti / 0.85f;
    const float ringRadius = (0.5f + (reach_ + 1.8f) * easeOut(0.0f, 1.0f, ringAge));
    const float ringAlpha = (1.0f - smooth(0.2f, 1.0f, ringAge));
    if (ringAlpha > 0.01f) {
      VfxDraw d;
      d.gpu.model = glm::translate(glm::mat4(1.0f), centre_ + glm::vec3(0.0f, 0.04f, 0.0f)) * glm::scale(glm::mat4(1.0f), glm::vec3(ringRadius));
      d.gpu.params0 = {now, kRing, 2.6f, ringAlpha};
      d.gpu.params1 = glm::vec4(kCore, 0.92f);
      d.gpu.color = glm::vec4(kEmberGlow, 0.0f);
      d.gpu.firstVertex = disc, d.gpu.vertexCount = static_cast<uint32_t>(discGeometry().size()), d.gpu.firstInstance = 0xFFFFFFFFu;
      d.gpu.material = static_cast<uint32_t>(VfxMaterial::Combat);
      d.gpu.sightAt = centre_;
      d.blend = VfxBlend::Additive;
      d.depthTest = true;
      frame.draws.push_back(d);
    }
  }

  // Scorched hot ground pools on each hex
  if (ti >= -0.5f) {
    for (const HexSpot& h : hexes_) {
      const float heatAlpha = ti < 0.0f ? smooth(-0.5f, 0.0f, ti) * 0.5f : (1.0f - smooth(0.3f, 1.8f, ti)) * 0.7f;
      if (heatAlpha > 0.01f) {
        VfxDraw d;
        d.gpu.model = glm::translate(glm::mat4(1.0f), h.feet + glm::vec3(0.0f, 0.04f, 0.0f)) * glm::scale(glm::mat4(1.0f), glm::vec3(0.85f));
        d.gpu.params0 = {now, kRing, 1.8f, heatAlpha};
        d.gpu.params1 = glm::vec4(kCore, 0.85f);
        d.gpu.color = glm::vec4(kEmberGlow, 0.0f);
        d.gpu.firstVertex = disc, d.gpu.vertexCount = static_cast<uint32_t>(discGeometry().size()), d.gpu.firstInstance = 0xFFFFFFFFu;
        d.gpu.material = static_cast<uint32_t>(VfxMaterial::Combat);
        d.gpu.sightAt = h.feet;
        d.blend = VfxBlend::Additive;
        d.depthTest = true;
        frame.draws.push_back(d);
      }
    }
  }

  // 3D Fire volumes: roaring columns and hex plumes
  for (const Plume& p : plumes_) {
    const float a = ti - p.delay;
    if (a < 0.0f || a > p.life) continue;
    const float grow = smooth(0.0f, 0.16f, a), fade = smooth(p.life * 0.5f, p.life, a);
    const float foot = kAirFoot * (1.0f - smooth(0.0f, 0.12f, a)) + 0.4f * smooth(p.life * 0.6f, p.life, a);
    const float height = p.height * (0.35f + 0.65f * grow) * (1.0f - 0.65f * fade);
    const float radius = p.radius * (0.6f + 0.4f * grow) * (1.0f - 0.35f * fade);
    const float strength = (2.8f + 1.4f * (1.0f - smooth(0.0f, 0.25f, a))) * (1.0f - fade);
    if (strength <= 0.01f) continue;

    const glm::vec3 at = p.foot + glm::vec3(0.0f, foot, 0.0f);
    VfxDraw d;
    d.gpu.model = glm::mat4(1.0f);
    d.gpu.params0 = {now + p.seed, strength, height, radius};
    d.gpu.params1 = {at, p.seed};
    d.gpu.color = {1.0f, 0.0f, 0.0f, 0.0f};  // body 0 (flame), turbulence 1.0
    d.gpu.firstVertex = quad, d.gpu.vertexCount = 6, d.gpu.firstInstance = 0xFFFFFFFFu;
    d.gpu.material = static_cast<uint32_t>(VfxMaterial::FireVolume);
    d.gpu.sightAt = at + glm::vec3(0.0f, height * 0.5f, 0.0f);
    d.blend = VfxBlend::Alpha;
    d.depthTest = true;
    frame.draws.push_back(d);
  }

  // Particles: rising embers, spark fountains, and smoke
  std::vector<VfxInstance> live;
  for (const Particle& p : particles_) {
    const float age = ti - p.at;
    if (age < 0.0f || age >= p.life) continue;
    const float tt = age / p.life;
    const float dragTerm = p.drag < 0.001f ? age : (1.0f - std::exp(-p.drag * age)) / p.drag;
    const glm::vec3 pos = p.from + p.velocity * dragTerm - glm::vec3(0.0f, 0.5f * p.gravity * age * age, 0.0f);

    VfxInstance inst;
    const bool smoke = p.kind == 2;
    const float scale = p.size * (smoke ? 0.7f + tt * 1.5f : 0.7f + (1.0f - tt) * 0.5f);
    const float alpha = std::pow(std::sin(kPi * tt), smoke ? 0.5f : 0.7f) * (smoke ? 0.32f * std::pow(1.0f - tt, 0.6f) : 1.0f);
    const float emission = smoke ? 0.35f : 4.8f * (1.0f - tt);
    const glm::vec3 color = smoke ? glm::vec3(0.24f, 0.2f, 0.18f) : p.color;
    inst.a = {pos, scale};
    inst.b = {p.rotation + p.spin * age, 0.0f, alpha, emission};
    inst.c = {color, smoke ? 2.0f : 1.0f};
    live.push_back(inst);
  }

  if (!live.empty()) {
    const uint32_t firstInstance = static_cast<uint32_t>(frame.instances.size());
    frame.instances.insert(frame.instances.end(), live.begin(), live.end());
    VfxDraw d;
    d.gpu.model = glm::mat4(1.0f);
    d.gpu.firstVertex = quad, d.gpu.vertexCount = 6;
    d.gpu.firstInstance = firstInstance;
    d.gpu.material = static_cast<uint32_t>(VfxMaterial::ImpactParticle);
    d.gpu.sightAt = centre_;
    d.instances = static_cast<uint32_t>(live.size());
    d.blend = VfxBlend::Alpha;
    d.depthTest = true;
    frame.draws.push_back(d);
  }

  // Dynamic Scene Lighting
  const float gather = ti < 0.0f ? smooth(start_ - impact_, 0.0f, ti) * 8.0f : 0.0f;
  const float flare = ti >= 0.0f ? 85.0f * smooth(0.0f, 0.08f, ti) * (1.0f - smooth(0.18f, 1.8f, ti)) : 0.0f;
  const float flicker = 0.88f + 0.12f * std::sin(now * 21.0f) * std::sin(now * 8.7f + 1.3f);
  const float intensity = (gather + flare) * flicker;
  if (intensity > 0.05f) {
    lights.push_back({centre_ + glm::vec3(0.0f, 1.5f, 0.0f), kEmberGlow, intensity, reach_ + 5.5f});
  }
}

}  // namespace ember
