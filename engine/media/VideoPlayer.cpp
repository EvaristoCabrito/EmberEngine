#include "media/VideoPlayer.h"

#include <SDL3/SDL.h>
#include <imgui.h>
#include <imgui_internal.h>  // RegisterUserTexture (as render/UiArt.cpp)

#include <chrono>
#include <cstring>
#include <map>
#include <memory>
#include <optional>
#include <utility>

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

#include "core/Log.h"

namespace ember {

namespace {

double nowSeconds() {
  return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

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

// ImGui textures for films, one per picture size, kept for the game's life: the Vulkan backend frees a user texture
// only through its own destroy request, and a handful of film sizes is all there is.
ImTextureData* textureFor(int w, int h) {
  static std::map<std::pair<int, int>, std::unique_ptr<ImTextureData>> textures;
  auto& slot = textures[{w, h}];
  if (!slot) {
    slot = std::make_unique<ImTextureData>();
    slot->Create(ImTextureFormat_RGBA32, w, h);
    std::memset(slot->GetPixels(), 0, static_cast<size_t>(w) * h * 4);
    slot->UseColors = true;
    ImGui::RegisterUserTexture(slot.get());
  }
  return slot.get();
}

}  // namespace

VideoPlayer::~VideoPlayer() { stop(); }

void VideoPlayer::stop() {
  stopping_ = true;
  room_.notify_all();
  if (worker_.joinable()) worker_.join();
  if (SDL_AudioStream* s = audio_.exchange(nullptr)) SDL_DestroyAudioStream(s);
}

bool VideoPlayer::open(const std::filesystem::path& file) {
  std::error_code error;
  if (!std::filesystem::exists(file, error)) {
    logError("film not found: {}", file.string());
    failed_ = true;
    return false;
  }
  if (!SDL_WasInit(SDL_INIT_AUDIO) && !SDL_InitSubSystem(SDL_INIT_AUDIO)) logError("audio: {}", SDL_GetError());
  worker_ = std::thread([this, file] { decode(file); });
  return true;
}

// The worker: Media Foundation's source reader, converting the picture to 32-bit RGB and the sound to float PCM.
void VideoPlayer::decode(std::filesystem::path file) {
  const HRESULT com = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
  MFStartup(MF_VERSION);
  {
    Com<IMFAttributes> attributes;
    MFCreateAttributes(&attributes, 1);
    attributes->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, TRUE);
    Com<IMFSourceReader> reader;
    if (FAILED(MFCreateSourceReaderFromURL(file.wstring().c_str(), attributes.p, &reader))) {
      logError("film unreadable: {}", file.string());
      failed_ = true;
    } else {
      // Only the first picture and sound streams (some films also carry a cover image stream).
      reader->SetStreamSelection(static_cast<DWORD>(MF_SOURCE_READER_ALL_STREAMS), FALSE);
      reader->SetStreamSelection(static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM), TRUE);
      const bool hasAudio = SUCCEEDED(reader->SetStreamSelection(static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM), TRUE));
      {
        Com<IMFMediaType> type;
        MFCreateMediaType(&type);
        type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
        type->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
        if (FAILED(reader->SetCurrentMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM), nullptr, type.p))) failed_ = true;
      }
      bool audioOk = false;
      if (hasAudio) {
        Com<IMFMediaType> type;
        MFCreateMediaType(&type);
        type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
        type->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_Float);
        audioOk = SUCCEEDED(reader->SetCurrentMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM), nullptr, type.p));
      }
      if (!failed_) {
        Com<IMFMediaType> video;
        reader->GetCurrentMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM), &video);
        UINT32 w = 0, h = 0;
        MFGetAttributeSize(video.p, MF_MT_FRAME_SIZE, &w, &h);
        width_ = static_cast<int>(w), height_ = static_cast<int>(h);
        // Row order of a plain buffer: the type's default stride (negative = bottom-up). The video processor's RGB32
        // comes top-down here, so that is what a type without the attribute is taken to be.
        UINT32 stride = 0;
        stride_ = SUCCEEDED(video->GetUINT32(MF_MT_DEFAULT_STRIDE, &stride)) ? static_cast<int>(static_cast<LONG>(stride)) : width_ * 4;
        if (audioOk) {
          Com<IMFMediaType> audio;
          reader->GetCurrentMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM), &audio);
          audioRate_ = static_cast<int>(MFGetAttributeUINT32(audio.p, MF_MT_AUDIO_SAMPLES_PER_SECOND, 48000));
          audioChannels_ = static_cast<int>(MFGetAttributeUINT32(audio.p, MF_MT_AUDIO_NUM_CHANNELS, 2));
          SDL_AudioSpec spec{SDL_AUDIO_F32, audioChannels_, audioRate_};
          if (SDL_AudioStream* s = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr)) {
            if (muted_) SDL_SetAudioStreamGain(s, 0.0f);
            audio_ = s;
          } else {
            logError("film sound: {}", SDL_GetError());
          }
        }
        bool videoDone = false, audioDone = !audio_.load();
        bool started = false;
        while (!stopping_ && !(videoDone && audioDone)) {
          DWORD stream = 0, flags = 0;
          LONGLONG ts = 0;
          Com<IMFSample> sample;
          if (FAILED(reader->ReadSample(static_cast<DWORD>(MF_SOURCE_READER_ANY_STREAM), 0, &stream, &flags, &ts, &sample))) break;
          Com<IMFMediaType> current;
          reader->GetCurrentMediaType(stream, &current);
          GUID major{};
          if (current) current->GetGUID(MF_MT_MAJOR_TYPE, &major);
          const bool isVideo = major == MFMediaType_Video;
          if (flags & MF_SOURCE_READERF_ENDOFSTREAM) {
            (isVideo ? videoDone : audioDone) = true;
            continue;
          }
          if (!sample) continue;
          Com<IMFMediaBuffer> buffer;
          if (FAILED(sample->ConvertToContiguousBuffer(&buffer))) continue;
          if (isVideo) {
            Frame frame;
            frame.pts = static_cast<double>(ts) / 1e7;
            frame.rgba.resize(static_cast<size_t>(width_) * height_ * 4);
            // Rows through IMF2DBuffer (it knows the pitch and which way up), else a plain buffer in the type's row order.
            Com<IMF2DBuffer> buffer2d;
            BYTE* scan0 = nullptr;
            LONG pitch = 0;
            bool locked2d = SUCCEEDED(buffer->QueryInterface(IID_PPV_ARGS(&buffer2d))) && SUCCEEDED(buffer2d->Lock2D(&scan0, &pitch));
            BYTE* data = nullptr;
            DWORD length = 0;
            if (!locked2d) {
              if (FAILED(buffer->Lock(&data, nullptr, &length))) continue;
              pitch = stride_;
              scan0 = stride_ < 0 ? data + static_cast<size_t>(height_ - 1) * width_ * 4 : data;
            }
            for (int y = 0; y < height_; ++y) {
              const BYTE* src = scan0 + static_cast<ptrdiff_t>(pitch) * y;
              uint8_t* dst = frame.rgba.data() + static_cast<size_t>(y) * width_ * 4;
              for (int x = 0; x < width_; ++x) {  // BGRX -> RGBA
                dst[x * 4 + 0] = src[x * 4 + 2];
                dst[x * 4 + 1] = src[x * 4 + 1];
                dst[x * 4 + 2] = src[x * 4 + 0];
                dst[x * 4 + 3] = 255;
              }
            }
            if (locked2d) buffer2d->Unlock2D();
            else buffer->Unlock();
            std::unique_lock lock(mutex_);
            room_.wait(lock, [&] { return stopping_ || frames_.size() < 8; });
            frames_.push_back(std::move(frame));
            if (!started && audio_.load()) SDL_ResumeAudioStreamDevice(audio_.load());
            started = true;
          } else if (SDL_AudioStream* s = audio_.load()) {
            BYTE* data = nullptr;
            DWORD length = 0;
            if (SUCCEEDED(buffer->Lock(&data, nullptr, &length))) {
              SDL_PutAudioStreamData(s, data, static_cast<int>(length));
              audioPushed_ = audioPushed_.load() + static_cast<double>(length) / (4.0 * audioChannels_ * audioRate_);
              buffer->Unlock();
            }
          }
        }
      }
    }
  }
  ended_ = true;
  MFShutdown();
  if (SUCCEEDED(com)) CoUninitialize();
}

