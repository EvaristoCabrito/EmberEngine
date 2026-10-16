#include "render/vfx/BurningHands.h"

#include <algorithm>
#include <cmath>

#include <glm/gtc/matrix_transform.hpp>

#include "render/vfx/ThreeMath.h"

namespace ember {

namespace {

// DEFAULT_BURNING_HANDS_V2_SETTINGS
constexpr float kTongueCount = 10, kFlameLength = 1, kFlameWidth = 1, kTurbulence = 0.2f, kLightIntensity = 18, kLightRadius = 5.2f;
constexpr int kLights = 5;

}  // namespace

BurningHands::BurningHands(glm::vec3 origin, const std::vector<glm::vec3>& cells, bool poison, float start)
    : origin_(vfxToEmber(origin)), start_(start), poison_(poison) {
  // BattleSpells3D: the cells' average sets the direction and length; the widest cell sideways sets the width.
  glm::vec3 average{0.0f};
  for (const glm::vec3& c : cells) average += vfxToEmber(c);
  if (!cells.empty()) average /= static_cast<float>(cells.size());
  glm::vec2 dir{average.x - origin_.x, average.y - origin_.y};
  dir = glm::dot(dir, dir) > 0.0f ? glm::normalize(dir) : dir;
  const float length = std::max(1.0f, std::hypot(average.x - origin_.x, average.y - origin_.y));
  float halfWidth = 0.38f;
  for (const glm::vec3& w : cells) {
    const glm::vec3 c = vfxToEmber(w);
    halfWidth = std::max(halfWidth, std::abs(-dir.y * (c.x - origin_.x) + dir.x * (c.y - origin_.y)));
  }
  // The effect's own constructor.
  direction_ = glm::dot(dir, dir) < 0.001f ? glm::vec2(0, 1) : dir;
  sideways_ = {-direction_.y, direction_.x};
  length_ = std::max(1.0f, length) * kFlameLength;
  width_ = std::max(0.7f, halfWidth * 2.0f) * kFlameWidth;
  // emitterSettings()
  const FireEmitterSettings base;
  settings_.particleCount = std::max(48, std::min(ParticleEmitter::kMaxParticles, static_cast<int>(std::lround(kTongueCount * 8))));
  settings_.particleScale = base.particleScale * 1.0f * 1.5f * kFlameLength;
  settings_.velocity = base.velocity * 1.25f;
  settings_.turbulence = kTurbulence * 1.25f;
  settings_.coreIntensity = base.coreIntensity * 1.5f;
  settings_.lightIntensity = 0.0f;
  settings_.lifetimeScale = 1.7f;
  settings_.spread = 0.75f * kFlameWidth;
  settings_.flow = true;
  settings_.flowDirection = {direction_.x, direction_.y, 0.0f};
  settings_.flowSpeed = 3.5f;
  settings_.flowSpread = std::min(1.0f, std::atan2(width_ * 0.5f, length_) * 1.5f);
  // seedFor("spray", 0), with Engine2's cast id "burning-hands-v2-1" (18 characters).
  const uint32_t baseSeed = static_cast<uint32_t>((static_cast<uint64_t>(kTongueCount) * 2654435761ull + 18ull * 2246822519ull) & 0xFFFFFFFFull);
  const uint32_t seed = baseSeed ^ 0x85ebca6bu ^ (1u * 0x27d4eb2fu);
  emitter_.setSeed(seed ? seed : 1u);
  if (poison_) {
    emitter_.curves.color = [](float t) { return glm::vec3(0.28f + t * 0.12f, 0.7f - t * 0.2f, 0.12f); };
    emitter_.curves.emissive = [](float) { return 0.35f; };
  }
}

void BurningHands::advance(float t) {
  if (!started_) {
    // The first update: the emitter reset (staggered), then a zero step.
    started_ = true;
    emitter_.reset(settings_, true);
    emitter_.update(0.0f, settings_, elapsed_ < kSpray);
  }
  const float target = std::min(kFinish, t);
  while (elapsed_ + ParticleEmitter::kStep <= target + 1e-6f) {
    elapsed_ = std::min(kFinish, elapsed_ + ParticleEmitter::kStep);
    emitter_.update(ParticleEmitter::kStep, settings_, elapsed_ < kSpray);
  }
}

void BurningHands::build(float now, const Camera&, const VfxFlipbook& sheet, VfxFrame& frame, std::vector<VfxLight>& lights) {
  const float t = now - start_;
  if (t < 0.0f || done(now)) return;
  advance(t);
  const glm::mat4 group = vfxEmberToWorld() * glm::translate(glm::mat4(1.0f), origin_);
  const glm::vec3 sight = vfxFromEmber(origin_);
  // The emitter's mesh: 0.2 above the hands, scale 1; NormalBlending, no depth test (Engine2's particle material).
  drawParticleEmitter(emitter_, settings_, group * glm::translate(glm::mat4(1.0f), {0.0f, 0.0f, 0.2f}), sheet, settings_.coreIntensity, sight,
                      VfxBlend::Alpha, false, frame);

  // updateWorldLights: two at the hands, three along the advancing front, flickering, fading after the spray.
  const float fadeOut = 1.0f - three::smoothstep(t, kSpray, kFinish);
  const float front = length_ * three::smoothstep(t, 0.0f, kSpray);
  const float flicker = 0.92f + std::sin(t * 21.0f) * 0.045f + std::sin(t * 37.0f + 1.2f) * 0.035f;
  static const float kWeights[kLights] = {0.72f, 0.8f, 0.88f, 0.8f, 0.72f};
  static const float kRatios[kLights] = {0.12f, 0.32f, 0.54f, 0.76f, 0.96f};
  if (fadeOut <= 0.01f) return;
  for (int i = 0; i < kLights; ++i) {
    const float forward = front * kRatios[i];
    const float sideways = (static_cast<float>(i) / (kLights - 1) - 0.5f) * width_ * kRatios[i];
    const glm::vec3 at = origin_ + glm::vec3(direction_.x * forward + sideways_.x * sideways, direction_.y * forward + sideways_.y * sideways, 0.22f);
    const float heat = 0.2f - std::abs(i - 2) * 0.045f;
    const glm::vec3 colour = poison_ ? glm::vec3(0.32f, 0.8f, 0.12f) : glm::vec3(1.0f, 0.48f + heat, 0.16f + heat * 0.35f);
    lights.push_back({vfxFromEmber(at), colour, kLightIntensity * kWeights[i] * fadeOut * flicker, kLightRadius});
  }
}

}  // namespace ember
