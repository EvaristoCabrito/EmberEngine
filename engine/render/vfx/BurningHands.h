#pragma once

// Engine2's BurningHandsV2VFX (src/vfx/legacy/three/BurningHandsV2VFX.ts) with DEFAULT_BURNING_HANDS_V2_SETTINGS, as
// BattleSpells3D plays it, ported to the effect runtime: Mãos Flamejantes 01, and Sopro Venenoso 01 (the same with
// Engine2's poison: the poison-v2-main sheet, green curves, NormalBlending). One ParticleEmitter (80 flipbook flames)
// sprays from the caster toward the struck cells as a steady jet for 0.72 s, then its last flames burn out by 2.35 s;
// five lights (two at the hands, three riding the flame front) flicker and fade with it. The rules release at 0.56 s.
// Its math runs in Ember's frame (z up) as Engine2's does.
// Version 2 (ours, user 2026-10-16: "the cone is reversed"): Engine2's flames shrink and fade as they age and its jet
// barely opens (about +-5 degrees), so the spray read thick at the hands and thin at the target. Version 2 opens a real
// cone (+-26 degrees), the flames grow as they travel and stay visible longer, and they rise up (world y) instead of
// drifting along the board.

#include <vector>

#include "render/Camera.h"
#include "render/VfxDraw.h"
#include "render/vfx/ParticleEmitter.h"

namespace ember {

class BurningHands {
 public:
  // origin: the caster's anchor 0.9 up; cells: the struck cells' ground + 0.04. World space.
  BurningHands(glm::vec3 origin, const std::vector<glm::vec3>& cells, bool poison, float start, int version = 1);

  static constexpr float kSpray = 0.72f, kRelease = 0.56f, kFinish = 2.35f;
  bool done(float now) const { return now - start_ >= kFinish; }
  float end() const { return start_ + kFinish; }

  // sheet: fx/fire-flipbook-4x4 (or poison-v2-main-flipbook-4x4 for Poison Breath).
  void build(float now, const Camera& camera, const VfxFlipbook& sheet, VfxFrame& frame, std::vector<VfxLight>& lights);

 private:
  void advance(float t);  // the emitter simulated in fixed steps up to t (its own clock)

  glm::vec3 origin_;  // Ember frame
  glm::vec2 direction_{0, 1}, sideways_{-1, 0};
  float length_ = 1, width_ = 1, start_ = 0, elapsed_ = 0;
  bool poison_ = false, started_ = false;
  int version_ = 1;
  FireEmitterSettings settings_;
  ParticleEmitter emitter_;
};

}  // namespace ember
