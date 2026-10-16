#pragma once

// Engine2's ParticleEmitter (src/vfx/legacy/three/ThreeVfxSystem.ts): a pool of up to 96 flipbook flame particles
// simulated on the CPU (spawn, drag, rise, wiggle, gravity, an optional steady jet flow) and drawn as camera-facing
// quads sampling a 4x4 sheet with a little uv turbulence (material FlipbookParticle, NormalBlending, no depth test).
// Its curves (size, opacity, emissive, velocity, colour over a particle's life) can be swapped, as Engine2's element
// emitters and Poison Breath do. Engine2 stepped it by each frame's time; its owners here step it in fixed kStep (1/60 s)
// from its start, so a capture at any moment and a live run see the same flames.

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <vector>

#include "render/VfxDraw.h"

namespace ember {

struct FireEmitterSettings {
  int particleCount = 26;
  float particleScale = 1.18f, velocity = 1.02f, drag = 1.25f, turbulence = 0.65f, gravity = 0.35f, flipbookFps = 18.0f;
  float coreIntensity = 1.15f, lightIntensity = 6.5f, lightRadius = 3.2f;
  float lifetimeScale = 1.0f, spread = 1.0f;
  bool flow = false;  // a steady jet (flowDirection / flowSpeed / flowSpread)
  glm::vec3 flowDirection{0.0f};
  float flowSpeed = 0.0f, flowSpread = 0.0f;
  // Ours (Mãos Flamejantes 02): the jet runs in the x-z plane with y up (flowDirection.x / .y = world x / z), so flames
  // rise up instead of drifting along the board as Engine2's did (its emitter's y was the board's y in Ember's frame).
  bool groundFlow = false;
};

class ParticleEmitter {
 public:
  static constexpr int kMaxParticles = 96;
  static constexpr float kStep = 1.0f / 60.0f;

  struct Curves {
    std::function<float(float)> size = [](float t) { return 0.42f + std::sin(3.14159265f * (0.12f + t * 0.76f)) * 0.48f; };
    std::function<float(float)> opacity = [](float t) { return std::min(1.0f, t * 7.0f) * std::pow(1.0f - t, 0.8f); };
    std::function<float(float)> emissive = [](float t) { return 1.7f - t * 0.85f; };
    std::function<float(float)> velocity = [](float t) { return 1.0f - t * 0.42f; };
    std::function<glm::vec3(float)> color = [](float t) { return glm::vec3(1.0f, 0.94f - t * 0.56f, 0.68f - t * 0.6f); };
  };
  Curves curves;

  void setSeed(uint32_t seed) { state_ = seed ? seed : 1u; seeded_ = true; }
  void reset(const FireEmitterSettings& s, bool stagger = true);
  void update(float dt, const FireEmitterSettings& s, bool looping = true);
  // The particles as VfxInstances (a = centre in the emitter's space + size, b = rotation, frame, opacity, emissive,
  // c = tint) for one FlipbookParticle draw.
  void instances(const FireEmitterSettings& s, std::vector<VfxInstance>& out) const;
  float clock() const { return clock_; }

 private:
  struct Particle {
    float age = 0, life = 1, phase = 0, spin = 0, rotation = 0, size = 0, riseSpeed = 0;
    glm::vec3 position{0.0f}, velocity{0.0f};
  };
  float random();  // setSeed's xorshift (Engine2 uses Math.random when unseeded: seeded here always)
  void spawn(Particle& p, const FireEmitterSettings& s, float initialAge);

  std::array<Particle, kMaxParticles> particles_{};
  float clock_ = 0.0f;
  uint32_t state_ = 1u;
  bool seeded_ = false;
};

// One FlipbookParticle draw of an emitter's particles: `model` places the emitter (Ember -> world, its mesh position
// and scale), the sheet is the emitter's texture, `intensity` its uIntensity.
void drawParticleEmitter(const ParticleEmitter& emitter, const FireEmitterSettings& s, const glm::mat4& model, const VfxFlipbook& sheet,
                         float intensity, glm::vec3 sight, VfxBlend blend, bool depthTest, VfxFrame& frame);

}  // namespace ember
