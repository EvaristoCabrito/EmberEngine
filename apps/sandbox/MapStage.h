#pragma once

// A real Ember map from Engine2 (C:/Engine2/maps): its board and ground in both looks, its water, and its
// scene as Engine2 places it (decoration cards, 3D models, units from the spawns; imported by
// tools/scripts/engine2/export_scene.ts + measure_art.py), with Engine2's camera. D1 port, step by step
// (docs/ENGINE2_AUDIT.md). Engine2 stays read-only: maps and relief grids are read from it; textures,
// sprites and meshes come from the cooked packs (Cooker ground / sprites / meshes).
//
// Left button (Engine2's protected rule): hold 0.5 s, then drag to pan the board; a short click acts in the game.
// Camera (Ember's own orbit camera, 2026-10-15): opens turned -30 deg, tilted 20 deg, 30 deg lens, framing the
// whole board. It always turns around a pivot on the ground and glides to every input. Right-drag turns and
// tilts (5 to 89.5 deg), middle-drag grabs the ground, the wheel zooms toward the cursor, WASD/arrows slide the
// board (the pivot stays on the board: at most its edge reaches the screen centre); the eye never goes into the
// ground or a 3D piece. The camera bar (top right, Engine2's battle HUD copied 1:1 at the user's request): Normal /
// Tática, tilt 5 deg down / up, turn 15 deg left / right, back to the normal angle. Tática is the old-school 2D view
// (Engine2 CameraRig.flat): straight overhead through a 1 deg lens from far away (no visible perspective), no
// turning; cards and units lie flat facing up just over the highest ground, nearer rows drawn over farther ones.
//
// Combat (world/Battle.h, --battle-rules <map.battle.json> from tools/scripts/engine2/export_battle.ts): the left button
// picks a unit (its card or its hex) and shows its movement reach as Engine2's battle grid does; hovering a reachable hex
// shows the route, clicking it walks the unit there one hex per 0.22 s with Engine2's walk poses (UnitActor); "Fim do
// turno" gives every unit its movement back. Esc lets go of the unit. Foes the picked party unit can hit from where it
// stands are marked (Engine2's target red); hovering one shows Engine2's forecast, clicking attacks: both turn to face,
// the attack pose plays, at its impact Ember's roll lands (world/Combat.h: damage, hit chance, crit, the arcane / staff
// element), the target shows its hit or death pose and the number, then it strikes back when it can.
// Turns are Engine2's: one battle-opening initiative roll per unit (1d20 + class bonus), a round in that order, each
// unit once. The party's unit whose turn it is gets picked; it walks (its movement pool can be split) and strikes once;
// a foe it can reach from any hex of its walk is marked too, and clicking it walks there and strikes. "Fim do turno"
// ends its turn (so does striking with no movement left). Enemies take their turns on their own (Engine2 runAiFor's
// plain branch: strike the best foe they can reach, else close in on the nearest hero; enmity picks their focus).
// Other units can be inspected (their reach) at any time. Vitória / Derrota when one side has nobody standing.
//
// Fog of war (R7, world/FogOfWar.h) on maps that use it (or forced with --fog 1): the party's sight with
// occlusion; enemies out of sight are not drawn; the renderer darkens and masks everything by the mask.
// --scout walks the first party unit to the farthest enemy and back (a hex every 0.6 s), so never-seen,
// remembered and in-sight ground show together while lights move.

#include <SDL3/SDL.h>
#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <memory>
#include <optional>
#include <chrono>
#include <numbers>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <format>
#include <limits>
#include <string_view>

#include "Stage.h"
#include "core/Jobs.h"
#include "core/Log.h"
#include "render/BoardGround.h"
#include "render/FogLooks.h"
#include "render/Renderer.h"
#include "rhi/UploadQueue.h"
#include "render/MeshScene.h"
#include "render/SpriteSystem.h"
#include "world/Battle.h"
#include "world/Combat.h"
#include "world/Spells.h"
#include "render/UiArt.h"
#include "world/Board.h"
#include "world/FogOfWar.h"
#include "world/TimeOfDay.h"
#include "world/GroundAssets.h"
#include "world/GroundMesh.h"
#include "world/SceneData.h"

namespace ember {

struct MapStageOptions {
  std::filesystem::path map;
  std::filesystem::path scene;  // imported scene (optional): decorations, models, units
  uint64_t spriteBudgetMb = 1024;
  std::filesystem::path boardData = "content/engine2/board.json";
  std::filesystem::path engine2Public = "C:/Engine2/public";
  e2::EdgeStyle look = e2::EdgeStyle::Smooth;
  // Captures: camera relative to Engine2's normal view, and lighting layers switched on.
  float zoom = 1.0f;          // distance divided by this
  float turnDegrees = 0.0f;   // added to the normal view's turn
  float tiltDegrees = 0.0f;   // added to its tilt
  glm::vec2 offset{0.0f};     // target moved by this many board widths / depths
  uint32_t layers = 0xFFFFFFFFu;
  // Sun direction override (degrees; elevation < 0 = keep the default): azimuth from +x towards +z.
  float sunAzimuth = 0.0f, sunElevation = -1.0f;
  // Test lights for the proof scene: a torch carried around the party, a spell's light flying from the
  // party's caster to the nearest enemy and flashing on impact.
  bool movingLight = false, spellLight = false;
  // Gameplay measurement: for this many seconds the camera turns a full circle and zooms in and out twice
  // while units animate and lights move and flicker; every frame's main-thread and GPU time is recorded.
  float measureSeconds = 0.0f;
  // Fog of war: -1 = as the map says, 0 off, 1 on. scout: the walk above.
  int fog = -1;
  bool scout = false;
  // Volumetric fog (R7's literal fog, shaders/volume.hlsl): on unless switched off (--volume 0).
  bool volume = true;
  int fogLook = 3;  // the fog look shown first (render/FogLooks.h numbers)
  // Lighting (also in the panel): 0 the old prototype day (every capture before 2026-10-15), 1 Engine2's 3D presets for
  // the map's time of day (the floor), 2 physical sky with Engine2's strengths, 3 real (default): lux, physical camera.
  int lighting = 3;
  // Scene clock pinned to this many seconds (< 0 = real time): identical frames for paired captures.
  float sceneTime = -1.0f;
  // Night: tonight's moon phase (Engine2's moonPhase.ts names; "" = Engine2's stage default: the full moon's art at
  // the night's own strength) and where the moon's disc and light stand (see moonPlacement()); also in the panel.
  std::string moonPhase;
  int moonPath = 0;
  // Every unit's poses (tools/scripts/engine2/export_heroes.ts output; empty = idle only): the map's own units
  // play the pose chosen in the panel, each from its own catalog entry (found by the scene unit's key).
  std::filesystem::path heroes;
  int starLook = 3;  // the star field's saved look (looks/stars/README.md; also in the panel)
  // The battle's rules and units (tools/scripts/engine2/export_battle.ts output; empty = no combat).
  std::filesystem::path battle;
};

class MapStage final : public Stage {
 public:
  MapStage(VulkanContext& context, Renderer& renderer, AssetManager& assets, JobSystem& jobs, const MapStageOptions& options)
      : renderer_(renderer),
        uploads_(context),
        ground_(context, renderer.bindless(), assets, uploads_),
        sprites_(context, renderer.bindless(), assets, uploads_, {options.spriteBudgetMb << 20}),
        meshes_(context, renderer.bindless(), assets, uploads_, {.geometryBytes = 256ull << 20}),
        assets_(assets),
        look_(options.look),
        options_(options),
        jobs_(jobs) {
    yaw_ += glm::radians(options.turnDegrees);
    lightingMode_ = std::clamp(options.lighting, 0, 3);
    moonPath_ = std::clamp(options.moonPath, 0, 2);
    starLook_ = std::clamp(options.starLook, 1, kStarLooks);
    for (int i = 0; i < static_cast<int>(std::size(e2::kMoonPhases)); ++i)
      if (options.moonPhase == e2::kMoonPhases[i]) moonPhase_ = i;
    for (uint32_t i = 0; i < fogLooks().size(); ++i)
      if (options.fogLook == std::atoi(fogLooks()[i].number)) fogLook_ = i;
    pitch_ += glm::radians(options.tiltDegrees);
    layers_ = options.layers;
    data_ = e2::BoardData::load(options.boardData);
    std::optional<e2::MapDraft> draft = e2::MapDraft::load(options.map);
    if (!data_ || !draft) {
      failed_ = true;
      return;
    }
    board_ = std::make_unique<e2::Board>(*data_, std::move(*draft));
    if (!board_->loadRelief(options.engine2Public) || !ground_.build(*board_)) {
      failed_ = true;
      return;
    }
    const BoardGround::Stats s = ground_.stats();
    logInfo("map {} '{}': {}x{} hexes, smooth {} triangles, hard {} triangles, {} baked layers, {} textures, {:.1f} MB geometry",
            board_->draft().id, board_->draft().title, board_->layout().cols, board_->layout().rows, s.triangles[0], s.triangles[1],
            s.layers, s.textures, s.geometryBytes / 1048576.0);
    // Engine2's CameraRig.frame: centre on the board, far enough back to see it whole.
    const e2::Board::Bounds b = board_->bounds();
    double avgY = 0;
    for (const e2::Cell& c : board_->cells()) avgY += c.groundY;
    avgY /= static_cast<double>(board_->cells().size());
    target_ = {static_cast<float>((b.x0 + b.x1) / 2), static_cast<float>(avgY), static_cast<float>((b.z0 + b.z1) / 2)};
    meanGround_ = static_cast<float>(avgY);
    boardWidth_ = static_cast<float>(b.x1 - b.x0);
    boardDepth_ = static_cast<float>(b.z1 - b.z0);
    // The sun's shadow covers the board's bounding sphere (ground from y = 0 to its highest cell).
    double top = 0, low = 1e9;
    for (const e2::Cell& c : board_->cells()) top = std::max(top, c.groundY), low = std::min(low, c.groundY);
    boardTop_ = static_cast<float>(top);
    groundLow_ = static_cast<float>(low);
    boardCenter_ = {target_.x, static_cast<float>(top * 0.5), target_.z};
    boardRadius_ = 0.5f * std::sqrt(boardWidth_ * boardWidth_ + boardDepth_ * boardDepth_ + static_cast<float>(top * top)) + 1.0f;
    target_.x += options.offset.x * boardWidth_;
    target_.z += options.offset.y * boardDepth_;
    if (options.fog == 1 || (options.fog < 0 && board_->draft().fog)) fog_.emplace(*board_);

    if (!options.scene.empty()) {
      scene_ = e2::SceneData::load(options.scene);
      if (!scene_) {
        failed_ = true;
        return;
      }
      sprites_.start();
      // 3D models: Engine2's GLBs, cooked by Cooker meshes ("/game/models/x.glb" -> "meshes/models/x.mesh").
      for (const e2::SceneModel& m : scene_->models) {
        std::string asset = m.file.substr(std::string("/game/").size());
        asset = "meshes/" + asset.substr(0, asset.size() - 4) + ".mesh";
        if (!modelMeshes_.contains(asset)) modelMeshes_[asset] = meshes_.add(assetId(asset));
      }
      logInfo("scene {}: {} cards, {} models, {} units, {} architecture pieces (not drawn yet)", scene_->map, scene_->cards.size(),
              scene_->models.size(), scene_->units.size(), scene_->architecture);
      initUnits();
      setUpParty();
      if (!options.heroes.empty()) setUpPoses(options.heroes);
      if (!options.battle.empty()) setUpBattle(options.battle);
      if (!scene_->backdrop.empty()) ground_.requestTexture(e2::backdropAsset(scene_->backdrop));
      background_ = scene_->backdrop.empty() ? 1 : 0;
      if (e2::timeOfDay(scene_->timeOfDay).moon && scene_->environment != "indoor")
        for (const char* phase : e2::kMoonPhases) ground_.requestTexture(e2::skyMoonAsset(phase));  // 0.1 MB each
    }
  }

  ~MapStage() override {
    if (fogBusy_) jobs_.wait(fogJob_);  // the sight job uses this stage's board and fog
  }

  void frame(Renderer& renderer, VkExtent2D size) override {
    ground_.update();
    if (scene_) {
      sprites_.update();
      meshes_.update();
      if (!modelsPlaced_ && meshes_.ready()) placeModels();
    }
    uploads_.flush();
    const float time = options_.sceneTime >= 0.0f ? options_.sceneTime
                                                 : std::chrono::duration<float>(std::chrono::steady_clock::now() - start_).count();
    updateFog(time);
    const float aspect = static_cast<float>(size.width) / static_cast<float>(size.height);
    if (distance_ <= 0.0f) {
      const float tanHalf = std::tan(glm::radians(kFov * 0.5f));
      maxDistance_ = std::max(40.0f, std::max(boardWidth_, boardDepth_) * 2.2f);
      distance_ = std::min(maxDistance_, std::max(boardWidth_ / (2 * tanHalf * aspect), boardDepth_ * 1.35f) * 1.05f) / options_.zoom;
    }
    // Measurement: a full turn and two zooms in and out (to 35% of the framing distance) over the run.
    float yaw = yaw_, distance = distance_;
    if (measuring_) {
      const float t = std::chrono::duration<float>(std::chrono::steady_clock::now() - measureStart_).count();
      const float k = std::min(t / options_.measureSeconds, 1.0f);
      yaw = yaw_ + k * 2.0f * std::numbers::pi_v<float>;
      distance = distance_ * glm::mix(0.35f, 1.0f, 0.5f + 0.5f * std::cos(k * 4.0f * std::numbers::pi_v<float>));
      if (t >= options_.measureSeconds) finishMeasure();
    }
    // The orbit camera: yaw_ / pitch_ / distance_ / target_ are where the user sends it; cam_ follows them with a
    // critically damped glide every frame, so every input (drag, wheel, keys, the bar) moves without a jump.
    const float dt = std::min(std::chrono::duration<float>(std::chrono::steady_clock::now() - lastFrame_).count(), 0.1f);
    lastFrame_ = std::chrono::steady_clock::now();
    aspect_ = aspect;
    if (!captured_) {
      captured_ = true, homeYaw_ = yaw_, homePitch_ = pitch_, homeTarget_ = target_, homeDistance_ = distance_;
      cam_ = {target_, yaw_, pitch_, distance_};
    }
    if (!measuring_) panKeys(dt);
    updatePanHold();
    now_ = time;
    updateUnits(dt);
    updateExchange();
    updateTurns();
    updateHover();
    clampGoal();
    if (measuring_) {
      cam_ = {target_, yaw, pitch_, distance};  // measurements and captures: exactly the asked view
    } else {
      const float k = 1.0f - std::exp(-dt * kGlide);
      cam_.target += (target_ - cam_.target) * k;
      cam_.yaw += (yaw_ - cam_.yaw) * k;
      cam_.pitch += (pitch_ - cam_.pitch) * k;
      cam_.distance *= std::pow(distance_ / cam_.distance, k);  // zoom glides evenly in scale
    }
    // The ground under the pivot (the camera always turns around a point on the board), and the eye never inside
    // the ground or a 3D piece: the tilt rises just enough to clear it.
    cam_.target.y = pivotHeight(cam_.target);
    for (int i = 0; i < 8 && !flat_; ++i) {
      const glm::vec3 eye = eyeOf(cam_);
      const float floor = clearanceAt(eye.x, eye.z) + kEyeClearance;
      if (eye.y >= floor) break;
      const float need = std::asin(std::clamp((floor - cam_.target.y) / cam_.distance, -1.0f, 1.0f));
      cam_.pitch = std::min(std::max(cam_.pitch, need + 0.002f), kMaxPitch);
      if (pitch_ < cam_.pitch) pitch_ = cam_.pitch;  // the goal does not pull it back into the ground
    }
    // Tática: straight down from far away through the long lens, framing what the normal lens frames at this distance;
    // the top of the screen is the board's far side (-z). Its near plane hugs the board (Engine2: distance - 200).
    const float flatDistance = cam_.distance * std::tan(glm::radians(kFov * 0.5f)) / std::tan(glm::radians(kFlatFov * 0.5f));
    const Camera camera = flat_ ? Camera::lookAt(cam_.target + glm::vec3(0.0f, flatDistance, 0.0f), cam_.target, glm::radians(kFlatFov), aspect,
                                                 std::max(kNear, flatDistance - 200.0f), {0.0f, 0.0f, -1.0f})
                                : Camera::lookAt(eyeOf(cam_), cam_.target, glm::radians(kFov), aspect, kNear);
    lastCamera_ = camera;
    drawList_.clear();
    if (scene_ && sprites_.ready()) buildCards(camera, time);
    renderer.setScene(camera, drawList_);
    lights_.clear();
    if (scene_) buildLights(time);
    renderer.setLights(lights_);
    Renderer::Lighting lighting = mapLighting(time);
    if (lightingMode_ == 3) {
      // The physical camera (ARCHITECTURE 7.6 step A): EV100 from the light metered on the board (incident meter:
      // EV100 = log2(E / 2.5)), darker at night by up to 2.5 stops so night reads as night, and the eye adapting to
      // it (about a second); exposure = 1 / (1.2 x 2^EV) as a camera's.
      const float ev = std::log2(std::max(meterLux_, 1e-4f) / 2.5f);
      const float target = ev + nightDarkening(ev);
      if (std::isnan(adaptedEv_) || measuring_) adaptedEv_ = target;
      adaptedEv_ += (target - adaptedEv_) * (1.0f - std::exp(-dt * kAdaptPerSecond));
      lighting.exposure = 1.0f / (1.2f * std::exp2(adaptedEv_));
      lighting.litScale = std::max(meterLux_, 1e-4f) * 1.2f / 2.5f;  // exposure x this = 1 at the metered EV
    }
    exposure_ = lighting.exposure;
    if (options_.sunElevation >= 0.0f) {
      const float az = glm::radians(options_.sunAzimuth), el = glm::radians(options_.sunElevation);
      lighting.sunDirection = {std::cos(el) * std::cos(az), std::sin(el), std::cos(el) * std::sin(az)};
    }
    // Tática looks from far above: no air between the eye and the board (Engine2 drops its distance fog there).
    if (flat_) lighting.kmPerUnit = 0.0f;
    renderer.setMeshes(scene_ ? meshes_.view() : MeshSceneView{}, lighting);
    renderer.setTerrain(ground_.view(look_));
    {
      groundMarks_.clear();
      collectMarks(groundMarks_);
      renderer.setGroundMarks(groundMarks_, board_ ? board_->layout().cols : 0, board_ ? board_->layout().rows : 0);
    }
    renderer.setWater(ground_.water(look_));
    renderer.setSunShadowBounds(boardCenter_, boardRadius_);
    renderer.setLightingLayers(layers_);
    {
      uint32_t bg = kNoTextureIndex;  // the sky (or the clear colour)
      if (background_ == 2) bg = Renderer::kBlackBackground;
      if (background_ == 0 && scene_ && !scene_->backdrop.empty()) bg = ground_.textureIndex(e2::backdropAsset(scene_->backdrop));
      renderer.setBackground(bg);
    }
    renderer.setFogOfWar(fogReady_ && !noFog_ ? &fogMask_ : nullptr, maskVersion_, &fogTexels_);
    Renderer::VolumetricFog volume;
    // The froxel haze belongs to the old lighting (mode 0, where every saved fog look was captured); under the
    // brighter physical light it reads as a milky veil (2026-10-15), so it stays off there.
    volume.enabled = options_.volume && lightingMode_ == 0 && !flat_;
    volume.meanGround = meanGround_;
    // The froxel fog is the air's haze in sight; the fog of war's bank is drawn by the fog bank pass.
    volume.boundaryDensity = 0.0f;
    renderer.setVolumetricFog(volume);
    // The fog of war's mist: the numbered look on show (render/FogLooks.h; chosen in the Map panel).
    Renderer::FogBank bank;
    bank.enabled = options_.volume && fog_.has_value() && !noFog_;
    bank.look = fogLook_;
    bank.scene.boardCenter = {boardCenter_.x, boardCenter_.z};
    bank.scene.boardRadius = boardRadius_;
    bank.scene.meanGround = meanGround_;
    bank.scene.groundLow = groundLow_;
    bank.scene.groundHigh = boardTop_;
    const e2::Board::Bounds bb = board_->bounds();
    bank.scene.board = {bb.x0, bb.z0, bb.x1, bb.z1};
    bank.scene.partyCenter = partyCenter();
    renderer.setFogBank(bank);
    renderer.setBloom(bloomOn_ ? (board_->draft().bloomIntensity > 0.0f ? board_->draft().bloomIntensity : 0.16f) : 0.0f);
    renderer.waitFor(uploads_.timeline(),
                     std::max({ground_.uploadWaitValue(), sprites_.uploadWaitValue(), meshes_.uploadWaitValue()}));
    if (ground_.ready() && sceneReady() && (!fog_ || fogReady_)) ++readyFrames_;
    if (options_.measureSeconds > 0.0f && !measuring_ && !measured_ && readyFrames_ >= 30) {
      measuring_ = true;
      measureStart_ = std::chrono::steady_clock::now();
    }
  }

  void endFrame(double busyMs) override {
    if (!measuring_) return;
    if (++measureFrames_ <= 4) return;  // the GPU result is a few frames old
    double gpu = 0.0;
    for (const auto& r : renderer_.gpuTimer().results()) {
      if (std::string_view(r.name) == "frame") gpu = r.milliseconds;
      auto it = std::find_if(passes_.begin(), passes_.end(), [&](const auto& p) { return p.first == r.name; });
      if (it == passes_.end()) it = passes_.insert(passes_.end(), {std::string(r.name), 0.0});
      it->second += r.milliseconds;
      auto& worst = passWorst_[std::string(r.name)];
      worst.first = std::max(worst.first, r.milliseconds);
      ++worst.second;
    }
    busy_.push_back(busyMs);
    gpu_.push_back(gpu);
    if (fogUpdatedThisFrame_) fogUpdateMs_.push_back(lastFogMs_);
    fogUpdatedThisFrame_ = false;
  }

