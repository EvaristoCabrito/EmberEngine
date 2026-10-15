#pragma once

// A real Ember map from Engine2 (C:/Engine2/maps): its board and ground in both looks, Engine2's camera.
// D1 port, step by step (docs/ENGINE2_AUDIT.md): the ground first; water, decorations, units, lights,
// fog of war and spells follow. Engine2 stays read-only: maps and relief grids are read from it, the
// textures come from the cooked ground pack (Cooker ground).
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

#include "Stage.h"
#include "core/Log.h"
#include "render/BoardGround.h"
#include "render/Renderer.h"
#include "rhi/UploadQueue.h"
#include "world/Board.h"

namespace ember {

struct MapStageOptions {
  std::filesystem::path map;
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
      : uploads_(context), ground_(context, renderer.bindless(), assets, uploads_), look_(options.look), options_(options) {
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
  }

  void frame(Renderer& renderer, VkExtent2D size) override {
    ground_.update();
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
    renderer.setScene(camera, {});
    Renderer::Lighting lighting;
    if (options_.sunElevation >= 0.0f) {
      const float az = glm::radians(options_.sunAzimuth), el = glm::radians(options_.sunElevation);
      lighting.sunDirection = {std::cos(el) * std::cos(az), std::sin(el), std::cos(el) * std::sin(az)};
    }
    renderer.setMeshes({}, lighting);
    renderer.setTerrain(ground_.view(look_));
    renderer.setSunShadowBounds(boardCenter_, boardRadius_);
    renderer.setLightingLayers(layers_);
    renderer.waitFor(uploads_.timeline(), ground_.uploadWaitValue());
    if (ground_.ready()) ++readyFrames_;
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
    ImGui::End();
  }

  bool done() const override { return failed_ || readyFrames_ >= 8; }
  bool passed() const override { return !failed_ && ground_.ready(); }

 private:
  static constexpr float kFov = 30.0f, kNear = 0.5f;  // Engine2's normal lens

  UploadQueue uploads_;
  BoardGround ground_;
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
