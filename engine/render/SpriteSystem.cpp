#include "render/SpriteSystem.h"

#include <algorithm>
#include <cstring>
#include <string>

#include "rhi/Bindless.h"
#include "rhi/VulkanContext.h"

namespace ember {

namespace {

constexpr int32_t kStartupPriority = 200;           // roster, headers and tails before anything else
constexpr uint64_t kRetireFrames = kFramesInFlight + 1;  // frames before a dropped texture is destroyed
constexpr uint64_t kAbandonFrames = 120;            // a load nobody asked for in this long is dropped
constexpr VkDeviceSize kPoolBlockBytes = 64ull << 20;  // full-clip pool block (or the largest clip, if bigger)

uint64_t clipBytes(const SpriteClipHeader& h, uint32_t firstMip) {
  uint64_t bytes = 0;
  for (uint32_t m = firstMip; m < h.mipCount; ++m) bytes += bc7MipBytes(h.width, h.height, m, h.frames);
  return bytes;
}

}  // namespace

SpriteSystem::SpriteSystem(VulkanContext& context, Bindless& bindless, AssetManager& assets, UploadQueue& uploads,
                           const Budget& budget)
    : context_(context), bindless_(bindless), assets_(assets), uploads_(uploads), budget_(budget) {}

SpriteSystem::~SpriteSystem() {
  for (auto& clip : clips_) {
    uploads_.cancel(clip->tailUpload);
    uploads_.cancel(clip->fullUpload);
  }
  vkDeviceWaitIdle(context_.device);  // copies and draws may still use the images
  for (Retired& r : retired_) {
    destroyImage(context_, r.image);
    if (r.index != kNoTexture) bindless_.removeSampledImage(r.index);
  }
  for (auto& clip : clips_) {
    destroyImage(context_, clip->tailImage);
    destroyImage(context_, clip->fullImage);
    if (clip->tailIndex != kNoTexture) bindless_.removeSampledImage(clip->tailIndex);
    if (clip->fullIndex != kNoTexture) bindless_.removeSampledImage(clip->fullIndex);
    assets_.release(clip->headerHandle);
    assets_.release(clip->tailHandle);
    assets_.release(clip->fullHandle);
  }
  if (fullPool_) vmaDestroyPool(context_.allocator, fullPool_);
  assets_.release(rosterHandle_);
}

void SpriteSystem::start() {
  rosterHandle_ = assets_.load(assetId("sprites/roster"), kStartupPriority);
  started_ = true;
}

SpriteClipId SpriteSystem::find(std::string_view set, std::string_view clip) const {
  const auto it = byAsset_.find(assetId(std::string("sprites/") + std::string(set) + "/" + std::string(clip) + ".clip"));
  return it == byAsset_.end() ? kNoSpriteClip : it->second;
}

void SpriteSystem::request(SpriteClipId id, int32_t priority) {
  if (id >= clips_.size()) return;
  Clip& clip = *clips_[id];
  if (!clip.requestedThisFrame) {
    clip.requestedThisFrame = true;
    clip.priority = priority;
    requested_.push_back(id);
  } else {
    clip.priority = std::max(clip.priority, priority);
  }
  clip.lastRequested = frame_;
}

SpriteSystem::Texture SpriteSystem::texture(SpriteClipId id) const {
  if (id >= clips_.size()) return {};
  const Clip& clip = *clips_[id];
  if (clip.state == State::Resident) return {clip.fullIndex, true};
  return {clip.tailIndex, false};
}

ImageDesc SpriteSystem::clipImageDesc(const SpriteClipHeader& h, uint32_t firstMip) const {
  ImageDesc desc;
  desc.format = VK_FORMAT_BC7_SRGB_BLOCK;
  desc.extent = {std::max(1u, h.width >> firstMip), std::max(1u, h.height >> firstMip)};
  desc.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  desc.mipLevels = h.mipCount - firstMip;
  desc.layers = h.frames;
  desc.arrayView = true;
  desc.sharedWithTransfer = true;
  return desc;
}

void SpriteSystem::createFullPool() {
  // The real size of each full clip image (with the driver's padding), to size the blocks.
  VkDeviceSize largest = 0;
  const SpriteClipHeader* largestHeader = nullptr;
  for (const auto& clip : clips_) {
    if (!clip->headerReady) continue;
    uint32_t families[2];
    const VkImageCreateInfo info = imageCreateInfo(context_, clipImageDesc(clip->header, 0), families);
    VkDeviceImageMemoryRequirements query{VK_STRUCTURE_TYPE_DEVICE_IMAGE_MEMORY_REQUIREMENTS};
    query.pCreateInfo = &info;
    VkMemoryRequirements2 req{VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2};
    vkGetDeviceImageMemoryRequirements(context_.device, &query, &req);
    clip->memoryBytes = req.memoryRequirements.size;
    clip->memoryAlignment = req.memoryRequirements.alignment;
    if (req.memoryRequirements.size > largest) {
      largest = req.memoryRequirements.size;
      largestHeader = &clip->header;
    }
  }
  if (!largestHeader) return;

  uint32_t families[2];
  const VkImageCreateInfo info = imageCreateInfo(context_, clipImageDesc(*largestHeader, 0), families);
  VmaAllocationCreateInfo alloc{};
  alloc.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
  uint32_t memoryType = 0;
  VK_CHECK(vmaFindMemoryTypeIndexForImageInfo(context_.allocator, &info, &alloc, &memoryType));

  // Budget plus one block of slack, all allocated now; a fragmented pool opens holes (makeHole).
  // Measured (512 MB budget): 64 MB blocks stream as well as a pool 50% over budget.
  const VkDeviceSize block = std::max(kPoolBlockBytes, largest);
  const size_t blocks = static_cast<size_t>(budget_.gpuBytes / block + 1);
  VmaPoolCreateInfo poolInfo{};
  poolInfo.memoryTypeIndex = memoryType;
  poolInfo.blockSize = block;
  poolInfo.minBlockCount = blocks;
  poolInfo.maxBlockCount = blocks;
  const VkResult result = vmaCreatePool(context_.allocator, &poolInfo, &fullPool_);
  if (result != VK_SUCCESS) {
    logError("sprites: cannot reserve {} MB of GPU memory for full clips ({}); drawing tails only",
             (block * blocks) >> 20, string_VkResult(result));
    fullPool_ = VK_NULL_HANDLE;
    return;
  }
  fullPoolBlock_ = block;
  fullPoolBytes_ = block * blocks;
  logInfo("sprites: full-clip pool {} x {} MB (largest clip {:.1f} MB)", blocks, block >> 20, largest / 1048576.0);
}

void SpriteSystem::retire(Image& image, uint32_t& index) {
  retired_.push_back({image, index, frame_});
  image = {};
  index = kNoTexture;
}

void SpriteSystem::updateStartup() {
  // 1. The roster lists every clip.
  if (clips_.empty() && rosterHandle_) {
    const AssetState state = assets_.state(rosterHandle_);
    if (state == AssetState::Failed) {
      logError("sprites: no sprites/roster in the mounted archives");
      assets_.release(rosterHandle_);
      rosterHandle_ = {};
      return;
    }
    if (state != AssetState::Ready) return;
    const std::span<const std::byte> bytes = assets_.data(rosterHandle_);
    SpriteRosterHeader roster{};
    if (bytes.size() >= sizeof(roster)) std::memcpy(&roster, bytes.data(), sizeof(roster));
    if (roster.magic != kSpriteRosterMagic || roster.version != kSpriteFormatVersion ||
        bytes.size() < sizeof(roster) + uint64_t{roster.clipCount} * sizeof(AssetId)) {
      logError("sprites: damaged roster");
    } else {
      clips_.reserve(roster.clipCount);
      for (uint32_t i = 0; i < roster.clipCount; ++i) {
        auto clip = std::make_unique<Clip>();
        std::memcpy(&clip->clipAsset, bytes.data() + sizeof(roster) + i * sizeof(AssetId), sizeof(AssetId));
        clip->headerHandle = assets_.load(clip->clipAsset, kStartupPriority);
        byAsset_[clip->clipAsset] = static_cast<SpriteClipId>(clips_.size());
        clips_.push_back(std::move(clip));
      }
      headersPending_ = roster.clipCount;
      rosterRead_ = true;
      logInfo("sprites: roster lists {} clips", roster.clipCount);
    }
    assets_.release(rosterHandle_);
    rosterHandle_ = {};
  }

  for (auto& clipPtr : clips_) {
    Clip& clip = *clipPtr;
    // 2. Clip headers, then their tails.
    if (!clip.headerReady && clip.headerHandle) {
      const AssetState state = assets_.state(clip.headerHandle);
      if (state == AssetState::Ready || state == AssetState::Failed) {
        const std::span<const std::byte> bytes = assets_.data(clip.headerHandle);
        if (state == AssetState::Ready && bytes.size() == sizeof(SpriteClipHeader)) {
          std::memcpy(&clip.header, bytes.data(), sizeof(SpriteClipHeader));
          clip.headerReady = clip.header.magic == kSpriteClipMagic && clip.header.version == kSpriteFormatVersion;
        }
        if (!clip.headerReady) logError("sprites: clip {:016x} has no valid header", clip.clipAsset);
        assets_.release(clip.headerHandle);
        clip.headerHandle = {};
        --headersPending_;
        if (headersPending_ == 0) createFullPool();
        if (clip.headerReady) {
          clip.tailHandle = assets_.load(clip.header.tailAsset, kStartupPriority);
          ++tailsPending_;
        }
      }
    }
    // 3. A tail that arrived goes to the GPU; one that landed there becomes drawable.
    if (clip.tailHandle && !clip.tailImage.image) {
      const AssetState state = assets_.state(clip.tailHandle);
      const std::span<const std::byte> bytes = assets_.data(clip.tailHandle);
      if (state == AssetState::Failed || (state == AssetState::Ready && bytes.size() != clipBytes(clip.header, clip.header.tailFirstMip))) {
        logError("sprites: tail of {} is missing or damaged", clip.header.name);
        assets_.release(clip.tailHandle);
        clip.tailHandle = {};
        --tailsPending_;
      } else if (state == AssetState::Ready) {
        clip.tailImage = createImage(context_, clipImageDesc(clip.header, clip.header.tailFirstMip));
        clip.tailUpload = {clip.tailImage.image, clip.tailImage.extent.width, clip.tailImage.extent.height,
                           clip.header.mipCount - clip.header.tailFirstMip, clip.header.frames, kBc7BlockBytes, bytes};
        uploads_.enqueue(clip.tailUpload);
        tailBytes_ += bytes.size();
      }
    }
    if (clip.tailImage.image && clip.tailIndex == kNoTexture && uploads_.finished(clip.tailUpload)) {
      clip.tailIndex = bindless_.addSampledImage(clip.tailImage.view);
      uploadWaitValue_ = std::max(uploadWaitValue_, clip.tailUpload.signalValue);
      assets_.release(clip.tailHandle);
      clip.tailHandle = {};
      --tailsPending_;
    }
  }
}

void SpriteSystem::evict(Clip& clip) {
  retire(clip.fullImage, clip.fullIndex);
  clip.state = State::Idle;
  residentBytes_ -= clip.header.fullBytes;
  ++evictions_;
}

bool SpriteSystem::makeRoom(uint64_t bytes) {
  if (bytes > budget_.gpuBytes) return false;
  while (residentBytes_ + bytes > budget_.gpuBytes) {
    // Least recently drawn clip that nobody asked for this frame.
    Clip* victim = nullptr;
    for (auto& clip : clips_)
      if (clip->state == State::Resident && !clip->requestedThisFrame && (!victim || clip->lastRequested < victim->lastRequested))
        victim = clip.get();
    if (!victim) return false;
    evict(*victim);
  }
  return true;
}

void SpriteSystem::makeHole(const Clip& want) {
  // Where everything sits in the pool. Blind LRU eviction frees bytes scattered over the blocks and
  // the clip still does not fit; instead find a run of neighbours whose removal leaves a hole for it.
  enum Kind : uint8_t { Evictable, Leaving, Pinned };  // Leaving: dropped, freed within a few frames
  struct Span {
    VkDeviceSize offset, size;
    Kind kind;
    Clip* clip;
  };
  std::unordered_map<VkDeviceMemory, std::vector<Span>> blocks;
  const auto add = [&](VmaAllocation allocation, Kind kind, Clip* clip) {
    VmaAllocationInfo info;
    vmaGetAllocationInfo(context_.allocator, allocation, &info);
    blocks[info.deviceMemory].push_back({info.offset, info.size, kind, clip});
  };
  for (auto& clip : clips_)
    if (clip->fullImage.allocation)
      add(clip->fullImage.allocation, clip->state == State::Resident && !clip->requestedThisFrame ? Evictable : Pinned, clip.get());
  for (const Retired& r : retired_)
    if (r.image.allocation) add(r.image.allocation, Leaving, nullptr);

  // Best run: its most recently drawn victim is the oldest possible (LRU across runs), then the
  // fewest bytes. A run of nothing but leaving textures costs nothing: just wait for them.
  struct Run {
    std::vector<Span>* spans = nullptr;
    size_t first = 0, end = 0;
    uint64_t newest = 0, bytes = 0;
  } best;
  bool found = false;
  for (auto& [memory, spans] : blocks) {
    std::sort(spans.begin(), spans.end(), [](const Span& a, const Span& b) { return a.offset < b.offset; });
    for (size_t first = 0; first <= spans.size(); ++first) {
      if (first < spans.size() && spans[first].kind == Pinned) continue;
      const VkDeviceSize start = first == 0 ? 0 : spans[first - 1].offset + spans[first - 1].size;
      uint64_t newest = 0, bytes = 0;
      for (size_t end = first; end <= spans.size(); ++end) {
        const VkDeviceSize limit = end == spans.size() ? fullPoolBlock_ : spans[end].offset;
        const VkDeviceSize aligned = (start + want.memoryAlignment - 1) / want.memoryAlignment * want.memoryAlignment;
        if (aligned + want.memoryBytes <= limit) {
          if (!found || newest < best.newest || (newest == best.newest && bytes < best.bytes)) {
            best = {&spans, first, end, newest, bytes};
            found = true;
          }
          break;  // a longer run only evicts more
        }
        if (end == spans.size() || spans[end].kind == Pinned) break;
        if (spans[end].kind == Evictable) {
          newest = std::max(newest, spans[end].clip->lastRequested + 1);
          bytes += spans[end].size;
        }
      }
    }
  }
  if (!found) return;  // everything in the way is in use: retry when it is not
  for (size_t i = best.first; i < best.end; ++i) {
    const Span& span = (*best.spans)[i];
    if (span.kind == Evictable) evict(*span.clip);
  }
}

bool SpriteSystem::startUpload(Clip& clip) {
  ImageDesc desc = clipImageDesc(clip.header, 0);
  desc.pool = fullPool_;
  if (!tryCreateImage(context_, desc, clip.fullImage)) {
    // The pool has the bytes but no hole big enough: open one, then retry next frame.
    ++poolRetries_;
    makeHole(clip);
    return false;
  }
  clip.fullUpload = {clip.fullImage.image, clip.header.width, clip.header.height, clip.header.mipCount,
                     clip.header.frames, kBc7BlockBytes, assets_.data(clip.fullHandle)};
  uploads_.enqueue(clip.fullUpload);
  clip.state = State::Uploading;
  return true;
}

void SpriteSystem::update() {
  // Destroy textures the GPU can no longer be using.
  std::erase_if(retired_, [&](Retired& r) {
    if (r.frame + kRetireFrames > frame_) return false;
    destroyImage(context_, r.image);
    if (r.index != kNoTexture) bindless_.removeSampledImage(r.index);
    return true;
  });

  if (!ready()) updateStartup();

  // Progress full-quality clips: loaded -> uploading -> resident.
  for (auto& clipPtr : clips_) {
    Clip& clip = *clipPtr;
    if (clip.state == State::Loading) {
      const AssetState state = assets_.state(clip.fullHandle);
      const std::span<const std::byte> bytes = assets_.data(clip.fullHandle);
      const bool abandoned = !clip.requestedThisFrame && frame_ - clip.lastRequested > kAbandonFrames;
      if (state == AssetState::Failed || abandoned || (state == AssetState::Ready && bytes.size() != clip.header.fullBytes)) {
        if (!abandoned) logError("sprites: full clip {} is missing or damaged", clip.header.name);
        assets_.release(clip.fullHandle);
        clip.fullHandle = {};
        clip.state = State::Idle;
        residentBytes_ -= clip.header.fullBytes;
      } else if (state == AssetState::Ready) {
        startUpload(clip);  // stays Loading (and retries next frame) while the pool has no room
      }
    } else if (clip.state == State::Uploading && uploads_.finished(clip.fullUpload)) {
      clip.fullIndex = bindless_.addSampledImage(clip.fullImage.view);
      uploadWaitValue_ = std::max(uploadWaitValue_, clip.fullUpload.signalValue);
      assets_.release(clip.fullHandle);  // the CPU copy is no longer needed
      clip.fullHandle = {};
      clip.state = State::Resident;
    }
  }

  // Start what was asked for, most urgent first, within the budget.
  std::sort(requested_.begin(), requested_.end(),
            [&](SpriteClipId a, SpriteClipId b) { return clips_[a]->priority > clips_[b]->priority; });
  for (const SpriteClipId id : requested_) {
    Clip& clip = *clips_[id];
    if (clip.state != State::Idle || !clip.headerReady || !fullPool_) continue;
    if (!makeRoom(clip.header.fullBytes)) {
      ++starved_;
      continue;
    }
    clip.fullHandle = assets_.load(clip.header.fullAsset, clip.priority);
    clip.state = State::Loading;
    residentBytes_ += clip.header.fullBytes;
  }
  for (const SpriteClipId id : requested_) clips_[id]->requestedThisFrame = false;
  requested_.clear();
  ++frame_;
}

SpriteSystem::Stats SpriteSystem::stats() const {
  Stats s;
  s.clips = static_cast<uint32_t>(clips_.size());
  for (const auto& clip : clips_) {
    if (clip->state == State::Resident) ++s.resident;
    else if (clip->state == State::Loading) ++s.loading;
    else if (clip->state == State::Uploading) ++s.uploading;
  }
  s.residentBytes = residentBytes_;
  s.budgetBytes = budget_.gpuBytes;
  s.tailBytes = tailBytes_;
  s.evictions = evictions_;
  s.starvedRequests = starved_;
  s.poolBytes = fullPoolBytes_;
  s.poolRetries = poolRetries_;
  return s;
}

}  // namespace ember
