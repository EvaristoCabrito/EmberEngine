#pragma once

#include <array>
#include <memory>
#include <span>
#include <unordered_map>
#include <utility>
#include <vector>

#include "assets/AssetManager.h"
#include "render/MeshFormat.h"
#include "render/MeshGpu.h"
#include "render/TextureFormat.h"
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
  // A mesh built in memory in the same format (render/MeshBuild.h: the game's own meshes, e.g. Ember's architecture);
  // its textures still come from the packs by asset id.
  MeshId addBuilt(std::vector<uint8_t> bytes);
  bool ready() const;  // every added mesh's geometry and textures are on the GPU
  std::pair<uint32_t, uint32_t> progress() const;  // meshes and textures settled (on the GPU or failed), out of all added
  const MeshHeader* header(MeshId id) const;  // null until its asset arrived

  // Replaces every instance. The list becomes visible a few frames later (one upload).
  void setInstances(std::span<const InstanceGpu> instances);

  // --- Texture streaming (the user, 2026-10-29: "faz", the characters' sprites first, some blur on 3D objects accepted) ---------
  // Every texture first lands at its small base (largest mip <= kBaseSize) and grows to the size its objects need on screen, down
  // to the quality's cap (Opções > Gráficos > Texturas 3D: Média = the cooked size, Leve = one mip under it), as long as the
  // GPU's free memory stays above kReserve. Short of memory, the most over-detailed textures step down first.
  // skipTop: mips never loaded at the top (0 = Média, 1 = Leve).
  void setQuality(uint32_t skipTop) { qualitySkip_ = skipTop; }
  // A gated scene (MapStage) keeps every texture at its base until open(): the characters reserve their GPU memory first.
  void setStreamGate(bool gated) { gated_ = gated; }
  void openStreaming() { open_ = true; }
  // The camera this frame: eye position and focal length in pixels (screen height / (2 tan(fov / 2))). Without it, every texture
  // aims at the quality's cap.
  void setView(const glm::vec3& eye, float focalPixels) { eye_ = eye, focal_ = focalPixels, viewSet_ = true; }

  // Once per frame, before rendering.
  void update();
  MeshSceneView view() const;
  // The renderer must wait for this upload-timeline value before reading the scene.
  uint64_t uploadWaitValue() const { return uploadWaitValue_; }

  struct Stats {
    uint32_t meshes = 0, meshesReady = 0, textures = 0, texturesReady = 0, instances = 0;
    uint64_t geometryBytes = 0, textureBytes = 0;
    uint32_t texturesFull = 0, texturesStreaming = 0;  // at the quality's cap / with a load in flight
  };
  Stats stats() const;

  static constexpr uint32_t kBaseSize = 256;             // the always-resident base: largest mip at most this many texels
  static constexpr uint64_t kReserve = 448ull << 20;     // GPU memory left free for everything else (effects, the frame)

 private:
  struct Texture {
    AssetId asset = 0;
    AssetHandle handle;
    Image image;          // the resident image (mips skip .. mipCount-1)
    Image next;           // a resize being uploaded
    ImageUpload upload;
    uint32_t index = kNoTextureIndex;
    std::vector<uint32_t> users;  // material entries that reference it, per field (material * 4 + slot)
    bool failed = false;
    TextureHeader header{};
    bool known = false;                  // header read
    uint32_t skip = UINT32_MAX;          // top mip resident (UINT32_MAX = none yet)
    uint32_t target = UINT32_MAX;        // top mip being loaded (UINT32_MAX = no load)
    uint32_t baseSkip = 0;               // the base's top mip
    uint32_t want = 0;                   // top mip the objects need this tick
    uint64_t residentBytes = 0;
  };
  struct Retired {
    Image image;
    uint32_t index = kNoTextureIndex;
    uint64_t freeAtVersion = 0;  // the material table version that no longer names it (then kFramesInFlight more frames)
  };

  struct Mesh {
    AssetId asset = 0;
    AssetHandle handle;
    std::vector<uint8_t> built;  // addBuilt's bytes (no asset)
    MeshHeader header{};
    bool parsed = false, failed = false;
    uint64_t offset = 0;  // in the geometry buffer
    ImageUpload upload;   // geometry bytes
    bool uploaded = false;
    std::vector<Texture*> textures;  // the textures its materials use (streaming: what each instance needs)
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
  Texture& requestTexture(AssetId asset, uint32_t user);
  void updateTexture(Texture& texture);
  void startUpload(Texture& texture, std::span<const std::byte> bytes, uint32_t skip);
  void stream();                   // every few frames: wants, then upgrades / downgrades within the budget
  uint64_t freeGpuBytes() const;   // the device-local heap's budget minus its usage (VMA, VK_EXT_memory_budget)
  uint64_t bytesAt(const Texture& texture, uint32_t skip) const;
  uint32_t capSkip(const Texture& texture) const;  // the quality's top mip for this texture
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
  // streaming
  std::vector<Retired> retired_;  // old images kept until no frame in flight can read them
  uint32_t qualitySkip_ = 0;
  bool gated_ = false, open_ = false, settled_ = false;
  glm::vec3 eye_{0.0f};
  float focal_ = 0.0f;
  bool viewSet_ = false;
  uint64_t loggedFrame_ = 0;
};

}  // namespace ember
