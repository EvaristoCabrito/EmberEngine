#pragma once

// Engine2's menu windows, copied with their own design (src/ember-ui/styles.css .game-options / .ember-btn / .ember-veil
// / .status-panel, Tailwind sizes in CSS px = UI canvas units):
//   HelpModal       "Como jogar" (GameApp.tsx HelpModal): Básicos, Usos, Dano, Fórmulas, Loot. Opened by the title's
//                   Como jogar (GameApp's battle Regras button is left out, user 2026-10-16). Its texts and tables come from content/engine2/help.json
//                   (tools/scripts/engine2/export_help.ts: every number computed by Engine2's own rules).
//   OptionsDialog   "Opções" (OptionsMenu.tsx OptionsDialog): Jogo (Salvar / Carregar / Voltar à tela inicial, when the
//                   screen has them), Gráficos, Áudio, Idiomas, Acessibilidade, "Alterações salvas automaticamente."
//                   Opened by the title, the world maps, Modo teste and the battle's Esc menu.
//   BattleMenu      the battle's Esc menu (GameApp.tsx BattleScreen `paused`): Velocidade, Áudio, Opções, Continuar,
//                   Save, Load, Desistir / Encerrar teste, Tela inicial.
// The player's choices (Engine2 gamePreferences.ts + audio.ts) live in %LOCALAPPDATA%\Ember\game-preferences.json; the
// graphics switches are Dev Controls' own (DevGfxSettings.h, dev-gfx.json).
//
// What each choice does here: Sombras / Luzes locais / FX atmosféricos act on the map (live in a battle); Tela cheia;
// Som das cutscenes, Volume da cutscene, Exibir legendas and Idioma das legendas act on the films (CutsceneScreen);
// Tamanho do diálogo and Diálogos on the conversations (DialogScreen: a line's own English when Engine2 has one, else
// its Portuguese, Engine2's translatedText); Interface e nomes switches these windows, the title and the save slots
// between Portuguese and English (tr(): Engine2's dictionary and the English its screens give inline; anything without
// an English text stays Portuguese, Engine2's rule). Silenciar, Música and Efeitos sonoros act on the game's sound
// (engine/media/AudioMixer, read every frame by the game loop).
// Not ported: the battle menu's Zoom (left for later, user 2026-10-16).

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <numbers>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "CampaignUi.h"
#include "DevGfxSettings.h"
#include "GamePrefs.h"
#include "render/UiArt.h"

