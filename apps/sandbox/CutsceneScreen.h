#pragma once

// A film between screens, copied from Engine2 (GameApp CutsceneScreen + cutsceneSubtitles.ts + styles.css
// .cutscene-stage): the picture covering the screen (object-fit: cover), its subtitles in the language Opções picks
// (public/game/subtitles/<film>.pt-BR.vtt or .en-v2.vtt, cutsceneSubtitles.ts; drawn like a browser's caption: white on a
// dark band, bottom centre; none when Exibir legendas is off), the cutscene sound toggle top right (Opções' Som das
// cutscenes, independent of the game's mute) at Opções' cutscene volume, "Pular" bottom right. It ends at the film's own
// end, on Pular, or when the film never starts (8 s). Played by engine/media/VideoPlayer.

#include <imgui.h>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "CampaignUi.h"
#include "GamePrefs.h"
#include "media/VideoPlayer.h"
#include "render/UiArt.h"

namespace ember {

class CutsceneScreen {
 public:
  // `film`: a path under Engine2's public folder ("game/vau-intro.mp4"); its pt-BR subtitles are found by name.
  CutsceneScreen(const std::filesystem::path& engine2Public, const std::string& film) {
    player_ = std::make_unique<VideoPlayer>();
    player_->open(engine2Public / film);
    player_->setMuted(!soundOn() || gamePrefs().muted);  // audio.ts setMuted silences videos too
    player_->setVolume(gamePrefs().cutscene);
    const std::string name = std::filesystem::path(film).stem().string();
    const std::filesystem::path dir = engine2Public / "game" / "subtitles";
    if (gamePrefs().subtitleLanguage == "en") {
      loadSubtitles(dir / (name + ".en-v2.vtt"));
      if (cues_.empty()) loadSubtitles(dir / (name + ".en.vtt"));
    } else {
      loadSubtitles(dir / (name + ".pt-BR.vtt"));
      if (cues_.empty()) loadSubtitles(dir / (name + ".pt.vtt"));
    }
  }

  // Draws it full screen; true once it is over (ended, skipped, or never started).
  bool draw(ImVec2 size) {
    const float now = static_cast<float>(ImGui::GetTime());
    if (startedAt_ < 0) startedAt_ = now;
    player_->update();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled({0, 0}, size, IM_COL32(0, 0, 0, 255));
    if (const UiImage* img = player_->image()) imageCover(dl, *img, {0, 0}, size, 0.5f, 0.5f);
    // Subtitles: the cues showing now, one band per line.
    const double t = player_->time();
    std::vector<std::string> lines;
    for (const Cue& c : cues_)
      if (t >= c.from && t < c.to) lines.insert(lines.end(), c.lines.begin(), c.lines.end());
    if (!lines.empty() && gamePrefs().subtitles) {
      ImFont* ui = uiFont(UiFont::Ui);
      const float fs = size.y * 0.05f;
      float y = size.y * 0.965f - static_cast<float>(lines.size()) * fs * 1.25f;  // the last band ~3.5% above the edge
      for (const std::string& l : lines) {
        const ImVec2 ts = ui->CalcTextSizeA(fs, FLT_MAX, 0, l.c_str());
        const float x = (size.x - ts.x) / 2;
        dl->AddRectFilled({x - fs * 0.25f, y}, {x + ts.x + fs * 0.25f, y + fs * 1.2f}, IM_COL32(0, 0, 0, 204));
        dl->AddText(ui, fs, {x, y + fs * 0.1f}, IM_COL32(255, 255, 255, 255), l.c_str());
        y += fs * 1.25f;
      }
    }
    // The sound toggle, top right.
    ImGui::SetCursorScreenPos({size.x - 12 - 8 - 36, 12 + 8});
    if (ImGui::InvisibleButton("##filmsound", {36, 36})) {
      soundOn() = !soundOn();
      gamePrefs().save();
      player_->setMuted(!soundOn() || gamePrefs().muted);
    }
    {
      const bool hover = ImGui::IsItemHovered();
      const ImVec2 a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax();
      dl->AddRectFilled(a, b, IM_COL32(0, 0, 0, hover ? 140 : 102), 4);
      speaker(dl, {(a.x + b.x) / 2, (a.y + b.y) / 2}, IM_COL32(255, 255, 255, 230), soundOn());
    }
    // Pular, bottom right (a ghost button).
    bool skip = false;
    {
      ImFont* ui = uiFont(UiFont::Ui);
      const std::string skipLabel = tr("Pular");
      const ImVec2 ts = ui->CalcTextSizeA(16.0f, FLT_MAX, 0, skipLabel.c_str());
      const ImVec2 bsz{ts.x + 40, 44};
      const ImVec2 a{size.x - 20 - bsz.x, size.y - 20 - bsz.y};
      ImGui::SetCursorScreenPos(a);
      skip = ImGui::InvisibleButton("##skipfilm", bsz);
      const bool hover = ImGui::IsItemHovered();
      if (hover) dl->AddRectFilled(a, {a.x + bsz.x, a.y + bsz.y}, IM_COL32(255, 255, 255, 26), 6);
      dl->AddText(ui, 16.0f, {a.x + 20, a.y + (bsz.y - ts.y) / 2}, IM_COL32(240, 240, 240, 255), skipLabel.c_str());
    }
    const bool stuck = !player_->image() && now - startedAt_ > 8.0f;
    return skip || player_->finished() || stuck;
  }

