#pragma once

// Engine2's Map Editor (Modo teste > Map Editor), copied from src/editor/ember/MapEditorScreen.tsx (+ MapPreview3D.tsx):
// the map's settings, lighting and size on the left, the board in the game's own renderer on the right (MapStage with
// the draft in memory), rebuilt a moment after every edit. The draft is Engine2's MapDraft JSON, kept whole (every
// field it carries survives a save, ported or not).
//
//   Terreno     paint a terrain with one of its versions (Versões, City), turn a hex's art (Girar hex), replace the
//               whole base
//   Elevação    raise / lower / level a hex and its rings
//   Decoração   the 417 decorations by section (thumbnails, search): click places on the anchor hex or selects one
//               already there (Delete removes it), Girar objeto turns it a sixth
//   Herói / Inimigo / NPC / Invocação   click an empty hex to add, the same side's hex to remove; the unit list edits
//               name, class and level; Party completa adds the six heroes
//   Desfazer / Refazer (10 steps, a burst of clicks is one step), Novo, Carregar mapa da campanha, Abrir mapa salvo,
//   Salvar (content/maps/ember/<id><serial>.json, Engine2's file format and serials), Testar (the draft as a test battle:
//   Engine2's own exporters build its scene and rules, then the fight; back here afterwards).
//
// Not ported yet: Água 3D and 3D Walls / Decorations (EmberEngine draws neither water patches nor architecture yet),
// Gerar terreno (Engine2's dressMap), Locais / R-Encounter / CityHubs (campaign tables), the dialog editor, elemental FX,
// the technical grid, Exportar, the rule switches of a placed decoration.

#include <SDL3/SDL.h>
#include <imgui.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <deque>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <future>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "CampaignUi.h"
#include "MapStage.h"
#include "Stage.h"
#include "core/Log.h"
#include "render/Renderer.h"
#include "render/UiArt.h"
#include "rhi/VulkanContext.h"
#include "world/Board.h"
#include "world/Campaign.h"
#include "world/Overworld.h"
#include "world/SceneBuilder.h"

namespace ember {

class MapEditorStage : public Stage {
 public:
  struct Callbacks {
    // Voltar: the test menu; Testar: the prepared test battle. Both hand over the draft as it stands (main.cpp reopens
    // the editor on it, as GameApp keeps editorDraft across a playtest).
    std::function<void(const e2::Json& draft)> back;
    std::function<void(const MissionEntry&, const e2::Json& draft)> play;
  };
  struct Services {
    VulkanContext& context;
    Renderer& renderer;
    AssetManager& assets;
    JobSystem& jobs;
  };

  // `base`: the map options every preview starts from (sprite budget, Engine2's folder, heroes); `draft`: the map to
  // reopen (the one being edited before a test took the screen), else a new blank map.
  MapEditorStage(Services services, MapStageOptions base, CampaignPaths paths, const e2::Campaign& campaign, Callbacks callbacks,
                 std::optional<e2::Json> draft = std::nullopt)
      : services_(services), base_(std::move(base)), paths_(std::move(paths)), campaign_(campaign), callbacks_(std::move(callbacks)) {
    data_ = e2::BoardData::load(base_.boardData);
    if (!catalog_.load("content/engine2/editor-catalog.json")) note("Sem o catálogo do editor (content/engine2/editor-catalog.json).");
    editor_ = loadEditorTables("content/engine2/editor-catalog.json");
    draft_ = draft ? *draft : blankDraft();
    dirty_ = true;
  }
  ~MapEditorStage() override {
    if (playtest_.valid()) playtest_.wait();
    vkDeviceWaitIdle(services_.context.device);
  }

  // The draft as it stands (main.cpp keeps it across a test battle).
  const e2::Json& draft() const { return draft_; }
  // Captures: clicks as the board would get them, "mode:arg:x,y;..." (paint:<terrain>, raise, lower, deco:<id>, turn,
  // hero, enemy, npc, summon), then the preview as it stands; "save" writes the draft.
  // (Applied a second into the run, so the preview is rebuilt over a live one, as an edit does.)
  void previewEdits(const std::string& script) { script_ = script; }
  void runScript(const std::string& script) {
    size_t at = 0;
    while (at < script.size()) {
      const size_t end = std::min(script.find(';', at), script.size());
      const std::string step = script.substr(at, end - at);
      at = end + 1;
      if (step == "save") {
        save();
        continue;
      }
      if (step == "test") {
        playtest();
        continue;
      }
      const size_t c1 = step.find(':'), c2 = step.rfind(':');
      if (c1 == std::string::npos) continue;
      const std::string mode = step.substr(0, c1), arg = c1 == c2 ? std::string() : step.substr(c1 + 1, c2 - c1 - 1);
      int x = 0, y = 0;
      if (std::sscanf(step.c_str() + c2 + 1, "%d,%d", &x, &y) != 2) continue;
      turning_ = turningDeco_ = false;
      if (mode == "paint") mode_ = Mode::Paint, brush_ = arg, variant_ = 0;
      else if (mode == "raise" || mode == "lower") mode_ = Mode::Elevation, elevationTool_ = mode == "raise" ? 0 : 1, elevationRadius_ = arg.empty() ? 0 : std::atoi(arg.c_str());
      else if (mode == "deco") mode_ = Mode::Decoration, decoBrush_ = arg;
      else if (mode == "turn") mode_ = Mode::Decoration, turningDeco_ = true;
      else if (mode == "hero") mode_ = Mode::Player;
      else if (mode == "enemy") mode_ = Mode::Enemy;
      else if (mode == "npc") mode_ = Mode::Npc;
      else if (mode == "summon") mode_ = Mode::Summon;
      lastEdit_ = {};
      onCellClick(x, y);
    }
  }
  bool done() const override { return frames_ > 240 && !playtest_.valid(); }  // captures: the preview and its art, a Testar finished

  void frame(Renderer& renderer, VkExtent2D size) override {
    ++frames_;
    if (frames_ == 60 && !script_.empty()) runScript(std::exchange(script_, {}));
    // A click on the board (reported by the preview) is applied between frames.
    if (pendingClick_) {
      const auto [x, y] = *pendingClick_;
      pendingClick_.reset();
      onCellClick(x, y);
    }
    // The preview follows the draft a moment after the last edit (Engine2 setPreviewMission).
    const auto now = std::chrono::steady_clock::now();
    if (dirty_ && now - lastEdit_ > std::chrono::milliseconds(120)) rebuildPreview();
    // Testar finished preparing: the battle opens.
    if (playtest_.valid() && playtest_.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
      std::optional<MissionEntry> entry = playtest_.get();
      if (entry && callbacks_.play) callbacks_.play(*entry, draft_);
      else if (!entry) note(playtestError_);
    }
    if (preview_) preview_->frame(renderer, size);
    else {
      renderer.setScene(Camera{}, {});
      renderer.setMeshes(MeshSceneView{});
      renderer.setTerrain({});
      renderer.setWater({});
      renderer.setLights({});
      renderer.setBackground(Renderer::kBlackBackground);
    }
  }

  void event(const SDL_Event& e) override {
    if (e.type == SDL_EVENT_KEY_DOWN && !e.key.repeat && !(ImGui::GetCurrentContext() && ImGui::GetIO().WantTextInput)) {
      const bool ctrl = (e.key.mod & SDL_KMOD_CTRL) != 0;
      if (ctrl && e.key.key == SDLK_Z) return (e.key.mod & SDL_KMOD_SHIFT) ? redo() : undo();
      if (ctrl && e.key.key == SDLK_Y) return redo();
      if (e.key.key == SDLK_DELETE) return removeSelectedDecoration();
      if (e.key.key == SDLK_ESCAPE) return;  // the preview's pause menu is not part of the editor
    }
    if (preview_) preview_->event(e);
  }

  void drawPanel() override {
    if (preview_) preview_->drawPanel();  // the camera bar
    drawEditor();
  }

 private:
  // --- Engine2 tables (editor-catalog.json "editor") ------------------------------------------------------------------

  struct Version {
    int variant = 0;
    std::string label, thumb;
  };
  struct TerrainEntry {
    std::string id, name, swatch, hint, thumb;
    int variantCount = 1;
    double height = 0;
    std::vector<Version> versions;
  };
  struct ClassEntry {
    std::string name, role;
    bool summon = false;
  };
  struct EditorTables {
    int minGrid = 3, maxGrid = 160, testLevel = 15, enemyLevel0 = 1;
    std::vector<TerrainEntry> terrains;
    std::map<std::string, TerrainEntry> terrainInfo;  // every terrain id: name, height, variant count
    std::vector<Version> city;
    std::vector<std::string> decorationOrder, decorationSections;
    std::map<std::string, std::string> decoSection, decoTile, decoThumb;
    std::set<std::string> decoBlocks;
    std::map<std::string, ClassEntry> classes;
    std::vector<std::string> classOrder;
    std::vector<std::pair<std::string, std::string>> heroes;  // name, classId
    std::vector<std::pair<std::string, std::string>> npcs;    // id, name
    std::map<std::string, e2::Json> npcSpawn;                 // id: encounterNpcSpawn (name, classId, dialog, ...)
    struct Tod {
      std::string id, label;
      bool moon = false;
      double key = 0, ambient = 0;
    };
    std::vector<Tod> timeOfDay;
    double defaultSun = 3.5, defaultAmbient = 1.4, defaultBloom = 0.9;
    std::vector<std::tuple<std::string, std::string, std::string>> backgrounds;  // id, label, file
    std::vector<std::string> music;
  };

