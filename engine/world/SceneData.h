#pragma once

// A map's scene as imported from Ember (tools/scripts/data/export_scene.ts + measure_art.py):
// decoration cards, 3D models, architecture, light sources and units, placed by Ember's own rules.
// Positions are world units (one hex radius); `ground` holds the ground height under the anchor in each
// board look.

#include <array>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace ember {

struct SceneLight {
  std::array<float, 3> color{1, 1, 1};  // linear RGB
  float intensity = 0;  // Ember LightDef.intensity
  float radius = 0;     // reach in world units (already x LIGHT_RADIUS_MUL)
  float flicker = 0;    // 0 steady .. 1 full fire flicker
  float minHeight = 1;  // a map light never sits lower than this above the ground
  float seed = 0;       // flicker phase
  float offsetX = 0, offsetZ = 0;  // a 3D piece's flame off its centre, in the model's frame (the street lamp's hanging lantern)
};

struct SceneArt {
  float baseU = 0.5f, baseV = 0;   // the art's opaque base (v up from the bottom), 0..1 of the image
  float flameU = 0.5f, flameV = 0.7f;
  bool flameFound = false;
  int width = 0, height = 0;
};

// A flame painted on a 2D card (a burning house), drawn over it as a volume of fire: its foot on the picture (u across, v up from
// the bottom, 0..1 of the image, as SceneArt), its height and radius in world units. The editor catalogue's "fire" with "uv".
struct SceneCardFire {
  float u = 0.5f, v = 0.5f, height = 1, radius = 0.3f, turbulence = 1;
};

struct SceneCard {
  std::string id, image;  // image: Ember URL, "/game/decorations/<name>.png"
  float x = 0, z = 0, groundSmooth = 0, groundHard = 0;
  float w = 1, h = 1;
  bool mirror = false, onFloor = false, castShadow = true, standOn = false;
  std::optional<float> lockedYaw;  // radians; absent = stands square to the rows (Ember LOCK_PROPS)
  float nudge = 0;                 // along the card's facing, Ember's layer step
  std::optional<SceneLight> light;
  SceneArt art;
  std::vector<SceneCardFire> fires;  // the flames drawn over its painted ones
};

// A flame a 3D piece burns with, in the model's own space (x, y up, z; the model's units): its foot, its height and radius, how
// wild it is (1 a torch, less a candle). Drawn as a volume of fire (shaders/vfx.hlsl FireVolume); the editor catalogue's "fire".
struct SceneFire {
  float x = 0, y = 0, z = 0, height = 0.7f, radius = 0.17f, turbulence = 1;
};

struct SceneModel {
  std::string id, model, file;  // file: Ember URL, "/game/models/<path>.glb"
  float height = 1;             // uniform scale to this height, base centred on the anchor
  float x = 0, z = 0, groundSmooth = 0, groundHard = 0, rotY = 0;
  std::optional<SceneLight> light;  // a 3D piece that gives light (the Meshy hearth's fire: models.json "light"; SceneBuilder)
  std::vector<SceneFire> fires;     // ... and the flames it burns with
};

struct SceneUnit {
  std::string side, name, classId, key, sprite, set, clip;
  float x = 0, z = 0, groundSmooth = 0, groundHard = 0;
  int facing = 1;          // 1 right, -1 left (screen relative)
  bool mirror = false;     // the idle drawn mirrored
  float boxW = 1, boxH = 1, foot = 0;  // Ember unitBox for the whole original frame
  int natW = 0, natH = 0;
  int frames = 1;
  std::vector<int> pace;   // frame shown at each 1/240 s (Ember framePace)
  std::optional<SceneLight> light;
};

// Ember's 3D architecture (walls, doorways, doors, gates): the piece, its anchor hex and quarter turn; its solid is
// built from them (world/Architecture.h) when the map opens.
struct SceneArchitecture {
  std::string id;
  int x = 0, y = 0, rot = 0;
};

