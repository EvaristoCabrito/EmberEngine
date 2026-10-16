#pragma once

// The Map Editor's dialogue editor, copied from Engine2's src/editor/ember/DialogEditor.tsx: one DialogTree (the
// mission's opening, its closing, or one neutral unit's conversation) edited in a local copy until Salvar. Lines have a
// speaker, a portrait (a unit's sprite), a text and either "Leva a" (the next line) or branching replies, each reply
// with its next line and an affinity change (who answers, whose affinity moves, +3 / 0 / -3).

#include <imgui.h>

#include <algorithm>
#include <cstdio>
#include <functional>
#include <random>
#include <string>
#include <utility>
#include <vector>

#include "world/Campaign.h"  // Json

namespace ember {

class DialogEditorUi {
 public:
  struct Portrait {
    std::string id, label;  // sprite id, name shown
  };
  // `commit` gets the tree (null = no dialogue) and says whether it was saved; the window closes when it was.
  void open(std::string title, const e2::Json& tree, std::function<bool(const e2::Json&)> commit) {
    title_ = std::move(title), tree_ = tree, commit_ = std::move(commit);
    open_ = true, saveError_ = false, armedDelete_.clear();
    ImGui::OpenPopup("##dialogeditor");
  }
  bool isOpen() const { return open_; }

