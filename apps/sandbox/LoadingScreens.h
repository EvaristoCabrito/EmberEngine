#pragma once

// Engine2's loading screens, ported as they were (the user, 2026-10-16: "porte exatamente como era antes"):
//  - The loading curtain (src/game/MapLoadingOverlay.tsx LoadingArt): game/ui/LoadingSCreen.jpg (2128 x 912) covering the
//    screen over #131313, the progress bar inside the art's own bar frame and the status in its plaque, at fixed fractions
//    of the picture. Up from the moment a battle is asked for until it is ready, at least 380 ms (useLoadingCurtain
//    MIN_VISIBLE_MS), 20 s at most (GameApp's safety timer); on the world map "Carregando o mapa" (MapLoadingOverlay:
//    100%, then gone 280 ms later). Engine2's rule (LOCKED 2026-10-06): real numbers only; no number, no fill, never a
//    fake sweep.
//  - The battle's real steps (GameApp battleLoadCaption + loadStages.ts): each step's share of the bar is the time it took
//    last time (%LOCALAPPDATA%\Ember\loading-steps.json; Engine2 kept them in localStorage), inside a step its own count.
//  - The title page's bar (index.html #title-loader, title.css .title-loader, title/main.ts setProgress): "Despertando as
//    cinzas", a gothic-framed trough filled with molten ember, a sheen, a flickering spark at the leading edge and embers
//    drifting off it; real progress kept monotonic and under 97% until loading has truly finished, then it brightens,
//    fades 0.9 s later and is gone at 1.9 s.

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <fstream>
#include <numbers>
#include <optional>
#include <string>
#include <system_error>
#include <vector>

#include <nlohmann/json.hpp>

#include "CampaignUi.h"
#include "GamePrefs.h"
#include "Stage.h"
#include "render/UiArt.h"

namespace ember::loading {

// --- loadStages.ts ---------------------------------------------------------------------------------------------------

// 0..100 for being at `step` with `done` of `total` units finished there.
inline float stepPercent(const std::vector<double>& stepMs, int step, uint32_t done, uint32_t total) {
  double all = 0.0, before = 0.0;
  for (double ms : stepMs) all += std::max(1.0, ms);
  for (int i = 0; i < std::min<int>(step, static_cast<int>(stepMs.size())); ++i) before += std::max(1.0, stepMs[static_cast<size_t>(i)]);
  const double here = step < static_cast<int>(stepMs.size()) ? std::max(1.0, stepMs[static_cast<size_t>(step)]) : 0.0;
  const double inside = total > 0 ? std::min(1.0, static_cast<double>(done) / total) : 0.0;
  return static_cast<float>(std::min(100.0, (before + here * inside) / all * 100.0));
}

// Times each step of one loading as it runs, so the next one can be weighted by them.
class StepClock {
 public:
  StepClock(size_t steps, double now) : ms_(steps, 0.0), since_(now) {}
  int current() const { return step_; }
  // Move on to `step` (never back); the time since the last change belongs to the step left.
  void enter(int step, double now) {
    if (step <= step_) return;
    ms_[static_cast<size_t>(step_)] += (now - since_) * 1000.0;
    for (int i = step_ + 1; i < step && i < static_cast<int>(ms_.size()); ++i) ms_[static_cast<size_t>(i)] = 0.0;
    step_ = std::min(step, static_cast<int>(ms_.size()) - 1);
    since_ = now;
  }
  std::vector<double> finish(double now) {
    ms_[static_cast<size_t>(step_)] += (now - since_) * 1000.0;
    since_ = now;
    return ms_;
  }

