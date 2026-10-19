#pragma once

#include <atomic>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <memory_resource>
#include <set>
#include <span>
#include <unordered_map>
#include <vector>

#include "assets/AssetId.h"
#include "assets/Pak.h"
#include "core/Memory.h"
#include "platform/IoQueue.h"

namespace ember {

class JobSystem;

enum class AssetState : uint8_t {
  Queued,   // waiting for a streaming slot
  Loading,  // reading from disk or decompressing
  Ready,    // data() is valid
  Failed,   // missing from every archive, or the read/decompression failed
};

// Reference to a loaded (or loading) asset. Stale handles are detected, never misread.
struct AssetHandle {
  uint32_t index = UINT32_MAX;
  uint32_t generation = 0;
  explicit operator bool() const { return index != UINT32_MAX; }
};

// Streams assets out of mounted .pak archives. load() is cheap and never blocks: it queues the asset
// by priority. update() (once per frame, main thread) hands the highest-priority loads to the I/O
// queue within an in-flight budget; reads land in the job system, which decompresses them.
// Assets are reference counted; the last release() frees the memory.
//
// Disk order: within one priority, loads are read in file order (an elevator sweep that wraps
// around), and neighbouring assets merge into one large read. The drive sees a few big sequential
// reads instead of thousands of small random ones.
//
// Main-thread API: every call except stats() belongs to the thread that created the manager.
class AssetManager {
 public:
  struct Budget {
    uint32_t maxLoadsInFlight = 4096;
    uint64_t maxBytesInFlight = 256ull << 20;  // bytes being read or decompressed at once
    uint64_t maxReadBytes = 1ull << 20;        // merged reads stop growing here
    uint64_t maxMergeGap = 16ull << 10;        // read through a gap this small rather than split
  };

  AssetManager(JobSystem& jobs, IoQueue& io, const Budget& budget);
  ~AssetManager();  // waits for loads in flight, then frees everything
  AssetManager(const AssetManager&) = delete;
  AssetManager& operator=(const AssetManager&) = delete;

  // Adds an archive. Later mounts override earlier ones for the same id (patches, mods).
  bool mount(const std::filesystem::path& pak);
  // Every asset id in the mounted archives.
  std::vector<AssetId> mountedAssets() const;
  // The asset's source path as cooked ("" if no archive has it).
  std::string_view name(AssetId id) const;

  // Starts loading `id` (or adds a reference if it is already loaded or loading).
  // Higher priority streams first; a later load() with a higher priority raises it.
  // An id no mounted archive has returns an empty handle (state Failed).
  AssetHandle load(AssetId id, int32_t priority = 0);
  void release(AssetHandle handle);

  AssetState state(AssetHandle handle) const;
  // The asset's bytes; empty unless the state is Ready.
  std::span<const std::byte> data(AssetHandle handle) const;

  void update();

  struct Stats {
    uint32_t queued = 0;
    uint32_t loading = 0;
    uint32_t ready = 0;
    uint32_t failed = 0;
    uint64_t residentBytes = 0;     // decompressed asset memory held right now
    uint64_t bytesInFlight = 0;
    uint64_t loadsCompleted = 0;    // since start
    uint64_t readPoolBytes = 0;     // memory reserved for streaming reads (grows to the peak, then stays)
  };
  Stats stats() const;

 private:
  struct CatalogEntry {
    uint32_t archive = 0;
    const PakEntry* entry = nullptr;
    uint32_t slot = UINT32_MAX;  // the live slot for this id, if any
  };

  struct Slot {
    CatalogEntry* catalog = nullptr;  // map nodes never move
    uint32_t generation = 0;
    uint32_t refs = 0;
    int32_t priority = 0;
    std::atomic<AssetState> state{AssetState::Queued};
    const PakEntry* entry = nullptr;
    uint32_t archive = 0;
    void* allocation = nullptr;     // owns the bytes: asset heap, or OS memory if osMemory
    bool osMemory = false;
    const std::byte* bytes = nullptr;
    uint64_t size = 0;
    uint64_t inFlightBytes = 0;     // counted against the budget while loading
    bool inLoadingList = false;     // in loadingSlots_ until update() retires it: only update() may free it then
  };

  struct Archive {
    PakToc toc;
    FileHandle file;
  };

  // Queue order: highest priority first, then disk position.
  struct QueueKey {
    int32_t priority;
    uint32_t archive;
    uint64_t offset;
    uint32_t index;
    auto operator<=>(const QueueKey& other) const {
      if (priority != other.priority) return other.priority <=> priority;
      if (archive != other.archive) return archive <=> other.archive;
      if (offset != other.offset) return offset <=> other.offset;
      return index <=> other.index;
    }
    bool operator==(const QueueKey&) const = default;
  };

  struct ReadBatch;

  static constexpr uint32_t kSlotsPerChunk = 4096;

  Slot& slot(uint32_t index) const { return chunks_[index / kSlotsPerChunk][index % kSlotsPerChunk]; }
  Slot* resolve(AssetHandle handle) const;
  uint32_t allocateSlot();
  void freeSlot(uint32_t index);
  QueueKey queueKey(uint32_t index) const;
  void startBatch(std::vector<uint32_t>& members);
  // Job threads. Slots are passed by pointer: chunks_ may grow on the main thread meanwhile.
  void finishBatch(ReadBatch* batch, const IoResult& result);
  // Returns true if it kept `takeBuffer` (OS memory) as the asset's memory: a big uncompressed asset
  // read on its own needs no copy.
  bool finishAsset(Slot& slot, const std::byte* stored, bool ok, void* takeBuffer);

  void freeAssetMemory(Slot& slot);
  void releaseReadBuffer(ReadBatch* batch);

  JobSystem& jobs_;
  IoQueue& io_;
  Budget budget_;
  BlockPool readBlocks_;  // merged reads up to maxReadBytes; bigger reads get their own OS memory
  Heap assetHeap_;        // loaded asset bytes

  std::vector<std::unique_ptr<Archive>> archives_;
  std::unordered_map<AssetId, CatalogEntry> catalog_;

  std::vector<std::unique_ptr<Slot[]>> chunks_;  // chunked so slots never move while jobs write them
  uint32_t slotCount_ = 0;
  std::vector<uint32_t> freeSlots_;
  std::pmr::unsynchronized_pool_resource queueMemory_;  // queue nodes come from a pool, not the heap
  std::pmr::set<QueueKey> queue_{&queueMemory_};
  uint32_t cursorArchive_ = 0;  // elevator position: next read starts at or after here
  uint64_t cursorOffset_ = 0;

  std::vector<uint32_t> loadingSlots_;  // handed to I/O, not yet seen landing by update()
  uint64_t bytesInFlight_ = 0;

  std::atomic<uint32_t> statQueued_{0}, statLoading_{0}, statReady_{0}, statFailed_{0};
  std::atomic<uint64_t> statResident_{0}, statBytesInFlight_{0}, statCompleted_{0};
};

}  // namespace ember