double VideoPlayer::time() const {
  if (SDL_AudioStream* s = audio_.load()) {
    // The sound's clock: what was handed over minus what still waits to be heard. Once the sound has all been heard
    // and nothing more comes, the wall clock carries on from there (a picture longer than its sound still ends).
    const int queued = SDL_GetAudioStreamQueued(s);
    const double heard = audioPushed_.load() - static_cast<double>(std::max(0, queued)) / (4.0 * audioChannels_ * audioRate_);
    if (ended_ && queued <= 0) {
      if (drainedAt_ < 0) drainedAt_ = nowSeconds();
      return heard + (nowSeconds() - drainedAt_);
    }
    return heard;
  }
  return wallStart_ < 0 ? 0.0 : nowSeconds() - wallStart_;
}

void VideoPlayer::update() {
  std::optional<Frame> due;
  {
    std::lock_guard lock(mutex_);
    if (wallStart_ < 0 && !frames_.empty()) wallStart_ = nowSeconds() - frames_.front().pts;
    const double t = time();
    while (!frames_.empty() && (frames_.front().pts <= t || !shown_)) {
      due = std::move(frames_.front());
      frames_.pop_front();
      if (!shown_) break;  // the first picture at once
    }
  }
  room_.notify_all();
  if (!due) return;
  lastPts_ = due->pts;
  auto* tex = static_cast<ImTextureData*>(texture_);
  if (!tex) {
    tex = textureFor(width_, height_);
    texture_ = tex;
    image_ = {tex->GetTexRef(), width_, height_};
  }
  std::memcpy(tex->GetPixels(), due->rgba.data(), due->rgba.size());
  if (tex->Status == ImTextureStatus_OK) {
    tex->UpdateRect = {0, 0, static_cast<unsigned short>(width_), static_cast<unsigned short>(height_)};
    tex->Updates.resize(0);
    tex->Updates.push_back(tex->UpdateRect);
    tex->SetStatus(ImTextureStatus_WantUpdates);
  }
  shown_ = true;
}

bool VideoPlayer::finished() const {
  if (failed_) return true;
  if (!ended_) return false;
  std::lock_guard lock(const_cast<std::mutex&>(mutex_));
  return frames_.empty() && time() >= lastPts_;
}

void VideoPlayer::setMuted(bool muted) {
  muted_ = muted;
  if (SDL_AudioStream* s = audio_.load()) SDL_SetAudioStreamGain(s, muted ? 0.0f : 1.0f);
}

}  // namespace ember
