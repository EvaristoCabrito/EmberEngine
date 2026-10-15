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
    : context_(context), bindless_(bindless), assets_(assets), uploads_(uploads) {
  std::fill(std::begin(record_.proto), std::end(record_.proto), kNoTextureIndex);
  record_.plains049Color = record_.plains049Rough = kNoTextureIndex;
  for (uint32_t k = 0; k < kTerrainPaletteSize; ++k) record_.layerColor[k] = record_.layerNormal[k] = record_.layerRough[k] = kNoTextureIndex;
}

BoardGround::~BoardGround() {
  for (Mesh& m : meshes_) {
    uploads_.cancel(m.vertexUpload);
    uploads_.cancel(m.indexUpload);
  }
  uploads_.cancel(recordUpload_);
  for (auto& [asset, t] : textures_) uploads_.cancel(t->upload);
  vkDeviceWaitIdle(context_.device);  // copies and draws may still use the buffers and images
  for (auto& [asset, t] : textures_) {
    if (t->index != UINT32_MAX) bindless_.removeSampledImage(t->index);
    destroyImage(context_, t->image);
    if (t->handle) assets_.release(t->handle);
  }
  for (Mesh& m : meshes_) {
    destroyBuffer(context_, m.vertexBuffer);
    destroyBuffer(context_, m.indexBuffer);
  }
  destroyBuffer(context_, recordBuffer_);
}

void BoardGround::texture(const std::string& asset) {
  auto& t = textures_[asset];
  if (!t) {
    t = std::make_unique<Texture>();
    t->handle = assets_.load(assetId(asset), kLoadPriority);
  }
}

bool BoardGround::build(const e2::Board& board) {
  const e2::BoardData& data = board.data();
  // The palette: the baked layers this map's cells use, in layer order.
  std::vector<int> palette;
  for (const e2::Cell& c : board.cells())
    if (c.ground >= 0 && std::find(palette.begin(), palette.end(), c.ground) == palette.end()) palette.push_back(c.ground);
  std::sort(palette.begin(), palette.end());
  if (palette.size() > kTerrainPaletteSize) {
    logError("ground: map {} uses {} baked ground layers (the palette holds {})", board.draft().id, palette.size(), kTerrainPaletteSize);
    return false;
  }
  record_.layerCount = static_cast<uint32_t>(palette.size());
  stats_.layers = record_.layerCount;

  // Textures: the procedural surfaces, plains049, and each palette layer's maps.
  const auto want = [&](const std::string& asset, size_t word) {
    texture(asset);
    slots_.emplace_back(asset, word);
  };
  const size_t protoWord = offsetof(TerrainGpu, proto) / 4;
  static constexpr const char* kProto[9] = {"grass", "forest", "pavers", "snow", "rock", "gravel", "ash", "earth", "cliff"};
  for (size_t k = 0; k < 9; ++k) want(e2::groundProtoAsset(kProto[k]), protoWord + k);
  want(e2::groundPlains049Asset("color"), offsetof(TerrainGpu, plains049Color) / 4);
  want(e2::groundPlains049Asset("rough"), offsetof(TerrainGpu, plains049Rough) / 4);
  for (size_t k = 0; k < palette.size(); ++k) {
    const e2::GroundLayerSource& layer = data.groundLayers[palette[k]];
    if (layer.absent) continue;  // Engine2: this set has no such layer; the procedural ground stays
    want(e2::groundLayerAsset(layer.key, "color"), offsetof(TerrainGpu, layerColor) / 4 + k);
    want(e2::groundLayerAsset(layer.key, "color"), offsetof(TerrainGpu, layerHas) / 4 + k);  // 1 once resident
    if (!layer.normal.empty()) want(e2::groundLayerAsset(layer.key, "normal"), offsetof(TerrainGpu, layerNormal) / 4 + k);
    if (!layer.rough.empty()) want(e2::groundLayerAsset(layer.key, "rough"), offsetof(TerrainGpu, layerRough) / 4 + k);
  }
  stats_.textures = static_cast<uint32_t>(textures_.size());

  // Geometry, both looks.
  const e2::GroundGeometry looks[2] = {e2::buildSmoothGround(board), e2::buildHardGround(board)};
  for (int i = 0; i < 2; ++i) {
    pack(looks[i], palette, meshes_[i]);
    upload(meshes_[i]);
    stats_.vertices[i] = static_cast<uint32_t>(looks[i].vertexCount());
    stats_.triangles[i] = meshes_[i].count / 3;
  }
  return true;
}

void BoardGround::pack(const e2::GroundGeometry& g, const std::vector<int>& palette, Mesh& out) {
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
  for (const Mesh& m : meshes_)
    for (const ImageUpload* u : {&m.vertexUpload, &m.indexUpload})
      if (u->buffer && uploads_.finished(*u)) uploadWaitValue_ = std::max(uploadWaitValue_, u->signalValue);
  // Every texture resident: the record gets its bindless indices and goes up once.
  if (!recordQueued_ && ready == textures_.size() && !textures_.empty()) {
    for (const auto& [asset, word] : slots_) {
      const bool has = word >= offsetof(TerrainGpu, layerHas) / 4 && word < offsetof(TerrainGpu, layerHas) / 4 + kTerrainPaletteSize;
      reinterpret_cast<uint32_t*>(&record_)[word] = has ? 1u : textures_.at(asset)->index;
    }
    recordBuffer_ = createDeviceBuffer(context_, sizeof(TerrainGpu), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, true);
    recordUpload_ = {};
    recordUpload_.buffer = recordBuffer_.buffer;
    recordUpload_.data = std::as_bytes(std::span(&record_, 1));
    uploads_.enqueue(recordUpload_);
    recordQueued_ = true;
  }
  if (recordQueued_ && uploads_.finished(recordUpload_)) uploadWaitValue_ = std::max(uploadWaitValue_, recordUpload_.signalValue);
}

bool BoardGround::ready() const {
  if (!recordQueued_ || !uploads_.finished(recordUpload_)) return false;
  for (const Mesh& m : meshes_) {
    if (!m.vertexUpload.buffer || !uploads_.finished(m.vertexUpload)) return false;
    if (m.indexUpload.buffer && !uploads_.finished(m.indexUpload)) return false;
  }
  return true;
}

TerrainView BoardGround::view(e2::EdgeStyle style) const {
  if (!ready()) return {};
  const Mesh& m = meshes_[style == e2::EdgeStyle::Smooth ? 0 : 1];
  TerrainView v;
  v.record = bufferAddress(context_, recordBuffer_);
  v.vertices = bufferAddress(context_, m.vertexBuffer);
  v.indices = m.indices.empty() ? 0 : bufferAddress(context_, m.indexBuffer);
  v.count = m.count;
  return v;
}

}  // namespace ember
