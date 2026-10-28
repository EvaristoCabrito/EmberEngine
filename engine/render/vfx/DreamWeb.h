#pragma once

// Teia dos Sonhos 02 (ours, 3D): Ember's whole spell, not only its WebOfDreamsVFX net. Ember plays three things and
// 01 ported one of them (the net), laid on its side:
//   the shot    WEB_SHOT (gfx/shaders.ts, BattleEngine.webShotBeam): for WEB_SHOT_TRAVEL = 0.85 s a comet of silk leaves the caster
//               and grows to the zone: nine strands spread at the tail and converging on a dense knot at the head, a pulse of
//               light running up them, a violet glow travelling with it. Here: nine real tubes (the WebThread material) on a
//               slight arc, a head knot of tumbling gems, a travelling light.
//   the floor   GameArt.webfloor stamped on every hex the zone covers once the shot lands (a photo cut-out in Ember). Here: an
//               orb web of tubes on each hex (six spokes to the hex corners, three scalloped rings), growing outward from the
//               centre, resting on that hex's own ground.
//   the net     WebOfDreamsVFX / WebZoneVFX with DEFAULT_WEB_OF_DREAMS_VFX_SETTINGS (same seeds, knots, threads, pulses, tightening
//               at 1.04-1.48 s, binding flash at 1.42 s, lights), over the zone, and each unit inside wrapped by its own strands.
// What 01 got wrong (all in how the port was fed, see docs/SKILLS or the commit): it centred the net on the first hex the rules list
// (the top-left of the area, not the aimed hex), wrapped every hex of the area instead of the units standing in it, laid each unit's
// strands (Ember's vertical is its y, a flat card's up) along the ground, let half the net sink under the floor, drew no shot at
// all (0.85 s of nothing), no floor, used radius 1 for every level, and let go after 2.6 s while the rules keep the zone for rounds.
// This version: centred on the zone, sized by its radius (webZoneRadiusTiles = r x sqrt 3 + 1), the net a canopy at body height,
// the strands standing upright round each body, and it stands for as long as the rules keep the zone (SpellEffects::webStand /
// webEnd; without the rules, as in --fx-test, it holds 2.6 s after the shot lands and releases over 0.9 s like Ember's preview).
//
// Not carried over: the central light's shadow. Math in Ember's frame (z up) for the net and the floor, as Ember's.

#include <algorithm>
#include <map>
#include <vector>

#include "render/Camera.h"
#include "render/VfxDraw.h"
#include "render/vfx/ThreeMath.h"

namespace ember {

class DreamWeb {
 public:
  // A unit standing in the zone: `key` any stable number (its strands are seeded by it), its feet in the world, its card's height.
  struct Occupant {
    int key = 0;
    glm::vec3 feet{0.0f};
    float height = 1.5f;
  };
  // casterBody: where the shot leaves; centre: the zone's middle hex (ground); hexes: the ground of every hex it covers (centre
  // included); radius: the rules' zone radius in hexes (1, 2 or 3). The shot leaves at `start`; held until release() when
  // `standing`, else released 2.6 s after it lands.
  DreamWeb(glm::vec3 casterBody, glm::vec3 centre, std::vector<glm::vec3> hexes, int radius, std::vector<Occupant> occupants, float start,
           bool standing);

  static constexpr float kShotTravel = 0.85f, kFormation = 1.6f, kRelease = 0.9f, kPreviewHold = 2.6f;
  int id = 0;
  float landed() const { return start_ + kShotTravel; }
  // When the cast's own part is over (a standing web goes on until the rules end it).
  float end() const { return standing_ ? landed() + kFormation : releaseAt_ + kRelease; }
  bool done(float now) const { return !standing_ && now >= releaseAt_ + kRelease; }
  bool standing() const { return standing_; }
  // The rules' zone is gone: fade out from `now`.
  void release(float now) {
    standing_ = false;
    releaseAt_ = std::max(now, landed());
  }
  // Hold it until release() (the rules keep the zone).
  void stand() { standing_ = true, releaseAt_ = 1e9f; }
  // The units in the zone now (called every frame while the rules keep it).
  void setOccupants(const std::vector<Occupant>& units);

  void build(float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights);

  struct Strand {
    std::vector<glm::vec3> points;
    float delay, phase, width;
  };

 private:
  struct Wrap {
    std::vector<VfxVertex> strands;
    Occupant at;
  };
  void buildShot(float now, VfxFrame& frame, std::vector<VfxLight>& lights);

  glm::vec3 caster_, centre_;  // world
  glm::vec3 netCentre_;        // Ember frame, the canopy's middle
  std::vector<glm::vec3> hexes_;
  int radius_ = 1;
  float start_ = 0, releaseAt_ = 1e9f, extent_ = 1;
  bool standing_ = false;
  std::vector<glm::vec3> nodes_;
  std::vector<float> phase_;
  std::vector<VfxVertex> net_;
  std::vector<VfxVertex> floor_[2];  // two orb webs, laid by hex
  std::map<int, Wrap> wraps_;
};

}  // namespace ember