 private:
  std::vector<double> ms_;
  int step_ = 0;
  double since_ = 0.0;
};

inline std::filesystem::path stepsFile() {
  const char* base = std::getenv("LOCALAPPDATA");
  return std::filesystem::path(base ? base : ".") / "Ember" / "loading-steps.json";
}
inline std::vector<double> rememberedStepMs(const char* key, std::vector<double> fallback) {
  std::ifstream in(stepsFile(), std::ios::binary);
  const nlohmann::json j = in ? nlohmann::json::parse(in, nullptr, false) : nlohmann::json();
  if (!j.is_object() || !j.contains(key) || !j[key].is_array() || j[key].size() != fallback.size()) return fallback;
  std::vector<double> out;
  for (const nlohmann::json& v : j[key]) {
    if (!v.is_number() || v.get<double>() < 0.0) return fallback;
    out.push_back(v.get<double>());
  }
  return out;
}
inline void rememberStepMs(const char* key, const std::vector<double>& ms) {
  std::ifstream in(stepsFile(), std::ios::binary);
  nlohmann::json j = in ? nlohmann::json::parse(in, nullptr, false) : nlohmann::json::object();
  in.close();
  if (!j.is_object()) j = nlohmann::json::object();
  nlohmann::json list = nlohmann::json::array();
  for (double v : ms) list.push_back(std::round(v));
  j[key] = list;
  std::error_code ec;
  std::filesystem::create_directories(stepsFile().parent_path(), ec);
  std::ofstream(stepsFile(), std::ios::binary | std::ios::trunc) << j.dump(1);
}

// GameApp battleLoadCaption: which real step is running, with its own count.
inline std::string battleCaption(int step, uint32_t done, uint32_t total) {
  const std::string name = step == 0 ? tr("Carregando personagens", "Loading characters") : tr("Montando o campo", "Setting up the field");
  return total ? std::format("{} · {}/{}", name, done, total) : name;
}

// --- drawing helpers -------------------------------------------------------------------------------------------------

struct Stop {
  float at;
  ImU32 color;
};
// A left-to-right gradient through colour stops over [a, b].
inline void horizontalGradient(ImDrawList* dl, ImVec2 a, ImVec2 b, std::initializer_list<Stop> stops) {
  const Stop* prev = nullptr;
  for (const Stop& s : stops) {
    if (prev) {
      const float x0 = a.x + (b.x - a.x) * prev->at, x1 = a.x + (b.x - a.x) * s.at;
      if (x1 > x0) dl->AddRectFilledMultiColor({x0, a.y}, {x1, b.y}, prev->color, s.color, s.color, prev->color);
    }
    prev = &s;
  }
}
inline ImU32 withAlpha(ImU32 c, float k) {
  const float a = static_cast<float>((c >> IM_COL32_A_SHIFT) & 0xFF) * std::clamp(k, 0.0f, 1.0f);
  return (c & ~IM_COL32_A_MASK) | (static_cast<ImU32>(a + 0.5f) << IM_COL32_A_SHIFT);
}

// --- MapLoadingOverlay.tsx LoadingArt ----------------------------------------------------------------------------------

// The curtain over the whole screen: the art covering it, the bar in the art's frame (bar interior x 752-1362 / y 787-811
// of 2128 x 912), the status in the plaque (x 903-1203 / y 842-881). `progress` empty = no number, so no fill.
inline void drawLoadingArt(const std::filesystem::path& engine2Public, std::optional<float> progress, const std::string& label) {
  const ImGuiIO& io = ImGui::GetIO();
  ImGui::SetNextWindowPos({0, 0});
  ImGui::SetNextWindowSize(io.DisplaySize);
  ImGui::SetNextWindowFocus();  // over every window of the screen underneath, and takes its clicks
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
  ImGui::Begin("##loadingCurtain", nullptr,
               ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoSavedSettings |
                   ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoNav);
  ImDrawList* dl = ImGui::GetWindowDrawList();
  const ImVec2 size = io.DisplaySize;
  dl->AddRectFilled({0, 0}, size, IM_COL32(0x13, 0x13, 0x13, 255));
  // width max(100vw, 100vh * 2128 / 912), centred: no black border, the far left / right trimmed on narrow screens
  const float w = std::max(size.x, size.y * 2128.0f / 912.0f), h = w * 912.0f / 2128.0f;
  const ImVec2 o{(size.x - w) * 0.5f, (size.y - h) * 0.5f};
  if (const UiImage* art = uiImage(engine2Public / "game/ui/LoadingSCreen.jpg")) dl->AddImage(art->texture, o, {o.x + w, o.y + h});
  const ImVec2 barA{o.x + 0.3534f * w, o.y + 0.8629f * h}, barB{barA.x + 0.2867f * w, barA.y + 0.0263f * h};
  if (progress) {
    const float fill = barA.x + (barB.x - barA.x) * std::clamp(*progress, 0.0f, 100.0f) / 100.0f;
    dl->PushClipRect(barA, barB, true);
    // bg-gradient-to-r from-#444444 via-#e1a541 to-#fff0a2, across the fill
    horizontalGradient(dl, barA, {fill, barB.y}, {{0.0f, IM_COL32(0x44, 0x44, 0x44, 255)}, {0.5f, IM_COL32(0xe1, 0xa5, 0x41, 255)}, {1.0f, IM_COL32(0xff, 0xf0, 0xa2, 255)}});
    dl->PopClipRect();
  }
  // The plaque: uppercase, #eee0c8, 0.08em tracking, drop-shadow 0 1px 2px #000, 1.25% of the art's height, centred,
  // wrapping inside the plaque.
  const ImVec2 plaqueA{o.x + 0.4243f * w, o.y + 0.9232f * h};
  const float plaqueW = 0.141f * w, plaqueH = 0.0428f * h, fontSize = 0.0125f * h, spacing = 0.08f;
  ImFont* font = uiFont(UiFont::Ui);
  const std::string text = upperUtf8(label);
  std::vector<std::string> lines;
  {
    std::string line;
    size_t at = 0;
    while (at <= text.size()) {
      size_t end = text.find(' ', at);
      if (end == std::string::npos) end = text.size();
      const std::string word = text.substr(at, end - at);
      const std::string tryLine = line.empty() ? word : line + " " + word;
      if (!line.empty() && spacedTextWidth(font, fontSize, tryLine.c_str(), spacing) > plaqueW) {
        lines.push_back(line);
        line = word;
      } else {
        line = tryLine;
      }
      at = end + 1;
    }
    if (!line.empty()) lines.push_back(line);
  }
  const float lineH = fontSize * 1.15f;
  float y = plaqueA.y + (plaqueH - lineH * static_cast<float>(lines.size())) * 0.5f;
  for (const std::string& l : lines) {
    const float lw = spacedTextWidth(font, fontSize, l.c_str(), spacing);
    const ImVec2 at{plaqueA.x + (plaqueW - lw) * 0.5f, y + (lineH - fontSize) * 0.5f};
    spacedText(dl, font, fontSize, {at.x, at.y + 1.0f}, IM_COL32(0, 0, 0, 200), l.c_str(), spacing, false);
    spacedText(dl, font, fontSize, at, IM_COL32(0xee, 0xe0, 0xc8, 255), l.c_str(), spacing, false);
    y += lineH;
  }
  ImGui::End();
  ImGui::PopStyleVar(2);
}

// The curtain's life: a battle being set up (its real steps) or the world map opening (MapLoadingOverlay).
class Curtain {
 public:
  enum class Kind { None, Battle, Map };
  static constexpr const char* kBattleSteps = "battle";
  inline static const std::vector<double> kBattleFallback{1000.0, 4000.0};  // GameApp BATTLE_LOAD_STEPS_FALLBACK's first two

