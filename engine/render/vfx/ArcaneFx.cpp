#include "render/vfx/ArcaneFx.h"

#include <algorithm>
#include <cmath>
#include <numbers>

#include <glm/gtc/matrix_transform.hpp>

#include "render/vfx/IceParts.h"
#include "render/vfx/SigilParts.h"

namespace ember {

using namespace icefx;  // ease, hash01, quadGeometry

namespace {

constexpr float kPi = std::numbers::pi_v<float>, kTau = 2.0f * kPi;
constexpr glm::vec3 kUp{0.0f, 1.0f, 0.0f};
constexpr glm::vec3 kViolet{0.55f, 0.22f, 1.0f}, kPink{1.0f, 0.62f, 0.95f}, kArcaneLight{0.75f, 0.35f, 1.0f};
// The Arcane material's looks (shaders/vfx.hlsl): the energy (a billboard), a thread (a card), a mote (a billboard).
constexpr float kEnergy = 0.0f, kThread = 1.0f, kSpeck = 2.0f;

float easeOut(float edge0, float edge1, float x) {
  const float t = std::clamp((x - edge0) / std::max(edge1 - edge0, 1e-5f), 0.0f, 1.0f);
  return 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t);
}

struct Painter {
  VfxFrame& frame;
  const Camera& camera;
  float time;
  glm::vec3 sight;
  uint32_t quad, ring, star;

  void arcane(float mode, uint32_t first, size_t count, float intensity, float opacity, float seed, const std::vector<VfxInstance>* list) const {
    if (opacity <= 0.003f) return;
    VfxDraw d;
    d.gpu.model = glm::mat4(1.0f);
    d.gpu.params0 = {time, mode, intensity, opacity};
    d.gpu.params1 = {seed, 0, 0, 0};
    d.gpu.color = {1, 1, 1, 1};
    d.gpu.firstVertex = first;
    d.gpu.vertexCount = static_cast<uint32_t>(count);
    d.gpu.firstInstance = 0xFFFFFFFFu;
    if (list) {
      if (list->empty()) return;
      d.gpu.firstInstance = static_cast<uint32_t>(frame.instances.size());
      frame.instances.insert(frame.instances.end(), list->begin(), list->end());
      d.instances = static_cast<uint32_t>(list->size());
    }
    d.gpu.material = static_cast<uint32_t>(VfxMaterial::Arcane);
    d.gpu.sightAt = sight;
    d.blend = VfxBlend::Alpha;
    d.depthTest = true;
    frame.draws.push_back(d);
  }
  // A thread of energy from a to b, `width` wide, facing the eye.
  void thread(glm::vec3 a, glm::vec3 b, float width, float opacity, float seed) const {
    const glm::vec3 along = b - a;
    glm::vec3 across = glm::cross(along, camera.position - (a + b) * 0.5f);
    if (glm::dot(across, across) < 1e-8f || opacity <= 0.003f) return;
    across = glm::normalize(across) * (width * 0.5f);
    const glm::vec3 n = glm::normalize(glm::cross(along, across));
    const VfxVertex v0{a - across, 0, n, 0}, v1{a + across, 1, n, 0}, v2{b + across, 1, n, 1}, v3{b - across, 0, n, 1};
    arcane(kThread, frame.addVertices({v0, v1, v2, v0, v2, v3}), 6, 1.0f, opacity, seed, nullptr);
  }
  // The sigil (the original spells' own: the ring with its marks, the hexagram turning against it) in the frame `basis` (its
  // plane is the basis's xz), `radius` wide.
  void sigil(const glm::mat4& basis, float radius, float turn, float intensity, float opacity, bool onGround) const {
    if (opacity <= 0.003f) return;
    const size_t from = frame.draws.size();
    // (each line three times, a hair apart: one thin line vanishes behind the energy; the mark is the arcane's signature)
    for (const float k : {0.985f, 1.0f, 1.015f}) {
      line(ring, sigil::ringGeometry().size(), basis * glm::rotate(glm::mat4(1.0f), turn, kUp) * glm::scale(glm::mat4(1.0f), glm::vec3(radius * k)), kViolet, intensity, opacity);
      line(star, sigil::starGeometry().size(), basis * glm::rotate(glm::mat4(1.0f), -turn * 2.2f, kUp) * glm::scale(glm::mat4(1.0f), glm::vec3(radius * k)), kPink, intensity, opacity);
    }
    if (onGround)
      for (size_t k = from; k < frame.draws.size(); ++k) frame.draws[k].underCards = true;
  }
  void line(uint32_t first, size_t count, const glm::mat4& model, glm::vec3 colour, float intensity, float opacity) const {
    VfxDraw d;
    d.gpu.model = model;
    d.gpu.params0 = {time, 4.0f, intensity, opacity};  // the Holy material's line look, in the arcane's colours
    d.gpu.params1 = {kPink, 0.0f};
    d.gpu.color = {colour, 1.0f};
    d.gpu.firstVertex = first;
    d.gpu.vertexCount = static_cast<uint32_t>(count);
    d.gpu.firstInstance = 0xFFFFFFFFu;
    d.gpu.material = static_cast<uint32_t>(VfxMaterial::Holy);
    d.gpu.sightAt = sight;
    d.blend = VfxBlend::Additive;
    d.depthTest = true;
    frame.draws.push_back(d);
  }
};

// A billboard: a = centre + size, b = turn, seed, alpha, intensity.
VfxInstance blob(glm::vec3 at, float size, float alpha, float seed, float intensity = 1.0f) {
  VfxInstance inst;
  inst.a = glm::vec4(at, size);
  inst.b = {0.0f, seed, alpha, intensity};
  return inst;
}
// A frame standing at `at` whose xz plane faces along `normal` (a sigil in the air in front of a hand).
glm::mat4 facing(glm::vec3 at, glm::vec3 normal) {
  const glm::vec3 x = glm::normalize(glm::cross(kUp, normal)), z = glm::cross(x, normal);
  glm::mat4 m(1.0f);
  m[0] = glm::vec4(x, 0), m[1] = glm::vec4(normal, 0), m[2] = glm::vec4(z, 0), m[3] = glm::vec4(at, 1);
  return m;
}

}  // namespace