  // Drawn every frame (a modal over the editor).
  void draw(const std::vector<Portrait>& portraits, const std::vector<std::string>& affinityHeroes) {
    if (!open_) return;
    if (!ImGui::IsPopupOpen("##dialogeditor")) ImGui::OpenPopup("##dialogeditor");
    const ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos({io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f}, ImGuiCond_Always, {0.5f, 0.5f});
    ImGui::SetNextWindowSize({std::min(720.0f, io.DisplaySize.x - 32), io.DisplaySize.y * 0.88f}, ImGuiCond_Always);
    if (!ImGui::BeginPopupModal("##dialogeditor", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize)) return;
    ImGui::PushFont(nullptr, 22.0f);
    ImGui::TextUnformatted(title_.c_str());
    ImGui::PopFont();
    ImGui::SameLine(ImGui::GetWindowWidth() - 250);
    if (ImGui::Button("Salvar")) {
      if (commit_ && commit_(tree_)) close();
      else saveError_ = true;
    }
    if (!open_) {
      ImGui::EndPopup();
      return;
    }
    ImGui::SameLine();
    if (!tree_.is_null() && ImGui::Button("Apagar diálogo")) tree_ = e2::Json();
    ImGui::SameLine();
    if (ImGui::Button("X##close")) {
      close();
      ImGui::EndPopup();
      return;
    }
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::TextWrapped("As alterações só são gravadas ao clicar em Salvar. Afinidade por resposta: + = 3 pontos, 0 = neutro, − = perde 3 pontos.");
    ImGui::PopStyleColor();
    if (saveError_) ImGui::TextColored(ImColor(231, 133, 115), "Não foi possível salvar. Suas alterações continuam aqui; tente novamente.");
    ImGui::Separator();
    ImGui::BeginChild("##lines", {0, 0});
    if (tree_.is_null() || !tree_.contains("lines") || tree_["lines"].empty()) {
      ImGui::TextDisabled("Nenhum diálogo ainda.");
      if (ImGui::Button("Criar diálogo", {-1, 0})) addLine();
    } else {
      e2::Json& lines = tree_["lines"];
      // Linha inicial
      ImGui::TextColored(ImColor(180, 180, 180), "LINHA INICIAL");
      ImGui::SetNextItemWidth(-1);
      if (ImGui::BeginCombo("##start", lineLabel(lineById(tree_.value("startId", "")), 30).c_str())) {
        for (const e2::Json& l : lines)
          if (ImGui::Selectable((lineLabel(&l, 30) + "##" + l.value("id", "")).c_str(), l.value("id", "") == tree_.value("startId", ""))) tree_["startId"] = l.value("id", "");
        ImGui::EndCombo();
      }
      std::string remove;
      for (size_t i = 0; i < lines.size(); ++i) {
        e2::Json& line = lines[i];
        ImGui::PushID(static_cast<int>(i));
        ImGui::BeginChild("##line", {0, 0}, ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY);
        text("##speaker", line, "speaker", "Quem fala", 200);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(200);
        const std::string portrait = line.contains("portrait") && line["portrait"].is_string() ? line["portrait"].get<std::string>() : "";
        std::string shown = "Sem retrato";
        for (const Portrait& p : portraits)
          if (p.id == portrait) shown = p.label;
        if (ImGui::BeginCombo("##portrait", shown.c_str())) {
          if (ImGui::Selectable("Sem retrato", portrait.empty())) line.erase("portrait");
          for (const Portrait& p : portraits)
            if (ImGui::Selectable((p.label + "##" + p.id).c_str(), p.id == portrait)) line["portrait"] = p.id;
          ImGui::EndCombo();
        }
        ImGui::SameLine();
        const std::string key = "line:" + line.value("id", "");
        if (ImGui::SmallButton(armedDelete_ == key ? "Apagar?" : "X")) {
          if (armedDelete_ == key) remove = line.value("id", ""), armedDelete_.clear();
          else armedDelete_ = key;
        }
        textBox("##text", line, "text", "Texto da linha");
        bool branching = line.contains("replies") && line["replies"].is_array();
        if (ImGui::Checkbox("Respostas ramificadas", &branching)) {
          if (branching) line["replies"] = e2::Json::array({e2::Json{{"text", ""}}}), line.erase("next");
          else line.erase("replies");
        }
        if (branching) {
          e2::Json& replies = line["replies"];
          ImGui::Indent();
          int move = 0, at = -1, drop = -1;
          for (size_t k = 0; k < replies.size(); ++k) {
            e2::Json& reply = replies[k];
            ImGui::PushID(static_cast<int>(k));
            text("##rtext", reply, "text", "Texto da resposta", 180);
            ImGui::SameLine();
            nextCombo("##rnext", reply, "(fim)", 20, nullptr, 150);
            ImGui::SameLine();
            const e2::Json aff = reply.value("affinity", e2::Json::object());
            std::string from = aff.value("from", std::string("Kael")), to = aff.value("to", std::string("Neera"));
            int delta = aff.value("delta", 0);
            bool changed = false;
            ImGui::SetNextItemWidth(80);
            if (ImGui::BeginCombo("##from", from.c_str())) {
              for (const std::string& h : affinityHeroes)
                if (ImGui::Selectable(h.c_str(), h == from)) from = h, changed = true;
              ImGui::EndCombo();
            }
            ImGui::SameLine();
            ImGui::SetNextItemWidth(80);
            if (ImGui::BeginCombo("##to", to.c_str())) {
              for (const std::string& h : affinityHeroes)
                if (ImGui::Selectable(h.c_str(), h == to)) to = h, changed = true;
              ImGui::EndCombo();
            }
            ImGui::SameLine();
            ImGui::SetNextItemWidth(70);
            const char* deltas[3] = {"+ (+3)", "0", "− (−3)"};
            const int di = delta > 0 ? 0 : delta < 0 ? 2 : 1;
            if (ImGui::BeginCombo("##delta", deltas[di])) {
              for (int d = 0; d < 3; ++d)
                if (ImGui::Selectable(deltas[d], d == di)) delta = d == 0 ? 3 : d == 2 ? -3 : 0, changed = true;
              ImGui::EndCombo();
            }
            if (changed) reply["affinity"] = e2::Json{{"from", from}, {"to", to}, {"delta", delta}};
            ImGui::SameLine();
            ImGui::BeginDisabled(k == 0);
            if (ImGui::SmallButton("↑")) move = -1, at = static_cast<int>(k);
            ImGui::EndDisabled();
            ImGui::SameLine();
            ImGui::BeginDisabled(k + 1 == replies.size());
            if (ImGui::SmallButton("↓")) move = 1, at = static_cast<int>(k);
            ImGui::EndDisabled();
            ImGui::SameLine();
            if (ImGui::SmallButton("X##rdel")) drop = static_cast<int>(k);
            ImGui::PopID();
          }
          if (at >= 0) std::swap(replies[static_cast<size_t>(at)], replies[static_cast<size_t>(at + move)]);
          if (drop >= 0) {
            replies.erase(static_cast<size_t>(drop));
            if (replies.empty()) replies.push_back(e2::Json{{"text", ""}});
          }
          if (ImGui::SmallButton("+ resposta")) replies.push_back(e2::Json{{"text", ""}});
          ImGui::Unindent();
        } else {
          ImGui::TextColored(ImColor(180, 180, 180), "LEVA A");
          nextCombo("##next", line, "(fim do diálogo)", 30, &line, -1);
        }
        ImGui::EndChild();
        ImGui::PopID();
      }
      if (!remove.empty()) removeLine(remove);
      if (!tree_.is_null() && ImGui::Button("+ Adicionar linha", {-1, 0})) addLine();
    }
    ImGui::EndChild();
    ImGui::EndPopup();
  }