  void open(Kind kind, double now) {
    kind_ = kind;
    opened_ = now, finishedAt_ = -1.0, shown_ = 0.0f, target_ = 0.0f;
    hasNumber_ = kind == Kind::Battle;
    step_ = 0, done_ = 0, total_ = 0;
    if (kind == Kind::Battle) {
      stepMs_ = rememberedStepMs(kBattleSteps, kBattleFallback);
      clock_.emplace(stepMs_.size(), now);
    }
  }
  void close() { kind_ = Kind::None, clock_.reset(); }
  bool up() const { return kind_ != Kind::None; }

  // Once per frame with what the screen underneath still loads.
  void update(const Stage::Loading& l, double now, float dt) {
    if (kind_ == Kind::None) return;
    if (finishedAt_ < 0.0) {
      if (kind_ == Kind::Battle) {
        clock_->enter(l.step, now);
        step_ = clock_->current(), done_ = l.done, total_ = l.total;
        target_ = std::max(target_, std::floor(stepPercent(stepMs_, step_, done_, total_)));
      }
      if (l.ready) {
        finishedAt_ = now;
        hasNumber_ = true;
        target_ = 100.0f;
        if (clock_) rememberStepMs(kBattleSteps, clock_->finish(now));
      }
    }
    // the fill's 100 ms linear transition
    shown_ += (target_ - shown_) * std::min(1.0f, dt / 0.1f);
    if (target_ - shown_ < 0.5f) shown_ = target_;
    const bool minimum = now - opened_ >= 0.38;                                        // useLoadingCurtain MIN_VISIBLE_MS
    const bool lifted = kind_ == Kind::Map ? finishedAt_ >= 0.0 && now - finishedAt_ >= 0.28  // MapLoadingOverlay finish
                                           : finishedAt_ >= 0.0 && minimum;
    if (lifted || now - opened_ > 20.0) close();  // GameApp's safety: never stuck
  }

