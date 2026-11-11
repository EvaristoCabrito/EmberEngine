#pragma once

// Bolts that finish what a unit's own art begins (the user, 2026-11-10: "we used to have a webgl2D effect that would perfectly complete
// her bolt ... do something similar for her in 3D? the Telesm will also need this type of effect"). Ours, in 3D, on the Combat
// material's lightning (shaders/vfx.hlsl mode 6, as vfx/Lightning), after Engine2's ApparitionLightningV1 (vfx/legacy/two-d):
//
//   the charge   while the glow painted in the art gathers at the hands, small jagged arcs crackle across it, a new shape every 1/24 s,
//                brighter as the release nears, a cold light at the hands;
//   the release  on the frame the art lets go, a forked bolt runs from the hands to the struck body's chest (lit from the hands out
//                in kFlight seconds), flickers between shapes while it holds and fades;
//   the hit      a flash on the body, sparks flung out, a small ring over the ground, a dazzling light dying away.
//
// Where the hands are comes from the art: a track of points per frame of the pose (content/fx/hand-bolts.json, measured from the
// frames' glow), placed on the unit's card as drawn this frame (follow(): the stage passes the card every frame, so the arcs stay on
// the hands when the camera turns or the card is mirrored). One look per unit: the Aparição's blue-white, Telesm's cyan.

#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "render/Camera.h"
#include "render/VfxDraw.h"

namespace ember {

class HandBoltFx {
 public:
  enum class Look { Apparition, Telesm };
  // A unit's card as the stage draws it (MapStage addCard): its anchor (the feet line sits `foot` below it), the card's right and up,
  // its width and height, the sideways hold (bodyShift) and whether it is mirrored.
  struct Card {
    glm::vec3 anchor{0.0f}, right{1.0f, 0.0f, 0.0f}, up{0.0f, 1.0f, 0.0f};
    float width = 1.0f, height = 2.0f, foot = 0.0f, drift = 0.0f;
    bool mirror = false;
  };
  // The pose's glow per frame (full-image uv as drawn facing right, v from the top), the frames the charge starts and lets go on (1-based).
  struct Track {
    Look look = Look::Apparition;
    std::vector<glm::vec2> hands;
    int chargeFrame = 1, releaseFrame = 1;
  };
  // Seconds from the release to the bolt reaching the body.
  static constexpr float kFlight = 0.1f;
  // How far into the pose the bolt lets go (a share of its length).
  static float releaseShare(const Track& t) {
    return t.hands.empty() ? 0.5f : static_cast<float>(t.releaseFrame - 1) / static_cast<float>(t.hands.size());
  }

  // The pose of scene unit `unit` plays from `start` for `length` seconds; the bolt lands on the body (`targetFeet`, card height
  // `targetHeight`) at `impact` (the stage's blow: start + length x releaseShare + the flight).
  void start(int unit, const Track& track, const Card& card, glm::vec3 targetFeet, float targetHeight, float start, float length, float impact,
             uint32_t seed);
  // The unit's card this frame (every effect of that unit follows it).
  void follow(int unit, const Card& card);

  bool active(float now) const;
  float latestEnd() const;
  void clear() { effects_.clear(); }
  void build(float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights);

 private:
  struct Effect {
    int unit = -1;
    Track track;
    Card card;
    glm::vec3 targetFeet{0.0f};
    float targetHeight = 1.8f, start = 0.0f, length = 1.0f, release = 0.0f, impact = 0.0f;
    uint32_t seed = 0;
    float end() const { return impact + 0.7f; }
  };
  void buildCharge(const Effect& e, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) const;
  void buildBolt(const Effect& e, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) const;
  std::vector<Effect> effects_;
};

}  // namespace ember