  static EditorTables loadEditorTables(const std::filesystem::path& file) {
    EditorTables t;
    std::ifstream in(file, std::ios::binary);
    const nlohmann::json j = in ? nlohmann::json::parse(in, nullptr, false) : nlohmann::json();
    if (!j.is_object() || !j.contains("editor")) return t;
    const nlohmann::json& e = j["editor"];
    auto s = [](const nlohmann::json& o, const char* k) { return o.contains(k) && o[k].is_string() ? o[k].get<std::string>() : std::string(); };
    t.minGrid = e.value("minGrid", 3), t.maxGrid = e.value("maxGrid", 160), t.testLevel = e.value("defaultTestLevel", 15), t.enemyLevel0 = e.value("enemyLevel0", 1);
    for (const nlohmann::json& tr : e["terrains"]) {
      TerrainEntry te{s(tr, "id"), s(tr, "name"), s(tr, "swatch"), s(tr, "hint"), {}, tr.value("variantCount", 1), tr.value("height", 0.0), {}};
      std::set<int> hidden;
      for (const nlohmann::json& h : tr["hidden"]) hidden.insert(h.get<int>());
      for (const nlohmann::json& v : tr["versions"])
        if (!hidden.contains(v.value("variant", 0))) te.versions.push_back({v.value("variant", 0), s(v, "label"), s(v, "thumb")});
      if (e.contains("terrainThumb") && e["terrainThumb"].contains(te.id) && e["terrainThumb"][te.id].is_string()) te.thumb = e["terrainThumb"][te.id].get<std::string>();
      t.terrains.push_back(std::move(te));
    }
    for (const auto& [id, ti] : e["terrainInfo"].items())
      t.terrainInfo[id] = TerrainEntry{id, s(ti, "name"), {}, {}, {}, ti.value("variantCount", 1), ti.value("height", 0.0), {}};
    for (const nlohmann::json& v : e["city"]) t.city.push_back({v.value("variant", 0), s(v, "label"), s(v, "thumb")});
    for (const nlohmann::json& id : e["decorationOrder"]) t.decorationOrder.push_back(id.get<std::string>());
    for (const nlohmann::json& id : e["decorationSections"]) t.decorationSections.push_back(id.get<std::string>());
    for (const auto& [id, d] : e["decorationInfo"].items()) {
      t.decoSection[id] = s(d, "section");
      if (d["tile"].is_string()) t.decoTile[id] = d["tile"].get<std::string>();
      t.decoThumb[id] = s(d, "thumb");
      if (d.value("blocksByDefault", false)) t.decoBlocks.insert(id);
    }
    for (const auto& [id, c] : e["classes"].items()) t.classes[id] = {s(c, "name"), s(c, "role"), c.value("summon", false)};
    for (const nlohmann::json& id : e["classOrder"]) t.classOrder.push_back(id.get<std::string>());
    for (const nlohmann::json& h : e["editorHeroes"]) t.heroes.push_back({s(h, "name"), s(h, "classId")});
    for (const nlohmann::json& n : e["npcs"]) {
      t.npcs.push_back({s(n, "id"), s(n, "name")});
      if (n.contains("spawn") && n["spawn"].is_object()) t.npcSpawn[s(n, "id")] = e2::Json::parse(n["spawn"].dump());
    }
    for (const auto& [id, v] : e["timeOfDay"].items()) t.timeOfDay.push_back({id, s(v, "label"), v.value("moon", false), v.value("key", 0.0), v.value("ambient", 0.0)});
    t.defaultSun = e["defaults"].value("sun", 3.5), t.defaultAmbient = e["defaults"].value("ambient", 1.4), t.defaultBloom = e["defaults"].value("bloom", 0.9);
    for (const nlohmann::json& b : e["backgrounds"]) t.backgrounds.emplace_back(s(b, "id"), s(b, "label"), s(b, "file"));
    for (const nlohmann::json& m : e["music"]) t.music.push_back(m.get<std::string>());
    return t;
  }

  // --- the draft --------------------------------------------------------------------------------------------------------

  // MapEditorScreen blankDraft: 20 x 20 plains, the day's light.
  e2::Json blankDraft() const {
    const int cols = 20, rows = 20, n = cols * rows;
    const long long stamp = static_cast<long long>(e2::Campaign::nowMs());
    std::string id = "custom-";
    for (long long v = stamp; v > 0; v /= 36) id.insert(id.begin() + 7, "0123456789abcdefghijklmnopqrstuvwxyz"[v % 36]);
    e2::Json d;
    d["id"] = id;
    d["index"] = 0;
    d["title"] = "Mapa sem nome";
    d["place"] = "";
    d["briefing"] = "";
    d["objective"] = "Derrote todos os inimigos";
    d["win"] = "rout";
    d["hub"] = false;
    d["autoTactics"] = true;
    d["fog"] = false;
    d["environment"] = "outdoor";
    d["timeOfDay"] = "day";
    d["sunIntensity"] = editor_.defaultSun;
    d["ambientIntensity"] = editor_.defaultAmbient;
    d["mistIntensity"] = 0.2;
    d["mistSpeed"] = 1;
    d["mistType"] = "none";
    d["bloomIntensity"] = editor_.defaultBloom;
    d["wispIntensity"] = 0.02;
    d["wispSpeed"] = 1;
    d["wispColor"] = 0xffa552;
    d["locationId"] = "";
    d["cols"] = cols;
    d["rows"] = rows;
    d["tiles"] = e2::Json(std::vector<std::string>(n, "plains"));
    d["tileVariants"] = e2::Json(std::vector<int>(n, 0));
    d["baseTile"] = "plains";
    d["baseVariant"] = 0;
    d["tileRots"] = e2::Json(std::vector<int>(n, 0));
    d["music"] = "";
    d["decorations"] = e2::Json::array();
    d["elementalFx"] = e2::Json::array();
    d["playerSpawns"] = e2::Json::array();
    d["enemySpawns"] = e2::Json::array();
    d["neutralSpawns"] = e2::Json::array();
    return d;
  }

  int cols() const { return draft_.value("cols", 0); }
  int rows() const { return draft_.value("rows", 0); }
  e2::Json& arr(const char* key) {
    if (!draft_.contains(key) || !draft_[key].is_array()) draft_[key] = e2::Json::array();
    return draft_[key];
  }
  // Every edit goes through here: history (a burst within 600 ms is one step, Engine2's coalescing), then the preview.
  void beginEdit() {
    const auto now = std::chrono::steady_clock::now();
    if (now - lastEdit_ > std::chrono::milliseconds(600) || past_.empty()) {
      past_.push_back(draft_);
      if (past_.size() > 10) past_.pop_front();
      future_.clear();
    }
    lastEdit_ = now;
    dirty_ = true;
  }
  void undo() {
    if (past_.empty()) return;
    future_.push_front(draft_);
    if (future_.size() > 10) future_.pop_back();
    draft_ = past_.back();
    past_.pop_back();
    dirty_ = true, lastEdit_ = {};
  }
  void redo() {
    if (future_.empty()) return;
    past_.push_back(draft_);
    draft_ = future_.front();
    future_.pop_front();
    dirty_ = true, lastEdit_ = {};
  }
  void note(std::string text) {
    note_ = std::move(text);
    noteAt_ = static_cast<float>(ImGui::GetCurrentContext() ? ImGui::GetTime() : 0.0);
  }

  // data.ts placedFootprint for a draft placement.
  std::vector<e2::FootOffset> footprintOf(const e2::Json& p) const {
    const e2::CatalogDecoration* def = catalog_.decoration(p.value("id", ""));
    if (!def) return {};
    return e2::placedFootprint(*def, p.value("x", 0), p.value("y", 0), p.value("rot", 0.0));
  }
  bool covers(const e2::Json& p, int x, int y) const {
    for (const e2::FootOffset& f : footprintOf(p))
      if (p.value("x", 0) + f.dx == x && p.value("y", 0) + f.dy == y) return true;
    return false;
  }
  // baseForDraft: the ground a stamped terrain falls back to.
  std::pair<std::string, int> baseOf() const {
    std::string tile = draft_.contains("baseTile") && draft_["baseTile"].is_string() ? draft_["baseTile"].get<std::string>()
                       : draft_["tiles"].empty()                                    ? std::string("plains")
                                                                                    : draft_["tiles"][0].get<std::string>();
    const auto it = editor_.terrainInfo.find(tile);
    const int maxVariant = std::max(1, it != editor_.terrainInfo.end() ? it->second.variantCount : 1) - 1;
    int v = draft_.contains("baseVariant") && draft_["baseVariant"].is_number() ? draft_["baseVariant"].get<int>()
            : draft_["tileVariants"].empty() || !draft_["tileVariants"][0].is_number() ? 0
                                                                                        : draft_["tileVariants"][0].get<int>();
    return {tile, std::clamp(v, 0, maxVariant)};
  }
  int variantCount(const std::string& terrain) const {
    const auto it = editor_.terrainInfo.find(terrain);
    return it == editor_.terrainInfo.end() ? 1 : it->second.variantCount;
  }

