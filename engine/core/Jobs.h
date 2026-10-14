#pragma once

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <memory>
#include <thread>
#include <vector>

#include "core/Function.h"

namespace ember {

using JobFunction = InlineFunction<void()>;

// Counts unfinished jobs. A job group shares one counter; wait() on it to join the group.
class JobCounter {
 public:
  bool done() const { return pending_.load(std::memory_order_acquire) == 0; }

 private:
  friend class JobSystem;
  std::atomic<uint32_t> pending_{0};
};

// Runs work on every CPU core. Each thread owns a queue: it pushes and pops its own work at the back
// (hot in cache), idle threads steal from the front of other queues. Threads waiting on a counter run
// other jobs instead of blocking, so a wait never wastes a core. Idle workers sleep until work arrives.
//
// Thread 0 is the thread that created the system (the main thread); workers are 1..workerCount().
// Any other thread (e.g. the I/O completion thread) may submit jobs too.
class JobSystem {
 public:
  // 0 = one worker per hardware thread, minus the main thread.
  explicit JobSystem(uint32_t workerCount = 0);
  ~JobSystem();
  JobSystem(const JobSystem&) = delete;
  JobSystem& operator=(const JobSystem&) = delete;

  void run(JobFunction job, JobCounter* counter = nullptr);

  // Runs body(begin, end) over [0, count) in batches of `batchSize` and returns when all are done.
  template <class Body>
  void parallelFor(uint32_t count, uint32_t batchSize, const Body& body) {
    if (count == 0) return;
    batchSize = std::max(batchSize, 1u);
    JobCounter counter;
    for (uint32_t begin = 0; begin < count; begin += batchSize) {
      const uint32_t end = std::min(count, begin + batchSize);
      run([&body, begin, end] { body(begin, end); }, &counter);
    }
    wait(counter);
  }

  // Returns once every job counted by `counter` has finished, running other jobs meanwhile.
  void wait(JobCounter& counter);

  uint32_t workerCount() const { return static_cast<uint32_t>(threads_.size()); }
  // 0 on the main thread and on threads that are not part of the system.
  static uint32_t threadIndex();

  struct ThreadStats {
    uint64_t jobsRun = 0;
    uint64_t busyNanoseconds = 0;  // time spent inside jobs
  };
  // Per thread, index = threadIndex(). Totals since start; diff two snapshots for rates.
  std::vector<ThreadStats> stats() const;

 private:
  struct Job {
    JobFunction function;
    JobCounter* counter = nullptr;
  };

  // Growable ring of jobs guarded by a lock. Owner works at the back, thieves at the front.
  // Cache-line aligned so threads never contend on each other's queue memory.
#pragma warning(push)
#pragma warning(disable : 4324)  // padding from alignas is the point
  struct alignas(64) WorkQueue {
    void pushBack(Job&& job);
    bool popBack(Job& out);
    bool popFront(Job& out);

    std::atomic_flag lock;  // short critical sections: a spinlock beats a kernel mutex
    std::vector<Job> ring;
    uint64_t head = 0, tail = 0;  // jobs live in [head, tail)
    std::atomic<uint64_t> jobsRun{0};
    std::atomic<uint64_t> busyNanoseconds{0};
  };
#pragma warning(pop)

  void workerMain(uint32_t index);
  bool findJob(uint32_t self, Job& out);
  void execute(uint32_t self, Job& job);
  void wake();

  std::vector<std::unique_ptr<WorkQueue>> queues_;  // [0] = main / external threads
  std::vector<std::thread> threads_;
  std::atomic<uint32_t> epoch_{0};     // bumped on every submit; sleeping workers wait on it
  std::atomic<uint32_t> sleeping_{0};  // workers currently asleep
  std::atomic<uint64_t> pending_{0};  // submitted, not yet finished
  std::atomic<bool> stop_{false};
};

}  // namespace ember
