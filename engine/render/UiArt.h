#pragma once

// The game's look in its menus and HUD: Engine2's own fonts (title.css: Newsreader for titles, Figtree for text,
// Pirata One for marks and buttons; bundled in content/fonts, OFL) and its art, read at run time from Engine2's public
// folder (title backgrounds, panels, portraits) and shown through Dear ImGui.

#include <filesystem>

#include <imgui.h>

namespace ember {

enum class UiFont { Ui, Display, Mark };
// The font, or ImGui's default when its file is missing. Sizes are given per push (ImGui 1.93: PushFont(font, px)).
ImFont* uiFont(UiFont font);
// Called once by DebugUi while it builds the font atlas.
void loadUiFonts();

struct UiImage {
  ImTextureRef texture;
  int width = 0, height = 0;
};
// An image file (PNG / JPG), loaded on first use and kept; null when it cannot be read.
const UiImage* uiImage(const std::filesystem::path& file);

// Engine2's title.css helpers for an ImGui draw list.
// A vertical gradient through colour stops (position 0..1, colour).
struct GradientStop {
  float at;
  ImU32 color;
};
void verticalGradient(ImDrawList* dl, ImVec2 a, ImVec2 b, std::initializer_list<GradientStop> stops);
// An image covering the rectangle (CSS background-size: cover), its focus at (fx, fy) of the image (0.5, 0.42 = "center 42%").
void imageCover(ImDrawList* dl, const UiImage& image, ImVec2 a, ImVec2 b, float fx = 0.5f, float fy = 0.5f, ImU32 tint = IM_COL32_WHITE);
// Text with letter spacing (em of the font size) and an optional soft shadow (CSS text-shadow 0 2px 6px #000).
float spacedTextWidth(ImFont* font, float size, const char* text, float spacingEm);
void spacedText(ImDrawList* dl, ImFont* font, float size, ImVec2 at, ImU32 color, const char* text, float spacingEm, bool shadow = true);

}  // namespace ember
