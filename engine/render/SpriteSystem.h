#pragma once

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "assets/AssetManager.h"
#include "render/SpriteFormat.h"
#include "rhi/Image.h"
#include "rhi/UploadQueue.h"

namespace ember {

class Bindless;
class VulkanContext;

using SpriteClipId = uint32_t;
inline constexpr SpriteClipId kNoSpriteClip = UINT32_MAX;
inline constexpr uint32_t kNoTexture = UINT32_MAX;

// Character sprite residency. Every clip's small "tail" version stays on the GPU from start to end;
// full-quality clips stream in when asked for and leave, least recently used first, when the GPU
// budget needs room. Nothing ever waits: a clip draws from its tail until its full version lands.
// Pinned clips (pin()) keep a half-resolution base for good instead, so they never fall back to the tail.
//
// Each frame the game says which clips it draws now (and which it will soon) with request(); the
// higher the priority, the sooner it streams. A clip requested this frame is never evicted.
// Main thread only.
class SpriteSystem {
 public:
  struct Budget {
    uint64_t gpuBytes = 1ull << 30;  // full-quality clips (tails are separate and always resident)
  };

  // Priorities for request(): what is on screen beats what is coming.
  static constexpr int32_t kOnScreen = 100;
  static constexpr int32_t kSoon = 50;
  static constexpr int32_t kPinned = 200;

  SpriteSystem(VulkanContext& context, Bindless& bindless, AssetManager& assets, UploadQueue& uploads, const Budget& budget);
  ~SpriteSystem();
  SpriteSystem(const SpriteSystem&) = delete;
  SpriteSystem& operator=(const SpriteSystem&) = delete;

  // Reads the roster (sprites/roster) of the mounted archives and starts loading every tail.
  void start();
  // True once every clip's header and tail are on the GPU.
  bool ready() const { return rosterRead_ && tailsPending_ == 0 && headersPending_ == 0; }
  // How far start() has got: headers and tails on the GPU, out of two per clip (0 / 0 until the roster is read).
  std::pair<uint32_t, uint32_t> startupProgress() const {
    const uint32_t total = rosterRead_ ? static_cast<uint32_t>(clips_.size()) * 2u : 0u;
    const uint32_t pending = std::min(total, headersPending_ + tailsPending_);
    return {total - pending, total};
  }

  SpriteClipId find(std::string_view set, std::string_view clip) const;
  size_t clipCount() const { return clips_.size(); }
  const SpriteClipHeader& header(SpriteClipId id) const { return clips_[id]->header; }

  void request(SpriteClipId id, int32_t priority);

  // A clip held in two layers from now on (a fight's characters: every pose they can play, loaded before the fight
  // opens): its "base" (every mip from kBaseFirstMip, a quarter of the full clip's memory) stays on the GPU for good,
  // in memory of its own, so it never draws from its tail; its full version streams like any clip, asked for by the
  // game only while the card is big enough on screen to read the top mip (isPinned + baseHeight).
  void pin(std::string_view set, std::string_view clip);
  // Pinned clips whose base is on the GPU, out of all pinned (one whose base cannot be made counts as done: logged).
  std::pair<uint32_t, uint32_t> pinProgress() const;
  bool isPinned(SpriteClipId id) const { return id < clips_.size() && clips_[id]->pinned; }
  // Texels of the base's top mip, high: a card drawn taller than this on screen reads the full clip's top mip.
  uint32_t baseHeight(SpriteClipId id) const { return std::max(1u, clips_[id]->header.height >> kBaseFirstMip); }
  static constexpr uint32_t kBaseFirstMip = 1;

  struct Texture {
    uint32_t index = kNoTexture;  // bindless texture-array index; kNoTexture = not drawable yet
    bool full = false;            // false: drawing from the tail
  };
  Texture texture(SpriteClipId id) const;

  // Once per frame, before rendering.
  void update();
  // The renderer must wait for this upload-timeline value before sampling (already signalled when
  // a texture is published, so the wait is free).
  uint64_t uploadWaitValue() const { return uploadWaitValue_; }

