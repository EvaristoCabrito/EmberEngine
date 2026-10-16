#pragma once

// The effect runtime for Engine2's spell effects (its three.js VFX classes, ported one by one: render/vfx/*). Each
// frame the effects fill a VfxFrame: object-space vertices (three.js geometries rebuilt in C++: VfxGeometry.h), per-
// instance matrices, and draws (a model matrix, a material = one of Engine2's shaders ported to shaders/vfx.hlsl,
// its uniforms, blending and depth test). The renderer draws them over the finished scene with the flipbook effects
// (render/EffectDraw.h), every draw read by device address (vertex pulling, no vertex input state).
//
// Coordinates: world space (y up). Engine2's effects work in Ember's frame (z up, one hex radius per unit; vfx/
// adapter.ts); vfxFromEmber() turns that frame into ours, so the ported code keeps Engine2's own vectors.

#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

namespace ember {

// Matches VfxVertex in shaders/vfx.hlsl (32 bytes).
struct VfxVertex {
  glm::vec3 position;  // object space
  float u;
  glm::vec3 normal;
  float v;
};
static_assert(sizeof(VfxVertex) == 32);

// The shaders an effect can draw with (shaders/vfx.hlsl kMaterial*), each one of Engine2's materials.
enum class VfxMaterial : uint32_t {
  Basic = 0,           // three MeshBasicMaterial / LineBasicMaterial: colour x opacity
  MissileCore = 1,     // MagicMissileV2VFX BOLT_VERTEX + CORE_FRAGMENT
  MissileShell = 2,    // MagicMissileV2VFX BOLT_VERTEX + SHELL_FRAGMENT
  MissileTrail = 3,    // MagicMissileV2VFX TRAIL_VERTEX + TRAIL_FRAGMENT
  Standard = 4,        // three MeshStandardMaterial: lit by the scene (base colour, roughness, metalness) + emissive
  Flame = 5,           // OldFireBall flameMaterial (boiling fbm surface, heat colours; edge fade on the shell)
  ImpactParticle = 6,  // FireballImpactVFX particles (camera-facing quads: flipbook, procedural flame, smoke)
  ImpactFlash = 7,     // FireballImpactVFX flash (a camera-facing glow)
  BlessWave = 8,       // BlessVFX WAVE_VERTEX + WAVE_FRAGMENT (the rippling golden ring)
};

// Per-instance data (three InstancedMesh matrices, or a particle's attributes). Matches VfxInstance in vfx.hlsl.
struct VfxInstance {
  glm::mat4 matrix{1.0f};
  glm::vec4 a{0.0f}, b{0.0f}, c{0.0f};
};
static_assert(sizeof(VfxInstance) == 112);

enum class VfxBlend : uint32_t { Additive = 0, Alpha = 1 };

// Matches VfxDraw in shaders/vfx.hlsl (160 bytes).
struct VfxDrawGpu {
  glm::mat4 model;       // world from object
  glm::vec4 params0;     // material uniforms (vfx.hlsl says which)
  glm::vec4 params1;
  glm::vec4 color;       // rgb + a (Basic: colour, opacity; Standard: base colour, roughness)
  uint32_t firstVertex, vertexCount, firstInstance, material;  // firstInstance ~0u = no instance matrices
  glm::vec3 sightAt;     // where the fog of war is read
  uint32_t flags;
  uint32_t texture = 0xFFFFFFFFu;  // bindless texture array (an fx clip), or none
  uint32_t pad[3]{};
};
static_assert(sizeof(VfxDrawGpu) == 160);

struct VfxDraw {
  VfxDrawGpu gpu{};
  uint32_t instances = 1;
  VfxBlend blend = VfxBlend::Additive;
  bool depthTest = true;
};

// One frame's effect geometry and draws.
struct VfxFrame {
  std::vector<VfxVertex> vertices;
  std::vector<VfxInstance> instances;  // per instance: a matrix applied before the draw's model, or particle attributes
  std::vector<VfxDraw> draws;

  void clear() {
    vertices.clear();
    instances.clear();
    draws.clear();
  }
  // Appends triangles (three per triangle, already expanded) and returns where they start.
  uint32_t addVertices(const std::vector<VfxVertex>& v) {
    const uint32_t first = static_cast<uint32_t>(vertices.size());
    vertices.insert(vertices.end(), v.begin(), v.end());
    return first;
  }
};

// A light an effect gives off this frame: Engine2's three.js PointLight (colour, intensity, distance), world space.
struct VfxLight {
  glm::vec3 at{0.0f};
  glm::vec3 color{1.0f};
  float intensity = 0.0f, distance = 3.0f;
};

// Ember's frame (z up, y toward the board's far side) -> world (y up): (x, y, z) -> (x, z, -y) (vfx/adapter.ts).
inline glm::vec3 vfxFromEmber(glm::vec3 e) { return {e.x, e.z, -e.y}; }
inline glm::vec3 vfxToEmber(glm::vec3 w) { return {w.x, -w.z, w.y}; }
inline glm::mat4 vfxEmberToWorld() {
  glm::mat4 m(1.0f);
  m[0] = {1, 0, 0, 0};   // x -> x
  m[1] = {0, 0, -1, 0};  // y -> -z
  m[2] = {0, 1, 0, 0};   // z -> y
  return m;
}

}  // namespace ember
