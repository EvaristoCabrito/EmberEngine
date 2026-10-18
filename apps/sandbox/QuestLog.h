#pragma once

// The quest log, copied from Engine2 (src/game/OverworldMapScreen.tsx QuestLogPanel): the Inn's quests (quests.ts), accepted
// ones under "Missões ativas" (in progress, or every objective met: to hand in), handed-in ones under "Missões concluídas"; a
// quest not accepted yet is not listed. One panel for the world map (OverworldStage) and the fight (MapStage: the save with the
// fight's own finds and kills folded in, so it follows them live). Both languages: tr() for the panel's words, Quests.h's
// questEnglish / potionEnglish for the quests' own (EmberEngine's translation; Engine2 has none).

#include <imgui.h>
#include <imgui_internal.h>  // BringWindowToDisplayFront

#include <algorithm>
#include <cfloat>
#include <format>
#include <string>
#include <vector>

#include "CampaignUi.h"
#include "GamePrefs.h"
#include "render/UiArt.h"
#include "world/Quests.h"

namespace ember {

// Draws the log over the screen (a veil under a centred ember panel). False once the veil or the close button was clicked.
// `quests` = the world's quests table, `potions` = its potions table (the rewards' names), `save` = the quest lists to read.
inline bool drawQuestLogPanel(const e2::Json& quests, const e2::Json& potions, const e2::Json& save) {
  constexpr ImU32 kAccent = IM_COL32(216, 216, 216, 255), kMuted = IM_COL32(180, 180, 180, 255);
  const ImGuiIO& io = ImGui::GetIO();
  const ImVec2 size = io.DisplaySize;
  const bool en = uiEnglish();
  // the veil (ember-veil): a click on it closes the log
  ImGui::SetNextWindowPos({0, 0});
  ImGui::SetNextWindowSize(size);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
  ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(0, 0, 0, 0));
  ImGui::Begin("##questveil", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);
  ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());  // on top, without the focus change that drops a held click
  verticalGradient(ImGui::GetWindowDrawList(), {0, 0}, size, {{0.0f, IM_COL32(0, 0, 0, 89)}, {1.0f, IM_COL32(0, 0, 0, 184)}});
  const bool veilClicked = ImGui::InvisibleButton("##questveilbtn", size);
  ImGui::End();
  ImGui::PopStyleColor();
  ImGui::PopStyleVar();
  if (veilClicked) return false;

  struct Entry {
    const e2::Json* q;
    std::string status;
    int have, total;
  };
  std::vector<Entry> active, done;
  const size_t count = quests.is_array() ? quests.size() : 0;
  if (quests.is_array())
    for (const e2::Json& q : quests) {
      int have = 0, total = 0;
      e2::questProgress(q, save, have, total);
      const std::string status = e2::questStatus(q, save);
      if (status == "done") done.push_back({&q, status, have, total});
      else if (status == "active" || status == "ready") active.push_back({&q, status, have, total});
    }

  // the panel (ember-panel, max-w-2xl, max-h-[82dvh])
  const float width = std::min(672.0f, size.x - 32.0f), inner = width - 40.0f;
  ImGui::SetNextWindowPos({size.x / 2, size.y / 2}, ImGuiCond_Always, {0.5f, 0.5f});
  ImGui::SetNextWindowSize({width, 0});
  ImGui::SetNextWindowSizeConstraints({0, 0}, {width, size.y * 0.82f});
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {20, 20});
  ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(0, 0, 0, 0));
  ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(0, 0, 0, 0));
  ImGui::Begin("##questlog", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize);
  ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());
  ImDrawList* dl = ImGui::GetWindowDrawList();
  {
    const ImVec2 a = ImGui::GetWindowPos(), b{a.x + ImGui::GetWindowSize().x, a.y + ImGui::GetWindowSize().y};
    dl->ChannelsSplit(2);
    dl->ChannelsSetCurrent(0);
    emberPanel(dl, a, b);
    dl->ChannelsSetCurrent(1);
  }
  ImFont* ui = uiFont(UiFont::Ui);
  auto kicker = [&](const std::string& s, float sz) {
    emberKicker(dl, ImGui::GetCursorScreenPos(), sz, upperUtf8(s).c_str());
    ImGui::Dummy({1, sz * 1.3f});
  };
  const ImVec2 head = ImGui::GetCursorScreenPos();
  kicker(tr(std::format("Estalagem · {}/{} concluídas", done.size(), count), std::format("Inn · {}/{} completed", done.size(), count)), 12.0f);
  emberTitle(dl, ImGui::GetCursorScreenPos(), 24.0f, tr("Registro de missões", "Quest log").c_str());
  ImGui::Dummy({1, 24.0f * 1.1f});
  ImGui::SetCursorScreenPos({head.x + inner - 36, head.y});
  bool open = !emberPlate("##questclose", {36, 36}, [](ImDrawList* d, ImVec2 a, ImVec2 b, ImU32) {
    iconX(d, {(a.x + b.x) / 2, (a.y + b.y) / 2}, 16, IM_COL32(223, 191, 142, 255));
  });
  ImGui::SetCursorScreenPos({head.x, head.y + 64});
  const std::vector<Entry>* lists[2] = {&active, &done};
  const std::string titles[2] = {tr("Missões ativas", "Active quests"), tr("Missões concluídas", "Completed quests")};
  const std::string empties[2] = {tr("Nenhuma missão ativa. Fale com o pessoal da Estalagem.", "No active quests. Talk to the people at the Inn."),
                                  tr("Nenhuma missão concluída ainda.", "No quests completed yet.")};
  for (int s = 0; s < 2; ++s) {
    kicker(std::format("{} · {}", titles[s], lists[s]->size()), 14.0f);
    if (lists[s]->empty()) {
      const ImVec2 a = ImGui::GetCursorScreenPos();
      emberSlot(dl, a, {a.x + inner, a.y + 44});
      dl->AddText(ui, 14.0f, {a.x + 12, a.y + 14}, kMuted, empties[s].c_str());
      ImGui::Dummy({inner, 52});
    }
    for (const Entry& e : *lists[s]) {
      const e2::Json& q = *e.q;
      const std::string id = q.value("id", ""), kind = q.value("kind", "");
      const ImVec2 a = ImGui::GetCursorScreenPos();
      const float h = e.status == "done" ? 64 : 84;
      emberSlot(dl, a, {a.x + inner, a.y + h});
      if (e.status == "done") iconCheck(dl, {a.x + 20, a.y + 22}, 16, kAccent);
      else dl->AddCircleFilled({a.x + 20, a.y + 22}, 4, kAccent);
      const std::string title = en ? e2::questEnglish(id, "title", q.value("title", "")) : q.value("title", "");
      emberTitle(dl, {a.x + 36, a.y + 10}, 18.0f, title.c_str());
      const std::string state = e.status == "done" ? tr("Concluída", "Completed") : e.status == "ready" ? tr("Entregar", "Hand in") : tr("Em andamento", "In progress");
      const float sw = ui->CalcTextSizeA(12.0f, FLT_MAX, 0, state.c_str()).x;
      dl->AddText(ui, 12.0f, {a.x + inner - 12 - sw, a.y + 14}, kMuted, state.c_str());
      const std::string target = en ? e2::questEnglish(id, "target", q.value("targetName", "")) : q.value("targetName", "");
      const std::string place = en ? e2::questEnglish(id, "place", q.value("place", "")) : q.value("place", "");
      const std::string what = (kind == "kill" ? tr("Abater ", "Slay ") + target : tr(std::format("Recolher {} itens", e.total), std::format("Collect {} items", e.total))) + " · " + place;
      dl->AddText(ui, 14.0f, {a.x + 36, a.y + 36}, kMuted, what.c_str(), nullptr, inner - 48);
      if (e.status != "done") {
        std::string line = kind == "kill" ? (e.have >= e.total ? tr("Alvo abatido — entregar na Estalagem", "Target slain — hand in at the Inn") : tr("Alvo ainda vivo", "Target still alive"))
                                          : tr(std::format("Encontrados {} / {}", e.have, e.total), std::format("Found {} / {}", e.have, e.total));
        line += tr(std::format(" · Recompensa {} Gold", q.value("reward", 0)), std::format(" · Reward {} Gold", q.value("reward", 0)));
        if (q.contains("rewardPotions") && q["rewardPotions"].is_array() && potions.is_array())
          for (const e2::Json& p : q["rewardPotions"])
            for (const e2::Json& pd : potions)
              if (p.is_string() && pd.value("id", "") == p.get<std::string>()) line += " + " + (en ? e2::potionEnglish(pd.value("id", ""), pd.value("name", "")) : pd.value("name", ""));
        dl->AddText(ui, 12.0f, {a.x + 36, a.y + 58}, kMuted, line.c_str());
      }
      ImGui::Dummy({inner, h + 8});
    }
    ImGui::Dummy({1, 8});
  }
  dl->ChannelsMerge();
  ImGui::End();
  ImGui::PopStyleColor(2);
  ImGui::PopStyleVar();
  return open;
}

}  // namespace ember