float ArcaneSpell::lead(Style style, glm::vec3 casterFeet, glm::vec3 targetFeet) {
  if (style == Style::Phantasmal) return kPhantasm;
  return kGather + glm::length(targetFeet - casterFeet) / kMissileSpeed;
}

ArcaneSpell::ArcaneSpell(Style style, glm::vec3 casterFeet, float casterHeight, glm::vec3 targetFeet, float targetHeight, float start, float impact, uint32_t seed)
    : style_(style), feet_(targetFeet), height_(std::max(targetHeight, 1.2f)), impact_(std::max(impact, start + lead(style, casterFeet, targetFeet))), seed_(seed) {
  hand_ = casterFeet + kUp * (std::max(casterHeight, 1.2f) * 0.62f);
  chest_ = feet_ + kUp * (height_ * 0.55f);
  glm::vec3 d = chest_ - hand_;
  if (glm::dot(d, d) > 1e-6f) direction_ = glm::normalize(d);
  hand_ += direction_ * 0.45f;  // in front of the hand
  start_ = impact_ - lead(style, casterFeet, targetFeet);
  release_ = style == Style::Missile ? start_ + kGather : impact_;
  end_ = impact_ + (style == Style::Missile ? 1.6f : 1.7f);
}

void ArcaneSpell::aimFrom(int unit, glm::vec3 hand, float now) {
  if (unit < 0 || unit != unit_ || now >= release_) return;
  glm::vec3 d = chest_ - hand;
  if (glm::dot(d, d) > 1e-6f) direction_ = glm::normalize(d);
  hand_ = hand + direction_ * (0.12f * scale_);  // on the open palm, a little out from it
}

void ArcaneSpell::asAttack(float start, float release, float impact, float scale, int unit) {
  unit_ = unit;
  start_ = start, release_ = std::max(release, start + 0.05f), impact_ = std::max(impact, release_ + 0.05f), end_ = impact_ + 0.8f, scale_ = scale;
}