  // --- the board's clicks (MapEditorScreen onCellClick) ------------------------------------------------------------------

  void onCellClick(int x, int y) {
    if (x < 0 || y < 0 || x >= cols() || y >= rows()) return;
    const int i = y * cols() + x;
    if (mode_ == Mode::Elevation) return elevate(x, y);
    if (mode_ == Mode::Paint) {
      beginEdit();
      if (turning_) {
        e2::Json& rots = arr("tileRots");
        while (static_cast<int>(rots.size()) < cols() * rows()) rots.push_back(0);
        rots[i] = (rots[i].is_number() ? rots[i].get<int>() + 1 : 1) % 6;
        note(std::format("{} em {},{}: girado para {}°{}.", terrainName(draft_["tiles"][i].get<std::string>()), x, y, rots[i].get<int>() * 60,
                         rots[i].get<int>() == 0 ? " (de volta ao original)" : ""));
      } else {
        draft_["tiles"][i] = brush_;
        e2::Json& v = arr("tileVariants");
        while (static_cast<int>(v.size()) < cols() * rows()) v.push_back(0);
        v[i] = std::min(variant_, variantCount(brush_) - 1);
      }
      return;
    }
    if (mode_ == Mode::Decoration) {
      if (turningDeco_) return turnDecoration(x, y);
      return toggleDecoration(x, y);
    }
    toggleSpawn(x, y);
  }

  std::string terrainName(const std::string& id) const {
    const auto it = editor_.terrainInfo.find(id);
    return it == editor_.terrainInfo.end() ? id : it->second.name;
  }

  // Elevação: the hex and `radius` rings around it raised, lowered or set to a level (0..12); void is left alone.
  void elevate(int x, int y) {
    beginEdit();
    const int n = cols() * rows();
    e2::Json& elev = arr("terrainElevations");
    for (int k = static_cast<int>(elev.size()); k < n; ++k) {
      const auto it = editor_.terrainInfo.find(draft_["tiles"][k].get<std::string>());
      elev.push_back(it == editor_.terrainInfo.end() ? 0.0 : it->second.height);
    }
    for (int k = 0; k < n; ++k)
      if (!elev[k].is_number()) {
        const auto it = editor_.terrainInfo.find(draft_["tiles"][k].get<std::string>());
        elev[k] = it == editor_.terrainInfo.end() ? 0.0 : it->second.height;
      }
    std::vector<std::pair<int, int>> frontier{{x, y}};
    std::set<int> visited;
    for (int ring = 0; ring <= elevationRadius_; ++ring) {
      std::vector<std::pair<int, int>> next;
      for (const auto& [cx, cy] : frontier) {
        if (cx < 0 || cy < 0 || cx >= cols() || cy >= rows()) continue;
        const int k = cy * cols() + cx;
        if (!visited.insert(k).second) continue;
        if (draft_["tiles"][k] != "void")
          elev[k] = elevationTool_ == 2 ? static_cast<double>(elevationLevel_)
                                        : std::clamp(elev[k].get<double>() + (elevationTool_ == 0 ? 1.0 : -1.0), 0.0, 12.0);
        for (const e2::OverworldHex& nb : e2::Overworld::neighbors(cx, cy)) next.push_back({nb.col, nb.row});
      }
      frontier = std::move(next);
    }
  }

  // toggleDecoration: a click on a placed prop selects it; elsewhere the brush is placed on the anchor hex (refused when
  // its footprint would land on another prop); a Waypoint shares hexes with anything.
  void toggleDecoration(int x, int y) {
    e2::Json& decos = arr("decorations");
    const e2::Json* clicked = nullptr;
    for (const e2::Json& p : decos)
      if (covers(p, x, y)) {
        clicked = &p;
        break;
      }
    const e2::CatalogDecoration* brushDef = catalog_.decoration(decoBrush_);
    const bool brushIsWaypoint = brushDef && !brushDef->exitKind.empty();
    if (clicked) {
      const e2::CatalogDecoration* cd = catalog_.decoration(clicked->value("id", ""));
      const bool clickedWaypointAnchor = cd && !cd->exitKind.empty() && clicked->value("x", 0) == x && clicked->value("y", 0) == y;
      if (!brushIsWaypoint || clickedWaypointAnchor) {
        selected_ = *clicked;
        decoSection_ = editor_.decoSection.contains(clicked->value("id", "")) ? editor_.decoSection.at(clicked->value("id", "")) : decoSection_;
        if (!catalog_.decoration(clicked->value("id", ""))->model3d.empty()) {
        } else {
          decoBrush_ = clicked->value("id", "");
        }
        note(std::format("{} selecionada. Pressione Delete para remover.", cd ? cd->name : clicked->value("id", "")));
        return;
      }
    }
    if (!brushDef) return;
    e2::Json placed = {{"id", decoBrush_}, {"x", x}, {"y", y}};
    if (editor_.decoBlocks.contains(decoBrush_)) placed["blocksPath"] = true;
    // previewDecorationCells: every hex the other props cover.
    std::set<std::pair<int, int>> taken;
    for (const e2::Json& p : decos)
      for (const e2::FootOffset& f : footprintOf(p)) taken.insert({p.value("x", 0) + f.dx, p.value("y", 0) + f.dy});
    for (const e2::FootOffset& f : footprintOf(placed))
      if (!brushIsWaypoint && taken.contains({x + f.dx, y + f.dy})) return note(std::format("{} não cabe aqui — bateria em outra decoração.", brushDef->name));
    beginEdit();
    if (const auto t = editor_.decoTile.find(decoBrush_); t != editor_.decoTile.end())
      for (const e2::FootOffset& f : brushDef->footprint) {
        const int cx = x + f.dx, cy = y + f.dy;
        if (cx >= 0 && cy >= 0 && cx < cols() && cy < rows()) draft_["tiles"][cy * cols() + cx] = t->second;
      }
    if (const auto t = editor_.decoTile.find(decoBrush_); t != editor_.decoTile.end())
      note(std::format("{} sobre {} — o terreno vai junto.", brushDef->name, terrainName(t->second)));
    decos.push_back(std::move(placed));
  }

  // turnDecoration: the prop under the hex a sixth of a turn, footprint and stamped terrain with it; refused when it would
  // land on another prop.
  void turnDecoration(int x, int y) {
    e2::Json& decos = arr("decorations");
    for (e2::Json& p : decos) {
      if (!covers(p, x, y)) continue;
      const e2::CatalogDecoration* def = catalog_.decoration(p.value("id", ""));
      if (!def) return;
      e2::Json turned = p;
      turned["rot"] = (p.value("rot", 0) + 1) % (def->model3d.empty() ? 6 : 4);
      const std::vector<e2::FootOffset> before = footprintOf(p), after = footprintOf(turned);
      if (def->exitKind.empty()) {
        std::set<std::pair<int, int>> taken;
        for (const e2::Json& o : decos)
          if (&o != &p)
            for (const e2::FootOffset& f : footprintOf(o)) taken.insert({o.value("x", 0) + f.dx, o.value("y", 0) + f.dy});
        for (const e2::FootOffset& f : after)
          if (taken.contains({p.value("x", 0) + f.dx, p.value("y", 0) + f.dy}))
            return note(std::format("{} não cabe girada aqui — bateria em outra decoração.", def->name));
      }
      beginEdit();
      if (const auto t = editor_.decoTile.find(def->id); t != editor_.decoTile.end()) {
        const auto [baseTile, baseVariant] = baseOf();
        for (const e2::FootOffset& f : before) {
          const int cx = p.value("x", 0) + f.dx, cy = p.value("y", 0) + f.dy;
          if (cx < 0 || cy < 0 || cx >= cols() || cy >= rows()) continue;
          const int k = cy * cols() + cx;
          if (draft_["tiles"][k] == t->second) {
            draft_["tiles"][k] = baseTile;
            arr("tileVariants")[k] = baseVariant;
            if (draft_.contains("tileRots") && k < static_cast<int>(draft_["tileRots"].size())) draft_["tileRots"][k] = 0;
          }
        }
        for (const e2::FootOffset& f : after) {
          const int cx = p.value("x", 0) + f.dx, cy = p.value("y", 0) + f.dy;
          if (cx >= 0 && cy >= 0 && cx < cols() && cy < rows()) draft_["tiles"][cy * cols() + cx] = t->second;
        }
      }
      const int rot = turned["rot"].get<int>();
      note(std::format("{} em {},{}: girada para {}°{}.", def->name, p.value("x", 0), p.value("y", 0), rot * (def->model3d.empty() ? 60 : 90),
                       rot == 0 ? " (de volta ao original)" : ""));
      p = std::move(turned);
      return;
    }
    note("Não há decoração nessa casa pra girar.");
  }

