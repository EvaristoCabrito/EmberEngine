#include "world/SceneBuilder.h"

#include <cmath>
#include <format>
#include <fstream>
#include <numbers>

#include "core/Log.h"
#include "world/GroundMesh.h"

namespace ember::e2 {

namespace {

using json = nlohmann::json;

std::string str(const json& j, const char* key) { return j.contains(key) && j[key].is_string() ? j[key].get<std::string>() : std::string(); }
bool flag(const json& j, const char* key) { return j.contains(key) && j[key].is_boolean() && j[key].get<bool>(); }
float num(const json& j, const char* key, float fallback = 0) { return j.contains(key) && j[key].is_number() ? j[key].get<float>() : fallback; }

std::vector<FootOffset> offsets(const json& j) {
  std::vector<FootOffset> out;
  if (j.is_array())
    for (const json& o : j) out.push_back({o.value("dx", 0), o.value("dy", 0)});
  return out;
}

std::optional<SceneLight> lightOf(const json& j) {
  if (!j.is_object()) return std::nullopt;
  SceneLight l;
  if (j.contains("color") && j["color"].is_array())
    for (size_t i = 0; i < 3 && i < j["color"].size(); ++i) l.color[i] = j["color"][i].get<float>();
  l.intensity = num(j, "intensity");
  l.radius = num(j, "radius");
  l.flicker = num(j, "flicker");
  l.minHeight = num(j, "minHeight", 1);
  return l;
}

CatalogUnit unitOf(const json& j) {
  CatalogUnit u;
  u.classId = str(j, "classId");
  u.key = str(j, "key");
  u.sprite = str(j, "sprite");
  u.set = str(j, "set");
  u.clip = str(j, "clip");
  if (j.contains("box")) u.boxW = num(j["box"], "w", 1), u.boxH = num(j["box"], "h", 1), u.foot = num(j["box"], "foot");
  if (j.contains("nat") && j["nat"].is_array()) u.natW = j["nat"][0].get<int>(), u.natH = j["nat"][1].get<int>();
  u.frames = static_cast<int>(num(j, "frames", 1));
  if (j.contains("pace") && j["pace"].is_array()) u.pace = j["pace"].get<std::vector<int>>();
  u.reversed = flag(j, "reversed");
  if (j.contains("light")) u.light = lightOf(j["light"]);
  if (j.contains("front") && j["front"].is_array()) u.front = j["front"].get<std::vector<int>>();
  return u;
}

// JS `a % b` for numbers (sign of the dividend).
double jsMod(double a, double b) { return std::fmod(a, b); }

}  // namespace

bool EditorCatalog::load(const std::filesystem::path& file) {
  std::ifstream in(file, std::ios::binary);
  const json j = in ? json::parse(in, nullptr, false) : json();
  if (!in || j.is_discarded() || !j.contains("decorations")) {
    logError("editor catalog: cannot read {} (tools/scripts/engine2/export_editor_catalog.ts + measure_art.py --catalog)", file.string());
    return false;
  }
  for (const auto& [id, d] : j["decorations"].items()) {
    CatalogDecoration c;
    c.id = id;
    c.name = str(d, "name");
    c.footprint = offsets(d["footprint"]);
    c.blockingFootprint = offsets(d.value("blockingFootprint", json()));
    c.architectureSpan = flag(d, "architectureSpan");
    c.architecture = flag(d, "architecture");
    c.mirrorAlternate = flag(d, "mirrorAlternate");
    c.standOn = flag(d, "standOn");
    c.lockYaw = flag(d, "lockYaw");
    c.model3d = str(d, "model3d");
    c.exitKind = str(d, "exitKind");
    if (d.contains("model") && d["model"].is_object()) {
      SceneModel m;
      m.model = str(d["model"], "model");
      m.file = str(d["model"], "file");
      m.height = num(d["model"], "height", 1);
      c.model = m;
    }
    c.w = num(d["size"], "w", 1);
    c.h = num(d["size"], "h", 1);
    c.nudge = num(d, "nudge");
    if (d.contains("light")) c.light = lightOf(d["light"]);
    decorations_.emplace(id, std::move(c));
  }
  for (const auto& [art, faces] : j["facings"].items()) {
    std::vector<CatalogFacing>& list = facings_[art];
    for (const json& f : faces) list.push_back({str(f, "file"), str(f, "image"), flag(f, "mirror"), flag(f, "art")});
  }
  if (j.contains("measures"))
    for (const auto& [image, a] : j["measures"].items()) {
      SceneArt s;
      s.baseU = num(a["base"], "u", 0.5f);
      s.baseV = num(a["base"], "v");
      s.flameU = num(a["flame"], "u", 0.5f);
      s.flameV = num(a["flame"], "v", 0.7f);
      s.flameFound = flag(a, "flameFound");
      if (a.contains("size")) s.width = a["size"][0].get<int>(), s.height = a["size"][1].get<int>();
      measures_.emplace(image, s);
    }
  for (const auto& [name, u] : j["heroes"].items()) heroes_.emplace(name, unitOf(u));
  for (const auto& [id, u] : j["classes"].items()) classes_.emplace(id, unitOf(u));
  if (j.contains("backgrounds")) {
    for (const auto& [m, f] : j["backgrounds"]["byMap"].items()) backgroundByMap_.emplace(m, f.get<std::string>());
    for (const auto& [id, f] : j["backgrounds"]["byId"].items()) backgroundById_.emplace(id, f.get<std::string>());
  }
  return true;
}

const CatalogDecoration* EditorCatalog::decoration(const std::string& id) const {
  const auto it = decorations_.find(id);
  return it == decorations_.end() ? nullptr : &it->second;
}
const CatalogFacing* EditorCatalog::facing(const std::string& artId, int step) const {
  const auto it = facings_.find(artId);
  if (it == facings_.end() || step < 0 || step >= static_cast<int>(it->second.size())) return nullptr;
  return &it->second[static_cast<size_t>(step)];
}
const SceneArt* EditorCatalog::measure(const std::string& image) const {
  const auto it = measures_.find(image);
  return it == measures_.end() ? nullptr : &it->second;
}
const CatalogUnit* EditorCatalog::unit(const std::string& name, const std::string& classId, const std::string& side) const {
  if (side == "player" && !name.empty())
    if (const auto it = heroes_.find(name); it != heroes_.end()) return &it->second;
  const auto it = classes_.find(classId);
  return it == classes_.end() ? nullptr : &it->second;
}
std::string EditorCatalog::backdrop(const MapDraft& draft) const {
  if (draft.backgroundImage) {
    if (draft.backgroundImage->empty()) return {};
    const auto it = backgroundById_.find(*draft.backgroundImage);
    return it == backgroundById_.end() ? std::string() : it->second;
  }
  const auto it = backgroundByMap_.find(draft.id);
  return it == backgroundByMap_.end() ? std::string() : it->second;
}

std::vector<FootOffset> placedFootprint(const CatalogDecoration& def, int x, int y, double rot) {
  if (def.architectureSpan) {
    const double turn = jsMod(rot, 4);
    std::vector<FootOffset> out;
    for (const FootOffset& f : def.footprint)
      out.push_back(turn == 1 ? FootOffset{-f.dy, f.dx} : turn == 2 ? FootOffset{-f.dx, -f.dy} : turn == 3 ? FootOffset{f.dy, -f.dx} : f);
    return out;
  }
  // rotateFootprint: odd-r offset <-> cube about the real anchor, (x, y, z) -> (-z, -x, -y) per sixth.
  const int steps = ((static_cast<int>(std::floor(rot + 0.5)) % 6) + 6) % 6;
  if (steps == 0) return def.footprint;
  auto parity = [](int row) { return ((row % 2) + 2) % 2; };
  struct Cube {
    int x, y, z;
  };
  auto toCube = [&](int col, int row) {
    const int cx = col - (row - parity(row)) / 2;
    return Cube{cx, -cx - row, row};
  };
  const Cube anchor = toCube(x, y);
  std::vector<FootOffset> out;
  for (const FootOffset& f : def.footprint) {
    const Cube cell = toCube(x + f.dx, y + f.dy);
    int cx = cell.x - anchor.x, cy = cell.y - anchor.y, cz = cell.z - anchor.z;
    for (int i = 0; i < steps; ++i) {
      const int nx = -cz, ny = -cx, nz = -cy;
      cx = nx, cy = ny, cz = nz;
    }
    const int bx = cx + anchor.x, bz = cz + anchor.z;
    const int col = bx + (bz - parity(bz)) / 2;
    out.push_back({col - x, bz - y});
  }
  return out;
}

std::string placementArt(const MapDecoration& p) {
  if (p.id == "stone-stairs-up-001" || p.id == "stone-stairs-down-001") {
    // floorConnectorDirection
    const std::string dir = !p.connectorDirection.empty() ? p.connectorDirection
                            : p.id == "stone-stairs-up-001"   ? "up"
                            : p.id == "stone-stairs-down-001" ? "down"
                            : p.returnConnector               ? "up"
                                                              : "down";
    return dir == "up" ? "stone-stairs-up-001" : "stone-stairs-down-001";
  }
  if (p.id == "floor-connector") {
    const bool up = !p.connectorDirection.empty() ? p.connectorDirection == "up" : p.returnConnector;
    return up ? "floor-connector-up-gold-v1" : "floor-connector-down-red-v1";
  }
  return p.id;
}

SceneData buildScene(const Board& board, const EditorCatalog& catalog, std::map<std::string, int>* skipped) {
  const MapDraft& draft = board.draft();
  const HexLayout& L = board.layout();
  auto skip = [&](const std::string& why) {
    if (skipped) ++(*skipped)[why];
  };
  auto inBoard = [&](int c, int r) { return c >= 0 && r >= 0 && c < L.cols && r < L.rows; };
  auto heights = [&](double x, double z, float& smooth, float& hard) {
    smooth = static_cast<float>(groundHeightAt(board, EdgeStyle::Smooth, x, z));
    hard = static_cast<float>(groundHeightAt(board, EdgeStyle::Hard, x, z));
  };
  SceneData s;
  s.map = draft.id;
  s.title = draft.title;
  s.timeOfDay = draft.timeOfDay.empty() ? "day" : draft.timeOfDay;
  s.moonPhase = draft.moonPhase;
  s.environment = draft.environment.empty() ? "outdoor" : draft.environment;
  if (draft.sunIntensity) s.sunIntensity = static_cast<float>(*draft.sunIntensity);
  if (draft.ambientIntensity) s.ambientIntensity = static_cast<float>(*draft.ambientIntensity);
  if (draft.bloomSlider) s.bloomIntensity = static_cast<float>(*draft.bloomSlider);
  s.mistType = draft.mistType;
  s.backdrop = catalog.backdrop(draft);

  // The draft's decorations, then a barricade prop on every bare barricade tile (data.ts barricadeDecor).
  std::vector<MapDecoration> placements = draft.decorations;
  for (int y = 0; y < draft.rows; ++y)
    for (int x = 0; x < draft.cols; ++x) {
      const size_t i = static_cast<size_t>(y * draft.cols + x);
      if (i >= draft.tiles.size() || draft.tiles[i] != "barricade") continue;
      bool has = false;
      for (const MapDecoration& d : draft.decorations)
        if ((d.id == "barricade" || d.id == "barricade-2" || d.id == "wooden-barricade" || d.id == "wooden-barricade-1") && d.x == x && d.y == y) has = true;
      if (!has) placements.push_back({"barricade", x, y, 0});
    }
  for (const MapDecoration& p : placements) {
    const CatalogDecoration* def = catalog.decoration(p.id);
    if (!def) {
      skip("no definition: " + p.id);
      continue;
    }
    const std::vector<FootOffset> foot = placedFootprint(*def, p.x, p.y, p.rot);
    auto centroid = [&](double& cx, double& cz) {
      double sx = 0, sz = 0;
      int n = 0;
      for (const FootOffset& f : foot) {
        if (!inBoard(p.x + f.dx, p.y + f.dy)) continue;
        const Cell& c = board.cell(p.x + f.dx, p.y + f.dy);
        sx += c.x, sz += c.z, ++n;
      }
      if (!n) return false;
      cx = sx / n, cz = sz / n;
      return true;
    };
    if (def->architecture) {
      ++s.architecture;
      s.pieces.push_back({p.id, p.x, p.y, static_cast<int>(p.rot)});
      continue;
    }
    if (def->model) {
      double cx = 0, cz = 0;
      if (!centroid(cx, cz)) {
        skip("off the board");
        continue;
      }
      SceneModel m = *def->model;
      m.id = p.id;
      m.x = static_cast<float>(cx), m.z = static_cast<float>(cz);
      heights(cx, cz, m.groundSmooth, m.groundHard);
      m.rotY = static_cast<float>(-p.rot * std::numbers::pi / 2);
      if (def->light) {  // a 3D piece with its own fire (the hearth): lit like the cards' fires
        m.light = def->light;
        m.light->seed = static_cast<float>(jsMod(p.x * 7.31 + p.y * 3.17, 6.28));
      }
      s.models.push_back(std::move(m));
      continue;
    }
    const double rot = jsMod(p.rot + (def->mirrorAlternate && p.x >= draft.cols / 2.0 ? 3 : 0), 6);
    const int step = ((static_cast<int>(std::floor(rot + 0.5)) % 6) + 6) % 6;
    const CatalogFacing* face = catalog.facing(placementArt(p), step);
    if (!face || !face->art) {
      skip("no art: " + p.id);
      continue;
    }
    double cx = 0, cz = 0;
    if (!centroid(cx, cz)) {
      skip("off the board");
      continue;
    }
    SceneCard c;
    c.id = p.id;
    c.image = face->image;
    c.x = static_cast<float>(cx), c.z = static_cast<float>(cz);
    heights(cx, cz, c.groundSmooth, c.groundHard);
    c.w = def->w, c.h = def->h;
    c.mirror = face->mirror != p.mirrorX;
    c.onFloor = !def->exitKind.empty();
    c.castShadow = !def->light && !c.onFloor;
    if (def->lockYaw) c.lockedYaw = static_cast<float>(-(p.rot * std::numbers::pi) / 3);
    c.nudge = static_cast<float>(def->nudge + jsMod(p.x, 20) * 0.002);
    c.standOn = def->standOn;
    if (def->light) {
      c.light = def->light;
      c.light->seed = static_cast<float>(jsMod(p.x * 7.31 + p.y * 3.17, 6.28));
    }
    if (const SceneArt* a = catalog.measure(c.image)) c.art = *a;
    s.cards.push_back(std::move(c));
  }

  // Units: Engine2's dioramaView.placeUnits + UnitActor (idle pose).
  const std::pair<const std::vector<MapSpawn>*, const char*> sides[3] = {
      {&draft.playerSpawns, "player"}, {&draft.enemySpawns, "enemy"}, {&draft.neutralSpawns, "neutral"}};
  for (const auto& [list, side] : sides)
    for (const MapSpawn& sp : *list) {
      if (sp.x < 0 || sp.y < 0 || sp.x >= draft.cols || sp.y >= draft.rows) continue;
      const CatalogUnit* def = catalog.unit(sp.name, sp.classId, side);
      if (!def) {
        skip("no unit: " + sp.classId);
        continue;
      }
      double x = 0, z = 0;
      int n = 0;
      if (def->front) {
        for (int dx : *def->front)
          if (inBoard(sp.x + dx, sp.y)) {
            const Cell& c = board.cell(sp.x + dx, sp.y);
            x += c.x, z += c.z, ++n;
          }
      } else if (inBoard(sp.x, sp.y)) {
        const Cell& c = board.cell(sp.x, sp.y);
        x = c.x, z = c.z, n = 1;
      }
      if (!n) {
        const Cell& c = board.cell(sp.x, sp.y);
        x = c.x, z = c.z, n = 1;
      }
      x /= n, z /= n;
      SceneUnit u;
      u.side = side;
      u.name = sp.name;
      u.classId = sp.classId;
      u.key = def->key, u.sprite = def->sprite, u.set = def->set, u.clip = def->clip;
      u.x = static_cast<float>(x), u.z = static_cast<float>(z);
      heights(x, z, u.groundSmooth, u.groundHard);
      u.facing = std::string(side) == "enemy" ? -1 : 1;
      u.mirror = (u.facing == -1) != def->reversed;
      u.boxW = def->boxW, u.boxH = def->boxH, u.foot = def->foot;
      u.natW = def->natW, u.natH = def->natH;
      u.frames = def->frames;
      u.pace = def->pace;
      u.light = def->light;
      s.units.push_back(std::move(u));
    }
  return s;
}

// --- the check against export_scene.ts ------------------------------------------------------------------------------

int checkSceneBuilder(const BoardData& data, const EditorCatalog& catalog, const std::vector<std::filesystem::path>& maps,
                      const std::filesystem::path& scenes, const std::filesystem::path& engine2Public) {
  int differences = 0, checked = 0;
  size_t cards = 0, models = 0, units = 0;
  auto report = [&](const std::string& what) {
    if (++differences <= 30) logError("scene builder differs: {}", what);
  };
  auto close = [](float a, float b) { return std::abs(a - b) <= 1e-4f * std::max(1.0f, std::abs(b)); };
  for (const std::filesystem::path& mapFile : maps) {
    const std::filesystem::path sceneFile = scenes / (mapFile.stem().string() + ".scene.json");
    std::optional<MapDraft> draft = MapDraft::load(mapFile);
    std::optional<SceneData> ref = SceneData::load(sceneFile);
    if (!draft || !ref) {
      report("cannot read " + mapFile.string() + " or " + sceneFile.string());
      continue;
    }
    Board board(data, std::move(*draft));
    if (!board.loadRelief(engine2Public)) report(mapFile.stem().string() + ": relief");
    const SceneData s = buildScene(board, catalog);
    const std::string m = mapFile.stem().string();
    ++checked;
    if (s.backdrop != ref->backdrop) report(std::format("{}: backdrop '{}' vs '{}'", m, s.backdrop, ref->backdrop));
    if (s.timeOfDay != ref->timeOfDay || s.environment != ref->environment || s.mistType != ref->mistType) report(m + ": lighting");
    if (s.sunIntensity != ref->sunIntensity || s.ambientIntensity != ref->ambientIntensity || s.bloomIntensity != ref->bloomIntensity)
      report(m + ": lighting sliders");
    if (s.architecture != ref->architecture) report(std::format("{}: {} architecture vs {}", m, s.architecture, ref->architecture));
    if (s.cards.size() != ref->cards.size()) report(std::format("{}: {} cards vs {}", m, s.cards.size(), ref->cards.size()));
    for (size_t i = 0; i < std::min(s.cards.size(), ref->cards.size()); ++i) {
      const SceneCard &a = s.cards[i], &b = ref->cards[i];
      ++cards;
      const bool same = a.id == b.id && a.image == b.image && close(a.x, b.x) && close(a.z, b.z) && close(a.groundSmooth, b.groundSmooth) &&
                        close(a.groundHard, b.groundHard) && close(a.w, b.w) && close(a.h, b.h) && a.mirror == b.mirror && a.onFloor == b.onFloor &&
                        a.castShadow == b.castShadow && a.standOn == b.standOn && a.lockedYaw.has_value() == b.lockedYaw.has_value() &&
                        (!a.lockedYaw || close(*a.lockedYaw, *b.lockedYaw)) && close(a.nudge, b.nudge) && a.light.has_value() == b.light.has_value() &&
                        (!a.light || (close(a.light->intensity, b.light->intensity) && close(a.light->radius, b.light->radius) &&
                                      close(a.light->seed, b.light->seed) && close(a.light->flicker, b.light->flicker))) &&
                        close(a.art.baseU, b.art.baseU) && close(a.art.baseV, b.art.baseV) && close(a.art.flameU, b.art.flameU) &&
                        close(a.art.flameV, b.art.flameV) && a.art.width == b.art.width;
      if (!same) report(std::format("{}: card {} ({}) at {:.3f},{:.3f} vs {} ({}) at {:.3f},{:.3f}", m, i, a.id, a.x, a.z, b.id, b.image, b.x, b.z));
    }
    if (s.models.size() != ref->models.size()) report(std::format("{}: {} models vs {}", m, s.models.size(), ref->models.size()));
    for (size_t i = 0; i < std::min(s.models.size(), ref->models.size()); ++i) {
      const SceneModel &a = s.models[i], &b = ref->models[i];
      ++models;
      if (!(a.id == b.id && a.file == b.file && close(a.height, b.height) && close(a.x, b.x) && close(a.z, b.z) && close(a.groundSmooth, b.groundSmooth) &&
            close(a.rotY, b.rotY)))
        report(std::format("{}: model {} ({})", m, i, a.id));
    }
    if (s.units.size() != ref->units.size()) report(std::format("{}: {} units vs {}", m, s.units.size(), ref->units.size()));
    for (size_t i = 0; i < std::min(s.units.size(), ref->units.size()); ++i) {
      const SceneUnit &a = s.units[i], &b = ref->units[i];
      ++units;
      if (!(a.side == b.side && a.name == b.name && a.key == b.key && a.set == b.set && a.clip == b.clip && close(a.x, b.x) && close(a.z, b.z) &&
            close(a.groundSmooth, b.groundSmooth) && a.facing == b.facing && a.mirror == b.mirror && close(a.boxW, b.boxW) && close(a.boxH, b.boxH) &&
            close(a.foot, b.foot) && a.frames == b.frames && a.pace == b.pace && a.light.has_value() == b.light.has_value()))
        report(std::format("{}: unit {} ({} {})", m, i, a.name, a.key));
    }
  }
  if (!differences)
    logInfo("scene builder matches export_scene.ts on {} maps ({} cards, {} models, {} units)", checked, cards, models, units);
  else
    logError("scene builder: {} differences", differences);
  return differences;
}

}  // namespace ember::e2