namespace ember {

namespace menu {

// --- colours (styles.css @theme + .game-options) ------------------------------------------------------------------------
inline constexpr ImU32 kFg = IM_COL32(240, 240, 240, 255);         // --color-fg
inline constexpr ImU32 kMuted = IM_COL32(180, 180, 180, 255);      // --color-muted
inline constexpr ImU32 kAccent = IM_COL32(216, 216, 216, 255);     // --color-accent
inline constexpr ImU32 kBorder = IM_COL32(74, 74, 74, 255);        // --color-border
inline constexpr ImU32 kBorder60 = IM_COL32(74, 74, 74, 153);      // border-border/60
inline constexpr ImU32 kBg = IM_COL32(17, 17, 17, 255);            // --color-bg
inline constexpr ImU32 kOptText = IM_COL32(234, 220, 201, 255);    // .game-options color
inline constexpr ImU32 kOptMuted = IM_COL32(183, 164, 140, 255);   // .game-options .text-muted
inline constexpr ImU32 kGold = IM_COL32(196, 156, 105, 255);       // headings' icons, select chevron, legends
inline constexpr ImU32 kTitle = IM_COL32(241, 220, 192, 255);      // .ember-title

inline ImFont* ui() { return uiFont(UiFont::Ui); }
inline ImFont* mark() { return uiFont(UiFont::Mark); }

// --- lucide icons used by the menus (the options headings: stroke 1.5, styles.css .game-options-heading svg) -----------
inline void iconMonitor(ImDrawList* dl, ImVec2 c, float s, ImU32 ink) { uiIcon(dl, "monitor", c, s, ink, 1.5f); }
inline void iconVolume(ImDrawList* dl, ImVec2 c, float s, ImU32 ink) { uiIcon(dl, "volume-2", c, s, ink, 1.5f); }
inline void iconLanguages(ImDrawList* dl, ImVec2 c, float s, ImU32 ink) { uiIcon(dl, "languages", c, s, ink, 1.5f); }
inline void iconSliders(ImDrawList* dl, ImVec2 c, float s, ImU32 ink) { uiIcon(dl, "sliders-horizontal", c, s, ink); }
inline void iconSwords(ImDrawList* dl, ImVec2 c, float s, ImU32 ink) { uiIcon(dl, "swords", c, s, ink); }
inline void iconChevronDown(ImDrawList* dl, ImVec2 c, float s, ImU32 ink) { uiIcon(dl, "chevron-down", c, s, ink); }

// --- text -------------------------------------------------------------------------------------------------------------

struct Span {
  std::string text;
  ImU32 color;
};
// Spans laid word by word inside `width` (CSS normal wrapping); drawn when `dl` is given. Returns the height used.
inline float richText(ImDrawList* dl, ImFont* font, float size, float lineHeight, ImVec2 at, float width, const std::vector<Span>& spans) {
  float x = 0.0f, y = 0.0f;
  bool lineEmpty = true;
  const float baseline = std::round((lineHeight - size) * 0.5f);
  for (const Span& s : spans) {
    size_t i = 0;
    const std::string& t = s.text;
    while (i < t.size()) {
      if (t[i] == '\n') {
        x = 0.0f, y += lineHeight, lineEmpty = true, ++i;
        continue;
      }
      size_t j = i;
      while (j < t.size() && t[j] != ' ' && t[j] != '\n') ++j;
      size_t k = j;
      while (k < t.size() && t[k] == ' ') ++k;
      const char* w0 = t.data() + i;
      const float word = font->CalcTextSizeA(size, FLT_MAX, 0.0f, w0, t.data() + j).x;
      const float full = font->CalcTextSizeA(size, FLT_MAX, 0.0f, w0, t.data() + k).x;
      if (!lineEmpty && x + word > width + 0.5f) x = 0.0f, y += lineHeight, lineEmpty = true;
      if (lineEmpty && j == i) {  // spaces at the start of a line are dropped
        i = k;
        continue;
      }
      if (dl) dl->AddText(font, size, {std::round(at.x + x), at.y + y + baseline}, s.color, w0, t.data() + k);
      x += full, lineEmpty = false;
      i = k;
    }
  }
  return lineEmpty && x == 0.0f && y > 0.0f ? y : y + lineHeight;
}
inline float textWidth(ImFont* f, float size, const std::string& s) { return f->CalcTextSizeA(size, FLT_MAX, 0.0f, s.c_str()).x; }
// `size`, or smaller when the text would not fit in `room` (the boxes keep their size).
inline float fitSize(ImFont* f, float size, const std::string& s, float room) {
  const float w = textWidth(f, size, s);
  return w > room && w > 0.0f ? size * room / w : size;
}
// The Opções window's text, 1.4x Engine2's (user 2026-10-16: "all fonts too small"; boxes unchanged).
inline constexpr float kOptScale = 1.4f;
// Text with letter spacing (em), as spacedText but without its shadow.
inline void tracked(ImDrawList* dl, ImFont* f, float size, ImVec2 at, ImU32 ink, const std::string& s, float em) {
  spacedText(dl, f, size, at, ink, s.c_str(), em, false);
}

// --- backdrops ----------------------------------------------------------------------------------------------------------

// .ember-veil: radial darkening to the edges over a top-to-bottom shade.
inline void emberVeil(ImDrawList* dl, ImVec2 a, ImVec2 b) {
  verticalGradient(dl, a, b, {{0.0f, IM_COL32(0, 0, 0, 89)}, {1.0f, IM_COL32(0, 0, 0, 184)}});
  constexpr int n = 12;
  const ImVec2 size{b.x - a.x, b.y - a.y};
  auto alpha = [&](int i, int j) {
    const float u = static_cast<float>(i) / n * 2.0f - 1.0f, v = static_cast<float>(j) / n * 2.0f - 1.0f;
    const float d = std::sqrt(u * u + v * v) / std::numbers::sqrt2_v<float>;  // 0 centre, 1 corner
    const float k = std::clamp((d - 0.2f) / 0.8f, 0.0f, 1.0f);
    return IM_COL32(0, 0, 0, static_cast<int>(140.0f * k));
  };
  for (int j = 0; j < n; ++j)
    for (int i = 0; i < n; ++i) {
      const ImVec2 p{a.x + size.x * i / n, a.y + size.y * j / n}, q{a.x + size.x * (i + 1) / n, a.y + size.y * (j + 1) / n};
      dl->AddRectFilledMultiColor(p, q, alpha(i, j), alpha(i + 1, j), alpha(i + 1, j + 1), alpha(i, j + 1));
    }
}

// --- controls (.ember-btn, .game-options inputs) ------------------------------------------------------------------------

enum class Btn { Plain, Primary, Ghost };
// An .ember-btn at the cursor: Pirata One, spaced capitals with the gold diamond caps (full), or .ember-btn-sm (letter
// spacing .06em, no capitals, no caps). Plain = .ember-btn alone (no plate: the Opções window's Jogo buttons).
inline bool emberButton(const char* id, const std::string& label, ImVec2 size, Btn style, bool small, float fontSize, bool disabled = false,
                        ImU32 plainInk = kOptText) {
  ImGui::BeginDisabled(disabled);
  const bool clicked = ImGui::InvisibleButton(id, size);
  ImGui::EndDisabled();
  const bool hover = !disabled && ImGui::IsItemHovered();
  const ImVec2 a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax();
  ImDrawList* dl = ImGui::GetWindowDrawList();
  auto k = [&](int r, int g, int bl) {
    const float m = hover && style == Btn::Primary ? 1.1f : 1.0f;
    return IM_COL32(std::min(255, static_cast<int>(r * m)), std::min(255, static_cast<int>(g * m)), std::min(255, static_cast<int>(bl * m)), disabled ? 102 : 255);
  };
  if (style != Btn::Plain) {
    for (int i = 5; i >= 1; --i) dl->AddRectFilled({a.x - i, a.y + i * 1.5f}, {b.x + i, b.y + i * 1.7f}, IM_COL32(0, 0, 0, 20), 2.0f + i);
    if (style == Btn::Primary)
      for (int i = 3; i >= 1; --i) dl->AddRect({a.x - i * 4.0f, a.y - i * 4.0f}, {b.x + i * 4.0f, b.y + i * 4.0f}, IM_COL32(255, 110, 30, hover ? 22 : 12), 4.0f + i * 4.0f, 4.0f);
    else if (hover)
      for (int i = 3; i >= 1; --i) dl->AddRect({a.x - i * 3.0f, a.y - i * 3.0f}, {b.x + i * 3.0f, b.y + i * 3.0f}, IM_COL32(255, 110, 40, 14), 3.0f + i * 3.0f, 3.0f);
    dl->AddRect({a.x - 1, a.y - 1}, {b.x + 1, b.y + 1}, IM_COL32(0, 0, 0, 255), 2.0f);
    if (style == Btn::Primary) {
      verticalGradient(dl, a, b, {{0.0f, k(0xd9, 0x62, 0x1c)}, {0.55f, k(0x9c, 0x2f, 0x10)}, {1.0f, k(0x56, 0x17, 0x08)}});
      dl->AddLine({a.x + 1, a.y + 1}, {b.x - 1, a.y + 1}, IM_COL32(255, 225, 160, 140));
      dl->AddRectFilledMultiColor({a.x, b.y - 4}, b, IM_COL32(50, 8, 0, 0), IM_COL32(50, 8, 0, 0), IM_COL32(50, 8, 0, 153), IM_COL32(50, 8, 0, 153));
      dl->AddRect(a, b, IM_COL32(255, 200, 130, 128), 2.0f);
    } else {
      verticalGradient(dl, a, b, {{0.0f, k(0x23, 0x23, 0x23)}, {0.55f, k(0x0a, 0x0a, 0x0a)}, {1.0f, k(0x1a, 0x1a, 0x1a)}});
      dl->AddRect({a.x + 1, a.y + 1}, {b.x - 1, b.y - 1}, IM_COL32(255, 220, 160, hover ? 26 : 18), 2.0f);
      dl->AddRect(a, b, hover ? IM_COL32(226, 170, 100, 217) : IM_COL32(190, 150, 95, 128), 2.0f);
      if (hover) {  // the ember underline that lights on hover
        const float x0 = a.x + (b.x - a.x) * 0.18f, x1 = b.x - (b.x - a.x) * 0.18f, m = (x0 + x1) * 0.5f, y = b.y - 4;
        dl->AddRectFilledMultiColor({x0, y}, {m, y + 1}, IM_COL32(226, 104, 30, 0), IM_COL32(226, 104, 30, 255), IM_COL32(226, 104, 30, 255), IM_COL32(226, 104, 30, 0));
        dl->AddRectFilledMultiColor({m, y}, {x1, y + 1}, IM_COL32(226, 104, 30, 255), IM_COL32(226, 104, 30, 0), IM_COL32(226, 104, 30, 0), IM_COL32(226, 104, 30, 255));
      }
    }
  }
  if (!small)  // the gold diamond caps on both ends
    for (int side = 0; side < 2; ++side) {
      const float cx = side ? b.x + 1.0f : a.x - 1.0f, cy = (a.y + b.y) * 0.5f, r = 6.0f;
      const ImVec2 d[4] = {{cx, cy - r}, {cx + r, cy}, {cx, cy + r}, {cx - r, cy}};
      dl->AddConvexPolyFilled(d, 4, IM_COL32(0, 0, 0, disabled ? 102 : 255));
      dl->AddQuadFilled({cx, cy - r + 1}, {cx + r - 1, cy}, {cx, cy + r - 1}, {cx - r + 1, cy}, IM_COL32(176, 146, 100, disabled ? 102 : 255));
    }
  const std::string text = small ? label : upperUtf8(label);
  const float em = small ? 0.06f : 0.2f;
  const float tw = spacedTextWidth(mark(), fontSize, text.c_str(), em);
  const ImVec2 p{std::round((a.x + b.x - tw) * 0.5f), std::round((a.y + b.y - fontSize) * 0.5f)};
  ImU32 ink = style == Btn::Primary ? IM_COL32(255, 241, 216, 255) : style == Btn::Ghost ? (hover ? IM_COL32(255, 224, 180, 255) : IM_COL32(226, 194, 148, 255))
                                                                 : (hover ? IM_COL32(255, 224, 180, 255) : plainInk);
  if (disabled) ink = (ink & 0x00FFFFFFu) | (102u << 24);
  tracked(dl, mark(), fontSize, {p.x, p.y + 1}, style == Btn::Primary ? IM_COL32(50, 8, 0, 230) : IM_COL32(0, 0, 0, 255), text, em);
  tracked(dl, mark(), fontSize, p, ink, text, em);
  return clicked && !disabled;
}

// .ember-icon-btn with lucide X (32 px: size-8, icon size-4).
inline bool iconClose(const char* id, float size = 32.0f) {
  return emberPlate(id, {size, size}, [&](ImDrawList* d, ImVec2 a, ImVec2 b, ImU32 ink) {
    iconX(d, {(a.x + b.x) / 2, (a.y + b.y) / 2}, 16, ink == IM_COL32(240, 240, 240, 255) ? IM_COL32(223, 191, 142, 255) : ink);
  });
}

// .game-options checkbox: a dark inset well; checked = the primary button's molten fill with a light tick (1rem).
inline bool emberCheckbox(const char* id, bool& value, bool disabled = false) {
  ImGui::BeginDisabled(disabled);
  const bool clicked = ImGui::InvisibleButton(id, {16, 16});
  ImGui::EndDisabled();
  const ImVec2 a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax();
  ImDrawList* dl = ImGui::GetWindowDrawList();
  const int al = disabled ? 102 : 255;
  dl->AddRect({a.x - 1, a.y - 1}, {b.x + 1, b.y + 1}, IM_COL32(0, 0, 0, al), 2.0f);
  if (value) {
    for (int i = 3; i >= 1; --i) dl->AddRect({a.x - i * 2.0f, a.y - i * 2.0f}, {b.x + i * 2.0f, b.y + i * 2.0f}, IM_COL32(255, 110, 30, disabled ? 8 : 22), 2.0f + i * 2.0f, 2.0f);
    verticalGradient(dl, a, b, {{0.0f, IM_COL32(0xd9, 0x62, 0x1c, al)}, {0.55f, IM_COL32(0x9c, 0x2f, 0x10, al)}, {1.0f, IM_COL32(0x56, 0x17, 0x08, al)}});
    dl->AddLine({a.x + 1, a.y + 1}, {b.x - 1, a.y + 1}, IM_COL32(255, 225, 160, disabled ? 45 : 115));
    dl->AddRect(a, b, IM_COL32(255, 200, 130, disabled ? 60 : 153), 2.0f);
    // Engine2's tick (styles.css .game-options input:checked): its own SVG at 0.8 rem.
    uiIconSvg(dl, "options-tick",
        "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' fill='none' stroke='#ffffff' stroke-width='3.2' "
        "stroke-linecap='round' stroke-linejoin='round'><path d='m5 12 4 4L19 6'/></svg>",
        {(a.x + b.x) / 2, (a.y + b.y) / 2}, 12.8f, IM_COL32(255, 241, 216, al));
  } else {
    dl->AddRectFilled(a, b, IM_COL32(5, 5, 5, al), 2.0f);
    dl->AddRectFilledMultiColor(a, {b.x, a.y + 5}, IM_COL32(0, 0, 0, al), IM_COL32(0, 0, 0, al), IM_COL32(0, 0, 0, 0), IM_COL32(0, 0, 0, 0));
    dl->AddRect(a, b, IM_COL32(190, 150, 95, disabled ? 56 : 140), 2.0f);
  }
  if (clicked && !disabled) value = !value;
  return clicked && !disabled;
}

// .game-options range: the loader bar's track and molten fill with its gold diamond knob (8 px tall).
inline bool emberRange(const char* id, float& value, float width, bool disabled = false) {
  const ImVec2 at = ImGui::GetCursorScreenPos();
  ImGui::BeginDisabled(disabled);
  ImGui::InvisibleButton(id, {width, 14});
  ImGui::EndDisabled();
  bool changed = false;
  if (!disabled && ImGui::IsItemActive()) {
    const float v = std::clamp((ImGui::GetIO().MousePos.x - at.x) / width, 0.0f, 1.0f);
    const float stepped = std::round(v * 100.0f) / 100.0f;  // step 0.01
    if (stepped != value) value = stepped, changed = true;
  }
  ImDrawList* dl = ImGui::GetWindowDrawList();
  const int al = disabled ? 102 : 255;
  const ImVec2 a{at.x, at.y + 3}, b{at.x + width, at.y + 11};
  dl->AddRect({a.x - 1, a.y - 1}, {b.x + 1, b.y + 1}, IM_COL32(0, 0, 0, al), 2.0f);
  dl->AddRectFilled(a, b, IM_COL32(5, 5, 5, al), 2.0f);
  const float fx = a.x + (b.x - a.x) * value;
  if (fx > a.x + 0.5f) {
    // linear-gradient(90deg, #4a1408 0%, #a33210 45%, #e2681e 85%, #ffbe55 100%) sized to the fill
    const float w = fx - a.x;
    const struct { float at; ImU32 c; } stops[4] = {{0.0f, IM_COL32(0x4a, 0x14, 0x08, al)}, {0.45f, IM_COL32(0xa3, 0x32, 0x10, al)}, {0.85f, IM_COL32(0xe2, 0x68, 0x1e, al)}, {1.0f, IM_COL32(0xff, 0xbe, 0x55, al)}};
    for (int i = 0; i < 3; ++i)
      dl->AddRectFilledMultiColor({a.x + w * stops[i].at, a.y}, {a.x + w * stops[i + 1].at, b.y}, stops[i].c, stops[i + 1].c, stops[i + 1].c, stops[i].c);
  }
  dl->AddRectFilledMultiColor(a, {b.x, a.y + 3}, IM_COL32(0, 0, 0, disabled ? 80 : 204), IM_COL32(0, 0, 0, disabled ? 80 : 204), IM_COL32(0, 0, 0, 0), IM_COL32(0, 0, 0, 0));
  dl->AddRect(a, b, IM_COL32(190, 150, 95, disabled ? 46 : 115), 2.0f);
  // knob: a 13 px square turned 45 degrees, gold to iron
  const ImVec2 c{std::clamp(fx, a.x + 4.0f, b.x - 4.0f), (a.y + b.y) * 0.5f};
  const float r = 9.0f;
  for (int i = 3; i >= 1; --i) dl->AddCircleFilled(c, r + i * 2.0f, IM_COL32(255, 140, 50, disabled ? 6 : 20));
  const ImVec2 d[4] = {{c.x, c.y - r}, {c.x + r, c.y}, {c.x, c.y + r}, {c.x - r, c.y}};
  dl->AddConvexPolyFilled(d, 4, IM_COL32(0, 0, 0, al));
  const ImVec2 e[4] = {{c.x, c.y - r + 1}, {c.x + r - 1, c.y}, {c.x, c.y + r - 1}, {c.x - r + 1, c.y}};
  const ImU32 light = IM_COL32(240, 210, 156, al), mid = IM_COL32(150, 128, 98, al), dark = IM_COL32(59, 59, 59, al);
  dl->AddQuadFilled(e[3], e[0], {c.x, c.y}, {c.x, c.y}, light);
  dl->AddQuadFilled(e[0], e[1], {c.x, c.y}, {c.x, c.y}, mid);
  dl->AddQuadFilled(e[3], e[2], {c.x, c.y}, {c.x, c.y}, mid);
  dl->AddQuadFilled(e[1], e[2], {c.x, c.y}, {c.x, c.y}, dark);
  return changed;
}

// .game-options select: min(55%, 11rem) wide, 1.9rem tall, .8rem text, the gold chevron; its list opens below it.
// `options`: label + whether it can be picked (a disabled one is listed greyed).
inline bool emberSelect(const char* id, int& value, const std::vector<std::pair<std::string, bool>>& options, float width) {
  const float h = 30.4f, text = 12.8f * kOptScale;
  const bool clicked = ImGui::InvisibleButton(id, {width, h});
  const bool hover = ImGui::IsItemHovered();
  const ImVec2 a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax();
  ImDrawList* dl = ImGui::GetWindowDrawList();
  dl->AddRect({a.x - 1, a.y - 1}, {b.x + 1, b.y + 1}, IM_COL32(0, 0, 0, 255), 2.0f);
  dl->AddRectFilled(a, b, IM_COL32(8, 8, 8, 255), 2.0f);
  dl->AddRectFilledMultiColor(a, {b.x, a.y + 6}, IM_COL32(0, 0, 0, 255), IM_COL32(0, 0, 0, 255), IM_COL32(0, 0, 0, 0), IM_COL32(0, 0, 0, 0));
  dl->AddRect(a, b, hover ? IM_COL32(226, 170, 100, 191) : IM_COL32(190, 150, 95, 89), 2.0f);
  const std::string& label = options[static_cast<size_t>(std::clamp(value, 0, static_cast<int>(options.size()) - 1))].first;
  dl->PushClipRect(a, {b.x - 28, b.y}, true);
  const float fit = fitSize(ui(), text, label, width - 8.8f - 28.0f);
  dl->AddText(ui(), fit, {a.x + 8.8f, std::round((a.y + b.y - fit) * 0.5f)}, kOptText, label.c_str());
  dl->PopClipRect();
  iconChevronDown(dl, {b.x - 9.6f - 7.2f, (a.y + b.y) * 0.5f}, 14.4f, kGold);
  const std::string popup = std::string(id) + "_list";
  if (clicked) ImGui::OpenPopup(popup.c_str());
  bool changed = false;
  ImGui::SetNextWindowPos({a.x, b.y + 2});
  ImGui::SetNextWindowSize({width, 0});
  ImGui::PushStyleColor(ImGuiCol_PopupBg, IM_COL32(15, 15, 15, 250));
  ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(190, 150, 95, 89));
  ImGui::PushStyleColor(ImGuiCol_Header, IM_COL32(54, 54, 54, 255));
  ImGui::PushStyleColor(ImGuiCol_HeaderHovered, IM_COL32(48, 40, 30, 255));
  ImGui::PushStyleColor(ImGuiCol_HeaderActive, IM_COL32(64, 50, 34, 255));
  ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize, 1.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {4, 4});
  if (ImGui::BeginPopup(popup.c_str())) {
    ImGui::PushFont(ui(), text);
    for (size_t i = 0; i < options.size(); ++i) {
      ImGui::BeginDisabled(!options[i].second);
      ImGui::PushStyleColor(ImGuiCol_Text, kOptText);
      if (ImGui::Selectable(options[i].first.c_str(), static_cast<int>(i) == value, 0, {0, 24}) && options[i].second) {
        changed = value != static_cast<int>(i);
        value = static_cast<int>(i);
      }
      ImGui::PopStyleColor();
      ImGui::EndDisabled();
    }
    ImGui::PopFont();
    ImGui::EndPopup();
  }
  ImGui::PopStyleVar(2);
  ImGui::PopStyleColor(5);
  return changed;
}

