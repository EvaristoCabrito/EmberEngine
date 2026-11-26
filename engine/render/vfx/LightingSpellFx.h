#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "render/Camera.h"
#include "render/VfxDraw.h"

namespace ember {

// Pure 3D cast animation effects (zero 2D flipbooks, decals, or billboards) for
// Feixe de Luz (arcaneBeam), Luz Ancorada (anchoredLight), and Halo Luminoso (luminousHalo).
class LightingSpellCastFx {
 public:
  void cast(const std::string& kind, int look, glm::vec3 casterFeet, float casterHeight,
            glm::vec3 targetFeet, float start, float impact);
  bool active(float now) const;
  float latestEnd() const;
  void clear() { casts_.clear(); }
  void build(float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights);

 private:
  struct Cast {
    std::string kind;
    int look = 2;
    int power = 1;
    glm::vec3 casterFeet{0.0f};
    float casterHeight = 2.0f;
    glm::vec3 origin{0.0f};
    glm::vec3 targetPos{0.0f};
    glm::vec3 facing{0.0f, 0.0f, 1.0f};
    float range = 14.0f;
    float coneAngleDeg = 70.0f;
    float start = 0.0f;
    float impact = 0.0f;
    float end = 0.0f;
    uint32_t seed = 0;
  };
  std::vector<Cast> casts_;
  uint32_t seedSerial_ = 0;
};

// Appends persistent/sustained 3D visual effects for all active lighting spells
// (Feixe de Luz volumetric cone, Luz Ancorada luminous core, Halo Luminoso 3D motes/halo) to the frame.
void appendLightingSpellVfx(
    VfxFrame& frame,
    float time,
    const Camera& camera,
    const std::string& currentFloor,
    const std::string& currentLogicalMap,
    int64_t currentMinute,
    const std::function<std::optional<glm::vec3>(const std::string& casterId)>& getCasterPos,
    const std::function<std::optional<glm::vec3>(const std::string& casterId)>& getCasterHead = nullptr);

}  // namespace ember
