#pragma once

#include <array>
#include <cmath>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "render/BoardGround.h"
#include "render/Atmosphere.h"
#include "render/StarSky.h"
#include "render/FogLooks.h"
#include "render/Camera.h"
#include "render/EffectDraw.h"
#include "render/VfxDraw.h"
#include "render/MeshScene.h"
#include "render/SpriteDraw.h"
#include "render/Volumetrics.h"
#include "rhi/Bindless.h"
#include "rhi/GpuTimer.h"
#include "rhi/Image.h"
#include "rhi/VkCommon.h"
#include "world/FogOfWar.h"

namespace ember {

class VulkanContext;

// One local light (Engine2's CarriedLight: a three.js point light). Matches LocalLight in shaders/scene.hlsli.
struct LocalLight {
  glm::vec3 position;
  float range;     // influence ends here
  glm::vec3 color; // linear colour x intensity
  float decay;     // distance falloff exponent (Engine2: 1.25)
  glm::vec3 facing{0.0f};  // zero: every way; else only the half-space it faces is lit (Luz das janelas: an opening's light stays in the room)
  float behind = 0.0f;     // ... fully lit down to this far behind the light along -facing, gone 0.15 further; < 0: a flat emitter in the
                           // plane through the light, a cosine lobe pow(cos, -behind) about `facing` (a window's light: scene.hlsli lightFacing)
};
class Swapchain;
class DebugUi;

// Frame renderer. Everything is drawn into the renderer's own HDR targets (scene colour in
// half-float, reversed-Z depth); a final pass tonemaps into an 8-bit output image that is copied
// to the window, or read back to disk for captures.
class Renderer {
 public:
  // `swapchain` may be null: the renderer then works off-screen (tools, automated captures).
  Renderer(VulkanContext& context, Swapchain* swapchain, VkExtent2D size);
  ~Renderer();
  Renderer(const Renderer&) = delete;
  Renderer& operator=(const Renderer&) = delete;

  void resize(VkExtent2D size);

  // Draws and presents one frame. False when the swapchain is out of date (recreate, then resize).
  bool renderFrame(float time);

  // Draws one frame and saves it as a PNG (waits for the GPU; never presents).
  void captureFrame(float time, const std::string& path);

  // UI drawn on top of every frame (null = none). Must outlive the renderer or be cleared first.
  void setUi(DebugUi* ui) { ui_ = ui; }
  static constexpr VkFormat outputFormat() { return VK_FORMAT_R8G8B8A8_UNORM; }