// --- HelpModal ------------------------------------------------------------------------------------------------------------

class HelpModal {
 public:
  // content/engine2/help.json (export_help.ts), read once.
  static const nlohmann::json& data() {
    static const nlohmann::json j = [] {
      std::ifstream in("content/engine2/help.json", std::ios::binary);
      nlohmann::json v = in ? nlohmann::json::parse(in, nullptr, false) : nlohmann::json();
      return v.is_object() ? v : nlohmann::json::object();
    }();
    return j;
  }

  void open(int tab = 0) {
    if (!open_) openedFrame_ = ImGui::GetCurrentContext() ? ImGui::GetFrameCount() : -1;
    open_ = true, tab_ = tab, scrollTop_ = true;
  }
  void close() { open_ = false; }
  bool isOpen() const { return open_; }
  // Captures: a tab shown, scrolled to `scroll` canvas units.
  void preview(int tab, float scroll) { open(tab), previewScroll_ = scroll; }

  // Drawn over everything (its own window); Esc and the X / Entendi close it.
  void draw(ImVec2 screen) {
    if (!open_) return;
    if (ImGui::IsKeyPressed(ImGuiKey_Escape, false) && ImGui::GetFrameCount() != openedFrame_) {
      open_ = false;
      return;
    }
    static const nlohmann::json kEmpty = nlohmann::json::object(), kNone = nlohmann::json::array();
    const nlohmann::json& all = data();
    const auto lang = all.find(uiEnglish() ? "en" : "pt");
    const nlohmann::json& d = lang != all.end() && lang->is_object() ? *lang : kEmpty;
    const auto tabsIt = d.find("tabs");
    const nlohmann::json& tabs = tabsIt != d.end() && tabsIt->is_array() ? *tabsIt : kNone;
    tab_ = std::clamp(tab_, 0, std::max(0, static_cast<int>(tabs.size()) - 1));

    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize(screen);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(0, 0, 0, 0));
    ImGui::Begin("##helpmodal", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::SetWindowFocus();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    emberVeil(dl, {0, 0}, screen);
    ImGui::SetCursorScreenPos({0, 0});
    // the veil takes the clicks (nothing under it reacts); overlappable, so the window's own buttons drawn after it get theirs
    ImGui::SetNextItemAllowOverlap();
    ImGui::InvisibleButton("##helpveil", screen);

    // max-w-lg, max-h-[85dvh], p-6; header mb-4, tabs mb-4, the scrolling area, Entendi mt-5.
    const float pad = 24.0f, w = std::min(512.0f, screen.x - 32.0f), inner = w - 2 * pad;
    const float headH = 32.0f, tabsH = 36.0f, buttonH = 44.0f;
    const int cols = screen.x >= 640.0f ? 5 : 3;
    const int tabRows = (static_cast<int>(tabs.size()) + cols - 1) / cols;
    const float tabsBlock = tabRows * tabsH + (tabRows - 1) * 4.0f;
    const float fixed = pad + headH + 16.0f + tabsBlock + 16.0f + 20.0f + buttonH + pad;
    const float scrollW = inner - 4.0f;  // pr-1
    const nlohmann::json* blocksIt = nullptr;
    if (!tabs.empty()) {
      const nlohmann::json& tab = tabs[static_cast<size_t>(tab_)];
      if (const auto it = tab.find("blocks"); it != tab.end() && it->is_array()) blocksIt = &*it;
    }
    const nlohmann::json& blocks = blocksIt ? *blocksIt : kNone;
    const float contentH = layout(nullptr, {0, 0}, scrollW - 14.0f, blocks);
    const float maxH = screen.y * 0.85f;
    const float areaH = std::max(80.0f, std::min(contentH, maxH - fixed));
    const float h = fixed + areaH;
    // items-end on narrow screens, centred from sm (640) up
    const ImVec2 a{std::round((screen.x - w) * 0.5f), std::round(screen.x >= 640.0f ? (screen.y - h) * 0.5f : screen.y - 16.0f - h)};
    const ImVec2 b{a.x + w, a.y + h};
    emberPanel(dl, a, b);

    // header: "Como jogar" (font-display text-2xl ember-title) and the X
    emberTitle(dl, {a.x + pad, a.y + pad + 2.0f}, 24.0f, d.value("title", tr("Como jogar")).c_str());
    ImGui::SetCursorScreenPos({b.x - pad - 32.0f, a.y + pad});
    if (iconClose("##helpx")) open_ = false;

    // tabs: grid-cols-3 sm:grid-cols-5 gap-1, Button sm (primary when on, ghost otherwise)
    const float tabY = a.y + pad + headH + 16.0f;
    const float tabW = (inner - (cols - 1) * 4.0f) / cols;
    for (size_t i = 0; i < tabs.size(); ++i) {
      const int c = static_cast<int>(i) % cols, r = static_cast<int>(i) / cols;
      ImGui::SetCursorScreenPos({a.x + pad + c * (tabW + 4.0f), tabY + r * (tabsH + 4.0f)});
      ImGui::PushID(static_cast<int>(i));
      if (emberButton("##tab", tabs[i].value("label", std::string()), {tabW, tabsH}, static_cast<int>(i) == tab_ ? Btn::Primary : Btn::Ghost, true, 16.0f) &&
          tab_ != static_cast<int>(i))
        tab_ = static_cast<int>(i), scrollTop_ = true;
      ImGui::PopID();
    }

    // the scrolling area (.ember-scrollbar)
    const float areaY = tabY + tabsBlock + 16.0f;
    ImGui::SetCursorScreenPos({a.x + pad, areaY});
    ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, IM_COL32(14, 14, 14, 255));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab, IM_COL32(93, 93, 93, 255));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, IM_COL32(120, 120, 120, 255));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabActive, IM_COL32(140, 140, 140, 255));
    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 14.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarRounding, 0.0f);
    ImGui::BeginChild("##helparea", {scrollW, areaH}, 0, ImGuiWindowFlags_None);
    if (scrollTop_) ImGui::SetScrollY(0.0f), scrollTop_ = false;
    if (previewScroll_ > 0.0f) ImGui::SetScrollY(previewScroll_), previewScroll_ = 0.0f;
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const float used = layout(ImGui::GetWindowDrawList(), origin, scrollW - 14.0f, blocks);
    ImGui::Dummy({1.0f, used});
    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(5);

    // Entendi: Button md primary, full width
    ImGui::SetCursorScreenPos({a.x + pad, b.y - pad - buttonH});
    if (emberButton("##helpok", d.value("close", tr("Entendi")), {inner, buttonH}, Btn::Primary, false, 20.0f)) open_ = false;
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
  }

 private:
  using Json = nlohmann::json;

  static std::vector<Span> spansOf(const Json& list, ImU32 base) {
    std::vector<Span> out;
    for (const Json& s : list) {
      ImU32 c = base;
      if (s.value("accent", false)) c = kAccent;
      else if (s.value("muted", false)) c = kMuted;
      else if (s.value("medium", false)) c = kFg;
      out.push_back({s.value("text", std::string()), c});
    }
    return out;
  }

  // The instructions' text, 2 px over Engine2's sizes (user 2026-10-16).
  static constexpr float kPlus = 2.0f;

  // One tab's blocks top to bottom (draws when `dl` is given); returns the height.
  static float layout(ImDrawList* dl, ImVec2 at, float width, const Json& blocks) {
    float y = 0.0f;
    bool first = true;
    for (const Json& bl : blocks) {
      const std::string t = bl.value("t", std::string());
      const float mt = first ? 0.0f : bl.value("mt", 0.0f) * 16.0f;
      first = false;
      y += mt;
      const ImVec2 p{at.x, at.y + y};
      if (t == "list") {  // ul.space-y-3 text-sm text-muted leading-relaxed
        bool firstItem = true;
        for (const Json& item : bl.value("items", Json::array())) {
          if (!firstItem) y += 12.0f;
          firstItem = false;
          y += richText(dl, ui(), 14.0f + kPlus, (14.0f + kPlus) * 1.625f, {at.x, at.y + y}, width, {{item.get<std::string>(), kMuted}});
        }
      } else if (t == "p") {
        const bool xs = bl.value("size", std::string("sm")) == "xs";
        const bool muted = bl.value("muted", false);
        const float size = (xs ? 12.0f : 14.0f) + kPlus;
        const float lh = muted || xs ? size * 1.625f : size * 1.43f;
        y += richText(dl, ui(), size, lh, p, width, spansOf(bl.value("spans", Json::array()), muted ? kMuted : kFg));
      } else if (t == "h3") {  // font-display text-lg ember-title
        if (dl) emberTitle(dl, {p.x, p.y + 4.0f}, 18.0f, bl.value("text", std::string()).c_str());
        y += 28.0f;
      } else if (t == "box") {  // rounded-md border border-border bg-bg px-2 py-2 text-xs leading-relaxed
        float h = 8.0f;
        std::vector<std::vector<Span>> lines;
        for (const Json& line : bl.value("lines", Json::array())) lines.push_back(spansOf(line, kFg));
        for (const auto& line : lines) h += richText(nullptr, ui(), 12.0f + kPlus, (12.0f + kPlus) * 1.625f, {}, width - 16.0f, line);
        h += 8.0f;
        if (dl) {
          dl->AddRectFilled(p, {p.x + width, p.y + h}, kBg, 6.0f);
          dl->AddRect(p, {p.x + width, p.y + h}, kBorder, 6.0f);
          float ly = p.y + 8.0f;
          for (const auto& line : lines) ly += richText(dl, ui(), 12.0f + kPlus, (12.0f + kPlus) * 1.625f, {p.x + 8.0f, ly}, width - 16.0f, line);
        }
        y += h;
      } else if (t == "table") {
        y += table(dl, p, width, bl);
      }
    }
    return y;
  }

  // A help table (w-full border-collapse text-xs): columns sized like a browser's automatic table layout (each column
  // between its longest word and its whole content, the room shared in proportion), header text muted.
  // A table whose shortest columns are still wider than the window is drawn in smaller text, not cut off.
  static float table(ImDrawList* dl, ImVec2 at, float width, const Json& bl, float size = 12.0f + kPlus) {
    const Json head = bl.value("head", Json::array());
    const Json rows = bl.value("rows", Json::array());
    const Json align = bl.value("align", Json::array());
    const Json nowrapList = bl.value("nowrap", Json::array());
    const bool tight = bl.value("size", std::string()) == "xs-tight";
    const size_t n = head.size();
    if (n == 0) return 0.0f;
    const float lh = std::round(size * 4.0f / 3.0f), py = tight ? 2.0f : 4.0f;
    // padding: first column pr-2, middle px-1.5, last pl-1.5
    auto padL = [&](size_t c) { return c == 0 ? 0.0f : 6.0f; };
    auto padR = [&](size_t c) { return c == 0 ? 8.0f : c + 1 == n ? 0.0f : 6.0f; };
    auto nowrap = [&](size_t c) {
      for (const Json& v : nowrapList)
        if (v.get<size_t>() == c) return true;
      return false;
    };
    std::vector<float> minW(n, 0.0f), maxW(n, 0.0f);
    auto measure = [&](size_t c, const std::string& s) {
      maxW[c] = std::max(maxW[c], textWidth(ui(), size, s));
      if (nowrap(c)) {
        minW[c] = std::max(minW[c], textWidth(ui(), size, s));
        return;
      }
      size_t i = 0;
      while (i < s.size()) {
        size_t j = s.find(' ', i);
        if (j == std::string::npos) j = s.size();
        minW[c] = std::max(minW[c], ui()->CalcTextSizeA(size, FLT_MAX, 0.0f, s.data() + i, s.data() + j).x);
        i = j + 1;
      }
    };
    for (size_t c = 0; c < n; ++c) measure(c, head[c].get<std::string>());
    for (const Json& row : rows)
      for (size_t c = 0; c < n && c < row.size(); ++c)
        for (const Json& line : row[c]) {
          std::string s;
          for (const Json& span : line) s += span.value("text", std::string());
          measure(c, s);
        }
    std::vector<float> colW(n);
    float sumMin = 0, sumMax = 0;
    for (size_t c = 0; c < n; ++c) {
      minW[c] += padL(c) + padR(c), maxW[c] += padL(c) + padR(c);
      sumMin += minW[c], sumMax += maxW[c];
    }
    if (sumMin > width + 0.5f && size > 9.0f) return table(dl, at, width, bl, std::max(9.0f, std::floor(size * width / sumMin * 10.0f) / 10.0f));
    if (sumMax <= width) {
      for (size_t c = 0; c < n; ++c) colW[c] = maxW[c] + (width - sumMax) * (maxW[c] / sumMax);
    } else {
      const float room = std::max(0.0f, width - sumMin), spread = std::max(1e-3f, sumMax - sumMin);
      for (size_t c = 0; c < n; ++c) colW[c] = minW[c] + room * (maxW[c] - minW[c]) / spread;
    }
    auto cellX = [&](size_t c) {
      float x = at.x;
      for (size_t k = 0; k < c; ++k) x += colW[k];
      return x;
    };
    auto drawCell = [&](ImDrawList* d, size_t c, float y, const std::vector<std::vector<Span>>& lines) {
      const float x = cellX(c) + padL(c), w = colW[c] - padL(c) - padR(c);
      const std::string al = c < align.size() ? align[c].get<std::string>() : "l";
      float h = 0.0f;
      for (const auto& line : lines) {
        float lineW = 0.0f;
        for (const Span& s : line) lineW += textWidth(ui(), size, s.text);
        if (al != "l" && lineW <= w) {
          const float off = al == "r" ? w - lineW : (w - lineW) * 0.5f;
          if (d) {
            float sx = x + off;
            for (const Span& s : line) {
              d->AddText(ui(), size, {std::round(sx), y + h + std::round((lh - size) * 0.5f)}, s.color, s.text.c_str());
              sx += textWidth(ui(), size, s.text);
            }
          }
          h += lh;
        } else {
          h += richText(d, ui(), size, lh, {x, y + h}, w, line);
        }
      }
      return h;
    };
    float y = 0.0f;
    // header row
    {
      float h = 0.0f;
      for (size_t c = 0; c < n; ++c) h = std::max(h, drawCell(nullptr, c, 0, {{{head[c].get<std::string>(), kMuted}}}));
      if (dl)
        for (size_t c = 0; c < n; ++c) drawCell(dl, c, at.y + y + 4.0f, {{{head[c].get<std::string>(), kMuted}}});
      y += h + 8.0f;
      if (dl && bl.value("headBorder", false)) dl->AddLine({at.x, at.y + y - 0.5f}, {at.x + width, at.y + y - 0.5f}, kBorder60);
    }
    for (const Json& row : rows) {
      std::vector<std::vector<std::vector<Span>>> cells;
      for (size_t c = 0; c < n; ++c) {
        std::vector<std::vector<Span>> lines;
        if (c < row.size())
          for (const Json& line : row[c]) lines.push_back(spansOf(line, kFg));
        cells.push_back(std::move(lines));
      }
      float h = 0.0f;
      for (size_t c = 0; c < n; ++c) h = std::max(h, drawCell(nullptr, c, 0, cells[c]));
      if (dl) {
        dl->AddLine({at.x, at.y + y + 0.5f}, {at.x + width, at.y + y + 0.5f}, kBorder60);
        for (size_t c = 0; c < n; ++c) drawCell(dl, c, at.y + y + py, cells[c]);
      }
      y += h + 2 * py;
    }
    return y;
  }

  bool open_ = false;
  int openedFrame_ = -1;  // the frame it opened in: an Esc opening it must not also close it
  int tab_ = 0;
  bool scrollTop_ = false;
  float previewScroll_ = 0.0f;
};

