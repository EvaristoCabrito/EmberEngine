#pragma once

// A conversation's portraits (DialogOverlay.tsx's rule over content/engine2/portraits.json, export_portraits.ts): each line's
// {src, framed} by "<treeId>:<lineId>". Shared by the NPC conversations (NpcTalk.h) and the on-map cutscenes (MapStage.h).

#include <imgui.h>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "render/UiArt.h"
#include "world/Campaign.h"

namespace ember {

inline std::string speakerKey(const std::string& name);

// THE one portrait of each hero (content/hero-portraits.json; the user, 2026-10-25: "consolida tudo em um só retrato ... é ridiculo
// criar varios caminhos"): every screen that draws a hero asks here, by the hero's name, and draws it with drawHeroPortrait.
struct HeroPortrait {
  std::string file;      // under the art folder (content/game/public)
  float anchorY = 0.0f;  // no face measured: the part kept when the box is shorter than the picture (0 the top: no forehead cut)
  float faceX = 0.0f, faceY = 0.0f, faceH = 0.0f;  // the face box (shares of the picture: centre x, centre y, height); faceH 0 = none
};
// How a measured face is framed: its height this share of the box's, its centre this far down the box (Neera's approved framing in
// the world map's 5:6 box: face 0.32 of the height, centre at 0.34).
inline constexpr float kPortraitFaceShare = 0.32f, kPortraitFaceAt = 0.34f;
inline const HeroPortrait* heroPortrait(const std::string& name) {
  static const std::map<std::string, HeroPortrait> table = [] {
    std::map<std::string, HeroPortrait> out;
    std::ifstream in("content/hero-portraits.json", std::ios::binary);
    const e2::Json j = in ? e2::Json::parse(in, nullptr, false) : e2::Json();
    if (j.is_object() && j.contains("heroes") && j["heroes"].is_object())
      for (const auto& [hero, e] : j["heroes"].items())
        if (e.is_object() && e.contains("file") && e["file"].is_string()) {
          HeroPortrait p{e["file"].get<std::string>(), e.value("anchorY", 0.0f)};
          if (e.contains("face") && e["face"].is_array() && e["face"].size() == 3)
            p.faceX = e["face"][0].get<float>(), p.faceY = e["face"][1].get<float>(), p.faceH = e["face"][2].get<float>();
          out[speakerKey(hero)] = p;
        }
    return out;
  }();
  const auto it = table.find(speakerKey(name));
  return it == table.end() ? nullptr : &it->second;
}
// A hero's portrait filling the box, optionally rounded: the same framing on every screen. With a measured face, the window of the
// picture shown puts the face at kPortraitFaceShare of the box's height, centred across, its centre kPortraitFaceAt down (a picture
// too tight to zoom out that far shows as much of itself as the box's shape allows, the face as close to its place as the edges
// let it). Without one, the picture covers the box, cut on the sides or from below (anchorY).
// The status sheet frames a measured face closer, as close as the six heroes' busts stand in its tall box (their faces 0.3-0.6 of
// it, most near 0.45; the user, 2026-10-26: "selene ainda precisa do crop mais próximo na tela de status como os outros").
inline constexpr float kStatusFaceShare = 0.45f, kStatusFaceAt = 0.38f;
inline void drawHeroPortrait(ImDrawList* dl, const UiImage& img, ImVec2 a, ImVec2 b, const HeroPortrait& p, float rounding = 0.0f,
                             ImU32 tint = IM_COL32_WHITE, float faceShare = kPortraitFaceShare, float faceAt = kPortraitFaceAt) {
  const float bw = b.x - a.x, bh = b.y - a.y, iw = static_cast<float>(img.width), ih = static_cast<float>(img.height);
  if (bw <= 0 || bh <= 0 || iw <= 0 || ih <= 0) return;
  float winW, winH, x0, y0;  // the window of the picture shown, in its pixels
  if (p.faceH > 0.0f) {
    winH = std::min({p.faceH * ih / faceShare, ih, iw * bh / bw});
    winW = winH * bw / bh;
    x0 = std::clamp(p.faceX * iw - winW * 0.5f, 0.0f, iw - winW);
    y0 = std::clamp(p.faceY * ih - winH * faceAt, 0.0f, ih - winH);
  } else {
    const float scale = std::max(bw / iw, bh / ih);
    winW = bw / scale, winH = bh / scale;
    x0 = (iw - winW) * 0.5f, y0 = (ih - winH) * p.anchorY;
  }
  dl->AddImageRounded(img.texture, a, b, {x0 / iw, y0 / ih}, {(x0 + winW) / iw, (y0 + winH) / ih}, tint, rounding);
}

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
