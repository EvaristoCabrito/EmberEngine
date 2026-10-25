#include "media/VideoPlayer.h"

#include <SDL3/SDL.h>
#include <imgui.h>
#include <imgui_internal.h>  // RegisterUserTexture (as render/UiArt.cpp)

#include <algorithm>
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

#include <cstdio>

#include <dav1d/dav1d.h>
#include <mkvparser/mkvparser.h>
#include <opus.h>
#include <stb_image_write.h>

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

// libwebm reads through this: the file by its wide path (any name), positioned reads.
class WebmFile : public mkvparser::IMkvReader {
 public:
  ~WebmFile() override {
    if (file_) std::fclose(file_);
  }
  bool open(const std::filesystem::path& path) {
    file_ = _wfopen(path.c_str(), L"rb");
    if (!file_ || _fseeki64(file_, 0, SEEK_END) != 0) return false;
    size_ = _ftelli64(file_);
    return size_ > 0;
  }
  int Read(long long pos, long len, unsigned char* buf) override {
    if (pos < 0 || len < 0 || pos + len > size_ || _fseeki64(file_, pos, SEEK_SET) != 0) return -1;
    return std::fread(buf, 1, static_cast<size_t>(len), file_) == static_cast<size_t>(len) ? 0 : -1;
  }
  int Length(long long* total, long long* available) override {
    if (total) *total = size_;
    if (available) *available = size_;
    return 0;
  }

 private:
  FILE* file_ = nullptr;
  long long size_ = 0;
};

// A dav1d picture (8-bit 4:2:0) to RGBA: BT.709 or BT.601 as the stream says (untagged: 709 from 720 rows up, else 601, as
// players do), limited or full range; 16.16 fixed point.
void yuvToRgba(const Dav1dPicture& pic, uint8_t* out) {
  const int w = pic.p.w, h = pic.p.h;
  const Dav1dSequenceHeader* seq = pic.seq_hdr;
  const bool full = seq && seq->color_range;
  const Dav1dMatrixCoefficients m = seq ? seq->mtrx : DAV1D_MC_UNKNOWN;
  const bool bt709 = m == DAV1D_MC_BT709 || (m != DAV1D_MC_BT601 && m != DAV1D_MC_BT470BG && h >= 720);
  const double kr = bt709 ? 0.2126 : 0.299, kb = bt709 ? 0.0722 : 0.114, kg = 1.0 - kr - kb;
  const double ys = full ? 1.0 : 255.0 / 219.0, cs = full ? 1.0 : 255.0 / 224.0;
  const int yo = full ? 0 : 16;
  auto fx = [](double v) { return static_cast<int>(std::lround(v * 65536.0)); };
  const int cy = fx(ys), crv = fx(cs * 2.0 * (1.0 - kr)), cbu = fx(cs * 2.0 * (1.0 - kb));
  const int cgu = fx(cs * 2.0 * (1.0 - kb) * kb / kg), cgv = fx(cs * 2.0 * (1.0 - kr) * kr / kg);
  auto clamp8 = [](int v) { return static_cast<uint8_t>(v < 0 ? 0 : v > 255 ? 255 : v); };
  const auto* yPlane = static_cast<const uint8_t*>(pic.data[0]);
  const auto* uPlane = static_cast<const uint8_t*>(pic.data[1]);
  const auto* vPlane = static_cast<const uint8_t*>(pic.data[2]);
  for (int y = 0; y < h; ++y) {
    const uint8_t* yr = yPlane + static_cast<ptrdiff_t>(pic.stride[0]) * y;
    const uint8_t* ur = uPlane + static_cast<ptrdiff_t>(pic.stride[1]) * (y >> 1);
    const uint8_t* vr = vPlane + static_cast<ptrdiff_t>(pic.stride[1]) * (y >> 1);
    uint8_t* dst = out + static_cast<size_t>(y) * w * 4;
    for (int x = 0; x < w; ++x) {
      const int l = (yr[x] - yo) * cy + 32768, u = ur[x >> 1] - 128, v = vr[x >> 1] - 128;
      dst[x * 4 + 0] = clamp8((l + crv * v) >> 16);
      dst[x * 4 + 1] = clamp8((l - cgu * u - cgv * v) >> 16);
      dst[x * 4 + 2] = clamp8((l + cbu * u) >> 16);
      dst[x * 4 + 3] = 255;
    }
  }
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
  // A film asked for as .mp4 plays its AV1 twin when there is one (the MP4 stays for whoever has no .webm yet)
  std::filesystem::path play = file;
  if (play.extension() == ".mp4") {
    std::filesystem::path twin = play;
    twin.replace_extension(".webm");
    if (std::filesystem::exists(twin, error)) play = twin;
  }
  worker_ = std::thread([this, play] { decode(play); });
  return true;
}