  // The 3D scene for the next frame: camera and sprite cards (drawn in the given order, so pass them
  // back to front). Without a scene the renderer draws its bring-up triangle.
  void setScene(const Camera& camera, std::span<const SpriteInstance> sprites);
  // Cards drawn again after the spell effects, over them (a caster in front of its own spell; back to front).
  void setFrontCards(std::span<const SpriteInstance> cards) {
    frontCards_.assign(cards.begin(), cards.begin() + std::min<size_t>(cards.size(), kMaxFrontCards));
  }
  static constexpr uint32_t kMaxFrontCards = 64;
  // Spell effects for the next frame (render/EffectDraw.h; at most kMaxEffects). `brightness` multiplies every
  // effect's colour: the light mode's glow rule (display-relative art, scaled like painted flames).
  void setEffects(std::span<const EffectInstance> effects, float brightness = 1.0f) {
    effects_.assign(effects.begin(), effects.begin() + std::min<size_t>(effects.size(), kMaxEffects));
    effectBrightness_ = brightness;
  }
  static constexpr uint32_t kMaxEffects = 1u << 13;
  // Engine2's ported spell effects for the next frame (render/VfxDraw.h): drawn after the flipbook effects. Draws
  // past the buffers' room are dropped.
  void setVfx(const VfxFrame& frame) {
    vfxVertices_.assign(frame.vertices.begin(), frame.vertices.begin() + std::min<size_t>(frame.vertices.size(), kMaxVfxVertices));
    vfxInstances_.assign(frame.instances.begin(), frame.instances.begin() + std::min<size_t>(frame.instances.size(), kMaxVfxInstances));
    vfxDraws_.clear();
    for (const VfxDraw& d : frame.draws) {
      if (vfxDraws_.size() >= kMaxVfxDraws || d.gpu.firstVertex + d.gpu.vertexCount > vfxVertices_.size()) continue;
      if (d.gpu.firstInstance != 0xFFFFFFFFu && d.gpu.firstInstance + d.instances > vfxInstances_.size()) continue;
      vfxDraws_.push_back(d);
    }
  }
  static constexpr uint32_t kMaxVfxVertices = 1u << 18, kMaxVfxInstances = 1u << 12, kMaxVfxDraws = 1u << 12;
  // 3D meshes for the next frame (drawn GPU-driven: culling, cluster LOD, visibility buffer). An empty
  // view draws none.
  // One sun and a sky/ground hemisphere until the sky exists (ARCHITECTURE.md §7.1b). Defaults are the
  // floor: Engine2's diorama prototype (sun #fff0d4 x3 from (-30, 42, 20), hemisphere #d4e6ff / #5d4b33 x1).
  // Colours are linear; srgb() converts authored hex colours.
  static glm::vec3 srgb(uint32_t hex) {
    const auto c = [](uint32_t v) {
      const float x = static_cast<float>(v) / 255.0f;
      return x <= 0.04045f ? x / 12.92f : std::pow((x + 0.055f) / 1.055f, 2.4f);
    };
    return {c((hex >> 16) & 255), c((hex >> 8) & 255), c(hex & 255)};
  }
  struct Lighting {
    glm::vec3 sunDirection = glm::normalize(glm::vec3(-30.0f, 42.0f, 20.0f));  // towards the sun
    glm::vec3 sunColor = srgb(0xfff0d4);
    float sunIntensity = 3.0f;
    glm::vec3 skyColor = srgb(0xd4e6ff);
    glm::vec3 groundColor = srgb(0x5d4b33);
    float ambientIntensity = 1.0f;
    float exposure = 1.0f;  // multiplied into every light
    // The atmosphere (ARCHITECTURE 7.5): when on, the sky's light replaces the hemisphere (scaled so an upward
    // surface receives ambientIntensity x the luminance of skyColor, as the hemisphere did), the sky is drawn
    // behind the board and the air between camera and surfaces (aerial perspective) is applied.
    bool atmosphere = false;
    glm::vec3 skyIlluminance{1.0f};  // the key light outside the air (colour x strength) that lights the sky
    float kmPerUnit = 0.025f;         // aerial perspective scale (km of air per world unit; a board ~1 km of air)
    float skyScale = 1.0f;            // the sky behind the board (dark at night)
    // Night (the key light is the moon): no sun disc, stars out, and the moon's disc with tonight's phase art.
    bool sunDisc = true;
    float sunRadius = 0.0f;  // the sun disc's angular radius, radians (0 = the real 0.26 deg); panel "Sol: cinema" = the moon's small disc
    float stars = 0.0f;
    uint32_t starLook = 3;  // the star field's saved look (looks/stars/README.md)
    glm::vec3 moonDirection{0.0f, 1.0f, 0.0f};  // towards the disc
    uint32_t moonTexture = 0xFFFFFFFFu;         // bindless sRGB + alpha; none = no disc
    float moonRadius = 0.0f;                    // angular radius, radians
    float moonGlow = 0.0f;                      // the disc's radiance = the art x this (before exposure)
    // Light mode "real" (ARCHITECTURE 7.6 step A): the sky's ambient is what the physical sky gives (lux), times
    // this multiplier (the map's ambient slider), instead of being scaled to ambientIntensity.
    bool physicalSky = false;
    // Ciclo de 24 h: the sky's own sun (when the board's key light is the moon) and the moon as the sky's second light.
    glm::vec3 skySunDirection{0.0f};  // zero = sunDirection
    glm::vec3 moonIlluminance{0.0f};  // zero = none
    // Sol, look 001 (shaders/atmosphere.hlsli sunLook001): 0 the disc as before, 1 the real disc with its limb darkening,
    // corona and the air's aureole (and a halo round the moon at night). Panel "Sol".
    uint32_t sunLook = 0;
    float ambientMul = 1.0f;
    // Lit cards stop at kMaxLit x their colour x this x exposure (Engine2 MAX_LIT, written for lights near 1;
    // the physical mode sets it to the metered light so exposure x litScale stays near 1 by day).
    float litScale = 1.0f;
  };
  void setMeshes(const MeshSceneView& view, const Lighting& lighting) {
    meshView_ = view;
    lighting_ = lighting;
  }
  void setMeshes(const MeshSceneView& view) { setMeshes(view, Lighting{}); }
  // What the GPU did with the meshes, read back from a frame that finished (a few frames old).
  struct MeshStats {
    uint32_t visibleInstances = 0, clusters = 0, queueOverflow = 0, clusterOverflow = 0;
    uint32_t earlyClusters = 0, occludedInstances = 0, retestedClusters = 0, triangles = 0;
  };
  MeshStats meshStats() const { return meshStats_; }
  // Measurement only: 0 normal, 1 task shaders only, 2 mesh shaders without rasterisation.
  void setCostSplit(uint32_t mode) { costSplit_ = mode; }
  // Measurement only: 0 shaded, 1 geometric normals, 2 shading normals (into the scene colour, background black).
  void setDebugView(uint32_t view) { debugView_ = view; }
  // LOD threshold in pixels (1 = the engine's rule; ~0 forces full detail for reference images).
  void setLodPixelError(float pixels) { lodPixelError_ = pixels; }

  // The next frame's GPU work waits for `value` on `timeline` (texture uploads it samples).
  void waitFor(VkSemaphore timeline, uint64_t value) {
    waitTimeline_ = timeline;
    waitValue_ = value;
  }