  struct Stats {
    uint32_t clips = 0, resident = 0, loading = 0, uploading = 0;
    uint64_t residentBytes = 0;  // full clips on the GPU or on their way (counted against the budget)
    uint64_t budgetBytes = 0;
    uint64_t tailBytes = 0;
    uint64_t evictions = 0;      // since start
    uint64_t starvedRequests = 0;  // requests that found no room in the budget (since start)
    uint64_t poolBytes = 0;        // GPU memory reserved for full clips (budget + one block of slack)
    uint64_t poolRetries = 0;      // image creations that found the pool too fragmented (since start)
    uint64_t baseBytes = 0;        // pinned clips' bases on the GPU (outside the budget)
  };
  Stats stats() const;

 private:
  enum class State : uint8_t { Idle, Loading, Uploading, Resident };

  struct Clip {
    AssetId clipAsset = 0;
    AssetHandle headerHandle;
    bool headerReady = false;
    SpriteClipHeader header{};

    AssetHandle tailHandle;
    Image tailImage;
    ImageUpload tailUpload;
    uint32_t tailIndex = kNoTexture;

    State state = State::Idle;
    AssetHandle fullHandle;
    Image fullImage;
    ImageUpload fullUpload;
    uint32_t fullIndex = kNoTexture;
    VkDeviceSize memoryBytes = 0, memoryAlignment = 1;  // the full image's real GPU footprint

    uint64_t lastRequested = 0;  // frame
    int32_t priority = 0;        // highest asked for this frame
    bool requestedThisFrame = false;

    // Pinned clips: the base layer (mips from kBaseFirstMip, cut from the full asset), never evicted.
    bool pinned = false;
    State baseState = State::Idle;
    bool baseFailed = false;  // could not be made: the clip draws its tail until its full version lands
    AssetHandle baseHandle;
    Image baseImage;
    ImageUpload baseUpload;
    uint32_t baseIndex = kNoTexture;
    bool needsBase() const { return header.tailFirstMip > kBaseFirstMip; }  // else the tail is already as sharp
  };

  struct Retired {
    Image image;
    uint32_t index;
    uint64_t frame;  // destroyed once the GPU is surely past this frame
  };

  ImageDesc clipImageDesc(const SpriteClipHeader& header, uint32_t firstMip) const;
  void createFullPool();
  void evict(Clip& clip);
  void retire(Image& image, uint32_t& index);
  void updateStartup();
  bool makeRoom(uint64_t bytes);
  void makeHole(const Clip& want);
  bool startUpload(Clip& clip);
  void addPin(SpriteClipId id, std::string set);
  void updateBases();

  VulkanContext& context_;
  Bindless& bindless_;
  AssetManager& assets_;
  UploadQueue& uploads_;
  Budget budget_;

  bool started_ = false;
  bool rosterRead_ = false;  // the roster arrived and its clips are listed
  AssetHandle rosterHandle_;
  std::vector<std::unique_ptr<Clip>> clips_;  // stable addresses: uploads point at members
  std::unordered_map<AssetId, SpriteClipId> byAsset_;
  uint32_t headersPending_ = 0, tailsPending_ = 0;

  // Full clips live in one pool allocated up front: creating an image then never asks the driver
  // for memory (a fresh driver allocation cost up to 12 ms on the main thread).
  VmaPool fullPool_ = VK_NULL_HANDLE;
  VkDeviceSize fullPoolBlock_ = 0;
  uint64_t fullPoolBytes_ = 0;

  uint64_t frame_ = 0;
  uint64_t residentBytes_ = 0;
  uint64_t tailBytes_ = 0;
  uint64_t evictions_ = 0, starved_ = 0, poolRetries_ = 0;
  uint64_t uploadWaitValue_ = 0;
  std::vector<SpriteClipId> requested_;  // this frame
  std::vector<std::pair<AssetId, std::string>> pendingPins_;  // pinned before the roster was read (asset, sprite set)
  std::vector<SpriteClipId> pinned_;
  std::vector<std::string> pinnedSet_;   // each pinned clip's sprite set (one character's poses)
  uint64_t baseBytes_ = 0;
  uint64_t closeUps_ = 0;  // full versions of pinned clips that landed (since start)
  bool pinsLogged_ = false;
  std::chrono::steady_clock::time_point pinsAsked_{};
  std::vector<Retired> retired_;
};

}  // namespace ember
