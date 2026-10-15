#pragma once

// A real Ember map from Engine2 (C:/Engine2/maps): its board and ground in both looks, its water, and its
// scene as Engine2 places it (decoration cards, 3D models, units from the spawns; imported by
// tools/scripts/engine2/export_scene.ts + measure_art.py), with Engine2's camera. D1 port, step by step
// (docs/ENGINE2_AUDIT.md). Engine2 stays read-only: maps and relief grids are read from it; textures,
// sprites and meshes come from the cooked packs (Cooker ground / sprites / meshes).
//
// Camera, as Engine2's CameraRig: opens on the normal view (turned -30 deg, tilted 20 deg, 30 deg lens),
// framing the whole board. In a window: right-drag turns and tilts, the wheel zooms, WASD/arrows pan,
// 1 / 2 switch between the smooth and hard looks (Engine2's keys).
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
#include <format>
#include <string_view>

#include "Stage.h"
#include "core/Log.h"
#include "render/BoardGround.h"
#include "render/Renderer.h"
#include "rhi/UploadQueue.h"
#include "render/MeshScene.h"
#include "render/SpriteSystem.h"
#include "world/Board.h"
#include "world/FogOfWar.h"
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
  // Scene clock pinned to this many seconds (< 0 = real time): identical frames for paired captures.
  float sceneTime = -1.0f;
};

class MapStage final : public Stage {
 public:
  MapStage(VulkanContext& context, Renderer& renderer, AssetManager& assets, const MapStageOptions& options)
      : renderer_(renderer),
        uploads_(context),
        ground_(context, renderer.bindless(), assets, uploads_),
        sprites_(context, renderer.bindless(), assets, uploads_, {options.spriteBudgetMb << 20}),
        meshes_(context, renderer.bindless(), assets, uploads_, {.geometryBytes = 256ull << 20}),
        assets_(assets),
        look_(options.look),
        options_(options) {
    yaw_ += glm::radians(options.turnDegrees);
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
    double top = 0;
    for (const e2::Cell& c : board_->cells()) top = std::max(top, c.groundY);
    boardTop_ = static_cast<float>(top);
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
      setUpParty();
    }
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
    // Engine2: orientation from turn then tilt; the camera sits `distance` behind the target.
    const glm::vec3 back{distance * std::cos(pitch_) * std::sin(yaw), distance * std::sin(pitch_), distance * std::cos(pitch_) * std::cos(yaw)};
    const Camera camera = Camera::lookAt(target_ + back, target_, glm::radians(kFov), aspect, kNear);
    drawList_.clear();
    if (scene_ && sprites_.ready()) buildCards(camera, time);
    renderer.setScene(camera, drawList_);
    lights_.clear();
    if (scene_) buildLights(time);
    renderer.setLights(lights_);
    Renderer::Lighting lighting;
    if (options_.sunElevation >= 0.0f) {
      const float az = glm::radians(options_.sunAzimuth), el = glm::radians(options_.sunElevation);
      lighting.sunDirection = {std::cos(el) * std::cos(az), std::sin(el), std::cos(el) * std::sin(az)};
    }
    renderer.setMeshes(scene_ ? meshes_.view() : MeshSceneView{}, lighting);
    renderer.setTerrain(ground_.view(look_));
    renderer.setWater(ground_.water(look_));
    renderer.setSunShadowBounds(boardCenter_, boardRadius_);
    renderer.setLightingLayers(layers_);
    renderer.setFogOfWar(fogReady_ ? &fogMask_ : nullptr, maskVersion_);
    Renderer::VolumetricFog volume;
    volume.enabled = options_.volume;
    volume.meanGround = meanGround_;
    // The froxel fog is the air's haze in sight; the fog of war's bank is drawn by the fog bank pass.
    volume.boundaryDensity = 0.0f;
    renderer.setVolumetricFog(volume);
    Renderer::FogBank bank;
    bank.enabled = fog_.has_value() && options_.volume;
    bank.boardCenter = {boardCenter_.x, boardCenter_.z};
    bank.baseY = meanGround_ - 1.0f;
    bank.topY = boardTop_ + 3.0f;
    bank.fadeRadius = boardRadius_ * 2.2f;
    renderer.setFogBank(bank);
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

