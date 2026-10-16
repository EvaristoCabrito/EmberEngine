#pragma once

// The player's choices (Engine2 src/game/gamePreferences.ts + audio.ts), kept in %LOCALAPPDATA%\Ember\game-preferences.json,
// and the interface's language: tr() is Engine2's uiText (its English dictionary, content/engine2/ui-text.json from
// tools/scripts/engine2/export_help.ts, or the English a screen gives inline); translated() is its translatedText for
// dialogue lines. The Opções window (EmberMenus.h) changes them.

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

#include <nlohmann/json.hpp>

namespace ember {

// --- the player's choices (gamePreferences.ts + audio.ts) -------------------------------------------------------------

struct GamePrefs {
  bool muted = false;
  bool cutsceneEnabled = true;  // audio.ts cutsceneEnabled (the films' own sound toggle too)
  float music = 1.0f, sfx = 1.0f, cutscene = 1.0f;
  std::string uiLanguage = "pt", dialogueLanguage = "pt", subtitleLanguage = "pt";
  bool subtitles = true;
  float dialogueScale = 1.0f;  // 1 / 1.15 / 1.3

  static std::filesystem::path path() {
    const char* base = std::getenv("LOCALAPPDATA");
    return std::filesystem::path(base ? base : ".") / "Ember" / "game-preferences.json";
  }
  static GamePrefs load() {
    GamePrefs p;
    std::ifstream in(path(), std::ios::binary);
    if (!in) return p;
    const nlohmann::json j = nlohmann::json::parse(in, nullptr, false);
    if (!j.is_object()) return p;
    auto lang = [&](const char* k, std::string& out) {
      const std::string v = j.value(k, out);
      if (v == "pt" || v == "en") out = v;
    };
    auto unit = [&](const char* k, float& out) { out = std::clamp(j.value(k, out), 0.0f, 1.0f); };
    p.muted = j.value("muted", p.muted);
    p.cutsceneEnabled = j.value("cutsceneEnabled", p.cutsceneEnabled);
    unit("music", p.music), unit("sfx", p.sfx), unit("cutscene", p.cutscene);
    lang("uiLanguage", p.uiLanguage), lang("dialogueLanguage", p.dialogueLanguage), lang("subtitleLanguage", p.subtitleLanguage);
    p.subtitles = j.value("subtitles", p.subtitles);
    const float scale = j.value("dialogueScale", p.dialogueScale);
    if (scale == 1.0f || std::abs(scale - 1.15f) < 1e-3f || std::abs(scale - 1.3f) < 1e-3f) p.dialogueScale = scale;
    return p;
  }
  bool save() const {
    const nlohmann::json j = {{"muted", muted},           {"cutsceneEnabled", cutsceneEnabled}, {"music", music},
                              {"sfx", sfx},               {"cutscene", cutscene},               {"uiLanguage", uiLanguage},
                              {"dialogueLanguage", dialogueLanguage}, {"subtitleLanguage", subtitleLanguage},
                              {"subtitles", subtitles},   {"dialogueScale", dialogueScale}};
    std::error_code ec;
    std::filesystem::create_directories(path().parent_path(), ec);
    const std::filesystem::path tmp = path().string() + ".tmp";
    {
      std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
      if (!out) return false;
      out << j.dump(1);
      if (!out) return false;
    }
    std::filesystem::rename(tmp, path(), ec);
    return !ec;
  }
};
// The one copy every screen reads (loaded on first use, written on every change).
inline GamePrefs& gamePrefs() {
  static GamePrefs prefs = GamePrefs::load();
  return prefs;
}

// --- the interface's language (gamePreferences.ts uiText / translatedText) ----------------------------------------------

inline bool uiEnglish() { return gamePrefs().uiLanguage == "en"; }
// Engine2's dictionary (content/engine2/ui-text.json, export_help.ts): Portuguese -> English.
inline const nlohmann::json& uiDictionary() {
  static const nlohmann::json dict = [] {
    std::ifstream in("content/engine2/ui-text.json", std::ios::binary);
    const nlohmann::json j = in ? nlohmann::json::parse(in, nullptr, false) : nlohmann::json();
    return j.is_object() && j.contains("english") && j["english"].is_object() ? j["english"] : nlohmann::json::object();
  }();
  return dict;
}
// uiText(pt): the dictionary's English when the interface is in English, else the Portuguese.
inline std::string tr(const std::string& pt) {
  if (!uiEnglish()) return pt;
  const nlohmann::json& d = uiDictionary();
  const auto it = d.find(pt);
  return it != d.end() && it->is_string() ? it->get<std::string>() : pt;
}
// uiText(pt, { en }): the given English when the interface is in English.
inline std::string tr(const std::string& pt, const std::string& en) { return uiEnglish() && !en.empty() ? en : pt; }
// translatedText(original, translations, language): a dialogue line's own translation, else the original.
inline std::string translated(const std::string& original, const nlohmann::json& translations, const std::string& language) {
  if (language == "pt" || !translations.is_object()) return original;
  const std::string t = translations.value(language, std::string());
  return t.find_first_not_of(" \t\n") == std::string::npos ? original : t;
}

}  // namespace ember
