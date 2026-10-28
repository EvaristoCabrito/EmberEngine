#pragma once

// Tempestade de Gelo (Ember's iceStorm) on the effect runtime, two versions on one timeline: Ember's Blizzard2D's (9 s,
// gathering over 1.2 s, fading over its last 1.6 s; 95 motes per hex x the battle's density 1.3, a fixed seed so every
// storm looks the same; a spike lands, shatters and pulses one of three cold lights, ThreeBattleRenderer
// pulseBlizzardLight). Drawn with the IceStorm material (shaders/vfx.hlsl); world space, one hex radius = 1.
//   01  Ember's own Blizzard2D (src/game/gfx/Blizzard2D.ts), its sprites stood in the board where its canvas drew them:
//       wind wisps (blizzard-wind-reference), falling ice spikes and their shattering (blizzard-impact-reference), snow
//       dots, the frosted floor. The enemies' (the user, 2026-10-18: "deixa o 2D para os inimigos").
//   02  ours, in 3D (the heroes'): the floor frosts over from each hex's middle, bands of wind turn round the field, snow
//       swirls down through them, ice crystals (meshes) fall slanted by the gusts and shatter into splinters, a frost
//       ring and a puff of ice dust, under the same pulsing light.
// The rules keep the field for rounds; the storm plays its 9 s (Ember) and fades early when the rules drop it (stop()). In a
// battle it stands (stand()) for as long as the rules keep the field (the user, 2026-10-21: it vanished while still active), and
// fades only when they drop it.

#include <algorithm>
#include <array>
#include <limits>
#include <vector>

#include <glm/glm.hpp>

#include "render/Camera.h"
#include "render/VfxDraw.h"

namespace ember {

class IceStorm {
 public:
  // hexes: each hex of the field, its ground (feet). The storm starts at `start`.
  IceStorm(int version, std::vector<glm::vec3> hexes, float start);

  static constexpr float kDuration = 9.0f, kGather = 1.2f, kFade = 1.6f, kStopFade = 1.0f;
  int version() const { return version_; }
  float start() const { return start_; }
  float end() const { return end_; }
  bool done(float now) const { return now >= end_; }
  void stop(float now) {
    stopped_ = true;
    end_ = std::min(end_, now + kStopFade);
  }
  void stand() { standing_ = true, end_ = std::numeric_limits<float>::max(); }
  int id = 0;

  // ice / wind: Ember's two sheets (version 01 only; nothing is drawn by it until they are resident).
  void build(float now, const Camera& camera, const VfxFlipbook& ice, const VfxFlipbook& wind, VfxFrame& frame, std::vector<VfxLight>& lights);

 private:
  struct Mote {
    int cell;
    float dx, dy;
    int layer, kind;  // 0 wind, 1 spike, 2 snow, 3 a spike's impact
    float phase, speed, size, turn;
    int lastCycle = -1;
  };
  struct Pulse {
    glm::vec3 at{0.0f};
    float time = -100.0f, strength = 0.0f;
  };
  void buildClassic(float age, float env, glm::vec3 right, const VfxFlipbook& ice, const VfxFlipbook& wind, VfxFrame& frame);
  void buildOurs(float now, float age, float env, glm::vec3 right, VfxFrame& frame);

  int version_ = 2;
  std::vector<glm::vec3> hexes_;
  glm::vec3 centre_{0.0f};
  float radius_ = 1.0f;  // the field's reach from its centre
  float start_ = 0.0f, end_ = 0.0f;
  bool standing_ = false, stopped_ = false;
  std::vector<Mote> motes_;
  std::array<Pulse, 3> pulses_{};
};

}  // namespace ember
