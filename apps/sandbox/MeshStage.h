#pragma once

// Mesh scene bring-up: every cooked mesh in the mounted archive, laid out on a grid (grid x grid
// instances, cycling through the meshes), drawn GPU-driven. Off-screen it stops once everything is
// resident and has been drawn for a few frames; in a window the camera orbits.

#include <imgui.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <format>
#include <string>
#include <vector>

#include "Stage.h"
#include "assets/AssetManager.h"
#include "core/Log.h"
#include "render/MeshScene.h"
#include "render/Renderer.h"
#include "rhi/UploadQueue.h"

namespace ember {

class MeshStage final : public Stage {
 public:
  MeshStage(VulkanContext& context, Renderer& renderer, AssetManager& assets, uint32_t grid, bool orbit)
      : renderer_(renderer), uploads_(context), scene_(context, renderer.bindless(), assets, uploads_, {}), grid_(grid), orbit_(orbit) {
    for (const AssetId id : assets.mountedAssets())
      if (assets.name(id).ends_with(".mesh")) meshes_.push_back(scene_.add(id));
    logInfo("mesh stage: {} meshes, {} x {} instances", meshes_.size(), grid, grid);
  }

  void frame(Renderer& renderer, VkExtent2D size) override {
    scene_.update();
    if (!placed_ && std::all_of(meshes_.begin(), meshes_.end(), [&](MeshId m) { return scene_.header(m) != nullptr; })) place();
    uploads_.flush();

    const float t = orbit_ ? std::chrono::duration<float>(std::chrono::steady_clock::now() - start_).count() * 0.1f : 0.6f;
    const glm::vec3 eye = center_ + glm::vec3(std::cos(t) * distance_, distance_ * 0.55f, std::sin(t) * distance_);
    const Camera camera = Camera::lookAt(eye, center_, glm::radians(40.0f), static_cast<float>(size.width) / static_cast<float>(size.height));
    renderer.setScene(camera, {});
    renderer.setMeshes(scene_.view());
    renderer.waitFor(uploads_.timeline(), scene_.uploadWaitValue());
    if (scene_.ready() && scene_.view().instanceCount == instances_) ++readyFrames_;
  }

  void drawPanel() override {
    const MeshScene::Stats s = scene_.stats();
    const Renderer::MeshStats g = renderer_.meshStats();
    ImGui::SetNextWindowPos({12, 420}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowBgAlpha(0.82f);
    ImGui::Begin("Meshes", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing);
    ImGui::Text("meshes %u / %u resident, textures %u / %u, %.1f MB geometry, %.1f MB textures", s.meshesReady, s.meshes,
                s.texturesReady, s.textures, s.geometryBytes / 1048576.0, s.textureBytes / 1048576.0);
    ImGui::Text("instances %u, in view %u, clusters drawn %u", s.instances, g.visibleInstances, g.clusters);
    if (g.queueOverflow || g.clusterOverflow) ImGui::Text("OVERFLOW: queue %u, clusters %u", g.queueOverflow, g.clusterOverflow);
    ImGui::End();
  }

  bool done() const override { return readyFrames_ >= 8; }

 private:
  void place() {
    placed_ = true;
    float spacing = 1.0f;
    for (const MeshId m : meshes_) spacing = std::max(spacing, scene_.header(m)->boundsRadius * 2.2f);
    std::vector<InstanceGpu> instances;
    instances.reserve(size_t{grid_} * grid_);
    for (uint32_t z = 0; z < grid_; ++z)
      for (uint32_t x = 0; x < grid_; ++x) {
        const MeshId m = meshes_[(z * grid_ + x) % meshes_.size()];
        const MeshHeader& h = *scene_.header(m);
        InstanceGpu inst{};
        const glm::vec3 p{x * spacing, -h.boundsMin[1], z * spacing};  // standing on y = 0
        inst.rows[0] = {1, 0, 0, p.x};
        inst.rows[1] = {0, 1, 0, p.y};
        inst.rows[2] = {0, 0, 1, p.z};
        inst.mesh = m;
        inst.scale = 1.0f;
        instances.push_back(inst);
      }
    instances_ = static_cast<uint32_t>(instances.size());
    scene_.setInstances(instances);
    const float extent = (grid_ - 1) * spacing;
    center_ = {extent * 0.5f, 0.0f, extent * 0.5f};
    distance_ = std::max(extent * 0.9f, spacing * 1.5f);
  }

  Renderer& renderer_;
  UploadQueue uploads_;
  MeshScene scene_;
  std::vector<MeshId> meshes_;
  uint32_t grid_;
  bool orbit_;
  bool placed_ = false;
  uint32_t instances_ = 0;
  int readyFrames_ = 0;
  glm::vec3 center_{0.0f};
  float distance_ = 10.0f;
  std::chrono::steady_clock::time_point start_ = std::chrono::steady_clock::now();
};

}  // namespace ember
