#include "media/AudioMixer.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstring>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <mfapi.h>
#include <mferror.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <propidl.h>

#include "core/Log.h"

namespace ember {

namespace {

// EmberEngine's own tracks (the user's, not Ember's): a name found here plays from here, else from Ember's game/MUSIC.
const std::filesystem::path kOwnMusic = "content/music";

template <typename T>
struct Com {
  T* p = nullptr;
  ~Com() {
    if (p) p->Release();
  }
  T** operator&() { return &p; }
  T* operator->() const { return p; }
  explicit operator bool() const { return p != nullptr; }
};

// The device's mix format: SDL converts every voice to it.
constexpr SDL_AudioSpec kDeviceSpec{SDL_AUDIO_F32, 2, 48000};

// A Media Foundation reader on an audio file, giving 32-bit float PCM at the file's own rate and channels.
struct Reader {
  Com<IMFSourceReader> reader;
  int rate = 48000, channels = 2;
  bool open(const std::filesystem::path& file) {
    if (FAILED(MFCreateSourceReaderFromURL(file.wstring().c_str(), nullptr, &reader))) return false;
    reader->SetStreamSelection(static_cast<DWORD>(MF_SOURCE_READER_ALL_STREAMS), FALSE);
    reader->SetStreamSelection(static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM), TRUE);
    Com<IMFMediaType> type;
    MFCreateMediaType(&type);
    type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    type->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_Float);
    if (FAILED(reader->SetCurrentMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM), nullptr, type.p))) return false;
    Com<IMFMediaType> actual;
    reader->GetCurrentMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM), &actual);
    rate = static_cast<int>(MFGetAttributeUINT32(actual.p, MF_MT_AUDIO_SAMPLES_PER_SECOND, 48000));
    channels = static_cast<int>(MFGetAttributeUINT32(actual.p, MF_MT_AUDIO_NUM_CHANNELS, 2));
    return true;
  }
  // The next block of samples appended to `out`; false at the end (or on an error).
  bool read(std::vector<float>& out) {
    DWORD flags = 0;
    Com<IMFSample> sample;
    if (FAILED(reader->ReadSample(static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM), 0, nullptr, &flags, nullptr, &sample))) return false;
    if (sample) {
      Com<IMFMediaBuffer> buffer;
      if (SUCCEEDED(sample->ConvertToContiguousBuffer(&buffer))) {
        BYTE* data = nullptr;
        DWORD length = 0;
        if (SUCCEEDED(buffer->Lock(&data, nullptr, &length))) {
          const size_t n = length / sizeof(float);
          const size_t at = out.size();
          out.resize(at + n);
          std::memcpy(out.data() + at, data, n * sizeof(float));
          buffer->Unlock();
        }
      }
    }
    return !(flags & MF_SOURCE_READERF_ENDOFSTREAM);
  }
  void rewind() {
    PROPVARIANT start;
    PropVariantInit(&start);
    start.vt = VT_I8;
    start.hVal.QuadPart = 0;
    reader->SetCurrentPosition(GUID_NULL, start);
    PropVariantClear(&start);
  }
};

// Media Foundation for the calling thread, once.
void mfThread() {
  thread_local bool done = false;
  if (done) return;
  done = true;
  CoInitializeEx(nullptr, COINIT_MULTITHREADED);
  MFStartup(MF_VERSION);
}

}  // namespace

// A music track: its own stream, fed by a worker decoding ahead (about two seconds queued), looping at the end.
struct AudioMixer::Track {
  std::filesystem::path file;
  SDL_AudioStream* stream = nullptr;
  float base = 0.4f;
  bool bound = false;
  std::thread worker;
  std::mutex mutex;
  std::condition_variable wake;
  std::atomic<bool> feeding{false}, quit{false};

  ~Track() {
    quit = true;
    wake.notify_all();
    if (worker.joinable()) worker.join();
    if (stream) SDL_DestroyAudioStream(stream);
  }
  void start() {
    worker = std::thread([this] {
      mfThread();
      Reader r;
      if (!r.open(file)) {
        logError("music unreadable: {}", file.string());
        return;
      }
      const SDL_AudioSpec spec{SDL_AUDIO_F32, r.channels, r.rate};
      SDL_SetAudioStreamFormat(stream, &spec, &kDeviceSpec);
      const int ahead = 2 * r.rate * r.channels * static_cast<int>(sizeof(float));  // two seconds of the file's data
      std::vector<float> block;
      while (!quit) {
        {
          std::unique_lock lock(mutex);
          wake.wait_for(lock, std::chrono::milliseconds(50), [&] { return quit.load(); });
        }
        if (quit) break;
        if (!feeding) continue;
        while (!quit && feeding && SDL_GetAudioStreamQueued(stream) < ahead) {
          block.clear();
          const bool more = r.read(block);
          if (!block.empty()) SDL_PutAudioStreamData(stream, block.data(), static_cast<int>(block.size() * sizeof(float)));
          if (!more) r.rewind();  // loop (Ember: el.loop = true)
        }
      }
    });
  }
};

