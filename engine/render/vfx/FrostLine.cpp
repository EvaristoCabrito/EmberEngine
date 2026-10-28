#include "render/vfx/FrostLine.h"

#include <algorithm>
#include <cmath>
#include <numbers>

#include <glm/gtc/matrix_transform.hpp>

#include "render/vfx/IceParts.h"

namespace ember {

using namespace icefx;

namespace {

constexpr float kTau = 2.0f * std::numbers::pi_v<float>;
constexpr glm::vec3 kUp{0.0f, 1.0f, 0.0f};
constexpr glm::vec3 kFrostLight{0.71f, 0.91f, 1.0f};  // Ember's frost colour for the light (battleSpellOverlay3d)
// The IceStorm material's modes used here (shaders/vfx.hlsl).
constexpr float kFloor = 0.0f, kSnow = 1.0f, kCrystal = 3.0f, kDust = 6.0f, kRing = 7.0f;
constexpr int kSpikes = 6;  // crystals in a hex's cluster
constexpr float kShatterBurst = FrostLine::kShatterAt + 0.5f * (FrostLine::kLife - FrostLine::kShatterAt);  // the cluster bursts as it goes

float easeOut(float edge0, float edge1, float x) {
  const float t = std::clamp((x - edge0) / std::max(edge1 - edge0, 1e-5f), 0.0f, 1.0f);
  return 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t);
}
// Rises past its height and settles (a spike thrust up out of the ground).
float easeBack(float t) {
  t = std::clamp(t, 0.0f, 1.0f);
  const float c1 = 1.9f, c3 = c1 + 1.0f, u = t - 1.0f;
  return 1.0f + c3 * u * u * u + c1 * u * u;
}

// One batch of the IceStorm material: `list` instances of geometry `first`, or one object at `model`.
void draw(VfxFrame& frame, float time, float mode, uint32_t first, size_t count, const glm::mat4& model, float opacity, glm::vec4 params1,
          glm::vec3 sight, const std::vector<VfxInstance>* list) {
  VfxDraw d;
  d.gpu.model = model;
  d.gpu.params0 = {time, mode, 1.0f, opacity};
  d.gpu.params1 = params1;
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
  d.gpu.material = static_cast<uint32_t>(VfxMaterial::IceStorm);
  d.gpu.sightAt = sight;
  d.blend = VfxBlend::Alpha;
  d.depthTest = true;
  frame.draws.push_back(d);
}

// A crystal standing on `base`, its long point toward `up` (unit), `length` long and `radius` wide, turned about its axis.
VfxInstance crystal(glm::vec3 base, glm::vec3 up, float length, float radius, float spin, float alpha, float seed) {
  VfxInstance inst;
  // The mesh's long point is its -y end: the frame's +y runs down the crystal, so the point rises.
  inst.matrix = glm::translate(glm::mat4(1.0f), base + up * (length * 0.42f)) * alongAxis(-up, spin) * glm::scale(glm::mat4(1.0f), {radius, length, radius});
  inst.c = {alpha, seed, 0, 0};
  return inst;
}
VfxInstance billboard(glm::vec3 at, float angle, float alpha, glm::vec2 size, float seed) {
  VfxInstance inst;
  inst.a = glm::vec4(at, 0.0f);
  inst.b = {angle, 0.0f, alpha, 0.0f};
  inst.c = {size.x, size.y, 0.0f, seed};
  return inst;
}
VfxInstance ring(glm::vec3 at, float radius, float alpha, float seed) {
  VfxInstance inst;
  inst.matrix = glm::translate(glm::mat4(1.0f), at) * glm::scale(glm::mat4(1.0f), glm::vec3(radius));
  inst.c = {alpha, seed, 0, 0};
  return inst;
}

}  // namespace