  // The plaque's text: the status and, once there is a real number, " · N%".
  std::string label() const {
    const std::string status = kind_ == Kind::Map ? tr("Carregando o mapa", "Loading the map") : battleCaption(step_, done_, total_);
    if (!hasNumber_) return kind_ == Kind::Map ? status + "…" : status;
    return std::format("{} · {}%", status, static_cast<int>(std::round(target_)));
  }

  void draw(const std::filesystem::path& engine2Public) const {
    if (kind_ == Kind::None) return;
    drawLoadingArt(engine2Public, hasNumber_ ? std::optional<float>(shown_) : std::nullopt, label());
  }

 private:
  Kind kind_ = Kind::None;
  double opened_ = 0.0, finishedAt_ = -1.0;
  float shown_ = 0.0f, target_ = 0.0f;
  bool hasNumber_ = false;
  int step_ = 0;
  uint32_t done_ = 0, total_ = 0;
  std::vector<double> stepMs_;
  std::optional<StepClock> clock_;
};

// --- index.html #title-loader (title.css .title-loader, title/main.ts) ----------------------------------------------------

class TitleLoader {
 public:
  // The real share of the title's art load (0..1) and whether it has truly finished; kept monotonic and under 97% until
  // then (useArtLoadProgress).
  void set(float progress, bool ready, double now) {
    peak_ = std::max(peak_, ready ? 1.0f : std::min(progress, 0.97f));
    if (ready && readyAt_ < 0.0) readyAt_ = now;
  }
  bool ready() const { return readyAt_ >= 0.0; }
  bool gone(double now) const { return readyAt_ >= 0.0 && now - readyAt_ >= 1.9; }

