#include "core/Memory.h"

#include <immintrin.h>
#include <windows.h>

#include "core/Log.h"

namespace ember {

namespace {

size_t pageSize() {
  static const size_t size = [] {
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    return static_cast<size_t>(info.dwPageSize);
  }();
  return size;
}

}  // namespace

void* osAllocate(size_t bytes) { return VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE); }

void osFree(void* pointer) {
  if (pointer) VirtualFree(pointer, 0, MEM_RELEASE);
}

// --- BlockPool -----------------------------------------------------------------------------------

BlockPool::BlockPool(size_t blockSize, size_t blocksPerChunk)
    : blockSize_((blockSize + pageSize() - 1) / pageSize() * pageSize()), blocksPerChunk_(blocksPerChunk) {}

BlockPool::~BlockPool() {
  for (void* chunk : chunks_) osFree(chunk);
}

void BlockPool::lock() {
  while (lock_.test_and_set(std::memory_order_acquire))
    while (lock_.test(std::memory_order_relaxed)) _mm_pause();
}

void* BlockPool::acquire() {
  lock();
  if (free_.empty()) {
    // Grow by a whole chunk: one OS call per many blocks, and never again once warm.
    auto* chunk = static_cast<std::byte*>(osAllocate(blockSize_ * blocksPerChunk_));
    if (!chunk) {
      unlock();
      return nullptr;
    }
    chunks_.push_back(chunk);
    reserved_.fetch_add(blockSize_ * blocksPerChunk_, std::memory_order_relaxed);
    for (size_t i = blocksPerChunk_; i-- > 0;) free_.push_back(chunk + i * blockSize_);
  }
  void* block = free_.back();
  free_.pop_back();
  unlock();
  return block;
}

void BlockPool::release(void* block) {
  if (!block) return;
  lock();
  free_.push_back(block);
  unlock();
}

// --- Heap ----------------------------------------------------------------------------------------

Heap::Heap() : handle_(HeapCreate(0, 0, 0)) { EMBER_CHECK(handle_, "HeapCreate failed"); }

Heap::~Heap() { HeapDestroy(handle_); }

void* Heap::allocate(size_t bytes) { return HeapAlloc(handle_, 0, bytes ? bytes : 1); }

void Heap::free(void* pointer) {
  if (pointer) HeapFree(handle_, 0, pointer);
}

}  // namespace ember
