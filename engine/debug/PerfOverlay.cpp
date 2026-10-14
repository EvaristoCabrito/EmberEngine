#include "debug/PerfOverlay.h"

#include <imgui.h>

#include <algorithm>

#include "assets/AssetManager.h"
#include "platform/IoQueue.h"
#include "render/Renderer.h"

namespace ember {

namespace {

constexpr double kMegabyte = 1024.0 * 1024.0;

}  // namespace

void PerfOverlay::addFrame(double frameMs, double busyMs) {
  frameMs_[frames_ % kHistory] = static_cast<float>(frameMs);
  busyMs_[frames_ % kHistory] = static_cast<float>(busyMs);
  ++frames_;
}

void PerfOverlay::sampleRates(const Sources& sources) {
  const auto now = std::chrono::steady_clock::now();
  const double seconds = std::chrono::duration<double>(now - sampleTime_).count();
  if (seconds < 0.5) return;

  if (sources.jobs) {
    std::vector<JobSystem::ThreadStats> jobs = sources.jobs->stats();
    if (lastJobs_.size() == jobs.size()) {
      threadLoad_.resize(jobs.size());
      uint64_t ran = 0;
      for (size_t i = 0; i < jobs.size(); ++i) {
        const double busy = static_cast<double>(jobs[i].busyNanoseconds - lastJobs_[i].busyNanoseconds) * 1e-9;
        threadLoad_[i] = static_cast<float>(std::clamp(busy / seconds, 0.0, 1.0));
        ran += jobs[i].jobsRun - lastJobs_[i].jobsRun;
      }
      jobsPerSecond_ = static_cast<double>(ran) / seconds;
    }
    lastJobs_ = std::move(jobs);
  }
  if (sources.io) {
    const uint64_t bytes = sources.io->stats().bytesRead;
    readMegabytesPerSecond_ = static_cast<double>(bytes - lastBytesRead_) / kMegabyte / seconds;
    lastBytesRead_ = bytes;
  }
  sampleTime_ = now;
}

void PerfOverlay::draw(const Sources& sources) {
  sampleRates(sources);
  if (!visible) return;

  ImGui::SetNextWindowPos({12, 12}, ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowBgAlpha(0.82f);
  if (!ImGui::Begin("Performance  [F1]", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing)) {
    ImGui::End();
    return;
  }

  // --- Frame --------------------------------------------------------------------------------
  const size_t count = std::min(frames_, kHistory);
  if (count > 0) {
    float frameSum = 0, frameMax = 0, busyMax = 0, busySum = 0;
    for (size_t i = 0; i < count; ++i) {
      frameSum += frameMs_[i];
      frameMax = std::max(frameMax, frameMs_[i]);
      busySum += busyMs_[i];
      busyMax = std::max(busyMax, busyMs_[i]);
    }
    const float frameAvg = frameSum / static_cast<float>(count);
    ImGui::Text("%.0f fps   frame %.2f ms avg, %.2f ms worst", frameAvg > 0 ? 1000.0f / frameAvg : 0.0f, frameAvg, frameMax);
    ImGui::Text("main thread busy %.2f ms avg, %.2f ms worst", busySum / static_cast<float>(count), busyMax);
    // Oldest to newest, so the graph scrolls left.
    std::array<float, kHistory> ordered{};
    for (size_t i = 0; i < count; ++i) ordered[i] = busyMs_[(frames_ - count + i) % kHistory];
    ImGui::PlotLines("##busy", ordered.data(), static_cast<int>(count), 0, "main thread ms (0-16.7)", 0.0f, 16.7f, {320, 48});
  }

  // --- GPU ----------------------------------------------------------------------------------
  if (sources.renderer) {
    const GpuTimer& timer = sources.renderer->gpuTimer();
    if (ImGui::CollapsingHeader("GPU", ImGuiTreeNodeFlags_DefaultOpen)) {
      if (!timer.supported()) ImGui::TextDisabled("timestamps not supported");
      for (const GpuTimer::Result& result : timer.results()) ImGui::Text("%-10s %7.3f ms", result.name, result.milliseconds);
    }
  }

  // --- Jobs ---------------------------------------------------------------------------------
  if (sources.jobs && ImGui::CollapsingHeader("Jobs", ImGuiTreeNodeFlags_DefaultOpen)) {
    float total = 0;
    for (float load : threadLoad_) total += load;
    ImGui::Text("%u threads   %.0f%% load   %.0f jobs/s", sources.jobs->workerCount() + 1,
                threadLoad_.empty() ? 0.0 : 100.0 * total / static_cast<double>(threadLoad_.size()), jobsPerSecond_);
    if (!threadLoad_.empty())
      ImGui::PlotHistogram("##threads", threadLoad_.data(), static_cast<int>(threadLoad_.size()), 0, "load per thread",
                           0.0f, 1.0f, {320, 40});
  }

  // --- Streaming ----------------------------------------------------------------------------
  if ((sources.io || sources.assets) && ImGui::CollapsingHeader("Streaming", ImGuiTreeNodeFlags_DefaultOpen)) {
    if (sources.io) {
      const IoQueue::Stats io = sources.io->stats();
      ImGui::Text("disk  %.1f MB/s   %u in flight, %u queued", readMegabytesPerSecond_, io.inFlight, io.queued);
      ImGui::Text("      %.1f MB read, %llu reads", static_cast<double>(io.bytesRead) / kMegabyte,
                  static_cast<unsigned long long>(io.completed));
    }
    if (sources.assets) {
      const AssetManager::Stats assets = sources.assets->stats();
      ImGui::Text("assets  %u ready, %u loading, %u queued, %u failed", assets.ready, assets.loading, assets.queued,
                  assets.failed);
      ImGui::Text("        %.1f MB resident, %.1f MB in flight, %.0f MB read pool",
                  static_cast<double>(assets.residentBytes) / kMegabyte, static_cast<double>(assets.bytesInFlight) / kMegabyte,
                  static_cast<double>(assets.readPoolBytes) / kMegabyte);
    }
  }
  ImGui::End();
}

}  // namespace ember
