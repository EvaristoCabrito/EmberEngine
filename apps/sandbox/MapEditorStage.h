#pragma once

// Engine2's Map Editor (Modo teste > Map Editor), copied from src/editor/ember/MapEditorScreen.tsx (+ MapPreview3D.tsx,
// DialogEditor.tsx): the map's settings, lighting and size on the left, the board in the game's own renderer on the
// right (MapStage with the draft in memory), rebuilt a moment after every edit. The draft is Engine2's MapDraft JSON,
// kept whole (every field it carries survives a save).
//
//   Novo / Locais / R-Encounter / Gerar terreno / Carregar mapa da campanha / Abrir mapa salvo / CityHubs
//   the mission: id, title, place, music, objective, briefing, reward (gold, rations, NPCs that must live), opening and
//               closing dialogues, victory, world-map location (and its planned slots), the switches; lighting
//   Terreno     paint a terrain with one of its versions (Versões, City, Icelands), turn a hex's art (Girar hex)
//   Elevação    raise / lower / level a hex and its rings
//   Água 3D     paint or remove water patches (version, shape, size, level, area)
//   Decoração   the props by section; a click places on the anchor hex or selects one there (Delete removes it),
//               Girar objeto turns it; the dice keep a prop out of Gerar terreno; the selected prop's rules
//               (Bloquear caminho, Alto terreno) and a floor connector's target (Passagem de andar)
//   3D Walls / 3D Decorations   regular / thick walls, doors (Abrir porta / Fechar passagem), horizontal / vertical
//   Herói / Inimigo / NPC / Invocação   add / remove units; the lists edit name, class, level, a neutral's dialogue
//   the preview: press a unit or a prop and drag it to another hex (Delete while holding a unit removes it); the
//               technical grid (hex or square) turns hexes and props
//   Desfazer / Refazer (10 steps), Substituir base, the saved files (Carregar / Ativar / apagar), Testar, Salvar.
//
// Saves go to content/maps/ember/<id><serial>.json (Engine2's format and serials; Engine2's folder untouched); Locais /
// R-Encounter to content/maps/locais.json and random-encounters.json (world/Locais.h); Ativar to content/maps/active.json.
// Not here (Engine2's own left them out or they have no meaning here): the elemental 2D FX, Exportar, the browser-local
// versions.