  void event(const SDL_Event& e) override {
    if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_RIGHT) turning_ = true;
    if (e.type == SDL_EVENT_MOUSE_BUTTON_UP && e.button.button == SDL_BUTTON_RIGHT) turning_ = false;
    if (e.type == SDL_EVENT_MOUSE_MOTION && turning_) {
      yaw_ -= e.motion.xrel * 0.005f;
      pitch_ = std::clamp(pitch_ + e.motion.yrel * 0.005f, glm::radians(2.0f), glm::radians(89.0f));
    }
    if (e.type == SDL_EVENT_MOUSE_WHEEL) distance_ = std::clamp(distance_ * std::pow(0.9f, e.wheel.y), 3.0f, maxDistance_);
    if (e.type == SDL_EVENT_KEY_DOWN) {
      const float step = distance_ * 0.05f;
      const glm::vec3 forward{-std::sin(yaw_), 0.0f, -std::cos(yaw_)}, right{std::cos(yaw_), 0.0f, -std::sin(yaw_)};
      switch (e.key.key) {
        case SDLK_W: case SDLK_UP: target_ += forward * step; break;
        case SDLK_S: case SDLK_DOWN: target_ -= forward * step; break;
        case SDLK_A: case SDLK_LEFT: target_ -= right * step; break;
        case SDLK_D: case SDLK_RIGHT: target_ += right * step; break;
        case SDLK_1: look_ = e2::EdgeStyle::Smooth; break;
        case SDLK_2: look_ = e2::EdgeStyle::Hard; break;
        case SDLK_HOME: yaw_ = glm::radians(-30.0f), pitch_ = glm::radians(20.0f); break;
        // Lighting layers on and off, to see and measure each alone (lighting.hlsli kLayer*).
        case SDLK_F5: layers_ ^= 1u; break;  // sun
        case SDLK_F6: layers_ ^= 2u; break;  // shadows
        case SDLK_F7: layers_ ^= 4u; break;  // indirect
        case SDLK_F8: layers_ ^= 8u; break;  // local lights
        default: break;
      }
    }
  }

  void drawPanel() override {
    if (!board_) return;
    const BoardGround::Stats s = ground_.stats();
    ImGui::SetNextWindowPos({12, 420}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowBgAlpha(0.82f);
    ImGui::Begin("Map", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing);
    ImGui::Text("%s  %dx%d hexes  look: %s (keys 1/2)", board_->draft().id.c_str(), board_->layout().cols, board_->layout().rows,
                look_ == e2::EdgeStyle::Smooth ? "smooth" : "hard");
    ImGui::Text("ground: %u triangles, %u baked layers, textures %u / %u (%.1f MB), geometry %.1f MB",
                s.triangles[look_ == e2::EdgeStyle::Smooth ? 0 : 1], s.layers, s.texturesReady, s.textures, s.textureBytes / 1048576.0,
                s.geometryBytes / 1048576.0);
    ImGui::Text("camera: turn %.0f, tilt %.0f, distance %.1f", glm::degrees(yaw_), glm::degrees(pitch_), distance_);
    ImGui::Text("lighting: sun %s (F5), shadows %s (F6), indirect %s (F7), local lights %s (F8): %zu lights", layers_ & 1u ? "on" : "off",
                layers_ & 2u ? "on" : "off", layers_ & 4u ? "on" : "off", layers_ & 8u ? "on" : "off", lights_.size());
    if (scene_) {
      const SpriteSystem::Stats ss = sprites_.stats();
      ImGui::Text("scene: %zu cards, %zu units drawn (%u of %zu missing art), %u models; sprites %.0f MB resident",
                  scene_->cards.size(), scene_->units.size(), missing_, scene_->cards.size() + scene_->units.size(), modelCount_,
                  ss.residentBytes / 1048576.0);
    }
    if (fog_)
      ImGui::Text("fog of war: %d in sight, %d remembered, %d never seen; %u enemies hidden; last update %.2f ms", fogCounts_[2],
                  fogCounts_[1], fogCounts_[0], hiddenUnits_, lastFogMs_);
    ImGui::End();
  }

  bool done() const override { return failed_ || (options_.measureSeconds > 0.0f ? measured_ : readyFrames_ >= 30); }
  bool passed() const override { return !failed_ && ground_.ready() && measurePassed_; }

 private:
  static constexpr float kFov = 30.0f, kNear = 0.5f;  // Engine2's normal lens
  static constexpr float kFlameEmit = 2.5f;           // Engine2 FLAME_EMIT

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
      logInfo("  fog of war: {} mask updates (sight + mask on the main thread): median {:.2f}, worst {:.2f} ms", fogUpdateMs_.size(), fu.median,
              fu.worst);
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
    const glm::vec3 facing{-right.z, 0.0f, right.x};
    SpriteInstance s{};
    s.origin = anchor + right * x + glm::vec3(0, local.y + cy * h, 0) + facing * local.z;
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
      const float yaw = d.lockedYaw.value_or(0.0f);
      const glm::vec3 right{std::cos(yaw), 0.0f, -std::sin(yaw)};
      const glm::vec3 anchor{d.x, groundOf(d.groundSmooth, d.groundHard), d.z};
      const glm::vec3 local{(0.5f - d.art.baseU) * d.w, -d.art.baseV * d.h, d.nudge};
      SpriteInstance emit{};
      if (d.light) {
        emit.emitColor = {d.light->color[0], d.light->color[1], d.light->color[2]};
        emit.emitLevel = kFlameEmit * flicker(time, d.light->seed, d.light->flicker);
        emit.emitAt = {d.art.flameFound ? d.art.flameU : 0.5f, d.art.flameFound ? d.art.flameV : 0.7f, 0.0f};
      }
      addCard(clip, anchor, right, d.w, d.h, local, d.mirror, 0, kSpriteLit | (d.castShadow ? kSpriteCastsShadow : 0u), d.light ? &emit : nullptr);
    }
    // Units: Engine2's UnitActor idle. They turn to face the camera; Engine2's box for the original frame,
    // its foot line on the ground, the frame paced by Engine2's framePace.
    glm::vec3 right = camera.right;
    right.y = 0.0f;
    right = glm::length(right) > 0.0f ? glm::normalize(right) : glm::vec3(1, 0, 0);
    hiddenUnits_ = 0;
    for (size_t ui = 0; ui < scene_->units.size(); ++ui) {
      const e2::SceneUnit& u = scene_->units[ui];
      // Fog of war: a unit not in the party's sight is not drawn at all (and casts no shadow).
      if (fog_ && u.side != "player") {
        const auto [c, r] = e2::hexAt(board_->layout(), u.x, u.z);
        if (!fog_->visible(c, r)) {
          ++hiddenUnits_;
          continue;
        }
      }
      const SpriteClipId clip = sprites_.find(u.set, u.clip);
      if (clip == kNoSpriteClip) {
        ++missing_;
        continue;
      }
      uint32_t frame = 0;
      if (!u.pace.empty()) {
        const size_t period = pacePeriod(u.pace);
        frame = static_cast<uint32_t>(u.pace[static_cast<size_t>(time * 240.0f) % period]);
      }
      const glm::vec3 anchor = static_cast<int>(ui) == hero_ ? heroAt_ : glm::vec3{u.x, groundOf(u.groundSmooth, u.groundHard), u.z};
      addCard(clip, anchor, right, u.boxW, u.boxH, {0.0f, -u.foot, 0.0f}, u.mirror, frame, kSpriteLit | kSpriteFront | kSpriteCastsShadow, nullptr);
    }
    const glm::vec3 eye = camera.position, fwd = camera.forward;
    std::sort(drawList_.begin(), drawList_.end(),
              [&](const SpriteInstance& a, const SpriteInstance& b) { return glm::dot(a.origin - eye, fwd) > glm::dot(b.origin - eye, fwd); });
  }

  // Engine2's CarriedLight: three.js point light, decay 1.25, cut off at hypot(radius, height) x 2.5, colour x
  // intensity x 3.2 x flicker.
  static LocalLight carried(glm::vec3 at, const std::array<float, 3>& color, float intensity, float radius, float height, float flick) {
    LocalLight l;
    l.position = at;
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
  // kBlockHeight tall along their width. (Walls and doors are not imported yet: they block nothing.)
  void addOccluders() {
    for (const e2::SceneModel& m : scene_->models) {
      std::string asset = m.file.substr(std::string("/game/").size());
      asset = "meshes/" + asset.substr(0, asset.size() - 4) + ".mesh";
      const MeshHeader* h = meshes_.header(modelMeshes_.at(asset));
      if (!h) continue;
      const float k = m.height / std::max(h->boundsMax[1] - h->boundsMin[1], 1e-4f);
      const float hx = (h->boundsMax[0] - h->boundsMin[0]) * 0.5f * k, hz = (h->boundsMax[2] - h->boundsMin[2]) * 0.5f * k;
      const float c = std::cos(m.rotY), sn = std::sin(m.rotY);
      // The footprint (rotated box, centred on the anchor as placeModels centres it), sampled every 0.25.
      for (float a = -hx; a <= hx; a += 0.25f)
        for (float b = -hz; b <= hz; b += 0.25f) fog_->block(m.x + c * a + sn * b, m.z - sn * a + c * b, m.height);
      fog_->block(m.x, m.z, m.height);
    }
    for (const e2::SceneCard& d : scene_->cards) {
      if (d.onFloor || d.h < e2::kBlockHeight) continue;
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
    const auto started = std::chrono::steady_clock::now();
    bool changed = false;
    if (!fogReady_) {
      addOccluders();
      fogReady_ = true;
      changed = true;
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
    // The party's eyes: each living player unit's hex (the scout's where it is now).
    std::vector<e2::HexPoint> party;
    for (size_t i = 0; i < scene_->units.size(); ++i) {
      const e2::SceneUnit& u = scene_->units[i];
      if (u.side != "player") continue;
      if (static_cast<int>(i) == hero_ && path_.size() > 1) {
        party.push_back(heroHex_);
        continue;
      }
      const auto [c, r] = e2::hexAt(board_->layout(), u.x, u.z);
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
          relight(eyes);
        }
      }
      relight(party);
      lastParty_ = party;
      fogMask_ = fog_->paint(*board_, party);
      ++maskVersion_;
      fogCounts_ = {0, 0, 0};
      for (int r = 0; r < board_->layout().rows; ++r)
        for (int c = 0; c < board_->layout().cols; ++c) ++fogCounts_[fog_->state(c, r)];
      lastFogMs_ = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
      fogUpdatedThisFrame_ = true;
    }
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
  bool turning_ = false;
  glm::vec3 boardCenter_{0.0f};
  float boardRadius_ = 0.0f;
  uint32_t layers_ = 0xFFFFFFFFu;
  // Fog of war and the party.
  std::optional<e2::FogOfWar> fog_;
  e2::FogMask fogMask_;
  bool fogReady_ = false, replayed_ = false, fogUpdatedThisFrame_ = false;
  uint64_t maskVersion_ = 0;
  std::vector<e2::HexPoint> path_, lastParty_;
  int hero_ = -1;
  glm::vec3 heroAt_{0.0f};
  e2::HexPoint heroHex_;
  float meanGround_ = 0.0f, boardTop_ = 0.0f;
  std::array<int, 3> fogCounts_{};
  uint32_t hiddenUnits_ = 0;
  double lastFogMs_ = 0.0;
  std::vector<double> fogUpdateMs_;
  std::map<std::string, std::pair<double, size_t>> passWorst_;
};

}  // namespace ember