// --- OptionsDialog ----------------------------------------------------------------------------------------------------------

class OptionsDialog {
 public:
  // What the screen offers (GameMenuActions; a section only when it has something) and what it acts on.
  struct Actions {
    std::function<void()> save, load, title;  // Jogo: Salvar jogo, Carregar jogo, Voltar à tela inicial
    std::function<void()> fullscreen;          // Tela cheia
    DevGfx* gfx = nullptr;                      // Gráficos (Dev Controls' switches; saved on every change)
    std::function<void()> gfxChanged;           // a battle on screen takes the change at once
  };

  void open() {
    if (!open_) openedFrame_ = ImGui::GetCurrentContext() ? ImGui::GetFrameCount() : -1;
    open_ = true;
  }
  void close() { open_ = false; }
  bool isOpen() const { return open_; }

  void draw(ImVec2 screen, const Actions& actions) {
    if (!open_) return;
    if (ImGui::IsKeyPressed(ImGuiKey_Escape, false) && ImGui::GetFrameCount() != openedFrame_) {  // <dialog> onCancel (not the press that opened it)
      open_ = false;
      return;
    }
    GamePrefs& prefs = gamePrefs();
    bool prefsChanged = false, gfxChanged = false;

    // ::backdrop rgba(3, 3, 3, .82)
    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize(screen);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(3, 3, 3, 209));
    ImGui::Begin("##optionsbackdrop", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);
    ImGui::SetCursorScreenPos({0, 0});
    ImGui::InvisibleButton("##optionsveil", screen);
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);

    // .game-options.ember-panel: w-[min(94vw,44rem)], max-h-[94dvh], p-5, scrolls when taller.
    const float w = std::min(704.0f, screen.x * 0.94f), pad = 20.0f, inner = w - 2 * pad;
    const bool hasGame = actions.save || actions.load || actions.title;
    const float colW = (inner - 8.0f) * 0.5f;
    const bool twoCols = screen.x >= 640.0f;
    const float secW = twoCols ? colW : inner;

    // Sections (ember-slot px-3 py-2 space-y-1): rows of known heights.
    struct Row {
      float h;
      std::function<void(ImVec2, float)> draw;
    };
    struct Section {
      std::string title;
      void (*icon)(ImDrawList*, ImVec2, float, ImU32);
      std::vector<Row> rows;
    };
    std::vector<Section> sections;
    const float text = 12.8f * kOptScale;
    auto label = [=](ImVec2 at, float h, const std::string& s, ImU32 ink = kOptText, float room = FLT_MAX) {
      const float size = fitSize(ui(), text, s, room);
      ImGui::GetWindowDrawList()->AddText(ui(), size, {at.x, std::round(at.y + (h - size) * 0.5f)}, ink, s.c_str());
    };
    auto toggle = [&](const char* id, const std::string& name, bool& value, bool disabled, bool* changed) {
      return Row{30.4f, [=, &value](ImVec2 at, float rw) {
                   label(at, 30.4f, name, disabled ? IM_COL32(234, 220, 201, 102) : kOptText, rw - 16.0f - 8.0f);
                   ImGui::SetCursorScreenPos({at.x + rw - 16.0f, at.y + 7.2f});
                   if (emberCheckbox(id, value, disabled)) *changed = true;
                 }};
    };
    auto select = [&](const char* id, const std::string& name, int current, std::vector<std::pair<std::string, bool>> options, std::function<void(int)> set) {
      return Row{30.4f, [=](ImVec2 at, float rw) {
                   const float sw = std::min(rw * 0.55f, 176.0f);
                   label(at, 30.4f, name, kOptText, rw - sw - 8.0f);
                   ImGui::SetCursorScreenPos({at.x + rw - sw, at.y});
                   int v = current;
                   if (emberSelect(id, v, options, sw)) set(v);
                 }};
    };
    auto range = [&](const char* id, const std::string& name, float& value, bool disabled, bool* changed) {
      // label (block, .8rem) with its value on the right, then the range (mt .4rem, mb .3rem)
      return Row{37.5f, [=, &value](ImVec2 at, float rw) {
                   const ImU32 ink = disabled ? IM_COL32(234, 220, 201, 102) : kOptText;
                   label(at, 18.3f, name, ink);
                   const std::string pct = std::to_string(static_cast<int>(std::round(value * 100.0f))) + "%";
                   label({at.x + rw - textWidth(ui(), text, pct), at.y}, 18.3f, pct, ink);
                   ImGui::SetCursorScreenPos({at.x, at.y + 18.3f + 6.4f - 3.0f});
                   if (emberRange(id, value, rw, disabled)) *changed = true;
                 }};
    };

    if (hasGame) {
      Section s{tr("Jogo"), nullptr, {}};
      auto button = [&](const char* id, std::string text, std::function<void()> fn, Btn style) {
        return Row{40.0f, [=, this](ImVec2 at, float rw) {
                     ImGui::SetCursorScreenPos(at);
                     if (emberButton(id, text, {rw, 40.0f}, style, style == Btn::Ghost ? false : false, 16.0f * kOptScale) && fn) {
                       open_ = false;
                       fn();
                     }
                   }};
      };
      if (actions.save) s.rows.push_back(button("##optsave", tr("Salvar jogo"), actions.save, Btn::Plain));
      if (actions.load) s.rows.push_back(button("##optload", tr("Carregar jogo"), actions.load, Btn::Plain));
      if (actions.title) s.rows.push_back(button("##opttitle", tr("Voltar à tela inicial"), actions.title, Btn::Ghost));
      sections.push_back(std::move(s));
    }
    {
      Section s{tr("Gráficos"), &iconMonitor, {}};
      static DevGfx fallback;
      DevGfx& g = actions.gfx ? *actions.gfx : fallback;
      s.rows.push_back(toggle("##gfxshadows", tr("Sombras"), g.realShadows, !actions.gfx, &gfxChanged));
      s.rows.push_back(toggle("##gfxlights", tr("Luzes locais"), g.localLights, !actions.gfx, &gfxChanged));
      s.rows.push_back(toggle("##gfxatmo", tr("FX atmosféricos"), g.atmosphericFx, !actions.gfx, &gfxChanged));
      s.rows.push_back(select("##gfxcast", tr("Personagens", "Characters"),
                              prefs.characters == "alta" ? 0 : prefs.characters == "leve" ? 2 : 1,
                              {{tr("Alta", "High"), true}, {tr("Média", "Medium"), true}, {tr("Leve", "Light"), true}},
                              [&](int v) { prefs.characters = v == 0 ? "alta" : v == 2 ? "leve" : "media", prefsChanged = true; }));
      // Tela cheia: ember-btn ember-btn-ghost ember-btn-sm w-full py-1.5 mt-1
      s.rows.push_back(Row{36.0f, [&](ImVec2 at, float rw) {
                             ImGui::SetCursorScreenPos(at);
                             if (emberButton("##optfull", tr("Tela cheia"), {rw, 36.0f}, Btn::Ghost, true, 16.0f * kOptScale, !actions.fullscreen)) actions.fullscreen();
                           }});
      sections.push_back(std::move(s));
    }
    {
      Section s{tr("Áudio"), &iconVolume, {}};
      s.rows.push_back(toggle("##mute", tr("Silenciar"), prefs.muted, false, &prefsChanged));
      s.rows.push_back(toggle("##cutsound", tr("Som das cutscenes"), prefs.cutsceneEnabled, false, &prefsChanged));
      s.rows.push_back(range("##music", tr("Música"), prefs.music, false, &prefsChanged));
      s.rows.push_back(range("##sfx", tr("Efeitos sonoros"), prefs.sfx, false, &prefsChanged));
      s.rows.push_back(range("##cutvol", tr("Volume da cutscene"), prefs.cutscene, !prefs.cutsceneEnabled, &prefsChanged));
      sections.push_back(std::move(s));
    }
    {
      Section s{tr("Idiomas"), &iconLanguages, {}};
      const std::vector<std::pair<std::string, bool>> languages = {{"Português", true}, {"English", true}};
      s.rows.push_back(select("##uilang", tr("Interface e nomes"), prefs.uiLanguage == "en" ? 1 : 0, languages, [&](int v) {
        prefs.uiLanguage = v ? "en" : "pt", prefsChanged = true;
      }));
      s.rows.push_back(select("##dlglang", tr("Diálogos"), prefs.dialogueLanguage == "en" ? 1 : 0, languages, [&](int v) {
        prefs.dialogueLanguage = v ? "en" : "pt", prefsChanged = true;
      }));
      s.rows.push_back(select("##sublang", tr("Idioma das legendas"), prefs.subtitleLanguage == "en" ? 1 : 0, {{tr("Português (Brasil)"), true}, {"English", true}},
                              [&](int v) { prefs.subtitleLanguage = v ? "en" : "pt", prefsChanged = true; }));
      sections.push_back(std::move(s));
    }
    {
      Section s{tr("Acessibilidade"), nullptr, {}};
      s.rows.push_back(toggle("##subs", tr("Exibir legendas"), prefs.subtitles, false, &prefsChanged));
      const int size = prefs.dialogueScale > 1.25f ? 2 : prefs.dialogueScale > 1.05f ? 1 : 0;
      s.rows.push_back(select("##dlgsize", tr("Tamanho do diálogo"), size, {{tr("Normal"), true}, {tr("Grande"), true}, {tr("Muito grande"), true}}, [&](int v) {
        prefs.dialogueScale = v == 2 ? 1.3f : v == 1 ? 1.15f : 1.0f, prefsChanged = true;
      }));
      sections.push_back(std::move(s));
    }

    const float headingH = 26.4f;
    auto sectionH = [&](const Section& s) {
      float h = 8.0f + headingH;
      for (const Row& r : s.rows) h += 4.0f + r.h;
      return h + 8.0f;
    };
    // grid gap-2, items-start: rows of two
    float gridH = 0.0f;
    for (size_t i = 0; i < sections.size(); i += twoCols ? 2 : 1) {
      float rowH = sectionH(sections[i]);
      if (twoCols && i + 1 < sections.size()) rowH = std::max(rowH, sectionH(sections[i + 1]));
      gridH += (i ? 8.0f : 0.0f) + rowH;
    }
    const float headerH = 40.0f + 12.0f + 1.0f;  // Fechar's height, pb-3, the line
    const float footerH = 12.0f + 12.0f + 19.2f;  // mt-3 pt-3, one xs line
    const float natural = pad + headerH + 12.0f + gridH + footerH + pad;
    const float h = std::min(natural, screen.y * 0.94f);

    ImGui::SetNextWindowPos({std::round((screen.x - w) * 0.5f), std::round((screen.y - h) * 0.5f)});
    ImGui::SetNextWindowSize({w, h});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 10.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, IM_COL32(13, 13, 13, 255));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab, IM_COL32(98, 98, 98, 255));
    ImGui::Begin("##options", nullptr, ImGuiWindowFlags_NoDecoration & ~ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                                          ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
    ImGui::SetWindowFocus();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float scroll = ImGui::GetScrollY();
    const ImVec2 a{ImGui::GetWindowPos().x, ImGui::GetWindowPos().y - scroll};
    emberPanel(dl, a, {a.x + w, a.y + natural});
    float y = a.y + pad;
    // header: Opções (ember-title text-2xl) and Fechar (ember-btn-ghost ember-btn-sm px-4 py-2)
    emberTitle(dl, {a.x + pad, y + 3.0f}, 24.0f * kOptScale, tr("Opções").c_str());
    const std::string closeLabel = tr("Fechar");
    const float closeW = spacedTextWidth(mark(), 16.0f, closeLabel.c_str(), 0.06f) + 32.0f;
    ImGui::SetCursorScreenPos({a.x + w - pad - closeW, y});
    if (emberButton("##optclose", closeLabel, {closeW, 40.0f}, Btn::Ghost, true, 16.0f * kOptScale)) open_ = false;
    y += 40.0f + 12.0f;
    dl->AddLine({a.x + pad, y}, {a.x + w - pad, y}, IM_COL32(190, 150, 95, 77));
    y += 1.0f + 12.0f;
    for (size_t i = 0; i < sections.size(); i += twoCols ? 2 : 1) {
      float rowH = 0.0f;
      for (size_t k = i; k < sections.size() && k < i + (twoCols ? 2 : 1); ++k) {
        const Section& s = sections[k];
        const float sh = sectionH(s);
        rowH = std::max(rowH, sh);
        const ImVec2 sa{a.x + pad + (k - i) * (secW + 8.0f), y}, sb{sa.x + secW, y + sh};
        emberSlot(dl, sa, sb, false);
        // .game-options-heading: ember-title 1.1rem, its icon 1.1rem in gold
        float hx = sa.x + 12.0f;
        if (s.icon) {
          s.icon(dl, {hx + 8.8f, sa.y + 8.0f + headingH * 0.5f}, 17.6f, kGold);
          hx += 17.6f + 10.4f;
        }
        emberTitle(dl, {hx, sa.y + 8.0f + (headingH - 17.6f * kOptScale) * 0.5f}, 17.6f * kOptScale, s.title.c_str());
        float ry = sa.y + 8.0f + headingH;
        for (size_t r = 0; r < s.rows.size(); ++r) {
          ry += 4.0f;
          ImGui::PushID(static_cast<int>(k * 100 + r));
          s.rows[r].draw({sa.x + 12.0f, ry}, secW - 24.0f);
          ImGui::PopID();
          ry += s.rows[r].h;
        }
      }
      y += rowH + 8.0f;
    }
    y += 12.0f - 8.0f;
    // footer: ember-rule mt-3 pt-3, the check and "Alterações salvas automaticamente." centred, text-xs muted
    emberRule(dl, a.x + pad, a.x + w - pad, y);
    y += 12.0f;
    const std::string saved = prefsOk_ && gfxOk_ ? tr("Alterações salvas automaticamente.") : tr("Não foi possível gravar as alterações.", "The changes could not be saved.");
    const float footText = 12.0f * kOptScale;
    const float tw = textWidth(ui(), footText, saved), total = 14.0f + 8.0f + tw;
    const float fx = a.x + (w - total) * 0.5f;
    iconCheck(dl, {fx + 7.0f, y + 9.6f}, 14.0f, kGold);
    dl->AddText(ui(), footText, {fx + 22.0f, std::round(y + 9.6f - footText * 0.5f)}, kOptMuted, saved.c_str());
    ImGui::SetCursorScreenPos({a.x, a.y + natural - 1.0f});
    ImGui::Dummy({1.0f, 1.0f});
    ImGui::End();
    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar(3);

    if (prefsChanged) prefsOk_ = prefs.save();
    if (gfxChanged && actions.gfx) {
      gfxOk_ = actions.gfx->save();
      if (actions.gfxChanged) actions.gfxChanged();
    }
  }

 private:
  bool open_ = false;
  int openedFrame_ = -1;
  bool prefsOk_ = true, gfxOk_ = true;
};

