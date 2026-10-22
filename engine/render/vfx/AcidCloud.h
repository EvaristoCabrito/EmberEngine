#pragma once

#include <algorithm>
#include <vector>

#include <glm/glm.hpp>

#include "render/Camera.h"
#include "render/VfxDraw.h"

namespace ember {

// A low, persistent 3D acid field: lit vapor lobes, surface bubbles and short splashes.
class AcidCloud {
 public:
  AcidCloud(std::vector<glm::vec3> hexes, float start);
  bool done(float now) const { return now >= end_; }
  void stop(float now) { end_ = std::min(end_, now + 0.8f); }
  void build(float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) const;
  int id = 0;

 private:
  std::vector<glm::vec3> hexes_;
  float start_ = 0.0f, end_ = 1.0e9f;
};

}  // namespace ember