AudioMixer& AudioMixer::get() {
  static AudioMixer mixer;
  return mixer;
}

AudioMixer::~AudioMixer() { shutdown(); }

void AudioMixer::shutdown() {
  tracks_.clear();  // joins each track's worker
  for (Voice& v : voices_)
    if (v.stream) SDL_DestroyAudioStream(v.stream);
  voices_.clear();
  pending_.clear();
  if (deviceId_) SDL_CloseAudioDevice(deviceId_);
  deviceId_ = 0;
  opened_ = false;
  failed_ = true;  // nothing plays after this
  current_.clear();
}

void AudioMixer::setRoot(const std::filesystem::path& artRoot) { root_ = artRoot; }

bool AudioMixer::open() {
  if (opened_) return true;
  if (failed_ || root_.empty()) return false;  // no root: captures and tools stay silent
  if (!SDL_WasInit(SDL_INIT_AUDIO) && !SDL_InitSubSystem(SDL_INIT_AUDIO)) {
    logError("audio: {}", SDL_GetError());
    failed_ = true;
    return false;
  }
  deviceId_ = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &kDeviceSpec);
  if (!deviceId_) {
    logError("audio device: {}", SDL_GetError());
    failed_ = true;
    return false;
  }
  mfThread();
  opened_ = true;
  return true;
}

const AudioMixer::Pcm& AudioMixer::pcm(const std::string& file) {
  auto it = cache_.find(file);
  if (it != cache_.end()) return it->second;
  Pcm& p = cache_[file];
  mfThread();
  Reader r;
  const std::filesystem::path path = root_ / "game" / "SoundFX" / std::filesystem::path(reinterpret_cast<const char8_t*>(file.c_str()));
  if (r.open(path)) {
    p.rate = r.rate, p.channels = r.channels;
    while (r.read(p.samples)) {
    }
    p.ok = !p.samples.empty();
  }
  if (!p.ok) logError("sound unreadable: {}", path.string());
  return p;
}

void AudioMixer::preload(const std::vector<std::string>& files) {
  for (const std::string& f : files) pcm(f);
}

void AudioMixer::startVoice(const std::string& file, float volume, bool exclusive, float offset) {
  const Pcm& p = pcm(file);
  if (!p.ok) return;
  const SDL_AudioSpec spec{SDL_AUDIO_F32, p.channels, p.rate};
  Voice* voice = nullptr;
  if (exclusive)
    for (Voice& v : voices_)
      if (v.exclusive && v.file == file) voice = &v;
  if (voice) {
    SDL_ClearAudioStream(voice->stream);  // a retrigger restarts it (currentTime = startAt)
  } else {
    SDL_AudioStream* s = SDL_CreateAudioStream(&spec, &kDeviceSpec);
    if (!s) return;
    if (!SDL_BindAudioStream(deviceId_, s)) {
      SDL_DestroyAudioStream(s);
      return;
    }
    voices_.push_back({s, file, volume, -1.0f, 0.0f, exclusive});
    voice = &voices_.back();
  }
  voice->volume = volume, voice->fade = -1.0f;
  const size_t frames = p.samples.size() / static_cast<size_t>(p.channels);
  const size_t skip = std::min(frames, static_cast<size_t>(std::max(0.0f, offset) * static_cast<float>(p.rate)));
  const size_t from = skip * static_cast<size_t>(p.channels);
  SDL_PutAudioStreamData(voice->stream, p.samples.data() + from, static_cast<int>((p.samples.size() - from) * sizeof(float)));
  SDL_FlushAudioStream(voice->stream);
  SDL_SetAudioStreamGain(voice->stream, muted_ ? 0.0f : volume * sfx_);
}

void AudioMixer::playSfx(const std::string& file, float volume, bool exclusive, float offset, float delay) {
  if (muted_ || file.empty() || !open()) return;
  if (delay > 0.0f) {
    pending_.push_back({file, volume, offset, delay, exclusive});
    return;
  }
  startVoice(file, volume, exclusive, offset);
}

void AudioMixer::fadeSfx(const std::string& file, float seconds) {
  for (Voice& v : voices_)
    if (v.exclusive && v.file == file && v.fade < 0.0f && SDL_GetAudioStreamAvailable(v.stream) > 0) v.fade = v.fadeLeft = seconds;
}

