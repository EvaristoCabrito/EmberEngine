#include "render/vfx/LightingSpellFx.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

#include <glm/gtc/matrix_transform.hpp>

#include "render/VfxGeometry.h"
#include "world/LightingSpells.h"

namespace ember {

namespace {

constexpr float kPiF = std::numbers::pi_v<float>;
constexpr float kTau = 2.0f * kPiF;
constexpr float kModeStreak = 0.0f;
constexpr float kModeEnergy = 2.0f;

const std::vector<VfxVertex>& quadGeometry() {
  static const std::vector<VfxVertex> g = {
      {{-0.5f, -0.5f, 0.0f}, 0.0f, {0, 0, 1}, 0.0f}, {{0.5f, -0.5f, 0.0f}, 1.0f, {0, 0, 1}, 0.0f}, {{0.5f, 0.5f, 0.0f}, 1.0f, {0, 0, 1}, 1.0f},
      {{-0.5f, -0.5f, 0.0f}, 0.0f, {0, 0, 1}, 0.0f}, {{0.5f, 0.5f, 0.0f}, 1.0f, {0, 0, 1}, 1.0f}, {{-0.5f, 0.5f, 0.0f}, 0.0f, {0, 0, 1}, 1.0f}};
  return g;
}

const std::vector<VfxVertex>& sphereGeometry() {
  static const std::vector<VfxVertex> g = vfxgeo::sphere(1.0f, 20, 14);
  return g;
}

VfxInstance mote(glm::vec3 centre, float width, float alpha, glm::vec3 tint, float intensity) {
  return {glm::mat4(1.0f), glm::vec4(centre, width), glm::vec4(0.0f, 0.0f, 0.0f, alpha), glm::vec4(tint, intensity)};
}

}  // namespace

void appendLightingSpellVfx(
    VfxFrame& frame,
    float time,
    const Camera& camera,
    const std::string& currentFloor,
    const std::string& currentLogicalMap,
    int64_t currentMinute,
    const std::function<std::optional<glm::vec3>(const std::string& casterId)>& getCasterPos) {
  (void)camera;
  const auto& instances = LightingSpellRegistry::get().instances();
  if (instances.empty()) return;

  uint32_t sphereFirst = 0, sphereCount = 0;
  bool addedSphere = false;
  auto ensureSphere = [&]() {
    if (addedSphere) return;
    sphereFirst = frame.addVertices(sphereGeometry());
    sphereCount = static_cast<uint32_t>(sphereGeometry().size());
    addedSphere = true;
  };

  std::vector<VfxInstance> motes;
  glm::vec3 moteSight{0.0f};
  bool hasMoteSight = false;

  for (size_t idx = 0; idx < instances.size(); ++idx) {
    const LightSpellInstance& inst = instances[idx];
    if (inst.isExpired(currentMinute)) continue;
    if (!currentLogicalMap.empty() && inst.logicalMapId != currentLogicalMap) continue;
    const float seedOffset = static_cast<float>(idx) * 1.37f + static_cast<float>(inst.power) * 0.53f;

    if (inst.spellKind == kSpellAnchoredLight) {
      if (inst.floorId != currentFloor) continue;
      ensureSphere();

      const float breathe = 0.015f * std::sin(time * 2.1f + seedOffset);
      const glm::vec3 orbPos = inst.position + glm::vec3(0.0f, breathe, 0.0f);

      // Compact hovering 3D pearl core
      VfxDraw dCore;
      dCore.gpu.model = glm::scale(glm::translate(glm::mat4(1.0f), orbPos), glm::vec3(0.075f));
      dCore.gpu.params0 = {time, kModeEnergy, 1.5f, 0.78f};
      dCore.gpu.params1 = glm::vec4(1.0f, 0.98f, 1.0f, 0.72f);
      dCore.gpu.color = glm::vec4(kColorAnchoredLight, 0.06f);
      dCore.gpu.firstVertex = sphereFirst;
      dCore.gpu.vertexCount = sphereCount;
      dCore.gpu.firstInstance = 0xFFFFFFFFu;
      dCore.gpu.material = static_cast<uint32_t>(VfxMaterial::Combat);
      dCore.gpu.sightAt = inst.position;
      dCore.blend = VfxBlend::Additive;
      dCore.depthTest = true;
      dCore.cardAt = orbPos;
      frame.draws.push_back(dCore);

      // Restrained outer corona shell
      VfxDraw dCorona = dCore;
      dCorona.gpu.model = glm::scale(glm::translate(glm::mat4(1.0f), orbPos), glm::vec3(0.14f));
      dCorona.gpu.params0 = {time, kModeEnergy, 1.0f, 0.16f};
      dCorona.gpu.params1 = glm::vec4(kColorAnchoredLight, 0.25f);
      dCorona.gpu.color = glm::vec4(kColorAnchoredLight, 0.14f);
      frame.draws.push_back(dCorona);

      // 5 delicate suspended dust motes around the anchor core
      for (int m = 0; m < 5; ++m) {
        const float a = time * 0.42f + seedOffset + static_cast<float>(m) * (kTau / 5.0f);
        const float r = 0.34f + 0.16f * std::sin(time * 0.9f + static_cast<float>(m) * 1.7f);
        const float y = 0.14f * std::sin(time * 1.15f + static_cast<float>(m) * 2.1f);
        motes.push_back(mote(orbPos + glm::vec3(std::cos(a) * r, y, std::sin(a) * r), 0.014f, 0.24f, kColorAnchoredLight, 1.4f));
      }
      if (!hasMoteSight) moteSight = inst.position, hasMoteSight = true;

    } else if (inst.spellKind == kSpellArcaneBeam) {
      if (inst.floorId != currentFloor) continue;

      // 6 faint dust motes drifting forward inside the projected 3D cone (no solid white emitter sphere)
      const glm::vec3 origin = inst.position;
      const glm::vec3 fwd = glm::length(inst.facing) > 1e-4f ? glm::normalize(inst.facing) : glm::vec3(0.0f, 0.0f, 1.0f);
      const glm::vec3 right = glm::normalize(glm::cross(glm::vec3(0.0f, 1.0f, 0.0f), fwd));
      const float coneReach = std::min(inst.range * 0.65f, 8.0f);
      for (int m = 0; m < 6; ++m) {
        const float phase = std::fmod(time * 0.16f + static_cast<float>(m) / 6.0f + seedOffset * 0.1f, 1.0f);
        const float dist = 0.6f + phase * std::max(coneReach - 0.6f, 1.0f);
        const float lateralFrac = std::sin(static_cast<float>(m) * 2.3f + seedOffset) * std::tan(glm::radians(22.0f)) * 0.72f;
        const float heightOff = -0.35f + 0.35f * std::sin(time * 0.85f + static_cast<float>(m) * 1.9f);
        const glm::vec3 p = origin + fwd * dist + right * (dist * lateralFrac) + glm::vec3(0.0f, heightOff, 0.0f);
        const float edgeFade = std::sin(kPiF * phase);
        motes.push_back(mote(p, 0.014f, 0.22f * edgeFade, kColorArcaneBeam, 1.35f));
      }
      if (!hasMoteSight) moteSight = inst.position, hasMoteSight = true;

    } else if (inst.spellKind == kSpellLuminousHalo) {
      // Follows caster across floors; real 3D point light is the primary effect (no ring or ground circle)
      const std::optional<glm::vec3> casterPos = getCasterPos ? getCasterPos(inst.casterId) : std::nullopt;
      const glm::vec3 feet = casterPos ? *casterPos : inst.position;
      const glm::vec3 chest = feet + glm::vec3(0.0f, 1.15f, 0.0f);

      for (int m = 0; m < 4; ++m) {
        const float a = time * 0.38f + seedOffset + static_cast<float>(m) * (kTau / 4.0f);
        const float y = 0.18f * std::sin(time * 1.1f + static_cast<float>(m) * 1.6f);
        motes.push_back(mote(chest + glm::vec3(std::cos(a) * 0.42f, y, std::sin(a) * 0.42f), 0.014f, 0.20f, kColorLuminousHalo, 1.3f));
      }
      if (!hasMoteSight) moteSight = feet, hasMoteSight = true;
    }
  }

  if (!motes.empty()) {
    const uint32_t quadFirst = frame.addVertices(quadGeometry());
    const uint32_t quadCount = static_cast<uint32_t>(quadGeometry().size());
    const uint32_t firstInst = static_cast<uint32_t>(frame.instances.size());
    frame.instances.insert(frame.instances.end(), motes.begin(), motes.end());

    VfxDraw dMotes;
    dMotes.gpu.model = glm::mat4(1.0f);
    dMotes.gpu.params0 = {time, kModeStreak, 1.0f, 1.0f};
    dMotes.gpu.params1 = {1.0f, 0.98f, 0.92f, 0.0f};
    dMotes.gpu.color = glm::vec4(1.0f);
    dMotes.gpu.firstVertex = quadFirst;
    dMotes.gpu.vertexCount = quadCount;
    dMotes.gpu.firstInstance = firstInst;
    dMotes.gpu.material = static_cast<uint32_t>(VfxMaterial::Combat);
    dMotes.gpu.sightAt = moteSight;
    dMotes.instances = static_cast<uint32_t>(motes.size());
    dMotes.blend = VfxBlend::Additive;
    dMotes.depthTest = true;
    frame.draws.push_back(dMotes);
  }
}

}  // namespace ember
