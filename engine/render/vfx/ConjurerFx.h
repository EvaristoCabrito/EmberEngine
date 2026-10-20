#pragma once

// The Conjurador's spells (docs/SKILLS_CONJURER.md, 2026-10-20), ours, in 3D on the effect runtime (the Holy and Combat
// materials in the Conjurador's violet, with bone and shadow), one look each, the same on every cast:
//
//   Troca de Lugar      two spirals of violet motes cross between the two hexes as the cards fly, a flash and a ring at each
//                       end;
//   Lança de Osso       a lance of bone, lit by the scene, flies from the caster's hand along the line shedding violet motes;
//                       on each body it passes a flash, bone shards flung out, a violet ring on the ground;
//   Imagem Espelhada    a burst of violet on the ally as the illusions appear; the illusions themselves stand beside it for
//                       as long as the rules keep them (mirrorStart / mirrorUpdate / mirrorEnd): hooded shapes of violet
//                       energy, a halo at their feet, swaying; a struck copy shatters into motes;
//   Detonar Invocação   violet energy drawn into the summon while it swells; at the impact a flash, a dark pressure shell
//                       out to the edge of the blast, a dark ring and rays across the ground, ash and embers, a violet
//                       light; a flash on every body in it;
//   Elo de Sacrifício   dark chains of light run from the caster to each of its summons and pulse; a halo closes round the
//                       caster;
//   Pesadelo            a dark cloud rolls over the area, a violet pool under it, eyes of light open in the dark; tendrils
//                       of shadow rise round each foe, a flash, a violet light pulsing;
//   Fortalecer Invocações  a pulse from the caster; on each summon a violet shaft rises, a halo climbs the body, motes
//                       converge, a light.
//
// World space (y up). Points on a body are pulled toward the eye (kPull) so they show over the card, as CombatFx does.

#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "render/Camera.h"
#include "render/VfxDraw.h"

namespace ember {

class ConjurerFx {
 public:
  struct Target {
    glm::vec3 feet{0.0f};
    float height = 1.8f;  // the card's height (0 = not known: a body's height is assumed)
  };
  // Seconds each spell's effect gathers before its impact.
  static float leadIn(const std::string& kind);
  // The bone lance's flight along `hexes` (world points), seconds.
  static float lanceSeconds(size_t hexes);

  void swap(const Target& a, const Target& b, float start, float finish);
  void lance(glm::vec3 from, float fromHeight, const std::vector<glm::vec3>& hexes, std::vector<Target> bodies, float release, float impact);
  void mirrorBurst(const Target& ally, float impact);
  // The standing illusions beside a unit: `copies` of them from `now`; mirrorUpdate follows the unit and shatters the copies that
  // vanished; mirrorEnd fades them from `now`. Returns the effect's id.
  int mirrorStart(glm::vec3 feet, float height, int copies, float now);
  void mirrorUpdate(int id, glm::vec3 feet, int copies, float now);
  void mirrorEnd(int id, float now);
  void detonate(const Target& summon, float radius, std::vector<Target> bodies, float impact);
  void bond(glm::vec3 from, float fromHeight, std::vector<Target> summons, float impact);
  void nightmare(glm::vec3 centre, float radius, std::vector<Target> bodies, float impact);
  void empower(glm::vec3 from, float fromHeight, std::vector<Target> summons, float impact);

  bool active(float now) const;
  float latestEnd() const;
  void clear() { effects_.clear(); }
  void build(float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights);

 private:
  enum class Kind { Swap, Lance, MirrorBurst, Mirror, Detonate, Bond, Nightmare, Empower };
  struct Effect {
    Kind kind;
    int id = 0;
    glm::vec3 from{0.0f}, centre{0.0f};
    float fromHeight = 1.8f, height = 1.8f, radius = 2.0f;
    float start = 0.0f, impact = 0.0f, finish = 0.0f;
    std::vector<glm::vec3> hexes;
    std::vector<Target> bodies;
    // Mirror: the copies standing, when they end (-1 = standing), the shatters (time, slot)
    int copies = 0;
    float endAt = -1.0f;
    std::vector<std::pair<float, int>> shatters;
    uint32_t seed = 0;
    float end() const;
  };
  void buildSwap(const Effect& e, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights);
  void buildLance(const Effect& e, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights);
  void buildMirrorBurst(const Effect& e, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights);
  void buildMirror(const Effect& e, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights);
  void buildDetonate(const Effect& e, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights);
  void buildBond(const Effect& e, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights);
  void buildNightmare(const Effect& e, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights);
  void buildEmpower(const Effect& e, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights);
  // Where a standing illusion's feet are: slot 0.. round the unit, across the view.
  static glm::vec3 copyFeet(glm::vec3 feet, int slot, int copies, const Camera& camera, float t);

  std::vector<Effect> effects_;
  uint32_t serial_ = 0;
  uint32_t quadFirst_ = 0, sphereFirst_ = 0, discFirst_ = 0, columnFirst_ = 0, haloFirst_ = 0, lanceFirst_ = 0, phantomFirst_ = 0;
  uint32_t quadCount_ = 0, sphereCount_ = 0, discCount_ = 0, columnCount_ = 0, haloCount_ = 0, lanceCount_ = 0, phantomCount_ = 0;
  std::vector<VfxInstance> streaks_, puffs_, motes_, shards_;
};

}  // namespace ember