 private:
  void close() {
    open_ = false;
    ImGui::CloseCurrentPopup();
  }
  static std::string randomId(const char* prefix) {
    static std::mt19937 rng{std::random_device{}()};
    std::string id = std::string(prefix) + "-";
    for (int i = 0; i < 7; ++i) id += "0123456789abcdefghijklmnopqrstuvwxyz"[rng() % 36];
    return id;
  }
  const e2::Json* lineById(const std::string& id) const {
    if (tree_.is_null() || !tree_.contains("lines")) return nullptr;
    for (const e2::Json& l : tree_["lines"])
      if (l.value("id", "") == id) return &l;
    return nullptr;
  }
  // "<speaker or (sem nome)> — <the first n characters of the text or (vazio)>"
  static std::string lineLabel(const e2::Json* l, size_t n) {
    if (!l) return "";
    const std::string speaker = l->value("speaker", ""), text = l->value("text", "");
    std::string cut;
    size_t chars = 0;
    for (size_t i = 0; i < text.size() && chars < n; ++chars) {  // n code points (UTF-8)
      size_t len = 1;
      const unsigned char c = static_cast<unsigned char>(text[i]);
      if (c >= 0xF0) len = 4;
      else if (c >= 0xE0) len = 3;
      else if (c >= 0xC0) len = 2;
      cut += text.substr(i, len);
      i += len;
    }
    return (speaker.empty() ? std::string("(sem nome)") : speaker) + " — " + (cut.empty() ? std::string("(vazio)") : cut);
  }
  void addLine() {
    e2::Json line = {{"id", randomId("line")}, {"speaker", ""}, {"text", ""}};
    if (tree_.is_null() || !tree_.contains("lines") || tree_["lines"].empty()) {
      tree_ = e2::Json{{"id", randomId("tree")}, {"startId", line["id"]}, {"lines", e2::Json::array({line})}};
      return;
    }
    // Chains onto the last line when it is still a dead end (no next, no replies).
    e2::Json& lines = tree_["lines"];
    e2::Json& last = lines.back();
    const bool deadEnd = !(last.contains("next") && last["next"].is_string() && !last["next"].get<std::string>().empty()) && !last.contains("replies");
    if (deadEnd) last["next"] = line["id"];
    lines.push_back(std::move(line));
  }
  void removeLine(const std::string& id) {
    e2::Json kept = e2::Json::array();
    for (const e2::Json& l : tree_["lines"])
      if (l.value("id", "") != id) kept.push_back(l);
    if (kept.empty()) {
      tree_ = e2::Json();
      return;
    }
    if (tree_.value("startId", "") == id) tree_["startId"] = kept[0].value("id", "");
    tree_["lines"] = kept;
  }
  static void text(const char* id, e2::Json& o, const char* key, const char* hint, float width) {
    char buffer[1024];
    std::snprintf(buffer, sizeof(buffer), "%s", o.value(key, "").c_str());
    ImGui::SetNextItemWidth(width);
    if (ImGui::InputTextWithHint(id, hint, buffer, sizeof(buffer))) o[key] = std::string(buffer);
  }
  static void textBox(const char* id, e2::Json& o, const char* key, const char* hint) {
    char buffer[4096];
    std::snprintf(buffer, sizeof(buffer), "%s", o.value(key, "").c_str());
    if (ImGui::InputTextMultiline(id, buffer, sizeof(buffer), {-1, 64})) o[key] = std::string(buffer);
    if (o.value(key, "").empty() && !ImGui::IsItemActive()) {
      const ImVec2 at = ImGui::GetItemRectMin();
      ImGui::GetWindowDrawList()->AddText({at.x + 6, at.y + 4}, IM_COL32(120, 120, 120, 255), hint);
    }
  }
  // The next line of `o` ("next": an id, or null for the end); `self` is left out of the choices.
  void nextCombo(const char* id, e2::Json& o, const char* none, size_t n, const e2::Json* self, float width) {
    const std::string next = o.contains("next") && o["next"].is_string() ? o["next"].get<std::string>() : "";
    ImGui::SetNextItemWidth(width);
    if (ImGui::BeginCombo(id, next.empty() ? none : lineLabel(lineById(next), n).c_str())) {
      if (ImGui::Selectable(none, next.empty())) o["next"] = nullptr;
      for (const e2::Json& l : tree_["lines"]) {
        if (self && l.value("id", "") == self->value("id", "")) continue;
        if (ImGui::Selectable((lineLabel(&l, n) + "##" + l.value("id", "")).c_str(), l.value("id", "") == next)) o["next"] = l.value("id", "");
      }
      ImGui::EndCombo();
    }
  }

  bool open_ = false, saveError_ = false;
  std::string title_, armedDelete_;
  e2::Json tree_;
  std::function<bool(const e2::Json&)> commit_;
};

}  // namespace ember