  void removeSelectedDecoration() {
    if (!selected_) return note("Clique em qualquer hex da decoração e então pressione Delete.");
    e2::Json& decos = arr("decorations");
    for (size_t k = 0; k < decos.size(); ++k) {
      const e2::Json& p = decos[k];
      if (p.value("id", "") != selected_->value("id", "") || p.value("x", 0) != selected_->value("x", 0) || p.value("y", 0) != selected_->value("y", 0)) continue;
      beginEdit();
      const e2::CatalogDecoration* def = catalog_.decoration(p.value("id", ""));
      if (const auto t = editor_.decoTile.find(p.value("id", "")); t != editor_.decoTile.end()) {
        const auto [baseTile, baseVariant] = baseOf();
        for (const e2::FootOffset& f : footprintOf(p)) {
          const int cx = p.value("x", 0) + f.dx, cy = p.value("y", 0) + f.dy;
          if (cx < 0 || cy < 0 || cx >= cols() || cy >= rows()) continue;
          const int i = cy * cols() + cx;
          if (draft_["tiles"][i] == t->second) {
            draft_["tiles"][i] = baseTile;
            arr("tileVariants")[i] = baseVariant;
            if (draft_.contains("tileRots") && i < static_cast<int>(draft_["tileRots"].size())) draft_["tileRots"][i] = 0;
          }
        }
      }
      note(std::format("{} removida.", def ? def->name : p.value("id", "")));
      decos.erase(k);
      selected_.reset();
      return;
    }
    selected_.reset();
    note("Essa decoração já não está no mapa.");
  }

  // toggleSpawn: the side's brush lifts whoever stands on the hex on that side, else drops a new unit there.
  void toggleSpawn(int x, int y) {
    const char* key = mode_ == Mode::Enemy ? "enemySpawns"
                      : mode_ == Mode::Npc ? "neutralSpawns"
                      : mode_ == Mode::Summon ? (summonNeutral_ ? "neutralSpawns" : "playerSpawns")
                                              : "playerSpawns";
    e2::Json& list = arr(key);
    beginEdit();
    for (size_t k = 0; k < list.size(); ++k)
      if (list[k].value("x", -1) == x && list[k].value("y", -1) == y) {
        list.erase(k);
        return;
      }
    int summons = 0;
    for (const e2::Json& s : list)
      if (isSummon(s.value("classId", ""))) ++summons;
    const int plain = static_cast<int>(list.size()) - summons;
    e2::Json spawn;
    if (mode_ == Mode::Summon) {
      spawn = {{"name", std::format("{} {}", className(summonBrush_), summons + 1)}, {"classId", summonBrush_}, {"x", x}, {"y", y},
               {"level", std::string(key) == "playerSpawns" ? editor_.testLevel : editor_.enemyLevel0}};
    } else if (mode_ == Mode::Player) {
      spawn = {{"name", std::format("Herói {}", plain + 1)}, {"classId", "swordsman"}, {"x", x}, {"y", y}, {"level", editor_.testLevel}};
    } else if (mode_ == Mode::Npc) {
      if (npcBrush_ == "breadLady" || !editor_.npcSpawn.contains(npcBrush_)) {
        spawn = {{"name", std::format("Civil {}", plain + 1)}, {"classId", "breadLady"}, {"x", x}, {"y", y}, {"level", editor_.enemyLevel0}};
      } else {
        spawn = editor_.npcSpawn.at(npcBrush_);
        spawn["x"] = x, spawn["y"] = y, spawn["level"] = editor_.enemyLevel0;
      }
    } else {
      spawn = {{"name", std::format("Inimigo {}", plain + 1)}, {"classId", "miliciaV2"}, {"x", x}, {"y", y}, {"level", editor_.enemyLevel0}};
    }
    list.push_back(std::move(spawn));
  }

  bool isSummon(const std::string& classId) const {
    const auto it = editor_.classes.find(classId);
    return it != editor_.classes.end() && it->second.summon;
  }
  std::string className(const std::string& classId) const {
    const auto it = editor_.classes.find(classId);
    return it == editor_.classes.end() ? classId : it->second.name;
  }

  // addHeroes: each hero not on the board yet, on the first free passable hexes from the top-left.
  void addHeroes(const std::vector<std::pair<std::string, std::string>>& heroes) {
    e2::Json& list = arr("playerSpawns");
    std::set<std::pair<int, int>> taken;
    for (const char* key : {"playerSpawns", "enemySpawns", "neutralSpawns"})
      for (const e2::Json& s : arr(key)) taken.insert({s.value("x", 0), s.value("y", 0)});
    beginEdit();
    int added = 0;
    for (const auto& [name, classId] : heroes) {
      if (std::any_of(list.begin(), list.end(), [&](const e2::Json& s) { return s.value("name", "") == name; })) continue;
      bool placed = false;
      for (int yy = 0; yy < rows() && !placed; ++yy)
        for (int xx = 0; xx < cols() && !placed; ++xx) {
          const std::string t = draft_["tiles"][yy * cols() + xx].get<std::string>();
          if (taken.contains({xx, yy}) || t == "void" || t == "water" || t == "column" || t == "barricade") continue;
          list.push_back({{"name", name}, {"classId", classId}, {"x", xx}, {"y", yy}, {"level", editor_.testLevel}});
          taken.insert({xx, yy});
          placed = true, ++added;
        }
    }
    note(added ? std::format("{} herói(s) adicionado(s).", added) : "Já estão todos no mapa.");
  }

  // resize: the board grows or shrinks from the bottom-right; new cells take the base; props and units off the board go.
  void resize(int c, int r) {
    c = std::clamp(c, editor_.minGrid, editor_.maxGrid), r = std::clamp(r, editor_.minGrid, editor_.maxGrid);
    if (c == cols() && r == rows()) return;
    beginEdit();
    const auto [baseTile, baseVariant] = baseOf();
    const int oc = cols(), orows = rows();
    auto get = [&](const char* key, int i) -> e2::Json { return draft_.contains(key) && draft_[key].is_array() && i < static_cast<int>(draft_[key].size()) ? draft_[key][i] : e2::Json(); };
    e2::Json tiles = e2::Json::array(), variants = e2::Json::array(), rots = e2::Json::array(), elev = e2::Json::array();
    for (int y = 0; y < r; ++y)
      for (int x = 0; x < c; ++x) {
        const bool in = y < orows && x < oc;
        const int i = y * oc + x;
        tiles.push_back(in ? get("tiles", i) : e2::Json(baseTile));
        variants.push_back(in && get("tileVariants", i).is_number() ? get("tileVariants", i) : e2::Json(baseVariant));
        rots.push_back(in && get("tileRots", i).is_number() ? get("tileRots", i) : e2::Json(0));
        const auto info = editor_.terrainInfo.find(in ? get("tiles", i).get<std::string>() : baseTile);
        elev.push_back(in && get("terrainElevations", i).is_number() ? get("terrainElevations", i)
                                                                         : e2::Json(in && info != editor_.terrainInfo.end() ? info->second.height : 0.0));
      }
    draft_["cols"] = c, draft_["rows"] = r;
    draft_["tiles"] = tiles, draft_["tileVariants"] = variants, draft_["tileRots"] = rots, draft_["terrainElevations"] = elev;
    e2::Json decos = e2::Json::array();
    for (const e2::Json& p : arr("decorations")) {
      const e2::CatalogDecoration* def = catalog_.decoration(p.value("id", ""));
      if (!def) continue;
      bool inside = true;
      for (const e2::FootOffset& f : def->footprint) {
        const int x = p.value("x", 0) + f.dx, y = p.value("y", 0) + f.dy;
        if (x < 0 || y < 0 || x >= c || y >= r) inside = false;
      }
      if (inside) decos.push_back(p);
    }
    draft_["decorations"] = decos;
    for (const char* key : {"playerSpawns", "enemySpawns", "neutralSpawns"}) {
      e2::Json kept = e2::Json::array();
      for (const e2::Json& s : arr(key))
        if (s.value("x", 0) < c && s.value("y", 0) < r) kept.push_back(s);
      draft_[key] = kept;
    }
  }

  // --- the preview --------------------------------------------------------------------------------------------------

  void rebuildPreview() {
    dirty_ = false;
    if (!data_) return;
    std::optional<e2::MapDraft> draft = e2::MapDraft::fromJson(nlohmann::json::parse(draft_.dump()));
    if (!draft) return note("O mapa não tem tabuleiro.");
    e2::Board board(*data_, *draft);
    board.loadRelief(base_.engine2Public);
    std::map<std::string, int> skipped;
    e2::SceneData scene = e2::buildScene(board, catalog_, &skipped);
    MapStageOptions options = base_;
    options.map.clear(), options.scene.clear(), options.battle.clear();
    options.draftData = std::move(*draft);
    options.sceneData = std::move(scene);
    options.fog = 0;  // the editor shows the whole board
    options.spellsMenu = false;
    options.editorClick = [this](int x, int y) { pendingClick_ = std::make_pair(x, y); };
    std::optional<MapStage::View> view;
    if (preview_) view = preview_->view();
    vkDeviceWaitIdle(services_.context.device);
    preview_.reset();
    preview_ = std::make_unique<MapStage>(services_.context, services_.renderer, services_.assets, services_.jobs, options);
    if (preview_->failed()) {
      preview_.reset();
      return note("A prévia não abriu (veja o log).");
    }
    if (view) preview_->setView(*view);
  }

