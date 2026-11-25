#pragma once

// The lightning spells, ours, in 3D (the user, 2026-11-25: a second version of them, to set beside Relampago 04 and choose): Relâmpago,
// Relâmpago Maior and Choque 05, Tempestade Elétrica 02, Ira dos Céus 02, the echo of a bolt (StormBolt), Cadeia de Raios 03
// (ChainBolt). The same spells and the same moments as before; another way of striking, built on what real lightning does:
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
//   Tempestade Elétrica   a dark cloud gathers over the area, lit from inside by lightning running through it; its bolts fall out of
//                         it one after another, in no order, on everyone standing there and on a few empty hexes.
//   Ira dos Céus          the cloud, larger; every bolt at once, larger, and every one struck a second time down the same channel.
//   The echo              the bolt alone, small, with no cast: the leader and the stroke.
//   Cadeia de Raios       the bolt leaves the hand and jumps from body to body, each jump its own tree, thinner than the last; when
//                         it has reached the last body the whole chain lights at once, one more time.
//
// World space (y up). A bolt's return stroke lands at `impact` (the rules' damage, numbers and hit pose land then). Drawn with the
// Combat material (shaders/vfx.hlsl: 0 streaks, 3 ground ring, 6 lightning) and GasVolume puffs (the cloud, the smoke).

#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "render/Camera.h"
#include "render/VfxDraw.h"

namespace ember {

// One line of a bolt's tree: its points, how far along the trunk's length each one lies (the trunk's own run 0..1; a fork goes on from
// where it left), and its width there (1 = its generation's full width, 0 = its point).
struct BoltLimb {
  std::vector<glm::vec3> points;
  std::vector<float> along, width;
  int generation = 0;  // 0 the trunk, 1 a fork, 2 a twig
};

class StormBolt {
 public:
  enum class Style { Bolt, Storm, Wrath, Echo };
  struct Target {
    glm::vec3 feet{0.0f};
    float height = 1.8f;  // the card's height
    bool stands = true;   // a body (false: an empty hex, the bolt strikes the ground)
  };
  // The caster's feet and card height; the struck bodies (Bolt: the first return stroke at `impact`, the next ones a beat apart); the
  // cast gathers from `start`. scale 1 = Relâmpago, 0.75 = Choque.
  StormBolt(Style style, glm::vec3 casterFeet, float casterHeight, const std::vector<Target>& targets, float start, float impact, float scale, uint32_t seed);

  float end() const { return last_ + tail_; }
  bool done(float now) const { return now >= end(); }
  void build(float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) const;

 private:
  struct Strike {
    std::vector<BoltLimb> limbs;  // limbs[0] = the trunk
    glm::vec3 feet{0.0f}, body{0.0f}, head{0.0f}, top{0.0f};
    float height = 1.8f, at = 0.0f;  // at: its return stroke
    bool stands = true;
    uint32_t seed = 0;
  };
  void buildCaster(float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) const;
  void buildCloud(float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) const;
  void buildStrike(const Strike& strike, float age, float time, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) const;

  Style style_ = Style::Bolt;
  glm::vec3 hand_{0.0f}, feet_{0.0f}, cloudAt_{0.0f};
  float casterHeight_ = 2.0f, scale_ = 1.0f, start_ = 0.0f, impact_ = 0.0f, last_ = 0.0f, tail_ = 1.0f, cloudRadius_ = 0.0f;
  std::vector<Strike> strikes_;
  BoltLimb thrown_;  // the thin bolt the caster throws up to the sky
  uint32_t seed_ = 0;
};

// Cadeia de Raios 03.
class ChainBolt {
 public:
  struct Target {
    glm::vec3 feet{0.0f}, body{0.0f};
    float height = 1.8f;
  };
  // The bolt reaches the first body of the chain at `impact`, each next one kStagger later.
  ChainBolt(glm::vec3 casterFeet, float casterHeight, const std::vector<Target>& chain, float start, float impact, uint32_t seed);

  float end() const { return finale_ + 0.7f; }
  bool done(float now) const { return now >= end(); }
  void build(float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) const;

  static constexpr float kStagger = 0.12f;

 private:
  struct Link {
    std::vector<BoltLimb> limbs;
    glm::vec3 feet{0.0f}, body{0.0f};
    float height = 1.8f, at = 0.0f, size = 1.0f;
    uint32_t seed = 0;
  };
  glm::vec3 hand_{0.0f}, feet_{0.0f};
  float casterHeight_ = 2.0f, start_ = 0.0f, impact_ = 0.0f, finale_ = 0.0f;
  std::vector<Link> links_;
  uint32_t seed_ = 0;
};

}  // namespace ember
