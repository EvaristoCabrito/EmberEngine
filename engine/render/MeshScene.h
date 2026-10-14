#pragma once

#include <array>
#include <memory>
#include <span>
#include <unordered_map>
#include <vector>

#include "assets/AssetManager.h"
#include "render/MeshFormat.h"
#include "render/MeshGpu.h"
#include "rhi/Image.h"
#include "rhi/UploadQueue.h"

namespace ember {

class Bindless;
class VulkanContext;

using MeshId = uint32_t;

// What the renderer needs to draw the scene this frame (device addresses of the active tables).
struct MeshSceneView {
  uint64_t meshes = 0;     // MeshGpu[]
  uint64_t materials = 0;  // MaterialGpu[]
  uint64_t instances = 0;  // InstanceGpu[]
  uint32_t instanceCount = 0;
  uint32_t meshCount = 0;  // entries of meshes[] (instances naming a later mesh are skipped)
  uint64_t instancesVersion = 0;  // changes whenever a new instance list becomes active
  explicit operator bool() const { return instanceCount != 0 && meshes && materials && instances; }
};

// 3D meshes on the GPU: cooked meshes (geometry + materials + textures) loaded through the asset
// streamer and the upload queue, and the placed instances. All GPU data lives in device-local buffers;
// nothing is drawn from host memory. Main thread only.
//
// Tables the shaders read every frame (meshes, materials, instances) are double-buffered: a change is
// uploaded into the copy no frame in flight is reading, then becomes the active one, so the GPU never
// reads a table the transfer queue is writing.
//
// M3 keeps every added mesh resident (geometry capacity is fixed at creation); world streaming with
// eviction arrives with the world cells (M4).
class MeshScene {
 public:
  struct Capacity {
    uint64_t geometryBytes = 1ull << 30;  // cooked mesh data of every loaded mesh
    uint32_t meshes = 4096;
    uint32_t materials = 16384;
    uint32_t instances = 1u << 18;
  };
  static constexpr uint32_t kMaxTraversalDepth = 8;  // BVH levels the GPU traversal walks (checked at load)

  MeshScene(VulkanContext& context, Bindless& bindless, AssetManager& assets, UploadQueue& uploads, const Capacity& capacity);
  ~MeshScene();
  MeshScene(const MeshScene&) = delete;
  MeshScene& operator=(const MeshScene&) = delete;

  // Starts loading a cooked mesh (asset "meshes/<path>.mesh"). Its instances draw once it is resident.
  MeshId add(AssetId meshAsset);
  bool ready() const;  // every added mesh's geometry and textures are on the GPU
  const MeshHeader* header(MeshId id) const;  // null until its asset arrived

  // Replaces every instance. The list becomes visible a few frames later (one upload).
  void setInstances(std::span<const InstanceGpu> instances);

  // Once per frame, before rendering.
  void update();
  MeshSceneView view() const;
  // The renderer must wait for this upload-timeline value before reading the scene.
  uint64_t uploadWaitValue() const { return uploadWaitValue_; }

  struct Stats {
    uint32_t meshes = 0, meshesReady = 0, textures = 0, texturesReady = 0, instances = 0;
    uint64_t geometryBytes = 0, textureBytes = 0;
  };
  Stats stats() const;

 private:
  struct Texture {
    AssetHandle handle;
    Image image;
    ImageUpload upload;
    uint32_t index = kNoTextureIndex;
    std::vector<uint32_t> users;  // material entries that reference it, per field (material * 4 + slot)
    bool failed = false;
  };

  struct Mesh {
    AssetId asset = 0;
    AssetHandle handle;
    MeshHeader header{};
    bool parsed = false, failed = false;
    uint64_t offset = 0;  // in the geometry buffer
    ImageUpload upload;   // geometry bytes
    bool uploaded = false;
  };

  // A table the shaders read: two device copies, CPU contents, one upload at a time.
  struct Table {
    std::array<Buffer, 2> copies;
    int active = -1, writing = -1;
    uint64_t switchedFrame = 0;
    bool dirty = false;
    std::vector<std::byte> cpu, snapshot;
    uint64_t usedBytes = 0;    // sent up to here
    uint64_t activeBytes = 0;  // valid bytes in the active copy
    uint64_t version = 0;      // switches so far
    ImageUpload upload;
  };

  void createTable(Table& table, uint64_t bytes);
  void updateTable(Table& table);
  void parse(Mesh& mesh, MeshId id);
  uint32_t requestTexture(AssetId asset, uint32_t user);
  void updateTexture(Texture& texture);
  template <class T>
  T* tableData(Table& table) { return reinterpret_cast<T*>(table.cpu.data()); }

  VulkanContext& context_;
  Bindless& bindless_;
  AssetManager& assets_;
  UploadQueue& uploads_;
  Capacity capacity_;

  Buffer geometry_;
  uint64_t geometryUsed_ = 0;
  std::vector<std::unique_ptr<Mesh>> meshes_;     // stable addresses: uploads point at members
  std::unordered_map<AssetId, std::unique_ptr<Texture>> textures_;
  uint32_t materialCount_ = 0;
  uint32_t instanceCount_ = 0;
  Table meshTable_, materialTable_, instanceTable_;
  uint64_t frame_ = 0;
  uint64_t uploadWaitValue_ = 0;
  uint64_t textureBytes_ = 0;
};

}  // namespace ember
