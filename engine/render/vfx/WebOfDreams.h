#pragma once

// Engine2's WebOfDreamsVFX / WebZoneVFX (src/vfx/legacy/three/WebOfDreamsVFX.ts) with DEFAULT_WEB_OF_DREAMS_VFX_SETTINGS,
// ported to the effect runtime: Teia dos Sonhos 01. After the web shot's flight (WEB_SHOT_TRAVEL 0.85 s; the shot
// itself is Engine2's 2D canvas art, not part of this effect) a net forms over the zone: eight knots around its edge
// joined by a seeded lattice of 24 tapered threads (ring links, cross-bracing, loose tendrils) that grow in, wave and
// carry travelling pulses, tighten at 1.04-1.48 s with a binding flash at 1.42 s; glowing knots, 32 drifting
// shards, a central light and two travelling lights; every unit inside is wrapped by its own strands. In battle the
// zone holds while the rules keep it; here it holds until `releaseAt` (Engine2's preview: 2.6 s after the cast) and
// fades over 0.9 s. Its math runs in Ember's frame (z up) as Engine2's does.
//
// Not carried over: the central light's shadow.

#include <vector>

#include "render/Camera.h"
#include "render/VfxDraw.h"
#include "render/vfx/ThreeMath.h"

namespace ember {

class WebOfDreams {
 public:
  // center: the zone's centre hex, ground + 0.04; radiusTiles: webZoneRadiusTiles(zone radius) = radius x sqrt 3 + 1;
  // targets: the units inside (their anchors 0.9 up). World space. The net appears at start + 0.85 s.
  WebOfDreams(glm::vec3 center, float radiusTiles, const std::vector<glm::vec3>& targets, float start, float releaseAt);

  static constexpr float kShotTravel = 0.85f, kFormation = 1.6f, kRelease = 0.9f;
  bool done(float now) const { return now >= releaseAt_ + kRelease; }
  float end() const { return releaseAt_ + kRelease; }

  void build(float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights);

  struct Strand {
    std::vector<glm::vec3> points;
    float delay, phase, width;
  };

 private:
  glm::vec3 center_;  // Ember frame
  float start_ = 0, releaseAt_ = 0, extent_ = 1;
  std::vector<glm::vec3> nodes_;
  std::vector<float> phase_;
  std::vector<VfxVertex> net_;
  struct Wrapped {
    glm::vec3 offset;  // from the centre, Ember frame
    std::vector<VfxVertex> strands;
  };
  std::vector<Wrapped> targets_;
};

}  // namespace ember