#include <SDL3/SDL.h>
#include <imgui.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <deque>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <future>
#include <map>
#include <memory>
#include <numbers>
#include <optional>
#include <random>
#include <regex>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "CampaignUi.h"
#include "DialogEditorUi.h"
#include "MapExport.h"
#include "MapStage.h"
#include "Stage.h"
#include "core/Log.h"
#include "render/Renderer.h"
#include "render/UiArt.h"
#include "rhi/VulkanContext.h"
#include "world/Battle.h"
#include "world/Board.h"
#include "world/Campaign.h"
#include "world/Locais.h"
#include "world/MapDressing.h"
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
    // Locais / R-Encounter were written: the campaign and the world map take them at once (Engine2: "aplicada à campanha").
    std::function<void()> locaisSaved;
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
    if (!catalog_.load(kCatalog)) note("Sem o catálogo do editor (content/engine2/editor-catalog.json).");
    editor_ = loadEditorTables(kCatalog);
    if (!dress_.load(kCatalog)) note("O catálogo do editor não tem as tabelas de Gerar terreno (export_editor_catalog.ts).");
    locais_ = e2::Locais::load(kCatalog);
    order_ = locais_.editorOrder();
    shuffleExclude_ = loadShuffleExclude();
    draft_ = draft ? *draft : blankDraft();
    // MapEditorScreen's mount: summons and civilians placed as enemies move to the neutral list; walls under a
    // watchtower entrance go.
    moveMisplacedEnemies();
    if (draft_.contains("decorations") && draft_["decorations"].is_array())
      draft_["decorations"] = e2::removeWallsUnderWatchtowerEntrances(draft_["decorations"], dress_, catalog_);
    dirty_ = true;
    scan_ = std::async(std::launch::async, [maps = paths_.maps] { return scanSaved(maps); });
  }
  ~MapEditorStage() override {
    if (playtest_.valid()) playtest_.wait();
    for (auto& f : exports_) f.wait();
    if (scan_.valid()) scan_.wait();
    vkDeviceWaitIdle(services_.context.device);
  }

  // The draft as it stands (main.cpp keeps it across a test battle).
  const e2::Json& draft() const { return draft_; }
  // Captures: clicks as the board would get them, "mode:arg:x,y;..." (paint:<terrain>, raise, lower, deco:<id>,
  // arch:<id>, water:<level>, turn, hero, enemy, npc, summon), "gen" (Gerar terreno), "tech" (the technical grid),
  // "locais" / "encounters" / "dialog:intro" (those windows), "save"; then the preview as it stands.
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
      if (step == "gen") {
        generateTerrain(0x5eed);
        continue;
      }
      if (step == "tech") {
        showTechnical_ = true;
        continue;
      }
      if (step == "locais") {
        openLocais();
        continue;
      }
      if (step == "encounters") {
        showEncounters_ = true;
        continue;
      }
      if (step.starts_with("drag:")) {  // drag:x,y>x,y: the preview's press on a unit / prop and its drop elsewhere
        int ax = 0, ay = 0, bx = 0, by = 0;
        if (std::sscanf(step.c_str() + 5, "%d,%d>%d,%d", &ax, &ay, &bx, &by) == 4 && grab(ax, ay, false)) release(std::pair{bx, by}, true);
        continue;
      }
      if (step.starts_with("scroll:")) {  // the page scrolled down this far (captures of its lower sections)
        scrollTo_ = static_cast<float>(std::atof(step.c_str() + 7));
        continue;
      }
      if (step.starts_with("mode:")) {  // a brush's panel opened: paint water deco arch props hero summon
        const std::string m = step.substr(5);
        mode_ = m == "water" ? Mode::Water : m == "deco" ? Mode::Decoration : m == "arch" || m == "props" ? Mode::Architecture : m == "hero" ? Mode::Player
                : m == "summon" ? Mode::Summon : m == "elev" ? Mode::Elevation : Mode::Paint;
        architectureDecorations_ = m == "props";
        if (m == "arch" || m == "props") decoBrush_ = m == "props" ? "prop-3d-tavern-table" : "wall-3d-stone";
        continue;
      }
      if (step.starts_with("dialog:")) {
        pendingDialog_ = step.substr(7);
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
      else if (mode == "arch") mode_ = Mode::Architecture, decoBrush_ = arg, architectureDecorations_ = meta(arg).props3d;
      else if (mode == "water") mode_ = Mode::Water, waterLevel_ = arg.empty() ? 0.5 : std::atof(arg.c_str());
      else if (mode == "turn") mode_ = Mode::Decoration, turningDeco_ = true;
      else if (mode == "hero") mode_ = Mode::Player;
      else if (mode == "enemy") mode_ = Mode::Enemy;
      else if (mode == "npc") mode_ = Mode::Npc;
      else if (mode == "summon") mode_ = Mode::Summon;
      lastEdit_ = {};
      onCellClick(x, y, std::nullopt);
    }
  }
  bool done() const override {  // captures: the preview and its art, a Testar finished
    return frames_ > 240 && !playtest_.valid() && std::none_of(exports_.begin(), exports_.end(), [](const std::future<std::string>& f) { return f.valid(); });
  }

  void frame(Renderer& renderer, VkExtent2D size) override {
    ++frames_;
    if (frames_ == 60 && !script_.empty()) runScript(std::exchange(script_, {}));
    // A click on the board (reported by the preview) is applied between frames.
    if (pendingClick_) {
      const auto [x, y] = *pendingClick_;
      pendingClick_.reset();
      onCellClick(x, y, std::exchange(pendingPoint_, std::nullopt));
    }
    if (pendingRelease_) {
      const auto [hex, moved] = *pendingRelease_;
      pendingRelease_.reset();
      release(hex, moved);
    }
    // The saved maps on disk (both folders), read in the background.
    if (scan_.valid() && scan_.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
      try {
        saved_ = scan_.get();
      } catch (const std::exception& e) {
        logError("map editor: reading the saved maps failed: {}", e.what());
      }
      refreshFiles();
      std::string hubs;
      for (const MapRef& m : cityHubs()) hubs += (hubs.empty() ? "" : ", ") + m.id;
      logInfo("map editor: {} saved maps; CityHubs: {}", saved_.size(), hubs);
    }
    // The preview follows the draft a moment after the last edit (Engine2 setPreviewMission).
    const auto now = std::chrono::steady_clock::now();
    if (dirty_ && showPreview_ && now - lastEdit_ > std::chrono::milliseconds(120)) rebuildPreview();
    // Testar finished preparing: the battle opens.
    if (playtest_.valid() && playtest_.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
      std::optional<MissionEntry> entry = playtest_.get();
      if (entry && callbacks_.play) callbacks_.play(*entry, draft_);
      else if (!entry) note(playtestError_);
    }
    for (auto it = exports_.begin(); it != exports_.end();)
      if (it->valid() && it->wait_for(std::chrono::seconds(0)) == std::future_status::ready) note(it->get()), it = exports_.erase(it);
      else ++it;
    // the rings: the hex under the mouse, the selected prop's hexes (dioramaView setHover / setSelection)
    hover_ = preview_ && showPreview_ ? preview_->hexUnderMouse() : std::nullopt;
    if (preview_) preview_->setEditorMarks(rings());
    if (preview_ && showPreview_) preview_->frame(renderer, size);
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
      if (e.key.key == SDLK_DELETE) {
        // MapPreviewCanvas: Delete while a unit is held removes it; otherwise the selected prop goes.
        if (heldUnit_) return deleteHeldUnit();
        return removeSelectedDecoration();
      }
      if (e.key.key == SDLK_ESCAPE) return;  // the preview's pause menu is not part of the editor
    }
    if (preview_ && showPreview_) preview_->event(e);
  }

  void drawPanel() override {
    if (preview_ && showPreview_) preview_->drawPanel();  // the camera bar
    drawEditor();
    if (showTechnical_) drawTechnicalMap();
    if (showLocations_) drawLocais();
    if (showEncounters_) drawEncounters();
    if (!pendingDialog_.empty()) openDialogEditor(std::exchange(pendingDialog_, {}));
    dialogUi_.draw(portraits(), editor_.affinityHeroes);
    drawHoverTip();
  }

 private:
  static constexpr const char* kCatalog = "content/engine2/editor-catalog.json";
  struct HeldUnit {
    std::string key;  // playerSpawns / enemySpawns / neutralSpawns
    size_t index = 0;
  };
  struct BigNote {
    bool ok = true;
    std::string title;
    std::vector<std::string> lines;
  };
  static constexpr const char* kEntrance = "watchtower-stone-open-door-2hex";

  // --- Engine2 tables (editor-catalog.json "editor") ------------------------------------------------------------------

  struct Version {
    int variant = 0;
    std::string label, thumb;
  };
  struct TerrainEntry {
    std::string id, name, swatch, hint, thumb;
    int variantCount = 1;
    double height = 0;
    bool passable = true;
    std::vector<Version> versions;
  };
  struct ClassEntry {
    std::string name, role, sprite;
    bool summon = false;
  };
  struct DecoMeta {
    std::string model3d, exitKind, doorStyle, wallTexture, art;
    bool props3d = false, thickWall = false, solidHouse = false, solidCart = false, chest = false;
  };
  struct MissionInfo {
    std::string id, title;
    int index = 0;
    bool hub = false;
  };
  struct EditorTables {
    int minGrid = 3, maxGrid = 160, testLevel = 15, enemyLevel0 = 1, maxLevel = 30;
    std::vector<int> enemyLevels;
    std::vector<TerrainEntry> terrains;
    std::map<std::string, TerrainEntry> terrainInfo;  // every terrain id: name, height, variant count
    std::map<std::string, std::vector<std::string>> variantNames;
    std::vector<Version> city;
    std::vector<std::string> decorationOrder, decorationSections, architectureOrder;
    std::map<std::string, std::string> decoSection, decoTile, decoThumb;
    std::set<std::string> decoBlocks;
    std::map<std::string, DecoMeta> decoMeta;
    std::map<std::string, std::pair<std::string, std::string>> doorVariants;  // style: open, closed
    std::map<std::string, ClassEntry> classes;
    std::vector<std::string> classOrder;
    std::vector<std::pair<std::string, std::string>> heroes;  // name, classId
    std::vector<std::pair<std::string, std::string>> namedNonHeroes;
    std::vector<std::pair<std::string, std::string>> npcs;  // id, name
    std::map<std::string, e2::Json> npcSpawn;               // id: encounterNpcSpawn (name, classId, dialog, ...)
    std::vector<std::string> affinityHeroes;
    std::vector<MissionInfo> missions;
    std::map<std::string, int> watchtowerFloors;
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
    const e2::Json j = in ? e2::Json::parse(in, nullptr, false) : e2::Json();
    if (!j.is_object() || !j.contains("editor")) return t;
    const e2::Json& e = j["editor"];
    auto s = [](const e2::Json& o, const char* k) { return o.contains(k) && o[k].is_string() ? o[k].get<std::string>() : std::string(); };
    t.minGrid = e.value("minGrid", 3), t.maxGrid = e.value("maxGrid", 160), t.testLevel = e.value("defaultTestLevel", 15), t.enemyLevel0 = e.value("enemyLevel0", 1);
    t.maxLevel = e.value("maxLevel", 30);
    for (const e2::Json& l : e.value("enemyLevels", e2::Json::array())) t.enemyLevels.push_back(l.get<int>());
    for (const e2::Json& tr : e["terrains"]) {
      TerrainEntry te{s(tr, "id"), s(tr, "name"), s(tr, "swatch"), s(tr, "hint"), {}, tr.value("variantCount", 1), tr.value("height", 0.0), true, {}};
      std::set<int> hidden;
      for (const e2::Json& h : tr["hidden"]) hidden.insert(h.get<int>());
      for (const e2::Json& v : tr["versions"])
        if (!hidden.contains(v.value("variant", 0))) te.versions.push_back({v.value("variant", 0), s(v, "label"), s(v, "thumb")});
      if (e.contains("terrainThumb") && e["terrainThumb"].contains(te.id) && e["terrainThumb"][te.id].is_string()) te.thumb = e["terrainThumb"][te.id].get<std::string>();
      t.terrains.push_back(std::move(te));
    }
    for (const auto& [id, ti] : e["terrainInfo"].items())
      t.terrainInfo[id] = TerrainEntry{id, s(ti, "name"), {}, {}, {}, ti.value("variantCount", 1), ti.value("height", 0.0), ti.value("passable", true), {}};
    const e2::Json tileVariantNamesTable = e.value("tileVariantNames", e2::Json::object());  // a copy: items() of a temporary dangles
    for (const auto& [id, names] : tileVariantNamesTable.items())
      for (const e2::Json& n : names) t.variantNames[id].push_back(n.is_string() ? n.get<std::string>() : std::string());
    for (const e2::Json& v : e["city"]) t.city.push_back({v.value("variant", 0), s(v, "label"), s(v, "thumb")});
    for (const e2::Json& id : e["decorationOrder"]) t.decorationOrder.push_back(id.get<std::string>());
    for (const e2::Json& id : e["decorationSections"]) t.decorationSections.push_back(id.get<std::string>());
    for (const e2::Json& id : e.value("architectureOrder", e2::Json::array())) t.architectureOrder.push_back(id.get<std::string>());
    for (const auto& [id, d] : e["decorationInfo"].items()) {
      t.decoSection[id] = s(d, "section");
      if (d["tile"].is_string()) t.decoTile[id] = d["tile"].get<std::string>();
      t.decoThumb[id] = s(d, "thumb");
      if (d.value("blocksByDefault", false)) t.decoBlocks.insert(id);
    }
    const e2::Json decoMetaTable = e.value("decoMeta", e2::Json::object());  // a copy: items() of a temporary dangles
    for (const auto& [id, d] : decoMetaTable.items())
      t.decoMeta[id] = DecoMeta{s(d, "model3d"),          s(d, "exitKind"),          s(d, "doorStyle"),         s(d, "wallTexture"), s(d, "art"),
                                d.value("props3d", false), d.value("thickWall", false), d.value("solidHouse", false), d.value("solidCart", false),
                                d.value("chest", false)};
    const e2::Json doorVariantsTable = e.value("doorVariants", e2::Json::object());  // a copy: items() of a temporary dangles
    for (const auto& [style, v] : doorVariantsTable.items()) t.doorVariants[style] = {s(v, "open"), s(v, "closed")};
    for (const auto& [id, c] : e["classes"].items()) t.classes[id] = {s(c, "name"), s(c, "role"), {}, c.value("summon", false)};
    const e2::Json classSpriteTable = e.value("classSprite", e2::Json::object());  // a copy: items() of a temporary dangles
    for (const auto& [id, sprite] : classSpriteTable.items())
      if (sprite.is_string()) t.classes[id].sprite = sprite.get<std::string>();
    for (const e2::Json& id : e["classOrder"]) t.classOrder.push_back(id.get<std::string>());
    for (const e2::Json& h : e["editorHeroes"]) t.heroes.push_back({s(h, "name"), s(h, "classId")});
    for (const e2::Json& h : e.value("namedNonHeroes", e2::Json::array())) t.namedNonHeroes.push_back({s(h, "name"), s(h, "classId")});
    for (const e2::Json& n : e["npcs"]) {
      t.npcs.push_back({s(n, "id"), s(n, "name")});
      if (n.contains("spawn") && n["spawn"].is_object()) t.npcSpawn[s(n, "id")] = n["spawn"];
    }
    for (const e2::Json& h : e.value("affinityHeroes", e2::Json::array())) t.affinityHeroes.push_back(h.get<std::string>());
    for (const e2::Json& m : e.value("missions", e2::Json::array())) t.missions.push_back({s(m, "id"), s(m, "title"), m.value("index", 0), m.value("hub", false)});
    const e2::Json watchtowerFloorsTable = e.value("watchtowerFloors", e2::Json::object());  // a copy: items() of a temporary dangles
    for (const auto& [id, f] : watchtowerFloorsTable.items()) t.watchtowerFloors[id] = f.get<int>();
    for (const auto& [id, v] : e["timeOfDay"].items()) t.timeOfDay.push_back({id, s(v, "label"), v.value("moon", false), v.value("key", 0.0), v.value("ambient", 0.0)});
    t.defaultSun = e["defaults"].value("sun", 3.5), t.defaultAmbient = e["defaults"].value("ambient", 1.4), t.defaultBloom = e["defaults"].value("bloom", 0.9);
    for (const e2::Json& b : e["backgrounds"]) t.backgrounds.emplace_back(s(b, "id"), s(b, "label"), s(b, "file"));
    for (const e2::Json& m : e["music"]) t.music.push_back(m.get<std::string>());
    return t;
  }

  const DecoMeta& meta(const std::string& id) const {
    static const DecoMeta none;
    const auto it = editor_.decoMeta.find(id);
    return it == editor_.decoMeta.end() ? none : it->second;
  }
  std::string decoName(const std::string& id) const {
    const e2::CatalogDecoration* d = catalog_.decoration(id);
    return d ? d->name : id;
  }
  int enemyLevelFor(int index) const {
    if (editor_.enemyLevels.empty()) return editor_.enemyLevel0;
    return editor_.enemyLevels[static_cast<size_t>(std::clamp(index, 0, static_cast<int>(editor_.enemyLevels.size()) - 1))];
  }

  // --- the draft --------------------------------------------------------------------------------------------------------

  static std::string base36Now() {
    std::string out;
    for (long long v = static_cast<long long>(e2::Campaign::nowMs()); v > 0; v /= 36) out.insert(out.begin(), "0123456789abcdefghijklmnopqrstuvwxyz"[v % 36]);
    return out;
  }

  // MapEditorScreen blankDraft: 20 x 20 plains, the day's light.
  e2::Json blankDraft() const {
    const int cols = 20, rows = 20, n = cols * rows;
    e2::Json d;
    d["id"] = "custom-" + base36Now();
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

  void moveMisplacedEnemies() {
    e2::Json& enemies = arr("enemySpawns");
    e2::Json kept = e2::Json::array(), moved = e2::Json::array();
    for (const e2::Json& s : enemies) (isSummon(s.value("classId", "")) || isCivil(s.value("classId", "")) ? moved : kept).push_back(s);
    if (moved.empty()) return;
    draft_["enemySpawns"] = kept;
    for (const e2::Json& s : moved) arr("neutralSpawns").push_back(s);
  }

  int cols() const { return draft_.value("cols", 0); }
  int rows() const { return draft_.value("rows", 0); }
  e2::Json& arr(const char* key) {
    if (!draft_.contains(key) || !draft_[key].is_array()) draft_[key] = e2::Json::array();
    return draft_[key];
  }
  std::string tileAt(int i) const {
    return draft_.contains("tiles") && i >= 0 && i < static_cast<int>(draft_["tiles"].size()) && draft_["tiles"][i].is_string() ? draft_["tiles"][i].get<std::string>()
                                                                                                                              : std::string("plains");
  }
  int intAt(const char* key, int i, int fallback = 0) const {
    return draft_.contains(key) && draft_[key].is_array() && i >= 0 && i < static_cast<int>(draft_[key].size()) && draft_[key][i].is_number()
               ? draft_[key][i].get<int>()
               : fallback;
  }
  // An array of the board's size, filled with `fill` where missing (JS Array.from({length}, (_, i) => a?.[i] ?? fill)).
  e2::Json& boardArray(const char* key, const e2::Json& fill) {
    e2::Json& a = arr(key);
    while (static_cast<int>(a.size()) < cols() * rows()) a.push_back(fill);
    return a;
  }

  // Every edit goes through here: history (a burst within 600 ms is one step, Engine2's coalescing), then the preview.
  void beginEdit() {
    const auto now = std::chrono::steady_clock::now();
    if (now - lastEdit_ > std::chrono::milliseconds(600) || past_.empty()) {
      past_.push_back(draft_);
      if (past_.size() > kHistory) past_.pop_front();
      future_.clear();
    }
    lastEdit_ = now;
    dirty_ = true;
  }
  void undo() {
    if (past_.empty()) return;
    future_.push_front(draft_);
    if (future_.size() > kHistory) future_.pop_back();
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
  void setDraft(e2::Json d) {
    beginEdit();
    draft_ = std::move(d);
    selected_.reset(), heldUnit_.reset();
    refreshFiles();
  }
  void note(std::string text) { note_ = std::move(text); }

  // data.ts placedFootprint for a draft placement.
  std::vector<e2::FootOffset> footprintOf(const e2::Json& p) const {
    const e2::CatalogDecoration* def = catalog_.decoration(p.value("id", ""));
    if (!def) return {};
    const double rot = p.contains("rot") && p["rot"].is_number() ? p["rot"].get<double>() : 0.0;
    return e2::placedFootprint(*def, p.value("x", 0), p.value("y", 0), rot);
  }
  bool covers(const e2::Json& p, int x, int y) const {
    for (const e2::FootOffset& f : footprintOf(p))
      if (p.value("x", 0) + f.dx == x && p.value("y", 0) + f.dy == y) return true;
    return false;
  }
  static bool samePlacement(const e2::Json& a, const e2::Json& b) {
    return a.value("id", "") == b.value("id", "") && a.value("x", 0) == b.value("x", 0) && a.value("y", 0) == b.value("y", 0) && a.value("rot", 0) == b.value("rot", 0);
  }
  // previewDecorationCells: the hexes the other props cover; an entrance replaces walls and waypoints under it.
  std::set<std::pair<int, int>> otherCells(const e2::Json& placements, const e2::Json& candidate) const {
    std::set<std::pair<int, int>> mine;
    for (const e2::FootOffset& f : footprintOf(candidate)) mine.insert({candidate.value("x", 0) + f.dx, candidate.value("y", 0) + f.dy});
    const bool waypoint = !meta(candidate.value("id", "")).exitKind.empty();
    std::set<std::pair<int, int>> out;
    for (const e2::Json& p : placements) {
      bool overlaps = false;
      for (const e2::FootOffset& f : footprintOf(p)) overlaps = overlaps || mine.contains({p.value("x", 0) + f.dx, p.value("y", 0) + f.dy});
      if (overlaps) {
        const DecoMeta& m = meta(p.value("id", ""));
        if (candidate.value("id", "") == kEntrance && (m.model3d == "wall" || !m.exitKind.empty())) continue;
        if (waypoint && p.value("id", "") == kEntrance) continue;
      }
      for (const e2::FootOffset& f : footprintOf(p)) out.insert({p.value("x", 0) + f.dx, p.value("y", 0) + f.dy});
    }
    return out;
  }
  // findPreviewDecoration: the exact placement, else (a connector) whatever connector stands on that hex.
  std::optional<size_t> findDecoration(const e2::Json& sel) const {
    if (!draft_.contains("decorations")) return std::nullopt;
    const e2::Json& decos = draft_["decorations"];
    for (size_t i = 0; i < decos.size(); ++i)
      if (samePlacement(decos[i], sel)) return i;
    const std::string kind = meta(sel.value("id", "")).exitKind;
    if (kind != "connector") {
      std::vector<size_t> matches;
      for (size_t i = 0; i < decos.size(); ++i)
        if (decos[i].value("id", "") == sel.value("id", "")) matches.push_back(i);
      if (!kind.empty() && matches.size() == 1) return matches[0];
      return std::nullopt;
    }
    for (size_t i = 0; i < decos.size(); ++i)
      if (decos[i].value("x", 0) == sel.value("x", 0) && decos[i].value("y", 0) == sel.value("y", 0) && meta(decos[i].value("id", "")).exitKind == "connector") return i;
    return std::nullopt;
  }
  void cleanEntrances() { draft_["decorations"] = e2::removeWallsUnderWatchtowerEntrances(arr("decorations"), dress_, catalog_); }
  // baseForDraft: the ground a stamped terrain falls back to.
  std::pair<std::string, int> baseOf() const {
    std::string tile = draft_.contains("baseTile") && draft_["baseTile"].is_string() ? draft_["baseTile"].get<std::string>() : tileAt(0);
    const int maxVariant = std::max(1, variantCount(tile)) - 1;
    const int v = draft_.contains("baseVariant") && draft_["baseVariant"].is_number() ? draft_["baseVariant"].get<int>() : intAt("tileVariants", 0);
    return {tile, std::clamp(v, 0, maxVariant)};
  }
  int variantCount(const std::string& terrain) const {
    const auto it = editor_.terrainInfo.find(terrain);
    return it == editor_.terrainInfo.end() ? 1 : it->second.variantCount;
  }
  std::string terrainName(const std::string& id) const {
    const auto it = editor_.terrainInfo.find(id);
    return it == editor_.terrainInfo.end() ? id : it->second.name;
  }
  bool passable(const std::string& id) const {
    const auto it = editor_.terrainInfo.find(id);
    return it != editor_.terrainInfo.end() && it->second.passable;
  }
  static std::string lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (s.starts_with("\xC3\x81")) s.replace(0, 2, "\xC3\xA1");  // Água
    return s;
  }
  std::string variantName(const std::string& terrain, int v) const {
    const auto it = editor_.variantNames.find(terrain);
    if (it != editor_.variantNames.end() && v >= 0 && v < static_cast<int>(it->second.size()) && !it->second[static_cast<size_t>(v)].empty())
      return it->second[static_cast<size_t>(v)];
    return std::format("{} {:03}", terrainName(terrain), v + 1);
  }
  // baseVersionLabel: the terrain's name and the picker's number / label.
  std::string baseVersionLabel(const std::string& terrain, int v) const {
    if (terrain == "plains" && !editor_.city.empty() && v == editor_.city[0].variant) return "City · Stone paving";
    if (terrain == "plains" && v == 61) return "City · Indoor wood";
    for (const TerrainEntry& t : editor_.terrains)
      if (t.id == terrain)
        for (const Version& ver : t.versions)
          if (ver.variant == v) return terrainName(terrain) + " " + ver.label;
    return std::format("{} {:03}", terrainName(terrain), v + 1);
  }

  // --- the board's clicks (MapEditorScreen onCellClick) ------------------------------------------------------------------

  void onCellClick(int x, int y, std::optional<glm::vec3> point) {
    if (x < 0 || y < 0 || x >= cols() || y >= rows()) return;
    heldUnit_.reset();
    const int i = y * cols() + x;
    if (mode_ == Mode::Water) return paintWater(x, y, point);
    if (mode_ == Mode::Elevation) return elevate(x, y);
    if (mode_ == Mode::Paint) {
      if (turning_) {
        beginEdit();
        e2::Json& rots = boardArray("tileRots", 0);
        rots[i] = (rots[i].is_number() ? rots[i].get<int>() + 1 : 1) % 6;
        const int now = rots[i].get<int>() * 60;
        return note(std::format("{} em {},{}: girado para {}°{}.", terrainName(tileAt(i)), x, y, now, now == 0 ? " (de volta ao original)" : ""));
      }
      return setTile(i, brush_);
    }
    if (mode_ == Mode::Decoration || mode_ == Mode::Architecture) {
      if (turningDeco_) return turnDecoration(x, y);
      const e2::Json& decos = arr("decorations");
      const bool existing = std::any_of(decos.begin(), decos.end(), [&](const e2::Json& p) { return covers(p, x, y); });
      if (const auto t = editor_.decoTile.find(decoBrush_); !existing && t != editor_.decoTile.end())
        note(std::format("{} sobre {} — {}.", decoName(decoBrush_), lower(terrainName(t->second)), passable(t->second) ? "dá pra subir em cima" : "não se atravessa"));
      return toggleDecoration(x, y);
    }
    toggleSpawn(x, y);
  }

  void setTile(int i, const std::string& t) {
    const int x = i % cols(), y = i / cols();
    // Painting under a prop is allowed, but said out loud: the terrain decides whether that house can be climbed.
    for (const e2::Json& p : arr("decorations")) {
      const auto tile = editor_.decoTile.find(p.value("id", ""));
      if (tile == editor_.decoTile.end() || !covers(p, x, y)) continue;
      if (t != tile->second)
        note(std::format("{} agora está sobre {} — {}. O terreno manda, não o desenho.", decoName(p.value("id", "")), lower(terrainName(t)),
                         passable(t) ? "dá pra andar por cima" : "não se atravessa"));
      break;
    }
    beginEdit();
    draft_["tiles"][i] = t;
    boardArray("tileVariants", 0)[i] = std::min(variant_, variantCount(t) - 1);
  }

  // Elevação: the hex and `radius` rings around it raised, lowered or set to a level (0..12); void is left alone.
  void elevate(int x, int y) {
    beginEdit();
    const int n = cols() * rows();
    e2::Json& elev = arr("terrainElevations");
    for (int k = 0; k < n; ++k) {
      const auto it = editor_.terrainInfo.find(tileAt(k));
      const double h = it == editor_.terrainInfo.end() ? 0.0 : it->second.height;
      if (k >= static_cast<int>(elev.size())) elev.push_back(h);
      else if (!elev[k].is_number()) elev[k] = h;
    }
    std::vector<std::pair<int, int>> frontier{{x, y}};
    std::set<int> visited;
    for (int ring = 0; ring <= elevationRadius_; ++ring) {
      std::vector<std::pair<int, int>> next;
      for (const auto& [cx, cy] : frontier) {
        if (cx < 0 || cy < 0 || cx >= cols() || cy >= rows()) continue;
        const int k = cy * cols() + cx;
        if (!visited.insert(k).second) continue;
        if (tileAt(k) != "void")
          elev[k] = elevationTool_ == 2 ? static_cast<double>(elevationLevel_) : std::clamp(elev[k].get<double>() + (elevationTool_ == 0 ? 1.0 : -1.0), 0.0, 12.0);
        for (const e2::OverworldHex& nb : e2::Overworld::neighbors(cx, cy)) next.push_back({nb.col, nb.row});
      }
      frontier = std::move(next);
    }
  }

  // Água 3D: water patches of the brush's level, size and shape on the hex and its rings (or removed around them),
  // positioned in Ember's flat map units (the clicked point, else the hex centre).
  void paintWater(int x, int y, std::optional<glm::vec3> point) {
    beginEdit();
    const int n = cols() * rows();
    e2::Json& levels = boardArray("waterLevels", nullptr);
    e2::Json& footprints = boardArray("waterFootprints", nullptr);
    e2::Json patches = arr("waterPatches");
    const double sq3 = std::sqrt(3.0);
    std::optional<std::pair<double, double>> at;
    if (point) {
      const e2::HexLayout L = e2::makeLayout(cols(), rows());
      at = std::pair{point->x + L.ox + sq3 / 2, point->z + L.oz + 3.4};
    }
    auto dist = [&](const e2::Json& p, double px, double py, bool square) {
      const double dx = p.value("x", 0.0) - px, dy = p.value("y", 0.0) - py;
      return square ? std::max(std::abs(dx), std::abs(dy)) : std::hypot(dx, dy);
    };
    const std::string shape = waterSquare_ ? "square" : "round";
    std::vector<std::pair<int, int>> frontier{{x, y}};
    std::set<int> visited;
    for (int ring = 0; ring <= waterRadius_; ++ring) {
      std::vector<std::pair<int, int>> next;
      for (const auto& [cx, cy] : frontier) {
        if (cx < 0 || cy < 0 || cx >= cols() || cy >= rows()) continue;
        const int index = cy * cols() + cx;
        if (!visited.insert(index).second) continue;
        if (tileAt(index) != "void" || waterErase_) {
          double px, py;
          if (at) px = at->first + sq3 * (cx - x + 0.5 * ((cy & 1) - (y & 1))), py = at->second + 1.5 * (cy - y);
          else px = sq3 * (cx + 0.5 * (cy & 1) + 0.5), py = 2.4 + 1.5 * cy + 1;
          e2::Json kept = e2::Json::array();
          for (const e2::Json& p : patches) {
            if (waterErase_) {
              if (dist(p, px, py, at && waterSquare_) > (waterSize_ + p.value("size", 0.0)) * 1.25) kept.push_back(p);
            } else if (!(std::hypot(p.value("x", 0.0) - px, p.value("y", 0.0) - py) < 0.001 && p.value("size", 0.0) == waterSize_ &&
                         p.value("shape", "") == shape && p.value("level", 0.0) == waterLevel_)) {
              kept.push_back(p);
            }
          }
          patches = std::move(kept);
          if (waterErase_) levels[index] = nullptr, footprints[index] = nullptr;
          else patches.push_back(e2::Json{{"x", px}, {"y", py}, {"level", waterLevel_}, {"size", waterSize_}, {"shape", shape}});
        }
        for (const e2::OverworldHex& nb : e2::Overworld::neighbors(cx, cy)) next.push_back({nb.col, nb.row});
      }
      frontier = std::move(next);
    }
    (void)n;
    draft_["waterPatches"] = patches;
  }

  // toggleDecoration: a click on a placed prop selects it (another 3D piece over a 3D piece replaces it); elsewhere the
  // brush is placed on the anchor hex (refused when its footprint would land on another prop); a Waypoint shares hexes.
  void toggleDecoration(int x, int y) {
    e2::Json& decos = arr("decorations");
    std::optional<size_t> clicked;
    for (size_t k = 0; k < decos.size() && !clicked; ++k)
      if (covers(decos[k], x, y)) clicked = k;
    const DecoMeta& brush = meta(decoBrush_);
    if (clicked) {
      const e2::Json c = decos[*clicked];
      const DecoMeta& cm = meta(c.value("id", ""));
      if (!cm.model3d.empty() && !brush.model3d.empty() && c.value("id", "") != decoBrush_) {
        beginEdit();
        e2::Json replacement = c;
        replacement["id"] = decoBrush_, replacement["rot"] = wallVertical_ ? 1 : 0, replacement["wallOrientation"] = wallVertical_ ? "vertical" : "horizontal";
        replacement.erase("blocksPath"), replacement.erase("yieldsHighGround");
        decos[*clicked] = replacement;
        cleanEntrances();
        selected_.reset();
        return note(std::format("{} colocada em {},{}.", decoName(decoBrush_), c.value("x", 0), c.value("y", 0)));
      }
      const bool brushIsWaypoint = !brush.exitKind.empty();
      const bool entranceOverWaypoint = decoBrush_ == kEntrance && !cm.exitKind.empty();
      if (!entranceOverWaypoint && (!brushIsWaypoint || (!cm.exitKind.empty() && c.value("x", 0) == x && c.value("y", 0) == y))) {
        select(c);
        return note(std::format("{} selecionada. Pressione Delete para remover.", decoName(c.value("id", ""))));
      }
    }
    const e2::CatalogDecoration* def = catalog_.decoration(decoBrush_);
    if (!def) return;
    e2::Json placed = {{"id", decoBrush_}, {"x", x}, {"y", y}};
    if (!brush.model3d.empty()) placed["rot"] = wallVertical_ ? 1 : 0, placed["wallOrientation"] = wallVertical_ ? "vertical" : "horizontal";
    else if (editor_.decoBlocks.contains(decoBrush_)) placed["blocksPath"] = true;
    const std::set<std::pair<int, int>> covered = otherCells(decos, placed);
    for (const e2::FootOffset& f : footprintOf(placed))
      if (brush.exitKind.empty() && covered.contains({x + f.dx, y + f.dy})) return;
    beginEdit();
    if (const auto t = editor_.decoTile.find(decoBrush_); t != editor_.decoTile.end())
      for (const e2::FootOffset& f : def->footprint) {
        const int cx = x + f.dx, cy = y + f.dy;
        if (cx >= 0 && cy >= 0 && cx < cols() && cy < rows()) draft_["tiles"][cy * cols() + cx] = t->second;
      }
    arr("decorations").push_back(std::move(placed));
    cleanEntrances();
  }

  // A placed prop picked (a click, the preview's press): it opens its palette section with its brush armed.
  void select(const e2::Json& p) {
    selected_ = e2::Json{{"id", p.value("id", "")}, {"x", p.value("x", 0)}, {"y", p.value("y", 0)}, {"rot", p.value("rot", 0)}};
    const std::string id = p.value("id", "");
    if (!catalog_.decoration(id)) return;
    const bool model = !meta(id).model3d.empty();
    if (!model || mode_ != Mode::Architecture) decoBrush_ = id;
    if (model) mode_ = Mode::Architecture;
    else if (mode_ == Mode::Architecture) mode_ = Mode::Decoration;
    if (const auto s = editor_.decoSection.find(id); s != editor_.decoSection.end()) decoSection_ = s->second;
  }

  // Restores the base ground under a prop that stamped its own terrain, over `cells`.
  void unstamp(const std::string& id, const std::vector<std::pair<int, int>>& cells) {
    const auto t = editor_.decoTile.find(id);
    if (t == editor_.decoTile.end()) return;
    const auto [baseTile, baseVariant] = baseOf();
    for (const auto& [cx, cy] : cells) {
      if (cx < 0 || cy < 0 || cx >= cols() || cy >= rows()) continue;
      const int k = cy * cols() + cx;
      if (tileAt(k) != t->second) continue;
      draft_["tiles"][k] = baseTile;
      boardArray("tileVariants", 0)[k] = baseVariant;
      boardArray("tileRots", 0)[k] = 0;
    }
  }
  void stamp(const std::string& id, const std::vector<std::pair<int, int>>& cells) {
    const auto t = editor_.decoTile.find(id);
    if (t == editor_.decoTile.end()) return;
    for (const auto& [cx, cy] : cells)
      if (cx >= 0 && cy >= 0 && cx < cols() && cy < rows()) draft_["tiles"][cy * cols() + cx] = t->second;
  }
  std::vector<std::pair<int, int>> cellsOf(const e2::Json& p) const {
    std::vector<std::pair<int, int>> out;
    for (const e2::FootOffset& f : footprintOf(p)) out.push_back({p.value("x", 0) + f.dx, p.value("y", 0) + f.dy});
    return out;
  }

  // turnDecoration: the prop under the hex a sixth of a turn (a 3D piece a quarter), footprint and stamped terrain
  // with it; refused when it would land on another prop.
  void turnDecoration(int x, int y) {
    e2::Json& decos = arr("decorations");
    for (size_t k = 0; k < decos.size(); ++k) {
      const e2::Json hit = decos[k];
      if (!covers(hit, x, y)) continue;
      const DecoMeta& m = meta(hit.value("id", ""));
      if (!catalog_.decoration(hit.value("id", ""))) return;
      const bool model = !m.model3d.empty();
      e2::Json turned = hit;
      const int rot = (hit.value("rot", 0) + 1) % (model ? 4 : 6);
      turned["rot"] = rot;
      if (model) turned["wallOrientation"] = rot % 2 ? "vertical" : "horizontal";
      if (m.exitKind.empty()) {
        e2::Json others = e2::Json::array();
        for (size_t o = 0; o < decos.size(); ++o)
          if (o != k) others.push_back(decos[o]);
        const std::set<std::pair<int, int>> taken = otherCells(others, turned);
        for (const auto& c : cellsOf(turned))
          if (taken.contains(c)) return note(std::format("{} nao cabe girada aqui — bateria em outra decoracao.", decoName(hit.value("id", ""))));
      }
      beginEdit();
      unstamp(hit.value("id", ""), cellsOf(hit));
      stamp(hit.value("id", ""), cellsOf(turned));
      note(std::format("{} em {},{}: girada para {}°{}.", decoName(hit.value("id", "")), hit.value("x", 0), hit.value("y", 0), rot * (model ? 90 : 60),
                       rot == 0 ? " (de volta ao original)" : ""));
      decos[k] = turned;
      if (model) selected_ = e2::Json{{"id", turned["id"]}, {"x", turned["x"]}, {"y", turned["y"]}, {"rot", rot}};
      cleanEntrances();
      return;
    }
    note("Nao ha decoracao nessa casa pra girar.");
  }

  void removeSelectedDecoration() {
    if (!selected_) return note("Clique em qualquer hex da decoração e então pressione Delete.");
    const std::optional<size_t> k = findDecoration(*selected_);
    selected_.reset();
    if (!k) return note("Essa decoração já não está no mapa.");
    beginEdit();
    const e2::Json hit = arr("decorations")[*k];
    unstamp(hit.value("id", ""), cellsOf(hit));
    note(std::format("{} removida.", decoName(hit.value("id", ""))));
    arr("decorations").erase(*k);
  }

  // The selected placement's rule switches (off = the key absent, as Engine2 writes it).
  void toggleDecorationRule(const char* flag) {
    if (!selected_) return note("Clique em qualquer hex de uma decoração no mapa antes de mudar as regras dela.");
    const std::optional<size_t> k = findDecoration(*selected_);
    if (!k) return note("Essa decoração já não está no mapa.");
    beginEdit();
    e2::Json& p = arr("decorations")[*k];
    const bool on = !(p.contains(flag) && p[flag].is_boolean() && p[flag].get<bool>());
    if (on) p[flag] = true;
    else p.erase(flag);
    note(std::format("{}: {} {}.", decoName(p.value("id", "")), std::string(flag) == "blocksPath" ? "Bloquear caminho" : "Alto terreno", on ? "ligado" : "desligado"));
  }
  e2::Json* selectedPlacement() {
    if (!selected_) return nullptr;
    const std::optional<size_t> k = findDecoration(*selected_);
    return k ? &arr("decorations")[*k] : nullptr;
  }

  // toggleSpawn: the side's brush lifts whoever stands on the hex on that side, else drops a new unit there.
  void toggleSpawn(int x, int y) {
    const char* key = mode_ == Mode::Enemy  ? "enemySpawns"
                      : mode_ == Mode::Npc  ? "neutralSpawns"
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
  bool isCivil(const std::string& classId) const {
    const auto it = editor_.classes.find(classId);
    return it != editor_.classes.end() && it->second.role.starts_with("Civil");
  }
  std::string className(const std::string& classId) const {
    const auto it = editor_.classes.find(classId);
    return it == editor_.classes.end() ? classId : it->second.name;
  }
  // A picker's label for a class: a named hero's own name for their class, else "Name · role".
  std::string classLabel(const std::string& classId) const {
    for (const auto& [name, c] : editor_.heroes)
      if (c == classId) return name;
    for (const auto& [name, c] : editor_.namedNonHeroes)
      if (c == classId) return name;
    const auto it = editor_.classes.find(classId);
    return it == editor_.classes.end() ? classId : it->second.name + " · " + it->second.role;
  }

  // addHeroes: each hero not on the board yet, on the bottom row from the left, skipping taken hexes.
  void addHeroes(const std::vector<std::pair<std::string, std::string>>& heroes) {
    std::set<std::pair<int, int>> occupied;
    for (const char* key : {"playerSpawns", "enemySpawns", "neutralSpawns"})
      for (const e2::Json& s : arr(key)) occupied.insert({s.value("x", 0), s.value("y", 0)});
    std::set<std::string> already;
    for (const e2::Json& s : arr("playerSpawns")) already.insert(s.value("name", ""));
    std::vector<std::string> added, skipped;
    e2::Json put = e2::Json::array();
    int x = 0;
    const int y = rows() - 1;
    for (const auto& [name, classId] : heroes) {
      if (already.contains(name)) {
        skipped.push_back(name);
        continue;
      }
      while (x < cols() && occupied.contains({x, y})) ++x;
      if (x >= cols()) break;
      put.push_back({{"name", name}, {"classId", classId}, {"x", x}, {"y", y}, {"level", editor_.testLevel}});
      added.push_back(name);
      occupied.insert({x, y});
      ++x;
    }
    auto join = [](const std::vector<std::string>& v) {
      std::string s;
      for (const std::string& n : v) s += (s.empty() ? "" : ", ") + n;
      return s;
    };
    if (!added.empty()) {
      beginEdit();
      for (const e2::Json& s : put) arr("playerSpawns").push_back(s);
      note(skipped.empty() ? std::format("{} na linha de baixo.", join(added))
                           : std::format("{} na linha de baixo ({} já estava lá).", join(added), join(skipped)));
    } else {
      std::vector<std::string> names;
      for (const auto& h : heroes) names.push_back(h.first);
      note(std::format("{} já {} no mapa.", join(names), heroes.size() > 1 ? "estavam" : "estava"));
    }
  }

  // resize: the board grows or shrinks from the bottom-right; new cells take the base; props, water, units off the
  // board go.
  void resize(int c, int r) {
    c = std::clamp(c, editor_.minGrid, editor_.maxGrid), r = std::clamp(r, editor_.minGrid, editor_.maxGrid);
    if (c == cols() && r == rows()) return;
    beginEdit();
    const auto [baseTile, baseVariant] = baseOf();
    const int oc = cols(), orows = rows();
    auto get = [&](const char* key, int i) -> e2::Json { return draft_.contains(key) && draft_[key].is_array() && i < static_cast<int>(draft_[key].size()) ? draft_[key][i] : e2::Json(); };
    const bool hadElev = draft_.contains("terrainElevations") && draft_["terrainElevations"].is_array();
    const bool hadLevels = draft_.contains("waterLevels") && draft_["waterLevels"].is_array();
    const bool hadFoot = draft_.contains("waterFootprints") && draft_["waterFootprints"].is_array();
    e2::Json tiles = e2::Json::array(), variants = e2::Json::array(), rots = e2::Json::array(), elev = e2::Json::array(), levels = e2::Json::array(),
             foot = e2::Json::array();
    for (int y = 0; y < r; ++y)
      for (int x = 0; x < c; ++x) {
        const bool in = y < orows && x < oc;
        const int i = y * oc + x;
        foot.push_back(in && !get("waterFootprints", i).is_null() ? get("waterFootprints", i) : e2::Json());
        levels.push_back(in && !get("waterLevels", i).is_null() ? get("waterLevels", i) : e2::Json());
        tiles.push_back(in && get("tiles", i).is_string() ? get("tiles", i) : e2::Json(baseTile));
        variants.push_back(in && get("tileVariants", i).is_number() ? get("tileVariants", i) : e2::Json(baseVariant));
        rots.push_back(in && get("tileRots", i).is_number() ? get("tileRots", i) : e2::Json(0));
        const auto info = editor_.terrainInfo.find(in && get("tiles", i).is_string() ? get("tiles", i).get<std::string>() : baseTile);
        elev.push_back(in ? (get("terrainElevations", i).is_number() ? get("terrainElevations", i) : e2::Json(info != editor_.terrainInfo.end() ? info->second.height : 0.0))
                          : e2::Json(0));
      }
    draft_["cols"] = c, draft_["rows"] = r;
    draft_["tiles"] = tiles, draft_["tileVariants"] = variants, draft_["tileRots"] = rots;
    if (hadElev) draft_["terrainElevations"] = elev;
    if (hadLevels) draft_["waterLevels"] = levels;
    if (hadFoot) draft_["waterFootprints"] = foot;
    if (draft_.contains("waterPatches") && draft_["waterPatches"].is_array()) {
      e2::Json kept = e2::Json::array();
      for (const e2::Json& p : draft_["waterPatches"]) {
        const double px = p.value("x", 0.0), py = p.value("y", 0.0);
        if (px >= 0 && px < (c + 0.5) * std::sqrt(3.0) && py >= 2.4 && py < 2.4 + r * 1.5 + 0.5) kept.push_back(p);
      }
      draft_["waterPatches"] = kept;
    }
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
    e2::Json fx = e2::Json::array();
    for (const e2::Json& p : arr("elementalFx"))
      if (p.value("x", -1) >= 0 && p.value("y", -1) >= 0 && p.value("x", 0) < c && p.value("y", 0) < r) fx.push_back(p);
    draft_["elementalFx"] = fx;
    for (const char* key : {"playerSpawns", "enemySpawns", "neutralSpawns"}) {
      e2::Json kept = e2::Json::array();
      for (const e2::Json& s : arr(key))
        if (s.value("x", 0) < c && s.value("y", 0) < r) kept.push_back(s);
      draft_[key] = kept;
    }
  }

  // Gerar terreno (world/MapDressing.h, checked against Engine2's dressMap): replaces the props, keeps the units.
  void generateTerrain(uint32_t seed = 0) {
    std::mt19937 rng(seed ? seed : std::random_device{}());
    std::uniform_real_distribution<double> uniform(0.0, 1.0);
    const e2::DressResult r = e2::dressDraft(draft_, dress_, catalog_, shuffleExclude_, [&] { return uniform(rng); });
    beginEdit();
    draft_["tiles"] = r.tiles;
    draft_["tileVariants"] = r.tileVariants;
    e2::Json rots = e2::Json::array();
    for (size_t i = 0; i < r.tiles.size(); ++i) rots.push_back(intAt("tileRots", static_cast<int>(i)));
    draft_["tileRots"] = rots;
    draft_["decorations"] = r.decorations;
    selected_.reset();
    // the note counts terrains in the order they first appear (a JS Map)
    std::vector<std::pair<std::string, int>> counts;
    for (const std::string& t : r.tiles) {
      if (t == "plains") continue;
      auto it = std::find_if(counts.begin(), counts.end(), [&](const auto& c) { return c.first == t; });
      if (it == counts.end()) counts.push_back({t, 1});
      else ++it->second;
    }
    std::string summary;
    for (const auto& [t, n] : counts) summary += (summary.empty() ? "" : ", ") + std::format("{} {}", n, lower(terrainName(t)));
    note(std::format("Gerado em {}x{}: {}{} — apague o que não servir.", cols(), rows(), summary.empty() ? "nada" : summary,
                     r.decorations.empty() ? "" : std::format(", {} decoração(ões)", r.decorations.size())));
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
    options.editorClick = [this](int x, int y) {
      pendingClick_ = std::make_pair(x, y);
      pendingPoint_ = preview_ ? preview_->lastPickPoint() : std::nullopt;
    };
    options.editorGrab = [this](int x, int y, bool left) { return grab(x, y, left); };
    options.editorRelease = [this](std::optional<std::pair<int, int>> hex, bool moved) { pendingRelease_ = std::make_pair(hex, moved); };
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

  // MapPreviewCanvas onPointerDownCapture: a unit (any side) or a prop under the press is picked up; the left button only
  // when Girar objeto is not armed.
  bool grab(int x, int y, bool left) {
    if (left && turningDeco_) return false;
    for (const char* key : {"playerSpawns", "enemySpawns", "neutralSpawns"}) {
      const e2::Json& list = arr(key);
      for (size_t k = 0; k < list.size(); ++k)
        if (list[k].value("x", -1) == x && list[k].value("y", -1) == y) {
          heldUnit_ = HeldUnit{key, k};
          heldDeco_.reset();
          selected_.reset();
          note(std::format("{}: pressione Delete para remover, ou arraste para outro hex pra mover.", list[k].value("name", "")));
          return true;
        }
    }
    const e2::Json& decos = arr("decorations");
    std::optional<size_t> hit;
    for (size_t k = 0; k < decos.size(); ++k)
      if (covers(decos[k], x, y) && (!hit || (!meta(decos[k].value("id", "")).exitKind.empty() && meta(decos[*hit].value("id", "")).exitKind.empty()))) hit = k;
    if (!hit) return false;
    heldUnit_.reset();
    const e2::Json& p = decos[*hit];
    heldDeco_ = e2::Json{{"id", p.value("id", "")}, {"x", p.value("x", 0)}, {"y", p.value("y", 0)}, {"rot", p.value("rot", 0)}};
    select(p);
    note(std::format("{} selecionada. Arraste até um hex vazio da prévia para movê-la.", decoName(p.value("id", ""))));
    return true;
  }

  // The press let go: a drag drops the held unit / prop on the hex under the pointer; a plain press keeps the unit held
  // (Delete removes it).
  void release(std::optional<std::pair<int, int>> hex, bool moved) {
    if (heldUnit_) {
      if (!moved) return;
      const HeldUnit held = *heldUnit_;
      heldUnit_.reset();
      if (hex) placeUnit(held, hex->first, hex->second);
      return;
    }
    if (heldDeco_) {
      const e2::Json held = *heldDeco_;
      heldDeco_.reset();
      if (moved && hex) placeDecoration(held, hex->first, hex->second);
    }
  }
  // placePreviewUnit: refused onto a hex another unit holds.
  void placeUnit(const HeldUnit& held, int x, int y) {
    for (const char* key : {"playerSpawns", "enemySpawns", "neutralSpawns"}) {
      const e2::Json& list = arr(key);
      for (size_t k = 0; k < list.size(); ++k)
        if (!(held.key == key && held.index == k) && list[k].value("x", -1) == x && list[k].value("y", -1) == y)
          return note("Esse hex já tem uma unidade. Escolha um hex vazio.");
    }
    e2::Json& list = arr(held.key.c_str());
    if (held.index >= list.size()) return note("Essa unidade não existe mais. Arraste outra na prévia.");
    beginEdit();
    list[held.index]["x"] = x, list[held.index]["y"] = y;
    note(std::format("{} movido para {},{}.", list[held.index].value("name", ""), x, y));
  }
  void deleteHeldUnit() {
    const HeldUnit held = *heldUnit_;
    heldUnit_.reset();
    e2::Json& list = arr(held.key.c_str());
    if (held.index >= list.size()) return;
    beginEdit();
    note(std::format("{} removido do mapa.", list[held.index].value("name", "")));
    list.erase(held.index);
  }
  // placePreviewDecoration: refused off the board or onto another prop; the stamped terrain moves with it.
  void placeDecoration(const e2::Json& sel, int x, int y) {
    const std::optional<size_t> k = findDecoration(sel);
    if (!k) return note("Essa decoração já não está no mapa.");
    e2::Json& decos = arr("decorations");
    const e2::Json hit = decos[*k];
    if (hit.value("x", 0) == x && hit.value("y", 0) == y) return;
    e2::Json moved = hit;
    moved["x"] = x, moved["y"] = y;
    const std::string name = decoName(hit.value("id", ""));
    for (const auto& [cx, cy] : cellsOf(moved))
      if (cx < 0 || cy < 0 || cx >= cols() || cy >= rows()) return note(std::format("{} não cabe aí — sairia do mapa.", name));
    e2::Json others = e2::Json::array();
    for (size_t o = 0; o < decos.size(); ++o)
      if (o != *k) others.push_back(decos[o]);
    const std::set<std::pair<int, int>> taken = otherCells(others, moved);
    for (const auto& c : cellsOf(moved))
      if (taken.contains(c)) return note(std::format("{} não cabe aí — bateria em outra decoração.", name));
    beginEdit();
    unstamp(hit.value("id", ""), cellsOf(hit));
    stamp(hit.value("id", ""), cellsOf(moved));
    decos[*k] = moved;
    selected_ = e2::Json{{"id", moved["id"]}, {"x", x}, {"y", y}, {"rot", moved.value("rot", 0)}};
    note(std::format("{} movida para {},{}.", name, x, y));
    cleanEntrances();
  }

  // dioramaView's rings: the hovered hex (warm), every hex of the selected prop (cool).
  std::vector<Renderer::GroundMark> rings() const {
    std::vector<Renderer::GroundMark> out;
    auto add = [&](int x, int y, ImU32 color) {
      if (x >= 0 && y >= 0 && x < cols() && y < rows()) out.push_back({static_cast<uint32_t>(y * cols() + x), color, 0.97f, 0.8f});
    };
    if (selected_)
      for (const auto& [x, y] : cellsOf(*selected_)) add(x, y, IM_COL32(120, 200, 255, 230));
    if (hover_) add(hover_->first, hover_->second, IM_COL32(255, 214, 140, 230));
    return out;
  }
  // The hovered hex's terrain, art and level by the cursor (MapPreviewCanvas hoverTile).
  void drawHoverTip() {
    if (!hover_ || ImGui::GetIO().WantCaptureMouse || heldUnit_ || heldDeco_) return;
    const int i = hover_->second * cols() + hover_->first;
    const std::string t = tileAt(i);
    const auto info = editor_.terrainInfo.find(t);
    if (info == editor_.terrainInfo.end()) return;
    const double level = draft_.contains("terrainElevations") && draft_["terrainElevations"].is_array() && i < static_cast<int>(draft_["terrainElevations"].size()) &&
                                 draft_["terrainElevations"][i].is_number()
                             ? draft_["terrainElevations"][i].get<double>()
                             : info->second.height;
    ImGui::SetTooltip("%s — %s · Nível %g", info->second.name.c_str(), variantName(t, intAt("tileVariants", i)).c_str(), level);
  }

  // --- saved maps: the files on disk ---------------------------------------------------------------------------------------

  struct SavedFile {
    int serial = 0;
    std::filesystem::path file;
    bool own = false;  // content/maps/ember (else Engine2's folder, read-only)
    long long savedAt = 0;
  };
  struct Scenario {
    std::string id, title;
    int index = 0;
    bool hub = false;
    std::vector<SavedFile> files;      // by serial
    std::vector<std::string> floors;   // the maps its floor connectors lead to (newest draft's targetMapId)
  };
  // Every map id saved in Engine2's folder and in ours, with its newest draft's title / index / hub.
  static std::map<std::string, Scenario> scanSaved(const std::filesystem::path& engine2Maps) {
    std::map<std::string, Scenario> out;
    std::map<std::string, std::pair<int, std::filesystem::path>> newest;
    for (const auto& [dir, own] : {std::pair{engine2Maps, false}, std::pair{savedMapsDir(), true}}) {
      std::error_code ec;
      if (!std::filesystem::is_directory(dir, ec)) continue;
      for (const auto& e : std::filesystem::directory_iterator(dir, ec)) {
        if (e.path().extension() != ".json") continue;
        const std::string stem = e.path().stem().string();
        std::ifstream in(e.path(), std::ios::binary);
        const e2::Json j = in ? e2::Json::parse(in, nullptr, false) : e2::Json();
        if (!j.is_object()) continue;
        const e2::Json& d = j.contains("draft") ? j["draft"] : j;
        if (!d.is_object() || !d.contains("id") || !d["id"].is_string()) continue;
        const std::string id = d["id"].get<std::string>();
        const int serial = j.contains("serial") && j["serial"].is_number() ? j["serial"].get<int>() : 0;
        Scenario& s = out[id];
        s.id = id;
        s.files.push_back({serial, e.path(), own, j.contains("savedAt") && j["savedAt"].is_number() ? j["savedAt"].get<long long>() : 0LL});
        if (!newest.contains(id) || serial >= newest[id].first) {
          newest[id] = {serial, e.path()};
          s.title = d.contains("title") && d["title"].is_string() ? d["title"].get<std::string>() : id;
          s.index = d.contains("index") && d["index"].is_number() ? d["index"].get<int>() : 0;
          s.hub = d.contains("hub") && d["hub"].is_boolean() && d["hub"].get<bool>();
          s.floors.clear();
          if (d.contains("decorations") && d["decorations"].is_array())
            for (const e2::Json& deco : d["decorations"])
              if (deco.is_object() && deco.contains("targetMapId") && deco["targetMapId"].is_string() && !deco["targetMapId"].get<std::string>().empty())
                s.floors.push_back(deco["targetMapId"].get<std::string>());
        }
      }
    }
    for (auto& [id, s] : out) std::sort(s.files.begin(), s.files.end(), [](const SavedFile& a, const SavedFile& b) { return a.serial < b.serial || (a.serial == b.serial && !a.own); });
    return out;
  }
  void rescan() {
    if (scan_.valid()) return;
    scan_ = std::async(std::launch::async, [maps = paths_.maps] { return scanSaved(maps); });
  }
  void refreshFiles() {
    files_.clear();
    const auto it = saved_.find(draft_.value("id", ""));
    if (it != saved_.end()) files_ = it->second.files;
  }
  // The newest file of an id (the highest serial; ours over Engine2's on a tie).
  std::optional<std::filesystem::path> newestFile(const std::string& id) const {
    const auto it = saved_.find(id);
    if (it == saved_.end() || it->second.files.empty()) return std::nullopt;
    return it->second.files.back().file;
  }
  static e2::Json readDraft(const std::filesystem::path& file) {
    std::ifstream in(file, std::ios::binary);
    const e2::Json j = in ? e2::Json::parse(in, nullptr, false) : e2::Json();
    if (!j.is_object()) return e2::Json();
    return j.contains("draft") ? j["draft"] : j;
  }
  std::map<std::string, int> activeMaps() const {
    std::map<std::string, int> out;
    std::ifstream in(activeMapsFile(), std::ios::binary);
    const nlohmann::json j = in ? nlohmann::json::parse(in, nullptr, false) : nlohmann::json();
    if (j.is_object())
      for (const auto& [id, s] : j.items())
        if (s.is_number_integer()) out[id] = s.get<int>();
    return out;
  }
  void setActive(const std::string& id, std::optional<int> serial) {
    nlohmann::json j = nlohmann::json::object();
    for (const auto& [k, v] : activeMaps()) j[k] = v;
    if (serial) j[id] = *serial;
    else j.erase(id);
    std::error_code ec;
    std::filesystem::create_directories(activeMapsFile().parent_path(), ec);
    std::ofstream(activeMapsFile(), std::ios::binary | std::ios::trunc) << j.dump(2) << "\n";
  }

  // normalizeScenarioId / stripScenarioId: lowercase ASCII letters and digits, dashes, at most 64 (accents dropped).
  static std::string stripScenarioId(const std::string& id) {
    std::string out;
    for (size_t i = 0; i < id.size(); ++i) {
      unsigned char c = static_cast<unsigned char>(id[i]);
      if (c == 0xC3 && i + 1 < id.size()) {  // Latin-1 letters: their base letter (NFD without the marks)
        static const char* base = "AAAAAAACEEEEIIIIDNOOOOOxOUUUUYTsaaaaaaaceeeeiiiidnooooo/ouuuuyty";
        const unsigned char d = static_cast<unsigned char>(id[++i]);
        c = d >= 0x80 && d <= 0xBF ? static_cast<unsigned char>(base[d - 0x80]) : '-';
      } else if (c >= 0x80) {
        c = '-';
      }
      if (std::isalnum(c)) out += static_cast<char>(std::tolower(c));
      else if (out.empty() || out.back() != '-') out += '-';
    }
    if (out.size() > 64) out.resize(64);
    return out;
  }
  static std::string scenarioId(const std::string& id) {
    std::string out = stripScenarioId(id);
    while (!out.empty() && out.front() == '-') out.erase(out.begin());
    while (!out.empty() && out.back() == '-') out.pop_back();
    return out.empty() ? "scenario" : out;
  }

  // Salvar (doSave): Engine2's map file ({serial, savedAt, draft}, 2-space JSON) as the next serial of the id, counted
  // over Engine2's maps and ours; it becomes the id's active version. The game's scene and rules for it are prepared in
  // the background (Engine2's exporters, a few seconds). Engine2's folder is never written.
  bool save(std::optional<e2::Json> toSave = std::nullopt) {
    e2::Json d = toSave ? *toSave : draft_;
    d["id"] = scenarioId(d.value("id", "scenario"));
    if (d["id"] == "thebridge") d["title"] = "A Ponte de Pedra";
    if (d != draft_) draft_ = d, dirty_ = true;
    const std::string id = d["id"].get<std::string>();
    const int serial = std::max(highestMapSerial(savedMapsDir(), id), highestMapSerial(paths_.maps, id)) + 1;
    const std::filesystem::path file = savedMapsDir() / std::format("{}{:03}.json", id, serial);
    std::error_code ec;
    std::filesystem::create_directories(savedMapsDir(), ec);
    e2::Json out;
    out["serial"] = serial;
    out["savedAt"] = static_cast<long long>(e2::Campaign::nowMs());
    out["draft"] = d;
    out["draft"]["decorations"] = e2::removeWallsUnderWatchtowerEntrances(d.value("decorations", e2::Json::array()), dress_, catalog_);
    const std::filesystem::path tmp = file.string() + ".tmp";
    {
      std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
      f << out.dump(2) << "\n";
      if (!f) {
        note("NÃO SALVO: não foi possível gravar " + file.generic_string() + ". O rascunho continua aberto.");
        return false;
      }
    }
    std::filesystem::rename(tmp, file, ec);
    std::ifstream back(file, std::ios::binary);
    const e2::Json saved = ec ? e2::Json() : e2::Json::parse(back, nullptr, false);
    if (saved.is_discarded() || !saved.is_object() || saved.value("serial", 0) != serial || saved["draft"] != out["draft"]) {
      note("NÃO SALVO: o arquivo gravado não confere (" + file.generic_string() + "). O rascunho continua aberto.");
      return false;
    }
    setActive(id, serial);
    note(std::format("Salvo: {} (v{:03}). Preparando para o jogo…", file.filename().string(), serial));
    prepare(file);
    rescan();
    return true;
  }
  // A map file made playable for the campaign and for Modo teste (MapExport.h), in the background.
  void prepare(const std::filesystem::path& file) {
    const std::string stem = file.stem().string();
    const std::filesystem::path engine2 = base_.engine2Public.parent_path();
    exports_.push_back(std::async(std::launch::async, [paths = paths_, file, stem, engine2]() -> std::string {
      const std::filesystem::path log = std::filesystem::path("build/editor") / (stem + ".log");
      const MapExportResult c = exportMap(file, stem, MapExportKind::Campaign, paths, engine2, log);
      if (!c.ok) return "Salvo, mas a preparação para a campanha falhou (" + c.error + ").";
      const MapExportResult t = exportMap(file, stem, MapExportKind::Test, paths, engine2, log);
      if (!t.ok) return "Salvo e pronto na campanha; o modo teste falhou (" + t.error + ").";
      return "Salvo " + stem + ": a campanha e o modo teste já jogam esta versão.";
    }));
  }

  // mapstore draftToMission then missionToDraft (Carregar mapa da campanha): the board repaired (watchtower walls, rock
  // columns), test levels (heroes 15, foes by the mission's index), the location it is placed in.
  e2::Json missionToDraft(e2::Json d) const {
    const int n = d.value("cols", 0) * d.value("rows", 0);
    std::vector<std::string> tiles;
    for (const e2::Json& t : d.value("tiles", e2::Json::array())) tiles.push_back(t.is_string() ? t.get<std::string>() : "plains");
    tiles.resize(static_cast<size_t>(n), "plains");
    const std::string id = d.value("id", "");
    const std::optional<std::string> base = d.contains("baseTile") && d["baseTile"].is_string() ? std::optional(d["baseTile"].get<std::string>()) : std::nullopt;
    if (const auto f = editor_.watchtowerFloors.find(id); f != editor_.watchtowerFloors.end())
      d["title"] = std::regex_replace(d.value("title", ""), std::regex("^(?:Andar|Subsolo)\\s+[IVX\\d-]+"), std::format("Andar {}", f->second),
                                      std::regex_constants::format_first_only);
    if (id == "thebridge") d["title"] = "A Ponte de Pedra";
    const e2::Json decos = e2::closeWatchtowerWalls(id, tiles, d.value("cols", 0), d.value("rows", 0), d.value("decorations", e2::Json::array()), dress_, catalog_);
    tiles = e2::clearRockColumnTiles(tiles, d.value("cols", 0), d.value("rows", 0), decos, base, dress_, catalog_);
    tiles = e2::clearRockColumnTiles(tiles, d.value("cols", 0), d.value("rows", 0), decos, base, dress_, catalog_);
    d["tiles"] = tiles;
    d["decorations"] = decos;
    const int index = d.value("index", 0);
    e2::Json variants = e2::Json::array(), rots = e2::Json::array();
    for (int i = 0; i < n; ++i) {
      const e2::Json& v = d.contains("tileVariants") && d["tileVariants"].is_array() && i < static_cast<int>(d["tileVariants"].size()) ? d["tileVariants"][i] : e2::Json();
      const e2::Json& r = d.contains("tileRots") && d["tileRots"].is_array() && i < static_cast<int>(d["tileRots"].size()) ? d["tileRots"][i] : e2::Json();
      variants.push_back(v.is_number() ? v : e2::Json(0)), rots.push_back(r.is_number() ? r : e2::Json(0));
    }
    d["tileVariants"] = variants, d["tileRots"] = rots;
    auto levels = [&](const char* key, int level) {
      if (!d.contains(key) || !d[key].is_array()) return;
      for (e2::Json& s : d[key]) {
        if (s.value("classId", "") == "butcher") s["classId"] = "punisher";
        s["level"] = level;
      }
    };
    levels("playerSpawns", editor_.testLevel);
    levels("enemySpawns", enemyLevelFor(index));
    levels("neutralSpawns", enemyLevelFor(index));
    d["hub"] = d.contains("hub") && d["hub"].is_boolean() && d["hub"].get<bool>();
    d["autoTactics"] = !(d.contains("autoTactics") && d["autoTactics"].is_boolean() && !d["autoTactics"].get<bool>());
    d["fog"] = d.contains("fog") && d["fog"].is_boolean() && d["fog"].get<bool>();
    d["environment"] = d.value("environment", "outdoor") == "indoor" ? "indoor" : "outdoor";
    static const std::set<std::string> tods = {"noon", "dawn", "dusk", "brightNight", "darkNight"};
    d["timeOfDay"] = tods.contains(d.value("timeOfDay", "")) ? d["timeOfDay"].get<std::string>() : "day";
    auto number = [&](const char* key, double fallback) {
      if (!d.contains(key) || !d[key].is_number()) d[key] = fallback;
    };
    number("sunIntensity", editor_.defaultSun), number("ambientIntensity", editor_.defaultAmbient), number("bloomIntensity", editor_.defaultBloom);
    number("mistIntensity", 0.2), number("mistSpeed", 1), number("wispIntensity", 0.02), number("wispSpeed", 1), number("wispColor", 0xffa552);
    if (!d.contains("mistType") || !d["mistType"].is_string()) d["mistType"] = "none";
    if (!d.contains("music") || !d["music"].is_string()) d["music"] = "";
    if (!d.contains("elementalFx") || !d["elementalFx"].is_array()) d["elementalFx"] = e2::Json::array();
    if (!d.contains("neutralSpawns") || !d["neutralSpawns"].is_array()) d["neutralSpawns"] = e2::Json::array();
    d["locationId"] = locationOf(id);
    return d;
  }
  std::string locationOf(const std::string& missionId) const {
    for (const e2::Locais::Base& l : locais_.locations())
      if (std::find(l.missionIds.begin(), l.missionIds.end(), missionId) != l.missionIds.end()) return l.id;
    return "";
  }
  void loadCampaignMap(const std::string& id) {
    const std::optional<std::filesystem::path> file = newestFile(id);
    if (!file) return note("Esse mapa não tem arquivo salvo.");
    e2::Json d = missionToDraft(readDraft(*file));
    if (d.is_null()) return note("Não consegui ler " + file->generic_string() + ".");
    setDraft(d);
    note(std::format("Carregado \"{}\" ({}) no editor — {}x{}.", d.value("title", ""), id, cols(), rows()));
  }
  void openFile(const std::filesystem::path& file, const std::string& what) {
    const e2::Json d = readDraft(file);
    if (!d.is_object()) return note("Não consegui ler " + file.generic_string() + ".");
    setDraft(d);
    note(what);
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
    const std::filesystem::path engine2 = base_.engine2Public.parent_path();
    const std::string title = draft_.value("title", id), place = draft_.value("place", ""), briefing = draft_.value("briefing", ""),
                      objective = draft_.value("objective", "");
    note("Testando — Encerrar teste nas Opções traz o mapa de volta como está.");
    playtest_ = std::async(std::launch::async, [this, dir, id, file, engine2, title, place, briefing, objective]() -> std::optional<MissionEntry> {
      std::error_code ec;
      std::filesystem::create_directories(dir, ec);
      const std::filesystem::path source = dir / (id + ".draft.json");
      {
        std::ofstream f(source, std::ios::binary | std::ios::trunc);
        f << file.dump(2) << "\n";
      }
      const MapExportResult r = exportMap(source, id, MapExportKind::Playtest, paths_, engine2, dir / (id + ".log"), dir);
      if (!r.ok) {
        playtestError_ = "O teste não pôde ser preparado (" + r.error + ").";
        return std::nullopt;
      }
      MissionEntry e;
      e.id = id, e.title = title, e.place = place, e.briefing = briefing, e.objective = objective, e.draftId = id;
      e.map = r.map, e.scene = r.scene, e.battle = r.battle;
      return e;
    });
  }

  // --- the lists Engine2's pickers show ---------------------------------------------------------------------------------

  struct MapRef {
    std::string id, title;
    int index = 0;
  };
  // missionById: Engine2's mission table, else the newest saved draft.
  std::optional<MissionInfo> missionInfo(const std::string& id) const {
    if (id == draft_.value("id", "")) return MissionInfo{id, draft_.value("title", id), draft_.value("index", 0), draft_.value("hub", false)};
    for (const MissionInfo& m : editor_.missions)
      if (m.id == id) return m;
    if (const auto it = saved_.find(id); it != saved_.end()) return MissionInfo{id, it->second.title, it->second.index, it->second.hub};
    return std::nullopt;
  }
  static bool byName(const std::string& a, const std::string& b) { return lower(a) < lower(b); }
  static void sortRefs(std::vector<MapRef>& v) {
    std::sort(v.begin(), v.end(), [](const MapRef& a, const MapRef& b) { return a.index != b.index ? a.index < b.index : byName(a.title, b.title); });
  }
  std::set<std::string> campaignIds() const {
    std::set<std::string> ids;
    for (const std::string& loc : locais_.locationOrder) {
      const auto it = order_.find(loc);
      if (it != order_.end()) ids.insert(it->second.begin(), it->second.end());
      else
        for (const e2::Locais::Base& b : locais_.base)
          if (b.id == loc) ids.insert(b.missionIds.begin(), b.missionIds.end());
    }
    return ids;
  }
  // Only missions placed in a Local (not hubs): "Carregar mapa da campanha".
  std::vector<MapRef> campaignRefs() const {
    std::vector<MapRef> out;
    for (const std::string& id : campaignIds())
      if (const auto m = missionInfo(id); m && !m->hub) out.push_back({id, m->title, m->index});
    sortRefs(out);
    return out;
  }
  // Reserve maps: saved but in no Local, and Engine2's missions given to none (Locais "Adicionar mapa", R-Encounter).
  std::vector<MapRef> reserveRefs() const {
    const std::set<std::string> campaign = campaignIds();
    std::map<std::string, MapRef> known;
    for (const auto& [id, s] : saved_)
      if (!campaign.contains(id)) known[id] = {id, s.title, s.index};
    for (const MissionInfo& m : editor_.missions)
      if (!m.hub && !campaign.contains(m.id) && !known.contains(m.id)) known[m.id] = {m.id, m.title, m.index};
    std::vector<MapRef> out;
    for (auto& [id, r] : known) out.push_back(r);
    sortRefs(out);
    return out;
  }
  // The hub maps (the Inn, towns) and every floor their connectors lead to, floor after floor (the Inn's second floor is no
  // hub and sits in no Local: it is reached only by its stairs, so the editor lists it here with its Inn).
  std::vector<MapRef> cityHubs() const {
    std::map<std::string, MapRef> hubs;
    for (const MissionInfo& m : editor_.missions)
      if (m.hub) hubs[m.id] = {m.id, m.title, m.index};
    for (const auto& [id, s] : saved_)
      if (s.hub) hubs[id] = {id, s.title, s.index};
    std::vector<std::string> todo;
    for (const auto& [id, r] : hubs) todo.push_back(id);
    while (!todo.empty()) {
      const std::string id = todo.back();
      todo.pop_back();
      const auto it = saved_.find(id);
      if (it == saved_.end()) continue;
      for (const std::string& floor : it->second.floors)
        if (!hubs.contains(floor))
          if (const std::optional<MissionInfo> m = missionInfo(floor); m && saved_.contains(floor)) hubs[floor] = {floor, m->title, m->index}, todo.push_back(floor);
    }
    std::vector<MapRef> out;
    for (auto& [id, r] : hubs) out.push_back(r);
    sortRefs(out);
    return out;
  }
  // Every saved map, campaign or reserve, by name: a floor connector's target.
  std::vector<MapRef> connectorTargets() const {
    std::vector<MapRef> out = campaignRefs(), reserve = reserveRefs();
    out.insert(out.end(), reserve.begin(), reserve.end());
    std::sort(out.begin(), out.end(), [](const MapRef& a, const MapRef& b) { return byName(a.title, b.title); });
    return out;
  }

  // The dialogue editor's portrait picker: the heroes by their own names, then one entry per other sprite.
  std::vector<DialogEditorUi::Portrait> portraits() const {
    std::set<std::string> seen;
    std::vector<DialogEditorUi::Portrait> heroes, rest;
    for (const auto& [name, classId] : editor_.heroes) {
      const auto it = editor_.classes.find(classId);
      if (it == editor_.classes.end() || it->second.sprite.empty() || !seen.insert(it->second.sprite).second) continue;
      heroes.push_back({it->second.sprite, name});
    }
    for (const std::string& c : editor_.classOrder) {
      const auto it = editor_.classes.find(c);
      if (it == editor_.classes.end() || it->second.sprite.empty() || !seen.insert(it->second.sprite).second) continue;
      rest.push_back({it->second.sprite, it->second.name});
    }
    auto sorter = [](const DialogEditorUi::Portrait& a, const DialogEditorUi::Portrait& b) { return byName(a.label, b.label); };
    std::sort(heroes.begin(), heroes.end(), sorter), std::sort(rest.begin(), rest.end(), sorter);
    heroes.insert(heroes.end(), rest.begin(), rest.end());
    return heroes;
  }
  // Opens the dialogue editor on the mission's intro / outro or a neutral unit's conversation ("spawn:<index>").
  void openDialogEditor(const std::string& target) {
    std::string title;
    e2::Json tree;
    std::function<bool(const e2::Json&)> commit;
    if (target == "intro" || target == "outro") {
      const std::string key = target == "intro" ? "introDialog" : "outroDialog";
      title = target == "intro" ? "Diálogo de abertura" : "Diálogo de encerramento";
      tree = draft_.contains(key) ? draft_[key] : e2::Json();
      commit = [this, key](const e2::Json& t) {
        e2::Json next = draft_;
        if (t.is_null()) next.erase(key);
        else next[key] = t;
        beginEdit();
        draft_ = next;
        return save(next);
      };
    } else if (target.starts_with("spawn:")) {
      const size_t index = static_cast<size_t>(std::atoi(target.c_str() + 6));
      const e2::Json& list = arr("neutralSpawns");
      if (index >= list.size()) return;
      title = "Diálogo — " + list[index].value("name", "unidade");
      tree = list[index].contains("dialog") ? list[index]["dialog"] : e2::Json();
      commit = [this, index](const e2::Json& t) {
        e2::Json next = draft_;
        if (index >= next["neutralSpawns"].size()) return false;
        if (t.is_null()) next["neutralSpawns"][index].erase("dialog");
        else next["neutralSpawns"][index]["dialog"] = t;
        beginEdit();
        draft_ = next;
        return save(next);
      };
    } else {
      return;
    }
    dialogUi_.open(title, tree, std::move(commit));
  }

  // The dice: props Gerar terreno leaves out (Engine2 localStorage "ember-deco-shuffle-exclude").
  static std::filesystem::path shuffleExcludeFile() {
    const char* local = std::getenv("LOCALAPPDATA");
    return (local && *local ? std::filesystem::path(local) : std::filesystem::current_path()) / "Ember" / "editor-deco-shuffle-exclude.json";
  }
  static std::set<std::string> loadShuffleExclude() {
    std::ifstream in(shuffleExcludeFile(), std::ios::binary);
    const nlohmann::json j = in ? nlohmann::json::parse(in, nullptr, false) : nlohmann::json();
    std::set<std::string> out;
    if (j.is_array())
      for (const nlohmann::json& s : j)
        if (s.is_string()) out.insert(s.get<std::string>());
    return out;
  }
  void toggleShuffleExclude(const std::string& id) {
    if (!shuffleExclude_.erase(id)) shuffleExclude_.insert(id);
    std::error_code ec;
    std::filesystem::create_directories(shuffleExcludeFile().parent_path(), ec);
    std::ofstream(shuffleExcludeFile(), std::ios::binary | std::ios::trunc) << nlohmann::json(std::vector<std::string>(shuffleExclude_.begin(), shuffleExclude_.end())).dump();
  }

  // --- the panel (MapEditorScreen's page, left of the board) ---------------------------------------------------------------

  enum class Mode { Paint, Elevation, Water, Decoration, Architecture, Player, Enemy, Npc, Summon };

  // Engine2's muted help lines (text-muted), wrapped at the panel's edge.
  static void hint(const char* fmt, ...) IM_FMTARGS(1) {
    va_list args;
    va_start(args, fmt);
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::TextWrappedV(fmt, args);
    ImGui::PopStyleColor();
    va_end(args);
  }

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
    // the page scrolls; the action bar (notes, Testar, Salvar mapa) stays at the bottom
    const float footer = 92.0f + (bigNote_ ? 120.0f : 0.0f);
    if (scrollTo_) ImGui::SetNextWindowScroll({0, *scrollTo_});
    ImGui::BeginChild("##page", {0, -footer});
    scrollTo_.reset();
    drawToolbar();
    ImGui::Separator();
    drawSettings();
    if (ImGui::CollapsingHeader("Iluminação", ImGuiTreeNodeFlags_DefaultOpen)) drawLighting();
    drawSizeAndModes();
    drawModePanel();
    drawPreviewRow();
    drawDecorationList();
    drawUnits();
    drawFiles();
    ImGui::EndChild();
    drawFooter();
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
    // Desfazer / Refazer stay on the top bar, always visible (up to kHistory steps; Ctrl+Z / Ctrl+Shift+Z do the same).
    const float bw = 92.0f, gap = 6.0f, right = at.x + ImGui::GetContentRegionAvail().x;
    ImGui::SetCursorScreenPos({right - bw * 2 - gap, at.y + 6});
    ImGui::BeginDisabled(past_.empty());
    if (ImGui::Button("Desfazer##top", {bw, 28})) undo();
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("Desfaz a última ação (%d de até %d)", static_cast<int>(past_.size()), static_cast<int>(kHistory));
    ImGui::SameLine(0, gap);
    ImGui::BeginDisabled(future_.empty());
    if (ImGui::Button("Refazer##top", {bw, 28})) redo();
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("Refaz a ação desfeita (%d)", static_cast<int>(future_.size()));
    ImGui::SetCursorScreenPos({at.x, at.y + 40});
    ImGui::Dummy({1, 4});
  }

  void drawToolbar() {
    if (ImGui::Button("Novo")) {
      setDraft(blankDraft());
      note(std::format("Mapa novo em branco — cenário \"{}\", {}x{}.", draft_["id"].get<std::string>(), cols(), rows()));
    }
    ImGui::SameLine();
    if (ImGui::Button("Locais")) openLocais();
    ImGui::SameLine();
    if (ImGui::Button("R-Encounter")) showEncounters_ = true;
    ImGui::SameLine();
    if (ImGui::Button("Gerar terreno")) generateTerrain();
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Espalha barricadas, barrancos e terreno alto pelo mapa do tamanho atual — depois é só limpar o que não serve");
    ImGui::SetNextItemWidth(-1);
    if (ImGui::BeginCombo("##campaignmap", "Carregar mapa da campanha…")) {
      for (const MapRef& m : campaignRefs())
        if (ImGui::Selectable(std::format("{}##{}", m.title, m.id).c_str())) loadCampaignMap(m.id);
      ImGui::EndCombo();
    }
    // Abrir mapa salvo: reserve maps only (campaign maps are in the picker above), the newest save of each.
    const std::set<std::string> campaign = campaignIds();
    std::vector<const Scenario*> pickable;
    for (const auto& [id, s] : saved_)
      if (!campaign.contains(id)) pickable.push_back(&s);
    std::sort(pickable.begin(), pickable.end(), [](const Scenario* a, const Scenario* b) { return byName(a->title, b->title); });
    if (!pickable.empty()) {
      ImGui::SetNextItemWidth(-1);
      if (ImGui::BeginCombo("##savedmap", "Abrir mapa salvo…")) {
        const std::map<std::string, int> active = activeMaps();
        for (const Scenario* s : pickable) {
          const auto a = active.find(s->id);
          const std::string label = std::format("{} · {} ({} arquivo{}{})##{}", s->title, s->id, s->files.size(), s->files.size() == 1 ? "" : "s",
                                                a != active.end() ? std::format(", v{:03} ativa", a->second) : "", s->id);
          if (ImGui::Selectable(label.c_str()) && !s->files.empty())
            openFile(s->files.back().file, std::format("Aberto {} — o save mais novo de \"{}\".", s->files.back().file.filename().string(), s->id));
        }
        ImGui::EndCombo();
      }
      if (ImGui::IsItemHovered()) ImGui::SetTooltip("Abre o save mais recente desse cenário — a lista de arquivos abaixo deixa escolher outro serial");
    }
    const std::vector<MapRef> hubs = cityHubs();
    if (!hubs.empty()) {
      ImGui::SetNextItemWidth(-1);
      if (ImGui::BeginCombo("##cityhubs", "CityHubs…")) {
        for (const MapRef& m : hubs)
          if (ImGui::Selectable(std::format("{}##{}", m.title, m.id).c_str())) loadCampaignMap(m.id);
        ImGui::EndCombo();
      }
      if (ImGui::IsItemHovered()) ImGui::SetTooltip("Mapas de hub (a Estalagem, cidades) e os andares ligados a eles por escada — sem combate");
    }
  }

  // A text field bound to a draft string.
  void textField(const char* label, const char* key, bool multiline = false, bool readOnly = false, std::function<std::string(const std::string&)> filter = {}) {
    std::string value = draft_.value(key, "");
    char buffer[4096];
    std::snprintf(buffer, sizeof(buffer), "%s", value.c_str());
    ImGui::TextColored(ImColor(180, 180, 180), "%s", upperUtf8(label).c_str());
    ImGui::SetNextItemWidth(-1);
    const ImGuiInputTextFlags flags = readOnly ? ImGuiInputTextFlags_ReadOnly : 0;
    const bool changed = multiline ? ImGui::InputTextMultiline(std::format("##{}", key).c_str(), buffer, sizeof(buffer), {-1, 64}, flags)
                                   : ImGui::InputText(std::format("##{}", key).c_str(), buffer, sizeof(buffer), flags);
    if (changed) {
      beginEdit();
      draft_[key] = filter ? filter(buffer) : std::string(buffer);
      if (std::string(key) == "id") refreshFiles();
    }
  }
  bool flagOf(const char* key, bool fallback = false) const {
    return draft_.contains(key) && draft_[key].is_boolean() ? draft_[key].get<bool>() : fallback;
  }
  bool checkbox(const char* label, const char* key, bool fallback = false) {
    bool v = flagOf(key, fallback);
    if (!ImGui::Checkbox(label, &v)) return false;
    beginEdit(), draft_[key] = v;
    return true;
  }
  void slider(const char* label, const char* key, float lo, float hi, float fallback) {
    float v = draft_.contains(key) && draft_[key].is_number() ? draft_[key].get<float>() : fallback;
    ImGui::SetNextItemWidth(-110);
    if (ImGui::SliderFloat(std::format("{}##{}", label, key).c_str(), &v, lo, hi, "%.2f")) beginEdit(), draft_[key] = std::round(v * 100.0) / 100.0;
  }
  int intField(const char* label, int value, int lo, int hi, float width = 90) {
    ImGui::SetNextItemWidth(width);
    int v = value;
    if (ImGui::InputInt(label, &v, 1, 5)) v = std::clamp(v, lo, hi);
    return v;
  }

  void drawSettings() {
    textField("Cenário alvo (Id)", "id", false, false, [](const std::string& s) { return stripScenarioId(s); });
    if (ImGui::IsItemHovered())
      ImGui::SetTooltip("O cenário da campanha que essa edição mira. Bate com o id de uma missão real (ex.: o-vau) pra poder ativar essa versão nela, ou qualquer id livre pra um mapa avulso.");
    textField("Título", "title", false, draft_.value("id", "") == "thebridge");
    textField("Local", "place");
    // Trilha
    {
      const std::string music = draft_.value("music", "");
      ImGui::TextColored(ImColor(180, 180, 180), "TRILHA");
      ImGui::SetNextItemWidth(-1);
      if (ImGui::BeginCombo("##music", music.empty() ? "Tema padrão (pelo id da missão)" : music.c_str())) {
        if (ImGui::Selectable("Tema padrão (pelo id da missão)", music.empty())) beginEdit(), draft_["music"] = "", note("Trilha desta missão: a do tema padrão.");
        for (const std::string& f : editor_.music)
          if (ImGui::Selectable(f.c_str(), f == music)) beginEdit(), draft_["music"] = f, note(std::format("Trilha desta missão: {}.", f));
        ImGui::EndCombo();
      }
      hint("Os arquivos de public/game/MUSIC e de content/music, pelo nome. Toca durante o briefing, a batalha e as telas de fim.");
    }
    textField("Objetivo", "objective");
    textField("Briefing", "briefing", true);
    // Recompensa
    {
      const e2::Json reward = draft_.value("victoryReward", e2::Json::object());
      int ember = reward.value("ember", 0), rations = reward.value("rations", 0);
      ImGui::TextColored(ImColor(180, 180, 180), "RECOMPENSA · GOLD");
      ImGui::SameLine(panelWidth_ * 0.5f);
      ImGui::TextColored(ImColor(180, 180, 180), "RECOMPENSA · RAÇÕES");
      ImGui::SetNextItemWidth(panelWidth_ * 0.5f - 24);
      bool changed = ImGui::InputInt("##gold", &ember, 1, 10);
      ImGui::SameLine(panelWidth_ * 0.5f);
      ImGui::SetNextItemWidth(-1);
      changed = ImGui::InputInt("##rations", &rations, 1, 5) || changed;
      if (changed) {
        beginEdit();
        e2::Json r = draft_.value("victoryReward", e2::Json::object());
        r["ember"] = std::max(0, ember), r["rations"] = std::max(0, rations);
        draft_["victoryReward"] = r;
      }
      ImGui::TextColored(ImColor(180, 180, 180), "NPCS NECESSÁRIOS PARA A RECOMPENSA");
      std::vector<std::string> names;
      for (const e2::Json& s : arr("neutralSpawns"))
        if (s.contains("dialog") && !s["dialog"].is_null() && std::find(names.begin(), names.end(), s.value("name", "")) == names.end()) names.push_back(s.value("name", ""));
      std::vector<std::string> required;
      if (reward.contains("requiredNpcNames") && reward["requiredNpcNames"].is_array())
        for (const e2::Json& n : reward["requiredNpcNames"]) required.push_back(n.get<std::string>());
      for (const std::string& n : required)
        if (std::find(names.begin(), names.end(), n) == names.end()) names.push_back(n);
      for (const std::string& n : names) {
        bool on = std::find(required.begin(), required.end(), n) != required.end();
        if (ImGui::Checkbox((n + "##npcreq").c_str(), &on)) {
          beginEdit();
          e2::Json r = draft_.value("victoryReward", e2::Json::object());
          std::vector<std::string> next;
          for (const std::string& x : required)
            if (x != n) next.push_back(x);
          if (on) next.push_back(n);
          r["ember"] = r.value("ember", 0), r["rations"] = r.value("rations", 0), r["requiredNpcNames"] = next;
          draft_["victoryReward"] = r;
        }
      }
      hint("Pagamento ao concluir a vitória, com todos os inimigos derrotados e os NPCs indicados vivos.");
    }
    // Diálogos de abertura / encerramento
    for (const auto& [label, key, target] : {std::tuple{"Diálogo de abertura", "introDialog", "intro"}, std::tuple{"Diálogo de encerramento", "outroDialog", "outro"}}) {
      ImGui::TextColored(ImColor(180, 180, 180), "%s", upperUtf8(label).c_str());
      const std::string enabledKey = std::string(key) + "Enabled";
      bool enabled = !(draft_.contains(enabledKey) && draft_[enabledKey].is_boolean() && !draft_[enabledKey].get<bool>());
      if (ImGui::Checkbox(("Ativado##" + std::string(target)).c_str(), &enabled)) beginEdit(), draft_[enabledKey] = enabled;
      ImGui::SameLine();
      const bool has = draft_.contains(key) && !draft_[key].is_null();
      if (ImGui::SmallButton(((has ? "Editar diálogo##" : "Criar diálogo##") + std::string(target)).c_str())) openDialogEditor(target);
    }
    // Vitória
    {
      static const char* kWin[3][2] = {{"rout", "Derrote todos (rout)"}, {"boss", "Derrube o chefe (boss)"}, {"escape", "Transversal — alcance a saída (escape)"}};
      const std::string win = draft_.value("win", "rout");
      const char* shown = kWin[0][1];
      for (auto& w : kWin)
        if (win == w[0]) shown = w[1];
      ImGui::TextColored(ImColor(180, 180, 180), "VITÓRIA");
      ImGui::SetNextItemWidth(-1);
      if (ImGui::BeginCombo("##win", shown)) {
        for (auto& w : kWin)
          if (ImGui::Selectable(w[1], win == w[0])) beginEdit(), draft_["win"] = w[0];
        ImGui::EndCombo();
      }
    }
    // Local no mapa-múndi (+ its planned slots)
    {
      const std::string loc = draft_.value("locationId", "");
      auto label = [&](const e2::Locais::Base& l) {
        const int planned = locais_.slotsFor(l.id);
        const size_t n = order_.contains(l.id) ? order_.at(l.id).size() : l.missionIds.size();
        return l.name + (planned > 0 ? std::format(" ({}/{})", n, planned) : n == 0 ? std::string(" (vazio)") : std::format(" ({})", n));
      };
      std::string shown = "Nenhum — não aparece no mapa";
      for (const e2::Locais::Base& l : locais_.base)
        if (l.id == loc) shown = label(l);
      ImGui::TextColored(ImColor(180, 180, 180), "LOCAL NO MAPA-MÚNDI");
      ImGui::SetNextItemWidth(-1);
      if (ImGui::BeginCombo("##location", shown.c_str())) {
        if (ImGui::Selectable("Nenhum — não aparece no mapa", loc.empty())) beginEdit(), draft_["locationId"] = "";
        for (const e2::Locais::Base& l : locais_.base)
          if (ImGui::Selectable((label(l) + "##" + l.id).c_str(), l.id == loc)) beginEdit(), draft_["locationId"] = l.id;
        ImGui::EndCombo();
      }
      if (!loc.empty()) {
        const e2::Locais::Base* l = nullptr;
        for (const e2::Locais::Base& b : locais_.base)
          if (b.id == loc) l = &b;
        ImGui::TextColored(ImColor(180, 180, 180), "TELAS EM %s", upperUtf8(l ? l->name : loc).c_str());
        const int declared = locais_.slotsFor(loc);
        const int n = intField("##slots", declared, 0, 99, 80);
        if (n != declared) {
          if (n > 0) locais_.slots[loc] = n;
          else locais_.slots.erase(loc);
          saveLocais("Vagas do local atualizadas.");
        }
        const std::vector<std::string> ids = order_.contains(loc) ? order_.at(loc) : (l ? l->missionIds : std::vector<std::string>{});
        ImGui::SameLine();
        if (declared == 0) hint("%d feita(s) — sem plano definido", static_cast<int>(ids.size()));
        else hint("%d de %d feita(s), faltam %d", static_cast<int>(ids.size()), declared, std::max(0, declared - static_cast<int>(ids.size())));
        if (!ids.empty()) {
          std::string list;
          for (const std::string& id : ids) list += (list.empty() ? "" : ", ") + id;
          hint("Já lá: %s", list.c_str());
        }
      }
    }
    checkbox("Preservar posições iniciais (ignorar formação do grupo)", "lockPartyFormation");
    checkbox("É um hub (sem combate)", "hub");
    {
      bool square = draft_.contains("squareTiles") && draft_["squareTiles"].is_boolean() ? draft_["squareTiles"].get<bool>() : squareBorder();
      if (ImGui::Checkbox("Tiles e bordas quadradas", &square)) beginEdit(), draft_["squareTiles"] = square;
      if (ImGui::IsItemHovered()) ImGui::SetTooltip("Ligado: tiles alinhados e bordas retas para construir interiores. Desligado: piso e contorno hexagonais.");
    }
    checkbox("Exploração livre (sem turnos)", "explore");
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Sem turnos nem vitória: o líder anda livre com um clique e conversa com os NPCs (a Estalagem)");
    if (checkbox("Terreno automático", "autoTactics", true))
      note(flagOf("autoTactics") ? "Terreno automático ligado — barricadas e barrancos entram por cima do que você pintou."
                                 : "Terreno automático desligado — o mapa carrega exatamente como está pintado.");
    if (checkbox("Névoa de guerra", "fog"))
      note(flagOf("fog") ? "Névoa ligada — inimigos fora da linha de visão não aparecem nem podem ser alvo."
                         : "Névoa desligada — o mapa inteiro fica visível, como nas missões antigas.");
  }
  // mapFloor.ts hasSquareMapBorder: every non-void cell fills its bounding rectangle.
  bool squareBorder() const {
    int minX = cols(), minY = rows(), maxX = -1, maxY = -1, count = 0;
    for (int y = 0; y < rows(); ++y)
      for (int x = 0; x < cols(); ++x) {
        if (tileAt(y * cols() + x) == "void") continue;
        minX = std::min(minX, x), maxX = std::max(maxX, x), minY = std::min(minY, y), maxY = std::max(maxY, y), ++count;
      }
    return count > 0 && count == (maxX - minX + 1) * (maxY - minY + 1);
  }

  void drawLighting() {
    {
      const std::string current = draft_.contains("backgroundImage") && draft_["backgroundImage"].is_string() ? draft_["backgroundImage"].get<std::string>() : "";
      std::string shown = "Vazio preto", file;
      for (const auto& [id, label, f] : editor_.backgrounds)
        if (id == current) shown = label, file = f;
      ImGui::SetNextItemWidth(-110);
      if (ImGui::BeginCombo("Imagem de fundo", shown.c_str())) {
        if (ImGui::Selectable("Vazio preto", current.empty())) beginEdit(), draft_["backgroundImage"] = "";
        for (const auto& [id, label, f] : editor_.backgrounds)
          if (ImGui::Selectable(std::format("{}##{}", label, id).c_str(), id == current)) beginEdit(), draft_["backgroundImage"] = id;
        ImGui::EndCombo();
      }
      if (!file.empty())
        if (const UiImage* img = uiImage(base_.engine2Public / "game" / "map-backgrounds" / file)) {
          const float w = ImGui::GetContentRegionAvail().x, h = 96;
          const float aspect = static_cast<float>(img->width) / std::max(1.0f, static_cast<float>(img->height));
          // object-cover: the middle band of the image
          const float vis = std::min(1.0f, (w / h) / aspect), v0 = (1.0f - std::min(1.0f, aspect / (w / h))) * 0.5f;
          ImGui::Image(img->texture, {w, h}, {(1.0f - vis) * 0.5f, v0}, {(1.0f + vis) * 0.5f, 1.0f - v0});
        }
      hint("Pasta: public/game/map-backgrounds — JPG, PNG ou WebP");
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
                                         {"fog1", "Névoa 01 (só sobre área não revelada e o fundo, limpa no mapa revelado)"}, {"fog5", "Névoa 5 (rasteira, fiapos finos e véus)"},
                                         {"vignette", "Vinheta (tela inteira, bordas suaves)"}, {"vignette2", "Vinheta 2 (bancos de névoa profundos, centro limpo)"},
                                         {"vignette3", "Vinheta 3 (névoa rasteira em faixas, sem bordas escuras)"}, {"vignette4", "Vinheta 4 (névoa monocromática nas bordas)"}};
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
    {
      // Clima: the volumetric fog's regional profile (content/atmosphere/v1/profiles.json), saved with the map. Automático =
      // the profile whose map keys name this map (so a map needs nothing set here to get its own).
      static const std::vector<Volumetrics::Profile> profiles = Volumetrics::loadProfiles("content/atmosphere/v1/profiles.json");
      const std::string current = draft_.value("atmosphereProfile", std::string());
      std::string shown = "Automático (pelo nome do mapa)";
      for (const Volumetrics::Profile& p : profiles)
        if (p.name == current) shown = p.label;
      ImGui::SetNextItemWidth(-110);
      if (ImGui::BeginCombo("Clima", shown.c_str())) {
        if (ImGui::Selectable("Automático (pelo nome do mapa)", current.empty())) beginEdit(), draft_.erase("atmosphereProfile");
        for (const Volumetrics::Profile& p : profiles)
          if (ImGui::Selectable(p.label.c_str(), p.name == current)) beginEdit(), draft_["atmosphereProfile"] = p.name;
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
    hint("A vista 3D mostra estes ajustes na prévia; Testar também usa a iluminação salva no mapa.");
  }

  // A row of toggle buttons; true when one was pressed (its index in `pressed`).
  // The next small button on the same line when it still fits before `right` (the panel's edge), else on a new one.
  static void flow(bool first, const char* label, float right) {
    if (first) return;
    const float w = ImGui::CalcTextSize(label, nullptr, true).x + ImGui::GetStyle().FramePadding.x * 2;
    if (ImGui::GetItemRectMax().x + ImGui::GetStyle().ItemSpacing.x + w <= right) ImGui::SameLine();
  }
  static bool toggleRow(const std::vector<std::pair<std::string, bool>>& items, int& pressed) {
    bool any = false;
    const float right = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
    for (size_t k = 0; k < items.size(); ++k) {
      flow(k == 0, items[k].first.c_str(), right);
      if (items[k].second) ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(216, 216, 216, 255)), ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(17, 17, 17, 255));
      if (ImGui::SmallButton(items[k].first.c_str())) pressed = static_cast<int>(k), any = true;
      if (items[k].second) ImGui::PopStyleColor(2);
    }
    return any;
  }

  void drawSizeAndModes() {
    ImGui::Separator();
    const int c = intField("Col", cols(), editor_.minGrid, editor_.maxGrid);
    ImGui::SameLine();
    const int r = intField("Lin", rows(), editor_.minGrid, editor_.maxGrid);
    if (c != cols() || r != rows()) resize(c, r);
    ImGui::SameLine();
    int grid = -1;
    if (toggleRow({{"Hexágono", !gridSquare_}, {"Quadrado", gridSquare_}}, grid)) gridSquare_ = grid == 1;
    static const std::pair<int, const char*> kModes[] = {{0, "Terreno"}, {1, "Elevação"}, {2, "Água 3D"},  {3, "Decoração"}, {4, "3D Walls"},
                                                         {5, "3D Decorations"}, {6, "Herói"}, {7, "Inimigo"}, {8, "NPC"},       {9, "Invocação"}};
    std::vector<std::pair<std::string, bool>> items;
    for (const auto& [k, label] : kModes) {
      const bool on = k == 4 ? mode_ == Mode::Architecture && !architectureDecorations_
                      : k == 5 ? mode_ == Mode::Architecture && architectureDecorations_
                               : mode_ == modeOf(k);
      items.push_back({label, on});
    }
    int pressed = -1;
    if (toggleRow(items, pressed)) {
      turning_ = turningDeco_ = false;
      mode_ = modeOf(pressed);
      if (pressed == 4 || pressed == 5) {
        architectureDecorations_ = pressed == 5;
        decoBrush_ = pressed == 5 ? "prop-3d-tavern-table" : thickWalls_ ? "castle-3d-thick" : "wall-3d-stone";
      }
      if (pressed == 3 && !meta(decoBrush_).model3d.empty() && !editor_.decorationOrder.empty()) decoBrush_ = editor_.decorationOrder.front();
    }
    const float right = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
    if (ImGui::SmallButton("Party completa")) addHeroes(editor_.heroes);
    for (const auto& [name, classId] : editor_.heroes) {
      flow(false, name.c_str(), right);
      if (ImGui::SmallButton(name.c_str())) addHeroes({{name, classId}});
      if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", className(classId).c_str());
    }
    hint("Nível de cada um é editável na lista abaixo.");
  }
  static Mode modeOf(int k) {
    static const Mode m[] = {Mode::Paint, Mode::Elevation, Mode::Water, Mode::Decoration, Mode::Architecture, Mode::Architecture, Mode::Player, Mode::Enemy, Mode::Npc, Mode::Summon};
    return m[std::clamp(k, 0, 9)];
  }

  // A small image button for a thumbnail (an Engine2 public file); the swatch when there is none.
  bool thumbButton(const char* id, const std::string& thumb, const std::string& swatch, float size, bool selected, const std::string& tip, const char* text = nullptr) {
    const ImVec2 at = ImGui::GetCursorScreenPos();
    const bool clicked = ImGui::InvisibleButton(id, {size, size});
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const UiImage* image = thumb.empty() ? nullptr : uiImage(base_.engine2Public / thumb.substr(thumb.starts_with('/') ? 1 : 0));
    if (image) {
      const float aspect = static_cast<float>(image->width) / std::max(1.0f, static_cast<float>(image->height));
      const float w = aspect >= 1 ? size : size * aspect, h = aspect >= 1 ? size / aspect : size;
      dl->AddRectFilled(at, {at.x + size, at.y + size}, IM_COL32(28, 28, 28, 255));
      dl->AddImage(image->texture, {at.x + (size - w) / 2, at.y + (size - h) / 2}, {at.x + (size + w) / 2, at.y + (size + h) / 2});
    } else {
      unsigned c = 0x555555;
      if (swatch.size() == 7) c = static_cast<unsigned>(std::strtoul(swatch.c_str() + 1, nullptr, 16));
      dl->AddRectFilled(at, {at.x + size, at.y + size}, IM_COL32((c >> 16) & 255, (c >> 8) & 255, c & 255, 255));
    }
    if (text) {
      const ImVec2 ts = ImGui::CalcTextSize(text);
      dl->AddText({at.x + (size - ts.x) / 2, at.y + (size - ts.y) / 2}, IM_COL32(240, 240, 240, 255), text);
    }
    dl->AddRect(at, {at.x + size, at.y + size}, selected ? IM_COL32(252, 211, 77, 255) : IM_COL32(74, 74, 74, 255), 0.0f, selected ? 2.0f : 1.0f);
    if (ImGui::IsItemHovered() && !tip.empty()) ImGui::SetTooltip("%s", tip.c_str());
    return clicked;
  }

  void drawModePanel() {
    if (mode_ == Mode::Water) {
      int pick = -1;
      if (toggleRow({{"Pintar água", !waterErase_}, {"Remover água", waterErase_}}, pick)) waterErase_ = pick == 1;
      static const char* kVersions[4][2] = {{"v1", "Água V1 — clássica"}, {"v2", "Água V2 — lago realista"}, {"v3", "3D Water V3"}, {"v4", "3D Water V4"}};
      const std::string version = draft_.value("waterVersion", "v2");
      const char* shown = kVersions[1][1];
      for (auto& v : kVersions)
        if (version == v[0]) shown = v[1];
      ImGui::SetNextItemWidth(200);
      if (ImGui::BeginCombo("Versão", shown)) {
        for (auto& v : kVersions)
          if (ImGui::Selectable(v[1], version == v[0])) beginEdit(), draft_["waterVersion"] = v[0];
        ImGui::EndCombo();
      }
      ImGui::SetNextItemWidth(120);
      if (ImGui::BeginCombo("Formato", waterSquare_ ? "Quadrado" : "Redondo")) {
        if (ImGui::Selectable("Redondo", !waterSquare_)) waterSquare_ = false;
        if (ImGui::Selectable("Quadrado", waterSquare_)) waterSquare_ = true;
        ImGui::EndCombo();
      }
      ImGui::SameLine();
      ImGui::SetNextItemWidth(90);
      if (ImGui::BeginCombo("Tamanho", std::format("{}%", static_cast<int>(waterSize_ * 100)).c_str())) {
        for (double s : {0.25, 0.5, 0.75, 1.0})
          if (ImGui::Selectable(std::format("{}%", static_cast<int>(s * 100)).c_str(), s == waterSize_)) waterSize_ = s;
        ImGui::EndCombo();
      }
      ImGui::SetNextItemWidth(120);
      float level = static_cast<float>(waterLevel_);
      if (ImGui::InputFloat("Nível", &level, 0.25f, 1.0f, "%.2f")) waterLevel_ = std::clamp(static_cast<double>(level), 0.0, 12.0);
      ImGui::SameLine();
      ringCombo("Área", waterRadius_);
      hint("Superfície contínua com ondas. Use o mesmo nível para um lago; o terreno acima da água forma as margens. Ctrl+Z desfaz.");
    } else if (mode_ == Mode::Elevation) {
      int pick = -1;
      if (toggleRow({{"Elevar +1", elevationTool_ == 0}, {"Baixar −1", elevationTool_ == 1}, {"Nivelar", elevationTool_ == 2}}, pick)) elevationTool_ = pick;
      elevationLevel_ = intField("Nível", elevationLevel_, 0, 12, 90);
      ImGui::SameLine();
      ringCombo("Área", elevationRadius_);
      hint("Clique para esculpir. Níveis 0–12; Nivelar em 0 remove a elevação. Ctrl+Z desfaz.");
    } else if (mode_ == Mode::Paint) {
      drawPaintPanel();
    } else if (mode_ == Mode::Decoration || mode_ == Mode::Architecture) {
      drawDecorationPanel();
    } else {
      if (mode_ == Mode::Player || mode_ == Mode::Enemy)
        hint("Clique numa casa vazia pra adicionar %s; clique numa casa ocupada (do mesmo lado) pra remover. Edite nome/classe na lista abaixo.",
                            mode_ == Mode::Player ? "um herói" : "um inimigo");
      if (mode_ == Mode::Npc) {
        std::string shown = npcBrush_;
        for (const auto& [id, name] : editor_.npcs)
          if (id == npcBrush_) shown = name;
        ImGui::Text("Personagem");
        ImGui::SetNextItemWidth(-1);
        if (ImGui::BeginCombo("##npc", shown.c_str())) {
          for (const auto& [id, name] : editor_.npcs)
            if (ImGui::Selectable((name + "##" + id).c_str(), id == npcBrush_)) npcBrush_ = id;
          ImGui::EndCombo();
        }
        hint("Os novos personagens têm animação de quatro quadros e diálogo próprio.");
      }
      if (mode_ == Mode::Summon) {
        ImGui::TextColored(ImColor(180, 180, 180), "LADO");
        ImGui::SameLine();
        int pick = -1;
        if (toggleRow({{"Aliada", !summonNeutral_}, {"Neutra", summonNeutral_}}, pick)) summonNeutral_ = pick == 1;
        ImGui::SameLine();
        ImGui::TextColored(ImColor(180, 180, 180), "TIPO");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(-1);
        if (ImGui::BeginCombo("##summon", classLabel(summonBrush_).c_str())) {
          for (const std::string& c : editor_.classOrder) {
            if (!summonNeutral_ && !isSummon(c)) continue;  // allies: summons only; neutral: any class (a talkable NPC)
            if (ImGui::Selectable((classLabel(c) + "##" + c).c_str(), c == summonBrush_)) summonBrush_ = c;
          }
          ImGui::EndCombo();
        }
        hint("Clique numa casa vazia pra pôr %s %s; clique numa casa ocupada desse lado pra remover.", lower(className(summonBrush_)).c_str(),
                            summonNeutral_ ? "como fera neutra" : "do lado aliado");
        ImGui::TextWrapped("%s", summonNeutral_ ? "Neutras ficam paradas: não entram na ordem de turno e não contam pra limpar o mapa. Atacar uma acorda o bando inteiro da "
                                                  "mesma classe, que vira inimigo e passa a agir na rodada seguinte — a menos que ela tenha um Diálogo (veja a lista de "
                                                  "unidades abaixo): aí não pode ser atacada, e clicar nela conversa em vez de brigar."
                                                : "Aliadas não contam na derrota — perder todas não perde a missão.");
      }
    }
  }
  static void ringCombo(const char* label, int& radius) {
    ImGui::SetNextItemWidth(120);
    auto name = [](int r) { return r == 0 ? std::string("Uma célula") : r == 1 ? std::string("1 anel") : std::format("{} anéis", r); };
    if (ImGui::BeginCombo(label, name(radius).c_str())) {
      for (int r = 0; r <= 3; ++r)
        if (ImGui::Selectable(name(r).c_str(), r == radius)) radius = r;
      ImGui::EndCombo();
    }
  }

  void drawPaintPanel() {
    hint("Escolha um terreno ou grupo de tiles para pintar o mapa.");
    ImGui::SameLine();
    if (ImGui::SmallButton(turning_ ? "Girando — clique num hex" : "Girar hex")) {
      note(turning_ ? "Pincel de volta: clicar pinta o terreno."
                    : "Girar armado: cada clique num hex vira o desenho 60°, sem trocar o terreno. O ponto vermelho mostra onde é o lado de baixo.");
      turning_ = !turning_;
    }
    if (ImGui::IsItemHovered())
      ImGui::SetTooltip("Gira o desenho do hex 60° por clique, para casar costa, estrada e muro com o vizinho. O terreno e a variante não mudam; seis cliques voltam ao original.");
    // the terrains (City is plains with the city layers), by name; snow has its own Icelands section
    std::vector<std::tuple<std::string, std::string, std::string, std::string, std::string, std::string>> items;  // key, terrain, label, thumb, swatch, hint
    for (const TerrainEntry& t : editor_.terrains)
      if (t.id != "snow") items.emplace_back(t.id, t.id, t.name, t.thumb, t.swatch, t.hint);
    if (!editor_.city.empty()) items.emplace_back("city", "plains", "City", editor_.city[0].thumb, "#9c8f6f", "City");
    std::sort(items.begin(), items.end(), [](const auto& a, const auto& b) { return byName(std::get<2>(a), std::get<2>(b)); });
    auto pickTerrain = [&](const std::string& key, const std::string& terrain) {
      brush_ = terrain;
      if (key == "city") {
        cityMode_ = true;
        if (std::none_of(editor_.city.begin(), editor_.city.end(), [&](const Version& v) { return v.variant == variant_; })) variant_ = editor_.city[0].variant;
      } else {
        cityMode_ = false;
        variant_ = std::min(variant_, variantCount(terrain) - 1);
        if (terrain == "plains" && std::any_of(editor_.city.begin(), editor_.city.end(), [&](const Version& v) { return v.variant == variant_; })) variant_ = 0;
      }
    };
    int n = 0;
    const float columnWidth = 150;
    const int perRow = std::max(1, static_cast<int>(ImGui::GetContentRegionAvail().x / columnWidth));
    for (const auto& [key, terrain, label, thumb, swatch, hint] : items) {
      const bool selected = key == "city" ? cityMode_ && brush_ == "plains" : !cityMode_ && brush_ == terrain;
      if (n % perRow) ImGui::SameLine(static_cast<float>(n % perRow) * columnWidth + ImGui::GetStyle().WindowPadding.x);
      ImGui::BeginGroup();
      if (thumbButton(("##t" + key).c_str(), thumb, swatch, 28, selected, hint)) pickTerrain(key, terrain);
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
      ImGui::BeginChild("##versions", {0, 0}, ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY);
      float x = 0;
      const float avail = ImGui::GetContentRegionAvail().x;
      for (const Version& v : *versions) {
        const std::string text = v.label;
        const float w = 34 + ImGui::CalcTextSize(text.c_str()).x + 12;
        if (x > 0 && x + w < avail) ImGui::SameLine();
        else x = 0;
        ImGui::BeginGroup();
        if (thumbButton(std::format("##v{}", v.variant).c_str(), v.thumb, brush_ == "plains" ? "#5f8c28" : swatch, 30, variant_ == v.variant,
                        std::format("{} {}", cityMode_ ? std::string("City ·") : terrainName(brush_), v.label)))
          variant_ = v.variant;
        ImGui::SameLine();
        ImGui::TextUnformatted(text.c_str());
        ImGui::EndGroup();
        x += w;
      }
      ImGui::EndChild();
    }
    if (brush_ == "hill") hint("Colina cria relevo. Use Elevação para esculpir vários níveis e ver o resultado na prévia 3D.");
    // Icelands
    ImGui::BeginChild("##icelands", {0, 0}, ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY);
    ImGui::TextColored(ImColor(180, 180, 180), "ICELANDS");
    ImGui::SameLine();
    for (const TerrainEntry& t : editor_.terrains)
      if (t.id == "snow") {
        if (thumbButton("##tsnow", t.thumb, t.swatch, 28, !cityMode_ && brush_ == "snow", t.hint)) pickTerrain("snow", "snow");
        ImGui::SameLine();
        ImGui::TextUnformatted("Neve e gelo");
      }
    ImGui::EndChild();
  }

  // MapEditorScreen architectureOptions: 3D walls (regular or thick, doors and openings) or 3D decorations; the
  // watchtower entrance only on watchtower maps; walls last.
  std::vector<std::string> architectureOptions() const {
    std::vector<std::string> out;
    const bool watchtower = draft_.value("id", "").starts_with("watchtower-");
    for (const std::string& id : editor_.architectureOrder) {
      const DecoMeta& m = meta(id);
      if (m.model3d.empty() || m.props3d != architectureDecorations_) continue;
      if (id == kEntrance && !watchtower) continue;
      if (!architectureDecorations_ && m.thickWall != thickWalls_) continue;
      out.push_back(id);
    }
    std::stable_sort(out.begin(), out.end(), [&](const std::string& a, const std::string& b) { return (meta(a).model3d == "wall") < (meta(b).model3d == "wall"); });
    return out;
  }

  void drawDecorationPanel() {
    const bool arch = mode_ == Mode::Architecture;
    if (arch && !architectureDecorations_) {
      int pick = -1;
      if (toggleRow({{"Regular Walls", !thickWalls_}, {"Thick Walls", thickWalls_}}, pick)) {
        thickWalls_ = pick == 1;
        decoBrush_ = thickWalls_ ? "castle-3d-thick" : "wall-3d-stone";
      }
    }
    ImGui::TextWrapped("%s", arch ? "Coloque paredes em linhas e colunas para formar salas retangulares contínuas. Paredes e portas fechadas bloqueiam movimento e "
                                    "visão; passagens abertas permitem atravessar. Selecione uma peça e use Delete para remover."
                                  : "Clique na casa âncora pra colocar; clique em qualquer casa que a decoração cubra pra remover. Toda casa coberta fica "
                                    "intransponível e bloqueia visão/tiro, não importa o terreno por baixo.");
    if (ImGui::Button(turningDeco_ ? (arch ? "Girando — clique numa peça 3D" : "Girando — clique numa decoração") : "Girar objeto")) {
      note(turningDeco_ ? (arch ? "Pincel 3D ativo: clique para colocar ou selecionar uma peça." : "Pincel de volta: clicar coloca e remove decoração.")
                        : (arch ? "Girar peça 3D armado: cada clique vira a peça 90°. Quatro cliques voltam ao original."
                                : "Girar objeto armado: cada clique numa decoração vira ela 60°, com toda a área junto. Seis cliques voltam ao original."));
      turningDeco_ = !turningDeco_;
    }
    if (!arch) {
      hint("O dado em cada uma liga/desliga se ela pode sair no sorteio de \"Gerar terreno\" — aceso participa, apagado só entra no mapa se você colocar à mão.");
      ImGui::SetNextItemWidth(170);
      if (ImGui::BeginCombo("Decorações", decoSection_.c_str())) {
        for (const std::string& s : editor_.decorationSections)
          if (ImGui::Selectable(s.c_str(), s == decoSection_)) decoSection_ = s;
        ImGui::EndCombo();
      }
      ImGui::SameLine();
      ImGui::SetNextItemWidth(-1);
      ImGui::InputTextWithHint("##decosearch", "Procurar…", search_, sizeof(search_));
    }
    if (const e2::CatalogDecoration* d = catalog_.decoration(decoBrush_)) ImGui::Text("Pincel: %s", d->name.c_str());
    // the palette
    std::vector<std::string> visible;
    if (arch) {
      visible = architectureOptions();
    } else {
      const std::string query = lower(search_);
      for (const std::string& id : editor_.decorationOrder) {
        if (decoSection_ != "Todas" && editor_.decoSection[id] != decoSection_) continue;
        if (!query.empty() && lower(decoName(id)).find(query) == std::string::npos && id.find(query) == std::string::npos) continue;
        visible.push_back(id);
      }
    }
    ImGui::BeginChild("##decos", {-1, arch ? 200.0f : 240.0f}, ImGuiChildFlags_Borders);
    const float cell = 64, per = std::max(1.0f, std::floor((ImGui::GetContentRegionAvail().x + 6) / (cell + 8)));
    int k = 0;
    for (const std::string& id : visible) {
      const DecoMeta& m = meta(id);
      if (k % static_cast<int>(per)) ImGui::SameLine();
      ImGui::BeginGroup();
      const bool model = !m.model3d.empty();
      const std::string thumb = model ? m.wallTexture : editor_.decoThumb[id];
      if (thumbButton(("##d" + id).c_str(), thumb, "#333333", cell, decoBrush_ == id, std::format("{} · {} hexes", decoName(id), catalog_.decoration(id) ? catalog_.decoration(id)->footprint.size() : 0),
                      model && m.wallTexture.empty() ? "3D" : nullptr))
        decoBrush_ = id, selected_.reset(), turningDeco_ = false;
      if (!model) {
        // the dice: in Gerar terreno's draw or out of it
        const ImVec2 corner = ImGui::GetItemRectMax();
        const bool excluded = shuffleExclude_.contains(id);
        const ImVec2 d0{corner.x - 19, corner.y - 19};
        ImGui::SetCursorScreenPos(d0);
        if (ImGui::InvisibleButton(("##dice" + id).c_str(), {17, 17})) toggleShuffleExclude(id);
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImU32 ink = excluded ? IM_COL32(150, 150, 150, 255) : IM_COL32(252, 211, 77, 255);
        dl->AddRectFilled(d0, {d0.x + 17, d0.y + 17}, IM_COL32(17, 17, 17, 230), 3.0f);
        dl->AddRect(d0, {d0.x + 17, d0.y + 17}, ink, 3.0f, 1.5f);
        for (const ImVec2 pip : {ImVec2{4.5f, 4.5f}, ImVec2{12.5f, 4.5f}, ImVec2{8.5f, 8.5f}, ImVec2{4.5f, 12.5f}, ImVec2{12.5f, 12.5f}})
          dl->AddCircleFilled({d0.x + pip.x, d0.y + pip.y}, 1.6f, ink);
        if (ImGui::IsItemHovered())
          ImGui::SetTooltip("%s", excluded ? "Fora do sorteio de \"Gerar terreno\" — clique pra incluir" : "No sorteio de \"Gerar terreno\" — clique pra excluir");
      }
      ImGui::EndGroup();
      ++k;
    }
    ImGui::EndChild();
    drawRules(arch);
  }

  // Regras da decoração / da peça 3D, and a floor connector's Passagem de andar.
  void drawRules(bool arch) {
    e2::Json* p = selectedPlacement();
    ImGui::BeginChild("##rules", {0, 0}, ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY);
    ImGui::TextColored(ImColor(180, 180, 180), "%s", arch ? "REGRAS DA PEÇA 3D" : "REGRAS DA DECORAÇÃO");
    ImGui::SameLine();
    if (p) hint("%s em %d,%d", decoName(p->value("id", "")).c_str(), p->value("x", 0), p->value("y", 0));
    else hint("%s", arch ? "clique numa peça 3D no mapa" : "clique numa decoração no mapa");
    const std::string model = p ? meta(p->value("id", "")).model3d : std::string();
    if (arch) {
      const bool vertical = !model.empty() ? (p->contains("wallOrientation") && (*p)["wallOrientation"].is_string() ? (*p)["wallOrientation"] == "vertical" : p->value("rot", 0) % 2 == 1)
                                           : wallVertical_;
      int pick = -1;
      if (toggleRow({{"Horizontal", !vertical}, {"Vertical", vertical}}, pick)) {
        wallVertical_ = pick == 1;
        if (p && !model.empty()) {
          beginEdit();
          (*p)["rot"] = wallVertical_ ? 1 : 0, (*p)["wallOrientation"] = wallVertical_ ? "vertical" : "horizontal";
          selected_ = e2::Json{{"id", (*p)["id"]}, {"x", (*p)["x"]}, {"y", (*p)["y"]}, {"rot", (*p)["rot"]}};
          cleanEntrances();
          p = selectedPlacement();
        }
      }
      if (p && (model == "door" || model == "doorway" || model == "secretDoor") && p->value("id", "") != kEntrance) {
        const std::string style = meta(p->value("id", "")).doorStyle.empty() ? "oak" : meta(p->value("id", "")).doorStyle;
        const auto pair = editor_.doorVariants.find(style);
        const bool closed = model == "door" || model == "secretDoor";
        if (pair != editor_.doorVariants.end() && ImGui::Button(closed ? "Abrir porta" : "Fechar passagem")) {
          beginEdit();
          (*p)["id"] = closed ? pair->second.first : pair->second.second;
          p->erase("blocksPath");
          (*selected_)["id"] = (*p)["id"];
        }
      }
      hint("Escolha a orientação para colocar novas peças ou mudar a peça selecionada. Paredes, portas fechadas e passagens secretas bloqueiam o caminho; vãos abertos permitem atravessar.");
    } else {
      const DecoMeta& m = p ? meta(p->value("id", "")) : DecoMeta{};
      const bool forced = p && (m.solidHouse || m.solidCart || model == "wall" || model == "door" || model == "secretDoor");
      bool blocks = p && ((p->contains("blocksPath") && (*p)["blocksPath"].is_boolean() && (*p)["blocksPath"].get<bool>()) || forced);
      ImGui::BeginDisabled(!p || forced);
      if (ImGui::Checkbox("Bloquear caminho", &blocks)) toggleDecorationRule("blocksPath");
      ImGui::EndDisabled();
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("Ligado, o hexágono deixa de ser navegável. Nesta engine sólido é sólido: também passa a barrar flecha e névoa.");
      bool high = p && p->contains("yieldsHighGround") && (*p)["yieldsHighGround"].is_boolean() && (*p)["yieldsHighGround"].get<bool>();
      ImGui::BeginDisabled(!p);
      if (ImGui::Checkbox("Alto terreno", &high)) toggleDecorationRule("yieldsHighGround");
      ImGui::EndDisabled();
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("Ligado, quem estiver no hexágono recebe os bônus de terreno alto: +2 de dano, +1 de alcance para arco. Com Bloquear caminho também ligado vira "
                          "rochedo — ninguém sobe, e flecha de quem está embaixo não passa por cima.");
      hint("Os dois só acrescentam: desligados, o hexágono mantém a regra do terreno que está embaixo. Uma barricada segue intransponível com "
                          "\"Bloquear caminho\" desligado, porque é a definição dela que a torna sólida.");
    }
    ImGui::EndChild();
    if (p && meta(p->value("id", "")).exitKind == "connector") {
      ImGui::BeginChild("##connector", {0, 0}, ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY);
      ImGui::TextColored(ImColor(180, 180, 180), "PASSAGEM DE ANDAR");
      hint("Leva para");
      const std::string target = p->contains("targetMapId") && (*p)["targetMapId"].is_string() ? (*p)["targetMapId"].get<std::string>() : "";
      std::string shown = "Escolha um mapa…";
      const std::vector<MapRef> targets = connectorTargets();
      for (const MapRef& m : targets)
        if (m.id == target) shown = m.title + " · " + m.id;
      ImGui::SetNextItemWidth(-1);
      if (ImGui::BeginCombo("##leva", shown.c_str())) {
        if (ImGui::Selectable("Escolha um mapa…", target.empty())) beginEdit(), p->erase("targetMapId");
        for (const MapRef& m : targets)
          if (ImGui::Selectable(std::format("{} · {}##{}", m.title, m.id, m.id).c_str(), m.id == target)) beginEdit(), (*p)["targetMapId"] = m.id;
        ImGui::EndCombo();
      }
      bool back = p->contains("returnConnector") && (*p)["returnConnector"].is_boolean() && (*p)["returnConnector"].get<bool>();
      if (ImGui::Checkbox("Volta para o andar anterior (em vez de avançar)", &back)) {
        beginEdit();
        if (back) (*p)["returnConnector"] = true;
        else p->erase("returnConnector");
      }
      if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Desligado: o resultado da batalha mostra \"Avançar\" (mais fundo na masmorra). Ligado: mostra \"Voltar\" (andar anterior). Puramente o texto/sentido "
                          "mostrado ao jogador — quem decide qual mapa liga a qual é o campo \"Leva para\" acima.");
      ImGui::EndChild();
    }
  }

  // "Prévia com os gráficos do jogo": Substituir base, Desfazer / Refazer, the preview and the technical grid switches.
  void drawPreviewRow() {
    ImGui::Separator();
    ImGui::TextColored(ImColor(180, 180, 180), "PRÉVIA COM OS GRÁFICOS DO JOGO");
    const int v = std::min(variant_, variantCount(brush_) - 1);
    if (ImGui::SmallButton(std::format("Substituir base · {}", baseVersionLabel(brush_, v)).c_str())) {
      beginEdit();
      const int n = cols() * rows();
      draft_["baseTile"] = brush_, draft_["baseVariant"] = v;
      draft_["tiles"] = e2::Json(std::vector<std::string>(n, brush_));
      draft_["tileVariants"] = e2::Json(std::vector<int>(n, v));
      draft_["tileRots"] = e2::Json(std::vector<int>(n, 0));
      note(std::format("Base inteira substituída por {}.", baseVersionLabel(brush_, v)));
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Preenche todos os hexes com %s", baseVersionLabel(brush_, v).c_str());
    ImGui::SameLine();
    ImGui::BeginDisabled(past_.empty());
    if (ImGui::SmallButton("Desfazer")) undo();
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(future_.empty());
    if (ImGui::SmallButton("Refazer")) redo();
    ImGui::EndDisabled();
    if (ImGui::SmallButton(showPreview_ ? "Ocultar prévia" : "Mostrar prévia")) {
      showPreview_ = !showPreview_;
      if (showPreview_) dirty_ = true, lastEdit_ = {};
      else preview_.reset();
    }
    ImGui::SameLine();
    if (ImGui::SmallButton(showTechnical_ ? "Ocultar mapa técnico" : "Mostrar mapa técnico")) showTechnical_ = !showTechnical_;
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Mostrar ou ocultar a grade técnica de edição do mapa");
    hint("Na prévia: botão direito (ou esquerdo) numa unidade ou decoração e arraste para mover; Delete com a unidade segura a remove.");
  }

  // The technical grid: every hex in its terrain's colour, the art's serial, a red dot on its bottom side (the turn),
  // P / E / S / N units by side and D for props; a click turns the hex (Terreno) or the prop (Decoração).
  void drawTechnicalMap() {
    static const std::map<std::string, ImU32> kSwatch = {
        {"plains", IM_COL32(156, 143, 111, 255)}, {"woods", IM_COL32(63, 92, 58, 255)},    {"ruins", IM_COL32(107, 101, 96, 255)}, {"water", IM_COL32(44, 95, 122, 255)},
        {"ember", IM_COL32(122, 44, 44, 255)},    {"hill", IM_COL32(138, 122, 79, 255)},   {"flame", IM_COL32(181, 80, 31, 255)}, {"column", IM_COL32(74, 74, 82, 255)},
        {"nave", IM_COL32(38, 38, 44, 255)},      {"barricade", IM_COL32(90, 70, 48, 255)}, {"door", IM_COL32(74, 53, 36, 255)},  {"void", IM_COL32(0, 0, 0, 255)},
        {"snow", IM_COL32(216, 222, 226, 255)}};
    const ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos({panelWidth_ + 12, 60}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize({io.DisplaySize.x - panelWidth_ - 24, io.DisplaySize.y * 0.6f}, ImGuiCond_FirstUseEver);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(0, 0, 0, 240));
    if (!ImGui::Begin("Mapa técnico de edição", &showTechnical_, ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_NoSavedSettings)) {
      ImGui::End();
      ImGui::PopStyleColor();
      return;
    }
    std::map<std::pair<int, int>, std::pair<char, ImU32>> units;
    for (const auto& [key, side] : {std::pair{"playerSpawns", 'p'}, std::pair{"enemySpawns", 'e'}, std::pair{"neutralSpawns", 'n'}})
      for (const e2::Json& s : arr(key)) {
        const std::pair<int, int> at{s.value("x", 0), s.value("y", 0)};
        if (units.contains(at)) continue;
        const ImU32 ink = side == 'p' ? IM_COL32(125, 211, 252, 255) : side == 'n' ? IM_COL32(52, 211, 153, 255) : IM_COL32(248, 113, 113, 255);
        units[at] = {isSummon(s.value("classId", "")) ? 'S' : side == 'p' ? 'P' : side == 'n' ? 'N' : 'E', ink};
      }
    std::map<std::pair<int, int>, std::string> decoAt;
    for (const e2::Json& p : arr("decorations"))
      for (const auto& c : cellsOf(p)) decoAt[c] = decoName(p.value("id", ""));
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const float hr = 32, sq3 = std::sqrt(3.0f), cellPx = 56;
    const ImVec2 size = gridSquare_ ? ImVec2{cols() * (cellPx + 1), rows() * (cellPx + 1)} : ImVec2{hr * sq3 * (cols() + 0.5f), hr * (1.5f * (rows() - 1) + 2)};
    ImGui::InvisibleButton("##techboard", {std::max(1.0f, size.x), std::max(1.0f, size.y)});
    const bool clicked = ImGui::IsItemClicked();
    const ImVec2 mouse = io.MousePos;
    std::optional<std::pair<int, int>> under;
    for (int y = 0; y < rows(); ++y)
      for (int x = 0; x < cols(); ++x) {
        const int i = y * cols() + x;
        const std::string t = tileAt(i);
        const ImU32 fill = kSwatch.contains(t) ? kSwatch.at(t) : IM_COL32(85, 85, 85, 255);
        const bool deco = decoAt.contains({x, y});
        ImVec2 c;
        float half;
        if (gridSquare_) {
          const ImVec2 a{origin.x + x * (cellPx + 1), origin.y + y * (cellPx + 1)};
          dl->AddRectFilled(a, {a.x + cellPx, a.y + cellPx}, fill);
          if (deco) dl->AddRect(a, {a.x + cellPx, a.y + cellPx}, IM_COL32(251, 191, 36, 204), 0.0f, 2.0f);
          c = {a.x + cellPx / 2, a.y + cellPx / 2}, half = 21;
          if (mouse.x >= a.x && mouse.x < a.x + cellPx && mouse.y >= a.y && mouse.y < a.y + cellPx) under = {x, y};
        } else {
          c = {origin.x + hr * sq3 * (x + 0.5f * (y & 1) + 0.5f), origin.y + hr * (1.5f * y + 1)};
          ImVec2 pts[6];
          for (int k = 0; k < 6; ++k) {
            const float a = std::numbers::pi_v<float> / 180.0f * (60.0f * k - 90.0f);
            pts[k] = {c.x + hr * std::cos(a), c.y + hr * std::sin(a)};
          }
          dl->AddConvexPolyFilled(pts, 6, fill);
          dl->AddPolyline(pts, 6, deco ? IM_COL32(251, 191, 36, 255) : IM_COL32(0, 0, 0, 51), 1.0f, ImDrawFlags_Closed);
          half = hr * 0.66f;
          if (std::hypot(mouse.x - c.x, mouse.y - c.y) < hr * 0.87f) under = {x, y};
        }
        const std::string serial = std::format("{:03}", intAt("tileVariants", i) + 1);
        const ImVec2 ts = ImGui::CalcTextSize(serial.c_str());
        dl->AddText({c.x - ts.x / 2 + 1, c.y - ts.y / 2 + 1}, IM_COL32(0, 0, 0, 240), serial.c_str());
        dl->AddText({c.x - ts.x / 2, c.y - ts.y / 2}, IM_COL32(255, 255, 255, 255), serial.c_str());
        const float a = std::numbers::pi_v<float> / 2 + (intAt("tileRots", i) % 6) * std::numbers::pi_v<float> / 3;
        dl->AddCircleFilled({c.x + std::cos(a) * half, c.y + std::sin(a) * half}, 3.5f, IM_COL32(239, 68, 68, 255));
        if (const auto u = units.find({x, y}); u != units.end()) {
          const char g[2] = {u->second.first, 0};
          dl->AddText({c.x - 4, c.y + 6}, u->second.second, g);
        } else if (deco) {
          dl->AddText({c.x - 4, c.y + 6}, IM_COL32(252, 211, 77, 255), "D");
        }
      }
    if (under && ImGui::IsItemHovered()) {
      const auto [x, y] = *under;
      if (decoAt.contains({x, y})) ImGui::SetTooltip("%s", decoAt.at({x, y}).c_str());
      else ImGui::SetTooltip("%s", variantName(tileAt(y * cols() + x), intAt("tileVariants", y * cols() + x)).c_str());
    }
    if (clicked && under) technicalClick(under->first, under->second);
    ImGui::End();
    ImGui::PopStyleColor();
  }
  // onTechnicalClick: the fast turn gesture (Terreno: the hex's art; Decoração: the prop); otherwise the brush.
  void technicalClick(int x, int y) {
    if (mode_ == Mode::Paint) {
      const bool was = turning_;
      turning_ = true;
      onCellClick(x, y, std::nullopt);
      turning_ = was;
    } else if (mode_ == Mode::Decoration || mode_ == Mode::Architecture) {
      turnDecoration(x, y);
    } else {
      onCellClick(x, y, std::nullopt);
    }
  }

  // Decorações (N): every placed prop, its thumbnail, hex and a remove button.
  void drawDecorationList() {
    e2::Json& decos = arr("decorations");
    if (decos.empty()) return;
    if (!ImGui::CollapsingHeader(std::format("Decorações ({})###decolist", decos.size()).c_str())) return;
    std::optional<size_t> remove;
    for (size_t k = 0; k < decos.size(); ++k) {
      const e2::Json& p = decos[k];
      const std::string id = p.value("id", "");
      ImGui::PushID(static_cast<int>(k));
      const DecoMeta& m = meta(id);
      const std::string thumb = !m.model3d.empty() ? m.wallTexture : m.art.empty() ? editor_.decoThumb[id] : m.art;
      thumbButton("##dl", thumb, "#333333", 22, false, "", !m.model3d.empty() && m.wallTexture.empty() ? "3D" : nullptr);
      ImGui::SameLine();
      ImGui::Text("%s", decoName(id).c_str());
      ImGui::SameLine(ImGui::GetContentRegionAvail().x - 40);
      hint("%d,%d", p.value("x", 0), p.value("y", 0));
      ImGui::SameLine();
      if (ImGui::SmallButton("X")) remove = k;
      ImGui::PopID();
    }
    if (remove) beginEdit(), decos.erase(*remove), selected_.reset();
  }

  // The spawn lists in Engine2's groups (heroes, allied summons, enemies, neutral beasts, neutral summons): name, class,
  // level; Sortear classe for enemies, a neutral's dialogue, remove.
  void drawUnits() {
    static const std::tuple<const char*, bool, const char*> kGroups[] = {{"playerSpawns", false, "Heróis"},           {"playerSpawns", true, "Invocações aliadas"},
                                                                         {"enemySpawns", false, "Inimigos"},          {"neutralSpawns", false, "Feras neutras"},
                                                                         {"neutralSpawns", true, "Invocações neutras"}};
    for (const auto& [key, summons, label] : kGroups) {
      e2::Json& list = arr(key);
      std::vector<size_t> rowsOf;
      for (size_t k = 0; k < list.size(); ++k)
        if (isSummon(list[k].value("classId", "")) == summons) rowsOf.push_back(k);
      if (rowsOf.empty()) continue;
      ImGui::TextColored(ImColor(180, 180, 180), "%s (%d)", upperUtf8(label).c_str(), static_cast<int>(rowsOf.size()));
      const bool enemy = std::string(key) == "enemySpawns", neutral = std::string(key) == "neutralSpawns";
      for (size_t k : rowsOf) {
        e2::Json& s = list[k];
        ImGui::PushID(std::format("{}{}", key, k).c_str());
        hint("%d,%d", s.value("x", 0), s.value("y", 0));
        ImGui::SameLine(48);
        char name[128];
        std::snprintf(name, sizeof(name), "%s", s.value("name", "").c_str());
        // the name takes what the class, level and buttons leave
        const ImGuiStyle& st = ImGui::GetStyle();
        const float buttons = ImGui::CalcTextSize("X").x + st.FramePadding.x * 2 + st.ItemSpacing.x +
                              (enemy ? ImGui::CalcTextSize("Sortear").x : neutral ? ImGui::CalcTextSize("+ Diálogo").x : -st.ItemSpacing.x - st.FramePadding.x * 2) +
                              st.FramePadding.x * 2 + st.ItemSpacing.x;
        const float classW = 150, levelW = 40 + ImGui::CalcTextSize("Nv").x + st.ItemSpacing.x * 2;
        ImGui::SetNextItemWidth(std::max(60.0f, ImGui::GetContentRegionAvail().x - classW - levelW - buttons - st.ItemSpacing.x * 2));
        if (ImGui::InputText("##name", name, sizeof(name))) beginEdit(), s["name"] = std::string(name);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(classW);
        const std::string classId = s.value("classId", "");
        if (ImGui::BeginCombo("##class", classLabel(classId).c_str())) {
          for (const std::string& c : editor_.classOrder) {
            if (enemy && (isSummon(c) || isCivil(c))) continue;
            if (ImGui::Selectable((classLabel(c) + "##" + c).c_str(), c == classId)) beginEdit(), s["classId"] = c, s["useClassSprite"] = true;
          }
          ImGui::EndCombo();
        }
        ImGui::SameLine();
        hint("Nv");
        ImGui::SameLine();
        int level = s.value("level", editor_.testLevel);
        ImGui::SetNextItemWidth(40);
        if (ImGui::InputInt("##lv", &level, 0)) beginEdit(), s["level"] = std::clamp(level, 1, editor_.maxLevel);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Nível (só afeta o Testar)");
        if (enemy) {
          ImGui::SameLine();
          if (ImGui::SmallButton("Sortear")) {
            // never a named hero's own class for a random foe
            std::vector<std::string> pool;
            for (const std::string& c : editor_.classOrder) {
              if (isSummon(c) || isCivil(c)) continue;
              bool named = false;
              for (const auto& h : editor_.heroes) named = named || h.second == c;
              for (const auto& h : editor_.namedNonHeroes) named = named || h.second == c;
              if (!named) pool.push_back(c);
            }
            if (!pool.empty()) {
              static std::mt19937 rng{std::random_device{}()};
              beginEdit(), s["classId"] = pool[rng() % pool.size()], s["useClassSprite"] = true;
            }
          }
          if (ImGui::IsItemHovered()) ImGui::SetTooltip("Sortear uma classe inimiga aleatória");
        }
        if (neutral) {
          ImGui::SameLine();
          if (ImGui::SmallButton(s.contains("dialog") && !s["dialog"].is_null() ? "Diálogo" : "+ Diálogo")) pendingDialog_ = std::format("spawn:{}", k);
          if (ImGui::IsItemHovered()) ImGui::SetTooltip("Editar o diálogo desta unidade");
        }
        ImGui::SameLine();
        const bool remove = ImGui::SmallButton("X");
        ImGui::PopID();
        if (remove) {
          beginEdit();
          list.erase(k);
          heldUnit_.reset();
          break;
        }
      }
    }
  }

  // Arquivos de "<id>": every saved serial (ours and Engine2's), Carregar / Ativar / apagar (ours only, two clicks).
  void drawFiles() {
    const std::string id = draft_.value("id", "");
    ImGui::Separator();
    ImGui::TextColored(ImColor(180, 180, 180), "ARQUIVOS DE \"%s\" (%d)", id.c_str(), static_cast<int>(files_.size()));
    const std::map<std::string, int> active = activeMaps();
    const std::optional<int> activeSerial = active.contains(id) ? std::optional(active.at(id)) : std::nullopt;
    if (files_.empty()) {
      hint("Nenhum — Salvar grava content/maps/ember/%s001.json.", scenarioId(id).c_str());
    } else {
      const int latest = files_.back().serial;
      for (auto it = files_.rbegin(); it != files_.rend(); ++it) {
        const SavedFile& f = *it;
        ImGui::PushID(f.file.string().c_str());
        ImGui::TextColored(f.serial == latest ? ImColor(252, 211, 77) : ImColor(240, 240, 240), "%03d", f.serial);
        ImGui::SameLine();
        hint("%s%s%s", f.file.filename().string().c_str(), f.own ? "" : " (Engine2)",
                            activeSerial && *activeSerial == f.serial ? " · ativa na campanha" : f.serial == latest ? " · arquivo mais novo" : "");
        ImGui::SameLine();
        if (ImGui::SmallButton("Carregar")) openFile(f.file, std::format("Aberto {}.", f.file.filename().string()));
        ImGui::SameLine();
        ImGui::BeginDisabled(activeSerial && *activeSerial == f.serial);
        if (ImGui::SmallButton("Ativar")) {
          openFile(f.file, "");
          setActive(id, f.serial);
          note(std::format("{} agora é a cópia exata ativa na campanha. Preparando para o jogo…", f.file.filename().string()));
          prepare(f.file);
        }
        ImGui::EndDisabled();
        if (f.own) {
          ImGui::SameLine();
          const std::string name = f.file.filename().string();
          if (ImGui::SmallButton(armedDelete_ == name ? "Erase?" : "✕")) {
            if (armedDelete_ != name) {
              armedDelete_ = name;
              note(std::format("Clique de novo no X pra apagar {} — isso não tem volta.", name));
            } else {
              armedDelete_.clear();
              std::error_code ec;
              std::filesystem::remove(f.file, ec);
              note(ec ? std::format("NÃO APAGOU {}: {}", name, ec.message()) : std::format("{} apagado.", name));
              if (activeSerial && *activeSerial == f.serial) setActive(id, std::nullopt);
              ImGui::PopID();
              rescan();
              return;
            }
          }
        }
        ImGui::PopID();
      }
    }
    if (activeSerial && ImGui::SmallButton(std::format("Usar cenário original (desativar v{:03})", *activeSerial).c_str())) {
      setActive(id, std::nullopt);
      note(std::format("\"{}\" voltou a usar o cenário original.", id));
    }
  }

  // The action bar: the big confirmation (Locais' Salvar), the running note, Testar and Salvar mapa.
  void drawFooter() {
    if (bigNote_) {
      const ImU32 border = bigNote_->ok ? IM_COL32(52, 211, 153, 255) : IM_COL32(239, 68, 68, 255);
      ImGui::PushStyleColor(ImGuiCol_Border, border);
      ImGui::BeginChild("##bignote", {0, 112}, ImGuiChildFlags_Borders);
      ImGui::TextColored(bigNote_->ok ? ImColor(110, 231, 183) : ImColor(252, 165, 165), "%s", bigNote_->title.c_str());
      ImGui::SameLine(ImGui::GetContentRegionAvail().x - 10);
      if (ImGui::SmallButton("×")) bigNote_.reset();
      if (bigNote_)
        for (const std::string& l : bigNote_->lines) ImGui::TextWrapped("%s", l.c_str());
      ImGui::EndChild();
      ImGui::PopStyleColor();
    }
    if (!note_.empty()) {
      const bool bad = note_.starts_with("NÃO SALVO:"), good = note_.starts_with("Salvo:");
      ImGui::PushStyleColor(ImGuiCol_Text, bad ? IM_COL32(252, 165, 165, 255) : good ? IM_COL32(110, 231, 183, 255) : IM_COL32(224, 200, 150, 255));
      ImGui::BeginChild("##note", {0, 44});
      ImGui::TextWrapped("%s", note_.c_str());
      ImGui::EndChild();
      ImGui::PopStyleColor();
    }
    const float w = (ImGui::GetContentRegionAvail().x - 8) / 2;
    ImGui::BeginDisabled(playtest_.valid());
    if (ImGui::Button(playtest_.valid() ? "Preparando…" : "Testar", {w, 26})) playtest();
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Salvar mapa", {w, 26})) save();
  }

  // --- Locais (the campaign's locations, their missions, slots and floors) ------------------------------------------------

  void openLocais() {
    locais_ = e2::Locais::load(kCatalog);  // refreshLocaisState: what is on disk now
    order_ = locais_.editorOrder();
    showLocations_ = true;
    rescan();
  }
  // saveOrder / saveLocaisLocal: written at once and the campaign takes it.
  bool saveLocais(const std::string& said) {
    for (const auto& [loc, ids] : order_) locais_.order[loc] = ids;
    if (!locais_.save()) {
      note("NÃO SALVOU: não foi possível gravar " + e2::Locais::file().generic_string() + ".");
      return false;
    }
    if (callbacks_.locaisSaved) callbacks_.locaisSaved();
    note(said);
    return true;
  }
  void transferMission(const std::string& missionId, const std::string& to) {
    for (auto& [loc, ids] : order_) ids.erase(std::remove(ids.begin(), ids.end(), missionId), ids.end());
    order_[to].push_back(missionId);
    saveLocais("Ordem salva e aplicada à campanha.");
  }
  // setLocationSubmap: the mission becomes this location's floor (leaving every other floor list and every order).
  void setSubmap(const std::string& loc, const std::string& missionId, int floor) {
    std::optional<int> oldFloor;
    for (const e2::LocaisFloor& f : locais_.submaps[loc])
      if (f.missionId == missionId) oldFloor = f.floor;
    for (auto& [l, list] : locais_.submaps) list.erase(std::remove_if(list.begin(), list.end(), [&](const e2::LocaisFloor& f) { return f.missionId == missionId; }), list.end());
    std::vector<e2::LocaisFloor>& list = locais_.submaps[loc];
    for (e2::LocaisFloor& f : list)
      if (oldFloor && f.floor == floor) f.floor = *oldFloor;
    list.push_back({missionId, floor});
    std::stable_sort(list.begin(), list.end(), [](const e2::LocaisFloor& a, const e2::LocaisFloor& b) { return a.floor > b.floor; });
    for (auto& [l, ids] : order_) ids.erase(std::remove(ids.begin(), ids.end(), missionId), ids.end());
    saveLocais("Andares atualizados.");
  }

  void drawLocais() {
    const ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos({io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f}, ImGuiCond_Appearing, {0.5f, 0.5f});
    ImGui::SetNextWindowSize({620, io.DisplaySize.y * 0.85f}, ImGuiCond_Appearing);
    if (!ImGui::Begin("Locais", &showLocations_, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings)) return ImGui::End();
    hint("Setas do título mudam a progressão entre Locais; setas das missões mudam a sequência interna.");
    if (ImGui::Button("Salvar")) {
      if (saveLocais("Ordem das missões e dos Locais salva.")) {
        bigNote_ = BigNote{true, "ESTÁ SALVO",
                           {"Salvo em " + e2::Locais::file().generic_string() + " — vale já: a campanha e o mapa-múndi jogam esta ordem.",
                            std::format("{} locais com ordem definida; {} com vagas definidas; submaps e andares confirmados.", order_.size(), locais_.slots.size())}};
      } else {
        bigNote_ = BigNote{false, "NÃO SALVOU", {"Não foi possível gravar " + e2::Locais::file().generic_string() + "."}};
      }
    }
    const std::vector<MapRef> reserve = reserveRefs(), campaign = campaignRefs(), targets = connectorTargets();
    const std::set<std::string> inCampaign = campaignIds();
    std::optional<std::pair<std::string, int>> moveLocation;
    for (size_t li = 0; li < locais_.locationOrder.size(); ++li) {
      const std::string locId = locais_.locationOrder[li];
      const e2::Locais::Base* loc = nullptr;
      for (const e2::Locais::Base& b : locais_.base)
        if (b.id == locId) loc = &b;
      if (!loc) continue;
      ImGui::PushID(locId.c_str());
      std::vector<std::string>& ids = order_[locId];
      const int planned = locais_.slotsFor(locId);
      ImGui::BeginChild("##loc", {0, 0}, ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY);
      ImGui::TextColored(ImColor(180, 180, 180), "%s", upperUtf8(std::format("{}. {}{}", li + 1, loc->name,
                                                                             planned > 0 ? std::format(" · {}/{}", ids.size(), planned)
                                                                             : !ids.empty() ? std::format(" · {}", ids.size())
                                                                                            : std::string(" · vazio")))
                                                              .c_str());
      ImGui::SameLine(ImGui::GetContentRegionAvail().x - 50);
      ImGui::BeginDisabled(li == 0);
      if (ImGui::SmallButton("↑##loc")) moveLocation = {locId, -1};
      ImGui::EndDisabled();
      ImGui::SameLine();
      ImGui::BeginDisabled(li + 1 == locais_.locationOrder.size());
      if (ImGui::SmallButton("↓##loc")) moveLocation = {locId, 1};
      ImGui::EndDisabled();
      if (ImGui::SmallButton("Novo mapa aqui")) {
        e2::Json fresh = blankDraft();
        const std::string mapId = scenarioId(locId + "-" + base36Now());
        int next = -1;
        for (const std::string& id : ids) next = std::max(next, id == draft_.value("id", "") ? draft_.value("index", 0) : missionInfo(id) ? missionInfo(id)->index : -1);
        fresh["id"] = mapId, fresh["index"] = next + 1, fresh["title"] = "Novo mapa — " + loc->name, fresh["place"] = loc->name, fresh["locationId"] = locId;
        setDraft(fresh);
        ids.push_back(mapId);
        saveLocais("Mapa novo criado para " + loc->name + " e já na campanha. Salvar mapa grava o conteúdo dele.");
        showLocations_ = false;
      }
      ImGui::SameLine();
      ImGui::SetNextItemWidth(170);
      if (ImGui::BeginCombo("##add", "Adicionar mapa…")) {
        for (const MapRef& m : reserve)
          if (ImGui::Selectable(std::format("{} · {}##{}", m.title, m.id, m.id).c_str())) transferMission(m.id, locId);
        ImGui::EndCombo();
      }
      if (ImGui::IsItemHovered()) ImGui::SetTooltip("Coloca neste Local um mapa seu já salvo que ainda não está na campanha");
      ImGui::SameLine();
      ImGui::SetNextItemWidth(-1);
      if (ImGui::BeginCombo("##addcampaign", "Adicionar mapa da campanha…")) {
        for (const MapRef& m : campaign)
          if (inCampaign.contains(m.id) && std::find(ids.begin(), ids.end(), m.id) == ids.end() &&
              ImGui::Selectable(std::format("{} · {}##{}", m.title, m.id, m.id).c_str()))
            transferMission(m.id, locId);
        ImGui::EndCombo();
      }
      if (ImGui::IsItemHovered()) ImGui::SetTooltip("Move pra este Local um mapa que já está em outro Local da campanha");
      hint("Andar 0: entrada. Valores negativos: subsolo. Valores positivos: acima da entrada.");
      // the campaign's missions (floor 0) and the floors, highest first
      struct Row {
        std::string id;
        int floor = 0, campaignIndex = -1;
      };
      std::vector<Row> rowsOf;
      for (size_t i = 0; i < ids.size(); ++i) rowsOf.push_back({ids[i], 0, static_cast<int>(i)});
      for (const e2::LocaisFloor& f : locais_.submaps[locId]) rowsOf.push_back({f.missionId, f.floor, -1});
      std::stable_sort(rowsOf.begin(), rowsOf.end(), [](const Row& a, const Row& b) { return a.floor > b.floor; });
      std::optional<std::function<void()>> act;
      const bool table = !rowsOf.empty() && ImGui::BeginTable("##rows", 3, ImGuiTableFlags_SizingFixedFit);
      if (table) {
        ImGui::TableSetupColumn("andar", ImGuiTableColumnFlags_WidthFixed, 120);
        ImGui::TableSetupColumn("map", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("acts", ImGuiTableColumnFlags_WidthFixed, 250);
      }
      for (const Row& r : rowsOf) {
        if (!table) break;
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::PushID(r.id.c_str());
        const std::optional<MissionInfo> m = missionInfo(r.id);
        const std::string title = m ? m->title : r.id;
        ImGui::BeginDisabled(r.campaignIndex >= 0);
        int floor = r.floor;
        ImGui::SetNextItemWidth(60);
        if (ImGui::InputInt("Andar", &floor, 0) && floor != r.floor) act = [this, locId, id = r.id, floor] { setSubmap(locId, id, floor); };
        ImGui::EndDisabled();
        ImGui::TableNextColumn();
        ImGui::TextWrapped("%s", title.c_str());
        ImGui::TableNextColumn();
        if (ImGui::SmallButton("Editar")) act = [this, id = r.id] {
            loadCampaignMap(id);
            showLocations_ = false;
          };
        ImGui::SameLine();
        if (r.campaignIndex >= 0) {
          ImGui::BeginDisabled(r.campaignIndex == 0);
          if (ImGui::SmallButton("↑")) act = [this, locId, i = r.campaignIndex] {
              std::swap(order_[locId][static_cast<size_t>(i)], order_[locId][static_cast<size_t>(i - 1)]);
              saveLocais("Ordem salva e aplicada à campanha.");
            };
          ImGui::EndDisabled();
          ImGui::SameLine();
          ImGui::BeginDisabled(r.campaignIndex + 1 == static_cast<int>(ids.size()));
          if (ImGui::SmallButton("↓")) act = [this, locId, i = r.campaignIndex] {
              std::swap(order_[locId][static_cast<size_t>(i)], order_[locId][static_cast<size_t>(i + 1)]);
              saveLocais("Ordem salva e aplicada à campanha.");
            };
          ImGui::EndDisabled();
        } else {
          if (ImGui::SmallButton("↑")) act = [this, locId, id = r.id, f = r.floor] { setSubmap(locId, id, f == -1 ? 1 : f + 1); };
          ImGui::SameLine();
          if (ImGui::SmallButton("↓")) act = [this, locId, id = r.id, f = r.floor] { setSubmap(locId, id, f == 1 ? -1 : f - 1); };
        }
        ImGui::SameLine();
        const std::string key = "location:" + locId + ":" + r.id;
        if (ImGui::SmallButton("Remover")) {
          if (r.campaignIndex >= 0) {
            if (armedDelete_ != key) {
              armedDelete_ = key;
              note("Remover? Clique de novo para tirar este mapa de Locais. O arquivo do mapa não será apagado.");
            } else {
              armedDelete_.clear();
              act = [this, locId, id = r.id] {
                auto& list = order_[locId];
                list.erase(std::remove(list.begin(), list.end(), id), list.end());
                note("Mapa removido deste Local. Clique Salvar em Locais para gravar a campanha.");
              };
            }
          } else {
            act = [this, locId, id = r.id] {
              auto& list = locais_.submaps[locId];
              list.erase(std::remove_if(list.begin(), list.end(), [&](const e2::LocaisFloor& f) { return f.missionId == id; }), list.end());
              saveLocais("Submap removido.");
            };
          }
        }
        ImGui::PopID();
      }
      if (table) ImGui::EndTable();
      ImGui::Separator();
      hint("SUBMAPS (ANDARES EXTRAS, FORA DO MENU DE CAMPANHA)");
      ImGui::SetNextItemWidth(-1);
      if (ImGui::BeginCombo("##addsub", "Adicionar submap…")) {
        for (const MapRef& m : targets) {
          if (std::find(ids.begin(), ids.end(), m.id) != ids.end()) continue;
          const auto& subs = locais_.submaps[locId];
          if (std::any_of(subs.begin(), subs.end(), [&](const e2::LocaisFloor& f) { return f.missionId == m.id; })) continue;
          if (ImGui::Selectable(std::format("{} · {}##{}", m.title, m.id, m.id).c_str())) {
            int top = 0;
            for (const e2::LocaisFloor& f : subs) top = std::max(top, f.floor);
            act = [this, locId, id = m.id, top] { setSubmap(locId, id, top + 1); };
          }
        }
        ImGui::EndCombo();
      }
      ImGui::EndChild();
      ImGui::PopID();
      if (act) {
        (*act)();
        break;  // the lists changed under the loop
      }
    }
    if (moveLocation) {
      auto& v = locais_.locationOrder;
      const auto it = std::find(v.begin(), v.end(), moveLocation->first);
      const long long i = it - v.begin(), j = i + moveLocation->second;
      if (it != v.end() && j >= 0 && j < static_cast<long long>(v.size())) std::swap(v[static_cast<size_t>(i)], v[static_cast<size_t>(j)]), saveLocais("Ordem dos Locais salva.");
    }
    ImGui::End();
  }

  // --- R-Encounter (random encounters by region) --------------------------------------------------------------------------

  void saveRegions() {
    if (!locais_.saveRegions()) return note("Não foi possível gravar " + e2::Locais::regionsFile().generic_string() + ".");
    if (callbacks_.locaisSaved) callbacks_.locaisSaved();
    note("Regiões de R-Encounter atualizadas.");
  }
  void drawEncounters() {
    const ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos({io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f}, ImGuiCond_Appearing, {0.5f, 0.5f});
    ImGui::SetNextWindowSize({560, io.DisplaySize.y * 0.85f}, ImGuiCond_Appearing);
    if (!ImGui::Begin("R-Encounter", &showEncounters_, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings)) return ImGui::End();
    hint("Encontros separados da campanha. Regiões são grupos livres para receber novos biomas depois.");
    ImGui::SetNextItemWidth(220);
    ImGui::InputTextWithHint("##regionname", "Nome da nova região", regionName_, sizeof(regionName_));
    ImGui::SameLine();
    if (ImGui::Button("Nova região") && regionName_[0]) {
      const std::string id = scenarioId(regionName_);
      if (std::any_of(locais_.regions.begin(), locais_.regions.end(), [&](const e2::EncounterRegion& r) { return r.id == id; })) {
        note("Já existe uma região com esse nome.");
      } else {
        std::string name = regionName_;
        while (!name.empty() && std::isspace(static_cast<unsigned char>(name.back()))) name.pop_back();
        locais_.regions.push_back({id, name, {}});
        regionName_[0] = 0;
        saveRegions();
      }
    }
    std::set<std::string> assigned;
    for (const e2::EncounterRegion& r : locais_.regions) assigned.insert(r.encounterIds.begin(), r.encounterIds.end());
    const std::vector<MapRef> reserve = reserveRefs();
    std::optional<std::function<void()>> act;
    for (size_t ri = 0; ri < locais_.regions.size() && !act; ++ri) {
      e2::EncounterRegion& region = locais_.regions[ri];
      ImGui::PushID(region.id.c_str());
      ImGui::BeginChild("##region", {0, 0}, ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY);
      ImGui::TextColored(ImColor(180, 180, 180), "%s · %d encontro%s", upperUtf8(region.name).c_str(), static_cast<int>(region.encounterIds.size()),
                         region.encounterIds.size() == 1 ? "" : "s");
      ImGui::SameLine(ImGui::GetContentRegionAvail().x - 60);
      const std::string key = "region:" + region.id;
      if (ImGui::SmallButton(armedDelete_ == key ? "Remover?" : "✕##region")) {
        if (armedDelete_ != key) {
          armedDelete_ = key;
          note(std::format("Remover a região {}? Clique de novo. Os mapas não serão apagados.", region.name));
        } else {
          armedDelete_.clear();
          act = [this, ri] {
            locais_.regions.erase(locais_.regions.begin() + static_cast<long>(ri));
            saveRegions();
          };
        }
      }
      if (ImGui::SmallButton("Novo encontro")) act = [this, ri] {
          e2::EncounterRegion& r = locais_.regions[ri];
          const std::string id = scenarioId("random-" + r.id + "-" + base36Now());
          r.encounterIds.push_back(id);
          saveRegions();
          e2::Json fresh = blankDraft();
          const std::string suffix = id.substr(std::min(id.size(), std::string("random-" + r.id + "-").size()));
          fresh["id"] = id, fresh["index"] = 0, fresh["title"] = std::format("Encontro em {} · {}", r.name, suffix), fresh["place"] = r.name, fresh["locationId"] = "";
          setDraft(fresh);
          showEncounters_ = false;
          note(std::format("Novo encontro criado em {}. Salve o mapa para gravar o conteúdo.", r.name));
        };
      ImGui::SameLine();
      ImGui::SetNextItemWidth(-1);
      if (ImGui::BeginCombo("##addsaved", "Adicionar mapa salvo…")) {
        for (const MapRef& m : reserve)
          if (!assigned.contains(m.id) && ImGui::Selectable(std::format("{} · {}##{}", m.title, m.id, m.id).c_str()))
            act = [this, ri, id = m.id] {
              locais_.regions[ri].encounterIds.push_back(id);
              saveRegions();
            };
        ImGui::EndCombo();
      }
      if (region.encounterIds.empty()) hint("Nenhum encontro nesta região ainda.");
      for (size_t k = 0; k < region.encounterIds.size(); ++k) {
        const std::string id = region.encounterIds[k];
        ImGui::PushID(static_cast<int>(k));
        std::string title = id;
        for (const MapRef& m : reserve)
          if (m.id == id) title = m.title;
        hint("%d.", static_cast<int>(k + 1));
        ImGui::SameLine();
        if (ImGui::Selectable(title.c_str(), false, 0, {ImGui::GetContentRegionAvail().x - 90, 0})) act = [this, id] {
            if (const auto f = newestFile(id)) openFile(*f, std::format("Aberto o encontro \"{}\".", id)), showEncounters_ = false;
          };
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Abrir encontro no editor");
        ImGui::SameLine();
        ImGui::BeginDisabled(k == 0);
        if (ImGui::SmallButton("↑")) act = [this, ri, k] {
            std::swap(locais_.regions[ri].encounterIds[k], locais_.regions[ri].encounterIds[k - 1]);
            saveRegions();
          };
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(k + 1 == region.encounterIds.size());
        if (ImGui::SmallButton("↓")) act = [this, ri, k] {
            std::swap(locais_.regions[ri].encounterIds[k], locais_.regions[ri].encounterIds[k + 1]);
            saveRegions();
          };
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::SmallButton("✕")) act = [this, ri, k] {
            locais_.regions[ri].encounterIds.erase(locais_.regions[ri].encounterIds.begin() + static_cast<long>(k));
            saveRegions();
          };
        ImGui::PopID();
      }
      ImGui::EndChild();
      ImGui::PopID();
    }
    if (act) (*act)();
    ImGui::End();
  }

  // --- state ---------------------------------------------------------------------------------------------------------------

  Services services_;
  MapStageOptions base_;
  CampaignPaths paths_;
  const e2::Campaign& campaign_;
  Callbacks callbacks_;
  std::optional<e2::BoardData> data_;
  e2::EditorCatalog catalog_;
  EditorTables editor_;
  e2::DressTables dress_;
  e2::Locais locais_;
  std::map<std::string, std::vector<std::string>> order_;  // Locais: each location's missions as edited
  e2::Json draft_;
  static constexpr size_t kHistory = 20;  // Desfazer / Refazer reach this many steps back
  std::deque<e2::Json> past_, future_;
  std::chrono::steady_clock::time_point lastEdit_{};
  bool dirty_ = false;
  std::unique_ptr<MapStage> preview_;
  std::optional<std::pair<int, int>> pendingClick_, hover_;
  std::optional<glm::vec3> pendingPoint_;
  std::optional<std::pair<std::optional<std::pair<int, int>>, bool>> pendingRelease_;
  std::optional<HeldUnit> heldUnit_;
  std::optional<e2::Json> heldDeco_;
  std::future<std::optional<MissionEntry>> playtest_;
  std::vector<std::future<std::string>> exports_;  // Salvar / Ativar: the game's files for a map (their notes when done)
  std::future<std::map<std::string, Scenario>> scan_;
  std::map<std::string, Scenario> saved_;
  std::vector<SavedFile> files_;  // the draft's id's saved serials
  std::string playtestError_;
  std::string script_;  // captures: clicks to apply (previewEdits)
  std::string note_, armedDelete_, pendingDialog_;
  std::optional<BigNote> bigNote_;
  std::optional<float> scrollTo_;
  float panelWidth_ = 500;
  int frames_ = 0;
  DialogEditorUi dialogUi_;
  bool showPreview_ = true, showTechnical_ = false, gridSquare_ = false, showLocations_ = false, showEncounters_ = false;
  char regionName_[96] = {};
  std::set<std::string> shuffleExclude_;
  // the brushes
  Mode mode_ = Mode::Paint;
  std::string brush_ = "plains";
  int variant_ = 0;
  bool cityMode_ = false, turning_ = false, turningDeco_ = false;
  int elevationTool_ = 0, elevationLevel_ = 0, elevationRadius_ = 0;
  bool waterErase_ = false, waterSquare_ = false;
  double waterLevel_ = 0.5, waterSize_ = 1.0;
  int waterRadius_ = 0;
  bool architectureDecorations_ = false, thickWalls_ = false, wallVertical_ = false;
  std::string decoBrush_ = "dead-tree", decoSection_ = "Todas";
  char search_[64] = {};
  std::optional<e2::Json> selected_;
  std::string summonBrush_ = "familiar", npcBrush_ = "breadLady";
  bool summonNeutral_ = false;
};

}  // namespace ember
