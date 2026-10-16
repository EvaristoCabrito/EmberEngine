#pragma once

// A conversation, copied from Engine2 (src/game/DialogOverlay.tsx): the veil, an ember panel with the speaker's
// portrait, their name as a kicker, the line, and either "Próximo" / "Ok" or one button per reply. It starts at the
// tree's startId, follows each line's or reply's `next`, and closes when there is none (or a line is missing). The
// portraits are Engine2's own choice per line (export_dialogs.ts). A reply's affinity change is handed to the owner.

#include <imgui.h>

#include <algorithm>
#include <filesystem>
#include <functional>
#include <string>

#include "CampaignUi.h"
#include "render/UiArt.h"
#include "world/Campaign.h"

namespace ember {

class DialogScreen {
 public:
  // `tree`: Engine2's DialogTree JSON; `portraits`: "<treeId>:<lineId>" -> {src, framed} (or null).
  DialogScreen(e2::Json tree, e2::Json portraits, std::filesystem::path engine2Public)
      : tree_(std::move(tree)), portraits_(std::move(portraits)), public_(std::move(engine2Public)) {
    line_ = tree_.value("startId", "");
  }

  // Draws it over everything; true once it closed. `onReply` gets each chosen reply (its affinity, if any).
  bool draw(ImVec2 size, const std::function<void(const e2::Json&)>& onReply = {}) {
    const e2::Json* line = find(line_);
    if (!line) return true;  // a missing line closes rather than soft-locks
    ImDrawList* bg = ImGui::GetForegroundDrawList();
    (void)bg;
    // The veil (ember-veil), catching every click under the panel.
    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize(size);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(0, 0, 0, 0));
    ImGui::Begin("##dialogveil", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing);
    verticalGradient(ImGui::GetWindowDrawList(), {0, 0}, size, {{0.0f, IM_COL32(0, 0, 0, 89)}, {1.0f, IM_COL32(0, 0, 0, 184)}});
    ImGui::End();
    ImGui::PopStyleColor();
    // The panel: max-w-xl, p-5, centred.
    const float w = std::min(576.0f, size.x - 32);
    ImGui::SetNextWindowPos({size.x / 2, size.y / 2}, ImGuiCond_Always, {0.5f, 0.5f});
    ImGui::SetNextWindowSize({w, 0});
    ImGui::SetNextWindowFocus();
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {20, 20});
    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(0, 0, 0, 0));
    ImGui::Begin("##dialog", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImDrawListSplitter split;
    split.Split(dl, 2);
    split.SetCurrentChannel(dl, 1);
    const float inner = w - 40;
    const ImVec2 at = ImGui::GetCursorScreenPos();
    float textX = at.x;
    const e2::Json& portrait = portraits_.value(tree_.value("id", "") + ":" + line->value("id", ""), e2::Json());
    if (portrait.is_object()) {
      std::string src = portrait.value("src", "");
      if (const size_t q = src.find('?'); q != std::string::npos) src.resize(q);
      if (!src.empty() && src[0] == '/') src.erase(0, 1);
      const bool framed = portrait.value("framed", true);
      const UiImage* img = uiImage(public_ / std::filesystem::path(reinterpret_cast<const char8_t*>(src.c_str())));
      const float pw = 80, ph = framed ? 96.0f : 80.0f;
      if (img) {
        if (framed) {
          imageCover(dl, *img, at, {at.x + pw, at.y + ph});
          dl->AddRect(at, {at.x + pw, at.y + ph}, IM_COL32(74, 74, 74, 255), 8, 1.0f);
        } else {
          const float k = std::min(pw / img->width, ph / img->height);
          dl->AddImage(img->texture, at, {at.x + img->width * k, at.y + img->height * k});
        }
      }
      textX += pw + 16;
    }
    const float textW = inner - (textX - at.x);
    ImGui::SetCursorScreenPos({textX, at.y});
    ImGui::BeginGroup();
    emberKicker(dl, ImGui::GetCursorScreenPos(), 15.0f, upperUtf8(line->value("speaker", "")).c_str());
    ImGui::Dummy({1, 22});
    ImGui::PushFont(uiFont(UiFont::Ui), 16.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(240, 240, 240, 255));
    ImGui::PushTextWrapPos(textX + textW);
    ImGui::TextUnformatted(line->value("text", "").c_str());
    ImGui::PopTextWrapPos();
    ImGui::PopStyleColor();
    ImGui::PopFont();
    ImGui::Dummy({1, 12});
    bool close = false;
    std::string next;
    bool advance = false;
    const e2::Json replies = line->value("replies", e2::Json::array());
    if (!replies.empty()) {
      int i = 0;
      for (const e2::Json& reply : replies) {
        const std::string text = reply.value("text", "");
        ImFont* ui = uiFont(UiFont::Ui);
        const float th = ui->CalcTextSizeA(14.0f, FLT_MAX, textW - 24, text.c_str()).y;
        const float h = std::max(44.0f, th + 16);
        const ImVec2 p = ImGui::GetCursorScreenPos();
        ImGui::PushID(i++);
        const bool clicked = ImGui::InvisibleButton("##reply", {textW, h});
        const bool hover = ImGui::IsItemHovered();
        ImGui::PopID();
        verticalGradient(dl, p, {p.x + textW, p.y + h}, {{0.0f, IM_COL32(35, 35, 35, 240)}, {0.55f, IM_COL32(10, 10, 10, 242)}, {1.0f, IM_COL32(26, 26, 26, 240)}});
        dl->AddRect(p, {p.x + textW, p.y + h}, hover ? IM_COL32(226, 170, 100, 217) : IM_COL32(190, 150, 95, 128), 2);
        dl->AddText(ui, 14.0f, {p.x + 12, p.y + (h - th) / 2}, hover ? IM_COL32(255, 224, 180, 255) : IM_COL32(226, 194, 148, 255), text.c_str(), nullptr, textW - 24);
        ImGui::Dummy({1, 4});
        if (clicked) {
          if (onReply) onReply(reply);
          next = reply.value("next", e2::Json()).is_string() ? reply["next"].get<std::string>() : "";
          advance = true;
        }
      }
    } else {
      const bool hasNext = line->value("next", e2::Json()).is_string() && !line->value("next", "").empty();
      const ImVec2 p = ImGui::GetCursorScreenPos();
      if (emberBigButton("##next", hasNext ? "PRÓXIMO" : "OK", p, {textW, 44}, true, 15.0f)) {
        next = hasNext ? line->value("next", "") : "";
        advance = true;
      }
    }
    ImGui::EndGroup();
    if (advance) {
      if (next.empty()) close = true;
      else line_ = next;
    }
    const ImVec2 a = ImGui::GetWindowPos(), b{a.x + ImGui::GetWindowSize().x, a.y + ImGui::GetWindowSize().y};
    split.SetCurrentChannel(dl, 0);
    emberPanel(dl, a, b);
    split.Merge(dl);
    ImGui::End();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar();
    return close;
  }

 private:
  const e2::Json* find(const std::string& id) const {
    // Pointers into tree_ itself (value() would hand back a copy).
    const auto lines = tree_.find("lines");
    if (lines == tree_.end() || !lines->is_array()) return nullptr;
    for (const e2::Json& l : *lines)
      if (l.value("id", "") == id) return &l;
    return nullptr;
  }
  e2::Json tree_, portraits_;
  std::filesystem::path public_;
  std::string line_;
};

}  // namespace ember
