#include "render/UiArt.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <algorithm>
#include <condition_variable>
#include <cstring>
#include <cmath>
#include <deque>
#include <format>
#include <fstream>
#include <iterator>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <lunasvg.h>

#include <imgui_internal.h>
#include <stb_image.h>

#include "core/Log.h"

namespace ember {

namespace {

ImFont* gFonts[3] = {};

struct Sized {
  int width = 0, height = 0;
  std::unique_ptr<ImTextureData> data;
};
struct LoadedImage {
  std::unique_ptr<ImTextureData> data;
  UiImage image;
  bool failed = false;
  // Copies resampled to the sizes the image is drawn at (refineUiImages): exact ones first, then a halving chain for
  // images too big to resample per size, or drawn at more sizes than kMaxExactSizes.
  std::vector<Sized> exact;
  std::vector<Sized> halves;
};
std::map<std::string, LoadedImage>& images() {
  static std::map<std::string, LoadedImage> cache;
  return cache;
}
// One key per file however its path was written ("a/b\c.png" and "a/b/c.png" are the same picture).
std::string keyOf(const std::filesystem::path& file) { return file.lexically_normal().generic_string(); }

// The file actually read for a picture: the user's own replacements of Ember art (content/ui/replacements.json:
// "<folder>/<file>" -> a file of ours), so every screen that asks for the old picture gets the new one; else the file.
std::filesystem::path sourceOf(const std::filesystem::path& file) {
  static const std::map<std::string, std::filesystem::path> table = [] {
    std::map<std::string, std::filesystem::path> t;
    std::ifstream in("content/ui/replacements.json");
    std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    // a flat {"a/b.jpg": "c/d.jpg", ...}: read without a JSON library (this file has none)
    for (size_t at = 0;;) {
      std::string pair[2];
      for (std::string& s : pair) {
        const size_t open = text.find('"', at), close = open == std::string::npos ? open : text.find('"', open + 1);
        if (close == std::string::npos) return t;
        s = text.substr(open + 1, close - open - 1), at = close + 1;
      }
      std::transform(pair[0].begin(), pair[0].end(), pair[0].begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
      t[pair[0]] = pair[1];
    }
  }();
  if (table.empty()) return file;
  std::string tail = (file.parent_path().filename() / file.filename()).generic_string();
  std::transform(tail.begin(), tail.end(), tail.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  const auto it = table.find(tail);
  return it != table.end() && std::filesystem::exists(it->second) ? it->second : file;
}
// The full-size texture of each loaded image -> its entry (map nodes never move).
std::map<const ImTextureData*, LoadedImage*>& imagesByTexture() {
  static std::map<const ImTextureData*, LoadedImage*> index;
  return index;
}

// Textures made by refineUiImages, after ImGui::Render built this frame's texture list: added to it by hand, so the
// backend creates them before the draw that uses them.
std::vector<ImTextureData*>& lateTextures() {
  static std::vector<ImTextureData*> list;
  return list;
}

constexpr size_t kMaxExactSizes = 8;
constexpr long long kMaxExactSource = 2048LL * 2048LL;  // source pixels resampled per drawn size
constexpr long long kMaxExactTarget = 1024LL * 1024LL;  // drawn pixels resampled exactly

// Premultiplied float RGBA of an 8-bit straight-alpha image.
std::vector<float> premultiplied(const unsigned char* pixels, int w, int h) {
  std::vector<float> image(static_cast<size_t>(w) * h * 4);
  for (size_t i = 0; i < static_cast<size_t>(w) * h; ++i) {
    const float a = pixels[i * 4 + 3] / 255.0f;
    for (int c = 0; c < 3; ++c) image[i * 4 + c] = pixels[i * 4 + c] / 255.0f * a;
    image[i * 4 + 3] = a;
  }
  return image;
}

// 8-bit straight-alpha RGBA from premultiplied float RGBA.
std::vector<unsigned char> straightPixels(const std::vector<float>& premul, int w, int h) {
  std::vector<unsigned char> out(static_cast<size_t>(w) * h * 4);
  for (size_t i = 0; i < static_cast<size_t>(w) * h; ++i) {
    const float a = std::clamp(premul[i * 4 + 3], 0.0f, 1.0f);
    for (int c = 0; c < 3; ++c) out[i * 4 + c] = static_cast<unsigned char>(std::lround(std::clamp(a > 0.0f ? premul[i * 4 + c] / a : 0.0f, 0.0f, 1.0f) * 255.0f));
    out[i * 4 + 3] = static_cast<unsigned char>(std::lround(a * 255.0f));
  }
  return out;
}

// A UI texture from 8-bit RGBA; registered, created by the backend on its next draw.
std::unique_ptr<ImTextureData> makeTexture(const std::vector<unsigned char>& pixels, int w, int h) {
  auto tex = std::make_unique<ImTextureData>();
  tex->Create(ImTextureFormat_RGBA32, w, h);
  std::memcpy(tex->GetPixels(), pixels.data(), pixels.size());
  tex->UseColors = true;
  ImGui::RegisterUserTexture(tex.get());
  lateTextures().push_back(tex.get());
  return tex;
}

// Resampling runs on a thread of its own (the hunt for the battle hitch, 2026-10-19: 27 spell icons of 784 x 1168
// resampled on the main thread when the battle bar first showed cost 50-65 ms, their uploads 40-50 more): the image is
// drawn with the best copy it already has until the new one is ready, and at most kTexturesPerFrame new copies are handed
// to the backend per frame.
struct ResampleJob {
  LoadedImage* image;
  bool half;  // a halving (index level - 1) or an exact size
  int level, w, h;
  std::shared_ptr<const std::vector<unsigned char>> source;  // the image's own pixels (8-bit RGBA)
  int sw, sh;
  std::vector<unsigned char> result;
};
class Resampler {
 public:
  Resampler() : thread_([this](std::stop_token stop) { run(stop); }) {}
  ~Resampler() {
    thread_.request_stop();
    wake_.notify_all();
  }
  void push(ResampleJob job) {
    {
      std::lock_guard lock(mutex_);
      todo_.push_back(std::move(job));
    }
    wake_.notify_one();
  }
  std::vector<ResampleJob> takeDone(size_t most) {
    std::lock_guard lock(mutex_);
    const size_t n = std::min(most, done_.size());
    std::vector<ResampleJob> out(std::make_move_iterator(done_.begin()), std::make_move_iterator(done_.begin() + static_cast<ptrdiff_t>(n)));
    done_.erase(done_.begin(), done_.begin() + static_cast<ptrdiff_t>(n));
    return out;
  }

 private:
  void run(std::stop_token stop) {
    for (;;) {
      ResampleJob job;
      {
        std::unique_lock lock(mutex_);
        wake_.wait(lock, [&] { return stop.stop_requested() || !todo_.empty(); });
        if (stop.stop_requested()) return;
        job = std::move(todo_.front());
        todo_.pop_front();
      }
      const std::vector<float> source = premultiplied(job.source->data(), job.sw, job.sh);
      const std::vector<float> wide = uiResampleAxis(source, job.sw, job.sh, job.w, true);
      job.result = straightPixels(uiResampleAxis(wide, job.w, job.sh, job.h, false), job.w, job.h);
      std::lock_guard lock(mutex_);
      done_.push_back(std::move(job));
    }
  }
  std::mutex mutex_;
  std::condition_variable wake_;
  std::deque<ResampleJob> todo_, done_;
  std::jthread thread_;
};
Resampler& resampler() {
  static Resampler r;
  return r;
}
constexpr size_t kTexturesPerFrame = 4;

// The image's own pixels for the resampler (copied once, shared by every job of that image).
std::shared_ptr<const std::vector<unsigned char>> sourcePixels(const LoadedImage& image) {
  static std::map<const LoadedImage*, std::shared_ptr<const std::vector<unsigned char>>> copies;
  auto& copy = copies[&image];
  if (!copy) {
    const auto* p = static_cast<const unsigned char*>(image.data->GetPixels());
    copy = std::make_shared<const std::vector<unsigned char>>(p, p + static_cast<size_t>(image.image.width) * image.image.height * 4);
  }
  return copy;
}

// The texture to draw `image` with when its whole width / height cover needW / needH pixels on screen: the right copy
// if it is made, else one is asked for and the best copy already made (or the image itself) is drawn meanwhile.
const ImTextureData* textureFor(LoadedImage& image, float needW, float needH) {
  const int w = image.image.width, h = image.image.height;
  if (needW >= 0.8f * static_cast<float>(w) && needH >= 0.8f * static_cast<float>(h)) return image.data.get();
  const int tw = std::clamp(static_cast<int>(std::ceil(needW)), 1, w), th = std::clamp(static_cast<int>(std::ceil(needH)), 1, h);
  // Meanwhile: the smallest copy made that is still at least as big as the drawn size (bilinear covers the rest).
  const auto meanwhile = [&]() -> const ImTextureData* {
    const ImTextureData* best = image.data.get();
    int bestArea = w * h;
    auto consider = [&](const Sized& s) {
      if (s.data && s.width >= tw && s.height >= th && s.width * s.height < bestArea) best = s.data.get(), bestArea = s.width * s.height;
    };
    for (const Sized& s : image.exact) consider(s);
    for (const Sized& s : image.halves) consider(s);
    return best;
  };
  for (const Sized& s : image.exact)
    if (std::abs(s.width - tw) <= 1 && std::abs(s.height - th) <= 1) return s.data ? s.data.get() : meanwhile();
  if (image.exact.size() < kMaxExactSizes && static_cast<long long>(w) * h <= kMaxExactSource && static_cast<long long>(tw) * th <= kMaxExactTarget) {
    image.exact.push_back({tw, th, nullptr});  // asked for: filled when the resampler hands it back
    resampler().push({&image, false, 0, tw, th, sourcePixels(image), w, h, {}});
    return meanwhile();
  }
  // The smallest halving still at least as big as the drawn size; bilinear filtering covers the rest (under 2:1).
  int level = 0;
  while ((w >> (level + 1)) >= tw && (h >> (level + 1)) >= th && (w >> (level + 1)) > 0 && (h >> (level + 1)) > 0) ++level;
  if (level == 0) return image.data.get();
  while (static_cast<int>(image.halves.size()) < level) image.halves.push_back({});
  Sized& half = image.halves[static_cast<size_t>(level - 1)];
  if (half.width == 0) {
    half = {w >> level, h >> level, nullptr};
    resampler().push({&image, true, level, w >> level, h >> level, sourcePixels(image), w, h, {}});
  }
  return half.data ? half.data.get() : meanwhile();
}

// Copies the resampler finished: made into textures (a few per frame) and put where textureFor looks for them.
void collectResampled() {
  for (ResampleJob& job : resampler().takeDone(kTexturesPerFrame)) {
    Sized* slot = nullptr;
    if (job.half) {
      if (job.level - 1 < static_cast<int>(job.image->halves.size())) slot = &job.image->halves[static_cast<size_t>(job.level - 1)];
    } else {
      for (Sized& s : job.image->exact)
        if (s.width == job.w && s.height == job.h && !s.data) slot = &s;
    }
    if (slot && !slot->data) slot->data = makeTexture(job.result, job.w, job.h);
  }
}

// --- lucide icons ------------------------------------------------------------------------------------------------------
struct IconRaster {
  std::unique_ptr<ImTextureData> data;
  bool failed = false;
};
std::map<std::string, IconRaster>& iconRasters() {
  static std::map<std::string, IconRaster> cache;
  return cache;
}
std::map<std::string, std::string>& iconSources() {
  static std::map<std::string, std::string> cache;
  return cache;
}


}  // namespace

std::vector<float> uiResampleAxis(const std::vector<float>& src, int srcW, int srcH, int dstW, bool horizontal) {
  const int srcLen = horizontal ? srcW : srcH, dstLen = dstW;
  const int lines = horizontal ? srcH : srcW;
  const float step = static_cast<float>(srcLen) / static_cast<float>(dstLen);
  const float radius = std::max(1.0f, step);
  std::vector<float> dst(static_cast<size_t>(dstLen) * lines * 4, 0.0f);
  for (int d = 0; d < dstLen; ++d) {
    const float centre = (static_cast<float>(d) + 0.5f) * step - 0.5f;
    const int first = static_cast<int>(std::floor(centre - radius)), last = static_cast<int>(std::ceil(centre + radius));
    for (int line = 0; line < lines; ++line) {
      float sum[4] = {}, weights = 0.0f;
      for (int s = std::max(first, 0); s <= std::min(last, srcLen - 1); ++s) {
        const float w = std::max(0.0f, 1.0f - std::abs(static_cast<float>(s) - centre) / radius);
        if (w <= 0.0f) continue;
        const size_t at = horizontal ? (static_cast<size_t>(line) * srcW + s) * 4 : (static_cast<size_t>(s) * srcW + line) * 4;
        for (int c = 0; c < 4; ++c) sum[c] += src[at + c] * w;
        weights += w;
      }
      const size_t out = horizontal ? (static_cast<size_t>(line) * dstLen + d) * 4 : (static_cast<size_t>(d) * srcW + line) * 4;
      for (int c = 0; c < 4; ++c) dst[out + c] = weights > 0.0f ? sum[c] / weights : 0.0f;
    }
  }
  return dst;
}

namespace {

// A UI texture released by releaseUiImages that a screen draws again: back on the GPU from the pixels kept in RAM (still there
// if its destruction has not run yet; else created again before this frame's draw).
void keepOnGpu(ImTextureData* t) {
  if (!t || !t->WantDestroyNextFrame) return;
  t->WantDestroyNextFrame = false;
  if (t->Status == ImTextureStatus_WantDestroy) t->Status = ImTextureStatus_OK, t->UnusedFrames = 0;
  else if (t->Status == ImTextureStatus_Destroyed) t->Status = ImTextureStatus_WantCreate, lateTextures().push_back(t);
}

}  // namespace

void releaseUiImages() {
  size_t count = 0;
  uint64_t bytes = 0;
  const auto drop = [&](ImTextureData* t) {
    if (!t || t->WantDestroyNextFrame || t->Status == ImTextureStatus_Destroyed) return;
    t->WantDestroyNextFrame = true;
    ++count, bytes += static_cast<uint64_t>(t->Width) * static_cast<uint64_t>(t->Height) * 4u;
  };
  for (auto& [key, image] : images()) {
    drop(image.data.get());
    for (Sized& s : image.exact) drop(s.data.get());
    for (Sized& s : image.halves) drop(s.data.get());
  }
  logInfo("ui: {} images off the GPU ({} MB); each comes back when a screen draws it again", count, bytes >> 20);
}

void refineUiImages(ImDrawData* drawData) {
  if (!drawData) return;
  collectResampled();
  const auto& index = imagesByTexture();
  if (index.empty()) return;
  const ImVec2 scale = drawData->FramebufferScale;
  for (ImDrawList* list : drawData->CmdLists) {
    for (ImDrawCmd& cmd : list->CmdBuffer) {
      if (cmd.UserCallback || !cmd.TexRef._TexData) continue;
      const auto it = index.find(cmd.TexRef._TexData);
      if (it == index.end()) continue;
      // The whole texture's size on screen, from each triangle's mapping uv -> position (any shape: quads, rounded).
      float needW = 0.0f, needH = 0.0f;
      for (unsigned int i = 0; i + 2 < cmd.ElemCount; i += 3) {
        const ImDrawVert& a = list->VtxBuffer[cmd.VtxOffset + list->IdxBuffer[cmd.IdxOffset + i]];
        const ImDrawVert& b = list->VtxBuffer[cmd.VtxOffset + list->IdxBuffer[cmd.IdxOffset + i + 1]];
        const ImDrawVert& c = list->VtxBuffer[cmd.VtxOffset + list->IdxBuffer[cmd.IdxOffset + i + 2]];
        const float du1 = b.uv.x - a.uv.x, dv1 = b.uv.y - a.uv.y, du2 = c.uv.x - a.uv.x, dv2 = c.uv.y - a.uv.y;
        const float det = du1 * dv2 - du2 * dv1;
        if (std::abs(det) < 1e-9f) continue;
        const float e1x = (b.pos.x - a.pos.x) * scale.x, e1y = (b.pos.y - a.pos.y) * scale.y;
        const float e2x = (c.pos.x - a.pos.x) * scale.x, e2y = (c.pos.y - a.pos.y) * scale.y;
        // Position change per unit u and per unit v.
        const float ux = (e1x * dv2 - e2x * dv1) / det, uy = (e1y * dv2 - e2y * dv1) / det;
        const float vx = (e2x * du1 - e1x * du2) / det, vy = (e2y * du1 - e1y * du2) / det;
        needW = std::max(needW, std::sqrt(ux * ux + uy * uy));
        needH = std::max(needH, std::sqrt(vx * vx + vy * vy));
      }
      if (needW <= 0.0f || needH <= 0.0f) {
        keepOnGpu(const_cast<ImTextureData*>(it->first));
        continue;
      }
      const ImTextureData* tex = textureFor(*it->second, needW, needH);
      keepOnGpu(const_cast<ImTextureData*>(tex));
      if (tex != cmd.TexRef._TexData) cmd.TexRef = const_cast<ImTextureData*>(tex)->GetTexRef();
    }
  }
  if (drawData->Textures)
    for (ImTextureData* tex : lateTextures())
      if (!drawData->Textures->contains(tex)) drawData->Textures->push_back(tex);
  lateTextures().clear();
}

void uiIconSvg(ImDrawList* dl, const std::string& key, const std::string& svg, ImVec2 centre, float size, ImU32 ink) {
  const float zoom = std::max(ImGui::GetIO().DisplayFramebufferScale.x, 0.01f);
  const int px = std::max(1, static_cast<int>(std::lround(size * zoom)));
  IconRaster& raster = iconRasters()[std::format("{}@{}", key, px)];
  if (raster.failed) return;
  if (!raster.data) {
    std::string text = svg;
    for (size_t at = 0; (at = text.find("currentColor", at)) != std::string::npos;) text.replace(at, 12, "#ffffff");
    const std::unique_ptr<lunasvg::Document> doc = lunasvg::Document::loadFromData(text);
    lunasvg::Bitmap bitmap = doc ? doc->renderToBitmap(px, px) : lunasvg::Bitmap();
    if (bitmap.isNull()) {
      logError("UI icon not drawn: {}", key);
      raster.failed = true;
      return;
    }
    bitmap.convertToRGBA();
    raster.data = std::make_unique<ImTextureData>();
    raster.data->Create(ImTextureFormat_RGBA32, px, px);
    for (int y = 0; y < px; ++y) std::memcpy(static_cast<unsigned char*>(raster.data->GetPixels()) + static_cast<size_t>(y) * px * 4, bitmap.data() + static_cast<size_t>(y) * bitmap.stride(), static_cast<size_t>(px) * 4);
    raster.data->UseColors = true;
    ImGui::RegisterUserTexture(raster.data.get());
  }
  // On whole screen pixels, so each raster pixel lands on one screen pixel.
  const float x0 = std::round(centre.x * zoom - px * 0.5f) / zoom, y0 = std::round(centre.y * zoom - px * 0.5f) / zoom;
  dl->AddImage(raster.data->GetTexRef(), {x0, y0}, {x0 + px / zoom, y0 + px / zoom}, {0, 0}, {1, 1}, ink);
}

void uiIcon(ImDrawList* dl, const char* name, ImVec2 centre, float size, ImU32 ink, float strokeWidth) {
  std::string& source = iconSources()[name];
  if (source.empty()) {
    const std::string file = std::format("content/ui/lucide/{}.svg", name);
    std::ifstream in(file, std::ios::binary);
    source = in ? std::string(std::istreambuf_iterator<char>(in), {}) : std::string(" ");
    if (!in) logError("UI icon not found: {}", file);
  }
  if (source == " ") return;
  if (strokeWidth == 2.0f) return uiIconSvg(dl, name, source, centre, size, ink);
  std::string thin = source;
  if (const size_t at = thin.find("stroke-width=\"2\""); at != std::string::npos) thin.replace(at, 16, std::format("stroke-width=\"{}\"", strokeWidth));
  uiIconSvg(dl, std::format("{}~{}", name, strokeWidth), thin, centre, size, ink);
}

void loadUiFonts() {
  ImGuiIO& io = ImGui::GetIO();
  const char* files[3] = {"content/fonts/Figtree.ttf", "content/fonts/Newsreader.ttf", "content/fonts/PirataOne.ttf"};
  for (int i = 0; i < 3; ++i) {
    std::error_code error;
    if (std::filesystem::exists(files[i], error)) gFonts[i] = io.Fonts->AddFontFromFileTTF(files[i], 18.0f);
    else logError("UI font not found: {}", files[i]);
    // Figtree has no ⌊ ⌋ (the status sheet's damage formulas, Ember StatusPanel): Segoe UI Symbol fills in the glyphs it lacks.
    if (i == 0 && gFonts[0]) {
      char windows[MAX_PATH] = {};
      const UINT length = GetWindowsDirectoryA(windows, MAX_PATH);
      const std::string symbols = (length > 0 && length < MAX_PATH ? std::string(windows) : std::string("C:\\Windows")) + "\\Fonts\\seguisym.ttf";
      std::error_code error;
      if (std::filesystem::exists(symbols, error)) {
        ImFontConfig merge;
        merge.MergeMode = true;
        io.Fonts->AddFontFromFileTTF(symbols.c_str(), 18.0f, &merge);
      }
    }
  }
}

ImFont* uiFont(UiFont font) {
  ImFont* f = gFonts[static_cast<int>(font)];
  return f ? f : ImGui::GetFont();
}

namespace {

// A decoded file handed to the cache (main thread): null pixels = it could not be read.
const UiImage* adopt(const std::string& key, stbi_uc* pixels, int w, int h) {
  LoadedImage& slot = images()[key];
  if (slot.data || slot.failed) {  // already there (drawn before the preload reached it)
    if (pixels) stbi_image_free(pixels);
    return slot.failed ? nullptr : &slot.image;
  }
  if (!pixels) {
    logError("UI image not found: {}", key);
    slot.failed = true;
    return nullptr;
  }
  slot.data = std::make_unique<ImTextureData>();
  slot.data->Create(ImTextureFormat_RGBA32, w, h);
  std::memcpy(slot.data->GetPixels(), pixels, static_cast<size_t>(w) * static_cast<size_t>(h) * 4);
  stbi_image_free(pixels);
  slot.data->UseColors = true;
  ImGui::RegisterUserTexture(slot.data.get());  // the Vulkan backend creates it on its next draw
  slot.image = {slot.data->GetTexRef(), w, h};
  imagesByTexture()[slot.data.get()] = &slot;
  return &slot.image;
}

}  // namespace

const UiImage* uiImage(const std::filesystem::path& file) {
  const std::string key = keyOf(file);
  if (const auto it = images().find(key); it != images().end() && (it->second.data || it->second.failed))
    return it->second.failed ? nullptr : &it->second.image;
  int w = 0, h = 0, n = 0;
  stbi_uc* pixels = stbi_load(sourceOf(file).string().c_str(), &w, &h, &n, 4);
  return adopt(key, pixels, w, h);
}

bool uiImageLoaded(const std::filesystem::path& file) {
  const auto it = images().find(keyOf(file));
  return it != images().end() && (it->second.data || it->second.failed);
}

UiPreload::UiPreload(std::vector<std::filesystem::path> files) : files_(std::move(files)) {
  worker_ = std::jthread([this](std::stop_token stop) {
    for (const std::filesystem::path& f : files_) {
      if (stop.stop_requested()) return;
      Decoded d{keyOf(f)};
      d.pixels = stbi_load(sourceOf(f).string().c_str(), &d.w, &d.h, &d.n, 4);
      std::lock_guard lock(mutex_);
      decoded_.push_back(d);
    }
  });
}

UiPreload::~UiPreload() {
  worker_.request_stop();
  if (worker_.joinable()) worker_.join();
  for (Decoded& d : decoded_)
    if (d.pixels) stbi_image_free(d.pixels);
}

std::pair<size_t, size_t> UiPreload::update() {
  std::vector<Decoded> ready;
  {
    std::lock_guard lock(mutex_);
    ready.swap(decoded_);
  }
  for (Decoded& d : ready) {
    adopt(d.key, d.pixels, d.w, d.h);
    ++done_;
  }
  return {done_, files_.size()};
}

void verticalGradient(ImDrawList* dl, ImVec2 a, ImVec2 b, std::initializer_list<GradientStop> stops) {
  const GradientStop* prev = nullptr;
  for (const GradientStop& s : stops) {
    if (prev) {
      const float y0 = a.y + (b.y - a.y) * prev->at, y1 = a.y + (b.y - a.y) * s.at;
      dl->AddRectFilledMultiColor({a.x, y0}, {b.x, y1}, prev->color, prev->color, s.color, s.color);
    }
    prev = &s;
  }
}

void imageCover(ImDrawList* dl, const UiImage& image, ImVec2 a, ImVec2 b, float fx, float fy, ImU32 tint) {
  const float w = b.x - a.x, h = b.y - a.y;
  const float scale = std::max(w / static_cast<float>(image.width), h / static_cast<float>(image.height));
  const float iw = image.width * scale, ih = image.height * scale;
  // CSS background-position x%: the image's x% point sits on the box's x% point.
  const ImVec2 p{a.x + (w - iw) * fx, a.y + (h - ih) * fy};
  dl->PushClipRect(a, b, true);
  dl->AddImage(image.texture, p, {p.x + iw, p.y + ih}, {0, 0}, {1, 1}, tint);
  dl->PopClipRect();
}

void imageContain(ImDrawList* dl, const UiImage& image, ImVec2 a, ImVec2 b, ImU32 tint) {
  const float w = b.x - a.x, h = b.y - a.y;
  const float scale = std::min(w / static_cast<float>(image.width), h / static_cast<float>(image.height));
  const float iw = image.width * scale, ih = image.height * scale;
  const ImVec2 p{a.x + (w - iw) * 0.5f, a.y + (h - ih) * 0.5f};
  dl->AddImage(image.texture, p, {p.x + iw, p.y + ih}, {0, 0}, {1, 1}, tint);
}

float spacedTextWidth(ImFont* font, float size, const char* text, float spacingEm) {
  float w = 0.0f;
  int count = 0;
  for (const char* c = text; *c;) {
    const char* next = c;
    unsigned int cp = 0;
    next += ImTextCharFromUtf8(&cp, c, nullptr);
    w += font->CalcTextSizeA(size, FLT_MAX, 0.0f, c, next).x;
    c = next;
    ++count;
  }
  return w + spacingEm * size * static_cast<float>(std::max(0, count - 1));
}

void spacedText(ImDrawList* dl, ImFont* font, float size, ImVec2 at, ImU32 color, const char* text, float spacingEm, bool shadow) {
  for (int pass = shadow ? 0 : 1; pass < 2; ++pass) {
    float x = at.x;
    const ImU32 alpha = (color >> IM_COL32_A_SHIFT) & 0xFF;
    for (const char* c = text; *c;) {
      const char* next = c;
      unsigned int cp = 0;
      next += ImTextCharFromUtf8(&cp, c, nullptr);
      if (pass == 0) {
        // soft shadow: a few offset copies, darker toward the centre (text-shadow 0 2px 6px #000)
        for (int k = 0; k < 4; ++k) {
          const float r = 1.0f + k;
          dl->AddText(font, size, {x, at.y + 2.0f + r * 0.4f}, IM_COL32(0, 0, 0, static_cast<int>(alpha * 0.22f)), c, next);
          (void)r;
        }
      } else {
        dl->AddText(font, size, {x, at.y}, color, c, next);
      }
      x += font->CalcTextSizeA(size, FLT_MAX, 0.0f, c, next).x + spacingEm * size;
      c = next;
    }
  }
}

void emberPanel(ImDrawList* dl, ImVec2 a, ImVec2 b) {
  for (int i = 8; i >= 1; --i) dl->AddRectFilled({a.x - i, a.y + i * 1.8f}, {b.x + i, b.y + i * 2.2f}, IM_COL32(0, 0, 0, 16), 4.0f);  // 0 18px 48px
  dl->AddRect({a.x - 1, a.y - 1}, {b.x + 1, b.y + 1}, IM_COL32(0, 0, 0, 255), 3.0f);
  verticalGradient(dl, a, b, {{0.0f, IM_COL32(30, 30, 30, 242)}, {0.6f, IM_COL32(10, 10, 10, 245)}, {1.0f, IM_COL32(24, 24, 24, 242)}});
  dl->AddRect({a.x + 1, a.y + 1}, {b.x - 1, b.y - 1}, IM_COL32(255, 220, 160, 15), 3.0f);
  dl->AddRect(a, b, IM_COL32(190, 150, 95, 115), 3.0f);
  const ImU32 c = IM_COL32(224, 192, 140, 191);
  const float i = 5.0f, l = 16.0f;
  const ImVec2 p{a.x + i, a.y + i}, q{b.x - i, b.y - i};
  dl->AddLine(p, {p.x + l, p.y}, c), dl->AddLine(p, {p.x, p.y + l}, c);
  dl->AddLine({q.x, p.y}, {q.x - l, p.y}, c), dl->AddLine({q.x, p.y}, {q.x, p.y + l}, c);
  dl->AddLine({p.x, q.y}, {p.x + l, q.y}, c), dl->AddLine({p.x, q.y}, {p.x, q.y - l}, c);
  dl->AddLine(q, {q.x - l, q.y}, c), dl->AddLine(q, {q.x, q.y - l}, c);
  const float w = b.x - a.x, s0 = a.x + w * 0.18f, s1 = b.x - w * 0.18f, m = (s0 + s1) * 0.5f;
  for (int g = 2; g >= 0; --g) {
    const int al = g == 0 ? 255 : 50;
    dl->AddRectFilledMultiColor({s0, a.y - 1 - g}, {m, a.y + g}, IM_COL32(226, 104, 30, 0), IM_COL32(226, 104, 30, al), IM_COL32(226, 104, 30, al), IM_COL32(226, 104, 30, 0));
    dl->AddRectFilledMultiColor({m, a.y - 1 - g}, {s1, a.y + g}, IM_COL32(226, 104, 30, al), IM_COL32(226, 104, 30, 0), IM_COL32(226, 104, 30, 0), IM_COL32(226, 104, 30, al));
  }
}

void emberRule(ImDrawList* dl, float x0, float x1, float y) {
  const float a = x0 + (x1 - x0) * 0.2f, b = x1 - (x1 - x0) * 0.2f;
  const ImU32 gold = IM_COL32(190, 150, 95, 153), clear = IM_COL32(190, 150, 95, 0);
  dl->AddRectFilledMultiColor({x0, y}, {a, y + 1}, clear, gold, gold, clear);
  dl->AddRectFilled({a, y}, {b, y + 1}, gold);
  dl->AddRectFilledMultiColor({b, y}, {x1, y + 1}, gold, clear, clear, gold);
  const float m = (x0 + x1) * 0.5f, r = 4.5f;
  const ImVec2 d[4] = {{m, y - r}, {m + r, y}, {m, y + r}, {m - r, y}};
  dl->AddConvexPolyFilled(d, 4, IM_COL32(0, 0, 0, 255));
  const ImVec2 e[4] = {{m, y - r + 1}, {m + r - 1, y}, {m, y + r - 1}, {m - r + 1, y}};
  dl->AddQuadFilled(e[0], e[1], e[2], e[3], IM_COL32(160, 136, 100, 255));
}

void emberSlot(ImDrawList* dl, ImVec2 a, ImVec2 b, bool hover) {
  if (hover) for (int k = 3; k >= 1; --k) dl->AddRect({a.x - k * 3.0f, a.y - k * 3.0f}, {b.x + k * 3.0f, b.y + k * 3.0f}, IM_COL32(255, 110, 40, 14), 4.0f, 3.0f);
  verticalGradient(dl, a, b, {{0.0f, IM_COL32(12, 12, 12, 230)}, {1.0f, IM_COL32(5, 5, 5, 230)}});
  dl->AddRectFilledMultiColor(a, {b.x, a.y + 8}, IM_COL32(0, 0, 0, 200), IM_COL32(0, 0, 0, 200), IM_COL32(0, 0, 0, 0), IM_COL32(0, 0, 0, 0));
  dl->AddRect(a, b, hover ? IM_COL32(226, 170, 100, 191) : IM_COL32(190, 150, 95, 71), 2.0f);
}

void emberKicker(ImDrawList* dl, ImVec2 at, float size, const char* text, bool defeat) {
  ImFont* f = uiFont(UiFont::Mark);
  spacedText(dl, f, size, {at.x, at.y + 1}, IM_COL32(0, 0, 0, 200), text, 0.3f, false);
  spacedText(dl, f, size, at, defeat ? IM_COL32(201, 122, 98, 255) : IM_COL32(184, 154, 112, 255), text, 0.3f, false);
}

void emberTitle(ImDrawList* dl, ImVec2 at, float size, const char* text) {
  ImFont* f = uiFont(UiFont::Mark);
  for (int k = 3; k >= 1; --k) dl->AddText(f, size, {at.x, at.y + 1.0f + k * 0.6f}, IM_COL32(0, 0, 0, 70), text);
  dl->AddText(f, size, at, IM_COL32(241, 220, 192, 255), text);
}

bool emberBigButton(const char* id, const char* label, ImVec2 at, ImVec2 size, bool primary, float fontSize) {
  ImGui::SetCursorScreenPos(at);
  const bool clicked = ImGui::InvisibleButton(id, size);
  const bool hover = ImGui::IsItemHovered();
  ImDrawList* dl = ImGui::GetWindowDrawList();
  const ImVec2 a = at, b{at.x + size.x, at.y + size.y};
  auto k = [&](int r, int g, int bl) {
    const float m = hover ? 1.1f : 1.0f;
    return IM_COL32(std::min(255, static_cast<int>(r * m)), std::min(255, static_cast<int>(g * m)), std::min(255, static_cast<int>(bl * m)), 255);
  };
  for (int i = 6; i >= 1; --i) dl->AddRectFilled({a.x - i, a.y + i * 1.6f}, {b.x + i, b.y + i * 1.8f}, IM_COL32(0, 0, 0, 22), 3.0f + i);
  if (primary) for (int i = 4; i >= 1; --i) dl->AddRect({a.x - i * 4.0f, a.y - i * 4.0f}, {b.x + i * 4.0f, b.y + i * 4.0f}, IM_COL32(255, 110, 30, hover ? 22 : 12), 4.0f + i * 4.0f, 4.0f);
  dl->AddRect({a.x - 1, a.y - 1}, {b.x + 1, b.y + 1}, IM_COL32(0, 0, 0, 255), 2.0f);
  if (primary) {
    verticalGradient(dl, a, b, {{0.0f, k(0xd9, 0x62, 0x1c)}, {0.55f, k(0x9c, 0x2f, 0x10)}, {1.0f, k(0x56, 0x17, 0x08)}});
    dl->AddLine({a.x + 1, a.y + 1}, {b.x - 1, a.y + 1}, IM_COL32(255, 225, 160, 140));
    dl->AddRect(a, b, IM_COL32(255, 200, 130, 128), 2.0f);
  } else {
    verticalGradient(dl, a, b, {{0.0f, k(0x23, 0x23, 0x23)}, {0.55f, k(0x0a, 0x0a, 0x0a)}, {1.0f, k(0x1a, 0x1a, 0x1a)}});
    dl->AddRect(a, b, hover ? IM_COL32(226, 170, 100, 216) : IM_COL32(190, 150, 95, 128), 2.0f);
  }
  // the gold diamond caps on both ends
  for (int side = 0; side < 2; ++side) {
    const float cx = side ? b.x + 1.0f : a.x - 1.0f, cy = (a.y + b.y) * 0.5f, r = 6.0f;
    const ImVec2 d[4] = {{cx, cy - r}, {cx + r, cy}, {cx, cy + r}, {cx - r, cy}};
    dl->AddConvexPolyFilled(d, 4, IM_COL32(0, 0, 0, 255));
    dl->AddQuadFilled({cx, cy - r + 1}, {cx + r - 1, cy}, {cx, cy + r - 1}, {cx - r + 1, cy}, IM_COL32(176, 146, 100, 255));
  }
  ImFont* f = uiFont(UiFont::Mark);
  const float tw = spacedTextWidth(f, fontSize, label, 0.2f);
  const ImVec2 p{std::round((a.x + b.x - tw) * 0.5f), std::round((a.y + b.y - fontSize) * 0.5f)};
  spacedText(dl, f, fontSize, {p.x, p.y + 1}, primary ? IM_COL32(50, 8, 0, 230) : IM_COL32(0, 0, 0, 255), label, 0.2f, false);
  spacedText(dl, f, fontSize, p, primary ? IM_COL32(255, 241, 216, 255) : hover ? IM_COL32(255, 224, 180, 255) : IM_COL32(226, 194, 148, 255), label, 0.2f, false);
  return clicked;
}

}  // namespace ember