void ArcaneSpell::build(float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) const {
  if (now < start_ || done(now)) return;
  const Painter p{frame, camera, now - start_, feet_, frame.addVertices(quadGeometry()), frame.addVertices(sigil::ringGeometry()), frame.addVertices(sigil::starGeometry())};
  const float age = now - start_, since = now - impact_;
  const float s0 = hash01(seed_, 1) * 30.0f, S = scale_;
  const size_t firstDraw = frame.draws.size();
  std::vector<VfxInstance> energy, specks;
  // (threads snap in steps: their ends are chosen anew twelve times a second, never eased)
  const uint32_t tick = static_cast<uint32_t>(std::floor(now * 12.0f));

  if (style_ == Style::Missile) {
    const float gather = release_ - start_;
    if (now < release_) {
      // The sigil stands in front of the hand; light is drawn in through it in spirals; it stalls a beat before the release.
      const float pull = ease(0.0f, 0.75f, age / gather), stall = ease(0.75f, 1.0f, age / gather);
      p.sigil(facing(hand_, direction_), 1.4f * S * easeOut(0.0f, 0.4f, age / gather), age * 1.6f, 4.0f, pull, false);
      for (uint32_t i = 0; i < 26; ++i) {
        const float q = std::fmod(age / gather * (1.2f + 0.8f * hash01(seed_, i + 10)) + hash01(seed_, i + 40), 1.0f) * (1.0f - stall);
        const float a = hash01(seed_, i + 70) * kTau + q * 7.0f, r = 1.6f * S * (1.0f - q) * (1.0f - q);
        const glm::mat4 f = facing(hand_, direction_);
        specks.push_back(blob(glm::vec3(f * glm::vec4(std::cos(a) * r, -0.6f * S * (1.0f - q), std::sin(a) * r, 1.0f)), 0.12f * S, pull * ease(0.0f, 0.2f, q), hash01(seed_, i + 100)));
      }
      energy.push_back(blob(hand_, (0.7f + 1.5f * pull - 0.4f * stall) * S, pull, s0, 1.0f + 0.8f * stall));
      lights.push_back({hand_, kArcaneLight, (2.0f + 7.0f * pull) * S, 3.5f * S});
    } else if (now < impact_) {
      // The flight: the core on a tight spiral, wisps in orbit round it, threads snapping off behind it.
      const float flight = impact_ - release_, q = (now - release_) / flight;
      const glm::vec3 side = glm::normalize(glm::cross(direction_, kUp)), lift = glm::cross(side, direction_);
      auto path = [&](float t) { return glm::mix(hand_, chest_, t) + (side * std::cos(t * 15.0f + s0) + lift * std::sin(t * 15.0f + s0)) * (0.3f * S * std::sin(kPi * t)); };
      const glm::vec3 at = path(q);
      energy.push_back(blob(at, 2.4f * S, 1.0f, s0, 1.6f));
      for (uint32_t i = 0; i < 9; ++i) {
        const float a = age * 9.0f + static_cast<float>(i) * kTau / 9.0f, back = 0.15f * static_cast<float>(i);
        const glm::vec3 c = path(std::max(q - back * S / std::max(glm::length(chest_ - hand_), 1.0f), 0.0f));
        specks.push_back(blob(c + (side * std::cos(a) + lift * std::sin(a)) * (0.45f * S), (0.16f - 0.01f * static_cast<float>(i)) * S, 1.0f - back / 1.5f, hash01(seed_, i + 130)));
      }
      for (uint32_t i = 0; i < 3; ++i) {
        const float back = (0.4f + 1.4f * hash01(tick, i + 160)) * S;
        const glm::vec3 to = at - direction_ * back + (side * (hash01(tick, i + 190) - 0.5f) + lift * (hash01(tick, i + 220) - 0.5f)) * (1.4f * S);
        p.thread(at, to, 0.6f * S, 0.9f, hash01(tick, i + 250) * 9.0f);
      }
      lights.push_back({at, kArcaneLight, 9.0f * S, 4.5f * S});
    }
    // the sigil it left stays a moment and fades
    if (now >= release_ && now < release_ + 0.3f) p.sigil(facing(hand_, direction_), (1.4f + 0.8f * (now - release_) / 0.3f) * S, age * 1.6f, 4.0f, 1.0f - (now - release_) / 0.3f, false);
  } else if (since < 0.0f) {
    // The Phantasmal: a sigil under the foe and a second over its head, turning against each other; the energy gathers out of
    // nothing round the body between them; threads snap from the rings into it.
    const float g = ease(0.0f, 1.0f, age / kPhantasm);
    p.sigil(glm::translate(glm::mat4(1.0f), feet_ + kUp * 0.05f), 3.0f * easeOut(0.0f, 0.5f, age / kPhantasm), age * 1.2f, 4.0f, g, true);
    p.sigil(glm::translate(glm::mat4(1.0f), feet_ + kUp * (height_ + 1.2f)), 2.3f * easeOut(0.0f, 0.5f, age / kPhantasm), -age * 1.5f, 4.0f, g, false);
    energy.push_back(blob(chest_, 1.4f + 5.2f * g, 0.85f * g, s0, 0.8f + 0.7f * g));
    for (uint32_t i = 0; i < 8; ++i) {
      const float a = hash01(tick, i + 10) * kTau, top = i % 2 ? height_ + 1.2f : 0.05f, r = i % 2 ? 2.3f : 3.0f;
      p.thread(feet_ + glm::vec3(std::cos(a) * r, top, std::sin(a) * r), chest_ + glm::vec3(0, (hash01(tick, i + 40) - 0.5f) * height_ * 0.6f, 0), 0.55f, g, hash01(tick, i + 70) * 9.0f);
    }
    for (uint32_t i = 0; i < 22; ++i) {  // wisps in orbit, closing
      const float a = hash01(seed_, i + 10) * kTau + age * (3.0f + 2.0f * hash01(seed_, i + 40)), r = 1.6f - 0.9f * g;
      specks.push_back(blob(feet_ + glm::vec3(std::cos(a) * r, height_ * hash01(seed_, i + 70) + 0.2f, std::sin(a) * r), 0.13f, g, hash01(seed_, i + 100)));
    }
    lights.push_back({chest_, kArcaneLight, 8.0f * g, 4.5f});
  }

  // The hit (both): the energy folds in or bursts, a flash, a sharp ring racing over the ground, the runes scattering.
  if (since >= 0.0f) {
    const float life = end_ - impact_, k = since / life;
    const bool fold = style_ == Style::Phantasmal;
    energy.push_back(blob(chest_, fold ? 6.6f * (1.0f - easeOut(0.0f, 0.3f, since)) + 0.4f : (2.0f + 4.5f * easeOut(0.0f, 0.3f, since)) * S, std::pow(1.0f - k, 1.5f), s0,
                          2.2f * (1.0f - ease(0.0f, 0.4f, since)) + 0.4f));
    const glm::mat4 ground = glm::translate(glm::mat4(1.0f), feet_ + kUp * 0.05f);
    p.sigil(ground, ((fold ? 3.0f : 0.8f) + 2.6f * easeOut(0.0f, 0.35f, since)) * S, age * 1.2f, 5.0f, 1.0f - ease(0.5f, 1.4f, since), true);
    for (uint32_t i = 0; i < 30; ++i) {  // the runes scattering: flung out low, each stopping dead
      const float a = hash01(seed_, i + 300) * kTau, far = (1.0f + 2.4f * hash01(seed_, i + 330)) * S;
      specks.push_back(blob(feet_ + glm::vec3(std::cos(a), 0.0f, std::sin(a)) * (far * easeOut(0.0f, 0.3f, since)) + kUp * ((0.1f + 0.5f * hash01(seed_, i + 360)) * S), 0.14f * S, 1.0f - k,
                            hash01(seed_, i + 390)));
    }
    if (since < 0.35f)
      for (uint32_t i = 0; i < 4; ++i) {
        const float a = hash01(tick, i + 420) * kTau;
        p.thread(chest_, chest_ + glm::vec3(std::cos(a) * 2.4f * S, (hash01(tick, i + 450) - 0.3f) * 1.8f * S, std::sin(a) * 2.4f * S), 0.55f * S, 1.0f - since / 0.35f, hash01(tick, i + 480) * 9.0f);
      }
    lights.push_back({chest_, kArcaneLight, (22.0f * std::exp(-since * 8.0f) + 2.0f * (1.0f - k)) * S, 6.0f * S});
  }

  p.arcane(kEnergy, p.quad, 6, 1.0f, 1.0f, s0, &energy);
  p.arcane(kSpeck, p.quad, 6, 1.0f, 1.0f, s0, &specks);
  // the marks in the air go over the energy (the geometry is read through the light, not hidden by it)
  std::stable_partition(frame.draws.begin() + static_cast<std::ptrdiff_t>(firstDraw), frame.draws.end(),
                        [](const VfxDraw& d) { return d.underCards || d.gpu.material != static_cast<uint32_t>(VfxMaterial::Holy); });
}

}  // namespace ember
