#include "world/SceneData.h"

#include <fstream>

#include <nlohmann/json.hpp>

#include "core/Log.h"

namespace ember::e2 {

namespace {

using json = nlohmann::json;

float num(const json& j, const char* key, float fallback = 0) { return j.contains(key) && j[key].is_number() ? j[key].get<float>() : fallback; }
std::string str(const json& j, const char* key) { return j.contains(key) && j[key].is_string() ? j[key].get<std::string>() : std::string(); }
bool flag(const json& j, const char* key) { return j.contains(key) && j[key].is_boolean() && j[key].get<bool>(); }

std::optional<SceneLight> light(const json& j) {
  if (!j.contains("light") || !j["light"].is_object()) return std::nullopt;
  const json& l = j["light"];
  SceneLight out;
  if (l.contains("color") && l["color"].is_array())
    for (size_t i = 0; i < 3 && i < l["color"].size(); ++i) out.color[i] = l["color"][i].get<float>();
  out.intensity = num(l, "intensity");
  out.radius = num(l, "radius");
  out.flicker = num(l, "flicker");
  out.minHeight = num(l, "minHeight", 1);
  out.seed = num(l, "seed");
  return out;
}

void ground(const json& j, float& smooth, float& hard) {
  if (!j.contains("ground")) return;
  smooth = num(j["ground"], "smooth");
  hard = num(j["ground"], "hard");
}

}  // namespace

std::optional<SceneData> SceneData::load(const std::filesystem::path& sceneJson) {
  std::ifstream in(sceneJson, std::ios::binary);
  const json j = in ? json::parse(in, nullptr, false) : json();
  if (!in || j.is_discarded() || !j.contains("decor")) {
    logError("scene: cannot read {} (import it with tools/scripts/engine2/export_scene.ts + measure_art.py)", sceneJson.string());
    return std::nullopt;
  }
  SceneData s;
  s.map = str(j, "map");
  s.backdrop = str(j, "backdrop");
  s.title = str(j, "title");
  if (j.contains("lighting")) {
    s.timeOfDay = str(j["lighting"], "timeOfDay");
    s.environment = str(j["lighting"], "environment");
    const json& l = j["lighting"];
    const auto opt = [&](const char* key) { return l.contains(key) && l[key].is_number() ? std::optional<float>(l[key].get<float>()) : std::nullopt; };
    s.sunIntensity = opt("sunIntensity");
    s.ambientIntensity = opt("ambientIntensity");
    s.bloomIntensity = opt("bloomIntensity");
    s.mistType = str(l, "mistType");
  }
  for (const json& d : j["decor"]) {
    const std::string kind = str(d, "kind");
    if (kind == "card") {
      SceneCard c;
      c.id = str(d, "id");
      c.image = str(d, "image");
      c.x = num(d, "x");
      c.z = num(d, "z");
      ground(d, c.groundSmooth, c.groundHard);
      c.w = num(d, "w", 1);
      c.h = num(d, "h", 1);
      c.mirror = flag(d, "mirror");
      c.onFloor = flag(d, "onFloor");
      c.castShadow = flag(d, "castShadow");
      c.standOn = flag(d, "standOn");
      if (d.contains("lockedYaw") && d["lockedYaw"].is_number()) c.lockedYaw = d["lockedYaw"].get<float>();
      c.nudge = num(d, "nudge");
      c.light = light(d);
      if (d.contains("art")) {
        const json& a = d["art"];
        c.art.baseU = num(a["base"], "u", 0.5f);
        c.art.baseV = num(a["base"], "v");
        c.art.flameU = num(a["flame"], "u", 0.5f);
        c.art.flameV = num(a["flame"], "v", 0.7f);
        c.art.flameFound = flag(a, "flameFound");
        if (a.contains("size")) c.art.width = a["size"][0].get<int>(), c.art.height = a["size"][1].get<int>();
      } else {
        logWarn("scene: card {} has no art measurement (run measure_art.py)", c.id);
      }
      s.cards.push_back(std::move(c));
    } else if (kind == "model") {
      SceneModel m;
      m.id = str(d, "id");
      m.model = str(d, "model");
      m.file = str(d, "file");
      m.height = num(d, "height", 1);
      m.x = num(d, "x");
      m.z = num(d, "z");
      ground(d, m.groundSmooth, m.groundHard);
      m.rotY = num(d, "rotY");
      s.models.push_back(std::move(m));
    } else if (kind == "architecture") {
      ++s.architecture;
      SceneArchitecture a;
      a.id = str(d, "id");
      if (d.contains("at") && d["at"].is_array() && d["at"].size() == 2) a.x = d["at"][0].get<int>(), a.y = d["at"][1].get<int>();
      a.rot = d.contains("rot") && d["rot"].is_number() ? static_cast<int>(d["rot"].get<double>()) : 0;
      s.pieces.push_back(std::move(a));
    }
  }
  for (const json& u : j["units"]) {
    SceneUnit o;
    o.side = str(u, "side");
    o.name = str(u, "name");
    o.classId = str(u, "classId");
    o.key = str(u, "key");
    o.sprite = str(u, "sprite");
    o.set = str(u, "set");
    o.clip = str(u, "clip");
    o.x = num(u, "x");
    o.z = num(u, "z");
    ground(u, o.groundSmooth, o.groundHard);
    o.facing = static_cast<int>(num(u, "facing", 1));
    o.mirror = flag(u, "mirror");
    if (u.contains("box")) {
      o.boxW = num(u["box"], "w", 1);
      o.boxH = num(u["box"], "h", 1);
      o.foot = num(u["box"], "foot");
    }
    if (u.contains("nat") && u["nat"].is_array()) o.natW = u["nat"][0].get<int>(), o.natH = u["nat"][1].get<int>();
    o.frames = static_cast<int>(num(u, "frames", 1));
    if (u.contains("pace") && u["pace"].is_array()) o.pace = u["pace"].get<std::vector<int>>();
    o.light = light(u);
    s.units.push_back(std::move(o));
  }
  return s;
}

const HeroPose* Hero::find(const std::string& pose) const {
  for (const HeroPose& p : poses)
    if (p.pose == pose) return &p;
  return nullptr;
}

std::optional<std::vector<Hero>> loadHeroes(const std::filesystem::path& heroesJson) {
  std::ifstream in(heroesJson, std::ios::binary);
  const json j = in ? json::parse(in, nullptr, false) : json();
  if (!in || j.is_discarded() || !j.contains("heroes")) {
    logError("heroes: cannot read {} (export it with tools/scripts/engine2/export_heroes.ts)", heroesJson.string());
    return std::nullopt;
  }
  // The strides each walk clip holds, beside the roster ("set/clip": number); anything missing keeps the default.
  json strides;
  {
    std::ifstream sin(heroesJson.parent_path() / "walk-strides.json", std::ios::binary);
    if (sin) strides = json::parse(sin, nullptr, false);
  }
  std::vector<Hero> heroes;
  for (const json& h : j["heroes"]) {
    Hero o;
    o.name = str(h, "name");
    o.key = str(h, "key");
    o.reversed = flag(h, "reversed");
    for (const json& p : h["poses"]) {
      HeroPose q;
      q.pose = str(p, "pose");
      q.label = str(p, "label");
      q.set = str(p, "set");
      q.clip = str(p, "clip");
      if (p.contains("box")) q.boxW = num(p["box"], "w", 1), q.boxH = num(p["box"], "h", 1), q.foot = num(p["box"], "foot");
      q.frames = static_cast<int>(num(p, "frames", 1));
      if (p.contains("pace") && p["pace"].is_array()) q.pace = p["pace"].get<std::vector<int>>();
      q.doneAt = static_cast<int>(num(p, "doneAt", -1));
      if (strides.is_object()) {
        const auto it = strides.find(q.set + "/" + q.clip);
        if (it != strides.end() && it->is_number()) q.strides = it->get<float>();
        else if (strides.contains("__default") && strides["__default"].is_number()) q.strides = strides["__default"].get<float>();
      }
      if (p.contains("bodyShift") && p["bodyShift"].is_array()) q.bodyShift = p["bodyShift"].get<std::vector<float>>();
      o.poses.push_back(std::move(q));
    }
    heroes.push_back(std::move(o));
  }
  return heroes;
}

}  // namespace ember::e2
