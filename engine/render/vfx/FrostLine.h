#pragma once

// Frost 02, ours, in 3D (the user, 2026-10-19: "frost used to have a 3D FX already where is it?"; it never had: 01 is flipbook
// cards). A line of ice on the effect runtime (the IceStorm material, shaders/vfx.hlsl; the crystal mesh, rings and ice dust of
// Tempestade de Gelo 02 in render/vfx/IceParts.h). One look, the same on every cast (fixed seed).
//
//   The caster  cold gathers: a frost ring contracts round the feet over a frosting floor, snow and ice dust spiral in to the
//               hand, six shards orbit it and are thrown down the line when the cast lets go; a cold light rises at the hand.
//   Each hex    of the cast's targets, nearest first (0.055 s apart): ice erupts as a cluster of crystal spikes (a tall one and
//               satellites leaning out, overshooting as they rise), ice chips fly, a frost ring and a second slower one race
//               over the ground, a puff of ice dust and a low mist; the floor frosts over from the hex's middle; a strong cold
//               flash that settles into a glow. After a hold the spikes crack and shatter into splinters.
//
// World space (y up), one hex radius = 1. The hit lands at `impact` (the rules' damage and numbers land then).

#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "render/VfxDraw.h"

namespace ember {

class FrostLine {
 public:
  // The caster's feet and card height; the hexes of the area (their ground); the cast gathers from `start`, the first ice rises
  // at `impact`.
  FrostLine(glm::vec3 casterFeet, float casterHeight, std::vector<glm::vec3> hexes, float start, float impact, uint32_t seed);

  float end() const;
  bool done(float now) const { return now >= end(); }
  void build(float now, VfxFrame& frame, std::vector<VfxLight>& lights) const;

  static constexpr float kStagger = 0.055f;  // between one hex and the next
  static constexpr float kGrow = 0.24f, kShatterAt = 1.15f, kLife = 1.7f;

 private:
  void buildCaster(float now, VfxFrame& frame, std::vector<VfxLight>& lights) const;
  void buildHex(size_t index, float age, VfxFrame& frame, std::vector<VfxLight>& lights) const;

  glm::vec3 feet_{0.0f}, hand_{0.0f}, direction_{0.0f, 0.0f, 1.0f};
  float start_ = 0.0f, impact_ = 0.0f;
  std::vector<glm::vec3> hexes_;  // nearest first
  uint32_t seed_ = 0;
};

}  // namespace ember
