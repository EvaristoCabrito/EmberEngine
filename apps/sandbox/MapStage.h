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
#include "core/Jobs.h"
#include "core/Log.h"
#include "render/BoardGround.h"
#include "render/FogLooks.h"
#include "render/Renderer.h"
#include "rhi/UploadQueue.h"
#include "render/MeshScene.h"
#include "render/SpriteSystem.h"
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
  // the map's time of day (the floor), 2 physical: the map's time of day and sliders through the atmosphere.
  int lighting = 2;
  // Scene clock pinned to this many seconds (< 0 = real time): identical frames for paired captures.
  float sceneTime = -1.0f;
  // Night: tonight's moon phase (Engine2's moonPhase.ts names; "" = Engine2's stage default: the full moon's art at
  // the night's own strength) and where the moon's disc and light stand (see moonPlacement()); also in the panel.
  std::string moonPhase;
  int moonPath = 0;
  // The six heroes in a row in front of the camera, every pose chosen in the panel (tools/scripts/engine2/
  // export_heroes.ts output; empty = none).
  std::filesystem::path heroes;
  int starLook = 3;  // the star field's saved look (looks/stars/README.md; also in the panel)
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
    lightingMode_ = std::clamp(options.lighting, 0, 2);
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
      setUpParty();
      if (!options.heroes.empty()) setUpHeroes(options.heroes);
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
      if (!heroes_.empty()) distance_ = 48.0f / options_.zoom;  // the rows of units fill the view
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
    orbitEye_ = target_ + back;
    // The free camera (user, 2026-10-15: "move the camera at will"): once the user moves it, it flies from where the
    // framing was; captures and measurements keep the framing above.
    const float dt = std::min(std::chrono::duration<float>(std::chrono::steady_clock::now() - lastFrame_).count(), 0.1f);
    lastFrame_ = std::chrono::steady_clock::now();
    lastDt_ = dt;
    if (!measuring_) flyKeys(dt);
    if (!captured_) captured_ = true, homeYaw_ = yaw_, homePitch_ = pitch_, homeTarget_ = target_, homeDistance_ = distance_;
    const Camera camera = free_ && !measuring_ ? Camera::lookAt(eye_, eye_ + lookDirection(), glm::radians(kFov), aspect, kNear)
                                               : Camera::lookAt(orbitEye_, target_, glm::radians(kFov), aspect, kNear);
    drawList_.clear();
    if (scene_ && sprites_.ready()) buildCards(camera, time);
    renderer.setScene(camera, drawList_);
    lights_.clear();
    if (scene_) buildLights(time);
    renderer.setLights(lights_);
    Renderer::Lighting lighting = mapLighting(time);
    if (options_.sunElevation >= 0.0f) {
      const float az = glm::radians(options_.sunAzimuth), el = glm::radians(options_.sunElevation);
      lighting.sunDirection = {std::cos(el) * std::cos(az), std::sin(el), std::cos(el) * std::sin(az)};
    }
    renderer.setMeshes(scene_ ? meshes_.view() : MeshSceneView{}, lighting);
    renderer.setTerrain(ground_.view(look_));
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
    volume.enabled = options_.volume && lightingMode_ == 0;
    volume.meanGround = meanGround_;
    // The froxel fog is the air's haze in sight; the fog of war's bank is drawn by the fog bank pass.
    volume.boundaryDensity = 0.0f;
    renderer.setVolumetricFog(volume);
    // The fog of war's mist: the numbered look on show (render/FogLooks.h; chosen in the Map panel).
    Renderer::FogBank bank;
    bank.enabled = fog_.has_value() && options_.volume && !noFog_;
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

  // The free camera: drag with any mouse button to look anywhere (the sky too), the wheel flies forward and back.
  // Clicks on the panel never reach here (ImGui takes them).
  void event(const SDL_Event& e) override {
    if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN) looking_ = true;
    if (e.type == SDL_EVENT_MOUSE_BUTTON_UP) looking_ = false;
    if (e.type == SDL_EVENT_MOUSE_MOTION && looking_ && (e.motion.state & (SDL_BUTTON_LMASK | SDL_BUTTON_RMASK | SDL_BUTTON_MMASK))) {
      beginFree();
      lookYaw_ -= e.motion.xrel * 0.004f;
      lookPitch_ = std::clamp(lookPitch_ - e.motion.yrel * 0.004f, glm::radians(-89.0f), glm::radians(89.0f));
    }
    if (e.type == SDL_EVENT_MOUSE_WHEEL) {
      beginFree();
      eye_ += lookDirection() * e.wheel.y * flySpeed() * 0.25f;
    }
  }

  // Keys held move the free camera smoothly every frame: W A S D / arrows along the view, Q down, E up, Shift faster.
  void flyKeys(float dt) {
    if (ImGui::GetCurrentContext() && ImGui::GetIO().WantTextInput) return;  // only typing into a text field takes the keys
    const bool* k = SDL_GetKeyboardState(nullptr);
    if (!k) return;
    // Level moves (a game's camera): the arrows / WASD walk across the board, Q and E change the height.
    const glm::vec3 forward{-std::sin(lookYaw_), 0.0f, -std::cos(lookYaw_)}, right{std::cos(lookYaw_), 0.0f, -std::sin(lookYaw_)};
    glm::vec3 move{0.0f};
    if (k[SDL_SCANCODE_W] || k[SDL_SCANCODE_UP]) move += forward;
    if (k[SDL_SCANCODE_S] || k[SDL_SCANCODE_DOWN]) move -= forward;
    if (k[SDL_SCANCODE_D] || k[SDL_SCANCODE_RIGHT]) move += right;
    if (k[SDL_SCANCODE_A] || k[SDL_SCANCODE_LEFT]) move -= right;
    if (k[SDL_SCANCODE_E]) move.y += 1.0f;
    if (k[SDL_SCANCODE_Q]) move.y -= 1.0f;
    if (glm::length(move) < 1e-4f) return;
    beginFree();
    const float fast = k[SDL_SCANCODE_LSHIFT] || k[SDL_SCANCODE_RSHIFT] ? 3.0f : 1.0f;
    eye_ += glm::normalize(move) * flySpeed() * fast * dt;
  }

  // The first move starts the free camera exactly where the framing camera was, looking the same way.
  void beginFree() {
    if (free_) return;
    free_ = true;
    eye_ = orbitEye_;
    lookYaw_ = yaw_;
    lookPitch_ = -pitch_;
  }
  glm::vec3 lookDirection() const {
    return {-std::cos(lookPitch_) * std::sin(lookYaw_), std::sin(lookPitch_), -std::cos(lookPitch_) * std::cos(lookYaw_)};
  }
  float flySpeed() const { return std::max(8.0f, std::max(boardWidth_, boardDepth_) * 0.35f); }  // units per second

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
  // The camera's on-screen controls (top right): hold a button and the camera keeps moving; Voltar returns to the
  // first view.
  void drawCameraButtons() {
    ImGui::SetNextWindowPos({ImGui::GetIO().DisplaySize.x - 12.0f, 12.0f}, ImGuiCond_Always, {1.0f, 0.0f});
    ImGui::SetNextWindowBgAlpha(0.9f);
    ImGui::Begin("Câmera", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoMove);
    ImGui::PushFont(nullptr, 24.0f);
    const ImVec2 b{120.0f, 52.0f};
    const float dt = lastDt_, move = flySpeed() * dt, turn = 1.4f * dt;
    const glm::vec3 fwd{-std::sin(lookYaw_), 0.0f, -std::cos(lookYaw_)}, right{std::cos(lookYaw_), 0.0f, -std::sin(lookYaw_)};
    const auto hold = [&](const char* label, auto action) {
      ImGui::Button(label, b);
      if (ImGui::IsItemActive()) {
        beginFree();
        action();
      }
    };
    // Row 1: turn left, forward, turn right, up
    hold("girar ←##tl", [&] { lookYaw_ += turn; });
    ImGui::SameLine();
    hold("\u2191##f", [&] { eye_ += fwd * move; });
    ImGui::SameLine();
    hold("girar →##tr", [&] { lookYaw_ -= turn; });
    ImGui::SameLine();
    hold("+##in", [&] { eye_ += lookDirection() * move; });
    // Row 2: left, back, right, out
    hold("\u2190##l", [&] { eye_ -= right * move; });
    ImGui::SameLine();
    hold("\u2193##b", [&] { eye_ -= fwd * move; });
    ImGui::SameLine();
    hold("\u2192##r", [&] { eye_ += right * move; });
    ImGui::SameLine();
    hold("-##out", [&] { eye_ -= lookDirection() * move; });
    // Row 3: look up, look down, rise, sink
    hold("olhar \u2191##lu", [&] { lookPitch_ = std::min(lookPitch_ + turn, glm::radians(89.0f)); });
    ImGui::SameLine();
    hold("olhar \u2193##ld", [&] { lookPitch_ = std::max(lookPitch_ - turn, glm::radians(-89.0f)); });
    hold("subir##up", [&] { eye_.y += move; });
    ImGui::SameLine();
    hold("descer##dn", [&] { eye_.y -= move; });
    if (ImGui::Button("Voltar", {b.x * 4.0f + ImGui::GetStyle().ItemSpacing.x * 3.0f, b.y})) {
      free_ = false;
      yaw_ = homeYaw_, pitch_ = homePitch_, target_ = homeTarget_, distance_ = homeDistance_;
    }
    ImGui::PopFont();
    ImGui::End();
  }

  void drawPanel() override {
    drawCameraButtons();
    if (!board_) return;
    ImGui::SetNextWindowPos({12, 12}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowBgAlpha(0.9f);
    ImGui::Begin("Escolhas", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing);
    ImGui::PushFont(nullptr, 24.0f);  // large (ImGui 1.93: size per push)
    ImGui::PushItemWidth(220.0f);
    if (!heroPoses_.empty())
      dropdown("Animação", heroPose_, static_cast<int>(heroPoses_.size()), [this](int i) { return heroPoses_[static_cast<size_t>(i)].second; });
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
      static const char* kModes[3] = {"protótipo", "Engine2", "física"};
      dropdown("Luz", lightingMode_, 3, [](int i) { return std::string(kModes[i]); });
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
      if (fog_ && !noFog_ && u.side != "player") {
        const auto [c, r] = e2::hexAt(board_->layout(), u.x, u.z);
        // The states the shown mask was painted from (all hidden until the first mask arrives).
        const size_t i = static_cast<size_t>(e2::indexOf(board_->layout(), c, r));
        if (i >= fogStates_.size() || fogStates_[i] != e2::kFogVisible) {
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
    // The heroes in their row, each playing the pose chosen in the panel (its idle when it has none). One-shot
    // poses (attacks, casts) play through, hold half a second, and start again.
    for (size_t hi = 0; hi < heroes_.size(); ++hi) {
      const e2::Hero& h = heroes_[hi];
      const e2::HeroPose* p = h.find(heroPoses_.empty() ? std::string("idle") : heroPoses_[static_cast<size_t>(heroPose_)].first);
      if (!p) p = h.find("idle");
      if (!p || p->pace.empty()) continue;
      const SpriteClipId clip = sprites_.find(p->set, p->clip);
      if (clip == kNoSpriteClip) {
        ++missing_;
        continue;
      }
      const size_t period = p->doneAt >= 0 ? static_cast<size_t>(p->doneAt) + 120 : pacePeriod(p->pace);
      const size_t tick = static_cast<size_t>(time * 240.0f) % period;
      const uint32_t frame = static_cast<uint32_t>(p->pace[std::min(tick, p->pace.size() - 1)]);
      addCard(clip, heroAnchors_[hi], right, p->boxW, p->boxH, {0.0f, -p->foot, 0.0f}, h.reversed, frame,
              kSpriteLit | kSpriteFront | kSpriteCastsShadow, nullptr);
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

  // Where the party stands (the swirl turns round it): the middle of the living player units.
  glm::vec2 partyCenter() const {
    if (!scene_) return {boardCenter_.x, boardCenter_.z};
    glm::vec2 sum{0.0f};
    int n = 0;
    for (size_t i = 0; i < scene_->units.size(); ++i) {
      const e2::SceneUnit& u = scene_->units[i];
      if (u.side != "player") continue;
      sum += static_cast<int>(i) == hero_ ? glm::vec2(heroAt_.x, heroAt_.z) : glm::vec2(u.x, u.z);
      ++n;
    }
    return n ? sum / static_cast<float>(n) : glm::vec2(boardCenter_.x, boardCenter_.z);
  }

  // The six heroes in a row across the view, 3 units apart, at the board's centre on the ground; the camera looks at
  // them. The panel's poses: every pose any hero has, in Engine2's order of first appearance, with its label.
  void setUpHeroes(const std::filesystem::path& path) {
    std::optional<std::vector<e2::Hero>> heroes = e2::loadHeroes(path);
    if (!heroes) return;
    heroes_ = std::move(*heroes);
    for (const e2::Hero& h : heroes_)
      for (const e2::HeroPose& p : h.poses)
        if (std::none_of(heroPoses_.begin(), heroPoses_.end(), [&](const auto& q) { return q.first == p.pose; }))
          heroPoses_.emplace_back(p.pose, p.label);
    // Rows of 12 across the view, each unit given room for its own width, rows 5 units apart going away from the
    // camera; the block centred on the board.
    const glm::vec3 right{std::cos(yaw_), 0.0f, -std::sin(yaw_)}, away{-std::sin(yaw_), 0.0f, -std::cos(yaw_)};
    const glm::vec3 centre{boardCenter_.x, 0.0f, boardCenter_.z};
    constexpr size_t kPerRow = 12;
    const size_t rows = (heroes_.size() + kPerRow - 1) / kPerRow;
    for (size_t row = 0; row < rows; ++row) {
      const size_t first = row * kPerRow, last = std::min(heroes_.size(), first + kPerRow);
      std::vector<float> widths;
      float total = 0.0f;
      for (size_t i = first; i < last; ++i) {
        const e2::HeroPose* idle = heroes_[i].find("idle");
        widths.push_back(std::max(2.6f, (idle ? idle->boxW : heroes_[i].poses.front().boxW) * 0.8f));
        total += widths.back();
      }
      float x = -0.5f * total;
      for (size_t i = first; i < last; ++i) {
        x += 0.5f * widths[i - first];
        glm::vec3 at = centre + right * x + away * ((static_cast<float>(row) - 0.5f * static_cast<float>(rows - 1)) * 5.0f);
        at.y = static_cast<float>(e2::groundHeightAt(*board_, look_, at.x, at.z));
        heroAnchors_.push_back(at);
        x += 0.5f * widths[i - first];
      }
    }
    target_ = centre;
    target_.y = heroAnchors_.empty() ? target_.y : heroAnchors_[heroAnchors_.size() / 2].y + 1.6f;
    pitch_ = glm::radians(12.0f);
    logInfo("heroes: {} with {} poses between them", heroes_.size(), heroPoses_.size());
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
  std::vector<e2::Hero> heroes_;  // the hero row (setUpHeroes)
  std::vector<glm::vec3> heroAnchors_;
  std::vector<std::pair<std::string, std::string>> heroPoses_;  // pose id, label
  int heroPose_ = 0;
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
  // The free camera (event / flyKeys / beginFree).
  bool free_ = false, looking_ = false;
  glm::vec3 eye_{0.0f}, orbitEye_{0.0f};
  float lookYaw_ = 0.0f, lookPitch_ = 0.0f;
  float lastDt_ = 0.0f;  // the last frame's time step (the camera buttons move by it)
  bool captured_ = false;  // the first view is remembered for Voltar
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
  int lightingMode_ = 2;   // 0 old prototype day, 1 Engine2 presets, 2 physical
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
