#include "render/vfx/Ignite.h"

#include <algorithm>
#include <cmath>
#include <numbers>

#include "render/vfx/ThreeMath.h"

namespace ember {

namespace {

constexpr float kPi = std::numbers::pi_v<float>;
constexpr float kHex = 1.7320508f;    // the distance between two hexes' centres (world)
constexpr float kRingDelay = 0.09f;   // each ring outward catches fire this much later
constexpr float kBurstLife = 0.5f;    // the fire bursting up off a hex as it catches
constexpr float kFlameLife = 1.6f;    // a tongue of fire, from rising off the ground to sinking back into it
// Everything stays on the area's hexes (the user, 2026-11-25: "está desenhando muito além da area de efeito"): every flame, burst
// and particle keeps inside its own hex (its inner radius is 0.866) and the light reaches only the area's edge.
constexpr float kHexEdge = 1.0f;      // a hex's outer radius (world)

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
  auto jitter = [&](float r) { return glm::vec3((rand.next() - 0.5f) * 2.0f * r, 0.0f, (rand.next() - 0.5f) * 2.0f * r); };
  if (cells.empty()) cells.push_back(centre);
  for (const glm::vec3& c : cells) reach_ = std::max(reach_, glm::length(glm::vec2(c.x - centre.x, c.z - centre.z)));
  burn_ = 0.0f;
  for (const glm::vec3& c : cells) {
    const float ring = std::round(glm::length(glm::vec2(c.x - centre.x, c.z - centre.z)) / kHex);
    const bool middle = ring < 0.5f;
    const float catches = ring * kRingDelay + rand.next() * 0.05f;  // when this hex catches fire (from the impact)

    // the fire bursting up off the hex as it catches (a dome of fire that tears apart), then a low bed of fire boiling on it
    bursts_.push_back({c, catches, kBurstLife * (0.9f + rand.next() * 0.2f), middle ? 0.78f : 0.72f, rand.next() * 97.0f,
                       kFlameLife * (0.95f + rand.next() * 0.1f)});

    // the blaze on it: a tall tongue in the middle and four lower ones around it, each its own height, sway and timing
    const float turn = rand.next() * kPi * 2.0f;
    for (int i = 0; i < 5; ++i) {
      Flame f;
      const bool core = i == 0;
      const float a = turn + (i - 1) * kPi * 0.5f + (rand.next() - 0.5f) * 0.6f;
      const float out = core ? 0.0f : 0.26f + rand.next() * 0.08f;
      f.foot = c + (core ? jitter(0.06f) : glm::vec3(std::cos(a) * out, 0.0f, std::sin(a) * out));
      f.delay = catches + (core ? 0.0f : 0.03f + rand.next() * 0.12f);
      f.life = kFlameLife * (core ? 1.0f : 0.75f + rand.next() * 0.25f) * (0.92f + rand.next() * 0.16f);
      f.height = (core ? (middle ? 2.3f : 1.8f) : 0.95f + rand.next() * 0.55f) * (0.9f + rand.next() * 0.2f);
      f.radius = core ? 0.38f + rand.next() * 0.05f : 0.22f + rand.next() * 0.06f;
      f.strength = core ? 2.6f : 2.0f + rand.next() * 0.4f;
      f.turbulence = core ? 1.3f : 1.5f;
      f.seed = rand.next() * 97.0f;
      flames_.push_back(f);
      burn_ = std::max(burn_, f.delay + f.life);
    }
    const float life = kFlameLife;

    // the heat before it: embers drifting up off the ground (from the lead's start)
    for (int i = 0; i < 4; ++i) {
      Particle p{};
      p.kind = 1;
      p.at = -kLead + rand.next() * kLead * 0.8f;
      p.life = 0.5f + rand.next() * 0.5f;
      p.size = 0.07f + rand.next() * 0.06f;
      p.spin = (rand.next() - 0.5f) * 3.0f, p.rotation = rand.next() * kPi * 2.0f;
      p.from = c + jitter(0.3f) + glm::vec3(0.0f, 0.05f, 0.0f);
      p.velocity = glm::vec3(0.0f, 0.6f + rand.next() * 0.7f, 0.0f) + jitter(0.07f);
      p.gravity = -0.2f, p.drag = 0.6f;
      p.emission = 4.5f, p.color = {1.0f, 0.72f, 0.3f};
      particles_.push_back(p);
    }
    // the sparks thrown up as it catches (straight up off the fire, over its own hex)
    for (int i = 0; i < 8; ++i) {
      Particle p{};
      p.kind = 1;
      p.at = catches + 0.03f + rand.next() * 0.15f;
      p.life = 0.45f + rand.next() * 0.45f;
      p.size = 0.06f + rand.next() * 0.05f;
      p.spin = (rand.next() - 0.5f) * 4.0f, p.rotation = rand.next() * kPi * 2.0f;
      const float a = rand.next() * kPi * 2.0f;
      p.from = c + jitter(0.15f) + glm::vec3(0.0f, 0.3f + rand.next() * 0.9f, 0.0f);
      p.velocity = glm::vec3(std::cos(a) * 0.3f, 2.2f + rand.next() * 2.2f, std::sin(a) * 0.3f) * (0.6f + rand.next() * 0.6f);
      p.gravity = 5.5f, p.drag = 0.9f;
      p.emission = 5.5f, p.color = {1.0f, 0.92f, 0.62f};
      particles_.push_back(p);
    }
    // embers climbing out of the blaze all the while it burns
    for (int i = 0; i < 8; ++i) {
      Particle p{};
      p.kind = 1;
      p.at = catches + 0.1f + rand.next() * life * 0.85f;
      p.life = 0.9f + rand.next() * 0.6f;
      p.size = 0.045f + rand.next() * 0.04f;
      p.spin = (rand.next() - 0.5f) * 3.0f, p.rotation = rand.next() * kPi * 2.0f;
      p.from = c + jitter(0.25f) + glm::vec3(0.0f, 0.2f + rand.next() * 0.7f, 0.0f);
      p.velocity = glm::vec3(0.0f, 0.9f + rand.next() * 0.7f, 0.0f) + jitter(0.1f);
      p.gravity = -0.3f, p.drag = 0.4f;
      p.emission = 4.5f, p.color = {1.0f, 0.62f, 0.18f};
      particles_.push_back(p);
    }
    // the smoke rising over it as it burns out
    for (int i = 0; i < 3; ++i) {
      Particle p{};
      p.kind = 2;
      p.at = catches + life * (0.35f + rand.next() * 0.45f);
      p.life = 1.4f + rand.next() * 0.6f;
      p.size = 0.5f + rand.next() * 0.25f;  // at most ~1.6 wide as it spreads: the hex's own width
      p.spin = (rand.next() - 0.5f) * 0.6f, p.rotation = rand.next() * kPi * 2.0f;
      p.from = c + jitter(0.12f) + glm::vec3(0.0f, 1.3f + rand.next() * 0.8f, 0.0f);
      p.velocity = glm::vec3(0.0f, 0.5f + rand.next() * 0.35f, 0.0f) + jitter(0.05f);
      p.gravity = -0.1f, p.drag = 0.5f;
      p.emission = 0.4f, p.color = {0.2f, 0.18f, 0.16f};
      particles_.push_back(p);
    }
  }
  duration_ = burn_;
  for (const Burst& b : bursts_) duration_ = std::max(duration_, b.delay + std::max(b.life, b.bedLife));
  for (const Particle& p : particles_) duration_ = std::max(duration_, p.at + p.life);
}

void Ignite::build(float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) const {
  (void)camera;
  const float ti = now - impact_;  // < 0 while the heat gathers
  if (ti < -kLead || done(now)) return;
  const uint32_t quad = frame.addVertices(quadGeometry());
  auto volume = [&](glm::vec3 at, glm::vec4 p0, float seed, glm::vec4 color, const glm::mat4& model, glm::vec3 sight) {
    VfxDraw d;
    d.gpu.model = model;
    d.gpu.params0 = p0;
    d.gpu.params1 = {at, seed};
    d.gpu.color = color;
    d.gpu.firstVertex = quad, d.gpu.vertexCount = 6, d.gpu.firstInstance = 0xFFFFFFFFu;
    d.gpu.material = static_cast<uint32_t>(VfxMaterial::FireVolume);
    d.gpu.sightAt = sight;
    d.blend = VfxBlend::Alpha;
    d.depthTest = true;
    frame.draws.push_back(d);
  };

  // The fire bursting up off each hex as it catches: a dome of fire on the ground, tearing apart as it ages (body 2).
  // Then the bed of fire left on the hex: the same dome held young (whole, boiling with its noise, never hollowed out), low on the
  // ground under the tongues, rising in as the burst fades and sinking away as the hex burns out.
  for (const Burst& b : bursts_) {
    const float a = ti - b.delay;
    if (a < 0.0f) continue;
    if (a <= b.life) {
      const float age = a / b.life;
      const float r = b.radius * (0.45f + 0.55f * (1.0f - std::pow(1.0f - age, 3.0f)));
      const float strength = 4.2f * std::pow(1.0f - age, 1.2f);
      volume(b.at, {now + b.seed, strength, 0.0f, r}, b.seed, {1.3f, 0.0f, 2.0f, age}, glm::mat4(0.0f), b.at + glm::vec3(0.0f, r * 0.4f, 0.0f));
    }
    if (a <= b.bedLife) {
      const float in = smooth(b.life * 0.3f, b.life * 0.8f, a), out = smooth(b.bedLife * 0.6f, b.bedLife, a);
      const float r = b.radius * 0.82f * (1.0f - 0.45f * out);
      const float strength = 2.4f * in * (1.0f - out);
      if (strength > 0.01f)
        volume(b.at, {now + b.seed * 1.37f, strength, 0.0f, r}, b.seed + 31.0f, {1.3f, 0.0f, 2.0f, 0.18f}, glm::mat4(0.0f),
               b.at + glm::vec3(0.0f, r * 0.3f, 0.0f));
    }
  }

  // The tongues of fire (the user, 2026-11-25: "as chamas não fazem sentido começando por cima ao invés de por baixo"): each stands
  // on its hex from its first frame, rises up out of the ground, dances (its height and width breathing), then sinks back into it.
  for (const Flame& f : flames_) {
    const float a = ti - f.delay;
    if (a < 0.0f || a > f.life) continue;
    const float grow = smooth(0.0f, 0.25f, a), fade = smooth(f.life * 0.6f, f.life, a);
    const float dance = 1.0f + 0.14f * std::sin(now * 8.3f + f.seed) * std::sin(now * 5.1f + f.seed * 1.7f) + 0.06f * std::sin(now * 17.0f + f.seed * 3.1f);
    const float height = f.height * dance * (0.08f + 0.92f * grow) * (1.0f - 0.8f * fade);
    const float radius = f.radius * (0.6f + 0.4f * grow) * (1.0f - 0.35f * fade) * (1.0f + 0.5f * (dance - 1.0f));
    const float strength = (f.strength + 1.0f * (1.0f - smooth(0.0f, 0.3f, a))) * (1.0f - fade);
    if (strength <= 0.01f) continue;
    volume(f.foot, {now + f.seed, strength, height, radius}, f.seed, {f.turbulence, 0.0f, 0.0f, 0.0f}, glm::mat4(1.0f),
           f.foot + glm::vec3(0.0f, height * 0.5f, 0.0f));
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
    float scale = p.size, alpha = 1.0f, emission = p.emission;
    glm::vec3 color = p.color;
    if (p.kind == 1) {
      scale *= 0.7f + (1.0f - tt) * 0.5f;
      alpha = std::pow(std::sin(kPi * tt), 0.7f);
      emission *= 1.0f - tt;
      color = glm::max(glm::vec3(0.0f), color - glm::vec3(0.0f, tt * 0.5f, tt * 0.45f));
    } else {
      scale *= 0.7f + tt * 1.4f;
      alpha = std::pow(std::sin(kPi * tt), 0.5f) * 0.32f * std::pow(1.0f - tt, 0.6f);
    }
    inst.a = {pos, scale};
    inst.b = {p.rotation + p.spin * age, 0.0f, alpha, emission};
    inst.c = {color, static_cast<float>(p.kind)};
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

  // The light: a red glow while the heat gathers, the flare as the air ignites, a flickering glow while the area burns.
  const float gather = ti < 0.0f ? smooth(-kLead, 0.0f, ti) * 5.0f : 0.0f;
  const float flare = ti >= 0.0f ? 34.0f * smooth(0.0f, 0.06f, ti) * (1.0f - smooth(0.12f, 0.6f, ti)) : 0.0f;
  const float burning = ti >= 0.0f ? 12.0f * smooth(0.0f, 0.2f, ti) * (1.0f - smooth(burn_ * 0.6f, burn_, ti)) : 0.0f;
  const float flicker = 0.85f + 0.15f * std::sin(now * 23.0f) * std::sin(now * 7.3f + 1.1f);
  const float intensity = (gather + flare + burning) * flicker;
  if (intensity > 0.01f) lights.push_back({centre_ + glm::vec3(0.0f, 1.0f, 0.0f), three::hex(0xff7a2a), intensity, reach_ + kHexEdge});
}

}  // namespace ember