// --- the battle's Esc menu (GameApp BattleScreen `paused`) ---------------------------------------------------------------

class BattleMenu {
 public:
  enum class Action { None, Resume, Options, Save, Load, Quit, Title };
  enum class Speed { Slow, Normal, Fast };

  // `playtest`: the Map Editor's Testar battle ("Encerrar teste" instead of "Desistir"). `canSave` / `canLoad`: the
  // campaign's slots are there (Modo teste writes nothing: no Save).
  Action draw(ImVec2 screen, const std::filesystem::path& engine2Public, Speed& speed, bool playtest, bool canSave = false, bool canLoad = false) {
    Action act = Action::None;
    // bg-bg/80 over the battle; a click outside the panel resumes
    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize(screen);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(17, 17, 17, 204));
    ImGui::Begin("##battlemenu", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollWithMouse);
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // .status-panel.ember-window.rounded-xl.p-6: min(27rem, 100vw - 2rem) wide, the status art under a 25% surface wash
    const float pad = 24.0f, w = std::min(432.0f, screen.x - 32.0f), inner = w - 2 * pad;
    const GamePrefs& prefs = gamePrefs();
    (void)prefs;
    const float headH = 32.0f + 16.0f;
    const float speedH = 12.0f * 1.33f + 8.0f + 36.0f + 16.0f;
    const float audioH = 12.0f + 40.0f + (audioOpen_ ? 8.0f + 12.0f * 2 + 3 * 40.5f + 2 * 12.0f : 0.0f) + 16.0f;
    const float buttonsH = 48.0f + 8.0f + 44.0f + 8.0f + 44.0f + 8.0f + 44.0f + (canSave ? 52.0f : 0.0f) + (canLoad ? 52.0f : 0.0f);
    const float h = std::min(screen.y * 0.85f, pad + headH + speedH + audioH + buttonsH + pad);
    const ImVec2 a{std::round((screen.x - w) * 0.5f), std::round((screen.y - h) * 0.5f)}, b{a.x + w, a.y + h};
    ImGui::SetCursorScreenPos({0, 0});
    ImGui::SetNextItemAllowOverlap();  // the panel's buttons drawn after the veil take their own clicks
    if (ImGui::InvisibleButton("##battleveil", screen) && !ImGui::IsMouseHoveringRect(a, b)) act = Action::Resume;
    for (int i = 8; i >= 1; --i) dl->AddRectFilled({a.x - i * 1.5f, a.y + i * 2.0f}, {b.x + i * 1.5f, b.y + i * 2.6f}, IM_COL32(0, 0, 0, 16), 12.0f + i);
    dl->AddRectFilled(a, b, IM_COL32(28, 28, 28, 235), 12.0f);
    if (const UiImage* art = uiImage(engine2Public / "game/ui/status-background.jpg")) {
      dl->PushClipRect(a, b, true);
      imageCover(dl, *art, a, b);
      dl->PopClipRect();
    }
    dl->AddRectFilled(a, b, IM_COL32(28, 28, 28, 64), 12.0f);
    dl->AddRect({a.x + 1, a.y + 1}, {b.x - 1, b.y - 1}, IM_COL32(255, 255, 255, 10), 12.0f);
    dl->AddRect(a, b, kBorder, 12.0f);

