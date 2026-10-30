#include "render/MeshScene.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>

#include "render/TextureFormat.h"
#include "rhi/Bindless.h"
#include "rhi/VulkanContext.h"

namespace ember {

namespace {

constexpr int32_t kLoadPriority = 150;
// Geometry sent to the upload queue per frame (its own copy rate is 8 MB a frame). By bytes, not by count: the Monastery's 1068
// small built pieces at two a frame took 9 s to arrive (2026-10-30: its walls missing whenever the region streamed).
constexpr uint64_t kGeometryBytesPerFrame = 8ull << 20;
bool streamTrace() { static const bool enabled = std::getenv("EMBER_STREAM_CHECK") != nullptr; return enabled; }

uint32_t bvhDepth(const MeshNode* nodes, uint32_t index) {
  const MeshNode& n = nodes[index];
  if (n.group >= 0) return 1;
  uint32_t deepest = 0;
  for (uint32_t c = 0; c < n.childCount; ++c) deepest = std::max(deepest, bvhDepth(nodes, n.childOffset + c));
  return deepest + 1;
}

VkFormat vkFormat(TextureFormat format) {
  switch (format) {
    case TextureFormat::Bc7Srgb: return VK_FORMAT_BC7_SRGB_BLOCK;
    case TextureFormat::Bc7Unorm: return VK_FORMAT_BC7_UNORM_BLOCK;
    case TextureFormat::Bc5Unorm: return VK_FORMAT_BC5_UNORM_BLOCK;
  }
  return VK_FORMAT_UNDEFINED;
}

// MeshVertex's octahedral normal (2 x snorm16), as scene.hlsli decodeNormal.
glm::vec3 octNormal(uint32_t packed) {
  const float x = std::max(static_cast<float>(static_cast<int16_t>(packed & 0xFFFFu)) / 32767.0f, -1.0f);
  const float y = std::max(static_cast<float>(static_cast<int16_t>(packed >> 16)) / 32767.0f, -1.0f);
  glm::vec3 n{x, y, 1.0f - std::abs(x) - std::abs(y)};
  const float t = std::clamp(-n.z, 0.0f, 1.0f);
  n.x += n.x >= 0.0f ? -t : t;
  n.y += n.y >= 0.0f ? -t : t;
  return glm::normalize(n);
}

}  // namespace

MeshScene::MeshScene(VulkanContext& context, Bindless& bindless, AssetManager& assets, UploadQueue& uploads, const Capacity& capacity)
    : context_(context), bindless_(bindless), assets_(assets), uploads_(uploads), capacity_(capacity) {
  createTable(meshTable_, uint64_t{capacity.meshes} * sizeof(MeshGpu));
  createTable(materialTable_, uint64_t{capacity.materials} * sizeof(MaterialGpu));
  createTable(instanceTable_, uint64_t{capacity.instances} * sizeof(InstanceGpu));
}

MeshScene::~MeshScene() {
  for (auto& mesh : meshes_) uploads_.cancel(mesh->upload);
  for (auto& [id, texture] : textures_) uploads_.cancel(texture->upload);
  for (Table* table : {&meshTable_, &materialTable_, &instanceTable_}) uploads_.cancel(table->upload);
  vkDeviceWaitIdle(context_.device);  // copies and draws may still use the buffers and images
  for (auto& [id, texture] : textures_) {
    if (texture->index != kNoTextureIndex) bindless_.removeSampledImage(texture->index);
    destroyImage(context_, texture->image);
    if (texture->next.image) destroyImage(context_, texture->next);
    assets_.release(texture->handle);
  }
  for (Retired& r : retired_) {
    if (r.index != kNoTextureIndex) bindless_.removeSampledImage(r.index);
    destroyImage(context_, r.image);
  }
  for (auto& mesh : meshes_)
    if (mesh->built.empty()) assets_.release(mesh->handle);
  for (Table* table : {&meshTable_, &materialTable_, &instanceTable_})
    for (Buffer& b : table->copies) destroyBuffer(context_, b);
  for (auto& mesh : meshes_) destroyBuffer(context_, mesh->geometry);
  for (auto& retired : retiredGeometry_) destroyBuffer(context_, retired.buffer);
}

void MeshScene::createTable(Table& table, uint64_t bytes) {
  for (Buffer& b : table.copies)
    b = createDeviceBuffer(context_, bytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, true);
  table.cpu.assign(bytes, std::byte{0});
}

MeshId MeshScene::add(AssetId meshAsset) {
  auto mesh = std::make_unique<Mesh>();
  mesh->asset = meshAsset;
  mesh->handle = assets_.load(meshAsset, kLoadPriority);
  meshes_.push_back(std::move(mesh));
  const MeshId id = static_cast<MeshId>(meshes_.size() - 1);
  EMBER_CHECK(id < capacity_.meshes, "MeshScene: mesh capacity reached");
  return id;
}

MeshId MeshScene::addBuilt(std::vector<uint8_t> bytes) {
  auto mesh = std::make_unique<Mesh>();
  mesh->built = std::move(bytes);
  mesh->pinned = true;
  meshes_.push_back(std::move(mesh));
  const MeshId id = static_cast<MeshId>(meshes_.size() - 1);
  EMBER_CHECK(id < capacity_.meshes, "MeshScene: mesh capacity reached");
  parse(*meshes_.back(), id);
  return id;
}

MeshId MeshScene::addEffect(AssetId meshAsset) {
  const MeshId id = add(meshAsset);
  meshes_[id]->effect = true;
  return id;
}

MeshScene::EffectMesh MeshScene::effect(MeshId id) const {
  EffectMesh e;
  if (id >= meshes_.size() || !meshes_[id]->parsed || meshes_[id]->header.materialCount == 0) return e;
  const Mesh& mesh = *meshes_[id];
  e.triangles = &mesh.triangles;
  const auto* materials = reinterpret_cast<const MaterialGpu*>(materialTable_.cpu.data());
  const MaterialGpu& m = materials[mesh.materialBase];
  e.baseColor = m.baseColorTexture, e.normal = m.normalTexture, e.orm = m.ormTexture, e.emissive = m.emissiveTexture;
  e.emissiveFactor = m.emissive;
  return e;
}

const MeshHeader* MeshScene::header(MeshId id) const {
  return id < meshes_.size() && meshes_[id]->parsed ? &meshes_[id]->header : nullptr;
}

bool MeshScene::placeable() const {
  for (const auto& mesh : meshes_)
    if (!mesh->failed && !(streamingGeometry() ? mesh->parsed : mesh->uploaded)) return false;
  for (const auto& [id, texture] : textures_)
    if (!texture->failed && texture->index == kNoTextureIndex) return false;
  return true;
}

bool MeshScene::ready() const {
  // A gated scene also waits for the first streaming pass: every texture at the size it needs, or the budget reached.
  return placeable() && (!gated_ || (open_ && settled_));
}

std::pair<uint32_t, uint32_t> MeshScene::progress() const {
  uint32_t done = 0, total = 0;
  for (const auto& mesh : meshes_)
    ++total, done += (mesh->failed || mesh->uploaded || (streamingGeometry() && mesh->parsed && !mesh->selected)) ? 1u : 0u;
  for (const auto& [id, texture] : textures_) ++total, done += (texture->failed || texture->index != kNoTextureIndex) ? 1u : 0u;
  if (gated_) ++total, done += (open_ && settled_) ? 1u : 0u;
  return {done, total};
}

void MeshScene::setInstances(std::span<const InstanceGpu> instances) {
  EMBER_CHECK(instances.size() <= capacity_.instances, "MeshScene: instance capacity reached");
  instancesSet_ = true;
  instanceCount_ = static_cast<uint32_t>(instances.size());
  allInstances_.assign(instances.begin(), instances.end());
  instancesDirty_ = true;  // the GPU's list is made in update(): only the instances whose geometry is resident
}

void MeshScene::parse(Mesh& mesh, MeshId id) {
  const std::span<const std::byte> bytes =
      mesh.built.empty() ? assets_.data(mesh.handle) : std::span<const std::byte>(reinterpret_cast<const std::byte*>(mesh.built.data()), mesh.built.size());
  MeshHeader& h = mesh.header;
  if (bytes.size() >= sizeof(MeshHeader)) std::memcpy(&h, bytes.data(), sizeof(MeshHeader));
  const MeshSections s = meshSections(h);
  if (h.magic != kMeshMagic || h.version != kMeshFormatVersion || bytes.size() != s.end) {
    logError("meshes: {:016x} is not a valid cooked mesh", mesh.asset);
    mesh.failed = true;
    return;
  }
  const auto* nodes = reinterpret_cast<const MeshNode*>(bytes.data() + s.nodes);
  for (uint32_t level = 0; level < h.levelCount; ++level) {
    const uint32_t depth = bvhDepth(nodes, level);
    if (depth > kMaxTraversalDepth) {
      logError("meshes: {:016x} has a BVH {} levels deep (the GPU walks {})", mesh.asset, depth, kMaxTraversalDepth);
      mesh.failed = true;
      return;
    }
  }
  if (materialCount_ + h.materialCount > capacity_.materials) {
    logError("meshes: material capacity reached");
    mesh.failed = true;
    return;
  }

  // Materials: factors now, texture indices as each texture becomes resident.
  const uint32_t materialBase = materialCount_;
  const auto* materials = reinterpret_cast<const MeshMaterial*>(bytes.data() + s.materials);
  for (uint32_t m = 0; m < h.materialCount; ++m) {
    const MeshMaterial& src = materials[m];
    MaterialGpu& out = tableData<MaterialGpu>(materialTable_)[materialBase + m];
    out = {};
    out.baseColor = {src.baseColor[0], src.baseColor[1], src.baseColor[2], src.baseColor[3]};
    out.emissive = {src.emissive[0], src.emissive[1], src.emissive[2]};
    out.metallic = src.metallic;
    out.roughness = src.roughness;
    out.normalScale = src.normalScale;
    out.occlusionStrength = src.occlusionStrength;
    out.alphaCutoff = src.alphaCutoff;
    out.flags = src.flags;
    const AssetId textures[4] = {src.baseColorTexture, src.normalTexture, src.ormTexture, src.emissiveTexture};
    uint32_t* slots[4] = {&out.baseColorTexture, &out.normalTexture, &out.ormTexture, &out.emissiveTexture};
    for (int t = 0; t < 4; ++t) {
      if (!textures[t]) {
        *slots[t] = kNoTextureIndex;
        continue;
      }
      Texture& texture = requestTexture(textures[t], (materialBase + m) * 4 + t);
      *slots[t] = texture.index;
      if (std::find(mesh.textures.begin(), mesh.textures.end(), &texture) == mesh.textures.end()) mesh.textures.push_back(&texture);
    }
  }
  materialCount_ += h.materialCount;
  materialTable_.dirty = true;
  mesh.materialBase = materialBase;

  // An effect's mesh: its full-detail clusters (refinedGroup < 0) as plain triangles for the effect runtime (material 0 only).
  if (mesh.effect) {
    const auto* vertices = reinterpret_cast<const MeshVertex*>(bytes.data() + s.vertices);
    const auto* clusterVertices = reinterpret_cast<const uint32_t*>(bytes.data() + s.clusterVertices);
    const auto* triangles = reinterpret_cast<const uint8_t*>(bytes.data() + s.clusterTriangles);
    const auto* clusters = reinterpret_cast<const MeshCluster*>(bytes.data() + s.clusters);
    mesh.triangles.clear();
    for (uint32_t c = 0; c < h.clusterCount; ++c) {
      const MeshCluster& cl = clusters[c];
      if (cl.refinedGroup >= 0) continue;
      for (uint32_t k = 0; k < uint32_t{cl.triangleCount} * 3u; ++k) {
        const MeshVertex& v = vertices[clusterVertices[cl.vertexOffset + triangles[cl.triangleOffset + k]]];
        mesh.triangles.push_back({glm::vec3(v.position[0], v.position[1], v.position[2]), v.uv[0], octNormal(v.normal), v.uv[1]});
      }
    }
  }

  // Metadata stays resident; the region scheduler owns GPU geometry and reloads.
  mesh.bytes = s.end;
  const uint64_t base = 0;
  MeshGpu& record = tableData<MeshGpu>(meshTable_)[id];
  record = {};
  record.vertices = base + s.vertices;
  record.clusterVertices = base + s.clusterVertices;
  record.triangles = base + s.clusterTriangles;
  record.clusters = base + s.clusters;
  record.groups = base + s.groups;
  record.nodes = base + s.nodes;
  record.levelCount = h.levelCount;
  record.materialBase = materialBase;
  record.clusterCount = h.clusterCount;
  record.ready = 0;
  record.sphere = {h.boundsCenter[0], h.boundsCenter[1], h.boundsCenter[2], h.boundsRadius};
  meshTable_.dirty = true;
  mesh.parsed = true;
  if (!mesh.pinned) {
    assets_.release(mesh.handle);
    mesh.handle = {};
  }
}

// The first load's target: chosen once the header is known (the base while the scene is gated, else the quality's cap when it fits).
constexpr uint32_t kInitialTarget = UINT32_MAX - 1;

MeshScene::Texture& MeshScene::requestTexture(AssetId asset, uint32_t user) {
  auto& slot = textures_[asset];
  if (!slot) {
    slot = std::make_unique<Texture>();
    slot->asset = asset;
    slot->handle = assets_.load(asset, kLoadPriority);
    slot->target = kInitialTarget;
  }
  slot->users.push_back(user);
  return *slot;  // index kNoTextureIndex until resident
}

uint64_t MeshScene::bytesAt(const Texture& t, uint32_t skip) const {
  uint64_t offset = 0;
  for (uint32_t m = 0; m < skip && m < t.header.mipCount; ++m) offset += textureMipBytes(t.header.width, t.header.height, m);
  return t.header.dataBytes - offset;
}

uint32_t MeshScene::capSkip(const Texture& t) const {
  uint32_t skip = 0;
  while (skip + 1 < t.header.mipCount && std::max(t.header.width >> skip, t.header.height >> skip) > qualityMaxSize_) ++skip;
  return std::min(skip, t.baseSkip);
}

uint64_t MeshScene::freeGpuBytes() const {
  const VkPhysicalDeviceMemoryProperties* props = nullptr;
  vmaGetMemoryProperties(context_.allocator, &props);
  VmaBudget budgets[VK_MAX_MEMORY_HEAPS];
  vmaGetHeapBudgets(context_.allocator, budgets);
  uint32_t heap = 0;
  VkDeviceSize largest = 0;
  for (uint32_t i = 0; i < props->memoryHeapCount; ++i)
    if ((props->memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) && props->memoryHeaps[i].size > largest)
      largest = props->memoryHeaps[i].size, heap = i;
  uint64_t free = budgets[heap].budget > budgets[heap].usage ? budgets[heap].budget - budgets[heap].usage : 0;
  // Test hook: EMBER_TEXTURE_BUDGET_MB pretends the 3D textures may hold only that much (a smaller card), so the over-budget path
  // can be measured on this one. Unset: the card's real free memory.
  static const uint64_t testBudget = [] {
    const char* v = std::getenv("EMBER_TEXTURE_BUDGET_MB");
    return v ? std::strtoull(v, nullptr, 10) << 20 : 0ull;
  }();
  if (testBudget) free = std::min(free, testBudget + kReserve > textureBytes_ ? testBudget + kReserve - textureBytes_ : 0ull);
  return free;
}

void MeshScene::startUpload(Texture& t, std::span<const std::byte> bytes, uint32_t skip) {
  const TextureHeader& h = t.header;
  const uint64_t offset = h.dataBytes - bytesAt(t, skip);
  ImageDesc desc;
  desc.format = vkFormat(h.format);
  desc.extent = {std::max(1u, h.width >> skip), std::max(1u, h.height >> skip)};
  desc.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  desc.mipLevels = h.mipCount - skip;
  desc.sharedWithTransfer = true;
  t.next = createImage(context_, desc);
  t.upload = {t.next.image, desc.extent.width, desc.extent.height, desc.mipLevels, 1, 16, bytes.subspan(sizeof(h) + offset)};
  uploads_.enqueue(t.upload);
}

void MeshScene::updateTexture(Texture& texture) {
  if (texture.failed || texture.target == UINT32_MAX) return;
  if (!texture.next.image) {
    const AssetState state = assets_.state(texture.handle);
    if (state == AssetState::Failed) {
      assets_.release(texture.handle);
      texture.handle = {};
      texture.target = UINT32_MAX;
      if (texture.skip == UINT32_MAX) texture.failed = true;  // a resize that failed keeps the resident image
      return;
    }
    if (state != AssetState::Ready) return;
    const std::span<const std::byte> bytes = assets_.data(texture.handle);
    TextureHeader h{};
    if (bytes.size() >= sizeof(h)) std::memcpy(&h, bytes.data(), sizeof(h));
    if (h.magic != kTextureMagic || h.version != kTextureFormatVersion || bytes.size() != sizeof(h) + h.dataBytes || h.mipCount == 0) {
      logError("meshes: damaged texture asset");
      assets_.release(texture.handle);
      texture.handle = {};
      texture.target = UINT32_MAX;
      if (texture.skip == UINT32_MAX) texture.failed = true;
      return;
    }
    if (!texture.known) {
      texture.header = h, texture.known = true;
      uint32_t base = 0;
      while (base + 1 < h.mipCount && std::max(h.width >> base, h.height >> base) > kBaseSize) ++base;
      texture.baseSkip = base;
      texture.want = capSkip(texture);
    }
    if (texture.target == kInitialTarget) {
      const uint32_t cap = capSkip(texture);
      const bool fits = freeGpuBytes() > kReserve + bytesAt(texture, cap);
      texture.target = (gated_ && !open_) || !fits ? texture.baseSkip : cap;
    }
    // Loads can arrive several frames after admission; recheck actual headroom
    // before allocating, including other geometry and texture transfers.
    if (freeGpuBytes() < bytesAt(texture, texture.target) + (texture.target < texture.baseSkip ? (64ull << 20) : 0ull)) return;
    startUpload(texture, bytes, texture.target);
    return;
  }
  if (!uploads_.finished(texture.upload)) return;
  const uint32_t index = bindless_.addSampledImage(texture.next.view);
  uploadWaitValue_ = std::max(uploadWaitValue_, texture.upload.signalValue);
  // the old image leaves once the material table points away from it and no frame in flight can still read it
  // (a table upload already under way still carries the old index: the version after it is the first without it)
  if (texture.image.image) retired_.push_back({texture.image, texture.index, materialTable_.version + (materialTable_.writing >= 0 ? 2u : 1u)});
  textureBytes_ -= texture.residentBytes;
  texture.residentBytes = bytesAt(texture, texture.target);
  textureBytes_ += texture.residentBytes;
  texture.image = texture.next;
  texture.next = {};
  texture.index = index;
  if (streamTrace()) logInfo("stream texture {}: {} -> {} px, {} KB resident", assets_.name(texture.asset),
      texture.skip == UINT32_MAX ? 0u : std::max(texture.header.width >> texture.skip, texture.header.height >> texture.skip),
      std::max(texture.header.width >> texture.target, texture.header.height >> texture.target), texture.residentBytes >> 10);
  texture.skip = texture.target;
  texture.target = UINT32_MAX;
  assets_.release(texture.handle);
  texture.handle = {};
  auto* materials = tableData<MaterialGpu>(materialTable_);
  for (const uint32_t user : texture.users) {
    MaterialGpu& m = materials[user / 4];
    uint32_t* slots[4] = {&m.baseColorTexture, &m.normalTexture, &m.ormTexture, &m.emissiveTexture};
    *slots[user % 4] = texture.index;
  }
  materialTable_.dirty = true;
}

void MeshScene::stream() {
  // What each texture needs: the largest size any of its objects shows on screen (its bounds' diameter in pixels, with a margin),
  // never past the quality's cap, never under the base. Without a camera every texture aims at the cap.
  constexpr float kMargin = 1.5f;
  for (auto& [id, t] : textures_)
    if (t->known) { t->want = viewSet_ ? t->baseSkip : capSkip(*t); t->screenPixels = 0.0f; }
  if (viewSet_ && focal_ > 0.0f) {
    const InstanceGpu* instances = allInstances_.data();
    for (uint32_t i = 0; i < instanceCount_; ++i) {
      const InstanceGpu& inst = instances[i];
      if (inst.mesh >= meshes_.size() || (inst.flags & kInstanceShadowOnly)) continue;
      const Mesh& mesh = *meshes_[inst.mesh];
      if (!mesh.parsed || mesh.textures.empty() || (streamingGeometry() && (!mesh.wanted || !mesh.selected || !mesh.uploaded))) continue;
      const glm::vec4 c{mesh.header.boundsCenter[0], mesh.header.boundsCenter[1], mesh.header.boundsCenter[2], 1.0f};
      const glm::vec3 centre{glm::dot(inst.rows[0], c), glm::dot(inst.rows[1], c), glm::dot(inst.rows[2], c)};
      const float radius = mesh.header.boundsRadius * inst.scale;
      const float distance = std::max(glm::length(centre - eye_) - radius, 0.5f);
      const float pixels = std::max(1.0f, 2.0f * radius * focal_ / distance * kMargin);
      for (Texture* t : mesh.textures) {
        if (!t->known) continue;
        const float size = static_cast<float>(std::max(t->header.width, t->header.height));
        const int s = static_cast<int>(std::floor(std::log2(std::max(1.0f, size / pixels))));
        const uint32_t want = std::clamp(static_cast<uint32_t>(std::max(s, 0)), capSkip(*t), t->baseSkip);
        t->want = std::min(t->want, want);
        t->screenPixels = std::max(t->screenPixels, pixels);
      }
    }
  }
  for (const auto& mesh : meshes_)
    if (mesh->effect)
      for (Texture* t : mesh->textures)
        if (t->known) t->want = capSkip(*t);
  // Everything fits at the quality's size (the textures held now count as free): no streaming, every texture at its cap, the
  // camera never matters (the user, 2026-10-29: load the whole map when it fits; stream only past the budget). Before any instance
  // is placed the same: nothing yet tells what is small on screen.
  uint64_t free = freeGpuBytes();
  {
    uint64_t atCap = 0;
    for (auto& [id, t] : textures_)
      if (t->known) atCap += bytesAt(*t, capSkip(*t));
    if (instanceCount_ == 0 || free + textureBytes_ >= atCap + kReserve)
      for (auto& [id, t] : textures_)
        if (t->known && (!streamingGeometry() || !viewSet_ || instanceCount_ == 0 || t->screenPixels > 0.0f)) t->want = capSkip(*t);
  }
  uint32_t inFlight = 0;
  for (auto& [id, t] : textures_) inFlight += t->target != UINT32_MAX ? 1u : 0u;
  constexpr uint32_t kMaxInFlight = 6;
  std::vector<Texture*> pick;
  std::vector<Texture*> inactive;
  if (streamingGeometry() && instancesSet_) {
    for (auto& [id, t] : textures_) {
      bool active = false;
      for (const auto& mesh : meshes_)
        if (mesh->wanted && mesh->selected && mesh->uploaded && std::find(mesh->textures.begin(), mesh->textures.end(), t.get()) != mesh->textures.end()) { active = true; break; }
      if (!active && t->known && t->target == UINT32_MAX && t->skip < t->baseSkip) inactive.push_back(t.get());
    }
    for (Texture* t : inactive) {
      if (inFlight >= 2 || free < bytesAt(*t, t->baseSkip)) break;
      t->target = t->baseSkip;
      t->handle = assets_.load(t->asset, kLoadPriority);
      free -= bytesAt(*t, t->baseSkip);
      ++inFlight;
    }
  }
  if (free < kReserve) {
    // Short of memory: the most over-detailed resident textures step down first (to what they need), then the farthest one size.
    for (auto& [id, t] : textures_)
      if (t->known && t->target == UINT32_MAX && t->skip != UINT32_MAX && t->skip < t->baseSkip) pick.push_back(t.get());
    std::sort(pick.begin(), pick.end(), [](const Texture* a, const Texture* b) {
      const int oa = static_cast<int>(a->want) - static_cast<int>(a->skip), ob = static_cast<int>(b->want) - static_cast<int>(b->skip);
      return oa != ob ? oa > ob : a->want > b->want;
    });
    for (Texture* t : pick) {
      if (inFlight >= 2) break;
      const uint32_t target = t->want > t->skip ? t->want : t->skip + 1;
      const uint64_t replacement = bytesAt(*t, target);
      if (free < replacement) continue;
      free -= replacement;
      t->target = target;
      t->handle = assets_.load(t->asset, kLoadPriority);
      ++inFlight;
    }
    settled_ = inFlight == 0;
    return;
  }
  // Room: the textures furthest from what they need grow first, as long as the reserve stays free; one that needs less than it
  // holds steps down only when memory runs short (above).
  for (auto& [id, t] : textures_)
    if (t->known && t->target == UINT32_MAX && t->skip != UINT32_MAX && t->skip > t->want) pick.push_back(t.get());
  std::sort(pick.begin(), pick.end(), [](const Texture* a, const Texture* b) {
    const float ap = static_cast<float>(a->skip - a->want) * std::max(1.0f, a->screenPixels);
    const float bp = static_cast<float>(b->skip - b->want) * std::max(1.0f, b->screenPixels);
    return ap > bp;
  });
  bool blocked = false;
  for (Texture* t : pick) {
    if (inFlight >= kMaxInFlight) break;
    const uint64_t extra = bytesAt(*t, t->want);  // old image remains alive during replacement
    if (free < kReserve + extra) {
      blocked = true;
      continue;
    }
    t->target = t->want;
    t->handle = assets_.load(t->asset, kLoadPriority);
    free -= extra;
    ++inFlight;
  }
  const bool wasSettled = settled_;
  settled_ = inFlight == 0;
  if (settled_ && (!wasSettled || frame_ - loggedFrame_ > 3600)) {
    loggedFrame_ = frame_;
    const Stats s = stats();
    uint32_t enough = 0;
    for (auto& [id, t] : textures_) enough += t->known && t->skip != UINT32_MAX && t->skip <= t->want ? 1u : 0u;
    logInfo("meshes: textures {} of {} at the size the camera needs, {} at full size{} ({} MB on the GPU, {} MB free)", enough, s.textures,
            s.texturesFull, blocked ? ", the rest held by the budget" : "", textureBytes_ >> 20, free >> 20);
  }
}

void MeshScene::updateTable(Table& table) {
  // An upload that landed makes its copy the active one.
  if (table.writing >= 0 && uploads_.finished(table.upload)) {
    table.active = table.writing;
    table.activeBytes = table.snapshot.size();
    ++table.version;
    table.writing = -1;
    table.switchedFrame = frame_;
    uploadWaitValue_ = std::max(uploadWaitValue_, table.upload.signalValue);
  }
  if (!table.dirty || table.writing >= 0 || table.usedBytes == 0) return;
  // Write the copy no frame in flight can still be reading.
  if (table.active >= 0 && frame_ - table.switchedFrame <= kFramesInFlight) return;
  table.writing = table.active < 0 ? 0 : table.active ^ 1;
  table.snapshot.assign(table.cpu.begin(), table.cpu.begin() + static_cast<ptrdiff_t>(table.usedBytes));
  table.upload = {};
  table.upload.buffer = table.copies[table.writing].buffer;
  table.upload.data = table.snapshot;
  uploads_.enqueue(table.upload);
  table.dirty = false;
}

// A mesh record must stop naming an allocation before it can be recycled. Keep old
// addresses alive through the table switch and every graphics frame already in flight.
void MeshScene::retireGeometry(Mesh& mesh, MeshId id) {
  if (!mesh.uploaded || !mesh.geometry.buffer) return;
  if (streamTrace()) logInfo("stream evict {}: {:.2f} MB (deferred until GPU table releases it)", assets_.name(mesh.asset), mesh.bytes / 1048576.0);
  tableData<MeshGpu>(meshTable_)[id].ready = 0;
  meshTable_.dirty = true;
  retiredGeometry_.push_back({mesh.geometry, meshTable_.version + (meshTable_.writing >= 0 ? 2u : 1u)});
  mesh.geometry = {};
  mesh.uploaded = false;
  mesh.upload = {};
  instancesDirty_ = true;
}

void MeshScene::updateGeometry() {
  std::erase_if(retiredGeometry_, [&](RetiredGeometry& r) {
    if (meshTable_.version < r.version) return false;
    if (r.safeFrame == UINT64_MAX) r.safeFrame = frame_ + kFramesInFlight + 2;
    if (frame_ < r.safeFrame) return false;
    if (streamTrace()) logInfo("stream free geometry: {:.2f} MB", r.buffer.size / 1048576.0);
    geometryUsed_ -= r.buffer.size;
    destroyBuffer(context_, r.buffer);
    return true;
  });
  // Every parsed mesh's geometry against the budget: under it, the whole scene stays resident and nothing streams. Test hook:
  // EMBER_GEOMETRY_BUDGET_MB pretends the budget is that small (the over-budget path on this card).
  {
    static const uint64_t testBudget = [] {
      const char* v = std::getenv("EMBER_GEOMETRY_BUDGET_MB");
      return v ? static_cast<uint64_t>(std::max(1, std::atoi(v))) << 20 : 0ull;
    }();
    uint64_t total = 0;
    for (const auto& mesh : meshes_)
      if (mesh->parsed && !mesh->failed) total += mesh->bytes;
    const uint64_t budget = testBudget ? testBudget : capacity_.geometryBytes;
    geometryBudget_ = budget;
    const bool over = total > budget;
    if (over != overBudget_ && regional_)
      logInfo("meshes: geometry {:.1f} MB {} the {:.1f} MB budget: {}", total / 1048576.0, over ? "over" : "within", budget / 1048576.0,
              over ? "the camera's region streams" : "everything stays resident");
    overBudget_ = over;
  }
  for (MeshId id = 0; id < meshes_.size(); ++id) {
    Mesh& mesh = *meshes_[id];
    if (mesh.geometry.buffer && !mesh.uploaded && uploads_.finished(mesh.upload)) {
      if (streamTrace()) logInfo("stream resident {}: {:.2f} MB", assets_.name(mesh.asset), mesh.bytes / 1048576.0);
      mesh.uploaded = true;
      instancesDirty_ = true;
      uploadWaitValue_ = std::max(uploadWaitValue_, mesh.upload.signalValue);
      if (!mesh.pinned) assets_.release(mesh.handle);
      mesh.handle = {};
      tableData<MeshGpu>(meshTable_)[id].ready = 1;
      meshTable_.dirty = true;
    }
    mesh.wanted = !streamingGeometry() || mesh.pinned || mesh.effect;
    mesh.distance = mesh.wanted ? 0.0f : 1e30f;
    mesh.selected = false;
  }
  // A 16-world-unit prefetch border includes adjacent regions and nearby shadow
  // casters. Sphere tests retain whole trees even when their anchor is off screen.
  if (streamingGeometry() && instancesSet_ && regionViewSet_) {
    glm::vec4 rows[4];
    for (int r = 0; r < 4; ++r) rows[r] = {regionView_[0][r], regionView_[1][r], regionView_[2][r], regionView_[3][r]};
    const glm::vec4 planes[] = {rows[3]+rows[0], rows[3]-rows[0], rows[3]+rows[1], rows[3]-rows[1], rows[2], rows[3]-rows[2]};
    const InstanceGpu* instances = allInstances_.data();
    for (uint32_t i = 0; i < instanceCount_; ++i) {
      const InstanceGpu& inst = instances[i];
      if (inst.mesh >= meshes_.size()) continue;
      Mesh& mesh = *meshes_[inst.mesh];
      if (!mesh.parsed || mesh.failed) continue;
      const glm::vec4 local{mesh.header.boundsCenter[0], mesh.header.boundsCenter[1], mesh.header.boundsCenter[2], 1.0f};
      const glm::vec3 centre{glm::dot(inst.rows[0], local), glm::dot(inst.rows[1], local), glm::dot(inst.rows[2], local)};
      const float radius = mesh.header.boundsRadius * std::abs(inst.scale);
      bool inside = true;
      for (const glm::vec4& p : planes)
        if (glm::dot(glm::vec3(p), centre) + p.w < -(radius + 16.0f) * glm::length(glm::vec3(p))) { inside = false; break; }
      // Explicit shadow-only architecture remains resident.
      if (!inside && !(inst.flags & kInstanceShadowOnly)) continue;
      mesh.wanted = true;
      mesh.distance = std::min(mesh.distance, std::max(0.0f, glm::length(centre - eye_) - radius));
    }
  }
  std::vector<MeshId> order;
  for (MeshId id = 0; id < meshes_.size(); ++id) {
    Mesh& mesh = *meshes_[id];
    if (!mesh.parsed || mesh.failed) continue;
    if (mesh.wanted) mesh.lastWanted = frame_;
    if (mesh.wanted || (mesh.uploaded && frame_ - mesh.lastWanted < 120)) order.push_back(id);
  }
  std::stable_sort(order.begin(), order.end(), [&](MeshId a, MeshId b) {
    const Mesh& x = *meshes_[a]; const Mesh& y = *meshes_[b];
    const bool xp = x.pinned || x.effect, yp = y.pinned || y.effect;
    if (xp != yp) return xp;
    if (x.wanted != y.wanted) return x.wanted;
    return x.distance < y.distance;
  });
  uint64_t pinnedBytes = 0, largestWanted = 0;
  for (MeshId id : order) {
    const Mesh& mesh = *meshes_[id];
    if (mesh.pinned || mesh.effect) pinnedBytes += mesh.bytes;
    else if (mesh.wanted) largestWanted = std::max(largestWanted, mesh.bytes);
  }
  // A large tree must still be admissible beside small pinned spell/architecture
  // meshes. The nominal working-set budget is not a permanent asset-size limit.
  const uint64_t regionBudget = std::max(geometryBudget_, pinnedBytes + largestWanted);
  uint64_t selectedBytes = 0;
  for (MeshId id : order) {
    Mesh& mesh = *meshes_[id];
    // A single oversized model can occupy the region by itself; it is never
    // permanently marked failed merely because the scene exceeds its budget.
    if (selectedBytes && selectedBytes + mesh.bytes > regionBudget) continue;
    mesh.selected = true;
    selectedBytes += mesh.bytes;
  }
  for (MeshId id = 0; id < meshes_.size(); ++id) {
    Mesh& mesh = *meshes_[id];
    if (!mesh.selected) retireGeometry(mesh, id);
    if (!mesh.selected && !mesh.geometry.buffer && mesh.handle && mesh.parsed) {
      assets_.release(mesh.handle);
      mesh.handle = {};
    }
  }
  uint64_t started = 0;
  for (MeshId id : order) {
    if (started >= kGeometryBytesPerFrame) break;
    Mesh& mesh = *meshes_[id];
    if (!mesh.selected || mesh.geometry.buffer) continue;
    if (geometryUsed_ && geometryUsed_ + mesh.bytes > regionBudget) continue;
    if (!mesh.pinned && !mesh.handle) mesh.handle = assets_.load(mesh.asset, kLoadPriority);
    const auto bytes = mesh.pinned
        ? std::span<const std::byte>(reinterpret_cast<const std::byte*>(mesh.built.data()), mesh.built.size())
        : assets_.data(mesh.handle);
    if (bytes.empty()) continue;
    if (bytes.size() != mesh.bytes) {
      logError("meshes: geometry changed size while reloading {:016x}", mesh.asset);
      mesh.failed = true;
      continue;
    }
    // Leave headroom for images and transfers; admission is retried next frame.
    if (freeGpuBytes() < mesh.bytes + (64ull << 20)) {
      if (streamTrace() && frame_ % 120 == 0) logWarn("stream blocked {}: needs {:.2f} MB + 64 MB headroom, {:.2f} MB free", assets_.name(mesh.asset), mesh.bytes / 1048576.0, freeGpuBytes() / 1048576.0);
      continue;
    }
    if (streamTrace()) logInfo("stream request {}: {:.2f} MB, distance {:.2f}", assets_.name(mesh.asset), mesh.bytes / 1048576.0, mesh.distance);
    mesh.geometry = createDeviceBuffer(context_, mesh.bytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, true);
    geometryUsed_ += mesh.bytes;
    mesh.upload = {};
    mesh.upload.buffer = mesh.geometry.buffer;
    mesh.upload.data = bytes;
    uploads_.enqueue(mesh.upload);
    const MeshSections s = meshSections(mesh.header);
    const uint64_t base = bufferAddress(context_, mesh.geometry);
    MeshGpu& record = tableData<MeshGpu>(meshTable_)[id];
    record.vertices = base + s.vertices;
    record.clusterVertices = base + s.clusterVertices;
    record.triangles = base + s.clusterTriangles;
    record.clusters = base + s.clusters;
    record.groups = base + s.groups;
    record.nodes = base + s.nodes;
    record.ready = 0;
    meshTable_.dirty = true;
    started += mesh.bytes;
  }
}

void MeshScene::update() {
  for (MeshId id = 0; id < meshes_.size(); ++id) {
    Mesh& mesh = *meshes_[id];
    if (mesh.failed || mesh.parsed) continue;
    const AssetState state = assets_.state(mesh.handle);
    if (state == AssetState::Failed) {
      logError("meshes: {:016x} is missing", mesh.asset);
      mesh.failed = true;
    } else if (state == AssetState::Ready) {
      parse(mesh, id);
    }
  }
  updateGeometry();
  for (auto& [id, texture] : textures_) updateTexture(*texture);
  if ((!gated_ || open_) && frame_ % 8 == 0) stream();
  // Retired images leave once the active material table no longer names them and no frame in flight can read them.
  std::erase_if(retired_, [&](Retired& r) {
    if (materialTable_.version < r.freeAtVersion) return false;
    if (r.safeFrame == UINT64_MAX) r.safeFrame = frame_ + kFramesInFlight + 2;
    if (frame_ < r.safeFrame) return false;
    if (r.index != kNoTextureIndex) bindless_.removeSampledImage(r.index);
    destroyImage(context_, r.image);
    return true;
  });
  // A changed table is re-sent up to its last used entry.
  meshTable_.usedBytes = meshes_.size() * sizeof(MeshGpu);
  materialTable_.usedBytes = uint64_t{materialCount_} * sizeof(MaterialGpu);
  // The GPU's instance list: only the instances whose mesh geometry is resident (2026-10-30: an instance placed before its geometry
  // was on the GPU was never drawn once it arrived: the Monastery's walls and props gone with the region streaming), made again
  // whenever the list or a mesh's residency changes; each new list also redraws the sun's cached shadow with what arrived.
  if (instancesDirty_) {
    instancesDirty_ = false;
    InstanceGpu* out = tableData<InstanceGpu>(instanceTable_);
    uint32_t n = 0;
    for (const InstanceGpu& inst : allInstances_)
      if (inst.mesh < meshes_.size() && meshes_[inst.mesh]->uploaded) out[n++] = inst;
    gpuInstanceCount_ = n;
    instanceTable_.dirty = true;
    if (streamTrace()) {
      uint32_t up = 0, pinnedUp = 0, pinned = 0;
      for (const auto& m : meshes_) up += m->uploaded, pinned += m->pinned, pinnedUp += m->pinned && m->uploaded;
      logInfo("stream instances: {} of {} on the GPU list, meshes {} of {} resident (built {} of {})", n, instanceCount_, up, meshes_.size(), pinnedUp, pinned);
    }
  }
  instanceTable_.usedBytes = uint64_t{gpuInstanceCount_} * sizeof(InstanceGpu);
  updateTable(meshTable_);
  updateTable(materialTable_);
  updateTable(instanceTable_);
  ++frame_;
}

MeshSceneView MeshScene::view() const {
  MeshSceneView v;
  if (meshTable_.active < 0 || materialTable_.active < 0 || instanceTable_.active < 0) return v;
  v.meshes = bufferAddress(context_, meshTable_.copies[meshTable_.active]);
  v.materials = bufferAddress(context_, materialTable_.copies[materialTable_.active]);
  v.instances = bufferAddress(context_, instanceTable_.copies[instanceTable_.active]);
  v.instanceCount = static_cast<uint32_t>(instanceTable_.activeBytes / sizeof(InstanceGpu));
  v.meshCount = static_cast<uint32_t>(meshTable_.activeBytes / sizeof(MeshGpu));
  v.instancesVersion = instanceTable_.version;
  return v;
}

MeshScene::Stats MeshScene::stats() const {
  Stats s;
  s.meshes = static_cast<uint32_t>(meshes_.size());
  for (const auto& mesh : meshes_) s.meshesReady += mesh->uploaded ? 1 : 0;
  s.textures = static_cast<uint32_t>(textures_.size());
  for (const auto& [id, texture] : textures_) {
    s.texturesReady += texture->index != kNoTextureIndex ? 1 : 0;
    s.texturesFull += texture->known && texture->skip == capSkip(*texture) ? 1 : 0;
    s.texturesStreaming += texture->target != UINT32_MAX ? 1 : 0;
  }
  s.instances = instanceCount_;
  s.geometryBytes = geometryUsed_;
  s.textureBytes = textureBytes_;
  return s;
}

}  // namespace ember
