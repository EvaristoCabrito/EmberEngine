#pragma once

#include <atomic>
#include <cstddef>
#include <vector>

namespace ember {

// Memory straight from the OS (page-aligned, zero-filled). For big, long-lived blocks only:
// every call takes the process-wide address-space lock.
void* osAllocate(size_t bytes);
void osFree(void* pointer);

// Fixed-size blocks taken from the OS once and recycled forever. Hot paths (streaming reads) never
// call the OS allocator per use, so they never queue behind other threads on its locks.
// acquire() and release() may be called from any thread.
class BlockPool {
 public:
  // `blockSize` is rounded up to whole pages; blocks are page-aligned.
  explicit BlockPool(size_t blockSize, size_t blocksPerChunk = 16);
  ~BlockPool();
  BlockPool(const BlockPool&) = delete;
  BlockPool& operator=(const BlockPool&) = delete;

  void* acquire();
  void release(void* block);

  size_t blockSize() const { return blockSize_; }
  size_t reservedBytes() const { return reserved_.load(std::memory_order_relaxed); }

 private:
  void lock();
  void unlock() { lock_.clear(std::memory_order_release); }

  size_t blockSize_;
  size_t blocksPerChunk_;
  std::atomic_flag lock_;
  std::vector<void*> free_;
  std::vector<void*> chunks_;
  std::atomic<size_t> reserved_{0};
};

// A private heap: allocations here never contend with the process heap that the game thread,
// containers and libraries use. Thread safe.
class Heap {
 public:
  Heap();
  ~Heap();
  Heap(const Heap&) = delete;
  Heap& operator=(const Heap&) = delete;

  void* allocate(size_t bytes);  // 16-byte aligned; null when out of memory
  void free(void* pointer);

 private:
  void* handle_ = nullptr;
};

}  // namespace ember