  // The board's ground (BoardGround::view); replaces the placeholder ground plane. Empty = none.
  void setTerrain(const TerrainView& terrain) { terrain_ = terrain; }
  // The board's ground drawn only inside x0..x1, z0..z1 (world units; a map whose deck ends at its walls, MapDraft::emberDeckClip);
  // the default lets everything through.
  void setGroundClip(glm::vec4 clip) { groundClip_ = clip; }
  // The board always stands on the ground image, never floats (the user, 2026-10-23: "o tabuleiro precisa sempre encostar no
  // ground em todos os mapas, não existe exceção"): when the map's floor image lies `drop` world units under the board's base
  // (`base`: where its side walls end), terrain.hlsl lowers every side-wall bottom by `drop`, so the sides reach the floor.
  // 0 = nothing moves (every map whose floor image lies at the base).
  void setBoardDrop(float base, float drop) { boardBase_ = base, boardDrop_ = std::max(drop, 0.0f); }
  // Up to kGroundHoles rectangles (x0, z0, x1, z1) where the ground is not drawn (a stairwell going down; terrain.hlsl).
  static constexpr size_t kGroundHoles = 2;
  void setGroundHoles(std::span<const glm::vec4> holes) {
    for (size_t k = 0; k < kGroundHoles; ++k) groundHoles_[k] = k < holes.size() ? holes[k] : glm::vec4(0.0f);
  }
  // Marks painted on the ground (the battle grid: reach, route, cursor), drawn by the terrain pass so whatever stands
  // in front covers them. Per hex, up to kMarkLayers fills / rings in order (painter's), colours sRGB + alpha as on
  // screen (blended after tone mapping, so they keep their exact colour in any light). Radii in hex radii (1 = the
  // hex's corners); rIn 0 = a full fill. The board's columns / rows locate the hexes.
  struct GroundMark {
    uint32_t cell;   // row * cols + column
    uint32_t color;  // ImGui IM_COL32 order: r | g << 8 | b << 16 | a << 24
    float rOut, rIn;
  };
  static constexpr uint32_t kMarkLayers = 8;
  static constexpr uint32_t kMaxMarkCells = 128 * 128;
  void setGroundMarks(std::span<const GroundMark> marks, int cols, int rows) {
    marks_.assign(marks.begin(), marks.end());
    markCols_ = cols;
    markRows_ = rows;
  }
  // The battle grid's own looks (looks/grid/README.md, shaders/grid_lookNNN.hlsli): per hex a state word; the
  // terrain pass draws region outlines with their glow, the cell lattice, the route's flowing chevrons, the cursor,
  // the active unit's ring, target locks and pulses itself, from these flags and its clock. Same cell indexing and
  // board as the marks (both may be set: the marks paint first). `look` 0 = off.
  struct GridCell {
    uint32_t flags = 0;  // kGrid* bits
    uint32_t extra = 0;  // bits 0-7 route step (1 = the walker's hex, then 2, 3, ...; 0 = not on the route);
                         // bits 8-15 reach cost share 0..255 (0 by the unit, 255 at the limit of its movement)
  };
  enum GridFlag : uint32_t {
    kGridReach = 1u << 0,         // the picked party unit's movement
    kGridReachFoe = 1u << 1,      // a foe's movement (its threat zone)
    kGridTarget = 1u << 2,        // a foe the picked unit can strike
    kGridSpellAim = 1u << 3,      // where the spell may be aimed
    kGridSpellArea = 1u << 4,     // the hovered aim's area
    kGridSpellChosen = 1u << 5,   // a spell target chosen so far
    kGridHover = 1u << 6,         // the cursor
    kGridHoverBlocked = 1u << 7,  // ... where nothing can stand
    kGridActive = 1u << 8,        // the picked party unit's hex
    kGridActiveFoe = 1u << 9,     // the picked foe's hex
    kGridRouteEnd = 1u << 10,     // the route's destination
    kGridSpellTarget = 1u << 11,  // a hex the spell may be aimed at right now (a target in its zone)
    kGridStrikeZone = 1u << 12,   // "Atacar": the hexes the unit's weapon reaches from where it stands
  };
  void setGrid(std::span<const GridCell> cells, int cols, int rows, uint32_t look, float seconds) {
    grid_.assign(cells.begin(), cells.end());
    gridCols_ = cols;
    gridRows_ = rows;
    gridLook_ = look;
    gridSeconds_ = seconds;
  }
  // The board's water (BoardGround::water); empty = none.
  void setWater(const WaterView& water) { water_ = water; }
  // Ember's 3D water (BoardGround::water3d); empty = none.
  void setWater3d(const Water3DView& water) { water3d_ = water; }
  // The room's diffuse light probes (Luz das janelas; world/ProbeBake.h, shaders/probes.hlsl), after Final Fantasy XV's: the baked
  // grid, relit every frame from the hour's sky (its transfer) and the key light (its bounce, at the key's real direction). `bake`
  // per probe: valid (0 / 1), the 81 transfer numbers, keyDirections.size() x 4 key numbers. Null = none (the sky's SH as before).
  struct ProbeVolume {
    glm::vec3 origin{0.0f}, spacing{1.0f};
    glm::ivec3 dims{0};
    std::vector<glm::vec3> keyDirections;
    std::vector<float> bake;
    // The same bake with the animated doors' leaves closed (empty = none): probes.hlsl blends the two by setProbeOpenness (Dragon
    // Age: The Veilguard bakes once with the doors as placed; here a door that opens in play changes the room's light with it).
    std::vector<float> bakeClosed;
    // The room seen from above (255 inside, 0 outside), maskDims texels of maskSpacing from (origin.x, origin.z): outside it the sun
    // and the sky light nothing (lighting.hlsli probeRoom). Empty = everywhere inside.
    float maskSpacing = 0.0f;
    float roofHeight = 0.0f;  // the room's roof (world y; 0 = none): at it or above, nothing from outside lights a surface (lighting.hlsli probeRoom)
    glm::ivec2 maskDims{0};
    std::vector<uint8_t> mask;
  };
  void setProbes(std::shared_ptr<const ProbeVolume> probes);
  // Light the next frame with the probes (called every frame they apply; a stage that does not ask is lit as before).
  void useProbes() { probesWanted_ = true; }
  // The fires' bounce in the probes shown this much brighter than the exposure makes it (the fires' own boost; 1 = none).
  void setProbeFireScale(float scale) { probeFireScale_ = scale; }
  // How open the room's animated doors are this frame (0 closed .. 1 open): the doors-open and doors-closed bakes blended.
  void setProbeOpenness(float openness) { probeOpenness_ = openness; }
  // The probes' irradiance on an upward face in lux, as last read back (one per probe; empty until a frame with probes finished).
  const std::vector<float>& probeLux() const { return probeLux_; }
  // Local lights for the next frame (at most kMaxLights; the rest are dropped).
  void setLights(std::span<const LocalLight> lights) { lights_.assign(lights.begin(), lights.begin() + std::min<size_t>(lights.size(), kMaxLights)); }
  static constexpr uint32_t kMaxLights = 1024;
  // The sun's shadow map covers this sphere (world space); radius 0 = no sun shadows.
  void setSunShadowBounds(glm::vec3 center, float radius) {
    shadowCenter_ = center;
    shadowRadius_ = radius;
  }
  // The sun's (and the cards') shadow map edge in texels, 512..4096 (Dev Controls / graphics quality: 1024, 2048,
  // 4096); a change waits for the GPU and remakes both maps. The soft filter's radius in texels (1.6 by default).
  void setShadowResolution(uint32_t size);
  uint32_t shadowResolution() const { return shadowSize_; }
  void setShadowFilter(float texels) { shadowFilterTexels_ = texels; }
  // Lighting layers switched on (shaders/lighting.hlsli kLayer*: 1 sun, 2 shadows, 4 indirect), for
  // measuring each alone.
  void setLightingLayers(uint32_t layers) { lightingLayers_ = layers; }
  // What shows behind everything: kNoTextureIndex = the sky (with the atmosphere) or the clear colour,
  // kBlackBackground = black, else a bindless sRGB image (a painted backdrop), fitted to cover the screen.
  static constexpr uint32_t kBlackBackground = 0xFFFFFFFEu;
  // `mode` with a backdrop (resolve.hlsl backgroundAt; both need the atmosphere, else the painting covers the screen as before):
  // 0 cover-fit; 1 the painting up to the world's horizon, the sky above; 2 the painting aligned to the sky (content/engine2/
  // backdrop-fit.json): `fitA` = horizon row, bottom row kept, degrees its height spans above the horizon, degrees below the
  // horizon where the bottom row lands; `fitB` = centre azimuth (radians), sky-top row, degrees its width spans, 0.
  // Mode 2's ground around the board: a flat plane at `y` carrying the map's top-down ground image (`texture`, `tile` world
  // units across, mirrored at its edges; kNoTextureIndex = none), handing over to the painting beyond `far` units along the ray.
  struct BackdropPlane {
    float y = 0.0f;
    uint32_t texture = 0xFFFFFFFFu;
    float tile = 120.0f, far = 300.0f;
    float from = 10.0f;  // degrees below the horizon where the plane starts taking over from the painting
    // A room's drawn ceiling where the sky would show (modes 2-4; resolve.hlsl ceilingAt): the image on a flat plane `ceilingHeight`
    // above the eye, `ceilingTile` world units across; kNoTextureIndex = the sky.
    uint32_t ceilingTexture = 0xFFFFFFFFu;
    float ceilingHeight = 16.0f, ceilingTile = 24.0f;
    // A room's panorama projected onto a real room around the board (resolve.hlsl roomProjected): its centre, round wall radius
    // (0 = none) and the painting's eye height above the floor.
    float roomCentreX = 0.0f, roomCentreZ = 0.0f, roomRadius = 0.0f, roomEye = 1.7f;
  };
  // The background drawn by resolve_panorama.hlsl (resolve.hlsl with the painting's edge rows clamped) instead of resolve.hlsl: every map
  // but the Inn's floors, whose shader never changes (the user, 2026-10-23).
  void usePanoramaShader(bool on) { panoramaShader_ = on; }
  void setBackground(uint32_t background, uint32_t mode = 0, glm::vec4 fitA = {}, glm::vec4 fitB = {}, const BackdropPlane& plane = {}) {
    background_ = background;
    backgroundMode_ = mode;
    backdropFitA_ = fitA;
    backdropFitB_ = fitB;
    backdropPlane_ = plane;
  }
  // Full-scene bloom strength (Engine2: the map's bloomIntensity, default 0.16); 0 = none.
  void setBloom(float strength) { bloom_ = strength; }
  // Cards lean back with the camera's tilt so they project upright anywhere on screen (sprites.hlsl VSMain); off =
  // vertical cards, which converge under a tilted camera. Panel "Cards".
  void setCardLean(bool on) { cardLean_ = on; }
  // The spell light whose shadow the cards cast this frame (one of setLights' lights, by its position; its ground below it at
  // groundY); none = no spell shadow. The cards are drawn into a perspective map looking straight down from the light.
  void setLocalShadow(std::optional<glm::vec3> light, float range, float groundY) {
    localShadowLight_ = light, localShadowRange_ = range, localShadowGround_ = groundY;
  }
  static constexpr uint32_t kLocalShadowSize = 2048;
  bool cardLean() const { return cardLean_; }
  uint32_t lightingLayers() const { return lightingLayers_; }
  // Tactical visibility (fog of war, ARCHITECTURE §7.0 R7): Engine2's darkness mask over the board
  // (world/FogOfWar.h). Every lit surface, card, water, shadow caster and local light reads it, none
  // bypasses it: never seen = black and casts nothing, remembered = darkened without local light, in
  // sight = full lighting. Uploaded when `version` changes; null = no fog of war. `texels`, when given, is
  // fogTexels(*mask) packed ahead of time (on a worker), so the upload is a copy.
  void setFogOfWar(const e2::FogMask* mask, uint64_t version, const std::vector<uint16_t>* texels = nullptr);
  // The mask as the GPU stores it, four half floats per texel: darkness, then the volumetric fog's
  // boundary, ground, valley. Thread-safe (reads only the mask).
  static void fogTexels(const e2::FogMask& mask, std::vector<uint16_t>& out);
  // Volumetric fog (R7's literal fog: how the visible air looks, never what may be seen; shaders/volume.hlsl).
  // Defaults are a first look, to be tuned against the vision.
  struct VolumetricFog {
    bool enabled = false;
    glm::vec3 wind{0.25f, 0.02f, 0.12f};      // drift, world units per second
    glm::vec3 albedo{0.90f, 0.93f, 0.97f};    // scattering / extinction (slightly cool)
    float density = 0.035f;                   // extinction per world unit at the ground
    float scaleHeight = 1.5f;                 // falls by e over this height above the ground
    float valley = 0.6f;                      // extra density per unit the ground lies below its surroundings
    float boundaryDensity = 0.6f;             // the dense fog along the edge of never-seen ground
    float boundaryHeight = 2.5f;
    float noiseScale = 0.3f;
    float anisotropy = 0.35f;                 // Henyey-Greenstein g (forward scattering: shafts towards the sun)
    float historyWeight = 0.9f;               // temporal smoothing
    float nearDepth = 1.0f, farDepth = 200.0f;  // froxel slices span these view depths
    float meanGround = 0.0f;                  // ground height where the board has none
  };
  void setVolumetricFog(const VolumetricFog& fog) { volumetric_ = fog; }
  // The fog of war's mist (R7): one of the numbered looks in render/FogLooks.h (each frozen in its own
  // shader, shaders/fog_lookNNN.hlsl), drawn only while a fog-of-war mask is set.
  struct FogBank {
    bool enabled = false;
    uint32_t look = 2;  // index into fogLooks(); 2 = look 003 (the one the user kept)
    FogScene scene;
  };
  void setFogBank(const FogBank& bank) { fogBank_ = bank; }
  // Atmosfera volumétrica v1 (render/Volumetrics.h, docs/ATMOSPHERE.md): the visible air as a lit 3D medium. Its own
  // module; when enabled it owns the volume every surface reads (the old haze above stays off).
  Volumetrics& volumetrics() { return *volumetrics_; }
  // The sky's reflection map (shaders/skyreflect.hlsl; the plan's phase 4): with the atmosphere on, every specular surface
  // (water first) reflects the real sky, prefiltered by roughness, clouds in; off = the SH sky as before. Panel "Reflexo do céu".
  void setSkyReflection(bool on) { skyReflection_ = on; }
  // Nuvens básicas (shaders/clouds.hlsli): one drifting cloud sheet high over the board, in the sky, in the reflections and
  // as soft shadows on the board by day. Panel "Nuvens"; off = no sheet anywhere.
  struct Clouds {
    bool enabled = false;
    bool shadows = true;        // the sheet shadows the board (by day: the key light is the sun)
    float coverage = 0.4f;      // share of the sky covered (0 clear .. 1 overcast)
    float altitude = 450.0f;    // the sheet's world height (low: the camera sees the sky only near the horizon, where a
                                // higher sheet shrinks every cloud into streaks; measured 2026-10-19 at 900 and 1200)
    float shadow = 0.75f;       // how dark its shadow gets (0 none .. 1 full)
    float scale = 300.0f;       // world units per noise cell (the size of one cloud: a third of the sheet's height, so a
                                // cloud's edge and its eroded fringe are board-sized and the shadows sweep across the
                                // board instead of darkening it whole; measured 2026-10-19 with 500: whole-board only)
    glm::vec2 wind{8.0f, 3.0f}; // drift, world units per second (a shadow's edge crosses the board in seconds)
    // Nuvens volumétricas, look 001 (shaders/cloudsvol.hlsli): instead of the sheets, a real 3D layer between `base` and
    // `top`, ray marched once a frame at half resolution (sky, reflections, shadows on the board all from its density).
    bool volumetric = false;
    float base = 380.0f, top = 640.0f;  // world heights (the caller adds the board's mean ground)
  };
  void setClouds(const Clouds& clouds) { clouds_ = clouds; }
  // The real night sky (render/StarSky.h; star look 4): the catalogue's stars as world directions this frame, the galactic frame
  // for the Milky Way (strength `milkyWay`, 0 = none), the two Magellanic clouds (direction, radius in radians; 0 = none).
  static constexpr uint32_t kMaxStars = 512;
  void setStars(std::span<const GpuStar> stars, glm::vec3 galaxyPole, glm::vec3 galaxyCentre, float milkyWay, const glm::vec4 magellanic[2]) {
    stars_.assign(stars.begin(), stars.begin() + std::min<size_t>(stars.size(), kMaxStars));
    galaxyPole_ = galaxyPole;
    galaxyCentre_ = galaxyCentre;
    milkyWay_ = milkyWay;
    magellanic_[0] = magellanic[0];
    magellanic_[1] = magellanic[1];
  }
  // Clarão do sol e raios de luz, look 001 (shaders/sunglare.hlsl): the lens's glare round the sun or the moon and the
  // crepuscular rays from it, computed from the finished scene and added before the tonemap. Panels "Clarão", "Raios de luz".
  struct SunGlare {
    bool shafts = false;
    bool flare = false;
  };
  void setSunGlare(const SunGlare& glare) { glare_ = glare; }
  // Half the side of the square ground plane under the scene (metres, centred on the origin).
  void setGroundExtent(float halfExtent) { groundHalfExtent_ = halfExtent; }