  // --- saving and opening ----------------------------------------------------------------------------------------------

  static std::filesystem::path savedDir() { return "content/maps/ember"; }

  // The highest serial of `<id>NNN.json` in a folder (map-save-plugin nextSerial).
  static int highestSerial(const std::filesystem::path& dir, const std::string& id) {
    int best = 0;
    std::error_code ec;
    if (!std::filesystem::is_directory(dir, ec)) return 0;
    for (const auto& e : std::filesystem::directory_iterator(dir, ec)) {
      const std::string name = e.path().filename().string();
      if (name.size() != id.size() + 8 || !name.starts_with(id) || !name.ends_with(".json")) continue;
      const std::string digits = name.substr(id.size(), 3);
      if (std::all_of(digits.begin(), digits.end(), ::isdigit)) best = std::max(best, std::stoi(digits));
    }
    return best;
  }
  // normalizeScenarioId: lowercase ASCII letters / digits / dashes, at most 64.
  static std::string scenarioId(std::string id) {
    std::string out;
    for (unsigned char c : id) {
      if (std::isalnum(c)) out += static_cast<char>(std::tolower(c));
      else if (out.empty() || out.back() != '-') out += '-';
    }
    while (!out.empty() && out.front() == '-') out.erase(out.begin());
    while (!out.empty() && out.back() == '-') out.pop_back();
    if (out.size() > 64) out.resize(64);
    return out.empty() ? "scenario" : out;
  }

  // Salvar: Engine2's map file ({serial, savedAt, draft}, 2-space JSON) as the next serial of the id, counted over
  // Engine2's maps and EmberEngine's own; Engine2's folder is never written.
  void save() {
    draft_["id"] = scenarioId(draft_.value("id", "scenario"));
    const std::string id = draft_["id"].get<std::string>();
    const int serial = std::max(highestSerial(savedDir(), id), highestSerial(paths_.maps, id)) + 1;
    const std::filesystem::path file = savedDir() / std::format("{}{:03}.json", id, serial);
    std::error_code ec;
    std::filesystem::create_directories(savedDir(), ec);
    e2::Json out;
    out["serial"] = serial;
    out["savedAt"] = static_cast<long long>(e2::Campaign::nowMs());
    out["draft"] = draft_;
    const std::filesystem::path tmp = file.string() + ".tmp";
    {
      std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
      f << out.dump(2) << "\n";
      if (!f) return note("Não foi possível gravar " + file.string() + ".");
    }
    std::filesystem::rename(tmp, file, ec);
    if (ec) return note("Não foi possível gravar " + file.string() + ".");
    // Read back and compared, as Engine2's plugin does.
    std::ifstream back(file, std::ios::binary);
    const e2::Json saved = e2::Json::parse(back, nullptr, false);
    if (saved.is_discarded() || saved.value("serial", 0) != serial || saved["draft"] != draft_) return note("O arquivo gravado não confere: " + file.string());
    note(std::format("Salvo: {} (versão {}).", file.generic_string(), serial));
  }

  // Every map that can be opened: the campaign's newest (Engine2), and every map saved here.
  struct OpenEntry {
    std::string label;
    std::filesystem::path file;
  };
  std::vector<OpenEntry> savedMaps() const {
    std::vector<OpenEntry> out;
    std::error_code ec;
    if (std::filesystem::is_directory(savedDir(), ec))
      for (const auto& e : std::filesystem::directory_iterator(savedDir(), ec))
        if (e.path().extension() == ".json") out.push_back({e.path().stem().string(), e.path()});
    std::sort(out.begin(), out.end(), [](const OpenEntry& a, const OpenEntry& b) { return a.label < b.label; });
    return out;
  }
  void open(const std::filesystem::path& file) {
    std::ifstream in(file, std::ios::binary);
    const e2::Json j = in ? e2::Json::parse(in, nullptr, false) : e2::Json();
    if (j.is_discarded() || !j.is_object()) return note("Não consegui ler " + file.string() + ".");
    beginEdit();
    draft_ = j.contains("draft") ? j["draft"] : j;
    selected_.reset();
    note(std::format("Aberto {}.", file.filename().string()));
  }

  // Testar (MapEditorScreen onPlaytest -> GameApp startBattle with the draft as an override, test mode): the draft is
  // written as a map file, Engine2's own exporters add the missing heroes (make_test_map.ts, keeping the authored levels)
  // and build its scene and rules (export_scene.ts + measure_art.py, export_battle.ts with each spawn's own level), then
  // the battle opens. Runs off the main thread (a few seconds).
  void playtest() {
    if (playtest_.valid()) return;
    const std::string id = scenarioId(draft_.value("id", "scenario"));
    const std::filesystem::path dir = std::filesystem::path("build/editor/playtest");
    e2::Json file;
    file["serial"] = 0;
    file["savedAt"] = static_cast<long long>(e2::Campaign::nowMs());
    file["draft"] = draft_;
    file["draft"]["id"] = id;
    const std::string engine2 = base_.engine2Public.parent_path().generic_string();
    const std::string title = draft_.value("title", id), place = draft_.value("place", ""), briefing = draft_.value("briefing", ""),
                      objective = draft_.value("objective", "");
    note("Preparando o teste…");
    playtest_ = std::async(std::launch::async, [this, dir, id, file, engine2, title, place, briefing, objective]() -> std::optional<MissionEntry> {
      std::error_code ec;
      std::filesystem::create_directories(dir, ec);
      const std::filesystem::path source = dir / (id + ".draft.json"), map = dir / (id + ".json"), scene = dir / (id + ".scene.json"),
                                  battle = dir / (id + ".battle.json"), log = dir / (id + ".log");
      {
        std::ofstream f(source, std::ios::binary | std::ios::trunc);
        f << file.dump(2) << "\n";
      }
      const std::string vn = std::format("node \"{}/node_modules/vite-node/vite-node.mjs\" --root \"{}\"", engine2, (dir / "cache").generic_string());
      const std::string steps[4] = {
          std::format("set KEEP_LEVELS=1&& {} tools/scripts/engine2/make_test_map.ts -- \"{}\" \"{}\"", vn, source.generic_string(), map.generic_string()),
          std::format("{} tools/scripts/engine2/export_scene.ts -- \"{}\" \"{}\"", vn, map.generic_string(), scene.generic_string()),
          std::format("python -I tools/scripts/engine2/measure_art.py \"{}\" \"{}/public\"", scene.generic_string(), engine2),
          std::format("set TEST_LEVEL=15&& set LEVELS_FROM_DRAFT=1&& set NO_REFS=1&& {} tools/scripts/engine2/export_battle.ts -- \"{}\" \"{}\"", vn,
                      map.generic_string(), battle.generic_string()),
      };
      for (const std::string& step : steps) {
        // std::system runs cmd /c; the line starts unquoted, so its quotes are kept as written.
        const std::string command = std::format("{} >> \"{}\" 2>&1", step, log.generic_string());
        if (std::system(command.c_str()) != 0) {
          playtestError_ = "O teste não pôde ser preparado (veja " + log.generic_string() + ").";
          return std::nullopt;
        }
      }
      MissionEntry e;
      e.id = id, e.title = title, e.place = place, e.briefing = briefing, e.objective = objective, e.draftId = id;
      e.map = map, e.scene = scene, e.battle = battle;
      return e;
    });
  }

  // --- the panel (MapEditorScreen's page, left of the board) ---------------------------------------------------------------

  enum class Mode { Paint, Elevation, Decoration, Player, Enemy, Npc, Summon };