void AudioMixer::playMusic(const std::string& file, float base) {
  if (file.empty()) return stopMusic();
  if (muted_ || !open()) return;
  if (file == current_ && !paused_) return;
  // Silence every other track (silenceAllBut); each keeps its queued place.
  for (auto& [name, t] : tracks_)
    if (name != file && t->bound) {
      SDL_UnbindAudioStream(t->stream);
      t->bound = false, t->feeding = false;
    }
  std::unique_ptr<Track>& t = tracks_[file];
  if (!t) {
    t = std::make_unique<Track>();
    const std::filesystem::path name(reinterpret_cast<const char8_t*>(file.c_str()));
    std::error_code ec;
    t->file = std::filesystem::exists(kOwnMusic / name, ec) ? kOwnMusic / name : root_ / "game" / "MUSIC" / name;
    t->stream = SDL_CreateAudioStream(&kDeviceSpec, &kDeviceSpec);
    if (!t->stream) {
      tracks_.erase(file);
      return;
    }
    t->start();
  }
  t->base = base;
  SDL_SetAudioStreamGain(t->stream, base * music_);
  if (!t->bound) {
    SDL_BindAudioStream(deviceId_, t->stream);
    t->bound = true;
  }
  t->feeding = true;
  t->wake.notify_all();
  if (file != current_) logInfo("music: {} ({})", file, t->file.generic_string());  // the track each screen plays, in the game's log
  current_ = file;
  paused_ = false;
}

void AudioMixer::stopMusic() {
  for (auto& [name, t] : tracks_)
    if (t->bound) {
      SDL_UnbindAudioStream(t->stream);
      t->bound = false, t->feeding = false;
    }
  paused_ = true;
}

void AudioMixer::setVolumes(float music, float sfx, bool muted) {
  const bool wasMuted = muted_;
  music_ = std::clamp(music, 0.0f, 1.0f), sfx_ = std::clamp(sfx, 0.0f, 1.0f), muted_ = muted;
  if (muted && !wasMuted) stopMusic();  // setMuted(true): stopMusic
  applyGains();
}

void AudioMixer::applyGains() {
  for (auto& [name, t] : tracks_) SDL_SetAudioStreamGain(t->stream, muted_ ? 0.0f : t->base * music_);
  for (Voice& v : voices_)
    if (v.fade < 0.0f) SDL_SetAudioStreamGain(v.stream, muted_ ? 0.0f : v.volume * sfx_);
}

void AudioMixer::update(float dt) {
  if (!opened_) return;
  for (size_t i = 0; i < pending_.size();) {
    pending_[i].delay -= dt;
    if (pending_[i].delay <= 0.0f) {
      const Pending p = pending_[i];
      pending_.erase(pending_.begin() + static_cast<std::ptrdiff_t>(i));
      if (!muted_) startVoice(p.file, p.volume, p.exclusive, p.offset);
    } else {
      ++i;
    }
  }
  for (size_t i = 0; i < voices_.size();) {
    Voice& v = voices_[i];
    if (v.fade >= 0.0f) {
      v.fadeLeft -= dt;
      const float k = std::clamp(v.fadeLeft / std::max(1e-3f, v.fade), 0.0f, 1.0f);
      SDL_SetAudioStreamGain(v.stream, muted_ ? 0.0f : v.volume * sfx_ * k);
      if (v.fadeLeft <= 0.0f) SDL_ClearAudioStream(v.stream), v.fade = -1.0f;
    }
    // A finished layered copy goes; exclusive voices stay for their next retrigger.
    if (!v.exclusive && SDL_GetAudioStreamAvailable(v.stream) <= 0) {
      SDL_DestroyAudioStream(v.stream);
      voices_.erase(voices_.begin() + static_cast<std::ptrdiff_t>(i));
      continue;
    }
    ++i;
  }
}

int AudioMixer::check(const std::filesystem::path& artRoot) {
  mfThread();
  int failed = 0, files = 0;
  double seconds = 0.0;
  for (const char* dir : {"SoundFX", "MUSIC", "own MUSIC"}) {
    std::error_code ec;
    const std::filesystem::path folder = std::string(dir) == "own MUSIC" ? kOwnMusic : artRoot / "game" / dir;
    for (const auto& entry : std::filesystem::directory_iterator(folder, ec)) {
      if (entry.path().extension() != ".mp3") continue;
      ++files;
      Reader r;
      std::vector<float> pcm;
      bool ok = r.open(entry.path());
      const bool music = std::string(dir) != "SoundFX";
      // effects whole; music its first five seconds (it is streamed)
      while (ok && r.read(pcm) && (!music || pcm.size() < static_cast<size_t>(5 * r.rate * r.channels))) {
      }
      ok = ok && !pcm.empty();
      const double s = ok ? static_cast<double>(pcm.size()) / (r.rate * r.channels) : 0.0;
      seconds += s;
      if (!ok) ++failed;
      logInfo("{} {}/{}: {} Hz x{}, {:.2f} s", ok ? "ok " : "BAD", dir, entry.path().filename().string(), r.rate, r.channels, s);
    }
  }
  logInfo("audio check: {} files, {} failed, {:.1f} s decoded", files, failed, seconds);
  return failed;
}

}  // namespace ember
