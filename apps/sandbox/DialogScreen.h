#pragma once

// A conversation, copied from Ember (src/game/DialogOverlay.tsx): the veil, an ember panel with the speaker's
// portrait, their name as a kicker, the line, and either "Próximo" / "Ok" or one button per reply. It starts at the
// tree's startId, follows each line's or reply's `next`, and closes when there is none (or a line is missing). The
// portraits are Ember's own choice per line (export_dialogs.ts). A reply's affinity change is handed to the owner.
// Opções: each speaker, line and reply in the Diálogos language when the line has that translation (translatedText,
// else the original), the line's text at Tamanho do diálogo (1 / 1.15 / 1.3 em).

#include <imgui.h>

#include <algorithm>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "CampaignUi.h"
#include "GamePrefs.h"
#include "Portraits.h"
#include "render/UiArt.h"
#include "world/Campaign.h"

namespace ember {

class DialogScreen {
 public:
  // `tree`: Ember's DialogTree JSON; `portraits`: "<treeId>:<lineId>" -> {src, framed} (or null).
  DialogScreen(ember::Json tree, ember::Json portraits, std::filesystem::path artRoot)
      : tree_(std::move(tree)), portraits_(std::move(portraits)), public_(std::move(artRoot)) {
    line_ = tree_.value("startId", "");
  }

  // Cutscenes can advance with a click anywhere, without forcing the player to hit the small button.
  void requestAdvanceFromAnywhere() { advanceRequested_ = true; }

