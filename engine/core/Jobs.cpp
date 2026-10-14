#include "core/Jobs.h"

#include <immintrin.h>

#include <chrono>
#include <format>
#include <string>

#include <windows.h>

namespace ember {

namespace {

thread_local uint32_t tThreadIndex = 0;

constexpr uint64_t kInitialQueueCapacity = 1024;  // grows by doubling; never shrinks
constexpr int kSpinsBeforeSleep = 64;

class SpinGuard {
 public:
  explicit SpinGuard(std::atomic_flag& flag) : flag_(flag) {
    while (flag_.test_and_set(std::memory_order_acquire))
      while (flag_.test(std::memory_order_relaxed)) _mm_pause();
  }
  ~SpinGuard() { flag_.clear(std::memory_order_release); }
  SpinGuard(const SpinGuard&) = delete;
  SpinGuard& operator=(const SpinGuard&) = delete;

 private:
  std::atomic_flag& flag_;
};

uint64_t nowNanoseconds() {
  return static_cast<uint64_t>(
      std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
}

}  // namespace

// --- WorkQueue -----------------------------------------------------------------------------------

void JobSystem::WorkQueue::pushBack(Job&& job) {
  SpinGuard guard(lock);
  const uint64_t capacity = ring.size();
  if (tail - head == capacity) {
    std::vector<Job> grown(std::max<uint64_t>(capacity * 2, kInitialQueueCapacity));
    const uint64_t mask = grown.size() - 1;
    for (uint64_t i = head; i < tail; ++i) grown[i & mask] = std::move(ring[i & (capacity - 1)]);
    ring = std::move(grown);
  }
  ring[tail & (ring.size() - 1)] = std::move(job);
  ++tail;
}

bool JobSystem::WorkQueue::popBack(Job& out) {
  SpinGuard guard(lock);
  if (head == tail) return false;
  --tail;
  out = std::move(ring[tail & (ring.size() - 1)]);
  return true;
}

bool JobSystem::WorkQueue::popFront(Job& out) {
  // Thieves give up instead of queueing behind the owner.
  if (lock.test_and_set(std::memory_order_acquire)) return false;
  bool found = false;
  if (head != tail) {
    out = std::move(ring[head & (ring.size() - 1)]);
    ++head;
    found = true;
  }
  lock.clear(std::memory_order_release);
  return found;
}

// --- JobSystem -----------------------------------------------------------------------------------

JobSystem::JobSystem(uint32_t workerCount) {
  if (workerCount == 0) workerCount = std::max(2u, std::thread::hardware_concurrency()) - 1;
  queues_.reserve(workerCount + 1);
  for (uint32_t i = 0; i <= workerCount; ++i) queues_.push_back(std::make_unique<WorkQueue>());
  tThreadIndex = 0;
  threads_.reserve(workerCount);
  for (uint32_t i = 1; i <= workerCount; ++i) threads_.emplace_back([this, i] { workerMain(i); });
}

JobSystem::~JobSystem() {
  // Finish all submitted work (it may own memory, e.g. I/O buffers) before the workers stop.
  Job job;
  while (pending_.load(std::memory_order_acquire) > 0) {
    if (findJob(tThreadIndex, job)) execute(tThreadIndex, job);
    else std::this_thread::yield();
  }
  stop_.store(true, std::memory_order_seq_cst);
  epoch_.fetch_add(1, std::memory_order_seq_cst);
  epoch_.notify_all();
  for (std::thread& thread : threads_) thread.join();
}

uint32_t JobSystem::threadIndex() { return tThreadIndex; }

void JobSystem::run(JobFunction job, JobCounter* counter) {
  if (counter) counter->pending_.fetch_add(1, std::memory_order_relaxed);
  pending_.fetch_add(1, std::memory_order_relaxed);
  queues_[tThreadIndex]->pushBack(Job{std::move(job), counter});
  wake();
}

void JobSystem::wake() {
  // Paired with the seq_cst increment of sleeping_ in workerMain: either the worker sees the new
  // epoch and does not sleep, or this sees the sleeper and wakes it.
  epoch_.fetch_add(1, std::memory_order_seq_cst);
  if (sleeping_.load(std::memory_order_seq_cst) > 0) epoch_.notify_one();
}

bool JobSystem::findJob(uint32_t self, Job& out) {
  if (queues_[self]->popBack(out)) return true;
  const uint32_t count = static_cast<uint32_t>(queues_.size());
  for (uint32_t i = 1; i < count; ++i)
    if (queues_[(self + i) % count]->popFront(out)) return true;
  return false;
}

void JobSystem::execute(uint32_t self, Job& job) {
  const uint64_t start = nowNanoseconds();
  job.function();
  job.function.reset();
  WorkQueue& queue = *queues_[self];
  queue.busyNanoseconds.fetch_add(nowNanoseconds() - start, std::memory_order_relaxed);
  queue.jobsRun.fetch_add(1, std::memory_order_relaxed);
  if (job.counter) job.counter->pending_.fetch_sub(1, std::memory_order_acq_rel);
  pending_.fetch_sub(1, std::memory_order_release);
}

void JobSystem::workerMain(uint32_t index) {
  tThreadIndex = index;
  SetThreadDescription(GetCurrentThread(), std::format(L"Ember Worker {}", index).c_str());
  Job job;
  while (!stop_.load(std::memory_order_relaxed)) {
    bool ran = false;
    for (int spin = 0; spin < kSpinsBeforeSleep && !ran; ++spin) {
      if (findJob(index, job)) {
        execute(index, job);
        ran = true;
      } else {
        _mm_pause();
      }
    }
    if (ran) continue;

    sleeping_.fetch_add(1, std::memory_order_seq_cst);
    const uint32_t epoch = epoch_.load(std::memory_order_seq_cst);
    if (findJob(index, job)) {
      sleeping_.fetch_sub(1, std::memory_order_seq_cst);
      execute(index, job);
      continue;
    }
    if (!stop_.load(std::memory_order_seq_cst)) epoch_.wait(epoch, std::memory_order_seq_cst);
    sleeping_.fetch_sub(1, std::memory_order_seq_cst);
  }
}

void JobSystem::wait(JobCounter& counter) {
  const uint32_t self = tThreadIndex;
  Job job;
  int idle = 0;
  while (!counter.done()) {
    if (findJob(self, job)) {
      execute(self, job);
      idle = 0;
    } else if (++idle < kSpinsBeforeSleep) {
      _mm_pause();
    } else {
      std::this_thread::yield();
    }
  }
}

std::vector<JobSystem::ThreadStats> JobSystem::stats() const {
  std::vector<ThreadStats> out(queues_.size());
  for (size_t i = 0; i < queues_.size(); ++i) {
    out[i].jobsRun = queues_[i]->jobsRun.load(std::memory_order_relaxed);
    out[i].busyNanoseconds = queues_[i]->busyNanoseconds.load(std::memory_order_relaxed);
  }
  return out;
}

}  // namespace ember
