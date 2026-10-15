#include "render/UiArt.h"

#include <algorithm>
#include <cstring>
#include <map>
#include <memory>
#include <string>

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

const UiImage* uiImage(const std::filesystem::path& file) {
  LoadedImage& slot = images()[file.string()];
  if (slot.failed) return nullptr;
  if (slot.data) return &slot.image;
  int w = 0, h = 0, n = 0;
  stbi_uc* pixels = stbi_load(file.string().c_str(), &w, &h, &n, 4);
  if (!pixels) {
    logError("UI image not found: {}", file.string());
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

}  // namespace ember