  // Mouse: right-drag turns and tilts around the pivot, middle-drag grabs the ground and slides the board, the wheel
  // zooms toward the point under the cursor. The left button stays free for the game. Clicks on the panel and the
  // camera bar never reach here (ImGui takes them).
  void event(const SDL_Event& e) override {
    if (e.type == SDL_EVENT_MOUSE_MOTION) mouse_ = {e.motion.x, e.motion.y}, mouseWindow_ = e.motion.windowID;
    // Left button, Engine2's protected rule (cameraRig): held still for 0.5 s it arms panning (the grabbing hand) and
    // the board then follows the drag; a short click (released before, within 5 px) is a game click.
    if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_LEFT)
      leftDown_ = LeftPress{e.button.x, e.button.y, std::chrono::steady_clock::now()};
    if (e.type == SDL_EVENT_MOUSE_BUTTON_UP && e.button.button == SDL_BUTTON_LEFT) {
      const bool wasPanning = panning_;
      endPan();
      if (leftDown_ && !wasPanning && std::hypot(e.button.x - leftDown_->x, e.button.y - leftDown_->y) <= kClickSlop)
        click(e.button.windowID, e.button.x, e.button.y);
      leftDown_.reset();
    }
    if (e.type == SDL_EVENT_MOUSE_MOTION && panning_ && (e.motion.state & SDL_BUTTON_LMASK)) {
      const std::optional<glm::vec3> a = groundUnder(e.motion.windowID, e.motion.x - e.motion.xrel, e.motion.y - e.motion.yrel);
      const std::optional<glm::vec3> b = groundUnder(e.motion.windowID, e.motion.x, e.motion.y);
      if (a && b) target_ += glm::vec3{a->x - b->x, 0.0f, a->z - b->z}, cam_.target += glm::vec3{a->x - b->x, 0.0f, a->z - b->z};
    }
    if (e.type == SDL_EVENT_KEY_DOWN && !e.key.repeat && e.key.key == SDLK_ESCAPE) {
      if (!spellMode_.empty()) spellMode_.clear(), spellCells_.clear(), spellAim_.clear();
      else if (attackMode_) attackMode_ = false;
      else if (selected_ >= 0 && result_.empty()) select(-1);
      else pauseOpen_ = !pauseOpen_;  // nothing picked: the pause menu (Continuar / Menu principal)
    }
    if (e.type == SDL_EVENT_MOUSE_MOTION && (e.motion.state & SDL_BUTTON_RMASK) && !flat_) {  // Tática never turns
      yaw_ -= e.motion.xrel * kTurnPerPixel;
      pitch_ = std::clamp(pitch_ + e.motion.yrel * kTurnPerPixel, kMinPitch, kMaxPitch);
    }
    if (e.type == SDL_EVENT_MOUSE_MOTION && (e.motion.state & SDL_BUTTON_MMASK)) {
      // The ground point under the cursor stays under it (the board follows the hand).
      const std::optional<glm::vec3> a = groundUnder(e.motion.windowID, e.motion.x - e.motion.xrel, e.motion.y - e.motion.yrel);
      const std::optional<glm::vec3> b = groundUnder(e.motion.windowID, e.motion.x, e.motion.y);
      if (a && b) target_ += glm::vec3{a->x - b->x, 0.0f, a->z - b->z}, cam_.target += glm::vec3{a->x - b->x, 0.0f, a->z - b->z};
    }
    if (e.type == SDL_EVENT_MOUSE_WHEEL && e.wheel.y != 0.0f) {
      const float next = std::clamp(distance_ * std::pow(kZoomStep, e.wheel.y), kMinDistance, maxDistance_);
      // Toward the cursor: the pivot moves to the cursor's ground point by the share of the distance zoomed.
      if (const std::optional<glm::vec3> p = groundUnder(e.wheel.windowID, e.wheel.mouse_x, e.wheel.mouse_y)) {
        const float share = 1.0f - next / distance_;
        target_.x += (p->x - target_.x) * share;
        target_.z += (p->z - target_.z) * share;
      }
      distance_ = next;
    }
  }

