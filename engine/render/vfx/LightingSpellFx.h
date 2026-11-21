#pragma once

#include <functional>
#include <optional>
#include <string>

#include <glm/glm.hpp>

#include "render/Camera.h"
#include "render/VfxDraw.h"

namespace ember {

// Appends persistent/sustained 3D visual effects for all active lighting spells
// (Feixe de Luz beam cone, Luz Ancorada luminous sphere, Halo Luminoso aura/halo) to the frame.
void appendLightingSpellVfx(
    VfxFrame& frame,
    float time,
    const Camera& camera,
    const std::string& currentFloor,
    const std::string& currentLogicalMap,
    int64_t currentMinute,
    const std::function<std::optional<glm::vec3>(const std::string& casterId)>& getCasterPos);

}  // namespace ember
