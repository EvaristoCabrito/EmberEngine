#pragma once

// A conversation's portraits (DialogOverlay.tsx's rule over content/engine2/portraits.json, export_portraits.ts): each line's
// {src, framed} by "<treeId>:<lineId>". Shared by the NPC conversations (NpcTalk.h) and the on-map cutscenes (MapStage.h).

#include <cctype>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

#include "world/Campaign.h"

namespace ember {

// DialogOverlay.tsx speakerKey: accents off, lower case, trimmed.
inline std::string speakerKey(const std::string& name) {
  std::string out;
  for (size_t i = 0; i < name.size(); ++i) {
    const unsigned char c = static_cast<unsigned char>(name[i]);
    if (c == 0xC3 && i + 1 < name.size()) {  // the accented Latin letters (UTF-8 C3 80-BF) to their plain letter
      static const char kPlain[] = "AAAAAAACEEEEIIIIDNOOOOOxOUUUUYTsaaaaaaaceeeeiiiidnooooo/ouuuuyty";
      const unsigned char d = static_cast<unsigned char>(name[++i]);
      if (d >= 0x80 && d <= 0xBF) out += static_cast<char>(std::tolower(static_cast<unsigned char>(kPlain[d - 0x80])));
      continue;
    }
    out += static_cast<char>(std::tolower(c));
  }
  const size_t a = out.find_first_not_of(" \t"), b = out.find_last_not_of(" \t");
  return a == std::string::npos ? std::string() : out.substr(a, b - a + 1);
}

// export_dialogs.ts portraitOf over portraits.json: each line's {src, framed} by "<treeId>:<lineId>" (DialogScreen's input).
// `characters`: the units on the board (name, sprite): a line's portrait falls back to its speaker's own art.
inline e2::Json npcPortraits(const e2::Json& tree, const std::vector<std::pair<std::string, std::string>>& characters) {
  static const e2::Json table = [] {
    std::ifstream in("content/engine2/portraits.json", std::ios::binary);
    e2::Json j = in ? e2::Json::parse(in, nullptr, false) : e2::Json();
    return j.is_object() ? j : e2::Json::object();
  }();
  const e2::Json speakers = table.value("speakers", e2::Json::object()), sprites = table.value("sprites", e2::Json::object());
  auto portraitFor = [&](const std::string& sprite) -> e2::Json {
    if (sprites.contains(sprite)) return sprites[sprite];
    return {{"src", "/game/sprites/" + sprite + "/1.png"}, {"framed", false}};  // assets.ts portraitFor's fallback
  };
  e2::Json out = e2::Json::object();
  const e2::Json lines = tree.value("lines", e2::Json::array());
  for (const e2::Json& line : lines) {
    const std::string speaker = speakerKey(line.value("speaker", ""));
    const std::string key = tree.value("id", "") + ":" + line.value("id", "");
    if (speaker == "vargan") {
      out[key] = table.value("vargan", e2::Json());
      continue;
    }
    std::string sprite = speakers.value(speaker, "");
    if (sprite.empty()) sprite = line.value("portrait", "");
    if (sprite.empty())
      for (const auto& [name, s] : characters)
        if (speakerKey(name) == speaker && !s.empty()) {
          sprite = s;
          break;
        }
    if (sprite.empty())
      for (const auto& [name, s] : characters) {
        const std::string k = speakerKey(name);
        if (!s.empty() && k.substr(0, k.find(',')) == speaker.substr(0, speaker.find(','))) {
          sprite = s;
          break;
        }
      }
    if (sprite.empty())
      for (const e2::Json& l : lines)
        if (!l.value("portrait", "").empty() && speakerKey(l.value("speaker", "")) == speaker) {
          sprite = l.value("portrait", "");
          break;
        }
    out[key] = sprite.empty() ? e2::Json() : portraitFor(sprite);
  }
  return out;
}

}  // namespace ember