struct SceneData {
  std::string map, title;
  std::string timeOfDay, environment;
  std::string moonPhase;  // the Map Editor's Lua (ember::kMoonPhases name; "" = full): the moon when no campaign day gives one
  // The map's own lighting sliders (Ember Mission.sunIntensity / ambientIntensity / bloomIntensity; absent =
  // the time of day's default) and its mist type (the fog look for the scenario).
  std::optional<float> sunIntensity, ambientIntensity, bloomIntensity;
  std::string mistType;
  std::string backdrop;  // the painted backdrop's file name in public/game/map-backgrounds (UTF-8); empty = none
  std::vector<SceneCard> cards;
  std::vector<SceneModel> models;
  std::vector<SceneUnit> units;
  size_t architecture = 0;  // walls / doors / gates (pieces below)
  std::vector<SceneArchitecture> pieces;

  static std::optional<SceneData> load(const std::filesystem::path& sceneJson);
};

// Every unit with every pose it has (tools/scripts/data/export_heroes.ts: Ember's unit tables).
struct HeroPose {
  std::string pose, label;   // Ember's pose id ("attack") and its Portuguese label ("Atacar")
  std::string set, clip;     // the cooked sprite clip
  float boxW = 1, boxH = 1, foot = 0;  // Ember unitBox for this pose
  int frames = 1;
  std::vector<int> pace;     // frame shown at each 1/240 s (Ember framePace)
  int doneAt = -1;           // one-shot poses: the 1/240 s tick it finishes (-1 = loops)
  float strides = 2.0f;      // walk clips: full strides (both legs) the whole clip holds (content/data/walk-strides.json)
  // walk clips: how fast the feet cover the ground as the video plays, world units a second at the unit's size 1
  // (content/data/walk-speeds.json, tools/scripts/sprites/walk_speed.py); 0 = not read (fliers): the game's step
  float groundSpeed = 0.0f;
  float impact = -1;         // strike poses: share of the pose at which the blow lands (art cut from video); -1 = the default
  float release = -1;        // bow poses: share of the pose at which the arrow leaves the bow (art cut from video); -1 = none
  std::string medium;        // the same clip at ~610 px of figure in this sprite set (Personagens Média); empty = none
  std::string light;         // the clip's own Leve copy in this sprite set (fewer frames, e.g. Voss's 36); empty = Leve halves `medium`
  int lightStep = 0;         // the Leve copy holds every Nth frame of the clip (video_sprite.py frameStepNear); 0 = picked evenly
  std::string cinema;        // every frame of the clip at ~610 px of figure in this sprite set (Cinemática Média / Leve); empty = none
  // A cutscene-only pose (an emote; the user, 2026-11-12: "This emote will not load during combat, only during cutscenes"): cooked
  // in the "cutscene" pack, never pinned with a fight's characters, streamed in by a cutscene whose "pose" steps play it.
  bool cutscene = false;
  // Played once, whole, each time this pose ends as a one-shot, before the idle comes back (the roster's "then": a skill that ends
  // with the limbs open is followed by the unit's "fold"; docs/SPRITE_STANDARD.md "Abrir / fechar"). Empty = none.
  std::string then;
  // Poses Ember holds on their hex (units/catalog.ts centerBody): per frame, how far right of frame 1 the body sits, as a
  // share of the pose's width (tools/scripts/data/measure_body_shift.py); empty = the pose plays as drawn.
  std::vector<float> bodyShift;
  // Per frame of the full clip, the size it is drawn at (1 = the pose's box): a video whose camera pulls back as the limbs open (the
  // Birolhos' unfold) is drawn at the idle's body size on every frame (content/sprites/<set>/frame-scale.json,
  // tools/scripts/sprites/frame_scale.py); empty = the pose's box on every frame.
  std::vector<float> frameScale;
};
struct Hero {
  std::string name;
  std::string key;           // the unit's catalog key: a scene unit with the same key is this unit
  std::string group;         // hero / npc / monster / summon, from the exported catalog
  bool reversed = false;     // drawn facing left (Ember UnitActor.drawnReversed)
  std::vector<HeroPose> poses;
  const HeroPose* find(const std::string& pose) const;
};
std::optional<std::vector<Hero>> loadHeroes(const std::filesystem::path& heroesJson);

}  // namespace ember
