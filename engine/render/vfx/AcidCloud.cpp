#include "render/vfx/AcidCloud.h"

#include <algorithm>
#include <cmath>

#include <glm/gtc/matrix_transform.hpp>

#include "render/vfx/ThreeMath.h"
#include "render/VfxGeometry.h"

namespace ember {

AcidCloud::AcidCloud(std::vector<glm::vec3> hexes, float start) : hexes_(std::move(hexes)), start_(start) {}

void AcidCloud::build(float now, const Camera&, VfxFrame&, std::vector<VfxLight>& lights) const {
  if (now < start_ || done(now) || hexes_.empty()) return;
  const float fade = std::min(1.0f, std::max(0.0f, (end_ - now) / 0.8f));
  // No shapes of its own (the user, 2026-11-11: "tem um efeito duplo de poison, só quero o efeito normal com aparência normal,
  // remover essas bolotas"): whoever stands in it wears the normal poison (StatusFx); the field keeps only its green light.
  glm::vec3 centre{0.0f};
  for (const glm::vec3& h : hexes_) centre += h;
  centre /= static_cast<float>(hexes_.size());
  lights.push_back({centre + glm::vec3(0.0f, 0.32f, 0.0f), {0.24f, 0.65f, 0.08f}, 1.2f * fade, 1.7f + std::sqrt(static_cast<float>(hexes_.size()))});
}

}  // namespace ember
