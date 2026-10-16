#pragma once

// Plays a film (the game's cutscenes: H.264 + AAC in MP4) for the UI: Windows Media Foundation decodes it on a worker
// thread (hardware where the GPU offers it; no extra library), the sound goes out through an SDL3 audio stream, and the
// picture is an ImGui texture updated as the film plays. The sound is the clock (the picture follows it); a film
// without sound runs on the wall clock.

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <mutex>
#include <thread>
#include <vector>

#include "render/UiArt.h"

struct SDL_AudioStream;

namespace ember {

class VideoPlayer {
 public:
  VideoPlayer() = default;
  ~VideoPlayer();
  VideoPlayer(const VideoPlayer&) = delete;
  VideoPlayer& operator=(const VideoPlayer&) = delete;

  // Starts decoding; false when the file cannot be read.
  bool open(const std::filesystem::path& file);
  // Once per frame on the UI thread: shows the frame due at the current time.
  void update();
  // The current picture (null until the first frame), its size.
  const UiImage* image() const { return shown_ ? &image_ : nullptr; }
  double time() const;  // seconds since the film started
  bool finished() const;
  bool failed() const { return failed_; }
  void setMuted(bool muted);
  bool muted() const { return muted_; }

 private:
  struct Frame {
    double pts = 0;
    std::vector<uint8_t> rgba;
  };
  void decode(std::filesystem::path file);
  void stop();

  std::thread worker_;
  std::atomic<bool> stopping_{false}, ended_{false}, failed_{false};
  std::mutex mutex_;
  std::condition_variable room_;
  std::deque<Frame> frames_;
  int width_ = 0, height_ = 0;  // set by the worker before the first frame
  int stride_ = 0;              // bytes from one row to the next in a plain buffer (negative = bottom-up)
  std::atomic<SDL_AudioStream*> audio_{nullptr};
  std::atomic<double> audioPushed_{0};  // seconds of sound handed to the device
  int audioRate_ = 0, audioChannels_ = 0;
  double wallStart_ = -1, lastPts_ = 0;
  mutable double drainedAt_ = -1;  // when the last sound was heard (time() carries on from the wall clock)
  bool shown_ = false, muted_ = false;
  UiImage image_;
  void* texture_ = nullptr;  // ImTextureData, kept for reuse (see VideoPlayer.cpp)
};

}  // namespace ember
