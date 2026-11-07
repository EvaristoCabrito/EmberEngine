#pragma once

#include <algorithm>
#include <cmath>
#include <numbers>
#include <unordered_map>
#include <vector>

#include "render/VfxDraw.h"

namespace ember {

// A short, self-contained presentation of the stitch. Timing here is visual only.
class WitchStitchFx {
 public:
  void standStart(int id, glm::vec3 anchor, glm::vec3 target, float height, float now) {
    stands_[id] = {anchor, target, std::clamp(height, 1.0f, 4.0f), now, -1.0f};
  }
  void standMove(int id, glm::vec3 anchor, glm::vec3 target, float height) {
    if (auto it = stands_.find(id); it != stands_.end()) it->second.anchor = anchor, it->second.target = target, it->second.height = std::clamp(height, 1.0f, 4.0f);
  }
  void standEnd(int id, float now, bool rupture) {
    if (auto it = stands_.find(id); it != stands_.end()) it->second.end = now, it->second.rupture = rupture;
  }
  void cast(glm::vec3 anchor, float height, float impact) {
    effects_.push_back({anchor, std::clamp(height, 1.0f, 4.0f), impact});
  }
  void clear() { effects_.clear(); stands_.clear(); }
  bool active(float now) const {
    return !stands_.empty() || std::any_of(effects_.begin(), effects_.end(), [now](const Effect& e) { return now < e.impact + 2.1f; });
  }
  void build(float now, VfxFrame& frame, std::vector<VfxLight>& lights) {
    std::erase_if(effects_, [now](const Effect& e) { return now >= e.impact + 2.1f; });
    std::erase_if(stands_, [now](const auto& pair) { return pair.second.end >= 0.0f && now >= pair.second.end + (pair.second.rupture ? 0.42f : 0.7f); });
    for (const auto& [id, s] : stands_) {
      (void)id;
      const float fade = s.end < 0.0f ? 0.78f : std::clamp((s.end + (s.rupture ? 0.42f : 0.7f) - now) / (s.rupture ? 0.42f : 0.7f), 0.0f, 1.0f);
      const float pulse = 0.5f + 0.5f * std::sin(now * (s.rupture ? 26.0f : 4.0f));
      glm::vec3 a = s.anchor + glm::vec3(0.0f, 0.06f, 0.0f);
      glm::vec3 b = s.target + glm::vec3(0.0f, s.height * (0.48f + pulse * 0.035f), 0.0f);
      if (s.end >= 0.0f && s.rupture) b = a + (b - a) * (1.0f + pulse * 0.28f);
      appendStrand(a, b, 0.010f + pulse * 0.003f, fade, s.anchor, frame, lights);
    }
    for (const Effect& e : effects_) {
      const float t = now - e.impact;
      if (t < 0.0f) continue;
      const float fade = std::clamp((2.1f - t) / 0.5f, 0.0f, 1.0f);
      const float arrival = std::clamp(t / 0.18f, 0.0f, 1.0f);
      const glm::vec3 a = e.anchor + glm::vec3(0.12f, 0.025f, 0.08f);
      const glm::vec3 b = e.anchor + glm::vec3(0.0f, e.height * 0.57f * arrival, 0.0f);
      const glm::vec3 axis = b - a;
      if (glm::length(axis) < 0.001f) continue;
      glm::vec3 side = glm::cross(glm::normalize(axis), glm::vec3(0, 0, 1));
      if (glm::length(side) < 0.001f) side = glm::vec3(1, 0, 0);
      side = glm::normalize(side);
      const glm::vec3 other = glm::normalize(glm::cross(glm::normalize(axis), side));
      const float radius = 0.012f + 0.003f * std::sin(t * 9.0f);
      std::vector<VfxVertex> mesh;
      constexpr int segments = 8;
      for (int i = 0; i < segments; ++i) {
        const float x = 2.0f * std::numbers::pi_v<float> * i / segments;
        const float y = 2.0f * std::numbers::pi_v<float> * (i + 1) / segments;
        const glm::vec3 n0 = side * std::cos(x) + other * std::sin(x);
        const glm::vec3 n1 = side * std::cos(y) + other * std::sin(y);
        const VfxVertex a0{a + n0 * radius, 0, n0, 0}, a1{a + n1 * radius, 1, n1, 0};
        const VfxVertex b0{b + n0 * radius, 0, n0, 1}, b1{b + n1 * radius, 1, n1, 1};
        mesh.insert(mesh.end(), {a0, b0, a1, a1, b0, b1});
      }
      VfxDraw draw;
      draw.gpu.model = glm::mat4(1.0f);
      draw.gpu.color = glm::vec4(0.66f, 0.025f, 0.04f, fade);
      draw.gpu.firstVertex = frame.addVertices(mesh);
      draw.gpu.vertexCount = static_cast<uint32_t>(mesh.size());
      draw.gpu.firstInstance = 0xFFFFFFFFu;
      draw.gpu.material = static_cast<uint32_t>(VfxMaterial::Basic);
      draw.gpu.sightAt = e.anchor;
      draw.depthTest = true;
      draw.blend = VfxBlend::Additive;
      frame.draws.push_back(draw);
      lights.push_back({a, {0.65f, 0.025f, 0.035f}, fade * (1.2f + 1.4f * std::exp(-t * 5.0f)), 1.8f});
    }
  }
 private:
  struct Stand { glm::vec3 anchor, target; float height, start, end; bool rupture = false; };
  struct Effect { glm::vec3 anchor; float height, impact; };
  static void appendStrand(glm::vec3 a, glm::vec3 b, float radius, float fade, glm::vec3 sight, VfxFrame& frame, std::vector<VfxLight>& lights) {
    const glm::vec3 axis = b - a;
    if (glm::length(axis) < 0.001f || fade <= 0.0f) return;
    glm::vec3 side = glm::cross(glm::normalize(axis), glm::vec3(0, 0, 1));
    if (glm::length(side) < 0.001f) side = glm::vec3(1, 0, 0);
    side = glm::normalize(side);
    const glm::vec3 other = glm::normalize(glm::cross(glm::normalize(axis), side));
    std::vector<VfxVertex> mesh;
    constexpr int segments = 8;
    for (int i = 0; i < segments; ++i) {
      const float x = 2.0f * std::numbers::pi_v<float> * i / segments, y = 2.0f * std::numbers::pi_v<float> * (i + 1) / segments;
      const glm::vec3 n0 = side * std::cos(x) + other * std::sin(x), n1 = side * std::cos(y) + other * std::sin(y);
      const VfxVertex a0{a + n0 * radius, 0, n0, 0}, a1{a + n1 * radius, 1, n1, 0};
      const VfxVertex b0{b + n0 * radius, 0, n0, 1}, b1{b + n1 * radius, 1, n1, 1};
      mesh.insert(mesh.end(), {a0, b0, a1, a1, b0, b1});
    }
    VfxDraw draw; draw.gpu.model = glm::mat4(1.0f); draw.gpu.color = glm::vec4(0.66f, 0.025f, 0.04f, fade);
    draw.gpu.firstVertex = frame.addVertices(mesh); draw.gpu.vertexCount = static_cast<uint32_t>(mesh.size()); draw.gpu.firstInstance = 0xFFFFFFFFu;
    draw.gpu.material = static_cast<uint32_t>(VfxMaterial::Basic); draw.gpu.sightAt = sight; draw.depthTest = true; draw.blend = VfxBlend::Additive;
    frame.draws.push_back(draw); lights.push_back({a, {0.65f, 0.025f, 0.035f}, fade * 0.4f, 1.3f});
  }
  std::vector<Effect> effects_;
  std::unordered_map<int, Stand> stands_;
};
}  // namespace ember
