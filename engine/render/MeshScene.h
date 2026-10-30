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
#include "render/VfxDraw.h"
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
// Geometry has individually reclaimable allocations. Regional scenes keep the camera region
// resident, retire distant allocations safely, and reload their cooked assets on demand.
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
  // A cooked mesh a spell effect draws (render/vfx: the Nuvem de Adagas' dagger), never instanced: its full-detail triangles are
  // kept on the CPU for the effect runtime, and its textures always aim at the quality's cap (it flies close to the camera, and no
  // instance tells the streaming its size).
  MeshId addEffect(AssetId meshAsset);
  struct EffectMesh {
    const std::vector<VfxVertex>* triangles = nullptr;  // mesh space (glTF: +Y up), three vertices a triangle; null until loaded
    uint32_t baseColor = kNoTextureIndex, normal = kNoTextureIndex, orm = kNoTextureIndex, emissive = kNoTextureIndex;  // material 0
    glm::vec3 emissiveFactor{0.0f};
    explicit operator bool() const { return triangles && !triangles->empty() && baseColor != kNoTextureIndex; }
  };
  EffectMesh effect(MeshId id) const;  // the bindless indices change as the textures stream: read it every frame
  bool ready() const;  // every added mesh's geometry and textures are on the GPU
  // Geometry and every texture's base on the GPU: the models can be placed (the textures sharpen later); ready() also waits for
  // the first streaming pass, which a short GPU never settles (the user, 2026-10-29: a map with no 3D, no fog, no monsters).
  bool placeable() const;
  std::pair<uint32_t, uint32_t> progress() const;  // meshes and textures settled (on the GPU or failed), out of all added
  const MeshHeader* header(MeshId id) const;  // null until its asset arrived

  // Replaces every instance. The list becomes visible a few frames later (one upload).
  void setInstances(std::span<const InstanceGpu> instances);

  // --- Texture streaming (the user, 2026-10-29: "faz", the characters' sprites first, some blur on 3D objects accepted) ---------
  // Every texture first lands at its small base (largest mip <= kBaseSize) and grows to the size its objects need on screen, down
  // to the quality's cap (Opções > Gráficos > Texturas 3D: Alta 2048, Média 1024, Leve 512, all from the one cooked file), as long
  // as the GPU's free memory stays above kReserve. Short of memory, the most over-detailed textures step down first.
  // maxSize: the largest side ever loaded; the top mips above it are skipped, a texture already under it keeps its size.
  void setQuality(uint32_t maxSize) { qualityMaxSize_ = maxSize; }
  // A gated scene (MapStage) keeps every texture at its base until open(): the characters reserve their GPU memory first.
  void setStreamGate(bool gated) { gated_ = gated; }
  void openStreaming() { open_ = true; }
  // The camera this frame: eye position and focal length in pixels (screen height / (2 tan(fov / 2))). Without it, every texture
  // aims at the quality's cap.
  void setRegionStreaming(bool enabled) { regional_ = enabled; }
  void setRegionView(const glm::mat4& viewProjection) { regionView_ = viewProjection; regionViewSet_ = true; }
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
  static constexpr uint64_t kReserve = 128ull << 20;     // GPU memory left free for everything else (effects, the frame)

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
    float screenPixels = 0.0f;
    uint64_t residentBytes = 0;
  };
  struct Retired {
    Image image;
    uint32_t index = kNoTextureIndex;
    uint64_t freeAtVersion = 0;  // first material version without this image
    uint64_t safeFrame = UINT64_MAX;
  };

  struct RetiredGeometry {
    Buffer buffer;
    uint64_t version = 0, safeFrame = UINT64_MAX;
  };

  struct Mesh {
    AssetId asset = 0;
    AssetHandle handle;
    std::vector<uint8_t> built;  // addBuilt's bytes (no asset)
    MeshHeader header{};
    bool parsed = false, failed = false;
    Buffer geometry;
    uint64_t bytes = 0, lastWanted = 0;
    bool wanted = true, selected = false;
    float distance = 0.0f;
    ImageUpload upload;   // geometry bytes
    bool uploaded = false;
    std::vector<Texture*> textures;  // the textures its materials use (streaming: what each instance needs)
    uint32_t materialBase = 0;
    bool pinned = false;
    bool effect = false;                 // addEffect's: triangles kept, textures at the cap
    std::vector<VfxVertex> triangles;
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
  void updateGeometry();
  void retireGeometry(Mesh& mesh, MeshId id);
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

  std::vector<RetiredGeometry> retiredGeometry_;
  bool regional_ = false, instancesSet_ = false, regionViewSet_ = false;
  // The region streams only a scene whose geometry does not fit the budget (as the textures: everything fits = everything loaded):
  // the user, 2026-10-30, the Monastery's walls and props gone with the region on while all of it fit in memory.
  bool overBudget_ = false;
  uint64_t geometryBudget_ = 0;  // capacity_.geometryBytes, or the test hook's EMBER_GEOMETRY_BUDGET_MB
  bool streamingGeometry() const { return regional_ && overBudget_; }
  glm::mat4 regionView_{1.0f};
  uint64_t geometryUsed_ = 0;
  std::vector<std::unique_ptr<Mesh>> meshes_;     // stable addresses: uploads point at members
  std::unordered_map<AssetId, std::unique_ptr<Texture>> textures_;
  uint32_t materialCount_ = 0;
  uint32_t instanceCount_ = 0;     // setInstances' list (allInstances_)
  std::vector<InstanceGpu> allInstances_;
  uint32_t gpuInstanceCount_ = 0;  // the GPU's list: the instances whose geometry is resident (update)
  bool instancesDirty_ = false;
  Table meshTable_, materialTable_, instanceTable_;
  uint64_t frame_ = 0;
  uint64_t uploadWaitValue_ = 0;
  uint64_t textureBytes_ = 0;
  // streaming
  std::vector<Retired> retired_;  // old images kept until no frame in flight can read them
  uint32_t qualityMaxSize_ = 2048;
  bool gated_ = false, open_ = false, settled_ = false;
  glm::vec3 eye_{0.0f};
  float focal_ = 0.0f;
  bool viewSet_ = false;
  uint64_t loggedFrame_ = 0;
};

}  // namespace ember
