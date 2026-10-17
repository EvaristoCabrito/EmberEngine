#include "render/UiArt.h"

#include <algorithm>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <imgui_internal.h>
#include <stb_image.h>

#include "core/Log.h"

namespace ember {

namespace {

ImFont* gFonts[3] = {};

struct LoadedImage {
  std::unique_ptr<ImTextureData> data;
  UiImage image;
  bool failed = false;
};
std::map<std::string, LoadedImage>& images() {
  static std::map<std::string, LoadedImage> cache;
  return cache;
}
// One key per file however its path was written ("a/b\c.png" and "a/b/c.png" are the same picture).
std::string keyOf(const std::filesystem::path& file) { return file.lexically_normal().generic_string(); }

}  // namespace

void loadUiFonts() {
  ImGuiIO& io = ImGui::GetIO();
  const char* files[3] = {"content/fonts/Figtree.ttf", "content/fonts/Newsreader.ttf", "content/fonts/PirataOne.ttf"};
  for (int i = 0; i < 3; ++i) {
    std::error_code error;
    if (std::filesystem::exists(files[i], error)) gFonts[i] = io.Fonts->AddFontFromFileTTF(files[i], 18.0f);
    else logError("UI font not found: {}", files[i]);
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
  return &slot.image;
}

}  // namespace

const UiImage* uiImage(const std::filesystem::path& file) {
  const std::string key = keyOf(file);
  if (const auto it = images().find(key); it != images().end() && (it->second.data || it->second.failed))
    return it->second.failed ? nullptr : &it->second.image;
  int w = 0, h = 0, n = 0;
  stbi_uc* pixels = stbi_load(file.string().c_str(), &w, &h, &n, 4);
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
      d.pixels = stbi_load(f.string().c_str(), &d.w, &d.h, &d.n, 4);
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
