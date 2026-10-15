#pragma once

// The board's ground on the GPU (Engine2's terrain, engine/world/GroundMesh.h): both looks built at load
// and kept resident, so switching between them (R8, a player setting) is instant; the ground textures
// the map uses streamed from the archive (world/GroundAssets.h). Drawn by the renderer's terrain pass
// (shaders/terrain.hlsl).

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "assets/AssetManager.h"
#include "rhi/Image.h"
#include "rhi/UploadQueue.h"
#include "world/GroundMesh.h"

namespace ember {

class Bindless;
class VulkanContext;

inline constexpr uint32_t kTerrainPaletteSize = 24;  // baked ground layers one map may use

// GPU layouts, mirrored in shaders/terrain.hlsl.
struct TerrainVertexGpu {
  float position[3];
  uint32_t normal;       // octahedral, 2 x snorm16
  uint32_t surfaces[2];  // 8 surface weights, unorm8
  uint32_t layers[6];    // palette layer weights, unorm8
};
static_assert(sizeof(TerrainVertexGpu) == 48);

struct TerrainGpu {
  uint32_t proto[9];  // grass forest pavers snow rock gravel ash earth cliff
  uint32_t plains049Color, plains049Rough;
  uint32_t layerCount;
  uint32_t layerColor[kTerrainPaletteSize], layerNormal[kTerrainPaletteSize], layerRough[kTerrainPaletteSize];
  uint32_t layerHas[kTerrainPaletteSize];  // 1 = the layer exists in the set (its colour map is resident)
};
static_assert(sizeof(TerrainGpu) == 108 * 4);

struct WaterVertexGpu {
  float position[3];
  uint32_t normal;  // octahedral, 2 x snorm16
};
static_assert(sizeof(WaterVertexGpu) == 16);

struct WaterView {
  VkDeviceAddress vertices = 0, indices = 0;
  uint32_t count = 0;  // indices
  explicit operator bool() const { return vertices != 0 && count != 0; }
};

// What the renderer draws (device addresses; empty until everything is resident).
struct TerrainView {
  VkDeviceAddress record = 0, vertices = 0, indices = 0;
  uint32_t count = 0;  // vertices to draw (indices when indexed)
  float seamStep = 0.0f;  // tabletop: a dark seam on the walls at every this many units of height (0 = none)
  explicit operator bool() const { return record != 0 && count != 0; }
};

class BoardGround {
 public:
  BoardGround(VulkanContext& context, Bindless& bindless, AssetManager& assets, UploadQueue& uploads);
  ~BoardGround();
  BoardGround(const BoardGround&) = delete;
  BoardGround& operator=(const BoardGround&) = delete;

  // Builds both looks for the board and requests its textures. False if the map needs more baked layers
  // than the palette holds.
  bool build(const e2::Board& board);
  // Once per frame: streaming.
  void update();
  bool ready() const;
  // Another texture from the ground pack (the moon's phase art: GroundAssets.h), resident with the ground.
  void requestTexture(const std::string& asset) { texture(asset); }
  uint32_t textureIndex(const std::string& asset) const;  // bindless; kNoTextureIndex until resident
  TerrainView view(e2::EdgeStyle style) const;
  WaterView water(e2::EdgeStyle style) const;  // empty when the map has none
  // The renderer must wait for this upload-timeline value before drawing the ground.
  uint64_t uploadWaitValue() const { return uploadWaitValue_; }

  struct Stats {
    uint32_t vertices[3]{}, triangles[3]{};
    uint32_t textures = 0, texturesReady = 0, layers = 0;
    uint64_t geometryBytes = 0, textureBytes = 0;
  };
  Stats stats() const { return stats_; }

 private:
  struct Texture {
    AssetHandle handle;
    Image image;
    ImageUpload upload;
    uint32_t index = UINT32_MAX;  // bindless, once resident
    bool failed = false;
  };
  struct Mesh {
    std::vector<TerrainVertexGpu> vertices;
    std::vector<uint32_t> indices;
    Buffer vertexBuffer, indexBuffer;
    ImageUpload vertexUpload, indexUpload;
    uint32_t count = 0;
  };

  void texture(const std::string& asset);  // requests it once
  void pack(const e2::GroundGeometry& g, const std::vector<int>& palette, Mesh& out);
  void upload(Mesh& mesh);
  void updateTexture(Texture& t);

  VulkanContext& context_;
  Bindless& bindless_;
  AssetManager& assets_;
  UploadQueue& uploads_;
  std::unordered_map<std::string, std::unique_ptr<Texture>> textures_;
  std::vector<std::pair<std::string, size_t>> slots_;  // (asset, word in record_) to fill once resident
  TerrainGpu record_{};
  Buffer recordBuffer_;
  ImageUpload recordUpload_;
  bool recordQueued_ = false;
  Mesh meshes_[3];  // smooth, hard, tabletop
  struct Water {
    std::vector<WaterVertexGpu> vertices;
    std::vector<uint32_t> indices;
    Buffer vertexBuffer, indexBuffer;
    ImageUpload vertexUpload, indexUpload;
  };
  Water water_[2];
  uint64_t uploadWaitValue_ = 0;
  Stats stats_;
};

}  // namespace ember