  Bindless& bindless() { return bindless_; }
  static constexpr uint32_t kMaxSprites = 1u << 16;

  const GpuTimer& gpuTimer() const { return gpuTimer_; }
  // Time the last frame's CPU spent blocked on the GPU (fence) and on the window (acquire).
  double lastWaitMilliseconds() const { return lastWaitMs_; }
  // The last frame's CPU time recording the commands, in vkQueueSubmit2 and in vkQueuePresentKHR (slow-frame report).
  struct CpuSplit {
    double record = 0.0, submit = 0.0, present = 0.0;
  };
  CpuSplit lastCpuSplit() const { return lastSplit_; }

 private:
  struct Frame {
    VkCommandPool pool = VK_NULL_HANDLE;
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    VkFence done = VK_NULL_HANDLE;
    VkSemaphore imageAcquired = VK_NULL_HANDLE;
    Buffer sprites;  // this frame's SpriteInstance[] (host-visible, read by device address)
    Buffer effects;  // this frame's EffectInstance[] (host-visible)
    Buffer vfxVertices, vfxInstances, vfxDraws;  // this frame's VfxFrame (host-visible)
    Buffer meshFrame;     // MeshFrame for the mesh passes (device-local, written by vkCmdUpdateBuffer)
    Buffer shadowFrame;   // MeshFrame of the meshes' pass from the sun (when the cached shadow map is redrawn)
    Buffer lights;        // this frame's LocalLight[] (host-visible)
    Buffer marks;         // this frame's ground marks: per hex kMarkLayers x (colour, half2 radii) (host-visible)
    Buffer grid;          // this frame's battle grid: per hex GridCell (host-visible)
    Buffer meshCounters;  // the work buffer's counters, copied back for meshStats()
    Buffer fogStaging;    // the fog mask's texels on their way to fogMask_ (host-visible, grown on demand)
    Buffer volumeFrame;   // VolumeFrame for the volumetric fog passes (device-local)
    Buffer cloudFrame;    // CloudFrame for the fog bank passes (device-local)
    Buffer atmosphereFrame;  // AtmosphereFrame (device-local)
    Buffer stars;         // this frame's GpuStar[] (host-visible, read by device address)
    Buffer probeRead;     // this frame's probes' upward irradiance in lux (host-visible, written by probes.hlsl, read back)
    uint32_t probeReadCount = 0;  // ... how many probes it held when this frame was recorded (0 = none)
    float probeReadExposure = 0.0f;
    bool countersPending = false;
  };

