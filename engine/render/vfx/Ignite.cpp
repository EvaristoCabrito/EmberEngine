#include "render/vfx/Ignite.h"

#include <algorithm>
#include <cmath>
#include <numbers>

#include "render/vfx/ThreeMath.h"

namespace ember {

namespace {

constexpr float kPi = std::numbers::pi_v<float>;
constexpr float kHex = 1.7320508f;     // the distance between two hexes' centres (world)
constexpr float kRingDelay = 0.075f;   // each ring outward catches fire this much later
constexpr float kFlameLife = 1.0f;     // a hex's flame, from bursting out of the air to burning out
constexpr float kAirFoot = 0.85f;      // where the flame first appears: in the air, this high over the ground

// A camera-facing quad (corners at +-0.5): the flames' bound and the particles' card.
const std::vector<VfxVertex>& quadGeometry() {
  static const std::vector<VfxVertex> g = [] {
    const VfxVertex a{{-0.5f, -0.5f, 0}, 0, {0, 0, 1}, 0}, b{{0.5f, -0.5f, 0}, 1, {0, 0, 1}, 0}, c{{0.5f, 0.5f, 0}, 1, {0, 0, 1}, 1},
        d{{-0.5f, 0.5f, 0}, 0, {0, 0, 1}, 1};
    return std::vector<VfxVertex>{a, b, c, a, c, d};
  }();
  return g;
}

float smooth(float a, float b, float x) {
  const float t = three::clamp((x - a) / (b - a), 0.0f, 1.0f);
  return t * t * (3.0f - 2.0f * t);
}

}  // namespace

Ignite::Ignite(std::vector<glm::vec3> cells, glm::vec3 centre, float impact, uint32_t seed) : centre_(centre), impact_(impact) {
  three::Lcg rand;
  rand.state = seed;
  if (cells.empty()) cells.push_back(centre);
  for (const glm::vec3& c : cells) reach_ = std::max(reach_, glm::length(glm::vec2(c.x - centre.x, c.z - centre.z)));
  for (const glm::vec3& c : cells) {
    const float ring = std::round(glm::length(glm::vec2(c.x - centre.x, c.z - centre.z)) / kHex);
    Flame f;
    f.foot = c + glm::vec3((rand.next() - 0.5f) * 0.25f, 0.0f, (rand.next() - 0.5f) * 0.25f);
    f.delay = ring * kRingDelay + rand.next() * 0.05f;
    f.life = kFlameLife * (0.85f + rand.next() * 0.3f);
    f.height = (ring < 0.5f ? 2.5f : 1.9f) * (0.85f + rand.next() * 0.3f);
    f.radius = 0.48f + rand.next() * 0.1f;
    f.seed = rand.next() * 97.0f;
    flames_.push_back(f);
    // the heat before it: embers drifting up off the ground (from the lead's start)
    for (int i = 0; i < 3; ++i) {
      Particle p{};
      p.kind = 1;
      p.at = -kLead + rand.next() * kLead * 0.8f;
      p.life = 0.5f + rand.next() * 0.5f;
      p.size = 0.07f + rand.next() * 0.06f;
      p.spin = (rand.next() - 0.5f) * 3.0f, p.rotation = rand.next() * kPi * 2.0f;
      p.from = c + glm::vec3((rand.next() - 0.5f) * 1.3f, 0.05f, (rand.next() - 0.5f) * 1.3f);
      p.velocity = {(rand.next() - 0.5f) * 0.3f, 0.6f + rand.next() * 0.7f, (rand.next() - 0.5f) * 0.3f};
      p.gravity = -0.2f, p.drag = 0.6f;
      particles_.push_back(p);
    }
    // the sparks thrown as it ignites
    for (int i = 0; i < 6; ++i) {
      Particle p{};
      p.kind = 1;
      p.at = f.delay + 0.03f + rand.next() * 0.12f;
      p.life = 0.45f + rand.next() * 0.45f;
      p.size = 0.06f + rand.next() * 0.05f;
      p.spin = (rand.next() - 0.5f) * 4.0f, p.rotation = rand.next() * kPi * 2.0f;
      const float a = rand.next() * kPi * 2.0f;
      p.from = f.foot + glm::vec3(0.0f, 0.3f + rand.next() * 0.9f, 0.0f);
      p.velocity = glm::vec3(std::cos(a) * 1.4f, 2.2f + rand.next() * 2.2f, std::sin(a) * 1.4f) * (0.6f + rand.next() * 0.6f);
      p.gravity = 5.5f, p.drag = 0.9f;
      particles_.push_back(p);
    }
    // the smoke it leaves as it burns out
    for (int i = 0; i < 2; ++i) {
      Particle p{};
      p.kind = 2;
      p.at = f.delay + f.life * (0.45f + rand.next() * 0.35f);
      p.life = 1.1f + rand.next() * 0.7f;
      p.size = 0.7f + rand.next() * 0.5f;
      p.spin = (rand.next() - 0.5f) * 0.6f, p.rotation = rand.next() * kPi * 2.0f;
      p.from = f.foot + glm::vec3((rand.next() - 0.5f) * 0.4f, 0.8f + rand.next() * 0.8f, (rand.next() - 0.5f) * 0.4f);
      p.velocity = {(rand.next() - 0.5f) * 0.25f, 0.5f + rand.next() * 0.4f, (rand.next() - 0.5f) * 0.25f};
      p.gravity = -0.1f, p.drag = 0.5f;
      particles_.push_back(p);
    }
  }
  duration_ = 0.0f;
  for (const Flame& f : flames_) duration_ = std::max(duration_, f.delay + f.life);
  for (const Particle& p : particles_) duration_ = std::max(duration_, p.at + p.life);
}

void Ignite::build(float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) const {
  (void)camera;
  const float ti = now - impact_;  // < 0 while the heat gathers
  if (ti < -kLead || done(now)) return;
  const uint32_t quad = frame.addVertices(quadGeometry());

  // The flames: each bursts out of the air, drops onto its hex and roars up, then lifts off and burns out.
  for (const Flame& f : flames_) {
    const float a = ti - f.delay;
    if (a < 0.0f || a > f.life) continue;
    const float grow = smooth(0.0f, 0.14f, a), fade = smooth(f.life * 0.55f, f.life, a);
    const float foot = kAirFoot * (1.0f - smooth(0.0f, 0.12f, a)) + 0.6f * smooth(f.life * 0.6f, f.life, a);
    const float height = f.height * (0.3f + 0.7f * grow) * (1.0f - 0.65f * fade);
    const float radius = f.radius * (0.6f + 0.4f * grow) * (1.0f - 0.35f * fade);
    const float strength = (2.4f + 1.2f * (1.0f - smooth(0.0f, 0.25f, a))) * (1.0f - fade);
    if (strength <= 0.01f) continue;
    const glm::vec3 at = f.foot + glm::vec3(0.0f, foot, 0.0f);
    VfxDraw d;
    d.gpu.model = glm::mat4(1.0f);
    d.gpu.params0 = {now + f.seed, strength, height, radius};
    d.gpu.params1 = {at, f.seed};
    d.gpu.color = {1.0f, 0.0f, 0.0f, 0.0f};  // a torch's turbulence, no pull, body 0 (a flame)
    d.gpu.firstVertex = quad, d.gpu.vertexCount = 6, d.gpu.firstInstance = 0xFFFFFFFFu;
    d.gpu.material = static_cast<uint32_t>(VfxMaterial::FireVolume);
    d.gpu.sightAt = at + glm::vec3(0.0f, height * 0.5f, 0.0f);
    d.blend = VfxBlend::Alpha;
    d.depthTest = true;
    frame.draws.push_back(d);
  }

  // The embers, the sparks and the smoke.
  std::vector<VfxInstance> live;
  for (const Particle& p : particles_) {
    const float age = ti - p.at;
    if (age < 0.0f || age >= p.life) continue;
    const float tt = age / p.life;
    const float dragTerm = p.drag < 0.001f ? age : (1.0f - std::exp(-p.drag * age)) / p.drag;
    const glm::vec3 pos = p.from + p.velocity * dragTerm - glm::vec3(0.0f, 0.5f * p.gravity * age * age, 0.0f);
    VfxInstance inst;
    const bool smoke = p.kind == 2;
    const float scale = p.size * (smoke ? 0.7f + tt * 1.4f : 0.7f + (1.0f - tt) * 0.5f);
    const float alpha = std::pow(std::sin(kPi * tt), smoke ? 0.5f : 0.7f) * (smoke ? 0.28f * std::pow(1.0f - tt, 0.6f) : 1.0f);
    const float emission = smoke ? 0.4f : 4.5f * (1.0f - tt);
    const glm::vec3 color = smoke ? glm::vec3(0.22f, 0.2f, 0.18f) : glm::vec3(1.0f, 0.9f - tt * 0.55f, 0.6f - tt * 0.5f);
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

  // The light: a red glow while the heat gathers, the flare as the air ignites, a flicker as the fire dies.
  const float gather = ti < 0.0f ? smooth(-kLead, 0.0f, ti) * 5.0f : 0.0f;
  const float flare = ti >= 0.0f ? 34.0f * smooth(0.0f, 0.06f, ti) * (1.0f - smooth(0.12f, kFlameLife + kRingDelay * 2.0f, ti)) : 0.0f;
  const float flicker = 0.88f + 0.12f * std::sin(now * 23.0f) * std::sin(now * 7.3f + 1.1f);
  const float intensity = (gather + flare) * flicker;
  if (intensity > 0.01f) lights.push_back({centre_ + glm::vec3(0.0f, 1.0f, 0.0f), three::hex(0xff7a2a), intensity, reach_ + 3.0f});
}

}  // namespace ember