  void drawEditor() {
    const ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSizeConstraints({360, io.DisplaySize.y}, {io.DisplaySize.x * 0.6f, io.DisplaySize.y});
    ImGui::SetNextWindowSize({panelWidth_, io.DisplaySize.y}, ImGuiCond_FirstUseEver);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(17, 17, 17, 242));
    ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(74, 74, 74, 255));
    ImGui::Begin("##mapeditor", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings);
    panelWidth_ = ImGui::GetWindowWidth();
    ImGui::PushFont(uiFont(UiFont::Ui), 15.0f);
    drawHeader();
    drawToolbar();
    if (!note_.empty()) {
      ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(224, 200, 150, 255));
      ImGui::TextWrapped("%s", note_.c_str());
      ImGui::PopStyleColor();
    }
    ImGui::Separator();
    if (ImGui::CollapsingHeader("Mapa", ImGuiTreeNodeFlags_DefaultOpen)) drawSettings();
    if (ImGui::CollapsingHeader("Iluminação")) drawLighting();
    drawSizeAndModes();
    drawModePanel();
    if (ImGui::CollapsingHeader("Unidades", ImGuiTreeNodeFlags_DefaultOpen)) drawUnits();
    ImGui::PopFont();
    ImGui::End();
    ImGui::PopStyleColor(2);
  }

  void drawHeader() {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 at = ImGui::GetCursorScreenPos();
    if (ImGui::InvisibleButton("##edback", {40, 40}) && callbacks_.back) callbacks_.back(draft_);
    dl->AddRect(at, {at.x + 40, at.y + 40}, ImGui::IsItemHovered() ? IM_COL32(216, 216, 216, 255) : IM_COL32(74, 74, 74, 255));
    iconChevronLeft(dl, {at.x + 20, at.y + 20}, 20, IM_COL32(240, 240, 240, 255));
    spacedText(dl, uiFont(UiFont::Ui), 13.0f, {at.x + 52, at.y}, IM_COL32(180, 180, 180, 255), "MODO TESTE", 0.18f, false);
    dl->AddText(uiFont(UiFont::Display), 24.0f, {at.x + 52, at.y + 16}, IM_COL32(240, 240, 240, 255), "Map Editor");
    ImGui::Dummy({1, 4});
  }

  void drawToolbar() {
    if (ImGui::Button("Novo")) {
      beginEdit();
      draft_ = blankDraft();
      selected_.reset();
      note(std::format("Mapa novo em branco — cenário \"{}\", {}x{}.", draft_["id"].get<std::string>(), cols(), rows()));
    }
    ImGui::SameLine();
    if (ImGui::Button("Salvar")) save();
    ImGui::SameLine();
    ImGui::BeginDisabled(playtest_.valid());
    if (ImGui::Button(playtest_.valid() ? "Preparando…" : "Testar")) playtest();
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(past_.empty());
    if (ImGui::Button("Desfazer")) undo();
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(future_.empty());
    if (ImGui::Button("Refazer")) redo();
    ImGui::EndDisabled();
    ImGui::SetNextItemWidth(-1);
    if (ImGui::BeginCombo("##campaignmap", "Carregar mapa da campanha…")) {
      for (const e2::CampaignMission& m : campaign_.data().missions) {
        if (m.mapFile.empty()) continue;
        if (ImGui::Selectable(std::format("{}##{}", m.title, m.id).c_str())) {
          // the newest file of the mission's id: EmberEngine's own save when it is newer than Engine2's
          const std::string stem = std::filesystem::path(m.mapFile).stem().string();
          const int own = highestSerial(savedDir(), m.id);
          const int theirs = highestSerial(paths_.maps, m.id);
          open(own > theirs ? savedDir() / std::format("{}{:03}.json", m.id, own) : paths_.maps / m.mapFile);
          (void)stem;
        }
      }
      ImGui::EndCombo();
    }
    const std::vector<OpenEntry> saved = savedMaps();
    if (!saved.empty()) {
      ImGui::SetNextItemWidth(-1);
      if (ImGui::BeginCombo("##savedmap", "Abrir mapa salvo…")) {
        for (const OpenEntry& e : saved)
          if (ImGui::Selectable(e.label.c_str())) open(e.file);
        ImGui::EndCombo();
      }
    }
  }

  // A text field bound to a draft string.
  void textField(const char* label, const char* key, bool multiline = false) {
    std::string value = draft_.value(key, "");
    char buffer[4096];
    std::snprintf(buffer, sizeof(buffer), "%s", value.c_str());
    ImGui::TextColored(ImColor(180, 180, 180), "%s", label);
    ImGui::SetNextItemWidth(-1);
    const bool changed = multiline ? ImGui::InputTextMultiline(std::format("##{}", key).c_str(), buffer, sizeof(buffer), {-1, 64})
                                   : ImGui::InputText(std::format("##{}", key).c_str(), buffer, sizeof(buffer));
    if (changed) {
      beginEdit();
      draft_[key] = std::string(buffer);
    }
  }
  void checkbox(const char* label, const char* key) {
    bool v = draft_.contains(key) && draft_[key].is_boolean() && draft_[key].get<bool>();
    if (ImGui::Checkbox(label, &v)) beginEdit(), draft_[key] = v;
  }
  void slider(const char* label, const char* key, float lo, float hi, float fallback) {
    float v = draft_.contains(key) && draft_[key].is_number() ? draft_[key].get<float>() : fallback;
    ImGui::SetNextItemWidth(-110);
    if (ImGui::SliderFloat(std::format("{}##{}", label, key).c_str(), &v, lo, hi, "%.2f")) beginEdit(), draft_[key] = std::round(v * 100.0) / 100.0;
  }

  void drawSettings() {
    textField("Cenário alvo (Id)", "id");
    textField("Título", "title");
    textField("Local", "place");
    textField("Objetivo", "objective");
    textField("Briefing", "briefing", true);
    {
      static const char* kWin[3][2] = {{"rout", "Derrote todos (rout)"}, {"boss", "Derrube o chefe (boss)"}, {"escape", "Transversal — alcance a saída (escape)"}};
      const std::string win = draft_.value("win", "rout");
      const char* shown = kWin[0][1];
      for (auto& w : kWin)
        if (win == w[0]) shown = w[1];
      ImGui::TextColored(ImColor(180, 180, 180), "Vitória");
      ImGui::SetNextItemWidth(-1);
      if (ImGui::BeginCombo("##win", shown)) {
        for (auto& w : kWin)
          if (ImGui::Selectable(w[1], win == w[0])) beginEdit(), draft_["win"] = w[0];
        ImGui::EndCombo();
      }
    }
    checkbox("Névoa de guerra", "fog");
    checkbox("Terreno automático", "autoTactics");
    checkbox("É um hub (sem combate)", "hub");
    checkbox("Exploração livre (sem turnos)", "explore");
    checkbox("Preservar posições iniciais (ignorar formação do grupo)", "lockPartyFormation");
  }

  void drawLighting() {
    // Imagem de fundo
    {
      const std::string current = draft_.contains("backgroundImage") && draft_["backgroundImage"].is_string() ? draft_["backgroundImage"].get<std::string>() : "";
      std::string shown = "Vazio preto";
      for (const auto& [id, label, file] : editor_.backgrounds)
        if (id == current) shown = label;
      ImGui::SetNextItemWidth(-110);
      if (ImGui::BeginCombo("Imagem de fundo", shown.c_str())) {
        if (ImGui::Selectable("Vazio preto", current.empty())) beginEdit(), draft_["backgroundImage"] = "";
        for (const auto& [id, label, file] : editor_.backgrounds)
          if (ImGui::Selectable(std::format("{}##{}", label, id).c_str(), id == current)) beginEdit(), draft_["backgroundImage"] = id;
        ImGui::EndCombo();
      }
    }
    {
      const bool indoor = draft_.value("environment", "outdoor") == "indoor";
      ImGui::SetNextItemWidth(-110);
      if (ImGui::BeginCombo("Ambiente", indoor ? "Interno (luz suave, sem sol direto)" : "Externo (sol forte, sombras marcadas)")) {
        if (ImGui::Selectable("Externo (sol forte, sombras marcadas)", !indoor)) beginEdit(), draft_["environment"] = "outdoor";
        if (ImGui::Selectable("Interno (luz suave, sem sol direto)", indoor)) beginEdit(), draft_["environment"] = "indoor";
        ImGui::EndCombo();
      }
    }
    bool moon = false;
    {
      const std::string tod = draft_.value("timeOfDay", "day");
      std::string shown = tod;
      for (const auto& t : editor_.timeOfDay)
        if (t.id == tod) shown = t.label, moon = t.moon;
      ImGui::SetNextItemWidth(-110);
      if (ImGui::BeginCombo("Hora do dia", shown.c_str())) {
        for (const auto& t : editor_.timeOfDay)
          if (ImGui::Selectable(t.label.c_str(), t.id == tod)) beginEdit(), draft_["timeOfDay"] = t.id, draft_["sunIntensity"] = t.key, draft_["ambientIntensity"] = t.ambient;
        ImGui::EndCombo();
      }
    }
    slider(moon ? "Intensidade da lua" : "Intensidade do sol", "sunIntensity", 0, 6, static_cast<float>(editor_.defaultSun));
    slider("Luz ambiente", "ambientIntensity", 0, 4, static_cast<float>(editor_.defaultAmbient));
    slider("Brilho (bloom)", "bloomIntensity", 0, 3, static_cast<float>(editor_.defaultBloom));
    {
      static const char* kMist[10][2] = {{"none", "Sem névoa"}, {"mist2", "Névoa 2 (textura suave, mundo inteiro)"}, {"mist3", "Névoa 3 (ruído original, mundo inteiro)"},
                                         {"mist4", "Névoa 4 (vórtice nas bordas do mapa, centro sempre limpo)"},
                                         {"fog1", "Névoa 01 (só sobre área não revelada e o fundo)"}, {"fog5", "Névoa 5 (rasteira, fiapos finos e véus)"},
                                         {"vignette", "Vinheta (tela inteira, bordas suaves)"}, {"vignette2", "Vinheta 2 (bancos de névoa profundos, centro limpo)"},
                                         {"vignette3", "Vinheta 3 (névoa rasteira em faixas)"}, {"vignette4", "Vinheta 4 (névoa monocromática nas bordas)"}};
      const std::string mist = draft_.value("mistType", "none");
      const char* shown = kMist[0][1];
      for (auto& m : kMist)
        if (mist == m[0]) shown = m[1];
      ImGui::SetNextItemWidth(-110);
      if (ImGui::BeginCombo("Tipo de névoa", shown)) {
        for (auto& m : kMist)
          if (ImGui::Selectable(m[1], mist == m[0])) beginEdit(), draft_["mistType"] = m[0];
        ImGui::EndCombo();
      }
    }
    slider("Névoa", "mistIntensity", 0, 1, 0);
    slider("Vel. da névoa", "mistSpeed", 0.1f, 4, 1);
    slider("Wisps", "wispIntensity", 0, 1, 0);
    slider("Velocidade", "wispSpeed", 0.1f, 4, 1);
    {
      const unsigned c = draft_.contains("wispColor") && draft_["wispColor"].is_number() ? draft_["wispColor"].get<unsigned>() : 0xffa552u;
      float rgb[3] = {((c >> 16) & 255) / 255.0f, ((c >> 8) & 255) / 255.0f, (c & 255) / 255.0f};
      if (ImGui::ColorEdit3("Cor dos wisps", rgb, ImGuiColorEditFlags_NoInputs)) {
        beginEdit();
        draft_["wispColor"] = (static_cast<unsigned>(rgb[0] * 255 + 0.5f) << 16) | (static_cast<unsigned>(rgb[1] * 255 + 0.5f) << 8) | static_cast<unsigned>(rgb[2] * 255 + 0.5f);
      }
    }
    ImGui::TextDisabled("A vista 3D mostra estes ajustes na prévia; Testar também usa a iluminação salva no mapa.");
  }

  void drawSizeAndModes() {
    int c = cols(), r = rows();
    ImGui::SetNextItemWidth(90);
    if (ImGui::InputInt("Col", &c, 1, 5, ImGuiInputTextFlags_EnterReturnsTrue)) resize(c, rows());
    ImGui::SameLine();
    ImGui::SetNextItemWidth(90);
    if (ImGui::InputInt("Lin", &r, 1, 5, ImGuiInputTextFlags_EnterReturnsTrue)) resize(cols(), r);
    static const std::pair<Mode, const char*> kModes[] = {{Mode::Paint, "Terreno"},   {Mode::Elevation, "Elevação"}, {Mode::Decoration, "Decoração"},
                                                          {Mode::Player, "Herói"},    {Mode::Enemy, "Inimigo"},      {Mode::Npc, "NPC"},
                                                          {Mode::Summon, "Invocação"}};
    for (size_t k = 0; k < std::size(kModes); ++k) {
      if (k) ImGui::SameLine();
      const bool on = mode_ == kModes[k].first;
      if (on) ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(216, 216, 216, 255)), ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(17, 17, 17, 255));
      if (ImGui::SmallButton(kModes[k].second)) mode_ = kModes[k].first, turning_ = turningDeco_ = false;
      if (on) ImGui::PopStyleColor(2);
      if (ImGui::GetContentRegionAvail().x < 90 && k + 1 < std::size(kModes)) ImGui::NewLine();
    }
    if (ImGui::SmallButton("Party completa")) addHeroes(editor_.heroes);
    for (const auto& [name, classId] : editor_.heroes) {
      ImGui::SameLine();
      if (ImGui::SmallButton(name.c_str())) addHeroes({{name, classId}});
    }
  }

  // A small image button for a thumbnail (an Engine2 public file); the swatch when there is none.
  bool thumbButton(const char* id, const std::string& thumb, const std::string& swatch, float size, bool selected, const std::string& tip) {
    const ImVec2 at = ImGui::GetCursorScreenPos();
    const bool clicked = ImGui::InvisibleButton(id, {size, size});
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const UiImage* image = thumb.empty() ? nullptr : uiImage(base_.engine2Public / thumb.substr(thumb.starts_with('/') ? 1 : 0));
    if (image) {
      // fitted inside the square, its aspect kept
      const float aspect = static_cast<float>(image->width) / std::max(1.0f, static_cast<float>(image->height));
      const float w = aspect >= 1 ? size : size * aspect, h = aspect >= 1 ? size / aspect : size;
      dl->AddRectFilled(at, {at.x + size, at.y + size}, IM_COL32(28, 28, 28, 255));
      dl->AddImage(image->texture, {at.x + (size - w) / 2, at.y + (size - h) / 2}, {at.x + (size + w) / 2, at.y + (size + h) / 2});
    } else {
      unsigned c = 0x555555;
      if (swatch.size() == 7) c = static_cast<unsigned>(std::strtoul(swatch.c_str() + 1, nullptr, 16));
      dl->AddRectFilled(at, {at.x + size, at.y + size}, IM_COL32((c >> 16) & 255, (c >> 8) & 255, c & 255, 255));
    }
    dl->AddRect(at, {at.x + size, at.y + size}, selected ? IM_COL32(216, 216, 216, 255) : IM_COL32(74, 74, 74, 255), 0.0f, selected ? 2.0f : 1.0f);
    if (ImGui::IsItemHovered() && !tip.empty()) ImGui::SetTooltip("%s", tip.c_str());
    return clicked;
  }

  void drawModePanel() {
    if (mode_ == Mode::Paint) {
      ImGui::TextDisabled("Escolha um terreno ou grupo de tiles para pintar o mapa.");
      if (ImGui::Button(turning_ ? "Girando — clique num hex" : "Girar hex")) {
        turning_ = !turning_;
        note(turning_ ? "Girar armado: cada clique num hex vira o desenho 60°, sem trocar o terreno." : "Pincel de volta: clicar pinta o terreno.");
      }
      // the terrains (City is plains with the city layers), by name
      std::vector<std::tuple<std::string, std::string, std::string, std::string, std::string>> items;  // key, terrain, label, thumb, swatch
      for (const TerrainEntry& t : editor_.terrains)
        if (t.id != "snow") items.emplace_back(t.id, t.id, t.name, t.thumb, t.swatch);
      if (!editor_.city.empty()) items.emplace_back("city", "plains", "City", editor_.city[0].thumb, "#9c8f6f");
      items.emplace_back("snow", "snow", "Neve e gelo", "", "#d8dee2");
      for (const TerrainEntry& t : editor_.terrains)
        if (t.id == "snow") std::get<3>(items.back()) = t.thumb;
      int n = 0;
      for (const auto& [key, terrain, label, thumb, swatch] : items) {
        const bool selected = key == "city" ? cityMode_ && brush_ == "plains" : !cityMode_ && brush_ == terrain;
        if (n % 4) ImGui::SameLine();
        ImGui::BeginGroup();
        std::string hint = label;
        for (const TerrainEntry& t : editor_.terrains)
          if (t.id == terrain && key != "city") hint = t.hint;
        if (thumbButton(("##t" + key).c_str(), thumb, swatch, 40, selected, hint)) {
          brush_ = terrain;
          if (key == "city") {
            cityMode_ = true;
            if (std::none_of(editor_.city.begin(), editor_.city.end(), [&](const Version& v) { return v.variant == variant_; })) variant_ = editor_.city[0].variant;
          } else {
            cityMode_ = false;
            variant_ = std::min(variant_, variantCount(terrain) - 1);
            if (terrain == "plains" && std::any_of(editor_.city.begin(), editor_.city.end(), [&](const Version& v) { return v.variant == variant_; })) variant_ = 0;
          }
        }
        ImGui::SameLine();
        ImGui::TextUnformatted(label.c_str());
        ImGui::EndGroup();
        ++n;
      }
      // Versões (City's own strip in City mode)
      const std::vector<Version>* versions = nullptr;
      std::string swatch = "#555555";
      if (cityMode_) versions = &editor_.city;
      else
        for (const TerrainEntry& t : editor_.terrains)
          if (t.id == brush_ && brush_ != "void") versions = &t.versions, swatch = t.swatch;
      if (versions) {
        ImGui::TextColored(ImColor(180, 180, 180), "VERSÕES");
        int k = 0;
        for (const Version& v : *versions) {
          if (k % 6) ImGui::SameLine();
          if (thumbButton(std::format("##v{}", v.variant).c_str(), v.thumb, brush_ == "plains" ? "#5f8c28" : swatch, 44, variant_ == v.variant,
                          std::format("{} {}", terrainName(brush_), v.label)))
            variant_ = v.variant;
          ++k;
        }
      }
      if (brush_ == "hill") ImGui::TextDisabled("Colina cria relevo. Use Elevação para esculpir vários níveis e ver o resultado na prévia 3D.");
      if (ImGui::Button(std::format("Substituir base · {}", terrainName(brush_)).c_str())) {
        beginEdit();
        const int v = std::min(variant_, variantCount(brush_) - 1), n2 = cols() * rows();
        draft_["baseTile"] = brush_, draft_["baseVariant"] = v;
        draft_["tiles"] = e2::Json(std::vector<std::string>(n2, brush_));
        draft_["tileVariants"] = e2::Json(std::vector<int>(n2, v));
        draft_["tileRots"] = e2::Json(std::vector<int>(n2, 0));
        note(std::format("Base inteira substituída por {}.", terrainName(brush_)));
      }
    } else if (mode_ == Mode::Elevation) {
      static const char* kTools[3] = {"Subir", "Descer", "Nivelar"};
      for (int k = 0; k < 3; ++k) {
        if (k) ImGui::SameLine();
        if (ImGui::RadioButton(kTools[k], elevationTool_ == k)) elevationTool_ = k;
      }
      if (elevationTool_ == 2) ImGui::SliderInt("Nível", &elevationLevel_, 0, 12);
      ImGui::SliderInt("Anéis", &elevationRadius_, 0, 3, elevationRadius_ == 0 ? "uma célula" : "%d");
      ImGui::TextDisabled("Clique num hex: ele e os anéis em volta sobem, descem ou ficam no nível escolhido (0 a 12).");
    } else if (mode_ == Mode::Decoration) {
      ImGui::TextDisabled("Clique na casa âncora pra colocar; clique numa decoração para selecionar (Delete remove).");
      if (ImGui::Button(turningDeco_ ? "Girando — clique numa decoração" : "Girar objeto")) {
        turningDeco_ = !turningDeco_;
        note(turningDeco_ ? "Girar objeto armado: cada clique numa decoração vira ela 60°, com toda a área junto." : "Pincel de volta: clicar coloca e remove decoração.");
      }
      ImGui::SetNextItemWidth(170);
      if (ImGui::BeginCombo("##section", decoSection_.c_str())) {
        for (const std::string& s : editor_.decorationSections)
          if (ImGui::Selectable(s.c_str(), s == decoSection_)) decoSection_ = s;
        ImGui::EndCombo();
      }
      ImGui::SameLine();
      ImGui::SetNextItemWidth(-1);
      ImGui::InputTextWithHint("##decosearch", "Procurar…", search_, sizeof(search_));
      if (const e2::CatalogDecoration* d = catalog_.decoration(decoBrush_)) ImGui::Text("Pincel: %s", d->name.c_str());
      ImGui::BeginChild("##decos", {-1, 260}, ImGuiChildFlags_Borders);
      const std::string query = toLower(search_);
      int k = 0;
      const float per = std::max(1.0f, std::floor((ImGui::GetContentRegionAvail().x + 6) / 70));
      for (const std::string& id : editor_.decorationOrder) {
        const e2::CatalogDecoration* d = catalog_.decoration(id);
        if (!d) continue;
        if (decoSection_ != "Todas" && editor_.decoSection[id] != decoSection_) continue;
        if (!query.empty() && toLower(d->name).find(query) == std::string::npos && id.find(query) == std::string::npos) continue;
        if (k % static_cast<int>(per)) ImGui::SameLine();
        if (thumbButton(("##d" + id).c_str(), editor_.decoThumb[id], "#333333", 64, decoBrush_ == id, d->name)) decoBrush_ = id, turningDeco_ = false;
        ++k;
      }
      ImGui::EndChild();
      if (selected_) {
        const e2::CatalogDecoration* d = catalog_.decoration(selected_->value("id", ""));
        ImGui::Text("Selecionada: %s em %d,%d", d ? d->name.c_str() : "?", selected_->value("x", 0), selected_->value("y", 0));
        if (ImGui::Button("Remover (Delete)")) removeSelectedDecoration();
      }
    } else {
      if (mode_ == Mode::Player || mode_ == Mode::Enemy)
        ImGui::TextDisabled("Clique numa casa vazia pra adicionar %s; clique numa casa ocupada (do mesmo lado) pra remover. Edite nome/classe na lista abaixo.",
                            mode_ == Mode::Player ? "um herói" : "um inimigo");
      if (mode_ == Mode::Npc) {
        std::string shown = npcBrush_;
        for (const auto& [id, name] : editor_.npcs)
          if (id == npcBrush_) shown = name;
        ImGui::SetNextItemWidth(-1);
        if (ImGui::BeginCombo("##npc", shown.c_str())) {
          for (const auto& [id, name] : editor_.npcs)
            if (ImGui::Selectable(name.c_str(), id == npcBrush_)) npcBrush_ = id;
          ImGui::EndCombo();
        }
        ImGui::TextDisabled("Os novos personagens têm animação de quatro quadros e diálogo próprio.");
      }
      if (mode_ == Mode::Summon) {
        if (ImGui::RadioButton("Aliada", !summonNeutral_)) summonNeutral_ = false;
        ImGui::SameLine();
        if (ImGui::RadioButton("Neutra", summonNeutral_)) summonNeutral_ = true;
        ImGui::SetNextItemWidth(-1);
        if (ImGui::BeginCombo("##summon", std::format("{} · {}", className(summonBrush_), editor_.classes[summonBrush_].role).c_str())) {
          for (const std::string& c : editor_.classOrder) {
            if (!summonNeutral_ && !isSummon(c)) continue;
            if (ImGui::Selectable(std::format("{} · {}##{}", className(c), editor_.classes[c].role, c).c_str(), c == summonBrush_)) summonBrush_ = c;
          }
          ImGui::EndCombo();
        }
        ImGui::TextDisabled(summonNeutral_ ? "Neutras ficam paradas: não entram na ordem de turno e não contam pra limpar o mapa."
                                           : "Aliadas não contam na derrota — perder todas não perde a missão.");
      }
    }
  }

  static std::string toLower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
  }

  // The spawn lists (name, class, level per unit; × removes).
  void drawUnits() {
    static const std::pair<const char*, const char*> kLists[] = {{"playerSpawns", "Heróis e aliados"}, {"enemySpawns", "Inimigos"}, {"neutralSpawns", "Neutros"}};
    for (const auto& [key, label] : kLists) {
      e2::Json& list = arr(key);
      ImGui::TextColored(ImColor(180, 180, 180), "%s (%d)", label, static_cast<int>(list.size()));
      for (size_t k = 0; k < list.size(); ++k) {
        e2::Json& s = list[k];
        ImGui::PushID(std::format("{}{}", key, k).c_str());
        char name[128];
        std::snprintf(name, sizeof(name), "%s", s.value("name", "").c_str());
        ImGui::SetNextItemWidth(110);
        if (ImGui::InputText("##name", name, sizeof(name))) beginEdit(), s["name"] = std::string(name);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(150);
        const std::string classId = s.value("classId", "");
        if (ImGui::BeginCombo("##class", className(classId).c_str())) {
          for (const std::string& c : editor_.classOrder)
            if (ImGui::Selectable(std::format("{} · {}##{}", className(c), editor_.classes[c].role, c).c_str(), c == classId)) beginEdit(), s["classId"] = c;
          ImGui::EndCombo();
        }
        ImGui::SameLine();
        int level = s.value("level", 1);
        ImGui::SetNextItemWidth(60);
        if (ImGui::InputInt("##lv", &level, 0)) beginEdit(), s["level"] = std::clamp(level, 1, 30);
        ImGui::SameLine();
        ImGui::Text("%d,%d", s.value("x", 0), s.value("y", 0));
        ImGui::SameLine();
        const bool remove = ImGui::SmallButton("x");
        ImGui::PopID();
        if (remove) {
          beginEdit();
          list.erase(k);
          break;
        }
      }
    }
  }

  Services services_;
  MapStageOptions base_;
  CampaignPaths paths_;
  const e2::Campaign& campaign_;
  Callbacks callbacks_;
  std::optional<e2::BoardData> data_;
  e2::EditorCatalog catalog_;
  EditorTables editor_;
  e2::Json draft_;
  std::deque<e2::Json> past_, future_;
  std::chrono::steady_clock::time_point lastEdit_{};
  bool dirty_ = false;
  std::unique_ptr<MapStage> preview_;
  std::optional<std::pair<int, int>> pendingClick_;
  std::future<std::optional<MissionEntry>> playtest_;
  std::string playtestError_;
  std::string script_;  // captures: clicks to apply (previewEdits)
  std::string note_;
  float noteAt_ = 0;
  float panelWidth_ = 470;
  int frames_ = 0;
  // the brushes
  Mode mode_ = Mode::Paint;
  std::string brush_ = "plains";
  int variant_ = 0;
  bool cityMode_ = false, turning_ = false, turningDeco_ = false;
  int elevationTool_ = 0, elevationLevel_ = 0, elevationRadius_ = 0;
  std::string decoBrush_ = "dead-tree", decoSection_ = "Todas";
  char search_[64] = {};
  std::optional<e2::Json> selected_;
  std::string summonBrush_ = "familiar", npcBrush_ = "breadLady";
  bool summonNeutral_ = false;
};

}  // namespace ember
