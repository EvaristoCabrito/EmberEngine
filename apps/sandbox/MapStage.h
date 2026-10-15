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

#include <SDL3/SDL.h>
#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <memory>
#include <optional>
#include <chrono>
#include <numbers>
#include <unordered_map>

#include "Stage.h"
#include "core/Log.h"
#include "render/BoardGround.h"
#include "render/Renderer.h"
#include "rhi/UploadQueue.h"
#include "render/MeshScene.h"
#include "render/SpriteSystem.h"
#include "world/Board.h"
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
};

class MapStage final : public Stage {
 public:
  MapStage(VulkanContext& context, Renderer& renderer, AssetManager& assets, const MapStageOptions& options)
      : uploads_(context),
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
    boardWidth_ = static_cast<float>(b.x1 - b.x0);
    boardDepth_ = static_cast<float>(b.z1 - b.z0);
    // The sun's shadow covers the board's bounding sphere (ground from y = 0 to its highest cell).
    double top = 0;
    for (const e2::Cell& c : board_->cells()) top = std::max(top, c.groundY);
    boardCenter_ = {target_.x, static_cast<float>(top * 0.5), target_.z};
    boardRadius_ = 0.5f * std::sqrt(boardWidth_ * boardWidth_ + boardDepth_ * boardDepth_ + static_cast<float>(top * top)) + 1.0f;
    target_.x += options.offset.x * boardWidth_;
    target_.z += options.offset.y * boardDepth_;

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
    const float aspect = static_cast<float>(size.width) / static_cast<float>(size.height);
    if (distance_ <= 0.0f) {
      const float tanHalf = std::tan(glm::radians(kFov * 0.5f));
      maxDistance_ = std::max(40.0f, std::max(boardWidth_, boardDepth_) * 2.2f);
      distance_ = std::min(maxDistance_, std::max(boardWidth_ / (2 * tanHalf * aspect), boardDepth_ * 1.35f) * 1.05f) / options_.zoom;
    }
    // Engine2: orientation from turn then tilt; the camera sits `distance` behind the target.
    const glm::vec3 back{distance_ * std::cos(pitch_) * std::sin(yaw_), distance_ * std::sin(pitch_),
                         distance_ * std::cos(pitch_) * std::cos(yaw_)};
    const Camera camera = Camera::lookAt(target_ + back, target_, glm::radians(kFov), aspect, kNear);
    const float time = std::chrono::duration<float>(std::chrono::steady_clock::now() - start_).count();
    drawList_.clear();
    if (scene_ && sprites_.ready()) buildCards(camera, time);
    renderer.setScene(camera, drawList_);
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
    renderer.waitFor(uploads_.timeline(),
                     std::max({ground_.uploadWaitValue(), sprites_.uploadWaitValue(), meshes_.uploadWaitValue()}));
    if (ground_.ready() && sceneReady()) ++readyFrames_;
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
    ImGui::Text("lighting: sun %s (F5), shadows %s (F6), indirect %s (F7)", layers_ & 1u ? "on" : "off", layers_ & 2u ? "on" : "off",
                layers_ & 4u ? "on" : "off");
    if (scene_) {
      const SpriteSystem::Stats ss = sprites_.stats();
      ImGui::Text("scene: %zu cards, %zu units drawn (%u of %zu missing art), %u models; sprites %.0f MB resident",
                  scene_->cards.size(), scene_->units.size(), missing_, scene_->cards.size() + scene_->units.size(), modelCount_,
                  ss.residentBytes / 1048576.0);
    }
    ImGui::End();
  }

  bool done() const override { return failed_ || readyFrames_ >= 30; }
  bool passed() const override { return !failed_ && ground_.ready(); }

 private:
  static constexpr float kFov = 30.0f, kNear = 0.5f;  // Engine2's normal lens
  static constexpr float kFlameEmit = 2.5f;           // Engine2 FLAME_EMIT

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
    for (const e2::SceneUnit& u : scene_->units) {
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
      const glm::vec3 anchor{u.x, groundOf(u.groundSmooth, u.groundHard), u.z};
      addCard(clip, anchor, right, u.boxW, u.boxH, {0.0f, -u.foot, 0.0f}, u.mirror, frame, kSpriteLit | kSpriteFront | kSpriteCastsShadow, nullptr);
    }
    const glm::vec3 eye = camera.position, fwd = camera.forward;
    std::sort(drawList_.begin(), drawList_.end(),
              [&](const SpriteInstance& a, const SpriteInstance& b) { return glm::dot(a.origin - eye, fwd) > glm::dot(b.origin - eye, fwd); });
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

  UploadQueue uploads_;
  BoardGround ground_;
  SpriteSystem sprites_;
  MeshScene meshes_;
  AssetManager& assets_;
  std::optional<e2::SceneData> scene_;
  std::unordered_map<std::string, MeshId> modelMeshes_;
  std::unordered_map<const std::vector<int>*, size_t> periods_;
  std::vector<SpriteInstance> drawList_;
  bool modelsPlaced_ = false;
  e2::EdgeStyle placedLook_ = e2::EdgeStyle::Smooth;
  uint32_t modelCount_ = 0, missing_ = 0, fullFrames_ = 0;
  std::chrono::steady_clock::time_point start_ = std::chrono::steady_clock::now();
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
};

}  // namespace ember
