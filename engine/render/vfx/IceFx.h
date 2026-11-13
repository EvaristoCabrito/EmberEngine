#pragma once

// The ice spells, ours, in 3D on the effect runtime (2026-11-12, the user's ice brief: "ice spells must look like real, cold, sharp
// ice from every camera angle"). One kit for every ice spell, drawn with the IceStorm material's ice look (shaders/vfx.hlsl modes
// 8..13: a body of ice, frost on the ground, cold mist, glints, frost streaks, the flash) over IceParts.h's meshes.
//
// Every spell runs the same life, each phase with its own time:
//   build-up  (the cast's wind-up, at least 0.3 s) mist is drawn to the caster's hand, small crystals form round it, a blue glow grows;
//   release   (0.1 s) a short white flash and a burst of small shards;
//   travel    the spell's own: Frost's thrown shards, the Lança's lance with its frost trail, glints and falling mist, the
//             Vendaval's flight of shards on streaks of wind, the Zero Absoluto's ring of frost racing out over the ground;
//   impact    (0.2 s, on every hex in its turn) ice snaps up out of the ground (it overshoots and settles), chips fly, a white
//             flash, a crack mark struck into the ground, a cold light;
//   linger    (1-3 s) frost spreads over the ground behind a bright leading edge, heavy mist sinks and spreads, glints, debris;
//   exit      (0.4 s) the frost melts back (its growth in reverse) and the ice dissolves through noise with a bright lip while
//             splinters fall: it never just fades.
// Ice is real meshes (crystals), so nothing rolls or tilts with the camera; frost and crack marks lie flat on the ground; mist,
// glints and the flash face the camera. Layers are drawn in one fixed order: ground, mist, streaks, ice, flash, glints.
//
// World space (y up), one hex radius = 1. The hit lands at `impact` (the rules' damage and numbers land then).

#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "render/Camera.h"
#include "render/VfxDraw.h"

namespace ember {

class IceSpell {
 public:
  enum class Style {
    Frost,         // a line of hexes from the caster: ice erupts on each, nearest first
    Lance,         // a lance thrown down a line: each hex it passes is struck as it goes by
    AbsoluteZero,  // an area: the cold bursts from its middle and every hex freezes, the middle first
    Gale,          // a cone: a wind of shards from the caster, the ground freezing under it
  };
  // The caster's feet and card height; the hexes of the area (their ground); the cast gathers from `start`, the first ice at `impact`.
  IceSpell(Style style, glm::vec3 casterFeet, float casterHeight, std::vector<glm::vec3> hexes, float start, float impact, uint32_t seed);

  float end() const { return end_; }
  bool done(float now) const { return now >= end_; }
  void build(float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) const;

  static constexpr float kErupt = 0.2f, kExit = 0.4f;  // the impact and the exit, on each hex
  static constexpr float kLanceSpeed = 22.0f;           // the lance's flight, world units a second

 private:
  struct Spec {
    int shards;                 // crystals in a hex's cluster
    float tall, small;          // the tall one's and the others' length
    float lean;                 // how far the cluster leans down the spell's way
    float frost;                // the ground frost's radius
    float mark;                 // the crack mark's strength
    float linger;               // seconds the ice stands
  };
  static Spec specOf(Style style);

  Style style_;
  Spec spec_;
  glm::vec3 feet_{0.0f}, hand_{0.0f}, direction_{0.0f, 0.0f, 1.0f}, centre_{0.0f};
  float start_ = 0.0f, impact_ = 0.0f, end_ = 0.0f, reach_ = 0.0f, radius_ = 1.0f;
  std::vector<glm::vec3> hexes_;
  std::vector<float> delay_;  // each hex's eruption after the impact
  uint32_t seed_ = 0;
};

}  // namespace ember
