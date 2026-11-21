#pragma once

#include <cstdint>
#include <vector>
#include <glm/glm.hpp>

#include "render/Camera.h"
#include "render/VfxDraw.h"

namespace ember {

// Brasa Primordial 02, in full 3D: pre-impact ground heating and rising embers across all target hexes,
// followed at impact by a colossal central primordial fire pillar (VfxMaterial::FireVolume body 0 & 2),
// expanding fiery shockwave rings, hex-by-hex erupting flame plumes, flying spark fountains,
// rolling smoke, scorched ground rings, and intense incandescent dynamic lighting.
class PrimordialEmber {
 public:
  struct HexSpot {
    glm::vec3 feet{0.0f};
    glm::vec3 body{0.0f};
    float cardHeight = 0.0f;
  };

  PrimordialEmber(glm::vec3 casterFeet, glm::vec3 casterBody, float casterHeight,
                  std::vector<HexSpot> hexes, float start, float impact, uint32_t seed);

  float end() const { return impact_ + duration_; }
  bool done(float now) const { return now >= end(); }
  void build(float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) const;

 private:
  struct Plume {
    glm::vec3 foot;
    float delay = 0.0f;
    float life = 1.3f;
    float height = 2.4f;
    float radius = 0.6f;
    float seed = 0.0f;
  };

  struct Particle {
    int kind = 1;  // 1 = ember/spark (ImpactParticle procedural flame), 2 = smoke
    float at = 0.0f;
    float life = 1.0f;
    float size = 0.1f;
    float spin = 0.0f, rotation = 0.0f;
    glm::vec3 from{0.0f}, velocity{0.0f};
    float gravity = 0.0f, drag = 0.0f;
    glm::vec3 color{1.0f};
  };

  void buildCaster(float now, const Camera& camera, VfxFrame& frame, std::vector<VfxLight>& lights) const;

  glm::vec3 casterFeet_{0.0f};
  glm::vec3 casterBody_{0.0f};
  float casterHeight_ = 2.0f;
  std::vector<HexSpot> hexes_;
  glm::vec3 centre_{0.0f};
  float reach_ = 1.0f;
  float start_ = 0.0f;
  float impact_ = 0.0f;
  float duration_ = 2.5f;
  uint32_t seed_ = 0;

  std::vector<Plume> plumes_;
  std::vector<Particle> particles_;
};

}  // namespace ember