void VideoPlayer::decode(std::filesystem::path file) {
  if (file.extension() == ".webm") decodeWebm(file);
  else decodeMediaFoundation(file);
  ended_ = true;
}

void VideoPlayer::openSound(int rate, int channels) {
  audioRate_ = rate, audioChannels_ = channels;
  if (bench_) return;
  SDL_AudioSpec spec{SDL_AUDIO_F32, audioChannels_, audioRate_};
  if (SDL_AudioStream* s = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr)) {
    SDL_SetAudioStreamGain(s, muted_ ? 0.0f : volume_.load());
    audio_ = s;
  } else {
    logError("film sound: {}", SDL_GetError());
  }
}

void VideoPlayer::pushPicture(Frame&& frame) {
  if (bench_) {
    if (!stillPath_.empty() && benchCount_ == stillAt_)
      stbi_write_png(stillPath_.string().c_str(), width_, height_, 4, frame.rgba.data(), width_ * 4);
    ++benchCount_;
    return;
  }
  // Room for 8 pictures; while the sound has under a quarter second left to play, more may queue (the sound it waits for lies
  // further on in the file than these pictures).
  auto soundLow = [&] {
    SDL_AudioStream* s = audio_.load();
    return s && soundStarted_ && SDL_GetAudioStreamQueued(s) < static_cast<int>(0.25 * 4.0 * audioChannels_ * audioRate_);
  };
  std::unique_lock lock(mutex_);
  while (!stopping_ && frames_.size() >= 8 && !(frames_.size() < 64 && soundLow())) room_.wait_for(lock, std::chrono::milliseconds(20));
  frames_.push_back(std::move(frame));
  if (!soundStarted_ && audio_.load()) SDL_ResumeAudioStreamDevice(audio_.load());
  soundStarted_ = true;
}

void VideoPlayer::pushSound(const float* samples, size_t bytes) {
  SDL_AudioStream* s = audio_.load();
  if (!s || bytes == 0) return;
  SDL_PutAudioStreamData(s, samples, static_cast<int>(bytes));
  audioPushed_ = audioPushed_.load() + static_cast<double>(bytes) / (4.0 * audioChannels_ * audioRate_);
}

int VideoPlayer::check(const std::filesystem::path& film, const std::filesystem::path& still, int stillAt) {
  std::vector<std::filesystem::path> files{film};
  std::error_code error;
  if (film.extension() == ".mp4") {
    std::filesystem::path twin = film;
    twin.replace_extension(".webm");
    if (std::filesystem::exists(twin, error)) files.push_back(twin);
  }
  int bad = 0;
  for (const std::filesystem::path& f : files) {
    VideoPlayer p;
    p.bench_ = true;
    if (!still.empty()) {
      p.stillAt_ = stillAt;
      p.stillPath_ = still.parent_path() / (still.stem().string() + (f.extension() == ".webm" ? "-av1.png" : "-h264.png"));
    }
    const double t0 = nowSeconds();
    p.decode(f);
    const double wall = nowSeconds() - t0;
    logInfo("film check {}: {} pictures in {:.2f} s = {:.0f} pictures a second{}", f.filename().string(), p.costFrames_, wall,
            wall > 0 ? p.costFrames_ / wall : 0.0, p.failed_ ? " (FAILED)" : "");
    if (p.failed_ || p.costFrames_ == 0) ++bad;
  }
  return bad;
}

void VideoPlayer::logCost(const std::filesystem::path& file, const char* decoder) const {
  if (costFrames_ == 0) return;
  logInfo("film {}: {} {}x{}, {} pictures, decode {:.2f} ms a picture (worst {:.2f} ms), {:.1f} MB", file.filename().string(), decoder, width_,
          height_, costFrames_, 1000.0 * costSum_ / costFrames_, 1000.0 * costMax_,
          static_cast<double>(std::filesystem::file_size(file)) / (1024.0 * 1024.0));
}