  // For captures: how far the film has played.
  double time() const { return player_->time(); }

 private:
  struct Cue {
    double from = 0, to = 0;
    std::vector<std::string> lines;
  };

  // The cutscene sound setting (audio.ts getCutsceneAudioSettings().enabled): Opções' Som das cutscenes.
  static bool& soundOn() { return gamePrefs().cutsceneEnabled; }

  static double parseTime(const std::string& s) {
    int h = 0, m = 0, sec = 0, ms = 0;
    if (std::sscanf(s.c_str(), "%d:%d:%d.%d", &h, &m, &sec, &ms) == 4) return h * 3600.0 + m * 60.0 + sec + ms / 1000.0;
    if (std::sscanf(s.c_str(), "%d:%d.%d", &m, &sec, &ms) == 3) return m * 60.0 + sec + ms / 1000.0;
    return 0;
  }

  // WebVTT: "start --> end [settings]" then the cue's lines up to a blank line.
  void loadSubtitles(const std::filesystem::path& file) {
    std::ifstream in(file, std::ios::binary);
    if (!in) return;
    std::string line;
    Cue* open = nullptr;
    while (std::getline(in, line)) {
      if (!line.empty() && line.back() == '\r') line.pop_back();
      if (line.starts_with("\xEF\xBB\xBF")) line.erase(0, 3);
      if (const size_t arrow = line.find("-->"); arrow != std::string::npos) {
        Cue c;
        c.from = parseTime(line.substr(0, arrow));
        std::string rest = line.substr(arrow + 3);
        while (!rest.empty() && rest[0] == ' ') rest.erase(0, 1);
        c.to = parseTime(rest.substr(0, rest.find(' ')));
        cues_.push_back(std::move(c));
        open = &cues_.back();
        continue;
      }
      if (line.empty()) {
        open = nullptr;
        continue;
      }
      if (open) {
        // Drop simple markup (<i>, <b>, voice tags).
        std::string clean;
        bool tag = false;
        for (char ch : line) {
          if (ch == '<') tag = true;
          else if (ch == '>') tag = false;
          else if (!tag) clean += ch;
        }
        open->lines.push_back(clean);
      }
    }
  }

  // lucide Volume2 / VolumeX at 16 px.
  static void speaker(ImDrawList* dl, ImVec2 c, ImU32 ink, bool on) {
    const Lucide b(c, 16);
    const ImVec2 body[6] = {b.at(11, 5), b.at(6, 9), b.at(2, 9), b.at(2, 15), b.at(6, 15), b.at(11, 19)};
    dl->AddPolyline(body, 6, ink, b.stroke(), ImDrawFlags_Closed);
    if (on) {
      dl->PathArcTo(b.at(13, 12), 3.5f * b.scale, -0.9f, 0.9f, 8);
      dl->PathStroke(ink, b.stroke(), ImDrawFlags_None);
      dl->PathArcTo(b.at(13, 12), 7.0f * b.scale, -0.95f, 0.95f, 12);
      dl->PathStroke(ink, b.stroke(), ImDrawFlags_None);
    } else {
      dl->AddLine(b.at(22, 9), b.at(16, 15), ink, b.stroke());
      dl->AddLine(b.at(16, 9), b.at(22, 15), ink, b.stroke());
    }
  }

  std::unique_ptr<VideoPlayer> player_;
  std::vector<Cue> cues_;
  float startedAt_ = -1;
};

}  // namespace ember
