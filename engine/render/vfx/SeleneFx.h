#pragma once

// Selene's Lâmina Arcana (docs/SKILLS_CONJURER.md, 2026-10-26), ours, in 3D on the effect runtime (the Holy and Combat materials in
// violet: streaks, pools, rings, halos, motes, energy shells), one look, the same on every cast:
//
//   the blade      her dagger lights up violet before the blow (a glow along it toward the foe, motes drawn into it, a cold light);
//   the mark       where the blow lands, a violet cut-flash and the rune drawn on the ground under the foe (a ring closing in, sparks);
//   standing       while the rules keep the mark (MapStage syncArcaneMarks): a rune circle at the foe's feet, slowly turning, six
//                  points of light running round it, a faint violet band about its chest, pulsing;
//   the burst      when the marked foe walks: the rune flares, a violet shell bursts out of the body, shards fly, a ring runs over
//                  the ground, a hard violet light.
//
// World space (y up). Points on a body are pulled toward the eye (kPull) so they show over the card, as CrusaderFx does.

#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "render/Camera.h"
#include "render/VfxDraw.h"

namespace ember {

class SeleneFx {
 public:
  struct Target {
    glm::vec3 feet{0.0f};
    float height = 1.8f;  // the card's height (0 = not known: a body's height is assumed)
  };
  // Seconds the rune flares before the burst's hit lands (the blow's lands on the pose's own impact).
  static float leadIn(const std::string& kind);

  // Lâmina Arcana: the blade's glow from `start` to the blow at `impact` (Selene at `from`, the foe at `toward`) ...
  void bladeGlow(glm::vec3 from, float fromHeight, glm::vec3 toward, float start, float impact);
  // ... and, when the blow landed, the mark laid on the struck body.
  void markImpact(const Target& struck, glm::vec3 from, float at);
  // The mark going off on `marked` (its hit lands at `impact`).
  void burst(const Target& marked, float impact);

  // The mark standing on a unit for as long as the rules keep it. standStart() returns its id.
  int standStart(glm::vec3 feet, float height, float now);
  void standMove(int id, glm::vec3 feet, float height);
  void standEnd(int id, float now);

  bool active(float now) const;
  float latestEnd() const;
  void clear() { effects_.clear(), stands_.clear(); }
  void build(float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights);

 private:
  enum class Kind { Blade, Mark, Burst };
  struct Effect {
    Kind kind;
    glm::vec3 from{0.0f}, at{0.0f};  // Selene's feet; the struck / marked body's feet (the blade: the foe's)
    float fromHeight = 1.8f, height = 1.8f;
    float start = 0.0f, impact = 0.0f;
    uint32_t seed = 0;
    float end() const;
  };
  struct Standing {
    int id = 0;
    glm::vec3 feet{0.0f};
    float height = 1.8f;
    float start = 0.0f, finish = -1.0f;  // finish: when it was ended (-1 = standing)
    uint32_t seed = 0;
  };
  void buildBlade(const Effect& e, float now, const Camera& camera, std::vector<VfxLight>& lights);
  void buildMark(const Effect& e, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights);
  void buildBurst(const Effect& e, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights);
  void buildStanding(const Standing& s, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights);

  std::vector<Effect> effects_;
  std::vector<Standing> stands_;
  uint32_t serial_ = 0;
  int standSerial_ = 0;
  uint32_t quadFirst_ = 0, sphereFirst_ = 0, discFirst_ = 0, haloFirst_ = 0;
  uint32_t quadCount_ = 0, sphereCount_ = 0, discCount_ = 0, haloCount_ = 0;
  std::vector<VfxInstance> streaks_, motes_;
};

}  // namespace ember