// AV1 + Opus in WebM: libwebm walks the file's clusters in order, dav1d decodes each picture (its own threads), libopus each
// sound packet (48 kHz float; the stream's pre-skip dropped from the start).
void VideoPlayer::decodeWebm(const std::filesystem::path& file) {
  WebmFile reader;
  long long pos = 0;
  mkvparser::EBMLHeader ebml;
  mkvparser::Segment* rawSegment = nullptr;
  if (!reader.open(file) || ebml.Parse(&reader, pos) < 0 || mkvparser::Segment::CreateInstance(&reader, pos, rawSegment) != 0 || !rawSegment) {
    logError("film unreadable: {}", file.string());
    failed_ = true;
    return;
  }
  std::unique_ptr<mkvparser::Segment> segment(rawSegment);
  if (segment->Load() < 0) {
    logError("film unreadable: {}", file.string());
    failed_ = true;
    return;
  }
  long long videoTrack = -1, audioTrack = -1;
  int preSkip = 0;
  OpusDecoder* opus = nullptr;
  const mkvparser::Tracks* tracks = segment->GetTracks();
  for (unsigned long i = 0; tracks && i < tracks->GetTracksCount(); ++i) {
    const mkvparser::Track* t = tracks->GetTrackByIndex(i);
    if (!t || !t->GetCodecId()) continue;
    const std::string codec = t->GetCodecId();
    if (videoTrack < 0 && t->GetType() == mkvparser::Track::kVideo && codec == "V_AV1") {
      videoTrack = t->GetNumber();
    } else if (audioTrack < 0 && t->GetType() == mkvparser::Track::kAudio && codec == "A_OPUS") {
      const auto* a = static_cast<const mkvparser::AudioTrack*>(t);
      const int channels = static_cast<int>(a->GetChannels());
      size_t headSize = 0;
      const unsigned char* head = t->GetCodecPrivate(headSize);  // OpusHead: pre-skip at bytes 10-11
      if (channels < 1 || channels > 2) {
        logError("film sound: {} channels (only mono / stereo films are played)", channels);
        continue;
      }
      int err = 0;
      opus = opus_decoder_create(48000, channels, &err);
      if (err != OPUS_OK) {
        logError("film sound: {}", opus_strerror(err));
        opus = nullptr;
        continue;
      }
      if (head && headSize >= 12) preSkip = head[10] | (head[11] << 8);
      audioTrack = t->GetNumber();
      openSound(48000, channels);
    }
  }
  if (videoTrack < 0) {
    logError("film {}: no AV1 picture track", file.string());
    failed_ = true;
    if (opus) opus_decoder_destroy(opus);
    return;
  }
  Dav1dSettings settings;
  dav1d_default_settings(&settings);
  settings.n_threads = 0;  // as many as the machine has
  Dav1dContext* dav1d = nullptr;
  if (dav1d_open(&dav1d, &settings) < 0) {
    logError("film {}: the AV1 decoder did not start", file.string());
    failed_ = true;
    if (opus) opus_decoder_destroy(opus);
    return;
  }
  // One picture from dav1d, out to the queue: true when one came. While decoding it is asked once after each packet (dav1d's
  // own loop: asking again at once would flush its frame threads); at the end, until it has none left.
  bool pictureFailed = false;
  auto takePicture = [&](double& spent) {
    Dav1dPicture pic{};
    const auto t0 = std::chrono::steady_clock::now();
    const int got = dav1d_get_picture(dav1d, &pic);
    if (got < 0) {
      if (got != DAV1D_ERR(EAGAIN)) pictureFailed = true;
      return false;
    }
    if (pic.p.layout != DAV1D_PIXEL_LAYOUT_I420 || pic.p.bpc != 8) {
      logError("film {}: only 8-bit 4:2:0 AV1 is played", file.string());
      dav1d_picture_unref(&pic);
      pictureFailed = true;
      return false;
    }
    width_ = pic.p.w, height_ = pic.p.h;
    Frame frame;
    frame.pts = static_cast<double>(pic.m.timestamp) / 1e9;
    frame.rgba.resize(static_cast<size_t>(width_) * height_ * 4);
    yuvToRgba(pic, frame.rgba.data());
    dav1d_picture_unref(&pic);
    spent += std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    costSum_ += spent, costMax_ = std::max(costMax_, spent), ++costFrames_;
    spent = 0;
    pushPicture(std::move(frame));
    return true;
  };
  std::vector<unsigned char> packet;
  std::vector<float> pcm(5760 * 2);  // 120 ms at 48 kHz, stereo: Opus's longest frame
  int skip = preSkip;
  double spent = 0;  // decode time not yet given to a picture
  const mkvparser::Cluster* cluster = segment->GetFirst();
  while (!stopping_ && !pictureFailed && cluster && !cluster->EOS()) {
    const mkvparser::BlockEntry* entry = nullptr;
    if (cluster->GetFirst(entry) < 0) break;
    while (!stopping_ && !pictureFailed && entry && !entry->EOS()) {
      const mkvparser::Block* block = entry->GetBlock();
      const long long track = block->GetTrackNumber();
      for (int k = 0; k < block->GetFrameCount() && (track == videoTrack || track == audioTrack); ++k) {
        const mkvparser::Block::Frame& f = block->GetFrame(k);
        packet.resize(static_cast<size_t>(f.len));
        if (f.Read(&reader, packet.data()) < 0) continue;
        if (track == videoTrack) {
          const auto t0 = std::chrono::steady_clock::now();
          Dav1dData data{};
          uint8_t* bytes = dav1d_data_create(&data, packet.size());
          if (!bytes) continue;
          std::memcpy(bytes, packet.data(), packet.size());
          data.m.timestamp = block->GetTime(cluster);  // nanoseconds
          while (data.sz > 0 && !stopping_ && !pictureFailed) {
            const int sent = dav1d_send_data(dav1d, &data);
            spent += std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
            if (sent < 0 && sent != DAV1D_ERR(EAGAIN)) {
              pictureFailed = true;
              break;
            }
            takePicture(spent);
          }
          if (data.sz > 0) dav1d_data_unref(&data);
        } else if (opus) {
          const int n = opus_decode_float(opus, packet.data(), static_cast<opus_int32>(packet.size()), pcm.data(), 5760, 0);
          if (n <= 0) continue;
          const int drop = std::min(skip, n);
          skip -= drop;
          pushSound(pcm.data() + static_cast<size_t>(drop) * audioChannels_, static_cast<size_t>(n - drop) * audioChannels_ * sizeof(float));
        }
      }
      if (cluster->GetNext(entry, entry) < 0) break;
    }
    cluster = segment->GetNext(cluster);
  }
  while (!stopping_ && !pictureFailed && takePicture(spent)) {}  // the pictures still inside the decoder
  if (pictureFailed) logError("film {}: AV1 decoding failed", file.string());
  dav1d_close(&dav1d);
  if (opus) opus_decoder_destroy(opus);
  logCost(file, "AV1 (dav1d)");
}