    float y = a.y + pad;
    emberTitle(dl, {a.x + pad, y + 4.0f}, 24.0f, tr("Opções").c_str());
    ImGui::SetCursorScreenPos({b.x - pad - 32.0f, y});
    if (iconClose("##battleclose")) act = Action::Resume;
    y += headH;

    // Velocidade: text-xs ember-kicker mb-2; grid-cols-3 gap-1 of Button sm; mb-4
    emberKicker(dl, {a.x + pad, y}, 12.0f, upperUtf8(tr("Velocidade", "Speed")).c_str());
    y += 12.0f * 1.33f + 8.0f;
    const std::string speeds[3] = {tr("Lenta", "Slow"), tr("Normal"), tr("Rápida", "Fast")};
    const float sw = (inner - 8.0f) / 3.0f;
    for (int i = 0; i < 3; ++i) {
      ImGui::SetCursorScreenPos({a.x + pad + i * (sw + 4.0f), y});
      ImGui::PushID(i);
      if (emberButton("##speed", speeds[i], {sw, 36.0f}, static_cast<int>(speed) == i ? Btn::Primary : Btn::Ghost, true, 12.0f)) speed = static_cast<Speed>(i);
      ImGui::PopID();
    }
    y += 36.0f + 16.0f;

    // Áudio: mb-4 ember-rule pt-3, an ember-slot row opening the volumes
    emberRule(dl, a.x + pad, b.x - pad, y);
    y += 12.0f;
    ImGui::SetCursorScreenPos({a.x + pad, y});
    const bool toggled = ImGui::InvisibleButton("##audio", {inner, 40.0f});
    const bool hover = ImGui::IsItemHovered();
    emberSlot(dl, {a.x + pad, y}, {b.x - pad, y + 40.0f}, hover);
    iconSliders(dl, {a.x + pad + 12.0f + 8.0f, y + 20.0f}, 16.0f, kAccent);
    dl->AddText(ui(), 14.0f, {a.x + pad + 12.0f + 16.0f + 8.0f, y + 12.0f}, kFg, tr("Áudio").c_str());
    const std::string hint = audioOpen_ ? tr("Fechar") : tr("Volumes", "Volumes");
    dl->AddText(ui(), 12.0f, {b.x - pad - 12.0f - textWidth(ui(), 12.0f, hint), y + 14.0f}, kMuted, hint.c_str());
    if (toggled) audioOpen_ = !audioOpen_;
    y += 40.0f;
    if (audioOpen_) {
      // mt-2 rounded-md border border-border bg-bg/40 p-3 flex-col gap-3
      y += 8.0f;
      const float boxH = 12.0f * 2 + 3 * 40.5f + 2 * 12.0f;
      dl->AddRectFilled({a.x + pad, y}, {b.x - pad, y + boxH}, IM_COL32(17, 17, 17, 102), 6.0f);
      dl->AddRect({a.x + pad, y}, {b.x - pad, y + boxH}, kBorder, 6.0f);
      GamePrefs& p = gamePrefs();
      bool changed = false;
      struct Vol {
        const char* id;
        std::string name;
        float* value;
        bool disabled;
      } vols[3] = {{"##bmusic", upperUtf8(tr("Música")), &p.music, false},
                     {"##bsfx", upperUtf8(tr("Efeitos", "Effects")), &p.sfx, false},
                     {"##bcut", upperUtf8(tr("Cutscenes", "Cutscenes")), &p.cutscene, false}};
      float vy = y + 12.0f;
      for (const Vol& v : vols) {
        const ImU32 ink = v.disabled ? IM_COL32(180, 180, 180, 102) : kMuted;
        tracked(dl, ui(), 12.0f, {a.x + pad + 12.0f, vy}, ink, v.name, 0.14f);
        const std::string pct = std::to_string(static_cast<int>(std::round(*v.value * 100.0f))) + "%";
        dl->AddText(ui(), 12.0f, {b.x - pad - 12.0f - textWidth(ui(), 12.0f, pct), vy}, v.disabled ? IM_COL32(240, 240, 240, 102) : kFg, pct.c_str());
        ImGui::SetCursorScreenPos({a.x + pad + 12.0f, vy + 16.0f + 6.0f});
        if (emberRange(v.id, *v.value, inner - 24.0f, v.disabled)) changed = true;
        vy += 40.5f + 12.0f;
      }
      if (changed) p.save();
      y += boxH;
    }
    y += 16.0f;

