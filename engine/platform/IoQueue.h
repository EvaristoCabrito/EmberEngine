#pragma once

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <thread>
#include <vector>

#include "core/Function.h"

namespace ember {

class JobSystem;

// An open file for asynchronous reads.
struct FileHandle {
  void* native = nullptr;  // Windows HANDLE
  uint64_t size = 0;
  bool unbuffered = false;
  explicit operator bool() const { return native != nullptr; }
};

struct IoResult {
  bool ok = false;
  uint32_t bytesRead = 0;  // may be less than requested at the end of the file
};

using IoCallback = InlineFunction<void(const IoResult&)>;

// Asynchronous file reads on Windows I/O completion ports. read() only queues the request (cheap,
// any thread); a dedicated I/O thread keeps up to `maxInFlight` reads with the OS at once and hands
// each finished read to the job system, where its callback runs. No game thread ever waits on a disk.
class IoQueue {
 public:
  // Unbuffered reads bypass the OS file cache (streaming archives: data is read once and the game
  // owns the memory). Their offset, size and destination must be multiples of kSectorAlignment.
  static constexpr uint32_t kSectorAlignment = 4096;

  IoQueue(JobSystem& jobs, uint32_t maxInFlight = 64);
  ~IoQueue();  // waits for every queued read to finish
  IoQueue(const IoQueue&) = delete;
  IoQueue& operator=(const IoQueue&) = delete;

  FileHandle open(const std::filesystem::path& path, bool unbuffered);
  void close(FileHandle& file);

  void read(const FileHandle& file, uint64_t offset, uint32_t size, void* destination, IoCallback done);

  struct Stats {
    uint32_t inFlight = 0;      // with the OS right now
    uint32_t queued = 0;        // waiting for an OS slot
    uint64_t completed = 0;     // reads finished since start
    uint64_t bytesRead = 0;     // since start
  };
  Stats stats() const;

 private:
  struct Request;
  void ioThreadMain();
  void issue(Request* request);
  void finish(Request* request, const IoResult& result);

  JobSystem& jobs_;
  const uint32_t maxInFlight_;
  void* port_ = nullptr;  // completion port

  mutable std::mutex queueMutex_;
  std::vector<Request*> queue_;  // FIFO waiting for an OS slot, consumed from queueHead_
  size_t queueHead_ = 0;

  std::atomic<uint32_t> inFlight_{0};
  std::atomic<uint32_t> outstanding_{0};  // queued + in flight + callbacks not yet run
  std::atomic<uint64_t> completed_{0};
  std::atomic<uint64_t> bytesRead_{0};
  std::thread thread_;
};

}  // namespace ember