  // Draws it over everything; true once it closed. `onReply` gets each chosen reply (its affinity, if any).
  bool draw(ImVec2 size, const std::function<void(const ember::Json&)>& onReply = {}, std::optional<ImVec2> cutsceneSpeaker = {},
            const std::vector<ImVec2>& cutsceneHeads = {}) {
    const ember::Json* line = find(line_);
    if (!line) return true;  // a missing line closes rather than soft-locks
    // A cutscene places a compact box opposite the speaker, leaving the face and the action visible as in a tactical RPG.
    // Regular conversations keep their existing layout.
    const float w = cutsceneSpeaker ? std::min(680.0f, std::max(1.0f, size.x - 32.0f)) : std::max(1.0f, size.x - 24.0f);
    bool bottom = false;
    if (cutsceneSpeaker) {
      float best = FLT_MAX, chosenX = 16.0f, chosenY = 16.0f;
      for (int side = 0; side < 2; ++side) for (int row = 0; row < 2; ++row) {
        const float x = side ? size.x - w - 16.0f : 16.0f;
        const float top = row ? size.y - 236.0f : (side ? 60.0f : 16.0f);
        float cost = (row == (cutsceneSpeaker->y < size.y * 0.5f) ? 0.0f : 8.0f) +
                     (side != (cutsceneSpeaker->x < size.x * 0.5f) ? 4.0f : 0.0f);
        for (const ImVec2 head : cutsceneHeads) {
          if (head.x >= x - 44.0f && head.x <= x + w + 44.0f && head.y >= top - 40.0f && head.y <= top + 240.0f)
            cost += 100.0f;
        }
        if (cost < best) best = cost, chosenX = x, chosenY = row ? size.y - 16.0f : top, bottom = row != 0;
      }
      ImGui::SetNextWindowPos({chosenX, chosenY}, ImGuiCond_Always, {0.0f, bottom ? 1.0f : 0.0f});
    } else {
      ImGui::SetNextWindowPos({size.x / 2, 0.0f}, ImGuiCond_Always, {0.5f, 0.0f});
    }
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
    // a hero speaks with THE one portrait (Portraits.h heroPortrait), whatever conversation this is; anyone else as the tree says
    const HeroPortrait* hero = heroPortrait(line->value("speaker", ""));
    const ember::Json portrait = hero ? ember::Json{{"src", "/" + hero->file}, {"framed", true}}
                                   : portraits_.value(tree_.value("id", "") + ":" + line->value("id", ""), ember::Json());
    if (portrait.is_object()) {
      std::string src = portrait.value("src", "");
      if (const size_t q = src.find('?'); q != std::string::npos) src.resize(q);
      if (!src.empty() && src[0] == '/') src.erase(0, 1);
      const bool framed = portrait.value("framed", true);
      const UiImage* img = uiImage(public_ / std::filesystem::path(reinterpret_cast<const char8_t*>(src.c_str())));
      const float pw = 80, ph = framed ? 96.0f : 80.0f;
      if (img) {
        if (framed) {
          if (hero) drawHeroPortrait(dl, *img, at, {at.x + pw, at.y + ph}, *hero);
          else imageCover(dl, *img, at, {at.x + pw, at.y + ph});
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
    const std::string lang = gamePrefs().dialogueLanguage;
    const std::string speaker = translated(line->value("speaker", ""), line->value("speakerTranslations", ember::Json()), lang);
    emberKicker(dl, ImGui::GetCursorScreenPos(), 15.0f, upperUtf8(speaker).c_str());
    ImGui::Dummy({1, 22});
    ImGui::PushFont(uiFont(UiFont::Ui), 16.0f * gamePrefs().dialogueScale);
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(240, 240, 240, 255));
    ImGui::PushTextWrapPos(textX + textW - ImGui::GetWindowPos().x);  // window-local (the panel is centred: screen x clipped the line)
    const float textTop = ImGui::GetCursorScreenPos().y;
    const float lineH = ImGui::GetTextLineHeightWithSpacing();
    ImGui::TextUnformatted(translated(line->value("text", ""), line->value("translations", ember::Json()), lang).c_str());
    ImGui::PopTextWrapPos();
    ImGui::PopStyleColor();
    ImGui::PopFont();
    ImGui::Dummy({1, 12});
    bool close = false;
    std::string next;
    bool advance = false;
    const ember::Json replies = line->value("replies", ember::Json::array());
    if (!replies.empty()) {
      advanceRequested_ = false;  // Keep explicit reply choices deliberate.
      int i = 0;
      for (const ember::Json& reply : replies) {
        const std::string text = translated(reply.value("text", ""), reply.value("translations", ember::Json()), lang);
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
          next = reply.value("next", ember::Json()).is_string() ? reply["next"].get<std::string>() : "";
          advance = true;
        }
      }
    } else {
      // The user's layout (2026-11-06, a Paint mock-up over the box): the text area keeps room for a few lines (four, whatever the
      // line's length, so the box holds one size over the actors), and a small Ok / Próximo sits in its bottom-right corner instead
      // of a bar across the box.
      const bool hasNext = line->value("next", ember::Json()).is_string() && !line->value("next", "").empty();
      const float areaBottom = std::max(ImGui::GetCursorScreenPos().y, textTop + lineH + 4.0f);
      const ImVec2 bsize{hasNext ? 104.0f : 64.0f, 30.0f};
      const ImVec2 p{textX + textW - bsize.x, areaBottom};
      ImGui::SetCursorScreenPos(p);
      const bool buttonClicked = emberBigButton("##next", upperUtf8(hasNext ? tr("Próximo") : tr("Ok")).c_str(), p, bsize, true, 13.0f);
      const bool clickAnywhere = advanceRequested_;
      advanceRequested_ = false;
      if (clickAnywhere || buttonClicked) {
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
    if (cutsceneSpeaker) {
      const float x = std::clamp(cutsceneSpeaker->x, a.x + 36.0f, b.x - 36.0f);
      const float edge = bottom ? a.y + 2.0f : b.y - 2.0f;
      const float tip = edge + (bottom ? -18.0f : 18.0f);
      const ImVec2 left{x - 14.0f, edge}, right{x + 14.0f, edge}, point{x, tip};
      dl->AddTriangleFilled(left, right, point, IM_COL32(18, 17, 17, 248));
      dl->AddLine(left, point, IM_COL32(180, 143, 95, 220), 1.0f);
      dl->AddLine(point, right, IM_COL32(180, 143, 95, 220), 1.0f);
    }
    split.Merge(dl);
    ImGui::End();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar();
    return close;
  }

 private:
  const ember::Json* find(const std::string& id) const {
    // Pointers into tree_ itself (value() would hand back a copy).
    const auto lines = tree_.find("lines");
    if (lines == tree_.end() || !lines->is_array()) return nullptr;
    for (const ember::Json& l : *lines)
      if (l.value("id", "") == id) return &l;
    return nullptr;
  }
  ember::Json tree_, portraits_;
  std::filesystem::path public_;
  std::string line_;
  bool advanceRequested_ = false;
};

}  // namespace ember
