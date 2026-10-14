#include "platform/IoQueue.h"

#include <windows.h>

#include "core/Jobs.h"
#include "core/Log.h"

namespace ember {

namespace {

// Completion keys: which kind of packet arrived on the port.
constexpr ULONG_PTR kKeyRead = 0;
constexpr ULONG_PTR kKeyWake = 1;  // new requests were queued
constexpr ULONG_PTR kKeyStop = 2;

constexpr ULONG kCompletionBatch = 64;

}  // namespace

struct IoQueue::Request {
  OVERLAPPED overlapped{};  // completion packets point here; must stay the first member
  HANDLE file = nullptr;
  uint64_t offset = 0;
  uint32_t size = 0;
  void* destination = nullptr;
  IoCallback done;
};

IoQueue::IoQueue(JobSystem& jobs, uint32_t maxInFlight) : jobs_(jobs), maxInFlight_(maxInFlight) {
  port_ = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 1);
  EMBER_CHECK(port_, std::format("CreateIoCompletionPort failed ({})", GetLastError()));
  thread_ = std::thread([this] { ioThreadMain(); });
}

IoQueue::~IoQueue() {
  // Every queued read finishes and its callback runs before the queue goes away.
  while (outstanding_.load(std::memory_order_acquire) > 0) std::this_thread::yield();
  PostQueuedCompletionStatus(port_, 0, kKeyStop, nullptr);
  thread_.join();
  CloseHandle(port_);
}

FileHandle IoQueue::open(const std::filesystem::path& path, bool unbuffered) {
  const DWORD flags = FILE_FLAG_OVERLAPPED | (unbuffered ? FILE_FLAG_NO_BUFFERING : 0);
  HANDLE handle = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, flags, nullptr);
  if (handle == INVALID_HANDLE_VALUE) return {};
  LARGE_INTEGER size{};
  GetFileSizeEx(handle, &size);
  EMBER_CHECK(CreateIoCompletionPort(handle, port_, kKeyRead, 0) == port_,
              std::format("Could not attach {} to the I/O port ({})", path.string(), GetLastError()));
  return {handle, static_cast<uint64_t>(size.QuadPart), unbuffered};
}

void IoQueue::close(FileHandle& file) {
  if (file.native) CloseHandle(file.native);
  file = {};
}

void IoQueue::read(const FileHandle& file, uint64_t offset, uint32_t size, void* destination, IoCallback done) {
  if (file.unbuffered) {
    EMBER_CHECK(offset % kSectorAlignment == 0 && size % kSectorAlignment == 0 &&
                    reinterpret_cast<uintptr_t>(destination) % kSectorAlignment == 0,
                "Unbuffered reads need sector-aligned offset, size and destination");
  }
  auto* request = new Request;
  request->file = file.native;
  request->offset = offset;
  request->size = size;
  request->destination = destination;
  request->done = std::move(done);
  outstanding_.fetch_add(1, std::memory_order_relaxed);

  bool wasEmpty;
  {
    std::lock_guard lock(queueMutex_);
    wasEmpty = queueHead_ == queue_.size();
    queue_.push_back(request);
  }
  // One wake packet per burst of requests, not one per request.
  if (wasEmpty) PostQueuedCompletionStatus(port_, 0, kKeyWake, nullptr);
}

void IoQueue::issue(Request* request) {
  request->overlapped.Offset = static_cast<DWORD>(request->offset);
  request->overlapped.OffsetHigh = static_cast<DWORD>(request->offset >> 32);
  inFlight_.fetch_add(1, std::memory_order_relaxed);
  // Even a read that completes at once posts a packet, so every completion goes through the port.
  if (!ReadFile(request->file, request->destination, request->size, nullptr, &request->overlapped) &&
      GetLastError() != ERROR_IO_PENDING) {
    inFlight_.fetch_sub(1, std::memory_order_relaxed);
    finish(request, {false, 0});
  }
}

void IoQueue::finish(Request* request, const IoResult& result) {
  completed_.fetch_add(1, std::memory_order_relaxed);
  bytesRead_.fetch_add(result.bytesRead, std::memory_order_relaxed);
  jobs_.run([this, request, result] {
    request->done(result);
    delete request;
    outstanding_.fetch_sub(1, std::memory_order_release);
  });
}

void IoQueue::ioThreadMain() {
  SetThreadDescription(GetCurrentThread(), L"Ember I/O");
  OVERLAPPED_ENTRY entries[kCompletionBatch];
  while (true) {
    // Hand queued requests to the OS while there are free slots.
    while (inFlight_.load(std::memory_order_relaxed) < maxInFlight_) {
      Request* next = nullptr;
      {
        std::lock_guard lock(queueMutex_);
        if (queueHead_ == queue_.size()) {
          queue_.clear();
          queueHead_ = 0;
        } else {
          next = queue_[queueHead_++];
        }
      }
      if (!next) break;
      issue(next);
    }

    ULONG count = 0;
    if (!GetQueuedCompletionStatusEx(port_, entries, kCompletionBatch, &count, INFINITE, FALSE)) continue;
    for (ULONG i = 0; i < count; ++i) {
      const OVERLAPPED_ENTRY& entry = entries[i];
      if (entry.lpCompletionKey == kKeyStop) return;
      if (entry.lpCompletionKey == kKeyWake) continue;
      auto* request = reinterpret_cast<Request*>(entry.lpOverlapped);
      inFlight_.fetch_sub(1, std::memory_order_relaxed);
      // Internal holds the NTSTATUS of the read; negative means failure.
      const bool ok = static_cast<LONG>(entry.Internal) >= 0;
      finish(request, {ok, ok ? entry.dwNumberOfBytesTransferred : 0});
    }
  }
}

IoQueue::Stats IoQueue::stats() const {
  Stats s;
  {
    std::lock_guard lock(queueMutex_);
    s.queued = static_cast<uint32_t>(queue_.size() - queueHead_);
  }
  s.inFlight = inFlight_.load(std::memory_order_relaxed);
  s.completed = completed_.load(std::memory_order_relaxed);
  s.bytesRead = bytesRead_.load(std::memory_order_relaxed);
  return s;
}

}  // namespace ember