// H.264 + AAC in MP4: Media Foundation's source reader, converting the picture to 32-bit RGB and the sound to float PCM.
void VideoPlayer::decodeMediaFoundation(const std::filesystem::path& file) {
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
          openSound(static_cast<int>(MFGetAttributeUINT32(audio.p, MF_MT_AUDIO_SAMPLES_PER_SECOND, 48000)),
                    static_cast<int>(MFGetAttributeUINT32(audio.p, MF_MT_AUDIO_NUM_CHANNELS, 2)));
        }
        bool videoDone = false, audioDone = !audio_.load();
        while (!stopping_ && !(videoDone && audioDone)) {
          DWORD stream = 0, flags = 0;
          LONGLONG ts = 0;
          Com<IMFSample> sample;
          const auto t0 = std::chrono::steady_clock::now();
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
            const double spent = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
            costSum_ += spent, costMax_ = std::max(costMax_, spent), ++costFrames_;
            pushPicture(std::move(frame));
          } else if (audio_.load()) {
            BYTE* data = nullptr;
            DWORD length = 0;
            if (SUCCEEDED(buffer->Lock(&data, nullptr, &length))) {
              pushSound(reinterpret_cast<const float*>(data), length);
              buffer->Unlock();
            }
          }
        }
        logCost(file, "H.264 (Media Foundation)");
      }
    }
  }
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
  if (SDL_AudioStream* s = audio_.load()) SDL_SetAudioStreamGain(s, muted ? 0.0f : volume_.load());
}

void VideoPlayer::setVolume(float volume) {
  volume_ = std::clamp(volume, 0.0f, 1.0f);
  if (SDL_AudioStream* s = audio_.load()) SDL_SetAudioStreamGain(s, muted_ ? 0.0f : volume_.load());
}

}  // namespace ember
