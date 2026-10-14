#include "render/MeshScene.h"

#include <algorithm>
#include <cstring>

#include "render/TextureFormat.h"
#include "rhi/Bindless.h"
#include "rhi/VulkanContext.h"

namespace ember {

namespace {

constexpr int32_t kLoadPriority = 150;
constexpr uint64_t kGeometryAlignment = 256;

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

}  // namespace

MeshScene::MeshScene(VulkanContext& context, Bindless& bindless, AssetManager& assets, UploadQueue& uploads, const Capacity& capacity)
    : context_(context), bindless_(bindless), assets_(assets), uploads_(uploads), capacity_(capacity) {
  geometry_ = createDeviceBuffer(context, capacity.geometryBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, true);
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
    assets_.release(texture->handle);
  }
  for (auto& mesh : meshes_) assets_.release(mesh->handle);
  for (Table* table : {&meshTable_, &materialTable_, &instanceTable_})
    for (Buffer& b : table->copies) destroyBuffer(context_, b);
  destroyBuffer(context_, geometry_);
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

const MeshHeader* MeshScene::header(MeshId id) const {
  return id < meshes_.size() && meshes_[id]->parsed ? &meshes_[id]->header : nullptr;
}

bool MeshScene::ready() const {
  for (const auto& mesh : meshes_)
    if (!mesh->failed && !mesh->uploaded) return false;
  for (const auto& [id, texture] : textures_)
    if (!texture->failed && texture->index == kNoTextureIndex) return false;
  return true;
}

void MeshScene::setInstances(std::span<const InstanceGpu> instances) {
  EMBER_CHECK(instances.size() <= capacity_.instances, "MeshScene: instance capacity reached");
  instanceCount_ = static_cast<uint32_t>(instances.size());
  std::memcpy(instanceTable_.cpu.data(), instances.data(), instances.size_bytes());
  instanceTable_.dirty = true;
}

void MeshScene::parse(Mesh& mesh, MeshId id) {
  const std::span<const std::byte> bytes = assets_.data(mesh.handle);
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
  if (geometryUsed_ + s.end > capacity_.geometryBytes || materialCount_ + h.materialCount > capacity_.materials) {
    logError("meshes: geometry or material capacity reached");
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
    for (int t = 0; t < 4; ++t) *slots[t] = textures[t] ? requestTexture(textures[t], (materialBase + m) * 4 + t) : kNoTextureIndex;
  }
  materialCount_ += h.materialCount;
  materialTable_.dirty = true;

  // Geometry: the whole cooked asset (header included) lands in the geometry buffer; the mesh record
  // points at its sections.
  mesh.offset = geometryUsed_;
  geometryUsed_ = (geometryUsed_ + s.end + kGeometryAlignment - 1) & ~(kGeometryAlignment - 1);
  mesh.upload = {};
  mesh.upload.buffer = geometry_.buffer;
  mesh.upload.bufferOffset = mesh.offset;
  mesh.upload.data = bytes;
  uploads_.enqueue(mesh.upload);

  const uint64_t base = bufferAddress(context_, geometry_) + mesh.offset;
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
}

uint32_t MeshScene::requestTexture(AssetId asset, uint32_t user) {
  auto& slot = textures_[asset];
  if (!slot) {
    slot = std::make_unique<Texture>();
    slot->handle = assets_.load(asset, kLoadPriority);
  }
  slot->users.push_back(user);
  return slot->index;  // kNoTextureIndex until resident
}

void MeshScene::updateTexture(Texture& texture) {
  if (texture.failed || texture.index != kNoTextureIndex) return;
  if (!texture.image.image) {
    const AssetState state = assets_.state(texture.handle);
    if (state == AssetState::Failed) {
      texture.failed = true;
      return;
    }
    if (state != AssetState::Ready) return;
    const std::span<const std::byte> bytes = assets_.data(texture.handle);
    TextureHeader h{};
    if (bytes.size() >= sizeof(h)) std::memcpy(&h, bytes.data(), sizeof(h));
    if (h.magic != kTextureMagic || h.version != kTextureFormatVersion || bytes.size() != sizeof(h) + h.dataBytes) {
      logError("meshes: damaged texture asset");
      texture.failed = true;
      return;
    }
    ImageDesc desc;
    desc.format = vkFormat(h.format);
    desc.extent = {h.width, h.height};
    desc.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    desc.mipLevels = h.mipCount;
    desc.sharedWithTransfer = true;
    texture.image = createImage(context_, desc);
    texture.upload = {texture.image.image, h.width, h.height, h.mipCount, 1, 16, bytes.subspan(sizeof(h))};
    uploads_.enqueue(texture.upload);
    textureBytes_ += h.dataBytes;
    return;
  }
  if (!uploads_.finished(texture.upload)) return;
  texture.index = bindless_.addSampledImage(texture.image.view);
  uploadWaitValue_ = std::max(uploadWaitValue_, texture.upload.signalValue);
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

void MeshScene::updateTable(Table& table) {
  // An upload that landed makes its copy the active one.
  if (table.writing >= 0 && uploads_.finished(table.upload)) {
    table.active = table.writing;
    table.activeBytes = table.snapshot.size();
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

void MeshScene::update() {
  for (MeshId id = 0; id < meshes_.size(); ++id) {
    Mesh& mesh = *meshes_[id];
    if (mesh.failed || mesh.uploaded) continue;
    if (!mesh.parsed) {
      const AssetState state = assets_.state(mesh.handle);
      if (state == AssetState::Failed) {
        logError("meshes: {:016x} is missing", mesh.asset);
        mesh.failed = true;
      } else if (state == AssetState::Ready) {
        parse(mesh, id);
      }
      continue;
    }
    if (uploads_.finished(mesh.upload)) {
      mesh.uploaded = true;
      uploadWaitValue_ = std::max(uploadWaitValue_, mesh.upload.signalValue);
      assets_.release(mesh.handle);  // the GPU copy is the only one needed
      mesh.handle = {};
      tableData<MeshGpu>(meshTable_)[id].ready = 1;
      meshTable_.dirty = true;
    }
  }
  for (auto& [id, texture] : textures_) updateTexture(*texture);
  // A changed table is re-sent up to its last used entry.
  meshTable_.usedBytes = meshes_.size() * sizeof(MeshGpu);
  materialTable_.usedBytes = uint64_t{materialCount_} * sizeof(MaterialGpu);
  instanceTable_.usedBytes = uint64_t{instanceCount_} * sizeof(InstanceGpu);
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
  return v;
}

MeshScene::Stats MeshScene::stats() const {
  Stats s;
  s.meshes = static_cast<uint32_t>(meshes_.size());
  for (const auto& mesh : meshes_) s.meshesReady += mesh->uploaded ? 1 : 0;
  s.textures = static_cast<uint32_t>(textures_.size());
  for (const auto& [id, texture] : textures_) s.texturesReady += texture->index != kNoTextureIndex ? 1 : 0;
  s.instances = instanceCount_;
  s.geometryBytes = geometryUsed_;
  s.textureBytes = textureBytes_;
  return s;
}

}  // namespace ember