    // flex-col gap-2: Opções (ghost py-3), Continuar (primary), Desistir / Encerrar teste, Tela inicial
    ImGui::SetCursorScreenPos({a.x + pad, y});
    if (emberButton("##bopts", tr("Opções"), {inner, 48.0f}, Btn::Ghost, false, 16.0f)) act = Action::Options;
    y += 48.0f + 8.0f;
    ImGui::SetCursorScreenPos({a.x + pad, y});
    if (emberButton("##bresume", tr("Continuar"), {inner, 44.0f}, Btn::Primary, false, 14.0f)) act = Action::Resume;
    y += 44.0f + 8.0f;
    // GameApp's own labels for these two ("Save", "Load").
    if (canSave) {
      ImGui::SetCursorScreenPos({a.x + pad, y});
      if (emberButton("##bsave", "Save", {inner, 44.0f}, Btn::Ghost, false, 14.0f)) act = Action::Save;
      y += 44.0f + 8.0f;
    }
    if (canLoad) {
      ImGui::SetCursorScreenPos({a.x + pad, y});
      if (emberButton("##bload", "Load", {inner, 44.0f}, Btn::Ghost, false, 14.0f)) act = Action::Load;
      y += 44.0f + 8.0f;
    }
    ImGui::SetCursorScreenPos({a.x + pad, y});
    if (emberButton("##bquit", playtest ? tr("Encerrar teste", "End test") : tr("Desistir", "Give up"), {inner, 44.0f}, Btn::Ghost, false, 14.0f)) act = Action::Quit;
    y += 44.0f + 8.0f;
    ImGui::SetCursorScreenPos({a.x + pad, y});
    if (emberButton("##btitle", tr("Tela inicial", "Title screen"), {inner, 44.0f}, Btn::Ghost, false, 14.0f)) act = Action::Title;
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
    return act;
  }

 private:
  bool audioOpen_ = false;
};

}  // namespace menu
}  // namespace ember
