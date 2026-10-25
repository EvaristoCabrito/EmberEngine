#pragma once

// Plays a film (the game's cutscenes) for the UI, decoded on a worker thread: the sound goes out through an SDL3 audio
// stream, and the picture is an ImGui texture updated as the film plays. The sound is the clock (the picture follows it);
// a film without sound runs on the wall clock.
//   .webm (AV1 + Opus; the user, 2026-10-25: our own decoders, the same on every PC, AV1 about half of H.264's size): libwebm
//         reads the file, dav1d decodes the picture, libopus the sound. A film asked for as .mp4 plays its .webm twin when
//         one lies beside it (the MP4 is kept).
//   .mp4  (H.264 + AAC): Windows Media Foundation (hardware where the GPU offers it).
// Each film logs its decode cost when it ends (ms per picture, average and worst), to compare the two.

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
  // The sound's level, 0..1 (Opções' cutscene volume); muting keeps it.
  void setVolume(float volume);
  bool muted() const { return muted_; }
  // Sandbox --film-check: decodes the film (and its .webm twin, for an .mp4) as fast as it goes, pictures and sound thrown away,
  // and logs each one's decode cost and speed; the number of films that failed. `still` (optional): picture number `stillAt` of
  // each film written beside it as <still stem>-<decoder>.png, to compare the two decoders' colours.
  static int check(const std::filesystem::path& film, const std::filesystem::path& still = {}, int stillAt = 100);

 private:
  struct Frame {
    double pts = 0;
    std::vector<uint8_t> rgba;
  };
  void decode(std::filesystem::path file);
  void decodeMediaFoundation(const std::filesystem::path& file);
  void decodeWebm(const std::filesystem::path& file);
  // Shared by both decoders: the sound device at the film's format, a picture into the queue (waits for room, unless the
  // sound runs low: a file whose sound lies further on than its pictures must not stall the clock), sound to the device.
  void openSound(int rate, int channels);
  void pushPicture(Frame&& frame);
  void pushSound(const float* samples, size_t bytes);
  void logCost(const std::filesystem::path& file, const char* decoder) const;
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
  bool soundStarted_ = false;  // the device resumes with the first picture
  bool bench_ = false;         // check(): no device, no queue, pictures dropped as they come
  std::filesystem::path stillPath_;  // check(): where picture `stillAt_` is written (empty = none)
  int stillAt_ = 0, benchCount_ = 0;
  // decode cost (worker only): seconds spent decoding + converting pictures, the worst one, how many
  double costSum_ = 0, costMax_ = 0;
  int costFrames_ = 0;
  std::atomic<float> volume_{1.0f};
  UiImage image_;
  void* texture_ = nullptr;  // ImTextureData, kept for reuse (see VideoPlayer.cpp)
};

}  // namespace ember
