#pragma once

// The board's ground on the GPU (Ember's terrain, engine/world/GroundMesh.h): both looks built at load
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
  float layerTile[kTerrainPaletteSize];    // world units per repeat; 0 = 4 (GroundLayerSource::tile)
};
static_assert(sizeof(TerrainGpu) == 132 * 4);

struct WaterVertexGpu {
  float position[3];
  uint32_t normal;  // octahedral, 2 x snorm16
};
static_assert(sizeof(WaterVertexGpu) == 16);

// The Map Editor's Terreno → Água versions (the user, 2026-10-26): variants 31..33 = "Água 01..03" (water looks 001..003), 34 =
// "Água original" (look 0); every other water tile draws look 003, the default ("eu quero essa da foto como padrão mas manter a
// original no jogo"). The ground under them is the 001 tile's (no layer of their own).
constexpr int kWaterLookFirstVariant = 31;
constexpr int kWaterOriginalVariant = 34;
constexpr uint32_t kWaterDefaultLook = 3;
inline uint32_t waterLookOfVariant(int variant) {
  if (variant == kWaterOriginalVariant) return 0u;
  return variant >= kWaterLookFirstVariant && variant < kWaterLookFirstVariant + 3 ? static_cast<uint32_t>(variant - kWaterLookFirstVariant + 1)
                                                                                    : kWaterDefaultLook;
}

struct WaterView {
  VkDeviceAddress vertices = 0, indices = 0;
  uint32_t count = 0;  // indices
  uint32_t lookCount[4]{};  // the indices grouped by the tiles' own look (0 original, 1..3 looks 001..003), in that order
  uint32_t look = 0;   // the panel's "Água": 0 = each tile its own look (lookCount), 1..3 = that look everywhere, 4 = the original
                       // everywhere; shaders/water.hlsl: 0 the original diorama water, 1 look 001 "ember" (water_look001.hlsli),
                       // 2 look 002, 3 look 003 (reads the scene below it: Renderer::waterRefracts)
  static constexpr uint32_t kOriginalEverywhere = 4;
  explicit operator bool() const { return vertices != 0 && count != 0; }
};

// Ember's 3D water (world/Water3D: the Map Editor's Agua 3D), in this board's world, and Ember's material
// numbers for the map's water version (ThreeWater.setVersion).
struct Water3DVertexGpu {
  float position[3];  // the still surface
  float coverage;     // brush coverage (edges fade, < 0.05 not drawn)
  float depth;        // above the ground, in Ember tiles (shore foam)
  float point[2];     // Ember's tile frame (x, -y): waves and patterns
  float pad;
};
static_assert(sizeof(Water3DVertexGpu) == 32);

struct Water3DView {
  VkDeviceAddress vertices = 0, indices = 0;
  uint32_t count = 0;  // indices
  uint32_t colorMap = 0, normalMap = 0, roughMap = 0;  // bindless
  float color[3]{};    // linear
  float opacity = 0, normalScale = 0, roughness = 0, modern = 0, v4 = 0, repeat = 1;
  uint32_t look = 0;  // shaders/water3d.hlsl: 0 rio (the board's river material), 1 leito (Ember's ThreeWater), 2 / 3 looks
                      // 001 / 002, 4 look 003 "decente" (reads the scene below it: Renderer::waterRefracts)
  float floorY = -1e30f;  // nothing of the sheet is drawn under this height: the board's own bottom (BoardGround::build)
  explicit operator bool() const { return vertices != 0 && count != 0; }
};

// What the renderer draws (device addresses; empty until everything is resident).
struct TerrainView {
  VkDeviceAddress record = 0, vertices = 0, indices = 0;
  uint32_t count = 0;  // vertices to draw (indices when indexed)
  float seamStep = 0.0f;  // tabletop: a dark seam on the walls at every this many units of height (0 = none)
  uint32_t wallFrom = 0;  // smooth look: vertices from here on are the board's added side walls, painted like its edge (0 = none)
  // wallFrom's top bit: the board's sides lit and shadowed like the ground (the Inn's floors); clear = its walls and steep side
  // faces take no light or shadow of their own (terrain.hlsl).
  static constexpr uint32_t kWallsLit = 0x80000000u;
  // wallFrom's bit 30: the added side walls wear the cliff texture (the map's tile set has a "side" rock wall) instead of the top's.
  static constexpr uint32_t kWallsRock = 0x40000000u;
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
  bool build(const ember::Board& board);
  // Once per frame: streaming.
  void update();
  bool ready() const;
  // Another texture from the ground pack (the moon's phase art: GroundAssets.h), resident with the ground.
  void requestTexture(const std::string& asset) { texture(asset); }
  uint32_t textureIndex(const std::string& asset) const;  // bindless; kNoTextureIndex until resident
  TerrainView view(ember::EdgeStyle style) const;
  WaterView water(ember::EdgeStyle style) const;  // empty when the map has none
  Water3DView water3d(ember::EdgeStyle style) const;  // empty when the map has no 3D water
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
    uint32_t wallFrom = 0;  // the first vertex of the board's added side walls (closeBoardSides); 0 = none
  };

  void texture(const std::string& asset);  // requests it once
  void pack(const ember::GroundGeometry& g, const std::vector<int>& palette, Mesh& out);
  void upload(Mesh& mesh);
  void buildWater3D(const ember::Board& board);
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
    std::vector<uint32_t> indices;  // grouped by look (WaterView::lookCount)
    uint32_t lookCount[4]{};
    Buffer vertexBuffer, indexBuffer;
    ImageUpload vertexUpload, indexUpload;
  };
  Water water_[2];
  struct Water3D {
    std::vector<Water3DVertexGpu> vertices;
    std::vector<uint32_t> indices;
    Buffer vertexBuffer, indexBuffer;
    ImageUpload vertexUpload, indexUpload;
  };
  Water3D water3d_[2];  // smooth, hard (tabletop uses hard's)
  float boardBottom_ = -1e30f;  // the smooth look's side walls end here (the 3D water's floorY)
  Water3DView water3dMaterial_;
  uint64_t uploadWaitValue_ = 0;
  Stats stats_;
};

}  // namespace ember
