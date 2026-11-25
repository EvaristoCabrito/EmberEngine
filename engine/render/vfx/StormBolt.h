#pragma once

// Relâmpago 05, ours, in 3D (the user, 2026-11-25: a second version of the lightning spells, to set beside Relampago 04 and choose).
// The same spell and the same moments as vfx/Lightning; another way of striking, built on what real lightning does:
//
//   The tree     one bolt = one tree, grown once (midpoint displacement, the push halved at every level) and never redrawn: the trunk
//                from the sky to the body, forks leaving it downward, twigs leaving the forks. Each generation has fewer levels than
//                the one it leaves (so it ends), is thinner and dimmer, and runs out to a point. Drawn as flat ribbons turned to the
//                eye at every point: the kinks stay sharp at any distance.
//   The cast     the charge is drawn in to the caster's hand (motes, arcs off the hand and down to the ground, a ring closing in),
//                then thrown up: a thin bolt leaves the hand for the sky a moment before the sky answers.
//   The body     knows it first: a ring closes in round its feet, thin streamers reach up off its head, a faint thread of light
//                marks the path (the pre-beam).
//   The strike   the stepped leader comes down the tree in jumps, dim, forks and all, a bright point at its tip; the return stroke
//                lights the whole tree at once, swollen and white; then three re-flashes down the trunk alone (the forks are spent),
//                each switched on and off, never faded; a violet trace of the path hangs a split second.
//   The ground   a flash, two rings racing out, arcs crawling over the floor from the feet and over the body (a new shape every
//                0.05 s), sparks thrown up that fall and bounce, a spot of ground left glowing, smoke; a light that spikes with
//                every flash and lights the whole area.
//
// World space (y up). The return stroke lands at `impact` (the rules' damage, numbers and hit pose land then). Choque is the same
// at 3/4 size. Drawn with the Combat material (shaders/vfx.hlsl: 0 streaks, 3 ground ring, 6 lightning) and a GasVolume puff.

#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "render/Camera.h"
#include "render/VfxDraw.h"

namespace ember {

class StormBolt {
 public:
  struct Target {
    glm::vec3 feet{0.0f};
    float height = 1.8f;  // the card's height
  };
  // The caster's feet and card height; the struck bodies (the first return stroke at `impact`, the next ones kStagger apart); the
  // cast gathers from `start`. scale 1 = Relâmpago, 0.75 = Choque.
  StormBolt(glm::vec3 casterFeet, float casterHeight, std::vector<Target> targets, float start, float impact, float scale, uint32_t seed);

  float end() const { return impact_ + kStagger * static_cast<float>(strikes_.empty() ? 0 : strikes_.size() - 1) + 1.0f; }
  bool done(float now) const { return now >= end(); }
  void build(float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) const;

  static constexpr float kStagger = 0.07f;

 private:
  // One line of the tree: its points, how far along the trunk's length each one lies (the trunk's own run 0..1; a fork goes on from
  // where it left), and its width there (1 = its generation's full width, 0 = its point).
  struct Limb {
    std::vector<glm::vec3> points;
    std::vector<float> along, width;
    int generation = 0;  // 0 the trunk, 1 a fork, 2 a twig
  };
  struct Strike {
    std::vector<Limb> limbs;  // limbs[0] = the trunk
    glm::vec3 feet{0.0f}, body{0.0f}, head{0.0f};
    float height = 1.8f;
    uint32_t seed = 0;
  };
  void grow(Strike& strike, glm::vec3 from, glm::vec3 to, int levels, int generation, float alongStart, float trunkLength, uint32_t seed) const;
  void buildCaster(float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) const;
  void buildStrike(const Strike& strike, float age, float time, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) const;

  glm::vec3 hand_{0.0f}, feet_{0.0f};
  float casterHeight_ = 2.0f, scale_ = 1.0f, start_ = 0.0f, impact_ = 0.0f;
  std::vector<Strike> strikes_;
  Limb thrown_;  // the thin bolt the caster throws up to the sky
  uint32_t seed_ = 0;
};

}  // namespace ember
