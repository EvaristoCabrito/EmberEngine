#pragma once

// The atmosphere's constants and frame record (shaders/atmosphere.hlsli, atmosphere.hlsl; ARCHITECTURE §7.5):
// Hillaire 2020 with Earth's air (the paper's and Unreal's defaults: Rayleigh, Mie, ozone), distances in km.

#include <glm/glm.hpp>

#include <cmath>
#include <cstdint>

namespace ember {

// Matches AtmosphereFrame in shaders/scene.hlsli. 336 bytes.
struct AtmosphereFrame {
  glm::mat4 inverseViewProjection{1.0f};
  glm::vec3 rayleighScattering{5.802e-3f, 13.558e-3f, 33.1e-3f};
  float rayleighScaleHeight = 8.0f;
  glm::vec3 mieScattering{3.996e-3f};
  float mieScaleHeight = 1.2f;
  glm::vec3 mieExtinction{4.440e-3f};
  float mieG = 0.8f;
  glm::vec3 ozoneAbsorption{0.650e-3f, 1.881e-3f, 0.085e-3f};
  float ozoneCenter = 25.0f;
  glm::vec3 groundAlbedo{0.3f};
  float ozoneWidth = 30.0f;
  glm::vec3 sunIlluminance{1.0f};
  float bottomRadius = 6360.0f;
  glm::vec3 sunDirection{0.0f, 1.0f, 0.0f};
  float topRadius = 6460.0f;
  float viewHeight = 6360.1f;
  float kmPerUnit = 0.08f;     // aerial perspective: kilometres of air per world unit (artistic: a board is ~50 m)
  float aerialMaxKm = 32.0f;
  float ambientTarget = 1.0f;
  uint32_t transmittance = 0, transmittanceStorage = 0, multiScattering = 0, multiScatteringStorage = 0;
  uint32_t skyView = 0, skyViewStorage = 0, aerial = 0, aerialStorage = 0;
  uint64_t ambient = 0;
  uint32_t linearSampler = 0;
  float skyScale = 1.0f;
  glm::vec3 moonDirection{0.0f, 1.0f, 0.0f};
  uint32_t moonTexture = 0xFFFFFFFFu;  // kNoTextureIndex: no disc
  float moonTanRadius = 0.0f;
  float moonGlow = 0.0f;
  float stars = 0.0f;
  uint32_t sunDisc = 1;
  uint32_t starLook = 3;
  // Nuvens básicas (shaders/clouds.hlsli): the cloud sheet's numbers (Renderer::Clouds); MeshFrame.cloudFlags switches it.
  float cloudCoverage = 0.0f;
  float cloudAltitude = 0.0f;
  float cloudShadow = 0.0f;
  float cloudScale = 1.0f;
  glm::vec2 cloudWind{0.0f};
  uint32_t pad0 = 0;
  // The cloud shadow map over the board (Renderer cloudShadow_): world rect, sampled index (none = per-surface cover).
  glm::vec4 cloudShadowRect{0.0f};
  uint32_t cloudShadowMap = 0xFFFFFFFFu;
  float sunTanRadius = 0.0f;  // the sun's disc: tan of its angular radius (0 = the real 0.26 deg, no corona)
  uint32_t pad2 = 0, pad3 = 0;
};
static_assert(sizeof(AtmosphereFrame) == 336);

// The air's transmittance from the ground to its top towards `direction` (world, +y up), on the CPU (the same
// integral as CSTransmittance): the colour the sun or moon arrives with.
inline glm::vec3 atmosphereTransmittance(const AtmosphereFrame& a, glm::vec3 direction) {
  const glm::vec3 o{0.0f, a.viewHeight, 0.0f}, d = glm::normalize(direction);
  const float b = glm::dot(o, d), c = glm::dot(o, o) - a.topRadius * a.topRadius;
  const float span = -b + std::sqrt(std::max(b * b - c, 0.0f));
  glm::vec3 depth{0.0f};
  constexpr int kSteps = 64;
  for (int i = 0; i < kSteps; ++i) {
    const glm::vec3 p = o + d * ((i + 0.5f) / kSteps * span);
    const float h = std::max(0.0f, glm::length(p) - a.bottomRadius);
    const float o3 = std::max(0.0f, 1.0f - std::abs(h - a.ozoneCenter) / (a.ozoneWidth * 0.5f));
    depth += (a.rayleighScattering * std::exp(-h / a.rayleighScaleHeight) + a.mieExtinction * std::exp(-h / a.mieScaleHeight) +
              a.ozoneAbsorption * o3) *
             (span / kSteps);
  }
  return glm::exp(-depth);
}

}  // namespace ember