  // Under the menu column: its left edge `x`, `width` wide (min(25rem, 100% - 2.5rem)), 1.75rem above the screen's
  // bottom. Text 1.4x Engine2's, as every title text (user 2026-10-16).
  void draw(ImDrawList* dl, ImVec2 screen, float x, float width, float rem, double now, float dt) {
    if (gone(now)) return;
    const int pct = static_cast<int>(std::round(peak_ * 100.0f));
    // width transition .42s cubic-bezier(.3,.7,.2,1): a quick start, a long settle
    shown_ += (static_cast<float>(pct) - shown_) * std::min(1.0f, dt / 0.12f);
    // .is-leaving: opacity 0 and 10px down over .9s, 0.9 s after ready
    float alpha = 1.0f, drop = 0.0f;
    if (readyAt_ >= 0.0 && now - readyAt_ > 0.9) {
      const float t = std::clamp(static_cast<float>(now - readyAt_ - 0.9) / 0.9f, 0.0f, 1.0f);
      const float e = 1.0f - (1.0f - t) * (1.0f - t);  // ease
      alpha = 1.0f - e, drop = 10.0f * e;
    }
    auto col = [alpha](int r, int g, int b, int a = 255) { return withAlpha(IM_COL32(r, g, b, a), alpha); };
    const float bottom = screen.y - 1.75f * rem + drop;
    // .loader-frame: 1px border, 3px padding, a 12px track
    const ImVec2 fa{x, bottom - 20.0f}, fb{x + width, bottom};
    // the label row: "Despertando as cinzas" (.78rem Newsreader, .32em, uppercase, #b89a70; ready #e8c79a) and the number
    ImFont* display = uiFont(UiFont::Display);
    const float labelSize = 0.78f * rem * 1.4f;
    const float labelY = fa.y - 0.55f * rem - labelSize;
    const std::string status = upperUtf8(tr("Despertando as cinzas", "Awakening the ashes"));
    const ImU32 labelInk = ready() ? col(0xe8, 0xc7, 0x9a) : col(0xb8, 0x9a, 0x70);
    spacedText(dl, display, labelSize, {x, labelY + 1.0f}, col(0, 0, 0, 200), status.c_str(), 0.32f, false);
    spacedText(dl, display, labelSize, {x, labelY}, labelInk, status.c_str(), 0.32f, false);
    const std::string number = std::format("{}%", pct);
    const float nw = spacedTextWidth(display, labelSize, number.c_str(), 0.14f);
    spacedText(dl, display, labelSize, {fb.x - nw, labelY + 1.0f}, col(0, 0, 0, 200), number.c_str(), 0.14f, false);
    spacedText(dl, display, labelSize, {fb.x - nw, labelY}, col(0xdf, 0xbf, 0x8e), number.c_str(), 0.14f, false);
    // frame: shadow 0 8px 26px rgba(0,0,0,.8), 0 0 0 1px #000, the iron gradient, the gold hairline
    for (int i = 5; i >= 1; --i) {
      const float s = static_cast<float>(i) * 4.0f;
      dl->AddRectFilled({fa.x - s * 0.5f, fa.y + s * 0.3f}, {fb.x + s * 0.5f, fb.y + s}, col(0, 0, 0, 26), 2.0f + s);
    }
    dl->AddRect({fa.x - 1, fa.y - 1}, {fb.x + 1, fb.y + 1}, col(0, 0, 0), 2.0f);
    verticalGradient(dl, fa, fb, {{0.0f, col(0x23, 0x23, 0x23)}, {0.55f, col(0x0a, 0x0a, 0x0a)}, {1.0f, col(0x1a, 0x1a, 0x1a)}});
    dl->AddRect({fa.x + 1, fa.y + 1}, {fb.x - 1, fb.y - 1}, col(255, 220, 160, 18), 2.0f);
    dl->AddRect(fa, fb, col(190, 150, 95, 128), 2.0f);
    // .loader-track
    const ImVec2 ta{fa.x + 4.0f, fa.y + 4.0f}, tb{fb.x - 4.0f, fb.y - 4.0f};
    dl->AddRectFilled(ta, tb, col(5, 5, 5));
    dl->AddRectFilledMultiColor(ta, {tb.x, ta.y + 3.0f}, col(0, 0, 0, 200), col(0, 0, 0, 200), col(0, 0, 0, 0), col(0, 0, 0, 0));
    // .loader-fill: the molten ember, its glow, top light and bottom shade; .is-ready: brighter
    const float fillX = ta.x + std::max(2.0f, (tb.x - ta.x) * shown_ / 100.0f);
    const ImVec2 la = ta, lb{fillX, tb.y};
    for (int i = 4; i >= 1; --i) {
      const float s = static_cast<float>(i) * 3.0f;
      dl->AddRectFilled({la.x - s, la.y - s}, {lb.x + s, lb.y + s}, col(255, 110, 30, 22), s);
    }
    const float lift = ready() ? 1.2f : 1.0f;
    auto bright = [&](int r, int g, int b) { return col(std::min(255, static_cast<int>(r * lift)), std::min(255, static_cast<int>(g * lift)), std::min(255, static_cast<int>(b * lift))); };
    horizontalGradient(dl, la, lb, {{0.0f, bright(0x4a, 0x14, 0x08)}, {0.45f, bright(0xa3, 0x32, 0x10)}, {0.85f, bright(0xe2, 0x68, 0x1e)}, {1.0f, bright(0xff, 0xbe, 0x55)}});
    dl->AddLine({la.x, la.y + 0.5f}, {lb.x, la.y + 0.5f}, col(255, 225, 160, 140));
    dl->AddRectFilledMultiColor({la.x, lb.y - 3.0f}, lb, col(60, 10, 0, 0), col(60, 10, 0, 0), col(60, 10, 0, 166), col(60, 10, 0, 166));
    // ::before: the sheen, a 45%-wide band crossing the fill every 2.4 s (ease-in-out, background-position -60% -> 160%)
    {
      const float fw = lb.x - la.x, band = fw * 0.45f;
      const float t = static_cast<float>(std::fmod(now, 2.4) / 2.4);
      const float e = t < 0.5f ? 2.0f * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 2.0f) / 2.0f;
      const float p = -0.6f + 2.2f * e;
      const float bx = la.x + (fw - band) * p;
      dl->PushClipRect(la, lb, true);
      dl->AddRectFilledMultiColor({bx, la.y}, {bx + band * 0.5f, lb.y}, col(255, 225, 160, 0), col(255, 225, 160, 97), col(255, 225, 160, 97), col(255, 225, 160, 0));
      dl->AddRectFilledMultiColor({bx + band * 0.5f, la.y}, {bx + band, lb.y}, col(255, 225, 160, 97), col(255, 225, 160, 0), col(255, 225, 160, 0), col(255, 225, 160, 97));
      dl->PopClipRect();
    }
    // .loader-spark: 20px at the leading edge, #fff4cf -> #ffb04a (34%) -> nothing (70%), flickering (.7s alternate)
    {
      const float k = 0.5f - 0.5f * std::cos(std::numbers::pi_v<float> * static_cast<float>(now / 0.7));
      const float scale = 0.85f + 0.3f * k, opacity = 0.65f + 0.35f * k;
      const ImVec2 c{lb.x, (la.y + lb.y) * 0.5f};
      const float r = 10.0f * scale;
      for (int i = 8; i >= 1; --i) {
        const float f = static_cast<float>(i) / 8.0f;  // share of the radius
        const float rr = r * f;
        ImU32 ink;
        if (f > 0.7f) continue;
        if (f > 0.34f) ink = col(255, 0x90, 0x30, static_cast<int>(255 * opacity * (0.7f - f) / 0.36f * 0.5f));
        else ink = col(0xff, static_cast<int>(0xb0 + (0xf4 - 0xb0) * (1.0f - f / 0.34f)), static_cast<int>(0x4a + (0xcf - 0x4a) * (1.0f - f / 0.34f)), static_cast<int>(255 * opacity * 0.6f));
        dl->AddCircleFilled(c, rr, ink, 20);
      }
    }
    // .loader-cap: gold diamonds at both ends (10px, rotated 45 deg, #e0c08c -> #3b3b3b)
    for (float cx : {fa.x - 2.0f, fb.x + 2.0f}) {
      const float cy = (fa.y + fb.y) * 0.5f, d = 7.07f;
      dl->AddQuadFilled({cx, cy - d - 1}, {cx + d + 1, cy}, {cx, cy + d + 1}, {cx - d - 1, cy}, col(0, 0, 0));
      dl->AddQuadFilled({cx, cy - d}, {cx + d, cy}, {cx, cy + d}, {cx - d, cy}, col(0x3b, 0x3b, 0x3b));
      dl->AddTriangleFilled({cx, cy - d}, {cx + d, cy}, {cx - d, cy}, col(0xe0, 0xc0, 0x8c));
      dl->AddTriangleFilled({cx - d * 0.5f, cy - d * 0.5f}, {cx + d * 0.5f, cy + d * 0.5f}, {cx - d, cy}, col(0x8e, 0x7e, 0x63));
    }
    // .loader-embers: eight sparks riding the fill (left = min(x, max(2, pct)) %), rising 30px and fading (ember-rise)
    static constexpr struct { float x, d, t; } kEmbers[] = {{8, 0, 3.4f}, {19, 1.1f, 4.1f}, {31, 2.2f, 3.7f}, {44, 0.5f, 4.4f},
                                                         {57, 1.7f, 3.5f}, {68, 2.9f, 4.0f}, {79, 0.9f, 3.8f}, {90, 2.4f, 4.3f}};
    for (const auto& e : kEmbers) {
      if (now < e.d) continue;
      const float p = static_cast<float>(std::fmod(now - e.d, e.t) / e.t);
      const float o = p < 0.15f ? 0.95f * p / 0.15f : 0.95f * (1.0f - (p - 0.15f) / 0.85f);
      const float left = fa.x + (fb.x - fa.x) * std::min(e.x, std::max(2.0f, static_cast<float>(pct))) / 100.0f;
      const ImVec2 c{left + 7.0f * p + 1.5f, fa.y + 2.0f - 30.0f * p - 1.5f};
      const float s = 1.0f - 0.7f * p;
      dl->AddCircleFilled(c, 4.0f * s, col(255, 120, 30, static_cast<int>(90 * o)), 12);
      dl->AddCircleFilled(c, 1.5f * s, col(0xff, 0xb0, 0x4a, static_cast<int>(255 * o)), 8);
    }
  }

 private:
  float peak_ = 0.0f, shown_ = 0.0f;
  double readyAt_ = -1.0;
};

}  // namespace ember::loading
