#pragma once

// The fog of war's mist, kept as numbered looks (the user, 2026-10-14: "save them in a serial ... I will delete
// one by one when I see them live"). Each look is frozen once seen: its own shader file
// (shaders/fog_lookNNN.hlsl: march + composite), its own settings and noise here, its capture in
// looks/fog/NNN/. A change is a new look, never an edit of a saved one; only the user deletes looks.
// The renderer runs the selected look (Renderer::FogBank::look); the map stage cycles them live (F9).

#include <cstdint>
#include <span>
#include <vector>

#include <glm/glm.hpp>

namespace ember {

// The scene facts every look may use (from the map).
struct FogScene {
  glm::vec2 boardCenter{0.0f};
  float boardRadius = 1.0f;
  float meanGround = 0.0f, groundLow = 0.0f, groundHigh = 0.0f;
  glm::vec4 board{0.0f};  // x0, z0, x1, z1
  glm::vec2 partyCenter{0.0f};  // where the party stands (the swirl turns around it)
  float floorY = -40.0f;        // the ground far under the board (MapDraft::emberFloor's plane), where a gorge fog rests
};

// What the renderer hands a look each frame.
struct FogLookInputs {
  glm::mat4 inverseViewProjection{1.0f};
  float time = 0.0f;
  uint32_t frameIndex = 0;
  uint32_t noise = 0, depth = 0, march = 0, marchSampled = 0, color = 0;  // bindless indices
  glm::uvec2 halfSize{0}, fullSize{0};
  FogScene scene;
};

enum class FogNoise : uint32_t { CellularBillows, SoftFbm, WhiteLattice, Count };
inline constexpr uint32_t kFogNoiseSize = 64;  // each noise volume is 64^3, tiling
std::vector<uint8_t> makeFogNoise(FogNoise kind);

struct FogLook {
  const char* number;  // "003"
  const char* shader;  // shader file stem: "<shader>.CSMarch", "<shader>.CSComposite"
  const char* note;    // what it is, and the user's verdict
  FogNoise noise;
  uint32_t frameBytes;  // size of its frame record
  void (*write)(const FogLookInputs&, void* frame);
  // false = a map's own fog drawn without a fog of war (MapDraft::emberWindFog): needs no mask, never in the panel's "Névoa" list
  bool fogOfWar = true;
};
inline constexpr uint32_t kMaxFogLookFrameBytes = 512;
std::span<const FogLook> fogLooks();

}  // namespace ember