  // The left button held still long enough arms the pan (called every frame).
  void updatePanHold() {
    if (!leftDown_ || panning_) return;
    if (std::chrono::steady_clock::now() - leftDown_->at < std::chrono::milliseconds(kHoldMs)) return;
    panning_ = true;
    if (!grabCursor_) grabCursor_ = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_MOVE);
    if (grabCursor_) SDL_SetCursor(grabCursor_);
  }
  void endPan() {
    if (!panning_) return;
    panning_ = false;
    SDL_SetCursor(SDL_GetDefaultCursor());
  }

  // Arrows / WASD slide the board along the view (level, whatever the tilt), faster when zoomed out; Shift doubles.
  void panKeys(float dt) {
    if (ImGui::GetCurrentContext() && ImGui::GetIO().WantTextInput) return;  // only typing into a text field takes the keys
    const bool* k = SDL_GetKeyboardState(nullptr);
    if (!k) return;
    const float yaw = flat_ ? 0.0f : yaw_;  // Tática: up on screen is the board's far side
    const glm::vec3 forward{-std::sin(yaw), 0.0f, -std::cos(yaw)}, right{std::cos(yaw), 0.0f, -std::sin(yaw)};
    glm::vec3 move{0.0f};
    if (k[SDL_SCANCODE_W] || k[SDL_SCANCODE_UP]) move += forward;
    if (k[SDL_SCANCODE_S] || k[SDL_SCANCODE_DOWN]) move -= forward;
    if (k[SDL_SCANCODE_D] || k[SDL_SCANCODE_RIGHT]) move += right;
    if (k[SDL_SCANCODE_A] || k[SDL_SCANCODE_LEFT]) move -= right;
    if (glm::length(move) < 1e-4f) return;
    const float fast = k[SDL_SCANCODE_LSHIFT] || k[SDL_SCANCODE_RSHIFT] ? 2.0f : 1.0f;
    target_ += glm::normalize(move) * std::max(distance_, 6.0f) * kPanPerSecond * fast * dt;
  }

  // The board never leaves the screen: the pivot stays on the board, so at most the edge reaches the screen's
  // centre (the edge hexes whole, from any angle); tilt and zoom stay in range.
  void clampGoal() {
    const e2::Board::Bounds b = board_->bounds();
    target_.x = std::clamp(target_.x, static_cast<float>(b.x0), static_cast<float>(b.x1));
    target_.z = std::clamp(target_.z, static_cast<float>(b.z0), static_cast<float>(b.z1));
    pitch_ = std::clamp(pitch_, kMinPitch, kMaxPitch);
    distance_ = std::clamp(distance_, kMinDistance, maxDistance_);
  }

  struct OrbitView {
    glm::vec3 target{0.0f};
    float yaw = 0.0f, pitch = 0.0f, distance = 1.0f;
  };
  static glm::vec3 eyeOf(const OrbitView& v) {
    return v.target + glm::vec3{v.distance * std::cos(v.pitch) * std::sin(v.yaw), v.distance * std::sin(v.pitch),
                                v.distance * std::cos(v.pitch) * std::cos(v.yaw)};
  }
  float groundAt(float x, float z) const { return static_cast<float>(board_->cellAt(x, z).groundY); }
  // The pivot's height: the ground averaged over about a hex around it, so it glides over steps instead of jumping.
  float pivotHeight(glm::vec3 p) const {
    float sum = 0.0f;
    for (int i = 0; i < 7; ++i) {
      const float a = static_cast<float>(i) * 1.0472f, r = i == 6 ? 0.0f : 1.2f;
      sum += groundAt(p.x + r * std::cos(a), p.z + r * std::sin(a));
    }
    return sum / 7.0f;
  }
  // The highest solid thing at (x, z): the ground on the board, and the top of any 3D piece standing there.
  float clearanceAt(float x, float z) const {
    const e2::Board::Bounds b = board_->bounds();
    float top = x < b.x0 || x > b.x1 || z < b.z0 || z > b.z1 ? groundLow_ : groundAt(x, z);
    for (const PieceBox& p : pieceBoxes_)
      if (std::abs(x - p.x) < p.radius && std::abs(z - p.z) < p.radius) top = std::max(top, p.top);
    return top;
  }
  // The ground point under a window position (window coordinates), on the pivot's level; none above the horizon.
  std::optional<glm::vec3> groundUnder(SDL_WindowID id, float mx, float my) const {
    int w = 0, h = 0;
    SDL_Window* window = SDL_GetWindowFromID(id);
    if (!window || !SDL_GetWindowSize(window, &w, &h) || w <= 0 || h <= 0) return std::nullopt;
    const float tanHalf = std::tan(glm::radians((flat_ ? kFlatFov : kFov) * 0.5f));
    const float nx = 2.0f * mx / static_cast<float>(w) - 1.0f, ny = 1.0f - 2.0f * my / static_cast<float>(h);
    const glm::vec3 up = glm::cross(lastCamera_.right, lastCamera_.forward);
    const glm::vec3 dir = glm::normalize(lastCamera_.forward + lastCamera_.right * (nx * tanHalf * aspect_) + up * (ny * tanHalf));
    if (dir.y > -1e-3f) return std::nullopt;
    const float t = (cam_.target.y - lastCamera_.position.y) / dir.y;
    return lastCamera_.position + dir * t;
  }

  // A dropdown: `count` choices named by name(i); true when the choice changed.
  template <typename Name>
  static bool dropdown(const char* label, int& current, int count, Name name) {
    bool changed = false;
    if (ImGui::BeginCombo(label, name(current).c_str())) {
      for (int i = 0; i < count; ++i) {
        const bool selected = i == current;
        if (ImGui::Selectable(name(i).c_str(), selected)) current = i, changed = true;
        if (selected) ImGui::SetItemDefaultFocus();
      }
      ImGui::EndCombo();
    }
    return changed;
  }

  // The window shows only the choices, in plain Portuguese words, large (user, 2026-10-15: "the only information I
  // want on that screen are the CHOICES, very visible"; no numbers, no statistics). Look numbers stay in the code
  // and the looks/ READMEs.
  // Engine2's ember-plate (src/ember-ui/styles.css): a near-black gradient (rgba 28,28,28,.94 to 10,10,10,.95), a 1 px
  // gold border (190,150,95,.45) inside a 1 px black ring, an 8 px inner shadow, a soft drop shadow 6 px down, 2 px
  // corners; on hover the text turns #ffe0b4, the border (226,170,100,.85) and an orange glow (255,110,40,.3) come on.
  // paint(drawList, min, max, ink) draws the face. True when clicked.
  template <typename Paint>
  static bool plate(const char* id, ImVec2 size, Paint paint) {
    const bool clicked = ImGui::InvisibleButton(id, size);
    const bool hover = ImGui::IsItemHovered();
    const ImVec2 a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    // Drop shadow (0 6px 18px rgba(0,0,0,.6)) and, on hover, the glow (0 0 14px rgba(255,110,40,.3)): blurred by layers.
    for (int i = 6; i >= 1; --i) {
      const float spread = 1.5f * static_cast<float>(i);
      dl->AddRectFilled({a.x - spread, a.y + 6.0f - spread}, {b.x + spread, b.y + 6.0f + spread}, IM_COL32(0, 0, 0, 14), 2.0f + spread);
    }
    if (hover)
      for (int i = 5; i >= 1; --i) {
        const float spread = 1.4f * static_cast<float>(i);
        dl->AddRectFilled({a.x - spread, a.y - spread}, {b.x + spread, b.y + spread}, IM_COL32(255, 110, 40, 12), 2.0f + spread);
      }
    dl->AddRect({a.x - 1.0f, a.y - 1.0f}, {b.x + 1.0f, b.y + 1.0f}, IM_COL32(0, 0, 0, 255), 3.0f);  // 0 0 0 1px #000
    const ImU32 top = IM_COL32(28, 28, 28, 240), bottom = IM_COL32(10, 10, 10, 242);
    dl->AddRectFilledMultiColor(a, b, top, top, bottom, bottom);
    for (int i = 0; i < 4; ++i) {  // inset 0 0 8px rgba(0,0,0,.85)
      const float d = 1.0f + 2.0f * static_cast<float>(i);
      dl->AddRect({a.x + d, a.y + d}, {b.x - d, b.y - d}, IM_COL32(0, 0, 0, 90 - 20 * i), 1.0f, 2.0f);
    }
    dl->AddRect(a, b, hover ? IM_COL32(226, 170, 100, 217) : IM_COL32(190, 150, 95, 115), 2.0f);
    paint(dl, a, b, hover ? IM_COL32(255, 224, 180, 255) : IM_COL32(240, 240, 240, 255));
    return clicked;
  }

  // A lucide icon's 24 px box drawn at 14 px (Engine2: size-3.5), centred on the button, stroke 2 at that scale.
  struct IconBox {
    ImVec2 origin;
    float scale = 14.0f / 24.0f;
    IconBox(ImVec2 a, ImVec2 b) : origin{(a.x + b.x) * 0.5f - 7.0f, (a.y + b.y) * 0.5f - 7.0f} {}
    ImVec2 at(float x, float y) const { return {origin.x + x * scale, origin.y + y * scale}; }
    float stroke() const { return std::max(1.0f, 2.0f * scale); }
  };
  // lucide Chevron*: a polyline of three points in the 24 px box.
  static auto chevron(float x0, float y0, float x1, float y1, float x2, float y2) {
    return [=](ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 ink) {
      const IconBox box(a, b);
      const ImVec2 p[3] = {box.at(x0, y0), box.at(x1, y1), box.at(x2, y2)};
      dl->AddPolyline(p, 3, ink, box.stroke());
    };
  }
  // Text with Engine2's 0.14 em letter spacing (tracking-[0.14em]), one glyph at a time.
  static float spacedWidth(const char* text) {
    float w = 0.0f;
    int glyphs = 0;
    for (const char* p = text; *p;) {
      const char* next = p + 1;
      while ((*next & 0xC0) == 0x80) ++next;  // whole UTF-8 sequence
      w += ImGui::CalcTextSize(p, next).x;
      ++glyphs;
      p = next;
    }
    return w + kPlateText * 0.14f * static_cast<float>(std::max(0, glyphs - 1));
  }
  static void drawSpaced(ImDrawList* dl, ImVec2 at, ImU32 ink, const char* text) {
    for (const char* p = text; *p;) {
      const char* next = p + 1;
      while ((*next & 0xC0) == 0x80) ++next;
      dl->AddText(at, ink, p, next);
      at.x += ImGui::CalcTextSize(p, next).x + kPlateText * 0.14f;
      p = next;
    }
  }

  // The camera bar, top right: Engine2's battle HUD (GameApp.tsx), copied 1:1 at the user's request (2026-10-15).
  // Normal / Tática (the 2D view), tilt down / up 5 deg, turn left / right 15 deg, back to the normal angle (leaves
  // Tática; zoom and pan stay, like Engine2's resetAngle). Each click glides.
  void drawCameraBar() {
    const ImGuiIO& io = ImGui::GetIO();
    // The window's padding (room for the shadow) sits outside the 8 px inset.
    ImGui::SetNextWindowPos({io.DisplaySize.x - kBarInset + kPlateGlow, kBarInset - kPlateGlow}, ImGuiCond_Always, {1.0f, 0.0f});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {kPlateGlow, kPlateGlow});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {kBarGap, kBarGap});
    ImGui::Begin("##camerabar", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBackground |
                     ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoSavedSettings);
    ImGui::PushFont(nullptr, kPlateText);
    // Engine2: h-7 px-2, 10 px uppercase, letter spacing 0.14 em.
    const char* label = flat_ ? "TÁTICA" : "NORMAL";
    if (plate("##view", {spacedWidth(label) + 16.0f, kPlate}, [&](ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 ink) {
          const float w = spacedWidth(label);
          drawSpaced(dl, {std::round((a.x + b.x - w) * 0.5f), std::round((a.y + b.y - ImGui::GetTextLineHeight()) * 0.5f)}, ink, label);
        }))
      setFlat(!flat_);
    ImGui::SameLine();
    if (plate("##down", {kPlate, kPlate}, chevron(6, 9, 12, 15, 18, 9))) pitch_ = std::clamp(pitch_ - kTiltStep, kMinPitch, kMaxPitch);
    ImGui::SameLine();
    if (plate("##up", {kPlate, kPlate}, chevron(18, 15, 12, 9, 6, 15))) pitch_ = std::clamp(pitch_ + kTiltStep, kMinPitch, kMaxPitch);
    ImGui::SameLine();
    if (plate("##left", {kPlate, kPlate}, chevron(15, 18, 9, 12, 15, 6))) yaw_ += kTurnStep;
    ImGui::SameLine();
    if (plate("##right", {kPlate, kPlate}, chevron(9, 18, 15, 12, 9, 6))) yaw_ -= kTurnStep;
    ImGui::SameLine();
    if (plate("##reset", {kPlate, kPlate}, [](ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 ink) {
          // lucide RotateCcw: "M3 12a9 9 0 1 0 9-9 9.75 9.75 0 0 0-6.74 2.74L3 8" + "M3 3v5h5" (24 px box, 14 px icon)
          const IconBox box(a, b);
          dl->PathArcTo(box.at(12, 12), 9.0f * box.scale, std::numbers::pi_v<float>, -0.75f * std::numbers::pi_v<float>, 24);
          dl->PathLineTo(box.at(3, 8));
          dl->PathStroke(ink, box.stroke());
          const ImVec2 arrow[3] = {box.at(3, 3), box.at(3, 8), box.at(8, 8)};
          dl->AddPolyline(arrow, 3, ink, box.stroke());
        })) {
      if (flat_) setFlat(false);
      // The shortest way round to the normal turn; zoom and pan stay.
      const float twoPi = 2.0f * std::numbers::pi_v<float>;
      yaw_ = homeYaw_ + twoPi * std::round((yaw_ - homeYaw_) / twoPi);
      pitch_ = homePitch_;
    }
    ImGui::PopFont();
    ImGui::End();
    ImGui::PopStyleVar(3);
  }

  // Tática on / off (Engine2 DioramaView.setFlat + Battle3D): leaving it lands on the normal view's angle at once;
  // zoom and pan stay where they are.
  void setFlat(bool on) {
    if (on == flat_) return;
    flat_ = on;
    if (on) return;
    const float twoPi = 2.0f * std::numbers::pi_v<float>;
    yaw_ = homeYaw_ + twoPi * std::round((yaw_ - homeYaw_) / twoPi);
    pitch_ = homePitch_;
    cam_.yaw = yaw_, cam_.pitch = pitch_;
  }

  void drawPanel() override {
    if (!board_) return;
    if (!result_.empty() && now_ - resultAt_ > kResultDelay) return drawResultScreen();
    drawAirParticles();
    drawMarks();
    drawCameraBar();
    drawTurnBar();
    drawPauseMenu();
    ImGui::SetNextWindowPos({8, 44}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowCollapsed(true, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowBgAlpha(0.9f);
    ImGui::Begin("Escolhas", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing);
    ImGui::PushFont(nullptr, 24.0f);  // large (ImGui 1.93: size per push)
    ImGui::PushItemWidth(220.0f);
    if (!poseChoices_.empty())
      dropdown("Animação", poseChoice_, static_cast<int>(poseChoices_.size()), [this](int i) { return poseChoices_[static_cast<size_t>(i)].second; });
    {
      int look = static_cast<int>(look_);
      static const char* kLooks[3] = {"suave", "hexágonos", "mesa"};
      if (dropdown("Chão", look, 3, [](int i) { return std::string(kLooks[i]); })) look_ = static_cast<e2::EdgeStyle>(look);
    }
    {
      static const char* kBackgrounds[3] = {"imagem do mapa", "céu", "OFF"};
      dropdown("Fundo", background_, 3, [](int i) { return std::string(kBackgrounds[i]); });
    }
    {
      static const char* kModes[4] = {"protótipo", "Engine2", "física", "real"};
      dropdown("Luz", lightingMode_, 4, [](int i) { return std::string(kModes[i]); });
    }
    {
      int bloom = bloomOn_ ? 1 : 0;
      if (dropdown("Brilho", bloom, 2, [](int i) { return std::string(i == 0 ? "OFF" : "Engine2"); })) bloomOn_ = bloom == 1;
    }
    {
      static const char* kLayers[4] = {"sol / lua", "sombras", "luz do céu", "tochas e fogo"};
      for (uint32_t i = 0; i < 4; ++i) {
        bool on = (layers_ >> i) & 1u;
        if (i) ImGui::SameLine();
        if (ImGui::Checkbox(kLayers[i], &on)) layers_ ^= 1u << i;
      }
    }
    if (scene_ && e2::timeOfDay(scene_->timeOfDay).moon) {
      static const char* kPaths[3] = {"Engine2", "atrás", "na frente"};
      int moon = moonOff_ ? 0 : moonPath_ + 1;
      if (dropdown("Lua", moon, 4, [](int i) { return i == 0 ? std::string("OFF") : std::string(kPaths[i - 1]); })) {
        moonOff_ = moon == 0;
        if (!moonOff_) moonPath_ = moon - 1;
      }
      // moonPhase.ts MOON_PHASE_LABEL, in kMoonPhases order
      static const char* kPhases[9] = {"nova", "crescente", "quarto cresc.", "gibosa cresc.", "cheia",
                                       "gibosa ming.", "quarto ming.", "minguante", "sangue"};
      int phase = moonPhase_ + 1;
      if (dropdown("Fase da lua", phase, 10,
                   [](int i) { return i == 0 ? std::string("padrão") : std::string(kPhases[i - 1]); }))
        moonPhase_ = phase - 1;
      static const char* kSizes[4] = {"pequena", "média", "grande", "enorme"};
      dropdown("Tamanho da lua", moonSize_, 4, [](int i) { return std::string(kSizes[i]); });
      static const char* kStars[kStarLooks] = {"quadradas", "poucas", "muitas"};
      int star = starsOff_ ? 0 : starLook_;
      if (dropdown("Estrelas", star, kStarLooks + 1, [](int i) { return i == 0 ? std::string("OFF") : std::string(kStars[i - 1]); })) {
        starsOff_ = star == 0;
        if (!starsOff_) starLook_ = star;
      }
    }
    if (fog_ && options_.volume) {
      // First "no fog at all" (no mist and no fog of war: the whole map seen, every unit drawn), then the saved
      // mist looks (looks/fog/README.md, in order).
      static const char* kFog[7] = {"nuvens", "lençol", "bancos", "bloco", "bloco + giro", "mist4", "mist4 + giro"};
      int look = noFog_ ? 0 : static_cast<int>(fogLook_) + 1;
      const int count = static_cast<int>(fogLooks().size());
      if (dropdown("Névoa", look, count + 1, [count](int i) {
            if (i == 0) return std::string("OFF");
            return count == 7 ? std::string(kFog[i - 1]) : std::string(fogLooks()[i - 1].note);
          })) {
        noFog_ = look == 0;
        if (!noFog_) fogLook_ = static_cast<uint32_t>(look - 1);
      }
    }
    ImGui::PopItemWidth();
    ImGui::PopFont();
    ImGui::End();
  }

  // The player asked to leave for the title screen (pause menu, or after the battle's end), or to fight it again.
  bool wantsTitle() const { return titleRequested_; }
  bool wantsRetry() const { return retryRequested_; }
  const MapStageOptions& options() const { return options_; }

  bool done() const override { return failed_ || (options_.measureSeconds > 0.0f ? measured_ : readyFrames_ >= 30); }
  bool passed() const override { return !failed_ && ground_.ready() && measurePassed_; }

 private:
  static constexpr float kFov = 30.0f, kNear = 0.5f;  // Engine2's normal lens
  static constexpr float kFlatFov = 1.0f;             // Engine2 FLAT_FOV: Tática's long lens
  static constexpr float kFlatRowStep = 0.002f;       // Engine2 FLAT_ROW_STEP: Tática's height step per unit of depth
  // The camera bar (Engine2 GameApp.tsx + ember-plate): 28 px plates (size-7), 4 px apart (gap-1), 8 px from the
  // corner (inset-x-2, top 0.5rem), 10 px text; the window's padding leaves room for the shadow and glow.
  static constexpr float kPlate = 28.0f, kBarGap = 4.0f, kBarInset = 8.0f, kPlateText = 10.0f, kPlateGlow = 10.0f;
  static constexpr float kFlameEmit = 2.5f;           // Engine2 FLAME_EMIT
  // Light mode "real" (ARCHITECTURE 7.6 step A).
  static constexpr float kSunLux = 128000.0f;       // the sun above the atmosphere
  static constexpr float kFullMoonLux = 0.5f;       // a full moon (real 0.25, lifted so moonlit ground reads)
  static constexpr float kIndoorFillLux = 3.0f, kIndoorAmbientLux = 1.5f;
  static constexpr float kLumensPerUnit = 12.0f;    // Engine2 intensity x reach^2 -> lumens
  static constexpr float kNightStops = 2.5f;        // the camera's darkening at night
  static constexpr float kAdaptPerSecond = 1.5f;    // eye adaptation rate

  // Median, 99th percentile and worst; frames over the 60 Hz budget.
  struct Spread {
    double median = 0, p99 = 0, worst = 0;
    size_t over = 0;
  };
  static Spread spread(std::vector<double> v) {
    Spread s;
    if (v.empty()) return s;
    std::sort(v.begin(), v.end());
    s.median = v[v.size() / 2];
    s.p99 = v[std::min(v.size() - 1, v.size() * 99 / 100)];
    s.worst = v.back();
    s.over = static_cast<size_t>(std::count_if(v.begin(), v.end(), [](double x) { return x > 1000.0 / 60.0; }));
    return s;
  }

  void finishMeasure() {
    measuring_ = false;
    measured_ = true;
    const Spread cpu = spread(busy_), gpu = spread(gpu_);
    const size_t n = busy_.size();
    measurePassed_ = n > 0 && cpu.worst <= 1000.0 / 60.0 && gpu.worst <= 1000.0 / 60.0;
    logInfo("measure {}: {} frames over {:.0f} s (camera turn + zoom, {} units animating, {} lights)", scene_ ? scene_->map : std::string("map"), n,
            options_.measureSeconds, scene_ ? scene_->units.size() : 0, lights_.size());
    logInfo("  main thread: median {:.2f}, 99th {:.2f}, worst {:.2f} ms; {} frames over 16.7 ms", cpu.median, cpu.p99, cpu.worst, cpu.over);
    logInfo("  GPU:         median {:.2f}, 99th {:.2f}, worst {:.2f} ms; {} frames over 16.7 ms", gpu.median, gpu.p99, gpu.worst, gpu.over);
    std::string line = "  GPU passes (average):";
    for (const auto& [name, sum] : passes_) line += std::format(" {} {:.2f}", name, sum / std::max<size_t>(1, n));
    logInfo("{}", line);
    line = "  GPU passes (worst, frames run):";
    for (const auto& [name, w] : passWorst_) line += std::format(" {} {:.2f} ({})", name, w.first, w.second);
    logInfo("{}", line);
    if (fog_) {
      const Spread fu = spread(fogUpdateMs_);
      logInfo("  fog of war: worst {} enclosed never-seen holes at once", worstHoles_);
      const Spread fd = spread(fogDeliverMs_);
      logInfo("  fog of war: {} mask updates (sight + mask on a worker): median {:.2f}, worst {:.2f} ms", fogUpdateMs_.size(), fu.median,
              fu.worst);
      logInfo("  fog of war: taking each result on the main thread: median {:.2f}, worst {:.2f} ms", fd.median, fd.worst);
    }
    logInfo("  {}", measurePassed_ ? "PASS: every frame within 16.7 ms" : "FAIL: frames over the 16.7 ms budget");
  }

  bool sceneReady() const {
    if (!scene_) return true;
    return sprites_.ready() && modelsPlaced_ && fullFrames_ > 0;
  }

  float groundOf(float smooth, float hard) const { return look_ == e2::EdgeStyle::Smooth ? smooth : hard; }

  // Engine2's flickerAt (src/ember/lighting.ts).
  static float flicker(float time, float seed, float amount) {
    if (amount <= 0.0f) return 1.0f;
    const float v = 0.06f * std::sin(time * 5.3f + seed) + 0.045f * std::sin(time * 8.9f + seed * 1.7f) +
                    0.03f * std::sin(time * 13.7f + seed * 2.9f) + 0.03f * std::sin(time * 2.1f + seed * 0.6f);
    return 1.0f + v * amount;
  }

  // Engine2's models.ts: scaled uniformly to their height, base centred on the anchor, turned by rotY.
  void placeModels() {
    modelsPlaced_ = true;
    std::vector<InstanceGpu> instances;
    pieceBoxes_.clear();
    for (const e2::SceneModel& m : scene_->models) {
      std::string asset = m.file.substr(std::string("/game/").size());
      asset = "meshes/" + asset.substr(0, asset.size() - 4) + ".mesh";
      const MeshHeader* h = meshes_.header(modelMeshes_.at(asset));
      if (!h) {
        logWarn("scene: model {} is not in the mesh pack", asset);
        continue;
      }
      const float k = m.height / std::max(h->boundsMax[1] - h->boundsMin[1], 1e-4f);
      const glm::vec3 offset{-(h->boundsMin[0] + h->boundsMax[0]) * 0.5f * k, -h->boundsMin[1] * k, -(h->boundsMin[2] + h->boundsMax[2]) * 0.5f * k};
      const float c = std::cos(m.rotY), sn = std::sin(m.rotY);
      // three.js rotation about y: x' = x cos + z sin, z' = -x sin + z cos
      const glm::vec3 t{c * offset.x + sn * offset.z + m.x, offset.y + groundOf(m.groundSmooth, m.groundHard), -sn * offset.x + c * offset.z + m.z};
      InstanceGpu inst{};
      inst.rows[0] = {c * k, 0, sn * k, t.x};
      inst.rows[1] = {0, k, 0, t.y};
      inst.rows[2] = {-sn * k, 0, c * k, t.z};
      inst.mesh = modelMeshes_.at(asset);
      inst.scale = k;
      instances.push_back(inst);
      const float half = 0.5f * k * std::max(h->boundsMax[0] - h->boundsMin[0], h->boundsMax[2] - h->boundsMin[2]);
      pieceBoxes_.push_back({m.x, m.z, half, t.y + m.height});
    }
    modelCount_ = static_cast<uint32_t>(instances.size());
    meshes_.setInstances(instances);
    placedLook_ = look_;
  }

  // One card: the whole art spans w x h world units with its bottom centre at `local` (card-local x along
  // `right`, y up, z along the facing); the cooked clip holds a trimmed part of it.
  void addCard(SpriteClipId clip, glm::vec3 anchor, glm::vec3 right, float w, float h, glm::vec3 local, bool mirror, uint32_t frame,
               uint32_t flags, const SpriteInstance* emit) {
    sprites_.request(clip, SpriteSystem::kOnScreen);
    const SpriteSystem::Texture tex = sprites_.texture(clip);
    if (tex.index == kNoTexture) return;
    if (tex.full) ++fullFrames_;
    const SpriteClipHeader& c = sprites_.header(clip);
    const float sw = static_cast<float>(c.sourceWidth), sh = static_cast<float>(c.sourceHeight);
    const float cx = (static_cast<float>(c.cropX) + c.contentWidth * 0.5f) / sw - 0.5f;
    const float cy = 1.0f - (static_cast<float>(c.cropY) + c.contentHeight) / sh;
    const float x = (local.x + cx * w) * (mirror ? -1.0f : 1.0f);
    const bool flat = (flags & kSpriteFlat) != 0;  // lying face up: its height runs toward -z
    const glm::vec3 facing = flat ? glm::vec3(0.0f, 1.0f, 0.0f) : glm::vec3(-right.z, 0.0f, right.x);
    const glm::vec3 up = flat ? glm::vec3(0.0f, 0.0f, -1.0f) : glm::vec3(0.0f, 1.0f, 0.0f);
    SpriteInstance s{};
    s.origin = anchor + right * x + up * (local.y + cy * h) + facing * local.z;
    s.width = w * c.contentWidth / sw;
    s.height = h * c.contentHeight / sh;
    s.textureIndex = tex.index;
    s.layer = std::min(frame, c.frames - 1);
    s.flags = flags | (mirror ? kSpriteMirrored : 0u);
    s.uvScale = {static_cast<float>(c.contentWidth) / c.width, static_cast<float>(c.contentHeight) / c.height};
    s.right = {right.x, right.z};
    if (emit) {
      // The light's place on the art (full-image uv, v up) in this texture's uv, and texture uv -> card heights.
      s.emitColor = emit->emitColor;
      s.emitLevel = emit->emitLevel;
      s.emitAt = {(emit->emitAt.x * sw - c.cropX) / c.contentWidth * s.uvScale.x, ((1.0f - emit->emitAt.y) * sh - c.cropY) / c.contentHeight * s.uvScale.y,
                  (static_cast<float>(c.contentWidth) / sw) / s.uvScale.x * (w / h)};
      s.emitScaleV = (static_cast<float>(c.contentHeight) / sh) / s.uvScale.y;
    }
    drawList_.push_back(s);
  }

  void buildCards(const Camera& camera, float time) {
    missing_ = 0;
    fullFrames_ = 0;
    if (placedLook_ != look_ && modelsPlaced_) placeModels();
    // Decorations: Engine2's DecorLayer. Cards stand square to the hex rows (LOCK_PROPS) or along their own
    // turn (barricades); the art's opaque base sits on the anchor; Ember's layer step moves them along their facing.
    for (const e2::SceneCard& d : scene_->cards) {
      if (d.onFloor) continue;  // waypoints lying on the floor: not drawn yet
      const std::string name = std::filesystem::path(d.image).stem().string();
      const SpriteClipId clip = sprites_.find("decor", name);
      if (clip == kNoSpriteClip) {
        ++missing_;
        continue;
      }
      // Tática (Engine2 DecorLayer.face with flatTop): every card lies flat square to the screen, just over the highest
      // ground, each row of depth a step higher so nearer rows cover farther ones; no layer step, no shadow.
      const float yaw = flat_ ? 0.0f : d.lockedYaw.value_or(0.0f);
      const glm::vec3 right{std::cos(yaw), 0.0f, -std::sin(yaw)};
      const glm::vec3 anchor{d.x, flat_ ? boardTop_ + d.z * kFlatRowStep : groundOf(d.groundSmooth, d.groundHard), d.z};
      const glm::vec3 local{(0.5f - d.art.baseU) * d.w, -d.art.baseV * d.h, flat_ ? 0.0f : d.nudge};
      SpriteInstance emit{};
      if (d.light) {
        emit.emitColor = {d.light->color[0], d.light->color[1], d.light->color[2]};
        emit.emitLevel = kFlameEmit * flicker(time, d.light->seed, d.light->flicker);
        if (lightingMode_ == 3) emit.emitLevel *= flameGlow() / std::max(exposure_, 1e-9f);
        emit.emitAt = {d.art.flameFound ? d.art.flameU : 0.5f, d.art.flameFound ? d.art.flameV : 0.7f, 0.0f};
      }
      const uint32_t flags = flat_ ? kSpriteLit | kSpriteFlat : kSpriteLit | (d.castShadow ? kSpriteCastsShadow : 0u);
      addCard(clip, anchor, right, d.w, d.h, local, d.mirror, 0, flags, d.light ? &emit : nullptr);
    }
    // Units: the pose chosen in the panel from each unit's own catalog entry (its idle when it has none). They turn
    // to face the camera; Engine2's box for the pose, its foot line on the ground, the frame paced by Engine2's
    // framePace. One-shot poses (attacks, casts) play through, hold half a second, and start again.
    const std::string& chosen = poseChoices_.empty() ? kIdle : poseChoices_[static_cast<size_t>(poseChoice_)].first;
    glm::vec3 right = camera.right;
    right.y = 0.0f;
    right = glm::length(right) > 0.0f && !flat_ ? glm::normalize(right) : glm::vec3(1, 0, 0);
    hiddenUnits_ = 0;
    for (CardRect& c : unitCard_) c.drawn = false;
    for (size_t ui = 0; ui < scene_->units.size(); ++ui) {
      const e2::SceneUnit& u = scene_->units[ui];
      // Fog of war: a unit not in the party's sight is not drawn at all (and casts no shadow).
      if (fog_ && !noFog_ && u.side != "player") {
        const auto [c, r] = e2::hexAt(board_->layout(), unitPos_[ui].x, unitPos_[ui].y);
        // The states the shown mask was painted from (all hidden until the first mask arrives).
        const size_t i = static_cast<size_t>(e2::indexOf(board_->layout(), c, r));
        if (i >= fogStates_.size() || fogStates_[i] != e2::kFogVisible) {
          ++hiddenUnits_;
          continue;
        }
      }
      // The scene's own idle unless the unit's catalog entry has the chosen pose. The mirror stays the scene's:
      // its side's facing against the unit's drawn direction, which is the same for every pose.
      // A walking unit shows Engine2's walk for its direction (UnitActor.walkShown), timed from the walk's start.
      const e2::Hero* entry = ui < unitCatalog_.size() ? unitCatalog_[ui] : nullptr;
      const bool walking = !unitWalk_[ui].points.empty();
      std::string shown = kIdle;
      const e2::HeroPose* p = nullptr;
      // The fallen: their death plays out, holds, then they are gone.
      if (ui < unitDiedAt_.size() && unitDiedAt_[ui] >= 0.0f && time - unitDiedAt_[ui] > poseSeconds(ui, "death") + kCorpseSeconds) continue;
      const OneShot* shot = ui < unitShot_.size() && !unitShot_[ui].pose.empty() ? &unitShot_[ui] : nullptr;
      if (shot && entry) {
        // Engine2 UnitActor.play: the left-facing cut when it faces left and has one.
        const std::string left = shot->pose + "Left";
        const e2::HeroPose* l = unitFacing_[ui] == -1 ? entry->find(left) : nullptr;
        p = l && !l->pace.empty() ? l : entry->find(shot->pose);
        if (p && p->pace.empty()) p = nullptr;
        if (p) shown = p == l ? left : shot->pose;
        if (!shot->hold && time - shot->start > poseSeconds(ui, shot->pose)) unitShot_[ui].pose.clear();
      } else if (walking && entry) {
        p = walkPose(*entry, unitWalk_[ui].dir, unitFacing_[ui], u.sprite, shown);
      } else if (chosen != kIdle && entry) {
        p = entry->find(chosen);
        if (p && p->pace.empty()) p = nullptr;
        if (p) shown = chosen;
      }
      const SpriteClipId clip = p ? sprites_.find(p->set, p->clip) : sprites_.find(u.set, u.clip);
      if (clip == kNoSpriteClip) {
        ++missing_;
        continue;
      }
      uint32_t frame = 0;
      if (p) {
        const size_t period = p->doneAt >= 0 ? static_cast<size_t>(p->doneAt) + 120 : pacePeriod(p->pace);
        const float clock = shot ? std::max(0.0f, time - shot->start) : walking ? std::max(0.0f, time - unitWalk_[ui].start) : time;
        // A one-shot pose plays through once and keeps its last frame.
        const size_t tick = shot ? std::min(static_cast<size_t>(clock * 240.0f), p->doneAt >= 0 ? static_cast<size_t>(p->doneAt) : p->pace.size() - 1)
                                 : static_cast<size_t>(clock * 240.0f) % period;
        frame = static_cast<uint32_t>(p->pace[std::min(tick, p->pace.size() - 1)]);
      } else if (!u.pace.empty()) {
        const size_t period = pacePeriod(u.pace);
        frame = static_cast<uint32_t>(u.pace[static_cast<size_t>(time * 240.0f) % period]);
      }
      const float boxW = p ? p->boxW : u.boxW, boxH = p ? p->boxH : u.boxH, foot = p ? p->foot : u.foot;
      glm::vec3 anchor = static_cast<int>(ui) == hero_ && path_.size() > 1 ? heroAt_ : unitWorld(ui);
      // Engine2's mirroring: the art's facing against the unit's, except cuts drawn for the left (and directional
      // walks while moving), which are never mirrored.
      const bool leftCut = shown == "walkLeft" || shown == "attackLeft" || shown == "attack2Left" || shown == "castLeft" ||
                           (walking && kDirectionalWalk.contains(u.sprite));
      const bool mirror = !leftCut && ((unitFacing_[ui] == -1) != (unitReversed_[ui] != 0));
      // Tática (Engine2 UnitActor.update with flatTop): flat over its own row's cards by half a row step, no shadow.
      if (flat_) anchor.y = boardTop_ + anchor.z * kFlatRowStep + kFlatRowStep * 0.5f;
      const uint32_t flags = flat_ ? kSpriteLit | kSpriteFlat : kSpriteLit | kSpriteFront | kSpriteCastsShadow;
      addCard(clip, anchor, right, boxW, boxH, {0.0f, -foot, 0.0f}, mirror, frame, flags, nullptr);
      unitCard_[ui] = {anchor - (flat_ ? glm::vec3(0.0f, 0.0f, -foot) : glm::vec3(0.0f, foot, 0.0f)), boxW, boxH, flat_, true};
    }
    const glm::vec3 eye = camera.position, fwd = camera.forward;
    std::sort(drawList_.begin(), drawList_.end(),
              [&](const SpriteInstance& a, const SpriteInstance& b) { return glm::dot(a.origin - eye, fwd) > glm::dot(b.origin - eye, fwd); });
  }

  // Engine2's CarriedLight: three.js point light, decay 1.25, cut off at hypot(radius, height) x 2.5, colour x
  // intensity x 3.2 x flicker.
  LocalLight carried(glm::vec3 at, const std::array<float, 3>& color, float intensity, float radius, float height, float flick) const {
    LocalLight l;
    l.position = at;
    if (lightingMode_ == 3) {
      // Real light: Engine2's intensity and reach read as a luminous flux (lantern ~900 lm, fire ~1200 lm, forge
      // ~1700 lm, candle ~200 lm), an isotropic source of flux / 4pi candela falling off as 1 / d^2 (lux), its
      // range where it drops under 0.03 lux.
      const float reach = radius / 1.3f;  // Engine2 LIGHT_RADIUS_MUL is already in the scene's radius
      const float lumens = intensity * reach * reach * kLumensPerUnit;
      const float candela = lumens / (4.0f * std::numbers::pi_v<float>);
      l.color = glm::vec3(color[0], color[1], color[2]) * candela * flick;
      l.decay = 2.0f;
      l.range = std::clamp(std::sqrt(candela / 0.03f), 2.0f, 48.0f);
      return l;
    }
    l.range = std::hypot(radius, height) * 2.5f;
    l.color = glm::vec3(color[0], color[1], color[2]) * intensity * 3.2f * flick;
    l.decay = 1.25f;
    return l;
  }

  void buildLights(float time) {
    for (const e2::SceneCard& d : scene_->cards) {
      if (!d.light) continue;
      const float yaw = d.lockedYaw.value_or(0.0f);
      const glm::vec3 right{std::cos(yaw), 0.0f, -std::sin(yaw)};
      const float height = std::max(d.light->minHeight, (d.art.flameV - d.art.baseV) * d.h);
      const float side = (d.art.flameU - d.art.baseU) * d.w * (d.mirror ? -1.0f : 1.0f);
      const glm::vec3 at = glm::vec3(d.x, groundOf(d.groundSmooth, d.groundHard), d.z) + right * side + glm::vec3(0, height, 0);
      lights_.push_back(carried(at, d.light->color, d.light->intensity, d.light->radius, height, flicker(time, d.light->seed, d.light->flicker)));
    }
    if (hero_ < 0) return;
    const e2::SceneUnit* hero = &scene_->units[static_cast<size_t>(hero_)];
    const glm::vec3 base = heroAt_;
    if (options_.movingLight) {
      // A torch (Engine2's NORMAL_FIRE: colour (1, 0.58, 0.26), intensity 8, radius 3.6, full flicker) carried
      // around the hero at hand height.
      const float a = time * 0.6f;
      const glm::vec3 at = base + glm::vec3(std::cos(a) * 2.5f, 1.6f, std::sin(a) * 2.5f);
      lights_.push_back(carried(at, {1.0f, 0.58f, 0.26f}, 8.0f, 3.6f, 1.6f, flicker(time, 1.7f, 1.0f)));
    }
    if (options_.spellLight) {
      // A violet arcane bolt from the hero to the nearest enemy: 1.2 s of flight, 0.5 s of impact flash.
      const e2::SceneUnit* target = nullptr;
      float best = 1e30f;
      for (const e2::SceneUnit& u : scene_->units) {
        if (u.side != "enemy") continue;
        const float d = glm::length(glm::vec2(u.x - base.x, u.z - base.z));
        if (d < best) best = d, target = &u;
      }
      if (target) {
        const glm::vec3 to{target->x, groundOf(target->groundSmooth, target->groundHard), target->z};
        const float cycle = std::fmod(time, 2.0f);
        const std::array<float, 3> violet{0.62f, 0.32f, 1.0f};
        if (cycle < 1.2f) {
          const float t = cycle / 1.2f;
          const glm::vec3 at = glm::mix(base, to, t) + glm::vec3(0, 1.8f + std::sin(t * 3.14159f) * 1.2f, 0);
          lights_.push_back(carried(at, violet, 10.0f, 3.5f, 1.8f, 1.0f));
        } else if (cycle < 1.7f) {
          const float k = 1.0f - (cycle - 1.2f) / 0.5f;
          lights_.push_back(carried(to + glm::vec3(0, 1.4f, 0), violet, 24.0f * k * k, 4.5f, 1.4f, 1.0f));
        }
      }
    }
  }

  // The moon tonight (ARCHITECTURE 7.5): it crosses the sky in Engine2's 600 s (moon.ts, stage.ts
  // MOON_CROSSING_SECONDS), starting at Engine2's 0.35 of the way. Engine2 drew the disc ahead of the normal view
  // but lit the board from the mirrored side (stage.ts moveMoon: light from ahead lit only the backs of units);
  // with a real sky the two cannot both hold, so three placements are kept, chosen in the panel until the user picks:
  //   0 Engine2's: disc ahead, low enough to show above the board; light from Engine2's side (26-43 deg up)
  //   1 high: disc and light together on Engine2's light (behind the normal view; the orbit camera never sees it)
  //   2 low ahead: disc and light together ahead and low (in view when the camera tilts low; units lit from behind)
  struct MoonPlacement {
    glm::vec3 light{0.0f, 1.0f, 0.0f}, disc{0.0f, 1.0f, 0.0f};  // towards each
  };
  MoonPlacement moonPlacement(float time) const {
    constexpr float kCrossingSeconds = 600.0f, kStart = 0.35f;
    // The low path (ours): the disc's lower edge 2 deg above the horizon at rise and set, 6 deg more at its highest,
    // so the whole disc always shows above the board, whatever its size.
    const float radius = glm::radians(kMoonSizes[moonSize_]);
    const float lowRise = radius + glm::radians(2.0f), lowHigh = radius + glm::radians(8.0f);
    const float progress = std::fmod(kStart + time / kCrossingSeconds, 1.0f);
    const e2::MoonPose pose = e2::moonPose(progress);
    const float height = std::sin(std::numbers::pi_v<float> * progress);
    float d[3];
    e2::lightDirection(pose.azimuth + std::numbers::pi_v<float>, pose.elevation, d);
    const glm::vec3 behind{d[0], d[1], d[2]};
    e2::lightDirection(pose.azimuth, lowRise + (lowHigh - lowRise) * height, d);
    const glm::vec3 lowAhead{d[0], d[1], d[2]};
    switch (moonPath_) {
      case 1: return {behind, behind};
      case 2: return {lowAhead, lowAhead};
      default: return {behind, lowAhead};
    }
  }

  // The map's light (ARCHITECTURE 7.5) in the chosen mode. Time of day picks sun or moon and where it stands;
  // the map's own sliders set the strengths (Engine2's 3D ignored them); the atmosphere colours the light.
  Renderer::Lighting mapLighting(float time) const {
    using e2::TimeOfDayLight;
    Renderer::Lighting l;
    if (lightingMode_ == 0) return l;  // the old prototype day
    const TimeOfDayLight& t = e2::timeOfDay(scene_ ? scene_->timeOfDay : std::string());
    const char* phase = moonPhase_ < 0 ? "full" : e2::kMoonPhases[moonPhase_];
    const e2::Moonlight moon = moonPhase_ < 0 ? e2::Moonlight{} : e2::moonlightFor(phase);
    MoonPlacement placed;
    if (t.moon) {
      placed = moonPlacement(time);
      l.sunDirection = placed.light;
    } else {
      float dir[3];
      e2::lightDirection(t.stageAzimuth, t.elevationDeg >= 0.0f ? glm::radians(t.elevationDeg) : t.stageElevation, dir);
      l.sunDirection = glm::normalize(glm::vec3(dir[0], dir[1], dir[2]));
    }
    if (lightingMode_ == 3) return realLighting(t, moon, phase, placed, time);
    if (lightingMode_ == 1) {  // Engine2's 3D stage, as it drew (at night: the moon's light from Engine2's side)
      if (t.moon) l.sunDirection = glm::normalize(moonPlacement(time).light);
      l.sunColor = Renderer::srgb(t.moon && moon.color ? moon.color : t.stageSun);
      l.sunIntensity = t.stageSunIntensity * (t.moon ? 1.5f * moon.strength : 1.0f);  // MOON_BRIGHTNESS x tonight's phase
      l.skyColor = Renderer::srgb(t.moon && moon.sky ? moon.sky : t.stageSky);
      l.groundColor = Renderer::srgb(t.stageGround);
      l.ambientIntensity = t.stageHemi;
      return l;
    }
    const float key = (scene_ && scene_->sunIntensity ? *scene_->sunIntensity : t.key) * (t.moon ? moon.strength : 1.0f);
    const float ambient = scene_ && scene_->ambientIntensity ? *scene_->ambientIntensity : t.ambient;
    const glm::vec3 keyColor = t.moon ? Renderer::srgb(moon.color ? moon.color : t.keyColor) : glm::vec3(1.0f);
    l.sunIntensity = key;
    l.ambientIntensity = ambient;
    l.skyColor = Renderer::srgb(t.moon && moon.sky ? moon.sky : t.skyColor);
    if (scene_ && scene_->environment == "indoor") {  // no sky indoors: the tuned colours on the hemisphere
      l.sunColor = Renderer::srgb(t.moon && moon.color ? moon.color : t.keyColor);
      l.groundColor = Renderer::srgb(t.stageGround);
      return l;
    }
    // The key light through the air: its colour (luminance 1; the map's strength sets how bright).
    glm::vec3 c = atmosphereTransmittance(AtmosphereFrame{}, l.sunDirection) * keyColor;
    const float lum = 0.2126f * c.r + 0.7152f * c.g + 0.0722f * c.b;
    l.sunColor = lum > 1e-6f ? c / lum : keyColor;
    l.atmosphere = true;
    l.skyIlluminance = keyColor * key;
    l.skyScale = t.moon ? 0.04f : 1.0f;  // a moonlit sky is dark
    if (t.moon) {
      // Night: stars, no sun disc, and the moon's disc with tonight's art. Its size is Engine2's disc as seen from
      // its normal view (moon.ts RADIUS 0.14 board half-sizes, ~3 half-sizes from the camera: ~2.7 deg radius; the
      // real moon's 0.26 would leave the phase unreadable); its brightness Engine2's GLOW (2.3 x the art).
      l.sunDisc = false;
      l.stars = starsOff_ ? 0.0f : 1.0f;
      l.starLook = static_cast<uint32_t>(starLook_);
      l.moonDirection = placed.disc;
      l.moonTexture = ground_.textureIndex(e2::skyMoonAsset(phase));
      l.moonRadius = moonOff_ ? 0.0f : glm::radians(kMoonSizes[moonSize_]);  // off: no disc (its light stays: "sol / lua")
      l.moonGlow = 2.3f;
    }
    return l;
  }

  // Light mode "real" (ARCHITECTURE 7.6 step A): sun, sky, moon and lamps on one physical scale (lux), the map's
  // sliders as multipliers on it. The sun outside the air gives 128,000 lux; the atmosphere's transmittance takes its
  // share and colours it; the physical sky (Hillaire tables) lit by the same sun gives the sky's own ambient. A full moon
  // gives 0.5 lux (twice the real 0.25, the usual art lift so moonlit ground still reads), times tonight's phase.
  // Dawn and dusk put the sun low (6 and 4 degrees) so they look like dawn and dusk. Indoors: a dim fill (3 lux) and
  // ambient (1.5 lux), so the torches carry the room. The camera meters what lands on the board (meterLux_).
  Renderer::Lighting realLighting(const e2::TimeOfDayLight& t, const e2::Moonlight& moon, const char* phase, const MoonPlacement& placed,
                                 float /*time*/) const {
    Renderer::Lighting l;
    const float keyMul = scene_ && scene_->sunIntensity ? *scene_->sunIntensity / t.key : 1.0f;
    const float ambientMul = scene_ && scene_->ambientIntensity && t.ambient > 0.0f ? *scene_->ambientIntensity / t.ambient : 1.0f;
    const bool indoor = scene_ && scene_->environment == "indoor";
    if (t.moon) {
      l.sunDirection = placed.light;
    } else {
      const std::string_view name = t.name;
      const float elevation = name == "dawn" ? glm::radians(6.0f) : name == "dusk" ? glm::radians(4.0f) : t.stageElevation;
      float dir[3];
      e2::lightDirection(t.stageAzimuth, elevation, dir);
      l.sunDirection = glm::normalize(glm::vec3(dir[0], dir[1], dir[2]));
    }
    const glm::vec3 keyColor = t.moon ? Renderer::srgb(moon.color ? moon.color : t.keyColor) : glm::vec3(1.0f);
    const float outside = (t.moon ? kFullMoonLux * moon.strength : kSunLux) * keyMul;  // the key light above the air
    const float sinElevation = std::max(l.sunDirection.y, 0.0f);
    if (indoor) {
      l.sunColor = Renderer::srgb(t.moon && moon.color ? moon.color : t.keyColor);
      l.sunIntensity = kIndoorFillLux * keyMul;
      l.skyColor = Renderer::srgb(t.moon && moon.sky ? moon.sky : t.skyColor);
      l.groundColor = Renderer::srgb(t.stageGround);
      l.ambientIntensity = kIndoorAmbientLux * ambientMul;
      meterLux_ = l.sunIntensity * std::max(sinElevation, 0.3f) + l.ambientIntensity;
      return l;
    }
    // Through the air: the key light's colour (luminance 1) and how much of it reaches the ground.
    const glm::vec3 c = atmosphereTransmittance(AtmosphereFrame{}, l.sunDirection) * keyColor;
    const float lum = 0.2126f * c.r + 0.7152f * c.g + 0.0722f * c.b;
    l.sunColor = lum > 1e-6f ? c / lum : keyColor;
    l.sunIntensity = outside * lum;
    l.atmosphere = true;
    l.skyIlluminance = keyColor * outside;
    l.skyScale = 1.0f;
    l.physicalSky = true;
    l.ambientMul = ambientMul;
    l.ambientIntensity = 0.0f;  // (not read: the sky gives the ambient)
    // The meter: the key light on level ground plus the sky (about a fifth of the key that gets through).
    meterLux_ = l.sunIntensity * std::max(sinElevation, 0.05f) + 0.2f * l.sunIntensity * ambientMul;
    if (t.moon) {
      l.sunDisc = false;
      l.stars = starsOff_ ? 0.0f : 1.0f;
      l.starLook = static_cast<uint32_t>(starLook_);
      l.moonDirection = placed.disc;
      l.moonTexture = ground_.textureIndex(e2::skyMoonAsset(phase));
      l.moonRadius = moonOff_ ? 0.0f : glm::radians(kMoonSizes[moonSize_]);
      l.moonGlow = 2.3f;
    }
    return l;
  }

  // How many stops darker than the meter the camera exposes (Unreal's exposure compensation curve by scene
  // brightness): none in full daylight, up to 0.75 as the sun sinks (dawn, dusk), then 2.5 by a moonlit night and more
  // on a darker one, so dusk reads as dusk and night as night, not as a dim day.
  static float nightDarkening(float ev100) {
    return 0.75f * std::clamp((14.0f - ev100) / 4.0f, 0.0f, 1.0f) + kNightStops * std::clamp((10.0f - ev100) / 12.5f, 0.0f, 1.3f);
  }
  // The painted flames' glow on screen: faint in daylight, full at night (from the camera's EV).
  float flameGlow() const {
    if (std::isnan(adaptedEv_)) return 1.0f;
    return glm::mix(0.15f, 1.0f, std::clamp((12.0f - adaptedEv_) / 12.0f, 0.0f, 1.0f));
  }

  // Where the party stands (the swirl turns round it): the middle of the living player units.
  glm::vec2 partyCenter() const {
    if (!scene_) return {boardCenter_.x, boardCenter_.z};
    glm::vec2 sum{0.0f};
    int n = 0;
    for (size_t i = 0; i < scene_->units.size(); ++i) {
      const e2::SceneUnit& u = scene_->units[i];
      if (u.side != "player") continue;
      sum += static_cast<int>(i) == hero_ ? glm::vec2(heroAt_.x, heroAt_.z) : unitPos_[i];
      ++n;
    }
    return n ? sum / static_cast<float>(n) : glm::vec2(boardCenter_.x, boardCenter_.z);
  }

  // The map's units find their own catalog entry by key. The panel's poses: those at least one of the map's units
  // has, in the catalog's order of first appearance with its label, the idle ("Parado") first.
  void setUpPoses(const std::filesystem::path& path) {
    std::optional<std::vector<e2::Hero>> catalog = e2::loadHeroes(path);
    if (!catalog) return;
    catalog_ = std::move(*catalog);
    std::unordered_map<std::string, const e2::Hero*> byKey;
    for (const e2::Hero& h : catalog_)
      if (!h.key.empty()) byKey.emplace(h.key, &h);
    unitCatalog_.assign(scene_->units.size(), nullptr);
    std::vector<std::string> unknown;
    for (size_t i = 0; i < scene_->units.size(); ++i) {
      const auto it = byKey.find(scene_->units[i].key);
      if (it != byKey.end()) unitCatalog_[i] = it->second;
      else unknown.push_back(scene_->units[i].name + " (" + scene_->units[i].key + ")");
    }
    for (const e2::Hero& h : catalog_) {
      if (std::find(unitCatalog_.begin(), unitCatalog_.end(), &h) == unitCatalog_.end()) continue;
      for (const e2::HeroPose& p : h.poses)
        if (std::none_of(poseChoices_.begin(), poseChoices_.end(), [&](const auto& q) { return q.first == p.pose; }))
          poseChoices_.emplace_back(p.pose, p.label.empty() ? p.pose : p.label);
    }
    const auto idle = std::find_if(poseChoices_.begin(), poseChoices_.end(), [](const auto& q) { return q.first == kIdle; });
    if (idle != poseChoices_.end()) std::rotate(poseChoices_.begin(), idle, idle + 1);
    else if (!poseChoices_.empty()) poseChoices_.insert(poseChoices_.begin(), {kIdle, "Parado"});
    logInfo("poses: {} of {} map units in the catalog, {} poses between them", scene_->units.size() - unknown.size(), scene_->units.size(),
            poseChoices_.size());
    for (const std::string& u : unknown) logWarn("poses: {} not in the catalog: idle only", u);
  }

  // Every scene unit's live state: where it stands, its facing, its art's own facing (from the scene's mirror rule).
  void initUnits() {
    const size_t n = scene_->units.size();
    unitPos_.resize(n), unitMoved_.assign(n, 0), unitFacing_.resize(n), unitReversed_.resize(n);
    unitWalk_.assign(n, {}), unitCard_.assign(n, {}), unitBattle_.assign(n, -1);
    unitShot_.assign(n, {}), unitDiedAt_.assign(n, -1.0f);
    for (size_t i = 0; i < n; ++i) {
      const e2::SceneUnit& u = scene_->units[i];
      unitPos_[i] = {u.x, u.z};
      unitFacing_[i] = u.facing < 0 ? -1 : 1;
      unitReversed_[i] = u.mirror != (u.facing == -1) ? 1 : 0;
    }
  }

  // The battle: each scene unit finds its battle unit by side and name, in spawn order.
  void setUpBattle(const std::filesystem::path& path) {
    battle_ = e2::Battle::load(path);
    if (!battle_) return;
    if (battle_->cols() != board_->layout().cols || battle_->rows() != board_->layout().rows) {
      logError("battle {}: {}x{} does not fit the map's {}x{} board", path.string(), battle_->cols(), battle_->rows(), board_->layout().cols,
               board_->layout().rows);
      battle_.reset();
      return;
    }
    std::vector<uint8_t> taken(battle_->units().size(), 0);
    size_t matched = 0;
    for (size_t i = 0; i < scene_->units.size(); ++i)
      for (size_t b = 0; b < battle_->units().size(); ++b)
        if (!taken[b] && battle_->units()[b].side == scene_->units[i].side && battle_->units()[b].name == scene_->units[i].name) {
          unitBattle_[i] = static_cast<int>(b), taken[b] = 1, ++matched;
          break;
        }
    logInfo("battle {}: {} of {} units in play", battle_->map(), matched, scene_->units.size());
    rng_ = e2::Mulberry32(static_cast<uint32_t>(std::chrono::steady_clock::now().time_since_epoch().count()));
    battle_->rollInitiative(rng_);
    for (const e2::BattleUnit& u : battle_->units()) startLevel_.emplace_back(u.level, u.xp);
  }

  int sceneUnitOf(int battleUnit) const {
    for (size_t i = 0; i < unitBattle_.size(); ++i)
      if (unitBattle_[i] == battleUnit && battleUnit >= 0) return static_cast<int>(i);
    return -1;
  }

  // Where a unit's feet are now: its spawn's measured ground until it moves, then the ground under it.
  glm::vec3 unitWorld(size_t i) const {
    const e2::SceneUnit& u = scene_->units[i];
    const glm::vec2 p = unitPos_[i];
    const float y = unitMoved_[i] ? static_cast<float>(e2::groundHeightAt(*board_, look_, p.x, p.y)) : groundOf(u.groundSmooth, u.groundHard);
    return {p.x, y, p.y};
  }

  // Where a battle unit's feet stand with its anchor on a hex: the middle of its body's front row (Ember's footprint
  // centroid, as the map importer places the spawns).
  glm::vec2 anchorWorld(const e2::BattleUnit& u, e2::BattleHex at) const {
    glm::vec2 sum{0.0f};
    int n = 0;
    for (const e2::BattleHex& p : e2::Battle::footprint(u, at.x, at.y)) {
      if (p.y != at.y || !battle_->inBounds(p.x, p.y)) continue;
      const e2::Cell& c = board_->cell(p.x, p.y);
      sum += glm::vec2(static_cast<float>(c.x), static_cast<float>(c.z));
      ++n;
    }
    if (!n) {
      const e2::Cell& c = board_->cell(at.x, at.y);
      return {static_cast<float>(c.x), static_cast<float>(c.z)};
    }
    return sum / static_cast<float>(n);
  }

  // Engine2 UnitActor.update for walking units: one hex every 0.22 s; facing and walk direction relative to the camera.
  void updateUnits(float dt) {
    if (!scene_) return;
    const float yaw = flat_ ? 0.0f : cam_.yaw;
    for (size_t i = 0; i < unitWalk_.size(); ++i) {
      Walk& w = unitWalk_[i];
      if (w.points.empty()) continue;
      glm::vec2& p = unitPos_[i];
      const glm::vec2 d = w.points.front() - p;
      const float dist = glm::length(d);
      const float screenX = d.x * std::cos(yaw) - d.y * std::sin(yaw);
      if (std::abs(screenX) > 0.05f) unitFacing_[i] = screenX > 0.0f ? 1 : -1;
      const float away = -(d.x * std::sin(yaw) + d.y * std::cos(yaw));
      if (dist > 1e-4f) w.dir = std::abs(away) > dist * 0.5f ? (away > 0.0f ? kWalkBack : kWalkFront) : kWalkSide;
      const float move = std::min(dist, std::sqrt(3.0f) / kStepSeconds * dt);
      if (dist > 1e-4f) p += d / dist * move;
      unitMoved_[i] = 1;
      if (dist - move < 1e-3f) w.points.erase(w.points.begin());
    }
    if (hero_ >= 0 && path_.size() <= 1) heroAt_ = unitWorld(static_cast<size_t>(hero_));  // the test lights follow it
  }

  // Engine2 UnitActor.walkShown: a still for the direction, else the up / down cycle, else the side walk (its own left
  // cut when facing left; the Lancer's cuts are swapped).
  static const e2::HeroPose* walkPose(const e2::Hero& h, int dir, int facing, const std::string& sprite, std::string& shown) {
    auto has = [&](const char* pose) -> const e2::HeroPose* {
      const e2::HeroPose* p = h.find(pose);
      if (!p || p->pace.empty()) return nullptr;
      shown = pose;
      return p;
    };
    static const char* kStill[3] = {"walkBack", "walkFront", "walkSide"};
    if (const e2::HeroPose* p = has(kStill[dir])) return p;
    if (dir == kWalkBack)
      if (const e2::HeroPose* p = has("walkUp")) return p;
    if (dir == kWalkFront)
      if (const e2::HeroPose* p = has("walkDown")) return p;
    const bool wantLeft = sprite == "lancer" ? facing == 1 : facing == -1;
    if (wantLeft)
      if (const e2::HeroPose* p = has("walkLeft")) return p;
    if (const e2::HeroPose* p = has("walk")) return p;
    shown = kIdle;
    return nullptr;
  }

  // The hex under the cursor: a drawn unit's card first (nearest wins), else the ground the view ray meets.
  std::optional<e2::BattleHex> pick(SDL_WindowID id, float mx, float my) const {
    int w = 0, h = 0;
    SDL_Window* window = SDL_GetWindowFromID(id);
    if (!window || !SDL_GetWindowSize(window, &w, &h) || w <= 0 || h <= 0) return std::nullopt;
    const ImVec2 cursor{mx, my};
    float best = 1e30f;
    std::optional<e2::BattleHex> hit;
    const glm::vec3 right = glm::length(glm::vec3(lastCamera_.right.x, 0.0f, lastCamera_.right.z)) > 0.0f && !flat_
                                ? glm::normalize(glm::vec3(lastCamera_.right.x, 0.0f, lastCamera_.right.z))
                                : glm::vec3(1.0f, 0.0f, 0.0f);
    for (size_t i = 0; i < unitCard_.size(); ++i) {
      const CardRect& c = unitCard_[i];
      if (!c.drawn || unitBattle_[i] < 0) continue;
      const glm::vec3 up = c.flat ? glm::vec3(0.0f, 0.0f, -1.0f) : glm::vec3(0.0f, 1.0f, 0.0f);
      const glm::vec3 corners[4] = {c.origin - right * (c.width * 0.5f), c.origin + right * (c.width * 0.5f),
                                    c.origin + right * (c.width * 0.5f) + up * c.height, c.origin - right * (c.width * 0.5f) + up * c.height};
      ImVec2 q[4];
      bool ok = true;
      for (int k = 0; k < 4 && ok; ++k)
        if (const std::optional<ImVec2> s = screenOf(corners[k], static_cast<float>(w), static_cast<float>(h))) q[k] = *s;
        else ok = false;
      if (!ok || !insideQuad(q, cursor)) continue;
      const float d = glm::length(c.origin - lastCamera_.position);
      if (d < best) best = d, hit = e2::BattleHex{battle_->units()[static_cast<size_t>(unitBattle_[i])].x, battle_->units()[static_cast<size_t>(unitBattle_[i])].y};
    }
    if (hit) return hit;
    // The ground: march the view ray down from just above the highest ground, then refine where it goes under.
    const float tanHalf = std::tan(glm::radians((flat_ ? kFlatFov : kFov) * 0.5f));
    const float nx = 2.0f * mx / static_cast<float>(w) - 1.0f, ny = 1.0f - 2.0f * my / static_cast<float>(h);
    const glm::vec3 camUp = glm::cross(lastCamera_.right, lastCamera_.forward);
    const glm::vec3 dir = glm::normalize(lastCamera_.forward + lastCamera_.right * (nx * tanHalf * aspect_) + camUp * (ny * tanHalf));
    if (dir.y > -1e-4f) return std::nullopt;
    const glm::vec3 o = lastCamera_.position;
    const e2::Board::Bounds b = board_->bounds();
    auto under = [&](float t) {
      const glm::vec3 p = o + dir * t;
      if (p.x < b.x0 || p.x > b.x1 || p.z < b.z0 || p.z > b.z1) return false;
      return p.y <= static_cast<float>(board_->cellAt(p.x, p.z).groundY);
    };
    float t = std::max(0.0f, (o.y - (boardTop_ + 2.0f)) / -dir.y);
    const float end = (o.y - (groundLow_ - 2.0f)) / -dir.y;
    for (; t < end; t += 0.15f)
      if (under(t)) {
        float lo = t - 0.15f, hi = t;
        for (int k = 0; k < 12; ++k) (under((lo + hi) * 0.5f) ? hi : lo) = (lo + hi) * 0.5f;
        const glm::vec3 p = o + dir * hi;
        const auto [c, r] = e2::hexAt(board_->layout(), p.x, p.z);
        if (!battle_->inBounds(c, r)) return std::nullopt;
        return e2::BattleHex{c, r};
      }
    return std::nullopt;
  }

  static bool insideQuad(const ImVec2 (&q)[4], ImVec2 p) {
    int sign = 0;
    for (int k = 0; k < 4; ++k) {
      const ImVec2 a = q[k], b = q[(k + 1) % 4];
      const float cross = (b.x - a.x) * (p.y - a.y) - (b.y - a.y) * (p.x - a.x);
      const int s = cross > 0.0f ? 1 : cross < 0.0f ? -1 : 0;
      if (s && sign && s != sign) return false;
      if (s) sign = s;
    }
    return true;
  }

  // A world point on screen (window coordinates), or none behind the eye.
  std::optional<ImVec2> screenOf(glm::vec3 p, float w, float h) const {
    const glm::vec4 clip = lastCamera_.viewProjection() * glm::vec4(p, 1.0f);
    if (clip.w <= 1e-4f) return std::nullopt;
    return ImVec2{(clip.x / clip.w * 0.5f + 0.5f) * w, (clip.y / clip.w * 0.5f + 0.5f) * h};
  }

  void updateHover() {
    hover_.reset();
    if (!battle_ || mouse_.x < 0.0f) return;
    if (ImGui::GetCurrentContext() && ImGui::GetIO().WantCaptureMouse) return;
    hover_ = pick(mouseWindow_, mouse_.x, mouse_.y);
  }

  // The left button: a reachable hex walks the picked player unit there; a unit picks it; anything else lets go.
  void click(SDL_WindowID id, float mx, float my) {
    if (!battle_ || !scene_) return;
    const std::optional<e2::BattleHex> hex = pick(id, mx, my);
    if (busy()) return;  // a walk, a strike or an enemy's turn is playing
    if (!spellMode_.empty()) {
      if (hex && myTurn()) aimSpell(*hex);
      return;
    }
    if (!hex) return select(-1);
    // A marked foe: the unit whose turn it is strikes it, walking first to the hex it strikes from.
    if (const int foe = sceneUnitOf(battle_->unitAt(hex->x, hex->y)); foe >= 0 && myTurn())
      for (const auto& [t, from] : targets_)
        if (t == foe) {
          const e2::BattleUnit& u = battle_->units()[static_cast<size_t>(unitBattle_[static_cast<size_t>(selected_)])];
          if (from.x == u.x && from.y == u.y) return attack(static_cast<size_t>(selected_), static_cast<size_t>(foe));
          pendingStrike_ = foe;
          return walkTo(static_cast<size_t>(selected_), from);
        }
    if (myTurn()) {
      const e2::BattleUnit& u = battle_->units()[static_cast<size_t>(unitBattle_[static_cast<size_t>(selected_)])];
      const bool own = hex->x == u.x && hex->y == u.y;
      if (!own && std::any_of(reach_.begin(), reach_.end(), [&](const e2::ReachCell& c) { return c.x == hex->x && c.y == hex->y; }))
        return walkTo(static_cast<size_t>(selected_), *hex);
    }
    const int ui = sceneUnitOf(battle_->unitAt(hex->x, hex->y));
    if (ui >= 0 && ui != selected_ && unitCard_[static_cast<size_t>(ui)].drawn && battle_->units()[static_cast<size_t>(unitBattle_[static_cast<size_t>(ui)])].alive)
      return select(ui);
    select(-1);
  }

  void select(int ui) {
    selected_ = ui;
    spellMode_.clear(), spellCells_.clear(), spellAim_.clear();
    attackMode_ = false;
    refreshReach();
  }

  // The battle bar picked a spell: every hex it may be aimed at is marked.
  void startSpell(const std::string& kind) {
    if (selected_ < 0) return;
    if (spellMode_ == kind) {  // the same socket again lets it go
      spellMode_.clear(), spellCells_.clear(), spellAim_.clear();
      return;
    }
    spellMode_ = kind;
    spellCells_.clear();
    spellAim_.clear();
    const size_t b = static_cast<size_t>(unitBattle_[static_cast<size_t>(selected_)]);
    const e2::BattleUnit& u = battle_->units()[b];
    for (int y = 0; y < battle_->rows(); ++y)
      for (int x = 0; x < battle_->cols(); ++x)
        if (e2::Battle::distance({u.x, u.y}, {x, y}) <= 9 && e2::spellAimValid(*battle_, b, kind, {x, y})) spellAim_.push_back({x, y});
  }

  // A hex clicked while a spell is aimed: one more target (Míssil Mágico takes one per missile), then the cast.
  void aimSpell(e2::BattleHex hex) {
    const size_t b = static_cast<size_t>(unitBattle_[static_cast<size_t>(selected_)]);
    std::string why;
    if (!e2::spellAimValid(*battle_, b, spellMode_, hex, &why)) {
      tip_ = why, tipAt_ = now_;
      return;
    }
    spellCells_.push_back(hex);
    if (static_cast<int>(spellCells_.size()) < e2::spellTargetCount(*battle_, b, spellMode_)) return;
    std::optional<std::vector<e2::BattleStep>> steps = e2::castSpell(*battle_, b, spellMode_, spellCells_, &why);
    const std::string kind = spellMode_;
    spellMode_.clear(), spellCells_.clear(), spellAim_.clear();
    if (!steps) {
      tip_ = why, tipAt_ = now_;
      return;
    }
    play(std::move(*steps), static_cast<size_t>(selected_));
  }

  void refreshReach() {
    reach_.clear(), reachPaths_.clear(), targets_.clear();
    if (!battle_ || selected_ < 0 || unitBattle_[static_cast<size_t>(selected_)] < 0) return;
    const size_t b = static_cast<size_t>(unitBattle_[static_cast<size_t>(selected_)]);
    const e2::BattleUnit& u = battle_->units()[b];
    if (!u.alive) return;
    // Its reach: the movement left for the unit whose turn it is, a whole pool for one being inspected.
    e2::Battle look = *battle_;
    if (!myTurn()) look.units()[b].moveUsed = 0;
    reach_ = look.reachable(b, true);
    reachPaths_ = look.reachable(b, false);  // routes may pass a friend's hex (Engine2: walk reach unpruned)
    // The foes it can strike from some hex of its reach, each from the first such hex (Engine2 attackableEnemies),
    // and that the party can see; only for the party's unit whose turn it is and that has not struck yet.
    if (!myTurn() || u.acted) return;
    for (const e2::ReachCell& c : reach_)
      for (size_t i = 0; i < scene_->units.size(); ++i) {
        const int fb = unitBattle_[i];
        if (fb < 0 || !unitCard_[i].drawn) continue;
        const e2::BattleUnit& f = battle_->units()[static_cast<size_t>(fb)];
        if (!f.alive || f.side == u.side) continue;
        if (std::any_of(targets_.begin(), targets_.end(), [&](const auto& t) { return t.first == static_cast<int>(i); })) continue;
        if (e2::canHitFrom(*battle_, b, {c.x, c.y}, static_cast<size_t>(fb))) targets_.push_back({static_cast<int>(i), e2::BattleHex{c.x, c.y}});
      }
  }

  // The party's unit whose turn it is, picked.
  bool myTurn() const {
    if (!battle_ || selected_ < 0 || unitBattle_[static_cast<size_t>(selected_)] < 0) return false;
    const int active = battle_->activeUnit();
    return active == unitBattle_[static_cast<size_t>(selected_)] && battle_->units()[static_cast<size_t>(active)].side == "player" && result_.empty();
  }

  // Something is playing that input must not cut into: a walk, a strike, an enemy's turn, the battle's end.
  bool busy() const {
    if (exchange_ || enemyTurn_ || !result_.empty()) return true;
    return std::any_of(unitWalk_.begin(), unitWalk_.end(), [](const Walk& w) { return !w.points.empty(); });
  }

  // Engine2's turn loop (tick / beginUnitTurn / startNewRound): whoever is next in the round acts; when everyone has
  // gone a new round starts. The party's unit is picked and waits for the player; an enemy plays its own turn.
  void updateTurns() {
    if (!battle_ || !scene_ || !result_.empty()) return;
    const bool walking = std::any_of(unitWalk_.begin(), unitWalk_.end(), [](const Walk& w) { return !w.points.empty(); });
    if (exchange_ || walking) return;
    // A strike the player asked for once the walk to its hex is over.
    if (pendingStrike_ >= 0) {
      const int foe = pendingStrike_;
      pendingStrike_ = -1;
      if (selected_ >= 0 && battle_->units()[static_cast<size_t>(unitBattle_[static_cast<size_t>(foe)])].alive)
        return attack(static_cast<size_t>(selected_), static_cast<size_t>(foe));
    }
    // An enemy's turn: its walk is over; now its strike, then its turn ends.
    if (enemyTurn_) {
      EnemyTurn t = *enemyTurn_;
      enemyTurn_.reset();
      if (!t.steps.empty() && !t.struck && battle_->units()[static_cast<size_t>(unitBattle_[t.unit])].alive) {
        t.struck = true;
        std::vector<e2::BattleStep> steps = t.steps;
        enemyTurn_ = t;
        play(std::move(steps), t.unit);
        return;
      }
      if (t.target >= 0 && !t.struck && battle_->units()[static_cast<size_t>(unitBattle_[static_cast<size_t>(t.target)])].alive &&
          battle_->units()[static_cast<size_t>(unitBattle_[t.unit])].alive) {
        t.struck = true;
        enemyTurn_ = t;
        attack(t.unit, static_cast<size_t>(t.target));
        return;
      }
      battle_->units()[static_cast<size_t>(unitBattle_[t.unit])].moved = true;
    }
    if (result_ = battle_->result(); !result_.empty()) {
      resultAt_ = now_;
      return select(-1);
    }
    int active = battle_->activeUnit();
    if (active < 0) {
      battle_->startNewRound();
      turnUnit_ = -1;
      active = battle_->activeUnit();
      if (active < 0) return;
    }
    if (active == turnUnit_) return;
    turnUnit_ = active;
    battle_->beginTurn(static_cast<size_t>(active));
    {
      // The start of its own turn: a lightning echo, the ground's flames.
      std::vector<e2::BattleEffect> fx;
      e2::startOfTurnEffects(*battle_, static_cast<size_t>(active), rng_, fx);
      showEffects(fx);
      if (!battle_->units()[static_cast<size_t>(active)].alive) {
        battle_->units()[static_cast<size_t>(active)].moved = true;
        return;
      }
    }
    const int ui = sceneUnitOf(active);
    const e2::BattleUnit& u = battle_->units()[static_cast<size_t>(active)];
    if (ui < 0) {  // a unit with no card in this scene sits its turns out
      battle_->units()[static_cast<size_t>(active)].moved = true;
      return;
    }
    if (u.side == "player") return select(ui);
    // Under fog an enemy that has not seen the party yet holds its ground (Engine2 wakeIfSeesParty; here: once the
    // party sees its hex).
    if (fog_ && !noFog_ && !awake_.contains(active)) {
      const size_t i = static_cast<size_t>(e2::indexOf(board_->layout(), u.x, u.y));
      if (i >= fogStates_.size() || fogStates_[i] != e2::kFogVisible) {
        battle_->units()[static_cast<size_t>(active)].moved = true;
        return;
      }
      awake_.insert(active);
    }
    select(-1);
    if (std::optional<e2::AiCast> cast = e2::planEnemyCast(*battle_, static_cast<size_t>(active))) {
      EnemyTurn t;
      t.unit = static_cast<size_t>(ui);
      t.steps = cast->steps;
      enemyTurn_ = t;
      if (cast->path.size() > 1) walkAlong(static_cast<size_t>(ui), cast->path);
      return;
    }
    const e2::AiPlan plan = battle_->planEnemy(static_cast<size_t>(active));
    EnemyTurn t;
    t.unit = static_cast<size_t>(ui);
    t.target = plan.target >= 0 ? sceneUnitOf(plan.target) : -1;
    enemyTurn_ = t;
    if (plan.path.size() > 1) walkAlong(static_cast<size_t>(ui), plan.path);
  }

  // Walk a unit along a route (start first): the battle's state moves at once, the walk then plays it.
  void walkAlong(size_t ui, const std::vector<e2::BattleHex>& route) {
    const size_t b = static_cast<size_t>(unitBattle_[ui]);
    e2::BattleUnit& u = battle_->units()[b];
    // Its cost along the route (the step costs of the reach it was planned on).
    for (const e2::ReachCell& c : battle_->reachable(b, false))
      if (c.x == route.back().x && c.y == route.back().y) u.moveUsed += c.cost;
    u.x = route.back().x, u.y = route.back().y;
    Walk w;
    for (size_t k = 1; k < route.size(); ++k) w.points.push_back(anchorWorld(u, route[k]));
    const auto [hx, hy] = e2::heading(route[route.size() - 2], route.back());
    u.faceDx = hx, u.faceDy = hy;
    w.start = now_;
    unitWalk_[ui] = std::move(w);
  }

  // --- The exchange: an attack and, when it can, the counter (Engine2 BattleEngine's combat queue for a plain attack). ---

  // A pose played once over the unit's own (attack, hit, death), from `start` on the scene clock.
  struct OneShot {
    std::string pose;
    float start = 0.0f;
    bool hold = false;  // keeps its last frame (death)
  };
  // An action in play (Engine2's queue): its steps one after another (a strike and its counter, a spell's hits, a
  // heal), each a pose with the rules applied at its impact.
  struct Exchange {
    std::vector<e2::BattleStep> steps;
    size_t step = 0;
    bool counter = false;  // the defender's strike back is playing
    float impact = 0.0f, end = 0.0f;
    bool landed = false;   // this step's roll is in
    size_t owner = 0;      // the scene unit whose action this is
  };

  // How long a unit's pose plays (its one-shot end, else its loop), seconds; 0.8 s when it has none.
  float poseSeconds(size_t ui, const std::string& pose) const {
    const e2::Hero* entry = ui < unitCatalog_.size() ? unitCatalog_[ui] : nullptr;
    const e2::HeroPose* p = entry ? entry->find(pose) : nullptr;
    if (!p || p->pace.empty()) return 0.8f;
    return static_cast<float>(p->doneAt >= 0 ? p->doneAt : static_cast<int>(p->pace.size())) / 240.0f;
  }

  // Turn a unit to face a hex: on screen (its card) and on the board (Engine2 faceSpriteToward).
  void faceToward(size_t ui, size_t otherUi) {
    const size_t b = static_cast<size_t>(unitBattle_[ui]), o = static_cast<size_t>(unitBattle_[otherUi]);
    e2::BattleUnit& u = battle_->units()[b];
    const e2::BattleUnit& v = battle_->units()[o];
    if (u.x == v.x && u.y == v.y) return;
    const auto [hx, hy] = e2::heading({u.x, u.y}, {v.x, v.y});
    u.faceDx = hx, u.faceDy = hy;
    const glm::vec2 d = unitPos_[otherUi] - unitPos_[ui];
    const float yaw = flat_ ? 0.0f : cam_.yaw;
    const float screenX = d.x * std::cos(yaw) - d.y * std::sin(yaw);
    if (std::abs(screenX) > 0.05f) unitFacing_[ui] = screenX > 0.0f ? 1 : -1;
  }

  void attack(size_t attackerUi, size_t defenderUi) {
    e2::BattleStep strike;
    strike.type = e2::BattleStep::Type::Combat;
    strike.att = static_cast<size_t>(unitBattle_[attackerUi]);
    strike.def = static_cast<size_t>(unitBattle_[defenderUi]);
    play({strike}, attackerUi);
  }

  void play(std::vector<e2::BattleStep> steps, size_t ownerUi) {
    if (steps.empty()) return;
    Exchange x;
    x.steps = std::move(steps);
    x.owner = ownerUi;
    exchange_ = std::move(x);
    reach_.clear(), reachPaths_.clear(), targets_.clear();
    spellMode_.clear(), spellCells_.clear(), spellAim_.clear();
    startStep(*exchange_);
  }

  // The step's pose starts: the striker turns to its target (and a struck foe to the striker).
  void startStep(Exchange& x) {
    const e2::BattleStep& s = x.steps[x.step];
    const int striker = sceneUnitOf(static_cast<int>(x.counter ? s.def : s.att));
    int target = -1;
    if (s.type != e2::BattleStep::Type::Spell) target = sceneUnitOf(static_cast<int>(x.counter ? s.att : s.def));
    else if (!s.ids.empty()) target = sceneUnitOf(static_cast<int>(s.ids[0]));
    if (striker >= 0 && target >= 0 && striker != target) {
      faceToward(static_cast<size_t>(striker), static_cast<size_t>(target));
      if (s.type != e2::BattleStep::Type::Heal) faceToward(static_cast<size_t>(target), static_cast<size_t>(striker));
    }
    std::string pose = s.type == e2::BattleStep::Type::Combat ? "attack" : e2::spellPose(s.kind);
    if (striker >= 0 && !hasPose(static_cast<size_t>(striker), pose)) pose = hasPose(static_cast<size_t>(striker), "cast") ? "cast" : "attack";
    const float length = striker >= 0 ? poseSeconds(static_cast<size_t>(striker), pose) : 0.6f;
    x.impact = now_ + length * kImpactShare;
    x.end = now_ + length + kExchangeGap;
    x.landed = false;
    if (striker >= 0) unitShot_[static_cast<size_t>(striker)] = {pose, now_, false};
  }

  bool hasPose(size_t ui, const std::string& pose) const {
    const e2::Hero* entry = ui < unitCatalog_.size() ? unitCatalog_[ui] : nullptr;
    return entry && entry->find(pose);
  }

  // What a step did, on screen: numbers, misses, heals, the struck unit's hit or death pose.
  void showEffects(const std::vector<e2::BattleEffect>& fx) {
    for (const e2::BattleEffect& e : fx) {
      const int ui = sceneUnitOf(static_cast<int>(e.unit));
      if (ui < 0) continue;
      const size_t u = static_cast<size_t>(ui);
      if (e.miss) {
        popups_.push_back({u, "Errou", IM_COL32(200, 200, 200, 255), now_, false});
        continue;
      }
      if (e.heal) {
        popups_.push_back({u, "+" + std::to_string(e.amount), IM_COL32(170, 226, 150, 255), now_, false});
        continue;
      }
      popups_.push_back({u, std::to_string(e.amount), e.crit ? IM_COL32(255, 214, 102, 255) : IM_COL32(255, 255, 255, 255), now_, e.crit});
      if (e.died) {
        unitShot_[u] = {"death", now_, true};
        unitDiedAt_[u] = now_;
      } else {
        unitShot_[u] = {"hit", now_, false};
      }
    }
  }

  // Every frame: each step's impact lands, a strike's counter follows, then the next step; at the end the action is
  // over (Engine2 finishAction).
  void updateExchange() {
    if (!exchange_) return;
    Exchange& x = *exchange_;
    const e2::BattleStep& s = x.steps[x.step];
    if (!x.landed && now_ >= x.impact) {
      x.landed = true;
      std::vector<e2::BattleEffect> fx;
      if (s.type == e2::BattleStep::Type::Combat) {
        if (battle_->units()[s.att].alive && battle_->units()[s.def].alive) e2::resolveStrike(*battle_, s, x.counter, rng_, fx);
      } else {
        e2::resolveStep(*battle_, s, rng_, fx);
      }
      showEffects(fx);
    }
    if (now_ < x.end) return;
    if (s.type == e2::BattleStep::Type::Combat && !x.counter && e2::strikesBack(*battle_, s)) {
      x.counter = true;
      startStep(x);
      return;
    }
    if (x.step + 1 < x.steps.size()) {
      ++x.step;
      x.counter = false;
      startStep(x);
      return;
    }
    // Engine2 finishAction: it has acted; its turn ends unless it is a hero with movement left (it stays picked).
    e2::BattleUnit& au = battle_->units()[static_cast<size_t>(unitBattle_[x.owner])];
    au.acted = true;
    exchange_.reset();
    if (!au.alive || au.side != "player" || au.mov - au.moveUsed <= 0) {
      if (au.side == "player") au.moved = true;
      return select(-1);
    }
    refreshReach();
  }

  // The move is decided at once (the battle's state), the walk then plays it hex by hex.
  void walkTo(size_t ui, e2::BattleHex to) {
    const size_t b = static_cast<size_t>(unitBattle_[ui]);
    const std::vector<e2::BattleHex> route = e2::Battle::path(reachPaths_, to.x, to.y);
    if (route.size() < 2) return;
    if (!std::any_of(reach_.begin(), reach_.end(), [&](const e2::ReachCell& c) { return c.x == to.x && c.y == to.y; })) return;
    (void)b;
    walkAlong(ui, route);  // the battle's heading becomes the last step's (Engine2 stepMove), for attacks from behind
    refreshReach();        // what is left of its movement, shown once it arrives
  }

  // The battle grid (Battle3D gridMarks' colours): the movement wash and edges, the route, the picked unit's marker
  // and the cursor; fills and hex borders, radii in hex radii. Painted on the ground by the terrain pass
  // (Renderer::setGroundMarks), so units, trees and houses in front cover them; never on ground not yet seen.
  void collectMarks(std::vector<Renderer::GroundMark>& marks) const {
    if (!battle_ || !scene_ || !board_) return;
    const e2::HexLayout& layout = board_->layout();
    auto add = [&](e2::BattleHex at, ImU32 color, float rOut, float rIn = 0.0f) {
      if (!e2::inBoard(layout, at.x, at.y)) return;
      marks.push_back({static_cast<uint32_t>(e2::indexOf(layout, at.x, at.y)), color, rOut, rIn});
    };
    auto border = [](float r) { return r * kBorderInner; };
    const bool walking = selected_ >= 0 && !unitWalk_[static_cast<size_t>(selected_)].points.empty();
    if (selected_ >= 0 && !walking && !spellMode_.empty()) {
      // Engine2's spell aim: every hex it may be aimed at washed, the hovered aim's area bright, chosen targets ringed.
      const size_t b = static_cast<size_t>(unitBattle_[static_cast<size_t>(selected_)]);
      for (const e2::BattleHex& c : spellAim_) add(c, IM_COL32(176, 124, 232, 34), 1.0f), add(c, IM_COL32(176, 124, 232, 110), 0.94f, border(0.94f));
      if (hover_) {
        if (e2::spellAimValid(*battle_, b, spellMode_, *hover_))
          for (const e2::BattleHex& c : e2::spellArea(*battle_, b, spellMode_, *hover_)) add(c, IM_COL32(214, 168, 255, 96), 1.0f);
        add(*hover_, IM_COL32(8, 12, 16, 242), 0.985f, border(0.985f));
        add(*hover_, e2::spellAimValid(*battle_, b, spellMode_, *hover_) ? IM_COL32(236, 214, 255, 255) : IM_COL32(231, 133, 115, 242), 0.94f, border(0.94f));
      }
      for (const e2::BattleHex& c : spellCells_) add(c, IM_COL32(255, 214, 102, 255), 0.7f, border(0.7f));
      const e2::BattleUnit& u = battle_->units()[b];
      add({u.x, u.y}, IM_COL32(220, 226, 235, 71), 0.94f, border(0.94f));
    } else if (selected_ >= 0 && !walking) {
      const bool player = scene_->units[static_cast<size_t>(selected_)].side == "player";
      const e2::BattleUnit& u = battle_->units()[static_cast<size_t>(unitBattle_[static_cast<size_t>(selected_)])];
      // The reach (tacticalGridStyle): the movement wash with its edges for the party, the enemy's wash for a foe.
      for (const e2::ReachCell& c : reach_) {
        if (c.x == u.x && c.y == u.y) continue;
        if (attackMode_) break;  // "Atacar": only the foes it can strike
        add({c.x, c.y}, player ? IM_COL32(112, 174, 220, 20) : IM_COL32(231, 133, 115, 31), 1.0f);
        if (player) add({c.x, c.y}, IM_COL32(112, 174, 220, 92), 0.94f, border(0.94f));
      }
      // The route to the hovered hex.
      if (player && hover_ && !(hover_->x == u.x && hover_->y == u.y) &&
          std::any_of(reach_.begin(), reach_.end(), [&](const e2::ReachCell& c) { return c.x == hover_->x && c.y == hover_->y; })) {
        const std::vector<e2::BattleHex> route = e2::Battle::path(reachPaths_, hover_->x, hover_->y);
        for (size_t i = 1; i < route.size(); ++i) {
          const bool last = i + 1 == route.size();
          if (last) {
            add(route[i], IM_COL32(8, 12, 16, 242), 0.985f, border(0.985f));
            add(route[i], IM_COL32(220, 226, 235, 31), 1.01f, border(1.01f));
            add(route[i], IM_COL32(220, 226, 235, 255), 0.94f, border(0.94f));
          } else {
            add(route[i], IM_COL32(8, 12, 16, 242), 0.16f);
            add(route[i], IM_COL32(220, 226, 235, 255), 0.12f);
          }
        }
      }
      // The picked unit's marker (Engine2's active-turn highlight).
      const e2::BattleHex at{u.x, u.y};
      if (player) {
        add(at, IM_COL32(220, 226, 235, 3), 1.035f, border(1.035f));
        add(at, IM_COL32(220, 226, 235, 6), 1.01f, border(1.01f));
        add(at, IM_COL32(220, 226, 235, 10), 0.985f, border(0.985f));
      }
      add(at, IM_COL32(12, 20, 25, 217), 0.98f, border(0.98f));
      add(at, player ? IM_COL32(220, 226, 235, 71) : IM_COL32(231, 133, 115, 255), 0.94f, border(0.94f));
    }
    // The foes in reach of a strike (Engine2 GRID_ENEMY_TARGET: its fill and border).
    for (const auto& [t, from] : targets_) {
      const e2::BattleUnit& f = battle_->units()[static_cast<size_t>(unitBattle_[static_cast<size_t>(t)])];
      add({f.x, f.y}, IM_COL32(110, 0, 8, 217), 1.0f);
      add({f.x, f.y}, IM_COL32(110, 0, 8, 255), 0.94f, border(0.94f));
    }
    // The cursor: red where nothing can stand.
    if (hover_) {
      const bool blocked = !battle_->cell(hover_->x, hover_->y).passable;
      add(*hover_, IM_COL32(8, 12, 16, 242), 0.985f, border(0.985f));
      add(*hover_, blocked ? IM_COL32(231, 133, 115, 242) : IM_COL32(220, 226, 235, 255), 0.94f, border(0.94f));
    }
    if (fog_ && !noFog_ && fogReady_)
      std::erase_if(marks, [&](const Renderer::GroundMark& m) { return m.cell < fogStates_.size() && fogStates_[m.cell] == e2::kFogUnseen; });
  }

  // Over the scene: the numbers popping off struck units and the strike forecast.
  void drawMarks() {
    if (!battle_ || !scene_) return;
    const ImGuiIO& io = ImGui::GetIO();
    drawPopups(ImGui::GetBackgroundDrawList(), io.DisplaySize.x, io.DisplaySize.y);
  }

  // The numbers over a struck unit: up 0.6 units over 1.2 s while they fade (crits larger and gold; "Errou" for a miss).
  void drawPopups(ImDrawList* dl, float w, float h) {
    std::erase_if(popups_, [&](const Popup& p) { return now_ - p.start > kPopupSeconds; });
    for (const Popup& p : popups_) {
      const CardRect& c = unitCard_[p.unit];
      const float k = (now_ - p.start) / kPopupSeconds;
      const glm::vec3 up = c.flat ? glm::vec3(0.0f, 0.0f, -1.0f) : glm::vec3(0.0f, 1.0f, 0.0f);
      const std::optional<ImVec2> at = screenOf(c.origin + up * (c.height + 0.6f * k), w, h);
      if (!at) continue;
      const float size = p.big ? 34.0f : 26.0f;
      ImGui::PushFont(nullptr, size);
      const ImVec2 t = ImGui::CalcTextSize(p.text.c_str());
      const ImU32 alpha = static_cast<ImU32>(255.0f * std::clamp(1.5f - 1.5f * k, 0.0f, 1.0f));
      const ImVec2 pos{at->x - t.x * 0.5f, at->y - t.y};
      dl->AddText(nullptr, size, {pos.x + 2, pos.y + 2}, (alpha << IM_COL32_A_SHIFT), p.text.c_str());
      dl->AddText(nullptr, size, pos, (p.color & ~IM_COL32_A_MASK) | (alpha << IM_COL32_A_SHIFT), p.text.c_str());
      ImGui::PopFont();
    }
  }

  // Engine2's forecast for the hovered target: the strike and the counter, as an ember plate by the cursor.
  void drawForecast() {
    if (!hover_ || selected_ < 0 || exchange_) return;
    const int foe = sceneUnitOf(battle_->unitAt(hover_->x, hover_->y));
    const auto target = std::find_if(targets_.begin(), targets_.end(), [&](const auto& t) { return t.first == foe; });
    if (foe < 0 || target == targets_.end()) return;
    const size_t ab = static_cast<size_t>(unitBattle_[static_cast<size_t>(selected_)]), db = static_cast<size_t>(unitBattle_[static_cast<size_t>(foe)]);
    const e2::BattleUnit &a = battle_->units()[ab], &d = battle_->units()[db];
    const e2::Forecast f = e2::forecast(*battle_, ab, target->second, db);  // from the hex it would strike from
    ImGui::SetNextWindowPos({mouse_.x + 18.0f, mouse_.y + 18.0f}, ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {12.0f, 10.0f});
    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(14, 14, 14, 240));
    ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(190, 150, 95, 115));
    ImGui::Begin("##forecast", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoInputs |
                                            ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoSavedSettings);
    ImGui::PushFont(nullptr, 20.0f);
    ImGui::TextColored(ImVec4(1.0f, 0.88f, 0.7f, 1.0f), "%s  ->  %s", a.name.c_str(), d.name.c_str());
    ImGui::Text("Vida %d / %d", d.hp, d.maxHp);
    ImGui::Text("Dano %d  ·  %.0f%%%s", f.dmgOut, f.hitOut, f.kill ? "  ·  mata" : "");
    if (f.canCounter) ImGui::Text("Contra-ataque %d  ·  %.0f%%", f.dmgBack, f.hitBack);
    else ImGui::TextDisabled("Sem contra-ataque");
    ImGui::PopFont();
    ImGui::End();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar();
  }

  // --- Engine2's battle HUD (GameApp.tsx footer .ember-hud-bar, battleHud3d.ts health bars, the turn order) -----------

  // Engine2 ThreeAtmosphereV2: the map's rising wisps (wispIntensity x 2000, wispSpeed, wispColor) and the dust
  // motes always in the air (brighter in sunlight); additive and over everything, as there (no depth test).
  void drawAirParticles() {
    if (!scene_ || flat_) return;
    const e2::MapDraft& d = board_->draft();
    static std::vector<float> wispSeeds, dustSeeds;
    if (wispSeeds.empty()) {
      e2::Mulberry32 a(0x5eed2), b(0x5eed1);
      for (int i = 0; i < 2000 * 4; ++i) wispSeeds.push_back(static_cast<float>(a()));
      for (int i = 0; i < 280 * 4; ++i) dustSeeds.push_back(static_cast<float>(b()));
    }
    const ImGuiIO& io = ImGui::GetIO();
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    const float w = io.DisplaySize.x, h = io.DisplaySize.y;
    const e2::Board::Bounds bb = board_->bounds();
    auto disc = [&](glm::vec3 p, float size, ImU32 rgb, float alpha) {
      const std::optional<ImVec2> at = screenOf(p, w, h);
      const std::optional<ImVec2> side = screenOf(p + glm::vec3(size, 0.0f, 0.0f), w, h);
      if (!at || !side || alpha <= 0.003f) return;
      const float r = std::max(1.0f, std::hypot(side->x - at->x, side->y - at->y));
      // a soft disc: smoothstep(1, 0, d)^2 as three rings
      for (int k = 0; k < 3; ++k) {
        const float rr = r * (1.0f - 0.3f * k), aa = alpha * (0.25f + 0.25f * k);
        dl->AddCircleFilled(*at, rr, (rgb & 0x00FFFFFFu) | (static_cast<ImU32>(std::clamp(aa, 0.0f, 1.0f) * 255.0f) << IM_COL32_A_SHIFT), 12);
      }
    };
    // Wisps: each rises 1.6 hexes from the ground over its life, wavering, fading in and out.
    const int wisps = static_cast<int>(std::round(std::clamp(d.wispIntensity, 0.0f, 1.0f) * 2000.0f));
    if (wisps > 0) {
      const float t = now_ * d.wispSpeed, size = 0.1f, rise = 1.6f, pad = 3.0f;
      const float ox = static_cast<float>(bb.x0) - pad, oz = static_cast<float>(bb.z0) - pad;
      const float ax = static_cast<float>(bb.x1 - bb.x0) + 2 * pad, az = static_cast<float>(bb.z1 - bb.z0) + 2 * pad;
      const ImU32 rgb = IM_COL32((d.wispColor >> 16) & 255, (d.wispColor >> 8) & 255, d.wispColor & 255, 0);
      for (int i = 0; i < wisps; ++i) {
        const float* sd = &wispSeeds[static_cast<size_t>(i) * 4];
        float phase = sd[3] + t * 0.055f * (0.6f + sd[2] * 0.8f);
        phase -= std::floor(phase);
        const float x = ox + sd[0] * ax + std::sin(t * 0.7f + sd[2] * 40.0f) * size * 2.5f * phase;
        const float z = oz + sd[1] * az + std::cos(t * 0.5f + sd[3] * 30.0f) * size * 2.5f * phase;
        const float y = groundAt(std::clamp(x, static_cast<float>(bb.x0), static_cast<float>(bb.x1)), std::clamp(z, static_cast<float>(bb.z0), static_cast<float>(bb.z1))) + phase * rise;
        const float alpha = std::sin(phase * 3.14159f) * (0.6f + 0.4f * std::sin(t * 3.0f + sd[2] * 60.0f)) * 0.9f;
        disc({x, y, z}, size * (0.7f + sd[2] * 0.6f), rgb, alpha);
      }
    }
    // Dust motes drifting with the wind over the board, twinkling.
    {
      const float t = now_, size = 0.06f, height = 2.2f;
      const float ox = static_cast<float>(bb.x0) - 2.0f, oz = static_cast<float>(bb.z0) - 2.0f;
      const float ax = static_cast<float>(bb.x1 - bb.x0) + 4.0f, az = static_cast<float>(bb.z1 - bb.z0) + 4.0f;
      const float sun = layers_ & 1u ? 0.6f : 0.0f;  // brighter in sunlight
      for (int i = 0; i < 280; ++i) {
        const float* sd = &dustSeeds[static_cast<size_t>(i) * 4];
        float wx = sd[0] * ax + 0.12f * t * (0.6f + sd[2] * 0.8f) + std::sin(t * 0.21f + sd[3] * 31.0f) * size * 3.0f;
        float wz = sd[1] * az + 0.05f * t * (0.6f + sd[2] * 0.8f) + std::cos(t * 0.17f + sd[2] * 23.0f) * size * 3.0f;
        wx = std::fmod(wx, ax), wz = std::fmod(wz, az);
        const float x = ox + wx, z = oz + wz;
        const float ground = groundAt(std::clamp(x, static_cast<float>(bb.x0), static_cast<float>(bb.x1)), std::clamp(z, static_cast<float>(bb.z0), static_cast<float>(bb.z1)));
        const float y = ground + height * (0.15f + sd[2] * 0.85f) + std::sin(t * 0.35f + sd[3] * 19.0f) * size * 2.0f;
        const float twinkle = 0.5f + 0.5f * std::sin(t * (0.5f + sd[3] * 0.9f) + sd[2] * 40.0f);
        const float edge = std::clamp(std::min({wx, ax - wx, wz, az - wz}) / (std::min(ax, az) * 0.08f), 0.0f, 1.0f);
        const float alpha = (0.25f + 0.75f * twinkle * twinkle) * edge * (0.22f + 0.4f * sun);
        disc({x, y, z}, size * (0.55f + sd[3] * 0.9f), IM_COL32(255, 242, 217, 0), alpha * 0.6f);
      }
    }
  }

  // GameApp portraitFor: a hero's own portrait, an enemy's head-and-shoulders cut, else its sprite's first frame.
  const UiImage* portraitOf(const e2::BattleUnit& u) const {
    static const std::pair<const char*, const char*> kHero[] = {
        {"defaultWarrior", "game/portraits/kael.png"}, {"kaelFinal", "game/portraits/kael-final-face-001.jpg"},
        {"neera", "game/portraits/NeeraProperSide-profile.jpg"}, {"voss", "game/portraits/voss.png"},
        {"salazar", "game/portraits/salazar-v2-bust.jpg"}, {"aldric", "game/portraits/aldric-profile-001.jpg"},
        {"defaultLancer", "game/portraits/aldric-profile-001.jpg"}, {"sandoval", "game/portraits/sandoval-001.jpg"},
        {"conjurer", "game/portraits/conjurer-002.png"}, {"malrec", "game/portraits/conjurer-002.png"},
        {"theButcher", "game/portraits/the-butcher-portrait-001.jpg"}, {"familiar3", "game/portraits/familiar3-profile.jpeg"},
        {"familiar", "game/portraits/familiar1-profile.png"}};
    const std::filesystem::path root = options_.engine2Public;
    for (const auto& [sprite, file] : kHero)
      if (u.sprite == sprite) return uiImage(root / file);
    std::error_code error;
    const std::filesystem::path cut = root / "game/portraits/sprites" / (u.sprite + ".png");
    if (std::filesystem::exists(cut, error)) return uiImage(cut);
    const std::filesystem::path frame = root / "game/sprites" / u.sprite / "1.png";
    if (std::filesystem::exists(frame, error)) return uiImage(frame);
    return nullptr;
  }

  // GameApp slotIcon / data.ts spellIcon.
  const UiImage* spellIconOf(const std::string& kind) const {
    static const std::pair<const char*, const char*> kIcons[] = {
        {"doubleStrike", "refresh-001/spells/cleave-crossed-blades.png"}, {"cleave", "refresh-001/spells/cleave.png"},
        {"fireball", "refresh-001/spells/fireball.png"}, {"frost", "frost.jpg"}, {"iceStorm", "ice-storm.jpg"},
        {"causticVenom", "refresh-001/spells/caustic-venom.png"}, {"minorVenom", "refresh-001/spells/caustic-venom.png"},
        {"divineBolt", "divine-bolt.jpg"}, {"lightning", "refresh-001/spells/lightning.png"}, {"lightningTier3", "refresh-001/spells/lightning.png"},
        {"shock", "refresh-001/spells/lightning.png"}, {"magicMissile", "refresh-001/spells/magic-missile.png"},
        {"longShot", "refresh-001/spells/long-shot.png"}, {"bloodyShot", "bloody-shot-002.jpg"}, {"piercing", "refresh-001/spells/piercing.png"},
        {"cureMinor", "refresh-006/healing/cure-light-new-006.png"}, {"cureWounds", "refresh-007/healing/cure-medium-golden-bowl-007.png"},
        {"cureDisease", "refresh-007/healing/cure-disease-green-lantern-007.png"}, {"piercingThrust", "refresh-001/spells/piercing-thrust.png"},
        {"sweep", "refresh-001/spells/sweep.png"}, {"trip", "refresh-001/spells/trip.png"}, {"summonFamiliar", "refresh-001/spells/summon-familiar.png"},
        {"phantasmalForce", "phantasmal-force.png"}, {"summonFamiliar2", "summon-familiar2.png"}, {"summonFamiliar3", "summon-familiar3.png"},
        {"summonFamiliar4", "summon-familiar4.png"}, {"summonZombieDog", "summon-zombie-dog-002.png"}, {"webOfDreams", "refresh-001/spells/web-of-dreams.png"},
        {"warp", "warp-portal.png"}, {"lifeDrain", "refresh-001/spells/life-drain.png"}, {"multiShot", "refresh-006/combat/multi-shot-006.png"},
        {"cureLight", "refresh-006/healing/cure-light-new-006.png"}, {"bless", "refresh-001/spells/bless.png"}, {"provoke", "provoke.jpg"},
        {"bullRush", "bull-rush.png"}, {"executionerStrike", "executioner-strike-002.png"}, {"shieldBash", "shield-bash.png"},
        {"burningHands", "burning-hands.png"}, {"poisonBreath", "poison-breath.png"}, {"createFoodAndWater", "create-food-and-water.png"},
        {"turnUndead", "turn-undead.jpg"}};
    for (const auto& [k, file] : kIcons)
      if (kind == k) return uiImage(options_.engine2Public / "game/icons" / file);
    return nullptr;
  }

  // .ember-btn.ember-btn-sm: Pirata One, primary (the ember gradient) or ghost (dark iron, an ember underline on hover).
  bool emberButton(const char* id, const char* label, bool primary, bool enabled, float height = 32.0f) {
    ImFont* font = uiFont(UiFont::Mark);
    const float size = 16.0f;
    const float w = spacedTextWidth(font, size, label, 0.06f) + 28.0f;
    ImGui::BeginDisabled(!enabled);
    const bool clicked = ImGui::InvisibleButton(id, {w, height});
    const bool hover = enabled && ImGui::IsItemHovered();
    ImGui::EndDisabled();
    const ImVec2 a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const int fade = enabled ? 255 : 102;
    auto k = [&](int r, int g, int bl) {
      const float m = hover ? 1.1f : 1.0f;
      return IM_COL32(std::min(255, static_cast<int>(r * m)), std::min(255, static_cast<int>(g * m)), std::min(255, static_cast<int>(bl * m)), fade);
    };
    for (int i = 4; i >= 1; --i) dl->AddRectFilled({a.x - i, a.y + i * 2.0f}, {b.x + i, b.y + i * 2.0f}, IM_COL32(0, 0, 0, enabled ? 28 : 12), 3.0f);
    dl->AddRect({a.x - 1, a.y - 1}, {b.x + 1, b.y + 1}, IM_COL32(0, 0, 0, fade), 2.0f);
    if (primary) {
      if (hover) for (int i = 3; i >= 1; --i) dl->AddRect({a.x - i * 3.0f, a.y - i * 3.0f}, {b.x + i * 3.0f, b.y + i * 3.0f}, IM_COL32(255, 120, 35, 22), 4.0f, 3.0f);
      verticalGradient(dl, a, b, {{0.0f, k(0xd9, 0x62, 0x1c)}, {0.55f, k(0x9c, 0x2f, 0x10)}, {1.0f, k(0x56, 0x17, 0x08)}});
      dl->AddLine({a.x + 1, a.y + 1}, {b.x - 1, a.y + 1}, IM_COL32(255, 225, 160, enabled ? 140 : 50));
      dl->AddRect(a, b, IM_COL32(255, 200, 130, enabled ? 128 : 50), 2.0f);
    } else {
      verticalGradient(dl, a, b, {{0.0f, k(0x23, 0x23, 0x23)}, {0.55f, k(0x0a, 0x0a, 0x0a)}, {1.0f, k(0x1a, 0x1a, 0x1a)}});
      dl->AddRect(a, b, hover ? IM_COL32(226, 170, 100, 216) : IM_COL32(190, 150, 95, enabled ? 128 : 60), 2.0f);
      if (hover) {
        const float l = a.x + (b.x - a.x) * 0.18f, r = b.x - (b.x - a.x) * 0.18f, y = b.y - 3.0f;
        dl->AddRectFilledMultiColor({l, y}, {(l + r) * 0.5f, y + 1}, IM_COL32(226, 104, 30, 0), IM_COL32(226, 104, 30, 255), IM_COL32(226, 104, 30, 255), IM_COL32(226, 104, 30, 0));
        dl->AddRectFilledMultiColor({(l + r) * 0.5f, y}, {r, y + 1}, IM_COL32(226, 104, 30, 255), IM_COL32(226, 104, 30, 0), IM_COL32(226, 104, 30, 0), IM_COL32(226, 104, 30, 255));
      }
    }
    const float tw = spacedTextWidth(font, size, label, 0.06f);
    const ImVec2 p{std::round((a.x + b.x - tw) * 0.5f), std::round((a.y + b.y - size) * 0.5f)};
    const ImU32 ink = primary ? IM_COL32(255, 241, 216, fade) : hover ? IM_COL32(255, 224, 180, 255) : IM_COL32(226, 194, 148, fade);
    spacedText(dl, font, size, {p.x, p.y + 1}, IM_COL32(0, 0, 0, fade), label, 0.06f, false);
    spacedText(dl, font, size, p, ink, label, 0.06f, false);
    return clicked && enabled;
  }

  // .ember-socket: a spell's icon with its uses left in the corner (lit while it is being aimed).
  bool spellSocket(const char* id, const std::string& kind, int uses, bool enabled, bool active) {
    const float side = 36.0f;
    ImGui::BeginDisabled(!enabled);
    const bool clicked = ImGui::InvisibleButton(id, {side, side});
    const bool hover = enabled && ImGui::IsItemHovered();
    ImGui::EndDisabled();
    const ImVec2 a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    verticalGradient(dl, a, b, {{0.0f, IM_COL32(12, 12, 12, 255)}, {1.0f, IM_COL32(5, 5, 5, 255)}});
    const int fade = enabled ? 255 : 102;
    if (const UiImage* icon = spellIconOf(kind)) dl->AddImage(icon->texture, {a.x + 6, a.y + 6}, {b.x - 6, b.y - 6}, {0, 0}, {1, 1}, IM_COL32(255, 255, 255, fade));
    if (active) for (int i = 3; i >= 1; --i) dl->AddRect({a.x - i * 2.0f, a.y - i * 2.0f}, {b.x + i * 2.0f, b.y + i * 2.0f}, IM_COL32(255, 110, 30, 40), 3.0f, 2.0f);
    dl->AddRect(a, b, active ? IM_COL32(226, 104, 30, 255) : hover ? IM_COL32(226, 170, 100, 216) : IM_COL32(190, 150, 95, 89), 2.0f);
    const std::string count = std::to_string(uses);
    ImFont* font = uiFont(UiFont::Ui);
    const ImVec2 t = font->CalcTextSizeA(10.0f, FLT_MAX, 0.0f, count.c_str());
    const ImVec2 c{b.x - t.x - 2.0f, b.y - t.y + 2.0f};
    dl->AddRectFilled({c.x - 2, c.y - 1}, {b.x + 3, b.y + 3}, IM_COL32(28, 28, 28, 255), 2.0f);
    dl->AddRect({c.x - 2, c.y - 1}, {b.x + 3, b.y + 3}, IM_COL32(74, 74, 74, 255), 2.0f);
    dl->AddText(font, 10.0f, c, IM_COL32(240, 240, 240, fade), count.c_str());
    if (hover) {
      ImGui::BeginTooltip();
      ImGui::PushFont(uiFont(UiFont::Ui), 15.0f);
      ImGui::TextUnformatted(e2::spellName(kind).c_str());
      if (!e2::spellPorted(kind)) ImGui::TextDisabled("ainda não existe no Ember");
      ImGui::PopFont();
      ImGui::EndTooltip();
    }
    return clicked && enabled;
  }

  // The unit the bar describes: the one under the cursor, else the one picked, else whoever's turn it is.
  int hudUnit() const {
    if (hover_) {
      const int b = battle_->unitAt(hover_->x, hover_->y);
      const int ui = sceneUnitOf(b);
      if (ui >= 0 && unitCard_[static_cast<size_t>(ui)].drawn && battle_->units()[static_cast<size_t>(b)].alive) return b;
    }
    if (selected_ >= 0 && unitBattle_[static_cast<size_t>(selected_)] >= 0) return unitBattle_[static_cast<size_t>(selected_)];
    return battle_->activeUnit();
  }

  static std::string sheetLine(const e2::BattleUnit& u) {
    const std::string range = u.minRange == u.maxRange ? std::to_string(u.maxRange) : std::format("{}-{}", u.minRange, u.maxRange);
    return std::format("AT {} · MAG {} · DF {} · DEX {} · Mov {} · Alc {}", u.atk, u.mag, u.def, u.dex, u.mov, range);
  }

  void drawTurnBar() {
    if (!battle_) return;
    drawTurnPlate();
    drawHealthBars();
    drawTurnOrder();
    drawHudBar();
  }

  // GameApp's footer: portrait, name · level · XP, class, HP bar, the sheet line or the forecast; then the actions.
  void drawHudBar() {
    const ImGuiIO& io = ImGui::GetIO();
    const float height = 132.0f;
    ImGui::SetNextWindowPos({0.0f, io.DisplaySize.y - height});
    ImGui::SetNextWindowSize({io.DisplaySize.x, height});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {10.0f, 8.0f});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {6.0f, 4.0f});
    ImGui::Begin("##hudbar", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBackground |
                                          ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 a = ImGui::GetWindowPos(), b{a.x + io.DisplaySize.x, a.y + height};
    for (int i = 10; i >= 1; --i) dl->AddRectFilled({a.x, a.y - i * 2.4f}, {b.x, a.y}, IM_COL32(0, 0, 0, 9));  // 0 -10px 24px
    verticalGradient(dl, a, b, {{0.0f, IM_COL32(24, 24, 24, 255)}, {1.0f, IM_COL32(10, 10, 10, 255)}});
    dl->AddLine({a.x, a.y - 1}, {b.x, a.y - 1}, IM_COL32(0, 0, 0, 255));
    dl->AddLine(a, {b.x, a.y}, IM_COL32(190, 150, 95, 115));
    {  // the ember seam along the top, 20% in from each side
      const float l = a.x + io.DisplaySize.x * 0.2f, r = b.x - io.DisplaySize.x * 0.2f, m = (l + r) * 0.5f;
      for (int g = 3; g >= 0; --g) {
        const int al = g == 0 ? 255 : 40;
        dl->AddRectFilledMultiColor({l, a.y - g}, {m, a.y + 1 + g}, IM_COL32(226, 104, 30, 0), IM_COL32(226, 104, 30, al), IM_COL32(226, 104, 30, al), IM_COL32(226, 104, 30, 0));
        dl->AddRectFilledMultiColor({m, a.y - g}, {r, a.y + 1 + g}, IM_COL32(226, 104, 30, al), IM_COL32(226, 104, 30, 0), IM_COL32(226, 104, 30, 0), IM_COL32(226, 104, 30, al));
      }
    }
    ImFont* text = uiFont(UiFont::Ui);
    const int ub = hudUnit();
    const float rowY = a.y + 8.0f;
    if (ub >= 0) {
      const e2::BattleUnit& u = battle_->units()[static_cast<size_t>(ub)];
      float x = a.x + 10.0f;
      if (const UiImage* face = portraitOf(u)) {
        const ImVec2 p{x, rowY}, q{x + 56.0f, rowY + 80.0f};
        dl->AddRectFilled(p, q, IM_COL32(17, 17, 17, 255), 6.0f);
        imageCover(dl, *face, p, q, 0.5f, 0.3f);
        dl->AddRect(p, q, IM_COL32(74, 74, 74, 255), 6.0f);
        x = q.x + 10.0f;
      }
      const float right = b.x - 10.0f;
      ImGui::PushFont(text, 14.0f);
      const std::string name = std::format("{} · Nv {}", u.name, u.level);
      dl->AddText(text, 15.0f, {x, rowY + 2.0f}, IM_COL32(240, 240, 240, 255), name.c_str());
      float nx = x + text->CalcTextSizeA(15.0f, FLT_MAX, 0.0f, name.c_str()).x + 6.0f;
      if (u.side == "player") {
        const std::string xp = u.level >= 30 ? "· NÍVEL MÁX." : std::format("· {}/{} XP", u.xp, xpToLevel(u.level));
        dl->AddText(text, 12.0f, {nx, rowY + 4.5f}, IM_COL32(180, 180, 180, 255), xp.c_str());
      }
      const std::string cls = u.className.empty() ? u.classId : u.className;
      const float cw = text->CalcTextSizeA(12.0f, FLT_MAX, 0.0f, cls.c_str()).x;
      dl->AddText(text, 12.0f, {right - cw, rowY + 4.5f}, u.side == "enemy" ? IM_COL32(181, 74, 50, 255) : IM_COL32(180, 180, 180, 255), cls.c_str());
      // HP bar (.hp-fill: accent for the party, danger for foes)
      const std::string hp = std::format("{}/{}", u.hp, u.maxHp);
      const float hw = text->CalcTextSizeA(12.0f, FLT_MAX, 0.0f, hp.c_str()).x;
      const float barY = rowY + 28.0f, barR = right - hw - 8.0f;
      dl->AddRectFilled({x, barY}, {barR, barY + 6.0f}, IM_COL32(74, 74, 74, 255), 3.0f);
      const float f = u.maxHp > 0 ? std::clamp(static_cast<float>(u.hp) / static_cast<float>(u.maxHp), 0.0f, 1.0f) : 0.0f;
      if (f > 0.0f) dl->AddRectFilled({x, barY}, {x + (barR - x) * f, barY + 6.0f}, u.side == "enemy" ? IM_COL32(181, 74, 50, 255) : IM_COL32(216, 216, 216, 255), 3.0f);
      dl->AddText(text, 12.0f, {right - hw, barY - 4.0f}, IM_COL32(240, 240, 240, 255), hp.c_str());
      // the sheet line, or the strike's forecast while a marked foe is under the cursor
      std::string line = sheetLine(u);
      if (selected_ >= 0 && hover_ && !targets_.empty()) {
        const int foe = sceneUnitOf(battle_->unitAt(hover_->x, hover_->y));
        const auto t = std::find_if(targets_.begin(), targets_.end(), [&](const auto& q) { return q.first == foe; });
        if (foe >= 0 && t != targets_.end()) {
          const size_t ab = static_cast<size_t>(unitBattle_[static_cast<size_t>(selected_)]), db = static_cast<size_t>(unitBattle_[static_cast<size_t>(foe)]);
          const e2::Forecast fc = e2::forecast(*battle_, ab, t->second, db);
          line = fc.canCounter ? std::format("{:.0f}% em {} · contra {:.0f}%", fc.hitOut, battle_->units()[db].name, fc.hitBack)
                               : std::format("{:.0f}% em {} · sem contra", fc.hitOut, battle_->units()[db].name);
        }
      }
      dl->AddText(text, 11.5f, {x, barY + 12.0f}, IM_COL32(180, 180, 180, 255), line.c_str());
      if (!tip_.empty() && now_ - tipAt_ < 3.0f) dl->AddText(text, 12.0f, {x, barY + 30.0f}, IM_COL32(224, 189, 128, 255), tip_.c_str());
      ImGui::PopFont();
    } else {
      dl->AddText(text, 12.0f, {a.x + 10.0f, rowY + 4.0f}, IM_COL32(180, 180, 180, 255),
                  battle_->activeUnit() >= 0 && battle_->units()[static_cast<size_t>(battle_->activeUnit())].side == "enemy"
                      ? "O inimigo age…"
                      : "Clique num aliado ou num inimigo para ver o status.");
    }
    // the actions row
    ImGui::SetCursorScreenPos({a.x + 10.0f, b.y - 32.0f - 10.0f});
    const bool mine = myTurn() && !busy();
    const e2::BattleUnit* actor = mine ? &battle_->units()[static_cast<size_t>(unitBattle_[static_cast<size_t>(selected_)])] : nullptr;
    const bool canAct = actor && !actor->acted;
    if (emberButton("##attack", "Atacar", true, canAct && !targets_.empty() && spellMode_.empty())) attackMode_ = !attackMode_;
    ImGui::SameLine();
    if (emberButton("##wait", "Esperar", false, mine)) endUnitTurn();
    ImGui::SameLine();
    if (emberButton("##cancel", "Cancelar", false, mine && (!spellMode_.empty() || attackMode_))) {
      spellMode_.clear(), spellCells_.clear(), spellAim_.clear();
      attackMode_ = false;
    }
    if (actor) {
      int n = 0;
      for (const std::string& kind : actor->spellList) {
        ImGui::SameLine();
        const int uses = e2::spellUsesLeft(*battle_, static_cast<size_t>(unitBattle_[static_cast<size_t>(selected_)]), kind);
        const bool ok = canAct && e2::spellPorted(kind) && uses > 0;
        if (spellSocket(std::format("##spell{}", n++).c_str(), kind, uses, ok, spellMode_ == kind)) startSpell(kind);
      }
    }
    const float endW = spacedTextWidth(uiFont(UiFont::Mark), 16.0f, "Fim do turno", 0.06f) + 28.0f;
    ImGui::SameLine(io.DisplaySize.x - endW - 20.0f);
    if (emberButton("##endturn", "Fim do turno", false, mine)) endUnitTurn();
    ImGui::End();
    ImGui::PopStyleVar(3);
  }

  static int xpToLevel(int level) {
    const int c = std::max(1, std::min(29, level));
    return c >= 28 ? 600 : c >= 23 ? 500 : c >= 18 ? 400 : c >= 13 ? 300 : c >= 8 ? 225 : c >= 3 ? 150 : 100;
  }

  void endUnitTurn() {
    if (selected_ < 0) return;
    battle_->units()[static_cast<size_t>(unitBattle_[static_cast<size_t>(selected_)])].moved = true;
    select(-1);
  }

  // battleHud3d.ts healthBars: a bar and the HP over each unit's head, sized by its hex on screen.
  void drawHealthBars() {
    const ImGuiIO& io = ImGui::GetIO();
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    const float w = io.DisplaySize.x, h = io.DisplaySize.y;
    for (size_t i = 0; i < scene_->units.size(); ++i) {
      const int b = unitBattle_[i];
      if (b < 0 || !unitCard_[i].drawn) continue;
      const e2::BattleUnit& u = battle_->units()[static_cast<size_t>(b)];
      if (!u.alive) continue;
      const CardRect& c = unitCard_[i];
      const glm::vec3 up = c.flat ? glm::vec3(0.0f, 0.0f, -1.0f) : glm::vec3(0.0f, 1.0f, 0.0f);
      const glm::vec3 head = c.origin + up * c.height;
      const std::optional<ImVec2> at = screenOf(head, w, h);
      const std::optional<ImVec2> side = screenOf(head + glm::vec3(1.0f, 0.0f, 0.0f), w, h);
      const std::optional<ImVec2> deep = screenOf(head + glm::vec3(0.0f, 0.0f, 1.0f), w, h);
      if (!at || !side || !deep) continue;
      const float px = std::max(std::hypot(side->x - at->x, side->y - at->y), std::hypot(deep->x - at->x, deep->y - at->y));
      const float cell = px * 1.7320508f;
      const float gap = cell * 0.12f;
      const float width = cell * (u.size >= 4 ? 1.35f : u.size == 2 ? 0.9f : 0.62f);
      const float height = std::max(3.0f, cell * 0.07f);
      const float left = at->x - width * 0.5f, top = at->y - gap - height;
      const int alpha = u.moved && u.side == "player" ? 230 : 255;
      dl->AddRectFilled({left - 1, top - 1}, {left + width + 1, top + height + 1}, IM_COL32(12, 11, 10, 209 * alpha / 255));
      dl->AddRectFilled({left, top}, {left + width, top + height}, IM_COL32(44, 40, 36, alpha));
      const ImU32 fill = u.side == "player" ? IM_COL32(200, 196, 188, alpha) : u.side == "neutral" ? IM_COL32(95, 158, 82, alpha) : IM_COL32(181, 74, 50, alpha);
      const float f = u.maxHp > 0 ? std::clamp(static_cast<float>(u.hp) / static_cast<float>(u.maxHp), 0.0f, 1.0f) : 0.0f;
      if (f > 0.0f) dl->AddRectFilled({left, top}, {left + width * f, top + height}, fill);
      if (cell >= 32.0f) {
        ImFont* font = uiFont(UiFont::Ui);
        const float size = std::max(10.0f, std::round(cell * 0.22f));
        const std::string hp = std::to_string(u.hp);
        const ImVec2 t = font->CalcTextSizeA(size, FLT_MAX, 0.0f, hp.c_str());
        const ImVec2 p{at->x - t.x * 0.5f, top - 1.0f - t.y};
        for (int dx = -1; dx <= 1; ++dx)
          for (int dy = -1; dy <= 1; ++dy)
            if (dx || dy) dl->AddText(font, size, {p.x + dx, p.y + dy}, IM_COL32(12, 11, 10, 230), hp.c_str());
        dl->AddText(font, size, p, IM_COL32(240, 235, 227, 255), hp.c_str());
      }
    }
  }

  // GameApp's "Ordem": a small button top left that unfolds this round's order.
  void drawTurnOrder() {
    ImGui::SetNextWindowPos({8.0f, 8.0f});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0.0f, 0.0f});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {2.0f, 4.0f});
    ImGui::Begin("##order", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove |
                                         ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoSavedSettings);
    ImFont* font = uiFont(UiFont::Ui);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    auto chip = [&](const char* id, const std::string& label, ImU32 ink, ImU32 bg, bool button) {
      const ImVec2 t = font->CalcTextSizeA(11.0f, FLT_MAX, 0.0f, label.c_str());
      const ImVec2 size{t.x + 14.0f, t.y + 9.0f};
      bool clicked = false;
      if (button) clicked = ImGui::InvisibleButton(id, size);
      else ImGui::Dummy(size);
      const ImVec2 a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax();
      dl->AddRectFilled(a, b, bg, 6.0f);
      dl->AddRect(a, b, IM_COL32(255, 255, 255, 38), 6.0f);
      dl->AddText(font, 11.0f, {a.x + 7.0f, a.y + 4.5f}, ink, label.c_str());
      return clicked;
    };
    if (chip("##ordem", "Ordem", IM_COL32(203, 213, 225, 255), IM_COL32(17, 27, 34, 242), true)) showTurnOrder_ = !showTurnOrder_;
    if (showTurnOrder_) {
      const int active = battle_->activeUnit();
      int n = 1;
      bool first = true;
      for (size_t b : battle_->turnOrder()) {
        const e2::BattleUnit& u = battle_->units()[b];
        if (!u.alive) continue;
        if (!first) ImGui::SameLine();
        first = false;
        const bool now = static_cast<int>(b) == active;
        const ImU32 ink = now ? IM_COL32(255, 224, 180, 255) : u.moved ? IM_COL32(100, 116, 139, 255) : u.side == "enemy" ? IM_COL32(231, 133, 115, 255) : IM_COL32(203, 213, 225, 255);
        chip(std::format("##q{}", b).c_str(), std::format("{} {}", n++, u.name), ink, now ? IM_COL32(37, 49, 59, 242) : IM_COL32(17, 27, 34, 217), false);
      }
    }
    ImGui::End();
    ImGui::PopStyleVar(3);
  }

  // Esc's pause menu, and after the battle's end its way out: ember plates in the middle of the screen.
  void drawPauseMenu() {
    if (!pauseOpen_ || !result_.empty()) return;
    const ImGuiIO& io = ImGui::GetIO();
    const bool end = !result_.empty();
    ImGui::SetNextWindowPos({io.DisplaySize.x * 0.5f, io.DisplaySize.y * (end ? 0.55f : 0.5f)}, ImGuiCond_Always, {0.5f, 0.0f});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {kPlateGlow, kPlateGlow});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {0.0f, 10.0f});
    ImGui::Begin("##pause", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove |
                                         ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoSavedSettings);
    ImGui::PushFont(nullptr, 18.0f);
    auto button = [&](const char* id, const char* label) {
      const float w = 240.0f, h = 40.0f;
      return plate(id, {w, h}, [&](ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 ink) {
        const ImVec2 t = ImGui::CalcTextSize(label);
        dl->AddText({std::round((a.x + b.x - t.x) * 0.5f), std::round((a.y + b.y - t.y) * 0.5f)}, ink, label);
      });
    };
    if (!end && button("##continue", "Continuar")) pauseOpen_ = false;
    if (button("##title", end ? "Voltar ao menu" : "Menu principal")) titleRequested_ = true;
    ImGui::PopFont();
    ImGui::End();
    ImGui::PopStyleVar(3);
  }

  // GameApp ResultScreen: the mission's art, "Vitória · T3" / "Derrota", its title and line, each hero's growth (level,
  // XP bar, HP after the fight), then the way on.
  void drawResultScreen() {
    const ImGuiIO& io = ImGui::GetIO();
    const ImVec2 size = io.DisplaySize;
    const float rem = 16.0f * std::clamp(size.y / 900.0f, 0.85f, 2.0f);
    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize(size);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::Begin("##result", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoSavedSettings);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const bool win = result_ == "victory";
    const float k = std::clamp((now_ - resultAt_ - kResultDelay) / 0.5f, 0.0f, 1.0f);  // fades in
    dl->AddRectFilled({0, 0}, size, IM_COL32(17, 17, 17, static_cast<int>(255 * k)));
    const char* art = engine2BriefArt(board_->draft().id);
    if (*art)
      if (const UiImage* image = uiImage(options_.engine2Public / std::filesystem::u8path(art))) {
        imageCover(dl, *image, {0, 0}, size, 0.5f, 0.5f, IM_COL32(255, 255, 255, static_cast<int>(255 * k)));
        verticalGradient(dl, {0, 0}, size, {{0.0f, IM_COL32(17, 17, 17, 51)}, {0.5f, IM_COL32(17, 17, 17, 102)}, {1.0f, IM_COL32(17, 17, 17, 230)}});
      }
    const float x = rem, z = 0.75f;  // the screen's own zoom: 0.75
    float y = 2.0f * rem;
    const std::string kicker = win ? std::format("VITÓRIA · T{}", battle_->round()) : std::format("DERROTA · T{}", battle_->round());
    emberKicker(dl, {x, y}, 0.875f * rem, kicker.c_str(), !win);
    y += 0.875f * rem * 1.4f + 0.5f * rem;
    const std::string title = board_->draft().title.empty() ? board_->draft().id : board_->draft().title;
    emberTitle(dl, {x, y}, 3.0f * rem, title.c_str());
    y += 3.0f * rem * 1.1f + 0.5f * rem;
    ImFont* ui = uiFont(UiFont::Ui);
    dl->AddText(ui, 1.125f * rem, {x, y}, IM_COL32(180, 180, 180, 255), win ? "O campo ficou em silêncio." : "A linha quebrou.");
    y += 1.125f * rem * 1.5f + 1.5f * rem * z;
    // growth: one trough per hero
    const float rowW = std::min(34.0f * rem, size.x - 2.0f * rem);
    for (size_t i = 0; i < battle_->units().size(); ++i) {
      const e2::BattleUnit& u = battle_->units()[i];
      if (u.side != "player" || u.summoned || i >= startLevel_.size()) continue;
      const auto [fromLevel, fromXp] = startLevel_[i];
      const float rowH = 4.6f * rem;
      const ImVec2 p{x, y}, q{x + rowW, y + rowH};
      emberSlot(dl, p, q);
      float ty = p.y + 0.6f * rem;
      std::string head = u.level != fromLevel ? std::format("{} · Nv {} → {}", u.name, fromLevel, u.level) : std::format("{} · Nv {}", u.name, u.level);
      if (!u.alive) head += " · caiu";
      dl->AddText(ui, 1.125f * rem, {p.x + 0.75f * rem, ty}, IM_COL32(240, 240, 240, 255), head.c_str());
      ty += 1.125f * rem * 1.35f;
      // the XP bar: what it had (dim) and what it earned (bright)
      const float barW = 12.0f * rem, barH = 0.4f * rem;
      const int next = xpToLevel(u.level);
      dl->AddRectFilled({p.x + 0.75f * rem, ty + 0.3f * rem}, {p.x + 0.75f * rem + barW, ty + 0.3f * rem + barH}, IM_COL32(74, 74, 74, 255), 2.0f);
      const float had = u.level == fromLevel ? std::clamp(static_cast<float>(fromXp) / next, 0.0f, 1.0f) : 0.0f;
      const float now = std::clamp(static_cast<float>(u.xp) / next, 0.0f, 1.0f);
      if (now > 0.0f)
        dl->AddRectFilled({p.x + 0.75f * rem, ty + 0.3f * rem}, {p.x + 0.75f * rem + barW * now, ty + 0.3f * rem + barH}, IM_COL32(226, 140, 60, 255), 2.0f);
      if (had > 0.0f)
        dl->AddRectFilled({p.x + 0.75f * rem, ty + 0.3f * rem}, {p.x + 0.75f * rem + barW * had, ty + 0.3f * rem + barH}, IM_COL32(216, 216, 216, 255), 2.0f);
      const std::string xp = std::format("{}/{} XP{}", u.xp, next, u.level != fromLevel ? " · subiu" : "");
      dl->AddText(ui, 0.875f * rem, {p.x + 0.75f * rem + barW + 0.5f * rem, ty}, IM_COL32(180, 180, 180, 255), xp.c_str());
      ty += 0.875f * rem * 1.45f;
      dl->AddText(ui, 0.875f * rem, {p.x + 0.75f * rem, ty}, IM_COL32(180, 180, 180, 255), std::format("Combate: {}/{}", u.hp, u.maxHp).c_str());
      y += rowH + 0.5f * rem;
    }
    // the way on
    const float bw = std::min(16.0f * rem, (size.x - 3.0f * rem) * 0.5f), bh = 3.0f * rem;
    const float by = size.y - rem - bh;
    if (!win) {
      if (emberBigButton("##retry", "TENTAR DE NOVO", {x, by}, {bw, bh}, true, rem)) retryRequested_ = true;
      if (emberBigButton("##map", "MAPA", {x + bw + rem, by}, {bw, bh}, false, rem)) titleRequested_ = true;
    } else if (emberBigButton("##map", "MAPA", {x, by}, {bw, bh}, true, rem)) {
      titleRequested_ = true;
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
  }

  void drawTurnPlate() {
    const ImGuiIO& io = ImGui::GetIO();
    std::string text;
    if (result_ == "victory") text = "Vitória";
    else if (result_ == "defeat") text = "Derrota";
    else if (const int a = battle_->activeUnit(); a >= 0)
      text = std::format("Rodada {}  ·  vez de {}", battle_->round(), battle_->units()[static_cast<size_t>(a)].name);
    if (text.empty()) return;
    const bool end = !result_.empty();
    ImGui::SetNextWindowPos({io.DisplaySize.x * 0.5f, end ? io.DisplaySize.y * 0.4f : kBarInset}, ImGuiCond_Always, {0.5f, end ? 0.5f : 0.0f});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {kPlateGlow, kPlateGlow});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::Begin("##turnplate", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove |
                                             ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoFocusOnAppearing |
                                             ImGuiWindowFlags_NoSavedSettings);
    const float size = end ? 40.0f : 18.0f;
    ImGui::PushFont(nullptr, size);
    const ImVec2 t = ImGui::CalcTextSize(text.c_str());
    plate("##turnplatebox", {t.x + (end ? 64.0f : 28.0f), t.y + (end ? 28.0f : 12.0f)}, [&](ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 ink) {
      dl->AddText({std::round((a.x + b.x - t.x) * 0.5f), std::round((a.y + b.y - t.y) * 0.5f)}, ink, text.c_str());
    });
    ImGui::PopFont();
    ImGui::End();
    ImGui::PopStyleVar(2);
  }

  // The party: the first player unit is the hero (test lights, the scout); its walk goes to the farthest enemy.
  void setUpParty() {
    for (size_t i = 0; i < scene_->units.size(); ++i)
      if (scene_->units[i].side == "player") {
        hero_ = static_cast<int>(i);
        break;
      }
    if (hero_ < 0) return;
    const e2::SceneUnit& h = scene_->units[static_cast<size_t>(hero_)];
    heroAt_ = {h.x, groundOf(h.groundSmooth, h.groundHard), h.z};
    if (!options_.scout) return;
    const auto [hc, hr] = e2::hexAt(board_->layout(), h.x, h.z);
    e2::HexPoint goal{hc, hr};
    int best = -1;
    for (const e2::SceneUnit& u : scene_->units) {
      if (u.side != "enemy") continue;
      const auto [c, r] = e2::hexAt(board_->layout(), u.x, u.z);
      const int d = e2::hexDistance({hc, hr}, {c, r});
      if (d > best) best = d, goal = {c, r};
    }
    path_ = e2::hexLine({hc, hr}, goal);
    logInfo("scout: {} walks {} hexes from ({}, {}) to ({}, {}) and back", h.name, path_.size() - 1, hc, hr, goal.c, goal.r);
  }

  // What stands on the board and blocks sight: 3D models over their footprint, standing cards at least
  // kBlockHeight tall and kBlockWidth wide along their width; 3D models at least kBlockWidth across. (Walls and doors are not imported yet: they block nothing.)
  void addOccluders() {
    for (const e2::SceneModel& m : scene_->models) {
      std::string asset = m.file.substr(std::string("/game/").size());
      asset = "meshes/" + asset.substr(0, asset.size() - 4) + ".mesh";
      const MeshHeader* h = meshes_.header(modelMeshes_.at(asset));
      if (!h) continue;
      const float k = m.height / std::max(h->boundsMax[1] - h->boundsMin[1], 1e-4f);
      const float hx = (h->boundsMax[0] - h->boundsMin[0]) * 0.5f * k, hz = (h->boundsMax[2] - h->boundsMin[2]) * 0.5f * k;
      if (2.0f * std::max(hx, hz) < e2::kBlockWidth) continue;  // a thin model (a post) does not block
      const float c = std::cos(m.rotY), sn = std::sin(m.rotY);
      // The footprint (rotated box, centred on the anchor as placeModels centres it), sampled every 0.25.
      for (float a = -hx; a <= hx; a += 0.25f)
        for (float b = -hz; b <= hz; b += 0.25f) fog_->block(m.x + c * a + sn * b, m.z - sn * a + c * b, m.height);
      fog_->block(m.x, m.z, m.height);
    }
    for (const e2::SceneCard& d : scene_->cards) {
      if (d.onFloor || d.h < e2::kBlockHeight || d.w < e2::kBlockWidth) continue;  // thin props do not block
      const float yaw = d.lockedYaw.value_or(0.0f);
      const glm::vec2 right{std::cos(yaw), -std::sin(yaw)};
      const float half = d.w * 0.5f;
      for (float a = -half; a <= half; a += 0.25f) fog_->block(d.x + right.x * a, d.z + right.y * a, d.h);
    }
  }

  // The scout's path index after `step` steps: out to the goal, back, again.
  int walkAt(int step) const {
    const int n = static_cast<int>(path_.size()) - 1, c = step % (2 * n);
    return c <= n ? c : 2 * n - c;
  }

  // Sight: replay every step the scout has taken since the last frame, then paint the mask when anything changed.
  void updateFog(float time) {
    if (hero_ >= 0 && path_.size() > 1) {
      const float stepTime = 0.6f, s = time / stepTime;
      const int step = static_cast<int>(s);
      const int at = walkAt(step), next = walkAt(step + 1);
      const e2::HexLayout& L = board_->layout();
      const float f = s - static_cast<float>(step);
      const glm::vec2 a{static_cast<float>(e2::hexX(L, path_[at].c, path_[at].r)), static_cast<float>(e2::hexZ(L, path_[at].r))};
      const glm::vec2 b{static_cast<float>(e2::hexX(L, path_[next].c, path_[next].r)), static_cast<float>(e2::hexZ(L, path_[next].r))};
      const glm::vec2 p = glm::mix(a, b, f);
      heroAt_ = {p.x, static_cast<float>(e2::groundHeightAt(*board_, look_, p.x, p.y)), p.y};
      // The scout stands on the hex its feet are over.
      const auto [hc, hr] = e2::hexAt(L, p.x, p.y);
      heroHex_ = {hc, hr};
    }
    if (!fog_ || !scene_ || !modelsPlaced_) return;
    bool changed = false;
    if (!occludersAdded_) {
      addOccluders();
      occludersAdded_ = true;
      changed = true;
    }
    // The party's eyes: each living player unit's hex (the scout's where it is now).
    std::vector<e2::HexPoint> party;
    for (size_t i = 0; i < scene_->units.size(); ++i) {
      const e2::SceneUnit& u = scene_->units[i];
      if (u.side != "player") continue;
      if (static_cast<int>(i) == hero_ && path_.size() > 1) {
        party.push_back(heroHex_);
        continue;
      }
      if (battle_ && unitBattle_[i] >= 0 && !battle_->units()[static_cast<size_t>(unitBattle_[i])].alive) continue;
      const auto [c, r] = e2::hexAt(board_->layout(), unitPos_[i].x, unitPos_[i].y);
      party.push_back({c, r});
    }
    if (changed || party != lastParty_) {
      // Every hex walked since the last relight sees in turn (a pinned clock replays the whole walk).
      if (hero_ >= 0 && path_.size() > 1 && options_.sceneTime >= 0.0f && !replayed_) {
        replayed_ = true;
        const int steps = static_cast<int>(time / 0.6f);
        for (int s = 0; s < steps; ++s) {
          std::vector<e2::HexPoint> eyes = party;
          eyes[0] = path_[walkAt(s)];  // the hero is the first player unit
          fogQueue_.push_back(std::move(eyes));
        }
      }
      fogQueue_.push_back(party);
      lastParty_ = party;
    }
    // Sight and the mask are worked out on a worker (6-10 ms on aldeia015); the main thread never waits on it.
    // While a job runs it alone touches fog_; its mask reaches the renderer the frame after it finishes.
    if (fogBusy_ && fogJob_.done()) {
      const auto started = std::chrono::steady_clock::now();
      fogBusy_ = false;
      fogMask_ = std::move(fogNext_);
      fogTexels_.swap(fogNextTexels_);
      ++maskVersion_;
      fogReady_ = true;
      const e2::HexLayout& L = board_->layout();
      fogStates_.resize(static_cast<size_t>(L.cols * L.rows));
      fogCounts_ = {0, 0, 0};
      for (int r = 0; r < L.rows; ++r)
        for (int c = 0; c < L.cols; ++c) {
          const uint8_t s = fog_->state(c, r);
          fogStates_[static_cast<size_t>(e2::indexOf(L, c, r))] = s;
          ++fogCounts_[s];
        }
      countHoles(time);
      lastFogMs_ = fogJobMs_;
      fogDeliverMs_.push_back(std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count());
      fogUpdatedThisFrame_ = true;
    }
    if (!fogBusy_ && !fogQueue_.empty()) {
      // Every party position queued since the last job relights in order, so what is remembered is exactly
      // what one relight per change would leave; the mask is painted once, for the latest.
      fogJobEyes_ = std::move(fogQueue_);
      fogQueue_.clear();
      fogBusy_ = true;
      jobs_.run([this] { runFogJob(); }, &fogJob_);
    }
  }

  // On a worker: relight every queued party position, then paint the mask for the last one.
  void runFogJob() {
    const auto started = std::chrono::steady_clock::now();
    if (!sightLogged_) {
      sightLogged_ = true;
      // How much the line of sight hides from each party unit where it starts.
      const e2::HexLayout& L = board_->layout();
      for (const e2::SceneUnit& u : scene_->units) {
        if (u.side != "player") continue;
        const auto [c, r] = e2::hexAt(L, u.x, u.z);
        int inRange = 0, seen = 0;
        for (int y = 0; y < L.rows; ++y)
          for (int x = 0; x < L.cols; ++x)
            if (e2::hexDistance({c, r}, {x, y}) <= e2::kSightRadius) ++inRange, seen += fog_->sightReaches({c, r}, {x, y}) ? 1 : 0;
        logInfo("sight: {} at ({}, {}) sees {} of the {} hexes in range ({} hidden by ground or what stands on it)", u.name, c, r, seen,
                inRange, inRange - seen);
      }
    }
    for (const std::vector<e2::HexPoint>& eyes : fogJobEyes_) relight(eyes);
    fogNext_ = fog_->paint(*board_, fogJobEyes_.back());
    Renderer::fogTexels(fogNext_, fogNextTexels_);  // the GPU's half floats, so the main thread only copies
    fogJobMs_ = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
  }

  // Holes: groups of never-seen hexes enclosed by explored ground (touching neither the board's edge nor the
  // big never-seen area). Each shows as a black pit in the middle of the map.
  void countHoles(float time) {
    const e2::HexLayout& L = board_->layout();
    std::vector<int> group(static_cast<size_t>(L.cols * L.rows), -1);
    std::vector<std::pair<int, int>> holeList;
    holes_ = 0;
    holeHexes_ = 0;
    for (int r = 0; r < L.rows; ++r)
      for (int c = 0; c < L.cols; ++c) {
        if (fog_->state(c, r) != e2::kFogUnseen || group[static_cast<size_t>(e2::indexOf(L, c, r))] >= 0) continue;
        std::vector<std::pair<int, int>> stack{{c, r}}, members;
        group[static_cast<size_t>(e2::indexOf(L, c, r))] = 1;
        bool edge = false;
        while (!stack.empty()) {
          const auto [x, y] = stack.back();
          stack.pop_back();
          members.push_back({x, y});
          if (x == 0 || y == 0 || x == L.cols - 1 || y == L.rows - 1) edge = true;
          for (const auto& [nx, ny] : e2::neighbors(L, x, y)) {
            int& g = group[static_cast<size_t>(e2::indexOf(L, nx, ny))];
            if (g < 0 && fog_->state(nx, ny) == e2::kFogUnseen) g = 1, stack.push_back({nx, ny});
          }
        }
        if (!edge && members.size() <= 12) {
          ++holes_, holeHexes_ += static_cast<int>(members.size());
          holeList.insert(holeList.end(), members.begin(), members.end());
        }
      }
    worstHoles_ = std::max(worstHoles_, holes_);
    // Where they are: each new set of holes goes to captures/fog/holes.txt with the whole board's states
    // (. never seen, r remembered, v in sight, H hole, S the scout), for tools/scripts/draw_fog_holes.py.
    std::sort(holeList.begin(), holeList.end());
    if (holeList.empty() || holeList == lastHoles_) return;
    lastHoles_ = holeList;
    std::FILE* file = std::fopen("captures/fog/holes.txt", holesLogged_ ? "a" : "w");
    if (!file) return;
    holesLogged_ = true;
    std::fprintf(file, "time %.2f scout %d %d holes %d\n", time, heroHex_.c, heroHex_.r, holes_);
    for (int r = 0; r < L.rows; ++r) {
      for (int c = 0; c < L.cols; ++c) {
        char ch = ".rv"[fog_->state(c, r)];
        if (std::binary_search(holeList.begin(), holeList.end(), std::make_pair(c, r))) ch = 'H';
        if (path_.size() > 1 && heroHex_.c == c && heroHex_.r == r) ch = 'S';
        std::fputc(ch, file);
      }
      std::fputc('\n', file);
    }
    std::fclose(file);
  }

  void relight(const std::vector<e2::HexPoint>& party) {
    std::vector<e2::Eye> eyes;
    for (const e2::HexPoint& p : party) eyes.push_back({p, e2::kSightRadius});
    fog_->relight(eyes);
  }

  // The shortest repeat of Engine2's sampled frame pacing (the whole sample when it does not repeat).
  size_t pacePeriod(const std::vector<int>& pace) {
    auto it = periods_.find(&pace);
    if (it != periods_.end()) return it->second;
    size_t best = pace.size();
    for (size_t p = 1; p <= pace.size() / 2 && best == pace.size(); ++p) {
      bool repeats = true;
      for (size_t i = 0; i + p < pace.size() && repeats; ++i) repeats = pace[i] == pace[i + p];
      if (repeats) best = p;
    }
    periods_[&pace] = best;
    return best;
  }

  Renderer& renderer_;
  UploadQueue uploads_;
  BoardGround ground_;
  SpriteSystem sprites_;
  MeshScene meshes_;
  AssetManager& assets_;
  std::optional<e2::SceneData> scene_;
  inline static const std::string kIdle = "idle";
  std::vector<e2::Hero> catalog_;                  // every unit's poses (setUpPoses)
  std::vector<const e2::Hero*> unitCatalog_;       // per scene unit: its catalog entry (null = idle only)
  std::vector<std::pair<std::string, std::string>> poseChoices_;  // the panel's "Animação": pose id, label
  int poseChoice_ = 0;
  int background_ = 0;  // the panel's "Fundo": 0 the map's painted backdrop, 1 the sky, 2 black
  std::unordered_map<std::string, MeshId> modelMeshes_;
  std::unordered_map<const std::vector<int>*, size_t> periods_;
  std::vector<SpriteInstance> drawList_;
  std::vector<LocalLight> lights_;
  bool modelsPlaced_ = false;
  e2::EdgeStyle placedLook_ = e2::EdgeStyle::Smooth;
  uint32_t modelCount_ = 0, missing_ = 0, fullFrames_ = 0;
  std::chrono::steady_clock::time_point start_ = std::chrono::steady_clock::now();
  bool measuring_ = false, measured_ = false, measurePassed_ = true;
  std::chrono::steady_clock::time_point measureStart_;
  uint32_t measureFrames_ = 0;
  std::vector<double> busy_, gpu_;
  std::vector<std::pair<std::string, double>> passes_;
  std::optional<e2::BoardData> data_;
  std::unique_ptr<e2::Board> board_;
  e2::EdgeStyle look_;
  MapStageOptions options_;
  bool failed_ = false;
  int readyFrames_ = 0;
  glm::vec3 target_{0.0f};
  float boardWidth_ = 1.0f, boardDepth_ = 1.0f;
  float yaw_ = glm::radians(-30.0f), pitch_ = glm::radians(20.0f);
  float distance_ = 0.0f, maxDistance_ = 40.0f;
  // The orbit camera: the goal above (yaw_ ... target_), the view on show gliding to it, its limits and steps.
  OrbitView cam_;
  Camera lastCamera_{};
  float aspect_ = 1.0f;
  struct PieceBox {
    float x, z, radius, top;  // a 3D piece's footprint (square around its centre) and top, world units
  };
  std::vector<PieceBox> pieceBoxes_;
  static constexpr float kGlide = 10.0f;  // 1/s: ~90% of a move in 0.23 s, no overshoot
  static constexpr float kMinPitch = 0.0873f, kMaxPitch = 1.5621f;  // 5 deg (nearly level) to 89.5 deg (straight down)
  static constexpr float kTiltStep = 0.0873f, kTurnStep = 0.2618f;  // Engine2's bar: 5 deg; 15 deg
  static constexpr float kTurnPerPixel = 0.005f, kZoomStep = 0.85f, kPanPerSecond = 0.9f;
  static constexpr float kMinDistance = 3.0f, kEyeClearance = 0.8f;
  bool captured_ = false;  // the first view is remembered: the bar's reset goes back to its angle
  bool flat_ = false;      // Tática: the old-school 2D view (the bar's Normal / Tática)
  // Left-button pan (Engine2 cameraRig: HOLD_MS 500, CLICK_SLOP 5).
  struct LeftPress {
    float x = 0.0f, y = 0.0f;
    std::chrono::steady_clock::time_point at;
  };
  std::optional<LeftPress> leftDown_;
  bool panning_ = false;
  SDL_Cursor* grabCursor_ = nullptr;
  static constexpr int kHoldMs = 500;
  static constexpr float kClickSlop = 5.0f;
  // Combat (world/Battle.h): the battle, each scene unit's battle unit and live state, the picked unit and its reach.
  std::optional<e2::Battle> battle_;
  std::vector<int> unitBattle_;        // per scene unit: its battle unit, -1 = none
  std::vector<glm::vec2> unitPos_;     // per scene unit: where its feet are now (x, z)
  std::vector<uint8_t> unitMoved_;     // it has left its spawn: its height is the ground under it
  std::vector<int> unitFacing_;        // 1 right, -1 left, on screen
  std::vector<uint8_t> unitReversed_;  // its art faces left (Engine2 drawnReversed)
  static constexpr int kWalkBack = 0, kWalkFront = 1, kWalkSide = 2;  // Engine2 WalkDir: up the map, down it, along a row
  struct Walk {
    std::vector<glm::vec2> points;  // the hexes still ahead (feet positions)
    float start = 0.0f;             // scene clock when it set off (the walk cycle's time)
    int dir = kWalkSide;
  };
  std::vector<Walk> unitWalk_;
  struct CardRect {  // a unit's card as drawn this frame, for picking
    glm::vec3 origin{0.0f};  // bottom centre
    float width = 0.0f, height = 0.0f;
    bool flat = false, drawn = false;
  };
  std::vector<CardRect> unitCard_;
  int selected_ = -1;                     // the picked scene unit
  std::vector<e2::ReachCell> reach_;      // where it can stop
  std::vector<e2::ReachCell> reachPaths_; // every hex it can pass (its routes)
  std::optional<e2::BattleHex> hover_;    // the hex under the cursor
  std::vector<std::pair<int, e2::BattleHex>> targets_;  // scene units the unit whose turn it is can strike, and from where
  int pendingStrike_ = -1;                // the foe to strike once the walk to its hex is over
  int turnUnit_ = -1;                     // the battle unit whose turn has begun
  struct EnemyTurn {
    size_t unit = 0;    // scene unit
    int target = -1;    // scene unit it strikes after its walk
    bool struck = false;
    std::vector<e2::BattleStep> steps;  // or the spell it casts after its walk
  };
  std::optional<EnemyTurn> enemyTurn_;
  std::unordered_set<int> awake_;         // enemies that have seen the party (fog)
  std::string result_;                    // "victory" / "defeat" once the battle is over
  bool pauseOpen_ = false, titleRequested_ = false, retryRequested_ = false;
  bool bloomOn_ = true;  // panel "Brilho" (Engine2's full-scene bloom)
  float resultAt_ = 0.0f;                  // when the battle ended (the result screen follows after a breath)
  static constexpr float kResultDelay = 1.8f;
  std::vector<std::pair<int, int>> startLevel_;  // per battle unit: level and XP when the battle began
  std::string spellMode_;                 // the spell being aimed (battle bar), empty = none
  std::vector<e2::BattleHex> spellCells_; // its targets chosen so far
  std::vector<e2::BattleHex> spellAim_;   // every hex it may be aimed at
  bool attackMode_ = false;               // "Atacar": only the foes it can strike are marked
  bool showTurnOrder_ = false;
  std::string tip_;                       // why a click did nothing (Engine2's tip line)
  float tipAt_ = -10.0f;
  std::vector<OneShot> unitShot_;         // per scene unit: a one-shot pose over its own
  std::vector<float> unitDiedAt_;         // per scene unit: when it fell (-1 = alive)
  std::optional<Exchange> exchange_;
  struct Popup {
    size_t unit = 0;
    std::string text;
    ImU32 color = 0;
    float start = 0.0f;
    bool big = false;
  };
  std::vector<Popup> popups_;
  e2::Mulberry32 rng_{1};
  static constexpr float kImpactShare = 0.55f;   // a strike lands this far into the attack pose
  static constexpr float kExchangeGap = 0.15f;   // a breath between the strike and the counter
  static constexpr float kCorpseSeconds = 1.5f;  // the fallen lie this long after their death pose
  static constexpr float kPopupSeconds = 1.2f;
  glm::vec2 mouse_{-1.0f};
  SDL_WindowID mouseWindow_ = 0;
  float now_ = 0.0f;  // this frame's scene clock
  static constexpr float kStepSeconds = 0.22f;      // Engine2 STEP_SECONDS: one hex of walking
  static constexpr float kBorderInner = 0.47f / 0.5f;  // Engine2 BORDER_INNER: a hex border's inner edge
  std::vector<Renderer::GroundMark> groundMarks_;  // this frame's battle grid (collectMarks)
  // Engine2 DIR_ACTION_WALK: sprites whose walks are drawn for each side, never mirrored while moving.
  inline static const std::unordered_set<std::string> kDirectionalWalk = {"aldric", "defaultLancer", "lancer", "sandoval", "theButcher",
      "familiar2", "familiar3", "cultist-v2", "militia-v2", "salazar", "cobalt-blue-deer", "neera"};
  float homeYaw_ = 0.0f, homePitch_ = 0.0f, homeDistance_ = 0.0f;
  glm::vec3 homeTarget_{0.0f};
  std::chrono::steady_clock::time_point lastFrame_ = std::chrono::steady_clock::now();
  glm::vec3 boardCenter_{0.0f};
  float boardRadius_ = 0.0f;
  uint32_t layers_ = 0xFFFFFFFFu;
  // Fog of war and the party.
  std::optional<e2::FogOfWar> fog_;
  e2::FogMask fogMask_;
  bool fogReady_ = false, replayed_ = false, fogUpdatedThisFrame_ = false, occludersAdded_ = false;
  std::vector<uint8_t> fogStates_;  // per hex, as of the mask on show (main thread)
  // The sight job (runFogJob): while fogBusy_, only the worker touches fog_, fogJobEyes_, fogNext_, fogJobMs_.
  JobSystem& jobs_;
  JobCounter fogJob_;
  bool fogBusy_ = false, sightLogged_ = false;
  std::vector<std::vector<e2::HexPoint>> fogQueue_, fogJobEyes_;  // party positions waiting / being relit, in order
  e2::FogMask fogNext_;
  std::vector<uint16_t> fogTexels_, fogNextTexels_;  // fogMask_ / fogNext_ packed for the GPU (Renderer::fogTexels)
  double fogJobMs_ = 0.0;
  std::vector<double> fogDeliverMs_;  // main-thread cost of taking a finished job's result
  uint64_t maskVersion_ = 0;
  std::vector<e2::HexPoint> path_, lastParty_;
  int hero_ = -1;
  glm::vec3 heroAt_{0.0f};
  e2::HexPoint heroHex_;
  float meanGround_ = 0.0f, boardTop_ = 0.0f, groundLow_ = 0.0f;
  uint32_t fogLook_ = 2;  // index into fogLooks(): look 003 unless --fog-look says otherwise
  int lightingMode_ = 3;   // 0 old prototype day, 1 Engine2 presets, 2 physical sky (Engine2 strengths), 3 real (lux + camera)
  // Light mode "real": the camera's exposure, adapted towards the scene's (EV100), and what was metered.
  float adaptedEv_ = std::numeric_limits<float>::quiet_NaN();
  mutable float meterLux_ = 1.0f;
  float exposure_ = 1.0f;  // the exposure in use (last frame's), for the painted flames' glow
  int moonPath_ = 0;       // moonPlacement()
  static constexpr int kStarLooks = 3;  // saved star looks (looks/stars/README.md)
  int starLook_ = 3;
  int moonPhase_ = -1;  // index in e2::kMoonPhases; -1 = Engine2's default (the full moon's art, the night's strength)
  // The disc's angular radius (degrees), chosen in the panel. 2.7 = Engine2's disc as seen from its normal view; the
  // user found it "millions of km away" (2026-10-15), so the panel starts at the large one.
  static constexpr float kMoonSizes[4] = {2.7f, 6.0f, 10.0f, 15.0f};
  int moonSize_ = 2;
  std::array<int, 3> fogCounts_{};
  int holes_ = 0, holeHexes_ = 0, worstHoles_ = 0;
  std::vector<std::pair<int, int>> lastHoles_;
  bool holesLogged_ = false;
  uint32_t hiddenUnits_ = 0;
  bool noFog_ = false;  // "sem névoa" in the panel: no mist and no fog of war (sight still runs underneath)
  bool moonOff_ = false, starsOff_ = false;  // "sem lua no céu" / "sem estrelas" in the panel
  double lastFogMs_ = 0.0;
  std::vector<double> fogUpdateMs_;
  std::map<std::string, std::pair<double, size_t>> passWorst_;
};

}  // namespace ember