  void createTargets(VkExtent2D size);
  void writeFrame(Frame& frame, VkCommandBuffer cmd, float time);
  void recordMeshes(Frame& frame, VkCommandBuffer cmd);
  void cullPass(VkCommandBuffer cmd, VkDeviceAddress frameAddress, VkPipeline instances, uint32_t drawPass, uint32_t instanceCount);
  void recordDepthPyramid(VkCommandBuffer cmd);
  void recordBloom(VkCommandBuffer cmd);  // the bloom pyramid (shaders/bloom.hlsl), before the tonemap pass
  void recordVisibilityPass(VkCommandBuffer cmd, VkDeviceAddress frameAddress, uint32_t pass);
  void destroyTargets();
  Frame& beginFrame();
  void record(Frame& frame, float time, VkImage swapImage, const Buffer* readback);
  // Waits for the frame: the swapchain image (if any) and the texture-upload timeline (if set).
  uint32_t gatherWaits(Frame& frame, bool swapImage, std::array<VkSemaphoreSubmitInfo, 2>& waits) const;

  VulkanContext& context_;
  Swapchain* swapchain_;
  Bindless bindless_;
  GpuTimer gpuTimer_;
  DebugUi* ui_ = nullptr;
  double lastWaitMs_ = 0.0;
  CpuSplit lastSplit_{};
  std::array<Frame, kFramesInFlight> frames_{};
  uint32_t frameIndex_ = 0;

