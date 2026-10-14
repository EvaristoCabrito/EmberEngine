#pragma once

// Proxy-quality measurement: one mesh drawn alone at several on-screen widths, captured as shaded
// colour, geometric normals and shading normals, for three variants:
//   reference  mesh A at full detail (LOD threshold ~0 px)
//   original   mesh A at the engine's LOD (1 px)
//   proxy      mesh B at the engine's LOD (1 px)
// Images: <out>/<width>px_<variant>_<view>.png. tools/compare_meshes.py turns them into silhouette and
// normal-error numbers.

#include <chrono>
#include <cmath>
#include <filesystem>
#include <format>
#include <string>
#include <vector>

#include "assets/AssetManager.h"
#include "core/Log.h"
#include "render/MeshScene.h"
#include "render/Renderer.h"
#include "rhi/UploadQueue.h"

namespace ember {

inline bool runMeshCompare(VulkanContext& context, Renderer& renderer, AssetManager& assets, AssetId meshA, AssetId meshB,
                           const std::filesystem::path& out, VkExtent2D size) {
  UploadQueue uploads(context);
  MeshScene scene(context, renderer.bindless(), assets, uploads, {.geometryBytes = 256ull << 20});
  const MeshId a = scene.add(meshA), b = scene.add(meshB);
  const auto pump = [&](int frames) {
    for (int i = 0; i < frames; ++i) {
      assets.update();
      scene.update();
      uploads.flush();
      renderer.setMeshes(scene.view());
      renderer.waitFor(uploads.timeline(), scene.uploadWaitValue());
      renderer.renderFrame(0.0f);
    }
  };
  const auto start = std::chrono::steady_clock::now();
  while (!scene.ready() || !scene.header(a) || !scene.header(b)) {
    pump(1);
    if (std::chrono::steady_clock::now() - start > std::chrono::seconds(60)) {
      logError("compare: meshes did not load");
      return false;
    }
  }
  std::filesystem::create_directories(out);
  renderer.setGroundExtent(0.0f);  // the mesh alone: background pixels stay black in the debug views
  const MeshHeader& h = *scene.header(a);
  const glm::vec3 center{h.boundsCenter[0], h.boundsCenter[1], h.boundsCenter[2]};
  const float extent = std::max(h.boundsMax[0] - h.boundsMin[0], h.boundsMax[2] - h.boundsMin[2]);
  const float aspect = static_cast<float>(size.width) / static_cast<float>(size.height);

  struct Variant {
    const char* name;
    MeshId mesh;
    float lodPixels;
  };
  const Variant variants[] = {{"reference", a, 0.001f}, {"original", a, 1.0f}, {"proxy", b, 1.0f}};
  const char* views[] = {"shaded", "geometric", "shading"};
  for (const int width : {400, 200, 100, 50, 25}) {
    // Distance at which the mesh's widest side covers `width` pixels.
    const float fov = glm::radians(40.0f);
    const float projX = 1.0f / std::tan(fov * 0.5f) / aspect;
    const float distance = extent * projX * static_cast<float>(size.width) / (2.0f * static_cast<float>(width));
    const float yaw = glm::radians(30.0f), pitch = glm::radians(25.0f);
    const glm::vec3 eye = center + distance * glm::vec3(std::sin(yaw) * std::cos(pitch), std::sin(pitch), std::cos(yaw) * std::cos(pitch));
    const Camera camera = Camera::lookAt(eye, center, fov, aspect, 0.05f);
    renderer.setScene(camera, {});
    for (const Variant& v : variants) {
      InstanceGpu inst{};
      inst.rows[0] = {1, 0, 0, 0};
      inst.rows[1] = {0, 1, 0, 0};
      inst.rows[2] = {0, 0, 1, 0};
      inst.mesh = v.mesh;
      inst.scale = 1.0f;
      scene.setInstances({&inst, 1});
      renderer.setLodPixelError(v.lodPixels);
      for (uint32_t view = 0; view < 3; ++view) {
        renderer.setDebugView(view);
        pump(6);  // the new instance table becomes active; occlusion history settles
        renderer.captureFrame(0.0f, (out / std::format("{}px_{}_{}.png", width, v.name, views[view])).string());
      }
    }
  }
  renderer.setDebugView(0);
  renderer.setLodPixelError(1.0f);
  return true;
}

}  // namespace ember
