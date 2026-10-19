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
    uploads_.cancel(clip->baseUpload);
  }
  vkDeviceWaitIdle(context_.device);  // copies and draws may still use the images
  for (Retired& r : retired_) {
    destroyImage(context_, r.image);
    if (r.index != kNoTexture) bindless_.removeSampledImage(r.index);
  }
  for (auto& clip : clips_) {
    destroyImage(context_, clip->tailImage);
    destroyImage(context_, clip->fullImage);
    destroyImage(context_, clip->baseImage);
    if (clip->tailIndex != kNoTexture) bindless_.removeSampledImage(clip->tailIndex);
    if (clip->fullIndex != kNoTexture) bindless_.removeSampledImage(clip->fullIndex);
    if (clip->baseIndex != kNoTexture) bindless_.removeSampledImage(clip->baseIndex);
    assets_.release(clip->headerHandle);
    assets_.release(clip->tailHandle);
    assets_.release(clip->fullHandle);
    assets_.release(clip->baseHandle);
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

void SpriteSystem::pin(std::string_view set, std::string_view clip, bool twoVersions) {
  const AssetId asset = assetId(std::string("sprites/") + std::string(set) + "/" + std::string(clip) + ".clip");
  if (pinned_.empty() && pendingPins_.empty()) pinsAsked_ = std::chrono::steady_clock::now();
  if (!rosterRead_) {
    const auto it = std::find_if(pendingPins_.begin(), pendingPins_.end(), [&](const PendingPin& p) { return p.asset == asset; });
    if (it == pendingPins_.end()) pendingPins_.push_back({asset, std::string(set), twoVersions});
    else it->twoVersions = it->twoVersions || twoVersions;
    return;
  }
  const auto it = byAsset_.find(asset);
  if (it != byAsset_.end()) addPin(it->second, std::string(set), twoVersions);
}

void SpriteSystem::addPin(SpriteClipId id, std::string set, bool twoVersions) {
  Clip& c = *clips_[id];
  if (c.pinned) {
    c.twoVersions = c.twoVersions || twoVersions;
    if (decided_) c.light = c.twoVersions && lightActive_ && c.state != State::Resident;
    return;
  }
  c.pinned = true;
  c.twoVersions = twoVersions;
  if (decided_) c.light = twoVersions && lightActive_;  // pinned during the fight (a summon): the fight's choice
  pinned_.push_back(id);
  pinnedSet_.push_back(std::move(set));
  pinsLogged_ = false;
}

std::pair<uint32_t, uint32_t> SpriteSystem::pinProgress() const {
  const uint32_t total = static_cast<uint32_t>(pinned_.size() + pendingPins_.size());
  uint32_t done = 0;
  for (const SpriteClipId id : pinned_) {
    const Clip& c = *clips_[id];
    const bool headerFailed = !c.headerReady && !c.headerHandle;
    if (headerFailed) ++done;
    else if (!decided_) continue;
    else if (c.light) done += c.baseFailed || c.baseState == State::Resident || (!c.needsBase() && c.tailIndex != kNoTexture) ? 1u : 0u;
    else done += c.state == State::Resident || !fullPool_ ? 1u : 0u;  // no pool at all: tails only (logged)
  }
  return {done, total};
}

SpriteSystem::Texture SpriteSystem::texture(SpriteClipId id) const {
  if (id >= clips_.size()) return {};
  const Clip& clip = *clips_[id];
  if (clip.state == State::Resident) return {clip.fullIndex, true};
  if (clip.baseIndex != kNoTexture) return {clip.baseIndex, true};
  return {clip.tailIndex, clip.light && !clip.needsBase()};
}

void SpriteSystem::updateBases() {
  // A Leve clip's base: its full asset read once, the mips from kBaseFirstMip on cut off its end (mip-major, like the
  // tail), uploaded into an image of its own; the CPU copy goes once it is on the GPU.
  for (const SpriteClipId id : pinned_) {
    Clip& c = *clips_[id];
    if (!c.light || !c.headerReady || !c.needsBase() || c.baseFailed) continue;
    if (c.baseState == State::Idle) {
      c.baseHandle = assets_.load(c.header.fullAsset, kPinned);
      c.baseState = State::Loading;
    }
    if (c.baseState == State::Loading) {
      const AssetState state = assets_.state(c.baseHandle);
      const std::span<const std::byte> bytes = assets_.data(c.baseHandle);
      if (state == AssetState::Failed || (state == AssetState::Ready && bytes.size() != c.header.fullBytes)) {
        logError("sprites: full clip {} is missing or damaged: no base, its tail is drawn", c.header.name);
        assets_.release(c.baseHandle);
        c.baseHandle = {};
        c.baseFailed = true;
        c.baseState = State::Idle;
      } else if (state == AssetState::Ready) {
        if (!tryCreateImage(context_, clipImageDesc(c.header, kBaseFirstMip), c.baseImage)) {
          logError("sprites: no GPU memory for the base of {} ({:.1f} MB): its tail is drawn", c.header.name,
                   clipBytes(c.header, kBaseFirstMip) / 1048576.0);
          assets_.release(c.baseHandle);
          c.baseHandle = {};
          c.baseFailed = true;
          c.baseState = State::Idle;
          continue;
        }
        const uint64_t skip = c.header.fullBytes - clipBytes(c.header, kBaseFirstMip);
        c.baseUpload = {c.baseImage.image, c.baseImage.extent.width, c.baseImage.extent.height, c.header.mipCount - kBaseFirstMip,
                        c.header.frames, kBc7BlockBytes, bytes.subspan(skip)};
        uploads_.enqueue(c.baseUpload);
        baseBytes_ += clipBytes(c.header, kBaseFirstMip);
        c.baseState = State::Uploading;
      }
    }
    if (c.baseState == State::Uploading && uploads_.finished(c.baseUpload)) {
      c.baseIndex = bindless_.addSampledImage(c.baseImage.view);
      uploadWaitValue_ = std::max(uploadWaitValue_, c.baseUpload.signalValue);
      assets_.release(c.baseHandle);
      c.baseHandle = {};
      c.baseState = State::Resident;
    }
  }
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

  // The pinned clips (a fight's characters) are loaded whole before the fight opens and held: Alta or Leve for those
  // with two versions (the player's choice), at full resolution for everyone else. Alta that does not fit the GPU's free
  // memory (with a margin) is taken as Leve, logged.
  uint64_t fullOne = 0, fullTwo = 0, halfTwo = 0;
  for (const SpriteClipId id : pinned_) {
    const Clip& c = *clips_[id];
    if (!c.headerReady) continue;
    (c.twoVersions ? fullTwo : fullOne) += c.header.fullBytes;
    if (c.twoVersions) halfTwo += clipBytes(c.header, std::min(kBaseFirstMip, c.header.tailFirstMip));
  }
  const VkPhysicalDeviceMemoryProperties* props = nullptr;
  vmaGetMemoryProperties(context_.allocator, &props);
  VmaBudget budgets[VK_MAX_MEMORY_HEAPS];
  vmaGetHeapBudgets(context_.allocator, budgets);
  const VmaBudget& heap = budgets[props->memoryTypes[memoryType].heapIndex];
  const uint64_t free = heap.budget > heap.usage ? heap.budget - heap.usage : 0;
  constexpr uint64_t kMargin = 256ull << 20;  // the rest of the fight (effects, meshes still landing)
  const uint64_t streaming = budget_.gpuBytes;
  lightActive_ = lightAsked_;
  if (!lightActive_ && fullTwo > 0 && streaming + fullOne + fullTwo + kMargin > free) {
    lightActive_ = true;
    logWarn("sprites: Personagens Alta needs {} MB, the GPU has {} MB free: Leve for this fight", (streaming + fullOne + fullTwo + kMargin) >> 20,
            free >> 20);
  }

  // Budget plus the pinned full clips plus one block of slack, all allocated now; a fragmented pool opens holes
  // (makeHole). Measured (512 MB budget): 64 MB blocks stream as well as a pool 50% over budget.
  const VkDeviceSize block = std::max(kPoolBlockBytes, largest);
  VmaPoolCreateInfo poolInfo{};
  poolInfo.memoryTypeIndex = memoryType;
  poolInfo.blockSize = block;
  const auto createPool = [&](uint64_t bytes) {
    poolInfo.minBlockCount = poolInfo.maxBlockCount = static_cast<size_t>(bytes / block + 1);
    return vmaCreatePool(context_.allocator, &poolInfo, &fullPool_);
  };
  VkResult result = createPool(streaming + fullOne + (lightActive_ ? 0 : fullTwo));
  if (result != VK_SUCCESS && !lightActive_ && fullTwo > 0) {
    logWarn("sprites: no GPU room for Personagens Alta ({}): Leve for this fight", string_VkResult(result));
    lightActive_ = true;
    result = createPool(streaming + fullOne);
  }
  if (result != VK_SUCCESS && fullOne > 0) {
    logError("sprites: no GPU room for the fight's {} MB of full clips ({}); they share the {} MB budget", fullOne >> 20,
             string_VkResult(result), streaming >> 20);
    result = createPool(streaming);
  }
  for (const SpriteClipId id : pinned_) clips_[id]->light = clips_[id]->twoVersions && lightActive_;
  if (!pinned_.empty())
    logInfo("sprites: Personagens {}{}: the cast {} MB at {} resolution, the others {} MB (GPU {} MB free before the fight)",
            lightActive_ ? "Leve" : "Alta", lightActive_ && !lightAsked_ ? " (Alta asked)" : "", (lightActive_ ? halfTwo : fullTwo) >> 20,
            lightActive_ ? "half" : "full", fullOne >> 20, free >> 20);
  const size_t blocks = poolInfo.maxBlockCount;
  if (result != VK_SUCCESS) {
    logError("sprites: cannot reserve {} MB of GPU memory for full clips ({}); drawing tails only",
             (block * blocks) >> 20, string_VkResult(result));
    fullPool_ = VK_NULL_HANDLE;
    return;
  }
  budget_.gpuBytes = streaming + fullOne + (lightActive_ ? 0 : fullTwo);
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
      for (PendingPin& p : pendingPins_) {
        const auto it = byAsset_.find(p.asset);
        if (it != byAsset_.end()) addPin(it->second, std::move(p.set), p.twoVersions);
      }
      pendingPins_.clear();
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
        if (headersPending_ == 0) {
          createFullPool();
          decided_ = true;
        }
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
  if (decided_) {
    // Pinned clips at full resolution are asked for every frame (so never evicted); Leve ones keep their base.
    for (const SpriteClipId id : pinned_)
      if (!clips_[id]->light) request(id, kPinned);
    updateBases();
  }
  if (!pinsLogged_ && !pinned_.empty() && pendingPins_.empty() && decided_) {
    const auto [done, total] = pinProgress();
    if (done >= total) {
      pinsLogged_ = true;
      logInfo("sprites: the fight's {} clips on the GPU {:.2f} s after they were asked for", total,
              std::chrono::duration<double>(std::chrono::steady_clock::now() - pinsAsked_).count());
      // What weighs the most, by sprite set (one character's poses each): what is on the GPU, and the full size.
      std::unordered_map<std::string, std::pair<uint64_t, uint64_t>> bySet;
      for (size_t i = 0; i < pinned_.size(); ++i) {
        const Clip& c = *clips_[pinned_[i]];
        if (!c.headerReady) continue;
        auto& [held, full] = bySet[pinnedSet_[i]];
        held += c.state == State::Resident ? c.header.fullBytes : c.baseState == State::Resident ? clipBytes(c.header, kBaseFirstMip) : 0;
        full += c.header.fullBytes;
      }
      std::vector<std::pair<std::string, std::pair<uint64_t, uint64_t>>> sets(bySet.begin(), bySet.end());
      std::sort(sets.begin(), sets.end(), [](const auto& a, const auto& b) { return a.second.second > b.second.second; });
      for (const auto& [set, bytes] : sets) logInfo("sprites:   {} {} MB (full {} MB)", set, bytes.first >> 20, bytes.second >> 20);
      // The whole game's GPU memory at that moment (VMA's own count where the driver gives no budget): the largest
      // device-local heap.
      const VkPhysicalDeviceMemoryProperties* props = nullptr;
      vmaGetMemoryProperties(context_.allocator, &props);
      VmaBudget budgets[VK_MAX_MEMORY_HEAPS];
      vmaGetHeapBudgets(context_.allocator, budgets);
      uint32_t heap = 0;
      for (uint32_t h = 0; h < props->memoryHeapCount; ++h)
        if ((props->memoryHeaps[h].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) && props->memoryHeaps[h].size > props->memoryHeaps[heap].size) heap = h;
      logInfo("sprites: GPU memory {} MB allocated by the game, {} MB used, budget {} MB, heap {} MB", budgets[heap].statistics.blockBytes >> 20,
              budgets[heap].usage >> 20, budgets[heap].budget >> 20, props->memoryHeaps[heap].size >> 20);
    }
  }

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
  s.baseBytes = baseBytes_;
  return s;
}

}  // namespace ember
