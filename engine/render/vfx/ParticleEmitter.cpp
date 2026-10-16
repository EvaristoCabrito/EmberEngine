#include "render/vfx/ParticleEmitter.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace ember {

float ParticleEmitter::random() {
  state_ ^= state_ << 13;
  state_ ^= state_ >> 17;
  state_ ^= state_ << 5;
  return static_cast<float>(static_cast<double>(state_) / 4294967296.0);
}

void ParticleEmitter::spawn(Particle& p, const FireEmitterSettings& s, float initialAge) {
  const float spread = s.spread;
  p.life = (0.62f + random() * 0.5f) * s.lifetimeScale;
  p.age = initialAge * p.life;
  p.phase = random() * 16.0f;
  p.size = 0.35f + random() * 0.33f;  // unitless: the emitter's scale applies at upload
  p.rotation = random() * std::numbers::pi_v<float> * 2.0f;
  p.spin = (random() - 0.5f) * 1.4f;
  const float px = (random() - 0.5f) * 0.28f * spread;
  const float py = 0.12f + random() * 0.22f;
  const float pz = (random() - 0.5f) * 0.28f * spread;
  p.position = {px, py, pz};
  const float angle = random() * std::numbers::pi_v<float> * 2.0f;
  const float radial = random() * 0.16f * spread;
  p.riseSpeed = s.velocity * (0.7f + random() * 0.55f);
  const float flowAngle = s.flow ? std::atan2(s.flowDirection.y, s.flowDirection.x) + (random() - 0.5f) * s.flowSpread : 0.0f;
  const float flowSpeed = s.flow ? s.flowSpeed : 0.0f;
  p.velocity = {std::cos(angle) * radial + (s.flow ? std::cos(flowAngle) * flowSpeed : 0.0f), p.riseSpeed + (s.flow ? std::sin(flowAngle) * flowSpeed : 0.0f),
                std::sin(angle) * radial + (s.flow ? s.flowDirection.z : 0.0f) * flowSpeed};
  p.position += p.velocity * p.age;
}

void ParticleEmitter::reset(const FireEmitterSettings& s, bool stagger) {
  clock_ = 0.0f;
  const int count = std::min(s.particleCount, kMaxParticles);
  for (int i = 0; i < count; ++i) {
    const float initial = stagger ? random() : 0.0f;
    spawn(particles_[static_cast<size_t>(i)], s, initial);
  }
  for (int i = count; i < kMaxParticles; ++i) particles_[static_cast<size_t>(i)].age = 99.0f;
}

void ParticleEmitter::update(float dt, const FireEmitterSettings& s, bool looping) {
  clock_ += dt;
  const int count = std::min(s.particleCount, kMaxParticles);
  for (int i = 0; i < count; ++i) {
    Particle& p = particles_[static_cast<size_t>(i)];
    p.age += dt;
    if (p.age >= p.life) {
      if (looping) spawn(p, s, 0.0f);
      else p.age = p.life, p.velocity = glm::vec3(0.0f);
    }
    const float t = std::min(1.0f, p.age / p.life);
    p.velocity *= std::exp(-s.drag * dt);
    const float wiggle = s.turbulence * dt;
    p.velocity.y += (p.riseSpeed * curves.velocity(t) - p.velocity.y) * std::min(1.0f, dt * 3.2f);
    p.velocity.x += (random() - 0.5f) * wiggle * 3.4f;
    p.velocity.z += (random() - 0.5f) * wiggle * 3.4f;
    p.velocity.y -= s.gravity * dt;
    p.position += p.velocity * dt;
    p.rotation += p.spin * dt;
  }
}

void ParticleEmitter::instances(const FireEmitterSettings& s, std::vector<VfxInstance>& out) const {
  const int count = std::min(s.particleCount, kMaxParticles);
  for (int i = 0; i < count; ++i) {
    const Particle& p = particles_[static_cast<size_t>(i)];
    const float t = std::min(1.0f, p.age / p.life);
    VfxInstance inst;
    inst.a = glm::vec4(p.position, p.size * s.particleScale * curves.size(t));
    inst.b = {p.rotation, std::fmod(p.phase + p.age * s.flipbookFps, 16.0f), curves.opacity(t), curves.emissive(t)};
    inst.c = glm::vec4(curves.color(t), 0.0f);
    out.push_back(inst);
  }
}

void drawParticleEmitter(const ParticleEmitter& emitter, const FireEmitterSettings& s, const glm::mat4& model, const VfxFlipbook& sheet,
                         float intensity, glm::vec3 sight, VfxBlend blend, bool depthTest, VfxFrame& frame) {
  if (!sheet) return;
  std::vector<VfxInstance> particles;
  emitter.instances(s, particles);
  if (particles.empty()) return;
  static const std::vector<VfxVertex> quad = [] {
    const VfxVertex a{{-0.5f, -0.5f, 0}, 0, {0, 0, 1}, 0}, b{{0.5f, -0.5f, 0}, 1, {0, 0, 1}, 0}, c{{0.5f, 0.5f, 0}, 1, {0, 0, 1}, 1},
        d{{-0.5f, 0.5f, 0}, 0, {0, 0, 1}, 1};
    return std::vector<VfxVertex>{a, b, c, a, c, d};
  }();
  const uint32_t firstInstance = static_cast<uint32_t>(frame.instances.size());
  frame.instances.insert(frame.instances.end(), particles.begin(), particles.end());
  VfxDraw d;
  d.gpu.model = model;
  d.gpu.params0 = {intensity, 0, 0, 0};
  d.gpu.params1 = {sheet.cellWidth, sheet.cellHeight, sheet.cropX, sheet.cropY};
  d.gpu.color = {1.0f / sheet.textureWidth, 1.0f / sheet.textureHeight, 0, 0};
  d.gpu.firstVertex = frame.addVertices(quad);
  d.gpu.vertexCount = static_cast<uint32_t>(quad.size());
  d.gpu.firstInstance = firstInstance;
  d.gpu.material = static_cast<uint32_t>(VfxMaterial::FlipbookParticle);
  d.gpu.sightAt = sight;
  d.gpu.texture = sheet.texture;
  d.instances = static_cast<uint32_t>(particles.size());
  d.blend = blend;
  d.depthTest = depthTest;
  frame.draws.push_back(d);
}

}  // namespace ember