  static constexpr uint32_t kNoIndex = UINT32_MAX;

  VkExtent2D size_{};
  Image sceneColor_, depth_, output_;
  uint32_t sceneColorIndex_ = kNoIndex;
  VkSampler linearClamp_ = VK_NULL_HANDLE;
  uint32_t linearClampIndex_ = 0;

  VkPipeline trianglePipeline_ = VK_NULL_HANDLE;
  VkPipeline tonemapPipeline_ = VK_NULL_HANDLE;
  VkPipeline groundPipeline_ = VK_NULL_HANDLE;
  VkPipeline terrainPipeline_ = VK_NULL_HANDLE;
  VkPipeline terrainShadowPipeline_ = VK_NULL_HANDLE;
  VkPipeline waterPipeline_ = VK_NULL_HANDLE;
  VkPipeline water3dPipeline_ = VK_NULL_HANDLE;
  VkPipeline meshShadowPipeline_ = VK_NULL_HANDLE, spriteShadowPipeline_ = VK_NULL_HANDLE, lightsPipeline_ = VK_NULL_HANDLE;
  std::vector<LocalLight> lights_;
  float bloom_ = 0.0f;
  bool cardLean_ = true;
  std::vector<GroundMark> marks_;
  int markCols_ = 0, markRows_ = 0;
  std::vector<GridCell> grid_;
  bool gridOn() const { return gridLook_ != 0 && gridCols_ > 0 && gridRows_ > 0 && static_cast<uint32_t>(gridCols_ * gridRows_) <= kMaxMarkCells; }
  int gridCols_ = 0, gridRows_ = 0;
  uint32_t gridLook_ = 0;
  float gridSeconds_ = 0.0f;
  Buffer clusters_;
  uint32_t clusterBufferIndex_ = kNoIndex;
  Image cardShadowMap_;
  uint32_t cardShadowMapIndex_ = kNoIndex;
  bool cardShadowsThisFrame_ = false;
  Image localShadowMap_;  // the spell light's card shadows (setLocalShadow), D16
  uint32_t localShadowMapIndex_ = kNoIndex;
  std::optional<glm::vec3> localShadowLight_;
  float localShadowRange_ = 0.0f, localShadowGround_ = 0.0f;
  glm::mat4 localShadowMatrix_{1.0f};
  bool localShadowThisFrame_ = false;
  uint64_t shadowMeshKey_ = 0;
  alignas(16) std::array<std::byte, 768> shadowFrameStorage_{};  // one MeshFrame (Renderer.cpp asserts the size)
  // Volumetric fog: two froxel volumes (this frame's and last frame's, swapped), the integrated one.
  VolumetricFog volumetric_;
  Image volumes_[2], volumeIntegrated_;
  uint32_t volumeStorage_[2] = {kNoIndex, kNoIndex}, volumeSampled_[2] = {kNoIndex, kNoIndex};
  uint32_t volumeIntegratedStorage_ = kNoIndex, volumeIntegratedSampled_ = kNoIndex;
  VkPipeline volumeInjectPipeline_ = VK_NULL_HANDLE, volumeIntegratePipeline_ = VK_NULL_HANDLE;
  uint32_t volumeCurrent_ = 0, volumeFrameIndex_ = 0;
  bool volumesReady_ = false, volumeThisFrame_ = false, volumeHistoryValid_ = false;
  glm::mat4 previousVolumeViewProjection_{1.0f};
  // The atmosphere: its tables (transmittance, multiple scattering, sky view, aerial perspective) and SH ambient.
  Image atmoTransmittance_, atmoMultiScattering_, atmoSkyView_, atmoAerial_;
  uint32_t atmoSampled_[4] = {kNoIndex, kNoIndex, kNoIndex, kNoIndex}, atmoStorage_[4] = {kNoIndex, kNoIndex, kNoIndex, kNoIndex};
  Buffer atmoAmbient_;
  VkPipeline atmoPipelines_[5] = {};  // transmittance, multiple scattering, sky view, aerial, ambient
  bool atmoReady_ = false, atmoThisFrame_ = false;
  // The sky's reflection map (skyreflect.hlsl): lat-long, a level per roughness step, rebuilt every frame with the sky.
  Image skyReflect_;
  std::vector<VkImageView> skyReflectLevelViews_;
  std::vector<uint32_t> skyReflectLevelIndices_;
  uint32_t skyReflectSampled_ = kNoIndex;
  VkPipeline skyReflectBasePipeline_ = VK_NULL_HANDLE, skyReflectFilterPipeline_ = VK_NULL_HANDLE;
  bool skyReflection_ = true, skyReflectReady_ = false, skyReflectThisFrame_ = false;
  // The room's light probes (setProbes): the bake (staged once into a device buffer), relit every frame into probeOutput_.
  std::shared_ptr<const ProbeVolume> probes_;
  bool probesDirty_ = false, probesWanted_ = false, probesThisFrame_ = false;
  bool probeGateThisFrame_ = false;  // the room's mask / roof gate on this frame (with or without the relight)
  Buffer probeStaging_, probeBake_, probeOutput_;
  uint32_t probeCount_ = 0;
  VkDeviceSize probeBakeBytes_ = 0, probeMaskBytes_ = 0, probeMaskAt_ = 0;
  float probeFireScale_ = 1.0f;  // the staging buffer's two parts, the mask's place in the output
  float probeOpenness_ = 1.0f;   // setProbeOpenness
  VkDeviceSize probeClosedOffset_ = 0;  // bytes from a probe's open row to its closed row (0 = one bake)
  VkPipeline probePipeline_ = VK_NULL_HANDLE;
  std::vector<float> probeLux_;
  Clouds clouds_;
  std::vector<GpuStar> stars_;  // the real night sky this frame (setStars), copied into frame.stars
  glm::vec3 galaxyPole_{0.0f, 1.0f, 0.0f}, galaxyCentre_{0.0f, 0.0f, 1.0f};
  float milkyWay_ = 0.0f;
  glm::vec4 magellanic_[2]{};
  // The cloud shadow map over the board (skyreflect.hlsl CSCloudShadow): R32F, the sun shadow's bounds, once a frame.
  static constexpr uint32_t kCloudShadowSize = 256;
  Image cloudShadow_;
  uint32_t cloudShadowStorage_ = kNoIndex, cloudShadowSampled_ = kNoIndex;
  VkPipeline cloudShadowPipeline_ = VK_NULL_HANDLE;
  bool cloudShadowReady_ = false, cloudShadowThisFrame_ = false;
  float cloudShadowHeight_ = 0.0f;
  // Nuvens volumétricas look 001 (cloudsvol.hlsl): the noise volumes (made once), the half-resolution march of the frame.
  static constexpr uint32_t kCloudNoiseBaseSize = 128, kCloudNoiseDetailSize = 32;
  Image cloudNoiseBase_, cloudNoiseDetail_, cloudMarch_;
  uint32_t cloudNoiseBaseStorage_ = kNoIndex, cloudNoiseBaseSampled_ = kNoIndex, cloudNoiseDetailStorage_ = kNoIndex, cloudNoiseDetailSampled_ = kNoIndex;
  uint32_t cloudMarchStorage_ = kNoIndex, cloudMarchSampled_ = kNoIndex;
  VkPipeline cloudNoiseBasePipeline_ = VK_NULL_HANDLE, cloudNoiseDetailPipeline_ = VK_NULL_HANDLE, cloudMarchPipeline_ = VK_NULL_HANDLE;
  bool cloudNoiseReady_ = false, cloudMarchThisFrame_ = false;
  // Clarão do sol e raios de luz look 001 (sunglare.hlsl): the half-resolution glare image added in the tonemap.
  SunGlare glare_;
  Image glareImage_;
  uint32_t glareStorage_ = kNoIndex, glareSampled_ = kNoIndex;
  VkPipeline glarePipeline_ = VK_NULL_HANDLE;
  bool glareThisFrame_ = false;
  // Fog bank: the tiling noise volume (made once), the half-resolution march result.
  FogBank fogBank_;
  Image bankMarch_;
  Image bankNoise_[static_cast<size_t>(FogNoise::Count)];
  Buffer bankNoiseStaging_[static_cast<size_t>(FogNoise::Count)];
  uint32_t bankNoiseIndex_[static_cast<size_t>(FogNoise::Count)] = {kNoIndex, kNoIndex, kNoIndex};
  bool bankNoiseUploaded_[static_cast<size_t>(FogNoise::Count)] = {};
  uint32_t bankMarchStorage_ = kNoIndex, bankMarchSampled_ = kNoIndex;
  // Bloom: half-resolution mip pyramid (RGBA16F), one storage view per level, one sampled view of the whole chain.
  Image bloomImage_;
  std::vector<VkImageView> bloomLevelViews_;
  std::vector<uint32_t> bloomLevelIndices_;
  uint32_t bloomSampledIndex_ = kNoIndex;
  VkExtent2D bloomSize_{};
  VkPipeline bloomDownPipeline_ = VK_NULL_HANDLE, bloomUpPipeline_ = VK_NULL_HANDLE;
  std::vector<VkPipeline> bankMarchPipelines_, bankCompositePipelines_;  // per look
  bool bankThisFrame_ = false;
  uint32_t bankLook_ = 0;  // the look this frame draws
  uint32_t bankFrameIndex_ = 0;
  // Fog of war: the CPU copy of the mask (also used to dim local lights by their source's sight), its image.
  bool fogOn_ = false, fogUploadPending_ = false;
  e2::FogMask fog_;
  std::vector<uint16_t> fogTexels_;  // fog_ packed for the GPU (fogTexels)
  uint64_t fogVersion_ = 0, shadowFogVersion_ = 0;
  Image fogMask_;
  uint32_t fogMaskIndex_ = kNoIndex;
  float fogSight(glm::vec3 position) const;  // 1 = in sight .. 0 = remembered or never seen (lighting.hlsli)
  WaterView water_;
  Water3DView water3d_;
  Image shadowMap_;
  uint32_t shadowMapIndex_ = kNoIndex;
  VkSampler shadowSampler_ = VK_NULL_HANDLE;
  uint32_t shadowSamplerIndex_ = 0;
  glm::vec3 shadowCenter_{0.0f};
  float shadowRadius_ = 0.0f;
  uint32_t shadowSize_ = 4096;
  float shadowFilterTexels_ = 1.6f;
  bool shadowsThisFrame_ = false;          // the shadow map is drawn this frame
  std::array<float, 8> shadowKey_{};       // what the cached shadow map was drawn for
  VkDeviceAddress shadowTerrain_ = 0;
  uint32_t background_ = 0xFFFFFFFFu;  // setBackground
  uint32_t backgroundMode_ = 0;
  glm::vec4 backdropFitA_{0.0f}, backdropFitB_{0.0f};
  BackdropPlane backdropPlane_;
  uint32_t lightingLayers_ = 0xFFFFFFFFu;
  TerrainView terrain_;
  glm::vec4 groundClip_{-1.0e9f, -1.0e9f, 1.0e9f, 1.0e9f};
  float boardBase_ = 0.0f, boardDrop_ = 0.0f;  // setBoardDrop: the board's sides lowered to the floor image
  std::array<glm::vec4, kGroundHoles> groundHoles_{};  // (0, 0, 0, 0) = none
  VkPipeline spritePipeline_ = VK_NULL_HANDLE;
  VkPipeline spriteDepthPipeline_ = VK_NULL_HANDLE;  // the cards' depth, after the cards
  VkPipeline effectPipeline_ = VK_NULL_HANDLE;
  VkPipeline vfxPipelines_[2] = {};  // depth-tested, over everything
  std::vector<VfxVertex> vfxVertices_;
  std::vector<VfxInstance> vfxInstances_;
  std::vector<VfxDraw> vfxDraws_;
  std::vector<EffectInstance> effects_;
  float effectBrightness_ = 1.0f;
  VkSampler spriteSampler_ = VK_NULL_HANDLE;
  uint32_t spriteSamplerIndex_ = 0;

