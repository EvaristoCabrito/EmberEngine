#pragma once

// The water spells, ours, in 3D on the effect runtime (2026-11-26, the user's water brief, then: "make it a Sahagin Spell", "and a tier 6
// elementalist", "two separated effects, smaller on the sahagin to match his casting video and something nicer for the T6 spell").
// Built the way game water is (the brief's study: the Jedi: Fallen Order splashes, mesh particles with a refraction shader; refraction
// sells it; foam is splash, surface foam and bubbles; the colour comes from depth). Every part is a real mesh (no flat cards, no flat
// decals: the user, 2026-11-25, "NUNCA MAIS USAR EFEITOS 2d") drawn with the IceStorm material's water look (shaders/vfx.hlsl mode 14):
// the scene behind read through it, bent (index 1.33); near clear where thin, teal where thick (Beer-Lambert: red taken first); the
// sky in it at a grazing angle (Fresnel); small sharp sun glints; white foam on its rims and where it is painted; a surface stirred by
// moving ripples; it leaves by tearing apart behind a white lip, never by fading.
//
//   Jet  (Jato d'Água, the Sahagin's, small): an orb of water gathers at his hand, drops spiralling into it; at the release it
//        stretches into a jet that flies to the foe, shedding drops; it bursts on the body: the water thrown open, a small crown where
//        it hits the ground, drops flung out falling at real gravity, each drop that lands its own small splash and ripple.
//   Wave (Maremoto, the Elementalista's T6, large): water rises out of the ground before the caster into a mound, drops climbing to
//        the hands; at the release a curling wall of water rolls out over the cone, as wide as the cone where it is, its crest foaming
//        and throwing spray ahead; as it passes each hex it crashes there (a tall crown, a spray of drops, ripples racing out; on a
//        body, the water bursting over it); at the end of its run it breaks and falls in one last spray.
//
// World space (y up), one hex radius = 1. The rules' hit lands at `impact` (the jet's arrival; the wave's first hex).

#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "render/Camera.h"
#include "render/VfxDraw.h"

namespace ember {

class WaterSpell {
 public:
  enum class Style { Jet, Wave };
  struct Target {
    glm::vec3 feet{0.0f};
    float height = 0.0f;  // a card's height, or 0 for an empty hex
  };
  // `hand`: where it leaves the caster (the cast pose's hands); the hexes it covers (a unit's card height on the ones a body stands on);
  // the cast gathers from `start`, the hit lands at `impact`.
  WaterSpell(Style style, glm::vec3 casterFeet, float casterHeight, glm::vec3 hand, std::vector<Target> hexes, float start, float impact, uint32_t seed);

  float end() const { return end_; }
  bool done(float now) const { return now >= end_; }
  void build(float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) const;

  // From the start of the cast to the hit: the gathering, then the jet's flight / the wave's run to the nearest hex.
  static float lead(Style style, glm::vec3 casterFeet, glm::vec3 hand, glm::vec3 first);

  static constexpr float kJetGather = 0.5f, kWaveGather = 0.8f;
  static constexpr float kJetSpeed = 8.5f, kWaveSpeed = 5.0f;  // world units a second
  static constexpr float kWaveFrom = 0.9f;                     // the wave rises this far before the caster

 private:
  void buildJet(float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) const;
  void buildWave(float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) const;

  Style style_;
  glm::vec3 feet_{0.0f}, hand_{0.0f}, direction_{0.0f, 0.0f, 1.0f};
  float casterHeight_ = 1.8f, k_ = 1.0f, start_ = 0.0f, release_ = 0.0f, impact_ = 0.0f, end_ = 0.0f;
  float reach_ = 0.0f, spread_ = 0.5f, firstAt_ = 1.0f;  // the wave: its run, how fast the cone widens, the nearest hex's distance
  std::vector<Target> hexes_;
  std::vector<float> crashAt_;  // the wave: when it crashes on each hex
  uint32_t seed_ = 0;
};

}  // namespace ember
