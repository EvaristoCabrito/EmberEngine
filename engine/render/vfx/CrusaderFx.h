#pragma once

// The Cruzado's spells (docs/SKILLS_CRUSADER.md, 2026-10-25), ours, in 3D on the effect runtime (the Holy and Combat materials:
// shafts of light, pools, halos, motes, rings, energy shells, streaks), one look each, the same on every cast:
//
//   Golpe Consagrado  the blade lights up in gold before the blow (a glow at the weapon hand toward the foe, motes drawn into it,
//                     a warm light); where the blow lands, a short flash with gold sparks flying out and a ring on the ground;
//   Toque Sagrado     his hand glows gold at the ally's shoulder; a soft light climbs the ally's body (a faint shaft, a halo
//                     rising from the feet past the head, motes going up), the ground glows under it;
//   Juramento         while it holds (MapStage syncCrusader): a faint gold aura pulsing round him, gold embers rising off it, a
//                     soft light; the rules say "Juramento" the first time it wakes;
//   Manto do Cruzado  a wave of gold light spreads from him across the radius; every ally it reaches flashes gold, and keeps a
//                     faint gold outline for as long as the rules keep the mantle (standing, MapStage syncCrusader); each blow
//                     with its holy dice throws a gold spark where it lands (spark()).
//
// World space (y up). Points on a body are pulled toward the eye (kPull) so they show over the card, as CombatFx does.

#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "render/Camera.h"
#include "render/VfxDraw.h"

namespace ember {

class CrusaderFx {
 public:
  struct Target {
    glm::vec3 feet{0.0f};
    float height = 1.8f;  // the card's height (0 = not known: a body's height is assumed)
  };
  // Seconds each spell's light gathers before its impact (the rules' hit lands on the impact; the blow's is the pose's).
  static float leadIn(const std::string& kind);

  // Golpe Consagrado: the blade's glow from `start` to the blow at `impact` (the caster at `from`, the foe at `toward`) ...
  void bladeGlow(glm::vec3 from, float fromHeight, glm::vec3 toward, float start, float impact);
  // ... and, when the blow landed, the holy flash on the struck body.
  void holyImpact(const Target& struck, glm::vec3 from, float at);
  // Toque Sagrado on the ally beside the caster at `from`.
  void touch(glm::vec3 from, float fromHeight, const Target& ally, float impact);
  // Manto do Cruzado: the wave from `from` over `radius`, on the allies it reaches.
  void mantle(glm::vec3 from, float fromHeight, float radius, std::vector<Target> allies, float impact);
  // A blow with the mantle's holy dice landed on `struck`.
  void spark(const Target& struck, glm::vec3 from, float at);

  // Standing on a unit for as long as the rules keep it: the Juramento's aura, the Manto's outline. start() returns its id.
  enum class Stand { Oath, Mantle };
  int standStart(Stand kind, glm::vec3 feet, float height, float now);
  void standMove(int id, glm::vec3 feet, float height);
  void standEnd(int id, float now);

  bool active(float now) const;
  float latestEnd() const;
  void clear() { effects_.clear(), stands_.clear(); }
  void build(float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights);

 private:
  enum class Kind { Blade, Impact, Touch, Mantle, Spark };
  struct Effect {
    Kind kind;
    glm::vec3 from{0.0f}, at{0.0f};  // the caster's feet; the struck / healed body's feet (the blade: the foe's)
    float fromHeight = 1.8f, height = 1.8f, radius = 2.0f;
    float start = 0.0f, impact = 0.0f;
    std::vector<Target> bodies;
    uint32_t seed = 0;
    float end() const;
  };
  struct Standing {
    Stand kind = Stand::Oath;
    int id = 0;
    glm::vec3 feet{0.0f};
    float height = 1.8f;
    float start = 0.0f, finish = -1.0f;  // finish: when it was ended (-1 = standing)
    uint32_t seed = 0;
  };
  void buildBlade(const Effect& e, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights);
  void buildImpact(const Effect& e, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights);
  void buildTouch(const Effect& e, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights);
  void buildMantle(const Effect& e, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights);
  void buildStanding(const Standing& s, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights);

  std::vector<Effect> effects_;
  std::vector<Standing> stands_;
  uint32_t serial_ = 0;
  int standSerial_ = 0;
  uint32_t quadFirst_ = 0, sphereFirst_ = 0, discFirst_ = 0, columnFirst_ = 0, haloFirst_ = 0;
  uint32_t quadCount_ = 0, sphereCount_ = 0, discCount_ = 0, columnCount_ = 0, haloCount_ = 0;
  std::vector<VfxInstance> streaks_, motes_;
};

}  // namespace ember