FrostLine::FrostLine(glm::vec3 casterFeet, float casterHeight, std::vector<glm::vec3> hexes, float start, float impact, uint32_t seed)
    : feet_(casterFeet),
      hand_(casterFeet + glm::vec3(0.0f, casterHeight * 0.62f, 0.0f)),
      start_(start),
      impact_(std::max(impact, start + 0.3f)),
      hexes_(std::move(hexes)),
      seed_(seed) {
  std::stable_sort(hexes_.begin(), hexes_.end(), [&](const glm::vec3& a, const glm::vec3& b) {
    return glm::length(glm::vec2(a.x - feet_.x, a.z - feet_.z)) < glm::length(glm::vec2(b.x - feet_.x, b.z - feet_.z));
  });
  if (!hexes_.empty()) {
    glm::vec2 d{hexes_.back().x - feet_.x, hexes_.back().z - feet_.z};
    if (glm::dot(d, d) > 1e-6f) {
      d = glm::normalize(d);
      direction_ = {d.x, 0.0f, d.y};
    }
  }
}

float FrostLine::end() const {
  return impact_ + kStagger * static_cast<float>(hexes_.empty() ? 0 : hexes_.size() - 1) + kLife + 0.1f;
}

void FrostLine::build(float now, VfxFrame& frame, std::vector<VfxLight>& lights) const {
  if (now < start_ || done(now)) return;
  buildCaster(now, frame, lights);
  for (size_t i = 0; i < hexes_.size(); ++i) {
    const float age = now - (impact_ + kStagger * static_cast<float>(i));
    if (age >= -0.02f && age < kLife + 0.05f) buildHex(i, age, frame, lights);
  }
}

// --- the caster: cold gathers while the cast winds up, the shards let go at the hit.
void FrostLine::buildCaster(float now, VfxFrame& frame, std::vector<VfxLight>& lights) const {
  const float gather = impact_ - start_;
  const float age = now - start_, since = now - impact_;
  if (since > 0.6f) return;
  const float rise = ease(0.0f, 0.5f, age / gather);                      // 0 -> 1 over the cast
  const float release = 1.0f - ease(0.0f, 0.35f, std::max(since, 0.0f));  // gone just after the hit
  const float env = rise * release;
  const uint32_t quadFirst = frame.addVertices(quadGeometry()), hexFirst = frame.addVertices(hexGeometry());
  const uint32_t discFirst = frame.addVertices(discGeometry()), crystalFirst = frame.addVertices(crystalGeometry());

  // The ground under the caster frosts; two rings contract onto the feet, one a half-cycle behind the other.
  const float cover = 0.1f + 1.0f * ease(0.0f, 1.0f, std::min(age / gather, 1.0f));
  draw(frame, age, kFloor, hexFirst, hexGeometry().size(), glm::translate(glm::mat4(1.0f), feet_ + glm::vec3(0, 0.04f, 0)), env * 0.8f, {cover, 0, 0, 0}, feet_, nullptr);
  std::vector<VfxInstance> rings;
  for (int k = 0; k < 2; ++k) {
    const float p = std::fmod(age / std::max(gather * 0.75f, 0.3f) + 0.5f * static_cast<float>(k), 1.0f);
    rings.push_back(ring(feet_ + glm::vec3(0, 0.05f, 0), 1.9f - 1.35f * p, env * 0.9f * ease(0.0f, 0.2f, p) * (1.0f - ease(0.8f, 1.0f, p)), static_cast<float>(k) * 0.37f));
  }
  if (since > 0.0f) rings.push_back(ring(feet_ + glm::vec3(0, 0.05f, 0), 0.6f + 1.4f * easeOut(0.0f, 0.5f, since), 0.9f * (1.0f - ease(0.0f, 0.5f, since)), 0.8f));
  draw(frame, age, kRing, discFirst, discGeometry().size(), glm::mat4(1.0f), 1.0f, {}, feet_, &rings);

  // Snow and ice dust spiral in from a wide circle to the hand.
  std::vector<VfxInstance> flakes, dust;
  for (uint32_t i = 0; i < 28; ++i) {
    const float speed = 0.7f + 0.7f * hash01(seed_, i), phase = hash01(seed_, i + 40);
    const float p = std::fmod(age * speed / std::max(gather, 0.4f) + phase, 1.0f);  // 0 outside .. 1 at the hand
    const float turn = hash01(seed_, i + 80) * kTau + age * (1.5f + 2.5f * p) * (i % 2 ? 1.0f : -1.0f);
    const float reach = (1.0f - p) * (1.0f - p) * (1.5f + 0.8f * hash01(seed_, i + 120));
    const float lift = (hash01(seed_, i + 160) - 0.3f) * (1.0f - p) * 1.2f;
    const glm::vec3 at = hand_ + glm::vec3(std::cos(turn) * reach, lift, std::sin(turn) * reach);
    const float alpha = env * ease(0.0f, 0.15f, p) * (1.0f - ease(0.85f, 1.0f, p));
    if (i % 3 == 0) {
      dust.push_back(billboard(at, turn, alpha * 0.4f, glm::vec2(0.18f + 0.14f * hash01(seed_, i + 200)), hash01(seed_, i + 240) * 13.0f));
    } else {
      const float size = 0.035f + 0.04f * hash01(seed_, i + 280);
      flakes.push_back(billboard(at, 0.0f, alpha * 0.9f, glm::vec2(size), hash01(seed_, i + 320)));
    }
  }
  draw(frame, age, kDust, quadFirst, quadGeometry().size(), glm::mat4(1.0f), 1.0f, {}, feet_, &dust);
  draw(frame, age, kSnow, quadFirst, quadGeometry().size(), glm::mat4(1.0f), 1.0f, {}, feet_, &flakes);

  // Six shards orbit the hand, rising with the cast; at the hit they are thrown down the line.
  std::vector<VfxInstance> shards;
  const glm::vec3 side = glm::normalize(glm::cross(direction_, kUp));
  for (uint32_t i = 0; i < 6; ++i) {
    const float a = static_cast<float>(i) * kTau / 6.0f + age * 2.6f;
    const float orbit = (0.42f - 0.12f * ease(0.7f, 1.0f, age / gather)) * rise;
    glm::vec3 at = hand_ + side * std::cos(a) * orbit + kUp * (std::sin(a) * 0.5f * orbit) + direction_ * (std::sin(a) * orbit * 0.7f);
    glm::vec3 axis = glm::normalize(side * -std::sin(a) + direction_ * std::cos(a) * 0.4f + kUp * 0.3f);  // the point leads round the orbit
    float alpha = env;
    if (since > 0.0f) {
      at += direction_ * (3.5f * since + 7.0f * since * since) + kUp * (0.2f * std::sin(since * 9.0f + a));
      axis = glm::normalize(direction_ + kUp * 0.05f);
      alpha = 0.95f * (1.0f - ease(0.15f, 0.55f, since));
    }
    if (alpha < 0.01f) continue;
    VfxInstance inst;
    inst.matrix = glm::translate(glm::mat4(1.0f), at) * alongAxis(-axis, a * 2.0f) * glm::scale(glm::mat4(1.0f), {0.035f, 0.26f, 0.035f});
    inst.c = {alpha, hash01(seed_, i + 360), 0, 0};
    shards.push_back(inst);
  }
  draw(frame, age, kCrystal, crystalFirst, crystalGeometry().size(), glm::mat4(1.0f), 1.0f, {}, feet_, &shards);

  lights.push_back({hand_ + glm::vec3(0, 0.1f, 0), kFrostLight, (0.8f + 5.0f * rise * rise) * release, 2.8f});
}

