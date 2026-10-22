#include "render/vfx/AcidCloud.h"

#include <algorithm>
#include <cmath>

#include <glm/gtc/matrix_transform.hpp>

#include "render/vfx/ThreeMath.h"
#include "render/VfxGeometry.h"

namespace ember {
namespace {

float hash(uint32_t a, uint32_t b, uint32_t c) {
  uint32_t h = a * 0x9e3779b9u ^ b * 0x85ebca6bu ^ c * 0xc2b2ae35u;
  h ^= h >> 16, h *= 0x7feb352du, h ^= h >> 15, h *= 0x846ca68bu, h ^= h >> 16;
  return static_cast<float>(h & 0xffffffu) / 16777216.0f;
}

const std::vector<VfxVertex>& sphere() {
  static const std::vector<VfxVertex> mesh = vfxgeo::sphere(1.0f, 16, 10);
  return mesh;
}

}  // namespace

AcidCloud::AcidCloud(std::vector<glm::vec3> hexes, float start) : hexes_(std::move(hexes)), start_(start) {}

void AcidCloud::build(float now, const Camera&, VfxFrame& frame, std::vector<VfxLight>& lights) const {
  if (now < start_ || done(now) || hexes_.empty()) return;
  const float age = now - start_;
  const float fade = std::min(1.0f, std::max(0.0f, (end_ - now) / 0.8f));
  std::vector<VfxInstance> vapor, bubbles, splashes;
  glm::vec3 centre{0.0f};
  for (size_t i = 0; i < hexes_.size(); ++i) {
    const glm::vec3 h = hexes_[i];
    centre += h;
    const uint32_t cell = static_cast<uint32_t>(i);
    // Overlapping, shallow vapor masses hug the ground and billow slowly across adjacent cells.
    for (uint32_t j = 0; j < 4; ++j) {
      const float a = hash(cell, j, 1) * 6.2831853f + age * (0.08f + hash(cell, j, 2) * 0.12f);
      const float r = 0.12f + hash(cell, j, 3) * 0.48f;
      const float pulse = 0.86f + 0.14f * std::sin(age * (0.7f + hash(cell, j, 4)) + j * 1.9f + cell);
      const glm::vec3 p = h + glm::vec3(std::cos(a) * r, 0.16f + hash(cell, j, 5) * 0.22f, std::sin(a) * r);
      const glm::vec3 scale((0.34f + hash(cell, j, 6) * 0.32f) * pulse, 0.15f + hash(cell, j, 7) * 0.12f,
                            (0.3f + hash(cell, j, 8) * 0.28f) * pulse);
      VfxInstance puff;
      puff.matrix = glm::translate(glm::mat4(1.0f), p) * three::eulerXYZ(0.0f, a, 0.0f) * glm::scale(glm::mat4(1.0f), scale);
      puff.c = glm::vec4(0.35f + hash(cell, j, 9) * 0.12f, 0.68f + hash(cell, j, 10) * 0.18f, 0.12f, 1.0f);
      vapor.push_back(puff);
    }
    // Glossy bubbles surface at different points and burst into short upward droplets.
    for (uint32_t j = 0; j < 3; ++j) {
      const float phase = hash(cell, j, 11) * 2.4f;
      const float cycle = std::fmod(age * (0.28f + hash(cell, j, 12) * 0.16f) + phase, 1.0f);
      const float a = hash(cell, j, 13) * 6.2831853f;
      const float r = 0.08f + hash(cell, j, 14) * 0.48f;
      const glm::vec3 base = h + glm::vec3(std::cos(a) * r, 0.06f, std::sin(a) * r);
      VfxInstance bubble;
      const float radius = 0.055f + 0.055f * std::sin(cycle * 3.1415926f);
      bubble.matrix = glm::translate(glm::mat4(1.0f), base) * glm::scale(glm::mat4(1.0f), glm::vec3(radius, radius * 0.72f, radius));
      bubble.c = {0.65f, 0.9f, 0.22f, 1.0f};
      bubbles.push_back(bubble);
      if (cycle < 0.18f) {
        const float u = cycle / 0.18f;
        VfxInstance drop;
        const glm::vec3 at = base + glm::vec3(0.04f * std::cos(a), 0.04f + 0.32f * u, 0.04f * std::sin(a));
        const float size = 0.035f * (1.0f - 0.45f * u);
        drop.matrix = glm::translate(glm::mat4(1.0f), at) * glm::scale(glm::mat4(1.0f), glm::vec3(size, size * 1.4f, size));
        drop.c = {0.72f, 0.98f, 0.28f, 1.0f};
        splashes.push_back(drop);
      }
    }
  }
  centre /= static_cast<float>(hexes_.size());
  auto draw = [&](std::vector<VfxInstance>& items, glm::vec3 color, float alpha, float emissive) {
    if (items.empty()) return;
    const uint32_t first = static_cast<uint32_t>(frame.instances.size());
    frame.instances.insert(frame.instances.end(), items.begin(), items.end());
    VfxDraw d;
    d.gpu.model = glm::mat4(1.0f);
    d.gpu.params0 = {0.38f, 0.03f, 0.0f, 0.0f};
    d.gpu.params1 = glm::vec4(three::hex(0x91d943), emissive * fade);
    d.gpu.color = glm::vec4(color, alpha * fade);
    d.gpu.firstVertex = frame.addVertices(sphere());
    d.gpu.vertexCount = static_cast<uint32_t>(sphere().size());
    d.gpu.firstInstance = first;
    d.gpu.material = static_cast<uint32_t>(VfxMaterial::Standard);
    d.gpu.sightAt = centre;
    d.instances = static_cast<uint32_t>(items.size());
    d.blend = VfxBlend::Alpha;
    d.depthTest = true;
    frame.draws.push_back(d);
  };
  draw(vapor, {0.3f, 0.52f, 0.12f}, 0.34f, 0.1f);
  draw(bubbles, {0.48f, 0.72f, 0.16f}, 0.92f, 0.32f);
  draw(splashes, {0.56f, 0.82f, 0.2f}, 0.96f, 0.5f);
  lights.push_back({centre + glm::vec3(0.0f, 0.32f, 0.0f), {0.24f, 0.65f, 0.08f}, 1.2f * fade, 1.7f + std::sqrt(static_cast<float>(hexes_.size()))});
}

}  // namespace ember