  // Mesh passes.
  static constexpr uint32_t kMaxQueue = 1u << 21;     // traversal items per queue (instance, BVH node)
  static constexpr uint32_t kMaxClusters = 1u << 22;  // clusters drawn per frame
  Image visibility_;
  uint32_t visibilityIndex_ = kNoIndex, sceneColorStorageIndex_ = kNoIndex;
  Buffer meshWork_;
  uint32_t meshWorkIndex_ = kNoIndex;
  VkSampler materialSampler_ = VK_NULL_HANDLE;
  uint32_t materialSamplerIndex_ = 0;
  // Occlusion culling: depth pyramid of the early pass, and each instance's verdict from last frame.
  static constexpr uint32_t kMaxInstances = 1u << 18;  // = MeshScene::Capacity::instances
  Image hiz_;
  std::vector<VkImageView> hizLevelViews_;
  std::vector<uint32_t> hizLevelIndices_;
  uint32_t hizIndex_ = kNoIndex, depthIndex_ = kNoIndex;
  // Water look 003 (water_look003.hlsli): the scene under the water (ground, 3D pieces) copied before the water is drawn, read
  // with the depth for its refraction and thickness. Used only when a water sheet draws look 003 (waterRefracts).
  Image waterBackdrop_;
  uint32_t waterBackdropIndex_ = kNoIndex;
  bool waterRefracts() const { return (water_ && water_.look == 3) || (water3d_ && water3d_.look == 4); }
  VkExtent2D hizSize_{};
  VkSampler minSampler_ = VK_NULL_HANDLE;
  uint32_t minSamplerIndex_ = 0;
  Buffer instanceVisibility_;
  uint32_t instanceVisibilityIndex_ = kNoIndex;
  uint64_t visibilityVersion_ = ~0ull;
  bool hizValid_ = false;
  uint32_t costSplit_ = 0;
  uint32_t debugView_ = 0;
  float lodPixelError_ = 1.0f;  // the pyramid holds last frame's depth (false after a resize)
  Camera previousCamera_;
  VkPipeline cullInstancesLatePipeline_ = VK_NULL_HANDLE, hizPipeline_ = VK_NULL_HANDLE;
  VkPipeline cullInstancesPipeline_ = VK_NULL_HANDLE, cullPreparePipeline_ = VK_NULL_HANDLE,
             cullTraversePipeline_ = VK_NULL_HANDLE, cullPrepareDrawPipeline_ = VK_NULL_HANDLE,
             meshletPipeline_ = VK_NULL_HANDLE, resolvePipeline_ = VK_NULL_HANDLE, skyPipeline_ = VK_NULL_HANDLE,
             panoramaResolvePipeline_ = VK_NULL_HANDLE, panoramaSkyPipeline_ = VK_NULL_HANDLE;  // resolve_panorama.hlsl (every map but the Inn)
  bool panoramaShader_ = false;  // usePanoramaShader
  MeshSceneView meshView_;
  Lighting lighting_;
  MeshStats meshStats_;

  float groundHalfExtent_ = 80.0f;
  bool hasScene_ = false;
  Camera camera_;
  std::vector<SpriteInstance> sprites_;
  std::vector<SpriteInstance> frontCards_;  // stored after sprites_ in the frame's sprite buffer
  VkSemaphore waitTimeline_ = VK_NULL_HANDLE;
  uint64_t waitValue_ = 0;
  // Atmosfera volumétrica v1 (last: it releases its bindless slots before the tables go).
  std::unique_ptr<Volumetrics> volumetrics_;
  bool volumetricsThisFrame_ = false;
};

}  // namespace ember