// --- one hex: the ice rises, holds, cracks and shatters. age = seconds since its eruption started.
void FrostLine::buildHex(size_t index, float age, VfxFrame& frame, std::vector<VfxLight>& lights) const {
  const glm::vec3 hex = hexes_[index];
  const uint32_t salt = seed_ + static_cast<uint32_t>(index) * 977u;
  const uint32_t quadFirst = frame.addVertices(quadGeometry()), hexFirst = frame.addVertices(hexGeometry());
  const uint32_t discFirst = frame.addVertices(discGeometry()), crystalFirst = frame.addVertices(crystalGeometry());
  const float t = std::max(age, 0.0f);
  const float shatter = std::clamp((t - kShatterAt) / (kLife - kShatterAt), 0.0f, 1.0f);  // 0 holding .. 1 gone
  const float fade = 1.0f - ease(0.5f, 1.0f, shatter);

  // The floor frosts over from the middle and goes with the ice.
  draw(frame, t, kFloor, hexFirst, hexGeometry().size(), glm::translate(glm::mat4(1.0f), hex + glm::vec3(0, 0.035f, 0)),
       ease(0.0f, 0.2f, t) * (1.0f - ease(0.6f, 1.0f, shatter)), {0.15f + 1.3f * easeOut(0.0f, 0.45f, t), 0, 0, 0}, hex, nullptr);

  // The cluster: a tall spike and satellites leaning out. They crack (jitter) before they shatter and sink as they melt.
  std::vector<VfxInstance> crystals;
  const float crack = ease(0.0f, 0.3f, shatter) * (1.0f - ease(0.3f, 0.5f, shatter));
  for (int k = 0; k < kSpikes; ++k) {
    const uint32_t i = static_cast<uint32_t>(k);
    const bool tall = k == 0;
    const float a = hash01(salt, i) * kTau, out = tall ? 0.0f : 0.28f + 0.42f * hash01(salt, i + 10);
    const float lean = tall ? 0.08f : 0.28f + 0.3f * hash01(salt, i + 20);
    const float length = tall ? 2.1f + 0.5f * hash01(salt, i + 30) : 0.8f + 0.7f * hash01(salt, i + 30);
    const float radius = tall ? 0.2f : 0.1f + 0.05f * hash01(salt, i + 40);
    // Each satellite starts a little after the tall one.
    const float delay = tall ? 0.0f : 0.03f + 0.06f * hash01(salt, i + 50);
    const float grow = easeBack((t - delay) / kGrow);
    if (grow <= 0.0f) continue;
    const glm::vec3 dir = glm::normalize(glm::vec3(std::cos(a) * std::sin(lean), std::cos(lean), std::sin(a) * std::sin(lean)));
    glm::vec3 base = hex + glm::vec3(std::cos(a) * out, 0.0f, std::sin(a) * out);
    base.y -= 0.12f * ease(0.5f, 1.0f, shatter) * length;
    const float jitter = crack * 0.02f * std::sin(t * 90.0f + static_cast<float>(k) * 2.0f);
    base.x += jitter, base.z -= jitter;
    const float melt = 1.0f - 0.45f * ease(0.3f, 1.0f, shatter);
    crystals.push_back(crystal(base, dir, length * grow * melt, radius * (0.55f + 0.45f * std::min(grow, 1.0f)) * melt, hash01(salt, i + 60) * kTau,
                               0.95f * fade, hash01(salt, i + 70)));
  }
  // Ice chips at the eruption and splinters at the shatter, thrown out and falling.
  auto flung = [&](float when, uint32_t count, uint32_t channel, float height, float lift0) {
    const float s = t - when;
    if (s < 0.0f || s > 0.65f) return;
    for (uint32_t j = 0; j < count; ++j) {
      const float a = hash01(salt, channel + j) * kTau, outv = 0.9f + 1.0f * hash01(salt, channel + j + 20);
      const float lift = lift0 + 1.1f * hash01(salt, channel + j + 40);
      const float y = height + lift * s - 4.9f * s * s;
      if (y < 0.0f) continue;
      const glm::vec3 dir{std::cos(a), 0.0f, std::sin(a)};
      const glm::vec3 vel = dir * outv + glm::vec3(0, lift - 9.8f * s, 0);
      VfxInstance inst;
      inst.matrix = glm::translate(glm::mat4(1.0f), hex + dir * (0.2f + outv * s) + glm::vec3(0, y, 0)) * alongAxis(-glm::normalize(vel), a * 3.0f + s * 9.0f) *
                    glm::scale(glm::mat4(1.0f), {0.02f, 0.09f + 0.08f * hash01(salt, channel + j + 60), 0.02f});
      inst.c = {0.9f * (1.0f - s / 0.65f), 0, 0, 0};
      crystals.push_back(inst);
    }
  };
  flung(0.0f, 6, 100, 0.05f, 1.4f);
  flung(kShatterBurst, 10, 200, 0.9f, 0.9f);
  draw(frame, t, kCrystal, crystalFirst, crystalGeometry().size(), glm::mat4(1.0f), 1.0f, {}, hex, &crystals);

  // Frost rings over the ground: the first fast, the second slower and wider; one more as the cluster shatters.
  std::vector<VfxInstance> rings;
  rings.push_back(ring(hex + glm::vec3(0, 0.05f, 0), 0.15f + 1.0f * easeOut(0.0f, 0.5f, t), 0.9f * (1.0f - ease(0.15f, 0.55f, t)), hash01(salt, 300)));
  if (t > 0.08f) rings.push_back(ring(hex + glm::vec3(0, 0.05f, 0), 0.2f + 1.5f * easeOut(0.08f, 0.9f, t), 0.6f * (1.0f - ease(0.2f, 0.9f, t)), hash01(salt, 301)));
  if (t > kShatterBurst)
    rings.push_back(ring(hex + glm::vec3(0, 0.05f, 0), 0.3f + 0.9f * easeOut(kShatterBurst, kShatterBurst + 0.45f, t),
                         0.7f * (1.0f - ease(kShatterBurst, kShatterBurst + 0.45f, t)), hash01(salt, 302)));
  draw(frame, t, kRing, discFirst, discGeometry().size(), glm::mat4(1.0f), 1.0f, {}, hex, &rings);

  // Ice dust at the eruption, a low mist hanging over the hex while it holds, dust again at the shatter.
  std::vector<VfxInstance> dust, flakes;
  if (t < 0.7f) {
    const float k = t / 0.7f;
    dust.push_back(billboard(hex + glm::vec3(0, 0.15f + 0.4f * k, 0), hash01(salt, 310) * kTau, 0.55f * std::pow(1.0f - k, 1.4f), glm::vec2(0.5f + 0.9f * k),
                             hash01(salt, 311) * 13.0f));
  }
  if (t > 0.1f)
    for (uint32_t j = 0; j < 3; ++j) {
      const float a = hash01(salt, 320 + j) * kTau + t * (0.4f + 0.2f * static_cast<float>(j));
      const float r = 0.2f + 0.5f * hash01(salt, 330 + j);
      dust.push_back(billboard(hex + glm::vec3(std::cos(a) * r, 0.12f + 0.15f * std::sin(t * 1.6f + static_cast<float>(j)), std::sin(a) * r), a,
                               0.2f * ease(0.1f, 0.5f, t) * (1.0f - ease(0.4f, 1.0f, shatter)), glm::vec2(0.7f + 0.3f * hash01(salt, 340 + j)),
                               hash01(salt, 350 + j) * 13.0f));
    }
  if (t > kShatterBurst && t < kShatterBurst + 0.5f) {
    const float k = (t - kShatterBurst) / 0.5f;
    dust.push_back(billboard(hex + glm::vec3(0, 0.6f + 0.5f * k, 0), 0.0f, 0.5f * std::pow(1.0f - k, 1.4f), glm::vec2(0.9f + 0.9f * k), hash01(salt, 312) * 13.0f));
  }
  // Snow glitters in the cold air above the cluster while it holds.
  if (t > 0.15f)
    for (uint32_t j = 0; j < 10; ++j) {
      const float p = std::fmod(t * (0.5f + 0.5f * hash01(salt, 360 + j)) + hash01(salt, 370 + j), 1.0f);
      const float a = hash01(salt, 380 + j) * kTau, r = 0.9f * hash01(salt, 390 + j);
      const float size = 0.03f + 0.03f * hash01(salt, 400 + j);
      flakes.push_back(billboard(hex + glm::vec3(std::cos(a) * r, 1.8f * (1.0f - p), std::sin(a) * r), 0.0f,
                                 0.8f * ease(0.15f, 0.4f, t) * ease(0.0f, 0.1f, p) * (1.0f - ease(0.4f, 1.0f, shatter)), glm::vec2(size), hash01(salt, 410 + j)));
    }
  draw(frame, t, kDust, quadFirst, quadGeometry().size(), glm::mat4(1.0f), 1.0f, {}, hex, &dust);
  draw(frame, t, kSnow, quadFirst, quadGeometry().size(), glm::mat4(1.0f), 1.0f, {}, hex, &flakes);

  // The cold light: a strong flash at the eruption (Ember FROST_FLASH) settling into a glow that goes as the ice does.
  const float flash = 9.0f * std::exp(-t * 7.0f), glow = 1.6f * ease(0.0f, 0.3f, t) * (1.0f - ease(0.4f, 1.0f, shatter));
  const float burst = t > kShatterBurst ? 3.0f * std::exp(-(t - kShatterBurst) * 8.0f) : 0.0f;
  lights.push_back({hex + glm::vec3(0, 0.9f, 0), kFrostLight, flash + glow + burst, 3.2f});
}

}  // namespace ember
