#pragma once

// The Mago Negro's Dissipar Magia and Globo de Invulnerabilidade (docs/SKILLS_MAGE.md, 2026-10-21), ours, in 3D on the effect
// runtime (the Combat material's energy shells, rings, streaks and motes; the Mago Negro's arcane violet):
//
//   Dissipar Magia      motes of arcane light drawn in from the area's rim to its centre as the cast gathers; at the impact a shell
//                       of violet light bursts out of the centre through the air to the area's edge, a ring runs out on the ground,
//                       a column of light stands a moment at the centre; on every body it reaches a shaft of white-violet light and
//                       the magic torn off it flung up and away as motes; a violet light;
//   Globo               the bubble rises out of the ground as a shell of violet light (a flash, a ring) and stands for as long as the
//                       rules keep it: a faint glassy dome with a bright rim, motes drifting slowly on its surface, a ring turning
//                       at its base, a soft light inside; it fades and sinks when the rules drop it; a spell it stops flashes on its
//                       surface where it struck (globeRipple).
//
// World space (y up).

#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "render/Camera.h"
#include "render/VfxDraw.h"

namespace ember {

class MageFx {
 public:
  struct Target {
    glm::vec3 feet{0.0f};
    float height = 1.8f;  // the card's height (0 = a hex: a body's height is assumed)
  };
  static constexpr float kDispelLead = 0.4f;  // seconds the light gathers before the burst

  // Dissipar Magia over the area (its centre on the ground, `radius` world units) bursting at `impact`, the magic torn off `bodies`.
  void dispel(glm::vec3 centre, float radius, std::vector<Target> bodies, float impact);
  // The Globo: its bubble (centre on the ground, `radius` world units) rising from `now`; returns its id. globeEnd fades it from `now`.
  int globeStart(glm::vec3 centre, float radius, float now);
  void globeEnd(int id, float now);
  // A spell the bubble stopped: a flash on its surface toward `at` (the struck one's chest), at `now`.
  void globeRipple(glm::vec3 at, float now);

  bool active(float now) const;  // a dispel or a ripple still playing (a standing globe is not "active")
  float latestEnd() const;
  void clear() { dispels_.clear(), globes_.clear(), ripples_.clear(); }
  void build(float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights);

 private:
  struct Dispel {
    glm::vec3 centre{0.0f};
    float radius = 2.0f;
    float impact = 0.0f;
    std::vector<Target> bodies;
    uint32_t seed = 0;
    float end() const { return impact + 1.3f; }
  };
  struct Globe {
    int id = 0;
    glm::vec3 centre{0.0f};
    float radius = 2.0f;
    float start = 0.0f, finish = -1.0f;
    uint32_t seed = 0;
  };
  struct Ripple {
    glm::vec3 at{0.0f};
    float start = 0.0f;
    uint32_t seed = 0;
    float end() const { return start + 0.45f; }
  };
  void buildDispel(const Dispel& d, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights);
  void buildGlobe(const Globe& g, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights);
  void buildRipple(const Ripple& r, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights);

  std::vector<Dispel> dispels_;
  std::vector<Globe> globes_;
  std::vector<Ripple> ripples_;
  int serial_ = 0;
  uint32_t seedSerial_ = 0;
  uint32_t quadFirst_ = 0, discFirst_ = 0, sphereFirst_ = 0;
  uint32_t quadCount_ = 0, discCount_ = 0, sphereCount_ = 0;
  std::vector<VfxInstance> streaks_;
};

}  // namespace ember
