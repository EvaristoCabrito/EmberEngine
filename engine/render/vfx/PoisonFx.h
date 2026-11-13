#pragma once

// The poison spells, ours, in 3D on the effect runtime (2026-11-13, the user's brief on poison smoke: "lit smoke with a sickly
// color, heavy motion and a faint inner glow"). The gas is real volumes (VfxMaterial::GasVolume, shaders/vfx.hlsl): the eye's ray
// walked through them, lit by the scene through their own thickness, cut by the scene's depth, so no card shows and the gas wraps
// the ground and the bodies. It is heavy: it rolls, sinks, pools and creeps, slowly, and leaves by thinning out through its noise.
// Its body is a desaturated olive; the saturated acid green is kept for the glow inside it. Droplets and specks fly through it.
//
// Each spell has its own body of gas, sized to what the rules strike:
//   Breath      Sopro Venenoso (a cone from the caster, both sides): a rolling cone of gas breathed out over the whole cone, which
//               loses its way, sinks and lies in two pools along it;
//   Minor       Veneno Menor (the aimed hex and those round it, weak): a small glob thrown in an arc; it bursts in a puff that
//               mushrooms and falls back into a thin pool;
//   Caustic     Veneno Cáustico (the aimed hex strong, a splash round it): a great glob trailing gas; where it lands a geyser of
//               gas and acid goes up and collapses into a deep rolling pool over the whole area;
//   Plague      Praga Rastejante (one foe; it passes to its neighbours turn by turn): a rope of gas crawls over the ground from the
//               caster to the foe, winds up round it and stays on it;
//   Pestilence  Peste Negra (a wide area, deadly, both sides): black smoke seeps up out of the ground all over the area while the
//               cast gathers, then a dome of it, near black with green veins, comes down over everything and lies there;
//   Cloud       Névoa Ácida (a field that stays for rounds): poured out from its middle, a deep blanket of yellow gas over the whole
//               field, turning slowly and bubbling, for as long as the rules keep the field.
//
// World space (y up), one hex radius = 1. The hit lands at `impact`.

#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "render/Camera.h"
#include "render/VfxDraw.h"

namespace ember {

class PoisonSpell {
 public:
  enum class Style { Breath, Minor, Caustic, Plague, Pestilence, Cloud };
  // The caster's feet and card height; the hexes struck (their ground); the cast gathers from `start`, the gas lands at `impact`.
  // A Cloud has no caster: it starts at `start` and stands until stop().
  PoisonSpell(Style style, glm::vec3 casterFeet, float casterHeight, std::vector<glm::vec3> hexes, float start, float impact, uint32_t seed);

  float end() const { return end_; }
  bool done(float now) const { return now >= end_; }
  bool standing() const { return style_ == Style::Cloud && !stopped_; }  // a field the rules still keep (not "playing")
  void stop(float now);  // the field's end: it thins out from `now`
  int id = 0;            // a Cloud's zone

  void build(float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) const;

 private:
  Style style_;
  glm::vec3 feet_{0.0f}, hand_{0.0f}, direction_{0.0f, 0.0f, 1.0f}, centre_{0.0f};
  float start_ = 0.0f, impact_ = 0.0f, end_ = 0.0f, reach_ = 1.0f, radius_ = 1.0f, linger_ = 2.0f;
  bool stopped_ = false;
  std::vector<glm::vec3> hexes_;
  uint32_t seed_ = 0;
};

}  // namespace ember
