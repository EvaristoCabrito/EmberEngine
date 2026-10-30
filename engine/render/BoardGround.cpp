#include "render/BoardGround.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "core/Log.h"
#include "render/MeshGpu.h"
#include "render/TextureFormat.h"
#include "rhi/Bindless.h"
#include "rhi/VulkanContext.h"
#include "world/GroundAssets.h"

namespace ember {

namespace {

constexpr int32_t kLoadPriority = 200;  // the ground is the first thing a map shows

int16_t snorm16(float v) { return static_cast<int16_t>(std::lround(std::clamp(v, -1.0f, 1.0f) * 32767.0f)); }

// Octahedral unit vector, 2 x snorm16 (shaders/scene.hlsli octDecode: (x, y) with z the folded axis).
// A zero vector (a vertex no triangle uses) encodes as +y.
uint32_t octEncode(const float* n) {
  const float l1 = std::abs(n[0]) + std::abs(n[1]) + std::abs(n[2]);
  float x = l1 > 0.0f ? n[0] / l1 : 0.0f, y = l1 > 0.0f ? n[1] / l1 : 1.0f;
  if (n[2] < 0.0f) {
    const float fx = (1.0f - std::abs(y)) * (x >= 0.0f ? 1.0f : -1.0f);
    const float fy = (1.0f - std::abs(x)) * (y >= 0.0f ? 1.0f : -1.0f);
    x = fx;
    y = fy;
  }
  return static_cast<uint16_t>(snorm16(x)) | (uint32_t{static_cast<uint16_t>(snorm16(y))} << 16);
}

uint32_t packUnorm8(const float* w, int count) {
  uint32_t out = 0;
  for (int k = 0; k < count; ++k) out |= uint32_t(std::lround(std::clamp(w[k], 0.0f, 1.0f) * 255.0f)) << (8 * k);
  return out;
}

VkFormat vkFormat(TextureFormat format) {
  switch (format) {
    case TextureFormat::Bc7Srgb: return VK_FORMAT_BC7_SRGB_BLOCK;
    case TextureFormat::Bc7Unorm: return VK_FORMAT_BC7_UNORM_BLOCK;
    case TextureFormat::Bc5Unorm: return VK_FORMAT_BC5_UNORM_BLOCK;
  }
  return VK_FORMAT_UNDEFINED;
}

}  // namespace

BoardGround::BoardGround(VulkanContext& context, Bindless& bindless, AssetManager& assets, UploadQueue& uploads)
    : context_(context), bindless_(bindless), assets_(assets), uploads_(uploads) {}

BoardGround::~BoardGround() {
  for (auto& [asset, t] : textures_) uploads_.cancel(t->upload);
  destroy(*geo_);  // (waits for the device: copies and draws may still use the buffers and images)
  if (old_) destroy(*old_);
  for (auto& [asset, t] : textures_) {
    if (t->index != UINT32_MAX) bindless_.removeSampledImage(t->index);
    destroyImage(context_, t->image);
    if (t->handle) assets_.release(t->handle);
  }
}

void BoardGround::destroy(Geometry& g) {
  for (Mesh& m : g.meshes) {
    uploads_.cancel(m.vertexUpload);
    uploads_.cancel(m.indexUpload);
  }
  for (Water& w : g.water) {
    uploads_.cancel(w.vertexUpload);
    uploads_.cancel(w.indexUpload);
  }
  uploads_.cancel(g.recordUpload);
  vkDeviceWaitIdle(context_.device);
  for (Mesh& m : g.meshes) {
    destroyBuffer(context_, m.vertexBuffer);
    destroyBuffer(context_, m.indexBuffer);
  }
  for (Water& w : g.water) {
    destroyBuffer(context_, w.vertexBuffer);
    destroyBuffer(context_, w.indexBuffer);
  }
  destroyBuffer(context_, g.recordBuffer);
}

bool BoardGround::uploaded(const Geometry& g) const {
  if (!g.recordQueued || !uploads_.finished(g.recordUpload)) return false;
  for (const Mesh& m : g.meshes) {
    if (!m.vertexUpload.buffer || !uploads_.finished(m.vertexUpload)) return false;
    if (m.indexUpload.buffer && !uploads_.finished(m.indexUpload)) return false;
  }
  for (const Water& w : g.water)
    if (w.vertexBuffer.buffer && (!uploads_.finished(w.vertexUpload) || !uploads_.finished(w.indexUpload))) return false;
  return true;
}

bool BoardGround::build(const ember::Board& board) { return buildInto(*geo_, board); }

bool BoardGround::rebuild(const ember::Board& board) {
  auto next = std::make_unique<Geometry>();
  stats_.geometryBytes = 0;
  if (!buildInto(*next, board)) return false;
  // A rebuild over a rebuild still on its way up: the one being drawn stays, the unfinished one goes.
  if (old_ && !uploaded(*geo_)) destroy(*geo_);
  else if (old_) destroy(*old_), old_ = std::move(geo_);
  else old_ = std::move(geo_);
  geo_ = std::move(next);
  return true;
}

void BoardGround::texture(const std::string& asset) {
  auto& t = textures_[asset];
  if (!t) {
    t = std::make_unique<Texture>();
    t->handle = assets_.load(assetId(asset), kLoadPriority);
  }
}

bool BoardGround::buildInto(Geometry& geo, const ember::Board& board) {
  TerrainGpu& record = geo.record;
  std::fill(std::begin(record.proto), std::end(record.proto), kNoTextureIndex);
  record.plains049Color = record.plains049Rough = kNoTextureIndex;
  for (uint32_t k = 0; k < kTerrainPaletteSize; ++k) record.layerColor[k] = record.layerNormal[k] = record.layerRough[k] = kNoTextureIndex;
  const ember::BoardData& data = board.data();
  // The palette: the baked layers this map's cells use, in layer order.
  std::vector<int> palette;
  for (const ember::Cell& c : board.cells())
    if (c.ground >= 0 && std::find(palette.begin(), palette.end(), c.ground) == palette.end()) palette.push_back(c.ground);
  std::sort(palette.begin(), palette.end());
  if (palette.size() > kTerrainPaletteSize) {
    logError("ground: map {} uses {} baked ground layers (the palette holds {})", board.draft().id, palette.size(), kTerrainPaletteSize);
    return false;
  }
  record.layerCount = static_cast<uint32_t>(palette.size());
  stats_.layers = record.layerCount;

  // Textures: the procedural surfaces, plains049, and each palette layer's maps.
  const auto want = [&](const std::string& asset, size_t word) {
    texture(asset);
    geo.slots.emplace_back(asset, word);
  };
  const size_t protoWord = offsetof(TerrainGpu, proto) / 4;
  static constexpr const char* kProto[9] = {"grass", "forest", "pavers", "snow", "rock", "gravel", "ash", "earth", "cliff"};
  // A tile set with a "side" tile (GroundAssets.h kEmberGroundTiles: the Misty Cave's rock wall) puts it on the board's cliff faces
  // in place of the procedural earth and cliff surfaces, and on its added side walls (terrain.hlsl: wallFrom's bit 30).
  std::string sideAsset;
  {
    const std::string& set = board.draft().emberTiles;
    if (const auto side = data.emberGroundLayer.find((set.empty() ? board.draft().id : set) + "|side"); side != data.emberGroundLayer.end())
      sideAsset = ember::groundLayerAsset(data.groundLayers[side->second].key, "color");
  }
  for (size_t k = 0; k < 9; ++k) want(!sideAsset.empty() && (k == 7 || k == 8) ? sideAsset : ember::groundProtoAsset(kProto[k]), protoWord + k);
  want(ember::groundPlains049Asset("color"), offsetof(TerrainGpu, plains049Color) / 4);
  want(ember::groundPlains049Asset("rough"), offsetof(TerrainGpu, plains049Rough) / 4);
  for (size_t k = 0; k < palette.size(); ++k) {
    const ember::GroundLayerSource& layer = data.groundLayers[palette[k]];
    if (layer.absent) continue;  // Ember: this set has no such layer; the procedural ground stays
    record.layerTile[k] = layer.tile;
    want(ember::groundLayerAsset(layer.key, "color"), offsetof(TerrainGpu, layerColor) / 4 + k);
    want(ember::groundLayerAsset(layer.key, "color"), offsetof(TerrainGpu, layerHas) / 4 + k);  // 1 once resident
    if (!layer.normal.empty()) want(ember::groundLayerAsset(layer.key, "normal"), offsetof(TerrainGpu, layerNormal) / 4 + k);
    if (!layer.rough.empty()) want(ember::groundLayerAsset(layer.key, "rough"), offsetof(TerrainGpu, layerRough) / 4 + k);
  }
  stats_.textures = static_cast<uint32_t>(textures_.size());

  // Geometry, every look.
  ember::GroundGeometry looks[3] = {ember::buildSmoothGround(board), ember::buildHardGround(board), ember::buildTabletopGround(board)};
  // The smooth look's cut edge is only a thin lip (0.35 under its ground): under raised ground the board showed huge holes (the
  // user, 2026-10-19: "quero que o cenário não tenha esses buracos enormes na elevações, pode preencher tudo?"). Its sides go down
  // to the lowest ground's lip, where the map's ground plane lies (MapStage). The hard and tabletop walls already reach the base.
  // The walls are painted like the edge's lip (terrain.hlsl, push.wallFrom; the user: "deixar o preenchimento inteiro da mesma cor
  // que o resto das bordas"): the vertices from wallFrom on are theirs.
  uint32_t wallFrom = 0;
  if (!board.draft().emberNoSides) {  // a suspended board (the bridge, MapDraft::emberNoSides) stands with no side walls
    const double low = board.lowestGround();
    wallFrom = static_cast<uint32_t>(looks[0].vertexCount());
    ember::closeBoardSides(looks[0], low - 0.35);
    // The 3D water ends where the board does: its sheet ran down past the board's bottom and hung under the ground as icicles (the
    // user, 2026-10-29: "esconde essa água vazando no subterrâneo"). Under the water's own beds, also where the floor lies over them
    // (MapDraft::emberFloorOverWater): the sheet stands at the bed + 0.65, never cut by the walls' bottom.
    double bed = low;
    for (const ember::Cell& c : board.cells()) bed = std::min(bed, c.groundY);
    geo.boardBottom = static_cast<float>(bed - 0.35);
    if (looks[0].vertexCount() == wallFrom) wallFrom = 0;
  }
  // The walls take no light and no shadow of their own (the user, 2026-10-23: "esse sombreado nas bordas do tabuleiro tem que acabar,
  // essa área não deve ser afetada por luz e sombra"; terrain.hlsl). The Inn's floors keep their walls lit as before (never touch the
  // Inn): the top bit of wallFrom says so.
  geo.meshes[0].wallFrom = wallFrom;
  if (!sideAsset.empty())
    for (Mesh& m : geo.meshes) m.wallFrom |= TerrainView::kWallsRock;
  if (board.draft().id.rfind("estalagem", 0) == 0)
    for (Mesh& m : geo.meshes) m.wallFrom |= TerrainView::kWallsLit;
  for (int i = 0; i < 3; ++i) {
    pack(looks[i], palette, geo.meshes[i]);
    upload(geo.meshes[i]);
    stats_.vertices[i] = static_cast<uint32_t>(looks[i].vertexCount());
    stats_.triangles[i] = geo.meshes[i].count / 3;
  }
  // Water, both looks (its sheet follows each look's shore).
  const VkBufferUsageFlags usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
  for (int i = 0; i < 2; ++i) {
    const ember::WaterGeometry g = ember::buildWater(board, i == 0 ? ember::EdgeStyle::Smooth : ember::EdgeStyle::Hard);
    if (g.indices.empty()) continue;
    Water& w = geo.water[i];
    w.vertices.resize(g.positions.size() / 3);
    for (size_t v = 0; v < w.vertices.size(); ++v) {
      std::memcpy(w.vertices[v].position, &g.positions[v * 3], 12);
      w.vertices[v].normal = octEncode(&g.normals[v * 3]);
    }
    // The triangles grouped by their tile's look (waterLookOfVariant): a water tile its own, a bank beside water the look of
    // the water next to it, so each group draws in one go.
    std::vector<uint32_t> grouped[4];
    const ember::HexLayout& layout = board.layout();
    auto lookOf = [&](const ember::Cell& c) { return waterLookOfVariant(board.draft().variant(c.i)); };
    for (size_t t = 0; t + 2 < g.indices.size(); t += 3) {
      float x = 0.0f, z = 0.0f;
      for (int k = 0; k < 3; ++k) x += g.positions[g.indices[t + k] * 3] / 3.0f, z += g.positions[g.indices[t + k] * 3 + 2] / 3.0f;
      const ember::Cell& cell = board.cellAt(x, z);
      uint32_t look = kWaterDefaultLook;
      if (cell.water) {
        look = lookOf(cell);
      } else {  // a bank: the look of the first water hex beside it
        for (const auto& [nc, nr] : ember::neighbors(layout, cell.c, cell.r))
          if (const ember::Cell& n = board.cell(nc, nr); n.water) {
            look = lookOf(n);
            break;
          }
      }
      grouped[look].insert(grouped[look].end(), g.indices.begin() + t, g.indices.begin() + t + 3);
    }
    w.indices.clear();
    for (int k = 0; k < 4; ++k) {
      w.lookCount[k] = static_cast<uint32_t>(grouped[k].size());
      w.indices.insert(w.indices.end(), grouped[k].begin(), grouped[k].end());
    }
    w.vertexBuffer = createDeviceBuffer(context_, w.vertices.size() * sizeof(WaterVertexGpu), usage, true);
    w.indexBuffer = createDeviceBuffer(context_, w.indices.size() * sizeof(uint32_t), usage, true);
    w.vertexUpload = {};
    w.vertexUpload.buffer = w.vertexBuffer.buffer;
    w.vertexUpload.data = std::as_bytes(std::span(w.vertices));
    w.indexUpload = {};
    w.indexUpload.buffer = w.indexBuffer.buffer;
    w.indexUpload.data = std::as_bytes(std::span(w.indices));
    uploads_.enqueue(w.vertexUpload);
    uploads_.enqueue(w.indexUpload);
    stats_.geometryBytes += w.vertices.size() * sizeof(WaterVertexGpu) + w.indices.size() * 4;
  }
  return true;
}

void BoardGround::pack(const ember::GroundGeometry& g, const std::vector<int>& palette, Mesh& out) {
  const size_t n = g.vertexCount();
  out.vertices.resize(n);
  std::vector<float> layerWeights(kTerrainPaletteSize);
  for (size_t v = 0; v < n; ++v) {
    TerrainVertexGpu& o = out.vertices[v];
    std::memcpy(o.position, &g.positions[v * 3], 12);
    o.normal = octEncode(&g.normals[v * 3]);
    const float* w = &g.weights[v * g.stride];
    o.surfaces[0] = packUnorm8(w, 4);
    o.surfaces[1] = packUnorm8(w + 4, 4);
    std::fill(layerWeights.begin(), layerWeights.end(), 0.0f);
    for (size_t k = 0; k < palette.size(); ++k) layerWeights[k] = w[8 + palette[k]];
    for (int k = 0; k < 6; ++k) o.layers[k] = packUnorm8(&layerWeights[k * 4], 4);
  }
  out.indices = g.indices;
  out.count = static_cast<uint32_t>(g.indices.empty() ? n : g.indices.size());
}

void BoardGround::upload(Mesh& mesh) {
  const VkBufferUsageFlags usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
  const VkDeviceSize vertexBytes = mesh.vertices.size() * sizeof(TerrainVertexGpu);
  mesh.vertexBuffer = createDeviceBuffer(context_, vertexBytes, usage, true);
  mesh.vertexUpload = {};
  mesh.vertexUpload.buffer = mesh.vertexBuffer.buffer;
  mesh.vertexUpload.data = std::as_bytes(std::span(mesh.vertices));
  uploads_.enqueue(mesh.vertexUpload);
  stats_.geometryBytes += vertexBytes;
  if (!mesh.indices.empty()) {
    const VkDeviceSize indexBytes = mesh.indices.size() * sizeof(uint32_t);
    mesh.indexBuffer = createDeviceBuffer(context_, indexBytes, usage, true);
    mesh.indexUpload = {};
    mesh.indexUpload.buffer = mesh.indexBuffer.buffer;
    mesh.indexUpload.data = std::as_bytes(std::span(mesh.indices));
    uploads_.enqueue(mesh.indexUpload);
    stats_.geometryBytes += indexBytes;
  }
}

void BoardGround::updateTexture(Texture& t) {
  if (t.failed || t.index != UINT32_MAX) return;
  if (!t.image.image) {
    const AssetState state = assets_.state(t.handle);
    if (state == AssetState::Failed) {
      t.failed = true;
      return;
    }
    if (state != AssetState::Ready) return;
    const std::span<const std::byte> bytes = assets_.data(t.handle);
    TextureHeader h{};
    if (bytes.size() >= sizeof(h)) std::memcpy(&h, bytes.data(), sizeof(h));
    if (h.magic != kTextureMagic || h.version != kTextureFormatVersion || bytes.size() != sizeof(h) + h.dataBytes) {
      t.failed = true;
      return;
    }
    ImageDesc desc;
    desc.format = vkFormat(h.format);
    desc.extent = {h.width, h.height};
    desc.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    desc.mipLevels = h.mipCount;
    desc.sharedWithTransfer = true;
    t.image = createImage(context_, desc);
    t.upload = {t.image.image, h.width, h.height, h.mipCount, 1, 16, bytes.subspan(sizeof(h))};
    uploads_.enqueue(t.upload);
    stats_.textureBytes += h.dataBytes;
    return;
  }
  if (!uploads_.finished(t.upload)) return;
  t.index = bindless_.addSampledImage(t.image.view);
  uploadWaitValue_ = std::max(uploadWaitValue_, t.upload.signalValue);
  assets_.release(t.handle);
  t.handle = {};
}

void BoardGround::update() {
  uint32_t ready = 0;
  bool failed = false;
  for (auto& [asset, t] : textures_) {
    updateTexture(*t);
    ready += t->index != UINT32_MAX ? 1 : 0;
    if (t->failed && !failed) {
      logError("ground: texture {} is missing or damaged (cook the ground pack: Cooker ground)", asset);
      failed = true;
    }
  }
  stats_.texturesReady = ready;
  Geometry& geo = *geo_;
  for (const Mesh& m : geo.meshes)
    for (const ImageUpload* u : {&m.vertexUpload, &m.indexUpload})
      if (u->buffer && uploads_.finished(*u)) uploadWaitValue_ = std::max(uploadWaitValue_, u->signalValue);
  for (const Water& w : geo.water)
    for (const ImageUpload* u : {&w.vertexUpload, &w.indexUpload})
      if (u->buffer && uploads_.finished(*u)) uploadWaitValue_ = std::max(uploadWaitValue_, u->signalValue);
  // Every texture resident: the record gets its bindless indices and goes up once.
  if (!geo.recordQueued && ready == textures_.size() && !textures_.empty()) {
    for (const auto& [asset, word] : geo.slots) {
      const bool has = word >= offsetof(TerrainGpu, layerHas) / 4 && word < offsetof(TerrainGpu, layerHas) / 4 + kTerrainPaletteSize;
      reinterpret_cast<uint32_t*>(&geo.record)[word] = has ? 1u : textures_.at(asset)->index;
    }
    geo.recordBuffer = createDeviceBuffer(context_, sizeof(TerrainGpu), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, true);
    geo.recordUpload = {};
    geo.recordUpload.buffer = geo.recordBuffer.buffer;
    geo.recordUpload.data = std::as_bytes(std::span(&geo.record, 1));
    uploads_.enqueue(geo.recordUpload);
    geo.recordQueued = true;
  }
  if (geo.recordQueued && uploads_.finished(geo.recordUpload)) uploadWaitValue_ = std::max(uploadWaitValue_, geo.recordUpload.signalValue);
  // A rebuild's new geometry is on the GPU: the previous one goes.
  if (old_ && uploaded(geo)) {
    destroy(*old_);
    old_.reset();
  }
}

uint32_t BoardGround::textureIndex(const std::string& asset) const {
  const auto it = textures_.find(asset);
  return it != textures_.end() && it->second->index != UINT32_MAX ? it->second->index : kNoTextureIndex;
}

// Ready = something to draw: the board's geometry, or a rebuild's previous one while the new one goes up.
bool BoardGround::ready() const {
  const Geometry& geo = served();
  if (!geo.recordQueued || !uploads_.finished(geo.recordUpload)) return false;
  for (const Mesh& m : geo.meshes) {
    if (!m.vertexUpload.buffer || !uploads_.finished(m.vertexUpload)) return false;
    if (m.indexUpload.buffer && !uploads_.finished(m.indexUpload)) return false;
  }
  return true;
}

WaterView BoardGround::water(ember::EdgeStyle style) const {
  const Geometry& geo = served();
  const Water& w = geo.water[style == ember::EdgeStyle::Smooth ? 0 : 1];
  if (!w.vertexBuffer.buffer || !uploads_.finished(w.vertexUpload) || !uploads_.finished(w.indexUpload)) return {};
  WaterView view{bufferAddress(context_, w.vertexBuffer), bufferAddress(context_, w.indexBuffer), static_cast<uint32_t>(w.indices.size())};
  std::copy(std::begin(w.lookCount), std::end(w.lookCount), view.lookCount);
  view.floorY = geo.boardBottom;
  return view;
}

TerrainView BoardGround::view(ember::EdgeStyle style) const {
  if (!ready()) return {};
  const Geometry& geo = served();
  const Mesh& m = geo.meshes[style == ember::EdgeStyle::Smooth ? 0 : style == ember::EdgeStyle::Hard ? 1 : 2];
  TerrainView v;
  v.record = bufferAddress(context_, geo.recordBuffer);
  v.vertices = bufferAddress(context_, m.vertexBuffer);
  v.indices = m.indices.empty() ? 0 : bufferAddress(context_, m.indexBuffer);
  v.count = m.count;
  v.seamStep = style == ember::EdgeStyle::Tabletop ? static_cast<float>(ember::kStep) : 0.0f;
  v.wallFrom = m.wallFrom;
  return v;
}

}  // namespace ember
