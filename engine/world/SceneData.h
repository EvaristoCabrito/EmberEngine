#pragma once

// A map's scene as imported from Engine2 (tools/scripts/engine2/export_scene.ts + measure_art.py):
// decoration cards, 3D models, architecture, light sources and units, placed by Engine2's own rules.
// Positions are world units (one hex radius); `ground` holds the ground height under the anchor in each
// board look.

#include <array>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace ember::e2 {

struct SceneLight {
  std::array<float, 3> color{1, 1, 1};  // linear RGB
  float intensity = 0;  // Engine2 LightDef.intensity
  float radius = 0;     // reach in world units (already x LIGHT_RADIUS_MUL)
  float flicker = 0;    // 0 steady .. 1 full fire flicker
  float minHeight = 1;  // a map light never sits lower than this above the ground
  float seed = 0;       // flicker phase
};

struct SceneArt {
  float baseU = 0.5f, baseV = 0;   // the art's opaque base (v up from the bottom), 0..1 of the image
  float flameU = 0.5f, flameV = 0.7f;
  bool flameFound = false;
  int width = 0, height = 0;
};

struct SceneCard {
  std::string id, image;  // image: Engine2 URL, "/game/decorations/<name>.png"
  float x = 0, z = 0, groundSmooth = 0, groundHard = 0;
  float w = 1, h = 1;
  bool mirror = false, onFloor = false, castShadow = true, standOn = false;
  std::optional<float> lockedYaw;  // radians; absent = stands square to the rows (Engine2 LOCK_PROPS)
  float nudge = 0;                 // along the card's facing, Ember's layer step
  std::optional<SceneLight> light;
  SceneArt art;
};

struct SceneModel {
  std::string id, model, file;  // file: Engine2 URL, "/game/models/<path>.glb"
  float height = 1;             // uniform scale to this height, base centred on the anchor
  float x = 0, z = 0, groundSmooth = 0, groundHard = 0, rotY = 0;
};

struct SceneUnit {
  std::string side, name, classId, key, sprite, set, clip;
  float x = 0, z = 0, groundSmooth = 0, groundHard = 0;
  int facing = 1;          // 1 right, -1 left (screen relative)
  bool mirror = false;     // the idle drawn mirrored
  float boxW = 1, boxH = 1, foot = 0;  // Engine2 unitBox for the whole original frame
  int natW = 0, natH = 0;
  int frames = 1;
  std::vector<int> pace;   // frame shown at each 1/240 s (Engine2 framePace)
  std::optional<SceneLight> light;
};

struct SceneData {
  std::string map, title;
  std::string timeOfDay, environment;
  std::vector<SceneCard> cards;
  std::vector<SceneModel> models;
  std::vector<SceneUnit> units;
  size_t architecture = 0;  // walls / doors not drawn yet

  static std::optional<SceneData> load(const std::filesystem::path& sceneJson);
};

}  // namespace ember::e2
