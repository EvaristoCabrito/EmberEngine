#pragma once

// Relâmpago 02, ours, in 3D (the user, 2026-10-19: "make me a new 3D Raio effect"): real forked lightning on the effect runtime,
// one look, the same on every cast. Drawn with the Combat material (shaders/vfx.hlsl mode 6: a jagged tube, white-hot down its
// middle and blue-white in its shell, a crackle along its length).
//
//   The caster   a ring of light on the ground and a crackling flare at the hand while the cast gathers, small arcs jumping off
//                the hand, sparks, a cold light rising.
//   Each target  (one bolt a beat after the other): a bolt strikes down from the sky onto the body in a few hundredths of a second,
//                jagged, with forks branching off it; it flickers between three shapes while it holds, flashes again (the return
//                stroke) and fades; arcs crawl over the body, a flash and a ring race over the ground, sparks are flung out, a
//                dazzling blue-white light dies away.
//
// World space (y up). The hit lands at `impact` (the rules' damage, numbers and hit pose land then). Choque is the same at 3/4 size.

#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "render/Camera.h"
#include "render/VfxDraw.h"

namespace ember {

class Lightning {
 public:
  struct Target {
    glm::vec3 feet{0.0f};
    float height = 1.8f;  // the card's height
  };
  // The caster's feet and card height; the struck bodies (the first bolt falls at `impact`, the next ones 0.05 s apart); the cast
  // gathers from `start`. scale 1 = Relâmpago, 0.75 = Choque.
  Lightning(glm::vec3 casterFeet, float casterHeight, std::vector<Target> targets, float start, float impact, float scale, uint32_t seed);

  float end() const { return impact_ + kStagger * static_cast<float>(targets_.size()) + 0.9f; }
  bool done(float now) const { return now >= end(); }
  void build(float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) const;

  static constexpr float kStagger = 0.05f;

 private:
  void buildCaster(float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) const;
  void buildStrike(size_t index, float age, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) const;

  glm::vec3 hand_{0.0f}, feet_{0.0f};
  float casterHeight_ = 2.0f, scale_ = 1.0f, start_ = 0.0f, impact_ = 0.0f;
  std::vector<Target> targets_;
  uint32_t seed_ = 0;
};

}  // namespace ember
