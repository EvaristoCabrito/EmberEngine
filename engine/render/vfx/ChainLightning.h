#pragma once

#include <cstdint>
#include <vector>
#include <glm/glm.hpp>

#include "render/Camera.h"
#include "render/VfxDraw.h"

namespace ember {

// Cadeia de Raios 02, in full 3D: an electric cascade arcing from the caster to the first target,
// then branching and leaping from enemy to enemy across the battlefield with crackling 3D electric tubes,
// branching forks, body arcs, ground shockwaves, flying sparks, and intense dynamic lighting.
class ChainLightning {
 public:
  struct Target {
    glm::vec3 feet{0.0f};
    glm::vec3 body{0.0f};
    float height = 1.8f;
  };

  ChainLightning(glm::vec3 casterFeet, glm::vec3 casterBody, float casterHeight,
                 std::vector<Target> chain, float start, float impact, uint32_t seed);

  float end() const { return impact_ + kStagger * static_cast<float>(chain_.size()) + 0.85f; }
  bool done(float now) const { return now >= end(); }
  void build(float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) const;

  static constexpr float kStagger = 0.12f;

 private:
  void buildCaster(float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) const;
  void buildArc(size_t index, float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) const;

  glm::vec3 casterFeet_{0.0f};
  glm::vec3 casterBody_{0.0f};
  float casterHeight_ = 2.0f;
  std::vector<Target> chain_;
  float start_ = 0.0f;
  float impact_ = 0.0f;
  uint32_t seed_ = 0;
};

}  // namespace ember
