#include "assets/AssetManager.h"

#include <zstd.h>

#include <algorithm>
#include <cstring>
#include <thread>

#include "core/Jobs.h"
#include "core/Log.h"

namespace ember {

namespace {

constexpr uint64_t kSector = IoQueue::kSectorAlignment;

uint64_t alignDown(uint64_t value) { return value & ~(kSector - 1); }
uint64_t alignUp(uint64_t value) { return (value + kSector - 1) & ~(kSector - 1); }

// One decompression context per thread, reused for every asset.
ZSTD_DCtx* threadDecompressor() {
  struct Context {
    ZSTD_DCtx* handle = ZSTD_createDCtx();
    ~Context() { ZSTD_freeDCtx(handle); }
  };
  thread_local Context context;
  return context.handle;
}

}  // namespace

// One disk read covering one or more neighbouring assets.
struct AssetManager::ReadBatch {
  uint64_t start = 0;  // file offset of buffer[0]
  void* buffer = nullptr;
  bool pooled = false;  // buffer is a readBlocks_ block (else OS memory)
  std::vector<Slot*> members;
  std::atomic<uint32_t> remaining{0};
};

AssetManager::AssetManager(JobSystem& jobs, IoQueue& io, const Budget& budget)
    : jobs_(jobs), io_(io), budget_(budget), readBlocks_(budget.maxReadBytes) {}

AssetManager::~AssetManager() {
  for (uint32_t index : loadingSlots_)
    while (slot(index).state.load(std::memory_order_acquire) == AssetState::Loading) std::this_thread::yield();
  for (uint32_t i = 0; i < slotCount_; ++i) freeAssetMemory(slot(i));
  for (auto& archive : archives_) io_.close(archive->file);
}

bool AssetManager::mount(const std::filesystem::path& pak) {
  auto archive = std::make_unique<Archive>();
  if (!readPakToc(pak, archive->toc)) return false;
  archive->file = io_.open(pak, true);
  if (!archive->file) {
    logError("pak {}: cannot open for streaming", pak.string());
    return false;
  }
  const uint32_t index = static_cast<uint32_t>(archives_.size());
  for (const PakEntry& entry : archive->toc.entries) {
    CatalogEntry& item = catalog_[entry.id];  // a live slot keeps the version it was loaded from
    item.archive = index;
    item.entry = &entry;
  }
  logInfo("mounted {} ({} assets)", pak.string(), archive->toc.entries.size());
  archives_.push_back(std::move(archive));
  return true;
}

std::vector<AssetId> AssetManager::mountedAssets() const {
  std::vector<AssetId> ids;
  ids.reserve(catalog_.size());
  for (const auto& [id, location] : catalog_) ids.push_back(id);
  return ids;
}

std::string_view AssetManager::name(AssetId id) const {
  const auto it = catalog_.find(id);
  return it == catalog_.end() ? std::string_view{} : archives_[it->second.archive]->toc.name(*it->second.entry);
}

uint32_t AssetManager::allocateSlot() {
  if (!freeSlots_.empty()) {
    const uint32_t index = freeSlots_.back();
    freeSlots_.pop_back();
    return index;
  }
  if (slotCount_ % kSlotsPerChunk == 0) chunks_.push_back(std::make_unique<Slot[]>(kSlotsPerChunk));
  return slotCount_++;
}

void AssetManager::freeAssetMemory(Slot& s) {
  if (s.osMemory) osFree(s.allocation);
  else assetHeap_.free(s.allocation);
  s.allocation = nullptr;
  s.osMemory = false;
}

void AssetManager::releaseReadBuffer(ReadBatch* batch) {
  if (batch->pooled) readBlocks_.release(batch->buffer);
  else osFree(batch->buffer);
}

AssetManager::QueueKey AssetManager::queueKey(uint32_t index) const {
  const Slot& s = slot(index);
  return {s.priority, s.archive, s.entry->offset, index};
}

void AssetManager::freeSlot(uint32_t index) {
  Slot& s = slot(index);
  switch (s.state.load(std::memory_order_acquire)) {
    case AssetState::Queued:
      queue_.erase(queueKey(index));
      statQueued_.fetch_sub(1, std::memory_order_relaxed);
      break;
    case AssetState::Ready:
      statReady_.fetch_sub(1, std::memory_order_relaxed);
      statResident_.fetch_sub(s.size, std::memory_order_relaxed);
      break;
    case AssetState::Failed: statFailed_.fetch_sub(1, std::memory_order_relaxed); break;
    case AssetState::Loading: fatal("freeing an asset slot that is still loading");
  }
  freeAssetMemory(s);
  s.catalog->slot = UINT32_MAX;
  s.catalog = nullptr;
  s.bytes = nullptr;
  s.size = 0;
  s.entry = nullptr;
  ++s.generation;  // invalidates outstanding handles
  freeSlots_.push_back(index);
}

AssetManager::Slot* AssetManager::resolve(AssetHandle handle) const {
  if (handle.index >= slotCount_) return nullptr;
  Slot& s = slot(handle.index);
  return s.generation == handle.generation && s.refs > 0 ? &s : nullptr;
}

AssetHandle AssetManager::load(AssetId id, int32_t priority) {
  const auto found = catalog_.find(id);
  if (found == catalog_.end()) {
    logError("asset {:016x} is in no mounted archive", id);
    return {};
  }
  CatalogEntry& item = found->second;
  if (item.slot != UINT32_MAX) {
    const uint32_t index = item.slot;
    Slot& s = slot(index);
    ++s.refs;
    if (priority > s.priority && s.state.load(std::memory_order_relaxed) == AssetState::Queued) {
      queue_.erase(queueKey(index));
      s.priority = priority;
      queue_.insert(queueKey(index));
    }
    return {index, s.generation};
  }

  const uint32_t index = allocateSlot();
  Slot& s = slot(index);
  s.catalog = &item;
  s.refs = 1;
  s.priority = priority;
  s.archive = item.archive;
  s.entry = item.entry;
  item.slot = index;
  s.state.store(AssetState::Queued, std::memory_order_relaxed);
  statQueued_.fetch_add(1, std::memory_order_relaxed);
  queue_.insert(queueKey(index));
  return {index, s.generation};
}

void AssetManager::release(AssetHandle handle) {
  Slot* s = resolve(handle);
  if (!s || --s->refs > 0) return;
  // A load in flight finishes first; update() frees the slot when it retires it. "In flight" is the loading list, not the
  // state: a worker may already have marked the load Ready while update() has not retired it yet, and freeing it here then
  // let update() free it a second time (2026-10-19: the Vau battle crashed on entry, AssetManager freeSlot twice).
  if (!s->inLoadingList) freeSlot(handle.index);
}

AssetState AssetManager::state(AssetHandle handle) const {
  const Slot* s = resolve(handle);
  return s ? s->state.load(std::memory_order_acquire) : AssetState::Failed;
}

std::span<const std::byte> AssetManager::data(AssetHandle handle) const {
  const Slot* s = resolve(handle);
  if (!s || s->state.load(std::memory_order_acquire) != AssetState::Ready) return {};
  return {s->bytes, static_cast<size_t>(s->size)};
}

void AssetManager::update() {
  // Retire loads that landed since the last frame.
  for (size_t i = 0; i < loadingSlots_.size();) {
    const uint32_t index = loadingSlots_[i];
    Slot& s = slot(index);
    if (s.state.load(std::memory_order_acquire) == AssetState::Loading) {
      ++i;
      continue;
    }
    bytesInFlight_ -= s.inFlightBytes;
    statBytesInFlight_.fetch_sub(s.inFlightBytes, std::memory_order_relaxed);
    s.inFlightBytes = 0;
    loadingSlots_[i] = loadingSlots_.back();
    loadingSlots_.pop_back();
    s.inLoadingList = false;
    if (s.refs == 0) freeSlot(index);  // released while it was loading
  }

  // Build reads from the queue: top priority band only, continuing the elevator sweep from the
  // cursor (wrapping to the band's start), merging entries that sit next to each other on disk.
  std::vector<uint32_t> members;
  while (!queue_.empty() && loadingSlots_.size() < budget_.maxLoadsInFlight) {
    const int32_t band = queue_.begin()->priority;
    auto first = queue_.lower_bound({band, cursorArchive_, cursorOffset_, 0});
    if (first == queue_.end() || first->priority != band) first = queue_.begin();

    members.clear();
    const uint32_t archive = first->archive;
    uint64_t readStart = 0, readEnd = 0, need = 0;
    auto it = first;
    for (; it != queue_.end() && it->priority == band && it->archive == archive; ++it) {
      if (loadingSlots_.size() + members.size() >= budget_.maxLoadsInFlight) break;
      const PakEntry& entry = *slot(it->index).entry;
      const uint64_t start = alignDown(entry.offset), end = alignUp(entry.offset + entry.storedSize);
      uint64_t newEnd = end;
      if (!members.empty()) {
        if (start > readEnd + budget_.maxMergeGap) break;
        newEnd = std::max(readEnd, end);
        if (newEnd - readStart > budget_.maxReadBytes) break;
      } else {
        readStart = start;
      }
      const uint64_t grow = (newEnd - (members.empty() ? readStart : readEnd)) + entry.size;
      // One oversized read may always start alone, so the byte budget can never stall streaming.
      if ((!loadingSlots_.empty() || !members.empty()) && bytesInFlight_ + need + grow > budget_.maxBytesInFlight) break;
      need += grow;
      readEnd = newEnd;
      members.push_back(it->index);
    }
    if (members.empty()) break;  // budget full

    cursorArchive_ = archive;
    cursorOffset_ = slot(members.back()).entry->offset + 1;
    queue_.erase(first, it);
    startBatch(members);
  }
}

void AssetManager::startBatch(std::vector<uint32_t>& members) {
  auto* batch = new ReadBatch;
  uint64_t readEnd = 0;
  batch->start = alignDown(slot(members.front()).entry->offset);
  for (const uint32_t index : members) {
    Slot& s = slot(index);
    const PakEntry& entry = *s.entry;
    const uint64_t end = alignUp(entry.offset + entry.storedSize);
    // Each asset carries its own output bytes; the first also carries the shared read buffer.
    s.inFlightBytes = entry.size + (batch->members.empty() ? end - batch->start : end > readEnd ? end - readEnd : 0);
    readEnd = std::max(readEnd, end);
    bytesInFlight_ += s.inFlightBytes;
    statBytesInFlight_.fetch_add(s.inFlightBytes, std::memory_order_relaxed);
    statQueued_.fetch_sub(1, std::memory_order_relaxed);
    statLoading_.fetch_add(1, std::memory_order_relaxed);
    s.state.store(AssetState::Loading, std::memory_order_relaxed);
    s.inLoadingList = true;
    loadingSlots_.push_back(index);
    batch->members.push_back(&s);
  }
  batch->remaining.store(static_cast<uint32_t>(members.size()), std::memory_order_relaxed);

  const uint64_t readSize = readEnd - batch->start;
  if (readSize > UINT32_MAX) {
    logError("asset {:016x} is larger than one streaming read (4 GB); split it in the cooker",
             slot(members.front()).entry->id);
    finishBatch(batch, {false, 0});
    return;
  }
  batch->pooled = readSize <= readBlocks_.blockSize();
  batch->buffer = batch->pooled ? readBlocks_.acquire() : osAllocate(static_cast<size_t>(readSize));
  EMBER_CHECK(batch->buffer, "out of memory for a streaming read");
  io_.read(archives_[slot(members.front()).archive]->file, batch->start, static_cast<uint32_t>(readSize),
           batch->buffer, [this, batch](const IoResult& result) { finishBatch(batch, result); });
}

void AssetManager::finishBatch(ReadBatch* batch, const IoResult& result) {
  const auto* base = static_cast<const std::byte*>(batch->buffer);
  const auto covered = [&](const PakEntry& entry) {
    return result.ok && result.bytesRead >= entry.offset - batch->start + entry.storedSize;
  };

  if (batch->members.size() == 1) {
    Slot& s = *batch->members.front();
    const PakEntry& entry = *s.entry;
    const bool ok = covered(entry);
    const bool took = finishAsset(s, ok ? base + (entry.offset - batch->start) : nullptr, ok,
                                  batch->pooled ? nullptr : batch->buffer);
    if (!took) releaseReadBuffer(batch);
    delete batch;
    return;
  }

  // Decompress members in parallel; the last one to finish frees the shared buffer.
  for (Slot* member : batch->members) {
    const PakEntry& entry = *member->entry;
    const bool ok = covered(entry);
    const std::byte* stored = ok ? base + (entry.offset - batch->start) : nullptr;
    jobs_.run([this, batch, member, stored, ok] {
      finishAsset(*member, stored, ok, nullptr);
      if (batch->remaining.fetch_sub(1, std::memory_order_acq_rel) == 1) {
        releaseReadBuffer(batch);
        delete batch;
      }
    });
  }
}

bool AssetManager::finishAsset(Slot& s, const std::byte* stored, bool ok, void* takeBuffer) {
  const PakEntry& entry = *s.entry;
  bool tookBuffer = false;
  if (ok && entry.compression == PakCompression::None && takeBuffer) {
    s.allocation = takeBuffer;  // the read buffer becomes the asset: no copy
    s.osMemory = true;
    s.bytes = stored;
    tookBuffer = true;
  } else if (ok) {
    void* out = assetHeap_.allocate(static_cast<size_t>(entry.size));
    EMBER_CHECK(out, "out of memory loading an asset");
    if (entry.compression == PakCompression::None) {
      std::memcpy(out, stored, static_cast<size_t>(entry.size));
    } else {
      const size_t written = ZSTD_decompressDCtx(threadDecompressor(), out, static_cast<size_t>(entry.size), stored,
                                                 static_cast<size_t>(entry.storedSize));
      ok = !ZSTD_isError(written) && written == entry.size;
    }
    if (ok) {
      s.allocation = out;
      s.osMemory = false;
      s.bytes = static_cast<const std::byte*>(out);
    } else {
      assetHeap_.free(out);
    }
  }
  if (ok) s.size = entry.size;

  if (!ok) logError("asset {:016x} failed to load", entry.id);
  statLoading_.fetch_sub(1, std::memory_order_relaxed);
  if (ok) {
    statReady_.fetch_add(1, std::memory_order_relaxed);
    statResident_.fetch_add(s.size, std::memory_order_relaxed);
  } else {
    statFailed_.fetch_add(1, std::memory_order_relaxed);
  }
  statCompleted_.fetch_add(1, std::memory_order_relaxed);
  s.state.store(ok ? AssetState::Ready : AssetState::Failed, std::memory_order_release);  // last touch of the slot
  return tookBuffer;
}

AssetManager::Stats AssetManager::stats() const {
  Stats out;
  out.queued = statQueued_.load(std::memory_order_relaxed);
  out.loading = statLoading_.load(std::memory_order_relaxed);
  out.ready = statReady_.load(std::memory_order_relaxed);
  out.failed = statFailed_.load(std::memory_order_relaxed);
  out.residentBytes = statResident_.load(std::memory_order_relaxed);
  out.bytesInFlight = statBytesInFlight_.load(std::memory_order_relaxed);
  out.loadsCompleted = statCompleted_.load(std::memory_order_relaxed);
  out.readPoolBytes = readBlocks_.reservedBytes();
  return out;
}

}  // namespace ember
