#pragma once

// The campaign's world map, copied from Ember (src/game/OverworldMapScreen.tsx, GameApp's overworld handlers and
// src/ember-ui/styles.css; user, 2026-10-15: "importar todos os menus corretamente"). Its rules are the C++ port in
// engine/world/Overworld (checked against Ember step for step).
//
// On screen: the header (Campanha · RPG / Mapa, Party, Missões, the day clock, the moon, rations, hunger, Alimentar
// todos, Opções, Gold); the painted world map (game/assets/world-map.jpg) at four zoom stops, dragged with the mouse;
// the fog over every place the party has never stood near; the time-of-day tint; the location pins (locked / open /
// done / the Inn's glow, with a mission count); the leader's idle sprite on the party's hex (click it to see the
// hexes it can reach and how long each takes); the party panel (portraits, hunger and life bars, Explorar); travel
// events; the location's mission list; the quest log; the Party menu (leader, formation, affinities, road training);
// the first-visit explainer. A mission opens its briefing, then its fight (main.cpp).
//
// The Inn's pin opens the Inn (InnScreen.h); a portrait in the party panel opens that hero's status sheet (BattleStatus.h), whose
// Mochila / Equipar open InventoryScreen.h.
// Not ported yet: field spells (Create Food and Water, Warp), the status sheet, the Inn's arrival film and walkable room,
// intro cutscenes, sound.

#include <SDL3/SDL.h>
#include <imgui.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <format>
#include <fstream>
#include <functional>
#include <optional>
#include <random>
#include <set>
#include <string>
#include <vector>

#include "BattleStatus.h"
#include "CampaignUi.h"
#include "CutsceneScreen.h"
#include "DialogScreen.h"
#include "EmberMenus.h"
#include "GameMusic.h"
#include "HeroHandoff.h"  // heroStatesFromSave, MapStage (the status sheet's hero, as a fight seats it)
#include "InnScreen.h"
#include "QuestLog.h"
#include "Stage.h"
#include "render/Camera.h"
#include "render/DebugUi.h"
#include "render/MeshScene.h"
#include "render/Renderer.h"
#include "render/UiArt.h"
#include "render/WorldMapFx.h"
#include "world/BattleExport.h"
#include "world/Campaign.h"
#include "world/Combat.h"
#include "world/Companions.h"
#include "world/FieldSpells.h"
#include "world/Inn.h"
#include "world/Interlude.h"
#include "world/Overworld.h"
#include "world/Underworld.h"
#include "world/Spells.h"

namespace ember {

class OverworldStage : public Stage {
 public:
  struct Callbacks {
    std::function<void(const MissionEntry&)> play;  // a mission's fight
    std::function<void()> title;                    // back to the title screen
    std::function<void()> fullscreen;               // Opções: Tela cheia
    DevGfx* gfx = nullptr;                          // Opções: Gráficos (the game's Dev Controls switches)
    // GameApp enterFromSave: a fight saved mid-way is resumed (its mission's battle, the save's snapshot applied)
    // when the campaign is entered from Continuar / a load (`resumeSaved`) or a slot is loaded here.
    std::function<void(const MissionEntry&)> resume;
    bool resumeSaved = false;
  };

  // `testSave`: Ember's Modo teste (GameApp testMode): the map runs on that save (held by the caller for the whole
  // test session, never the real bank) and every travel / party / mission restriction is lifted (Overworld `test`).
  // `interlude`: the historical chapter's data (content/interlude.json; docs/INTERLUDE.md), when loaded: the Ruins' quest gate and
  // the chapter's start on the first entry there.
  OverworldStage(CampaignPaths paths, ember::Campaign& campaign, const ember::Overworld& world, Callbacks callbacks, std::string notice = {},
                 ember::Json* testSave = nullptr, const ember::InterludeData* interlude = nullptr)
      : paths_(std::move(paths)), campaign_(campaign), world_(world), callbacks_(std::move(callbacks)), notice_(std::move(notice)), testSave_(testSave),
        interlude_(interlude) {
    hasInn_ = innData_.load("content/data/inn.json");
    hasUnder_ = under_.load("content/data/underworld.json");
    hasCompanions_ = companions_.load("content/data/companions.json", "content/data/companions-pt.json");
    save_ = world_.fitSave(testSave_ ? *testSave_ : campaign_.activeSave());
    const ember::Json& pos = save_["overworldPos"];
    prevCol_ = pos.value("col", 0);
    partyX_ = -1;  // placed on the first frame
    // GameApp start: a fight saved mid-way, else a pending mission, opens its briefing first.
    std::string id;
    if (save_.contains("battle") && save_["battle"].is_object()) id = save_["battle"].value("missionId", "");
    if (id.empty() && save_.contains("pendingMission") && save_["pendingMission"].is_string()) id = save_["pendingMission"].get<std::string>();
    // A new campaign opens with Ember's films: the boot vignette, then O Vau's intro (GameApp boot -> vauIntro).
    if (!testSave_ && campaign_.freshCampaign && id == "vau") {
      campaign_.freshCampaign = false;
      playFilm("game/title-open.mp4", [this] { playFilm("game/vau-intro.webm", [this] { openMission("vau", true); }); });
      return;
    }
    if (!id.empty() && callbacks_.resumeSaved && resumeSavedFight(id)) return;
    if (!id.empty()) openMission(id, true);
  }

  // The Underdark of Mordávia's exit (the Misty Cave's map 4, mordavia-underdark): a floor connector whose target is this id opens
  // the world map on the Atlas do Submundo (main.cpp), the campaign recording "underworldReached".
  static constexpr const char* kUnderworldExit = "underworld-atlas";
  // The Graveyard's film (CutsceneScreen opens an absolute path as it is: artRoot / absolute = the absolute path).
  static constexpr const char* kGraveyardFilm = "content/films/graves-intro-v01.webm";
  void showUnderworld() {
    if (!underworld_) toggleWorld();
  }
  // The location's mission list opened as a click on its pin would (main.cpp: back from the Interlude, the present-day Ruins).
  void openLocationPanel(const std::string& id) { open_ = id; }
  // A location's status as the pins show it: the world's rules, and the Ruins shut by the Interlude's quest gate (never in Modo teste).
  std::string locationStatus(const ember::MapLocation& loc) const {
    std::string st = world_.locationStatus(loc, save_, test());
    if (interlude_ && !test() && loc.id == interlude_->ruinsLocation && st != "locked" && !ember::ruinsOpen(*interlude_, world_, save_)) st = "locked";
    return st;
  }

  // Captures: a panel opened as a click would ("party", "skills", "quests", "options", "move", "location:<id>", "save").
  void preview(const std::string& what) {
    save_["seenOverworldIntro"] = true;  // a capture of a panel is not covered by the first-visit explainer
    if (what == "party") party_ = true;
    if (what == "underworld") toggleWorld();
    if (what == "measure" || what == "measure:underworld") {  // a luz e a névoa medidas (WorldMapFx): três segundos, a média no log
      measure_ = true;
      if (what.size() > 7) toggleWorld();
    }
    if (what == "party:end") party_ = true, partyScrollEnd_ = true;
    if (what == "skills") party_ = true, partyTab_ = 1;
    if (what == "talk" || what.starts_with("talk:")) {  // "talk:<tier>": that tier's conversation of the first companion opens
      party_ = true, partyTab_ = 2;
      if (what.size() > 5 && hasCompanions_) {
        const std::string lead = leader();
        for (const std::string& hero : partyHeroes())
          if (hero != lead) {
            convCompanion_ = hero;
            break;
          }
        const int tier = std::atoi(what.c_str() + 5);
        const ember::Json tree = companions_.build(lead, convCompanion_, tier, save_, uiEnglish());
        if (tree.is_object()) convLeader_ = lead, convDialog_.emplace(tree, companions_.portraits(), paths_.artRoot);
      }
    }
    if (what == "fieldspells") fieldSpells_ = true;
    if (what.starts_with("warp:")) warpHero_ = what.substr(5);
    if (what.starts_with("recall:")) recallHero_ = what.substr(7);  // Retornar's runes (the Conjurador's)
    if (what.starts_with("identify:")) identifyHero_ = what.substr(9);  // Identificar's unknown items (the Mago Negro's)
    if (what == "quests") questLog_ = true;
    if (what == "options") options_ = true;
    if (what == "move") movementOpen_ = true;
    if (what.starts_with("walk:"))  // "walk:<col>,<row>;...": each step clicked as the player would (leader, then the hex), logged
      for (size_t at = 5; at < what.size();) {
        const size_t end = std::min(what.find(';', at), what.size());
        const std::string s = what.substr(at, end - at);
        at = end + 1;
        if (s == "move") {  // "...;move": the step hexes shown at the end, as a click on the leader shows them
          movementOpen_ = true;
          continue;
        }
        if (s.starts_with("pin:")) {  // "...;pin:<id>": a click on that location's pin, step hexes closed
          movementOpen_ = false;
          const ember::Json& pos = save_["overworldPos"];
          const int pc = pos.value("col", 0), pr = pos.value("row", 0);
          for (const ember::MapLocation& loc : world_.locations())
            if (loc.id == s.substr(4))
              pinClicked(loc, locationStatus(loc),
                         world_.canStep(save_, {pc, pr}, {loc.col, loc.row}, test()) &&
                             std::ranges::any_of(ember::Overworld::neighbors(pc, pr), [&](const ember::OverworldHex& n) { return n.col == loc.col && n.row == loc.row; }),
                         world_.locationAt(pc, pr));
          logInfo("walk: pin {} clicked at ({},{}): now at ({},{}), open {}, briefing {}", s.substr(4), pc, pr, save_["overworldPos"].value("col", 0),
                  save_["overworldPos"].value("row", 0), open_ ? *open_ : std::string("-"), briefing_ ? briefing_->id : std::string("-"));
          continue;
        }
        const size_t comma = s.find(',');
        if (comma == std::string::npos) continue;
        const int c = std::stoi(s.substr(0, comma)), r = std::stoi(s.substr(comma + 1));
        const ember::Json& pos = save_["overworldPos"];
        const int fc = pos.value("col", 0), fr = pos.value("row", 0);
        const bool allowed = world_.canStep(save_, {fc, fr}, {c, r}, test());
        movementOpen_ = true;
        walkTo(c, r);
        logInfo("walk: ({},{}) -> ({},{}): canStep {}, now at ({},{}), day {} {}:00, event {}, briefing {}", fc, fr, c, r, allowed,
                save_["overworldPos"].value("col", 0), save_["overworldPos"].value("row", 0), static_cast<long long>(num(save_, "gameClock", 0)),
                ember::Overworld::campaignHour(save_), event_ ? event_->kind : std::string("-"), briefing_ ? briefing_->id : std::string("-"));
        if (briefing_) briefing_.reset();  // a road battle's briefing: closed as Voltar would, the walk goes on
        event_.reset();
      }
    if (what.starts_with("walkslow:"))  // "walkslow:<col>,<row>;...": one step every 45 frames while the map draws (the leader walks between them)
      for (size_t at = 9; at < what.size();) {
        const size_t end = std::min(what.find(';', at), what.size());
        const std::string s = what.substr(at, end - at);
        at = end + 1;
        const size_t comma = s.find(',');
        if (comma != std::string::npos) slowSteps_.push_back({std::stoi(s.substr(0, comma)), std::stoi(s.substr(comma + 1))});
      }
    if (what.starts_with("location:")) open_ = what.substr(9);
    if (what.starts_with("enter:"))  // "enter:<id>": the location entered as a click on its pin would (the Ruins: the Interlude's gate and start)
      for (const ember::MapLocation& loc : world_.locations())
        if (loc.id == what.substr(6)) enterLocation(loc, locationStatus(loc));
    if (what == "save") slots_.emplace(), slots_->open(SlotScreen::Mode::Save, true);
    if (what == "releaseui") releaseUiAt_ = 20;  // the loading screen's release mid-run: every UI image off the GPU, then drawn again
    if ((what == "pack" || what == "doll") && hasInn_) inventory_.emplace("Kael", what == "pack");
    if (what.starts_with("status:")) openSheet(what.substr(7));
    if (what.starts_with("inn") && hasInn_) {
      inn_.emplace();
      if (what.size() > 4) inn_->preview(what.substr(4));
    }
    // "film:<file under public>[@seconds]": a film playing, captured that far in (its subtitles at that moment).
    if (what.starts_with("film:")) {
      std::string film = what.substr(5);
      if (const size_t at = film.find('@'); at != std::string::npos) previewFilmAt_ = std::stod(film.substr(at + 1)), film.resize(at);
      playFilm(film.c_str(), [] {});
    }
  }
  // Captures end after a second of frames (time for Ember's art to load), or once a previewed film got that far; a measured
  // capture (--map-ui measure[:underworld]) runs three seconds for the mean.
  bool done() const override {
    return frames_ > (measure_ ? 180 : 60) && slowNext_ >= slowSteps_.size() && frames_ > slowLast_ + 60 && (!film_ || film_->time() >= previewFilmAt_);
  }
  void endFrame(double busyMs) override {
    if (frames_ > 60 && frames_ <= 180) measureCpuMs_ += busyMs, ++measureCpuFrames_;
  }
  // Qualidade gráfica (Opções > Gráficos; DevGfxSettings.h): 0 baixa, 1 média, 2 alta.
  int gfxQuality() const {
    const std::string q = callbacks_.gfx ? callbacks_.gfx->quality : std::string("high");
    return q == "low" ? 0 : q == "medium" ? 1 : 2;
  }

  void frame(Renderer& renderer, VkExtent2D) override {
    ++frames_;
    if (frames_ == releaseUiAt_) releaseUiImages();
    if (!fx_) {  // a luz e a névoa das duas telas (render/WorldMapFx.h): o passe, as configurações e as pinturas, uma vez
      fx_.emplace(renderer.context(), renderer.bindless());
      fxConfig_[0] = WorldMapConfig::load("content/worldmap/overworld-light-fog.json", false);
      fxConfig_[1] = WorldMapConfig::load("content/worldmap/underworld-light-fog.json", true);
      if (fxConfig_[0].enabled) {
        fxMap_[0] = fx_->loadTexture(kWorldMap);
        if (!fxConfig_[0].mask.empty()) fxMask_[0] = fx_->loadTexture(fxConfig_[0].mask);
        // O look 2D (2026-10-28): as LUTs por hora e as máscaras pintadas pelo usuário; uma ausente = aquela parte desligada.
        // tex[]: 0 luzes, 1 luzes borradas, 2 altura, 3 água, 4 névoa (worldmap.hlsl Push).
        const WorldMapConfig& c = fxConfig_[0];
        for (const WorldMapConfig::LutKey& k : c.luts) fxLuts_[0].push_back(k.file.empty() ? WorldMapFx::kNoTexture : fx_->loadTexture(k.file));
        uint32_t* t = fxTex_[0];
        if (!c.lightMask.empty()) t[0] = t[1] = fx_->loadTexture(c.lightMask);  // a zona dos wisps de ember (sem borrado)
        if (!c.heightMap.empty()) t[2] = fx_->loadTexture(c.heightMap);
        if (!c.waterMask.empty()) t[3] = fx_->loadTexture(c.waterMask);
        if (!c.mistMask.empty()) t[4] = fx_->loadTexture(c.mistMask);
        logInfo("worldmap look: {} LUTs, lights {}, height {}, water {}, mist {}", fxLuts_[0].size(), fxOn(0, 0), fxOn(0, 2), fxOn(0, 3), fxOn(0, 4));
      }
      if (fxConfig_[1].enabled) {
        fxMap_[1] = fx_->loadTexture(kUnderworldAtlas);
        if (!fxConfig_[1].mask.empty()) fxMask_[1] = fx_->loadTexture(fxConfig_[1].mask);
        // Submundo, o look 2D no ciclo invertido: tex[] 0 brilho fúngico, 1 borrado, 2 plantas, 3 água, 4 esporos, 5 janelas,
        // 6 ember, 7 ember borrado, 8 tremor de calor.
        const WorldMapConfig& c = fxConfig_[1];
        for (const WorldMapConfig::LutKey& k : c.luts) fxLuts_[1].push_back(k.file.empty() ? WorldMapFx::kNoTexture : fx_->loadTexture(k.file));
        uint32_t* t = fxTex_[1];
        if (!c.fungalMask.empty()) std::tie(t[0], t[1]) = fx_->loadLightMask(c.fungalMask, c.lightGlowDownsample, c.lightGlowRadius);
        if (!c.plantMask.empty()) t[2] = fx_->loadTexture(c.plantMask);
        if (!c.waterMask.empty()) t[3] = fx_->loadTexture(c.waterMask);
        if (!c.sporeMask.empty()) t[4] = fx_->loadTexture(c.sporeMask);
        if (!c.windowMask.empty()) t[5] = fx_->loadTexture(c.windowMask);
        if (!c.emberMask.empty()) std::tie(t[6], t[7]) = fx_->loadLightMask(c.emberMask, c.lightGlowDownsample, c.lightGlowRadius);
        if (!c.heatMask.empty()) t[8] = fx_->loadTexture(c.heatMask);
        logInfo("underworld look: {} LUTs, fungal {}, plants {}, water {}, spores {}, windows {}, ember {}, heat {}", fxLuts_[1].size(), fxOn(1, 0),
                fxOn(1, 2), fxOn(1, 3), fxOn(1, 4), fxOn(1, 5), fxOn(1, 6), fxOn(1, 8));
      }
      logInfo("worldmap: light + fog {} / {} ({:.1f} MB of textures)", fxConfig_[0].enabled ? "on" : "off", fxConfig_[1].enabled ? "on" : "off",
              double(fx_->textureBytes()) / 1048576.0);
    }
    // A medida (capturas: --title-screen map [--map-ui underworld]): o escopo "ui" do GpuTimer, a média sobre os quadros
    // depois do primeiro segundo, com a CPU da thread principal (endFrame), ditas no log quando a captura termina.
    if (frames_ > 60 && frames_ <= 180)
      for (const auto& r : renderer.gpuTimer().results())
        if (std::string(r.name) == "ui") measureUiMs_ += r.milliseconds, ++measureFrames_;
    if (frames_ == 180)
      logInfo("worldmap measure ({}, quality {}): gpu ui {:.3f} ms, main thread {:.3f} ms over {} frames; {:.1f} MB of textures",
              underworld_ ? "underworld" : "overworld", gfxQuality(), measureUiMs_ / std::max(1, measureFrames_),
              measureCpuMs_ / std::max(1, measureCpuFrames_), measureFrames_, double(fx_->textureBytes()) / 1048576.0);
    renderer.setScene(Camera{}, {});
    renderer.setMeshes(MeshSceneView{});
    renderer.setTerrain({});
    renderer.setWater({});
    renderer.setLights({});
    renderer.setGroundMarks({}, 0, 0);
    renderer.setFogOfWar(nullptr, 0);
    renderer.setVolumetricFog({});
    renderer.setFogBank({});
    renderer.setBackground(Renderer::kBlackBackground);
    renderer.setBloom(0.0f);
  }

  // GameApp's music chain: films are silent, the in-game save slots too; the Inn plays its theme (not over Vargan's
  // first film), a briefing its mission's track, the map its own piece.
  // The world map opening under Ember's loading curtain (MapLoadingOverlay "Carregando o mapa"): ready once the screen
  // has drawn what it shows, the map with its picture in (a briefing, a film or the Inn shown first need no picture).
  Loading loading() const override {
    const bool ready = drawn_ > 0 && (!mapDrawn_ || uiImageLoaded(kWorldMap));
    return {0, 0, 0, ready};
  }

  Music music() const override {
    if (film_ || slots_) return {};
    if (inn_) return inn_->filmPlaying() ? Music{} : music::theme("inn");
    if (briefing_) return music::forMission(*briefing_);
    return music::theme("worldMap");
  }

  void event(const SDL_Event& e) override {
    if (e.type == SDL_EVENT_KEY_DOWN && !e.key.repeat && e.key.key == SDLK_ESCAPE) {
      // Esc: closes what is open; on the bare map it opens Opções (OptionsButton escapeKey).
      if (convDialog_) convDialog_.reset();
      else if (sheet_.isOpen()) sheet_.closeConditionOrSheet();
      else if (!warpHero_.empty()) warpHero_.clear();
      else if (!recallHero_.empty()) recallHero_.clear();
      else if (!identifyHero_.empty()) identifyHero_.clear();
      else if (fieldSpells_) fieldSpells_ = false;
      else if (briefing_) briefing_.reset();
      else if (slots_) slots_.reset();
      else if (options_) options_ = false;
      else if (open_) open_.reset();
      else if (questLog_) questLog_ = false;
      else if (party_) party_ = false;
      else if (movementOpen_) movementOpen_ = false;
      else options_ = true;
    }
  }

  void drawPanel() override {
    const ImGuiIO& io = ImGui::GetIO();
    now_ = static_cast<float>(ImGui::GetTime());
    ++drawn_;
    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::Begin("##overworld", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoScrollWithMouse);
    if (film_) {
      if (film_->draw(io.DisplaySize)) {
        film_.reset();
        if (auto then = std::exchange(afterFilm_, {})) then();
      }
    } else if (inventory_) {
      InventoryScreen::Context context{save_, [this] { write(); }, innRules_, innData_, world_, paths_.artRoot};
      if (inventory_->draw(io.DisplaySize, context)) {
        inventory_.reset();
        if (!sheetReturn_.empty()) openSheet(std::exchange(sheetReturn_, {}));  // opened from the sheet: back to it, as now worn
      }
    } else if (inn_) {
      InnScreen::Context context{save_, [this] { write(); }, innRules_, innData_, world_, paths_.artRoot};
      if (inn_->draw(io.DisplaySize, context)) inn_.reset();  // GameApp returnToCurrentMap
    } else if (briefing_) {
      const int r = drawBriefing(io.DisplaySize, kRem, *briefing_, paths_.artRoot);
      if (r < 0) briefing_.reset();
      else if (r > 0 && callbacks_.play) {
        // GameApp beginMission: some missions open with their film (bound to the mission id, not its position).
        const std::string id = briefing_->draftId;
        const char* film = id == "aldeia" ? "game/aldeia-intro.webm" : id == "thebridge" ? "game/thebridge-intro.mp4" : id == "templo" ? "game/asherah-rite.mp4" : nullptr;
        // The Graveyard's film (the user's Graves video, 2026-10-23), the first time the party goes in: the campaign's save and
        // Modo teste's own session save each keep the flag.
        if (id == "cemiterio-esquecidos" && !save_.value("seenGraveyardIntro", false)) {
          MissionEntry entry = *briefing_;
          const std::string graves = std::filesystem::absolute(kGraveyardFilm).string();
          playFilm(graves.c_str(), [this, entry] {
            save_["seenGraveyardIntro"] = true;
            addAffinity("Neera", "Salazar", 2);  // the user: "after that scene play Neera and Salazar gain +2 Affinity"
            write();
            if (callbacks_.play) callbacks_.play(entry);
          });
        } else if (film) {  // Modo teste too: it differs only by the party at 30, the Gold and everything unlocked (the user, 2026-10-25)
          MissionEntry entry = *briefing_;
          playFilm(film, [this, entry] {
            if (callbacks_.play) callbacks_.play(entry);
          });
        } else {
          callbacks_.play(*briefing_);
        }
      }
    } else if (slots_) {
      bool back = false;
      const int chosen = slots_->draw(io.DisplaySize, kRem, campaign_, paths_.artRoot, back, &save_);
      if (back) slots_.reset();
      else if (chosen >= 0) {
        if (slots_->mode() == SlotScreen::Mode::Load) {
          // Modo teste keeps reading its own save (GameApp readMapSave): the slot is selected in the bank only.
          if (!test()) save_ = world_.fitSave(campaign_.activeSave());
          notice_ = slots_->status();
          slots_.reset();
          const ember::Json& pos = save_["overworldPos"];
          prevCol_ = pos.value("col", 0), partyX_ = -1, centred_ = false;
          std::string id;
          if (save_.contains("battle") && save_["battle"].is_object()) id = save_["battle"].value("missionId", "");
          if (id.empty() && save_.contains("pendingMission") && save_["pendingMission"].is_string()) id = save_["pendingMission"].get<std::string>();
          if (!id.empty() && !test() && resumeSavedFight(id)) return;
          if (!id.empty()) openMission(id, true);
        } else {
          notice_ = slots_->status();
          slots_.reset();
        }
      }
    } else {
      drawMap(io.DisplaySize);
      drawSheet();
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
  }

 private:
  static constexpr float kRem = 16.0f;  // CSS px are canvas units (DebugUi::kUiHeight)
  static constexpr float kOverworldZoomStops[4] = {110, 130, 150, 170};
  static constexpr float kUnderworldZoomStops[4] = {70, 90, 110, 130};
  // The six, then EmberEngine's own heroes (content/ember-classes.json "heroes": Isolde). Modo teste's party is everyone (the user,
  // 2026-10-25: "precisa adicionar Isolde a party no mapa mundi do debug"); the campaign keeps only the recruited (heroRecruited:
  // "recruited:Isolde" flag).
  // Selene too (the user, 2026-10-25: "make her available in debug mode as stress test"; never recruited in the campaign yet).
  static constexpr const char* kHeroes[8] = {"Kael", "Neera", "Voss", "Salazar", "Aldric", "Malrec", "Isolde", "Selene"};
  // EmberEngine's own heroes have no row in the world tables (baseClass, portraits): their class and bust from here.
  static const char* ownHeroClass(const std::string& hero) { return hero == "Isolde" || hero == "Selene" ? "conjurer" : nullptr; }
  int baseMaxHp(const std::string& hero) const {
    if (const int hp = world_.maxHp(save_, hero); hp > 0) return hp;
    const char* cls = ownHeroClass(hero);
    const ember::Json& promo = save_.contains("promotions") ? save_["promotions"].value(hero, ember::Json()) : ember::Json();
    const std::string classId = promo.is_string() ? promo.get<std::string>() : cls ? cls : "";
    return classId.empty() ? 0 : world_.classHp(classId, static_cast<int>(num(save_.value("levels", ember::Json::object()), hero, 1)));
  }
  static constexpr ImU32 kAccent = IM_COL32(216, 216, 216, 255), kMuted = IM_COL32(180, 180, 180, 255), kFg = IM_COL32(240, 240, 240, 255),
                         kDanger = IM_COL32(181, 74, 50, 255), kBorder = IM_COL32(74, 74, 74, 255);

  // --- the save -------------------------------------------------------------------------------------------------------

  bool test() const { return testSave_ != nullptr; }
  bool recruited(const std::string& hero) const { return world_.inParty(hero, save_, test()); }
  // The active party (Overworld::activeParty: the Party screen's choice, at most six): the map's followers, the leader, the
  // formation, the fights. The reserve = the rest of the party.
  std::vector<std::string> partyHeroes() const { return world_.activeParty(save_, test()); }
  std::vector<std::string> reserveHeroes() const {
    const std::vector<std::string> active = partyHeroes();
    std::vector<std::string> out;
    for (const char* h : kHeroes)
      if (recruited(h) && std::ranges::find(active, h) == active.end()) out.push_back(h);
    return out;
  }
  // partyLeaderOf: the saved leader while in the active party, else Kael, else the first of it.
  std::string leader() const {
    const std::vector<std::string> active = partyHeroes();
    const ember::Json& l = save_.contains("partyLeader") ? save_["partyLeader"] : ember::Json();
    if (l.is_string() && std::ranges::find(active, l.get<std::string>()) != active.end()) return l.get<std::string>();
    if (active.empty() || std::ranges::find(active, "Kael") != active.end()) return "Kael";
    return active.front();
  }
  void write() {
    if (testSave_) {  // Modo teste: the session's own save, never the real bank
      *testSave_ = save_;
      return;
    }
    if (!campaign_.saveActive(save_)) notice_ = "Não foi possível gravar o progresso (" + ember::Campaign::savePath().string() + ").";
  }
  double num(const ember::Json& obj, const std::string& key, double fallback) const {
    if (!obj.is_object()) return fallback;
    const auto it = obj.find(key);
    return it != obj.end() && it->is_number() ? it->get<double>() : fallback;
  }
  bool completed(const std::string& id) const {
    for (const ember::Json& c : save_.value("completed", ember::Json::array()))
      if (c.is_string() && c.get<std::string>() == id) return true;
    return false;
  }
  // OverworldMapScreen mapHeroMaxHp: class HP at the hero's level plus gear HP, less the hunger penalty.
  int heroMaxHp(const std::string& hero) const {
    double gear = 0;
    if (save_.contains("equipment") && save_["equipment"].contains(hero))
      for (const auto& [slot, id] : save_["equipment"][hero].items())
        if (id.is_string())
          for (const ember::Json& e : world_.tables()["equipment"])
            if (e["id"] == id) gear += e.value("hp", 0.0);
    const double streak = num(save_, "hungerStreak", 0);
    const double penalty = std::min(0.9, std::max(0.0, streak - 3) * 0.1);
    return static_cast<int>(std::floor((baseMaxHp(hero) + gear) * (1 - penalty) + 0.5));
  }

  // GameApp openMission: gated missions must be open; a finished mission only re-opens when it is a dungeon.
  // GameApp enterFromSave with a battle in the save: straight into that fight, resumed.
  bool resumeSavedFight(const std::string& id) {
    if (test() || !callbacks_.resume || !save_.contains("battle") || !save_["battle"].is_object() || save_["battle"].value("missionId", "") != id)
      return false;
    std::string error;
    std::optional<MissionEntry> e = campaignMission(campaign_, paths_, id, error);
    if (!e) return false;
    callbacks_.resume(*e);
    return true;
  }

  void openMission(const std::string& id, bool fromSave = false) {
    if (!fromSave) {
      if (const ember::MapMission* m = world_.mission(id); !test() && m && m->gated && world_.missionAccess(id, save_) != "available") return;
      if (!test() && completed(id)) {
        const ember::MapMission* done = world_.mission(id);
        if (!done || !done->keepsSpawns) {
          notice_ = "Você não pode repetir missões já completadas.";
          return;
        }
      }
      if (id == "estalagem") return enterInn();
      // Wisp Forest's entrance film, the first time the party goes into any of its missions (GameApp openMission).
      if (!save_.value("seenWispForestIntro", false)) {  // Modo teste too (its session save keeps "seen"; nothing is written)
        for (const ember::MapLocation& l : world_.locations()) {
          if (l.id != "wisp-forest" || std::find(l.missionIds.begin(), l.missionIds.end(), id) == l.missionIds.end()) continue;
          bool any = false;
          for (const std::string& m : l.missionIds) any = any || completed(m);
          if (any) break;
          playFilm("game/wisp-entrance.webm", [this, id] {
            save_["seenWispForestIntro"] = true;
            save_["pendingMission"] = nullptr;
            save_["battle"] = nullptr;
            write();
            openMission(id, true);
          });
          return;
        }
      }
    }
    std::string error;
    std::optional<MissionEntry> e;
    if (interlude_ && interlude_->hasMission(id)) e = interludeMission(paths_, id, error, test());  // the chapter's own maps (no campaign.json entry)
    else e = test() ? testMission(campaign_, paths_, id, error) : campaignMission(campaign_, paths_, id, error);
    if (e) briefing_ = std::move(e);
    else notice_ = error;
  }

  // GameApp beginMission("estalagem"): the first visit plays the arrival film (finishInnArrivalIntro), then the visit is
  // recorded (mission completed, the Inn's hex explored) and the party walks into the Inn's own room (its map, free roam:
  // startBattle on an explore mission); talking to Brue, Vargan or the old healer there opens their menus (NpcTalk.h).
  void enterInn() {
    if (!hasInn_) {
      notice_ = "Sem dados da Estalagem (content/data/inn.json).";
      return;
    }
    if (!save_.value("seenInnArrivalIntro", false)) {  // Modo teste too (its session save keeps "seen"; nothing is written)
      playFilm("game/inn-arrival.webm", [this] {
        save_["seenInnArrivalIntro"] = true;
        enterInn();
      });
      return;
    }
    if (!completed("estalagem")) save_["completed"].push_back("estalagem");
    for (const ember::MapLocation& l : world_.locations())
      if (l.id == "estalagem") {
        const std::string k = std::format("{},{}", l.col, l.row);
        bool seen = false;
        for (const ember::Json& h : save_.value("exploredHexes", ember::Json::array())) seen = seen || h == k;
        if (!seen) save_["exploredHexes"].push_back(k);
      }
    save_["pendingMission"] = nullptr;
    save_["battle"] = nullptr;
    write();
    std::string error;
    const std::optional<MissionEntry> room = test() ? testMission(campaign_, paths_, "estalagem", error) : campaignMission(campaign_, paths_, "estalagem", error);
    if (!room) notice_ = error;
    else if (callbacks_.play) callbacks_.play(*room);
  }

  // A film over everything, then `then`.
  // A story moment's affinity between two heroes, exactly the amount the user set (no leader catch-up): the conversations' pair
  // score (sorted key, one decimal, -100..100).
  void addAffinity(const std::string& a, const std::string& b, double delta) {
    const std::string pair = a < b ? a + "|" + b : b + "|" + a;
    ember::Json& scores = save_["affinityScores"];
    if (!scores.is_object()) scores = ember::Json::object();
    const double score = scores.contains(pair) && scores[pair].is_number() ? scores[pair].get<double>() : 0.0;
    scores[pair] = std::clamp(std::floor((score + delta) * 10 + 0.5) / 10, -100.0, 100.0);
  }

  void playFilm(const char* film, std::function<void()> then) {
    film_.emplace(paths_.artRoot, film);
    afterFilm_ = std::move(then);
  }

  // GameApp onOverworldStep: stepOverworld, written at once; a road battle opens its mission, anything else its notice.
  void step(int col, int row) {
    std::random_device device;
    ember::Mulberry32 rng(device());
    std::optional<ember::OverworldEvent> ev;
    ember::Json next = world_.step(save_, {col, row}, rng, ev, test());
    logInfo("map step: ({},{}) -> ({},{}): {}, day {} {}:00 -> day {} {}:00, event {}", save_["overworldPos"].value("col", 0),
            save_["overworldPos"].value("row", 0), col, row, next == save_ ? "REFUSED" : "taken", static_cast<long long>(num(save_, "gameClock", 0)),
            ember::Overworld::campaignHour(save_), static_cast<long long>(num(next, "gameClock", 0)), ember::Overworld::campaignHour(next),
            ev ? ev->kind + (ev->missionId.empty() ? std::string() : " " + ev->missionId) : std::string("-"));
    if (next == save_) return;
    save_ = std::move(next);
    write();
    if (ev && ev->kind == "battle" && !ev->missionId.empty()) {
      openMission(ev->missionId);
      return;
    }
    event_ = ev;
  }

  // hunger.ts useRation on each hero of the list; the party's hunger streak clears when everyone has eaten.
  int feedAll() {
    int fed = 0;
    for (const std::string& hero : partyHeroes()) {
      const double rations = num(save_, "rations", 0);
      const double hp = num(save_.value("unitHp", ember::Json::object()), hero, 1);
      const double full = ember::Overworld::fullness(save_.value("heroHunger", ember::Json::object()).value(hero, ember::Json()));
      if (rations < 1 || hp <= 0 || full >= 100) continue;
      save_["rations"] = static_cast<long long>(rations - 1);
      save_["heroHunger"][hero] = 100;
      ++fed;
    }
    if (fed && (num(save_, "hungerStreak", 0) > 0 || num(save_, "hungerHours", 0) > 0)) {
      bool all = true;
      for (const std::string& hero : partyHeroes()) {
        const double hp = num(save_.value("unitHp", ember::Json::object()), hero, baseMaxHp(hero));
        if (hp > 0 && !(num(save_.value("heroHunger", ember::Json::object()), hero, 100) > 0)) all = false;
      }
      if (all) save_["hungerStreak"] = 0, save_["hungerHours"] = 0;
    }
    if (fed) write();
    return fed;
  }

  void showHint(std::string text) {
    hint_ = std::move(text);
    hintAt_ = now_;
  }

  // --- the map ----------------------------------------------------------------------------------------------------------

  void drawMap(ImVec2 size) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    // radial-gradient(ellipse at 30% 20%, #1e1e1e 0%, #0b0b0b 70%)
    dl->AddRectFilled({0, 0}, size, IM_COL32(11, 11, 11, 255));
    for (int i = 12; i >= 1; --i) {
      const float k = static_cast<float>(i) / 12.0f;
      const int v = 11 + static_cast<int>(19 * (1 - k));
      dl->AddCircleFilled({size.x * 0.3f, size.y * 0.2f}, std::max(size.x, size.y) * 0.7f * k, IM_COL32(v, v, v, 40), 64);
    }
    const float headerH = drawHeader(size);
    drawViewport({0, headerH}, {size.x, size.y});
    drawPartyPanel(size);
    drawZoom(size);
    if (!hint_.empty() && now_ - hintAt_ < 1.2f) centredPlate(size, 96.0f, hint_, false);
    if (!notice_.empty() && centredPlate(size, 96.0f + (hint_.empty() || now_ - hintAt_ >= 1.2f ? 0.0f : 40.0f), notice_, true)) notice_.clear();
    drawTrainingHint(size);
    if (event_) drawEvent(size);
    if (open_) drawLocationPanel(size);
    if (questLog_) drawQuestLog(size);
    if (party_) drawParty(size);
    if (options_) drawOptions(size);
    if (fieldSpells_) drawFieldSpells(size);
    if (!warpHero_.empty()) drawWarpMenu(size);
    if (!recallHero_.empty()) drawRecallMenu(size);
    if (!identifyHero_.empty()) drawIdentifyMenu(size);
    drawWarpVisual(size);
    if (convDialog_) {  // a companion conversation, over everything: its replies change the affinity once
      const bool closed = convDialog_->draw(size, [this](const ember::Json& reply) {
        if (companions_.resolveReply(save_, reply, convLeader_)) write();
      });
      if (closed) convDialog_.reset();
    }
    if (!save_.value("seenOverworldIntro", false)) drawIntro(size);
  }

  // A small plate centred at `y` (hints, notices); true when clicked.
  bool centredPlate(ImVec2 size, float y, const std::string& text, bool button) {
    const float t = 12.0f;
    const ImVec2 ts = uiFont(UiFont::Ui)->CalcTextSizeA(t, FLT_MAX, 0.0f, text.c_str());
    ImGui::SetCursorScreenPos({(size.x - ts.x - 24.0f) * 0.5f, y});
    ImGui::PushID(button ? "notice" : "hint");
    const bool clicked = emberTextPlate("##p", {ts.x + 24.0f, ts.y + 12.0f}, text.c_str(), t);
    ImGui::PopID();
    return button && clicked;
  }

  // The header: kicker and title on the left, the plates on the right (wrapping like Ember's flex-wrap row).
  float drawHeader(ImVec2 size) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float top = 16.0f;
    float left = 16.0f;
    if (test()) {  // Modo teste: the back button (ember-icon-btn, ChevronLeft) before the title, to the test menu
      ImGui::SetCursorScreenPos({16.0f, top + 6.0f});
      if (emberPlate("##back", {40, 40}, [](ImDrawList* d, ImVec2 a, ImVec2 b, ImU32 ink) { iconChevronLeft(d, {(a.x + b.x) / 2, (a.y + b.y) / 2}, 20, ink); }) &&
          callbacks_.title)
        callbacks_.title();
      left += 40.0f + 12.0f;
    }
    emberKicker(dl, {left, top + 4.0f}, 14.0f, test() ? "MODO TESTE · RPG" : "CAMPANHA · RPG");
    // The world's name is the switch (the user, 2026-10-23: "near the upper left corner, you click the word Overworld and it switches
    // to Underworld"): Modo teste only (underworldOpen).
    {
      const char* word = underworld_ ? "Underworld" : "Overworld";
      ImFont* mark = uiFont(UiFont::Mark);
      const ImVec2 ts = mark->CalcTextSizeA(30.0f, FLT_MAX, 0.0f, word);
      const ImVec2 at{left, top + 22.0f};
      bool hover = false;
      if (underworldOpen()) {
        ImGui::SetCursorScreenPos(at);
        if (ImGui::InvisibleButton("##world", {ts.x, ts.y})) toggleWorld();
        hover = ImGui::IsItemHovered();
      }
      emberTitle(dl, at, 30.0f, word);
      if (hover) dl->AddLine({at.x, at.y + ts.y + 2.0f}, {at.x + ts.x, at.y + ts.y + 2.0f}, kAccent, 2.0f);
    }
    // The plates, right to left.
    ImFont* ui = uiFont(UiFont::Ui);
    const float h = 36.0f, gap = 12.0f;
    float x = size.x - 16.0f;
    const float y = top + 6.0f;
    auto place = [&](float w) {
      x -= w;
      const ImVec2 at{x, y};
      x -= gap;
      ImGui::SetCursorScreenPos(at);
    };
    // Gold
    const std::string gold = std::format("{} Gold", static_cast<long long>(num(save_, "ember", 0)));
    const float goldW = 8.0f + 24.0f + 6.0f + ui->CalcTextSizeA(14.0f, FLT_MAX, 0.0f, gold.c_str()).x + 8.0f;
    place(goldW);
    emberPlate("##gold", {goldW, h}, [&](ImDrawList* d, ImVec2 a, ImVec2 b, ImU32) {
      if (const UiImage* icon = uiImage(paths_.artRoot / "game/icons/gold-coins-pile.png"))
        imageContain(d, *icon, {a.x + 8, (a.y + b.y) * 0.5f - 12}, {a.x + 32, (a.y + b.y) * 0.5f + 12});  // object-contain
      d->AddText(ui, 14.0f, {a.x + 38, (a.y + b.y) * 0.5f - 8}, kMuted, gold.c_str());
    });
    // Opções
    place(h);
    if (emberPlate("##options", {h, h}, [&](ImDrawList* d, ImVec2 a, ImVec2 b, ImU32 ink) { iconGear(d, {(a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f}, 16, ink); }))
      options_ = true;
    // Som (the user, 2026-10-16: sound and options in the corner, as in Ember's other screens)
    place(h);
    soundButton("##sound", {h, h}, 16);
    // Feitiços de campo (Ember: the field-spells icon beside Opções)
    {
      place(h);
      const bool clicked = emberPlate("##fieldspellsbtn", {h, h}, [&](ImDrawList* d, ImVec2 a, ImVec2 b, ImU32) {
        if (const UiImage* icon = uiImage(paths_.artRoot / "game/icons/field-spells.png")) imageCover(d, *icon, {a.x + 3, a.y + 3}, {b.x - 3, b.y - 3});
      });
      if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tr("Feitiços de campo", "Field spells").c_str());
      if (clicked) fieldSpells_ = !fieldSpells_;
    }
    // Alimentar todos
    {
      const char* label = "Alimentar todos";
      const float w = ui->CalcTextSizeA(14.0f, FLT_MAX, 0.0f, label).x + 24.0f;
      place(w);
      if (emberTextPlate("##feed", {w, h}, label, 14.0f)) {
        const int heroes = static_cast<int>(partyHeroes().size());
        const int fed = feedAll();
        showHint(fed == 0 ? "Ninguém comeu — sem rações ou já saciados." : fed == heroes ? "Todos comeram." : std::format("{} comeram — rações não deram pros demais.", fed));
      }
    }
    // Fome
    if (const double streak = num(save_, "hungerStreak", 0); streak > 0) {
      const std::string text = std::format("Fome {}d", static_cast<long long>(streak));
      const float w = ui->CalcTextSizeA(14.0f, FLT_MAX, 0.0f, text.c_str()).x + 16.0f;
      place(w);
      emberPlate("##hunger", {w, h}, [&](ImDrawList* d, ImVec2 a, ImVec2 b, ImU32) { d->AddText(ui, 14.0f, {a.x + 8, (a.y + b.y) * 0.5f - 8}, kDanger, text.c_str()); });
    }
    // Rações
    {
      const std::string text = std::format("Rações {}", static_cast<long long>(num(save_, "rations", 0)));
      const float w = ui->CalcTextSizeA(14.0f, FLT_MAX, 0.0f, text.c_str()).x + 16.0f;
      place(w);
      emberPlate("##rations", {w, h}, [&](ImDrawList* d, ImVec2 a, ImVec2 b, ImU32) { d->AddText(ui, 14.0f, {a.x + 8, (a.y + b.y) * 0.5f - 8}, kMuted, text.c_str()); });
    }
    // The moon (moonPhase.ts: an 8-day cycle, every third full moon a blood moon)
    {
      place(h);
      const int day = static_cast<int>(num(save_, "gameClock", 0));
      static const char* kPhases[8] = {"new", "waxingCrescent", "firstQuarter", "waxingGibbous", "full", "waningGibbous", "lastQuarter", "waningCrescent"};
      const int d = std::max(0, day - 1);
      std::string phase = kPhases[d % 8];
      if (phase == "full" && (d / 8) % 3 == 2) phase = "blood";
      const std::string icon = phase == "full" ? "game/ui/moon/moon-full-v2.png" : "game/ui/moon/moon-" + phase + ".png";
      emberPlate("##moon", {h, h}, [&](ImDrawList* dd, ImVec2 a, ImVec2 b, ImU32) {
        if (const UiImage* img = uiImage(paths_.artRoot / icon)) dd->AddImage(img->texture, {a.x + 4, a.y + 4}, {b.x - 4, b.y - 4});
      });
    }
    // Clock
    {
      const std::string text = std::format("Dia {} · {:02}:00", static_cast<long long>(num(save_, "gameClock", 0)), ember::Overworld::campaignHour(save_));
      const float w = 8 + 16 + 8 + ui->CalcTextSizeA(14.0f, FLT_MAX, 0.0f, text.c_str()).x + 8;
      place(w);
      emberPlate("##clock", {w, h}, [&](ImDrawList* d, ImVec2 a, ImVec2 b, ImU32) {
        iconClock(d, {a.x + 16, (a.y + b.y) * 0.5f}, 16, kMuted);
        d->AddText(ui, 14.0f, {a.x + 32, (a.y + b.y) * 0.5f - 8}, kFg, text.c_str());
      });
    }
    // Missões
    {
      const float w = 12 + 16 + 8 + ui->CalcTextSizeA(14.0f, FLT_MAX, 0.0f, "Missões").x + 12;
      place(w);
      if (emberPlate("##quests", {w, 40.0f}, [&](ImDrawList* d, ImVec2 a, ImVec2 b, ImU32 ink) {
            iconBook(d, {a.x + 20, (a.y + b.y) * 0.5f}, 16, ink);
            d->AddText(ui, 14.0f, {a.x + 36, (a.y + b.y) * 0.5f - 8}, ink, "Missões");
          }))
        questLog_ = true;
    }
    // Conversas (Ember: the "Conversations" button of the map screens)
    if (hasCompanions_) {
      const std::string label = tr("Conversas", "Conversations");
      const float w = ui->CalcTextSizeA(14.0f, FLT_MAX, 0.0f, label.c_str()).x + 24;
      place(w);
      if (emberTextPlate("##conversations", {w, h}, label.c_str(), 14.0f)) party_ = true, partyTab_ = 2, formationSaved_.clear(), formation_.clear();
    }
    // Party
    {
      const float w = ui->CalcTextSizeA(14.0f, FLT_MAX, 0.0f, "Party").x + 20;
      place(w);
      if (emberTextPlate("##party", {w, h}, "Party", 14.0f)) party_ = true, partyTab_ = 0, formationSaved_.clear(), formation_.clear();
    }
    return top + 64.0f + 16.0f;
  }

  // The map itself: the painted world in a scrollable viewport (drag with the mouse, wheel scrolls), the fog, the
  // time-of-day tint, the pins, the step hexes and the leader.
  // The Underworld's map: the user's Atlas do Submundo de Mordávia (content/ui/underworld-atlas-v01.png), only the picture for now
  // (no locations, steps or fog there yet: it has no overworld board).
  // The user's new maps (2026-10-28: "UnderworldA" for the Underworld, "recraft-v4_Map" for the Overworld, Attachments), the
  // locations' pins at the same percent coordinates as before; the old pictures (content/ui/underworld-atlas-v01.png,
  // game/assets/world-map.jpg) stay where they are. v03 = UnderworldA4X (the user, 2026-10-28: the same painting at four times the
  // resolution, "só mudou a resolução"): the Underworld's hexes and marks are in percent of the atlas, so they stay where they were.
  static constexpr const char* kUnderworldAtlas = "content/worldmap/underworld-atlas-v03.png";
  // The picture: ember::kWorldMapPicture (Overworld.h names its version); the pins keep their percent coordinates.
  static constexpr const char* kWorldMap = ember::kWorldMapPicture;
  // Whether the word at the upper left switches worlds: Modo teste only, its "go anywhere" (the user, 2026-10-28: "no jogo haverão
  // algumas passagens, não se pode mudar assim, é só no debug"); the campaign crosses by the game's passages (showUnderworld).
  bool underworldOpen() const { return test(); }
  void toggleWorld() {
    underworld_ = !underworld_;
    std::swap(centreX_, otherCentreX_), std::swap(centreY_, otherCentreY_);  // each world keeps its own view
    recentre_ = true;
    movementOpen_ = false;
    partyX_ = -1;  // the leader stands on the shown world's hex at once (one walk state, shared by both worlds)
  }

  // The Underworld's board (world/Underworld, content/data/underworld.json; the user, 2026-10-28: "when I flip to the Underworld
  // Map I want my character in there at the starting position", "também anda"): the leader on the party's hex (Temple Descend the
  // first time), a click on the leader opens the neighbouring cells as step hexes, a click on one walks there (as the Overworld's
  // leader walks); Temple Descend's pin on the user's mark. A step moves the party and the campaign's clock (Underworld::step).
  void drawUnderworld(ImDrawList* dl, ImVec2 o, float imgW, float imgH, bool overViewport, bool clicked) {
    if (!hasUnder_) return;
    auto at = [&](double xPct, double yPct) { return ImVec2{o.x + static_cast<float>(xPct) / 100.0f * imgW, o.y + static_cast<float>(yPct) / 100.0f * imgH}; };
    const ImVec2 mouse = ImGui::GetIO().MousePos;
    const ember::OverworldHex pos = under_.position(save_);
    std::vector<ember::OverworldHex> dots;
    for (const ember::OverworldHex& n : ember::Overworld::neighbors(pos.col, pos.row))
      if (under_.isCell(n.col, n.row)) dots.push_back(n);
    std::optional<ember::OverworldHex> clickedDot;
    const ember::MapLocation* clickedPin = nullptr;
    bool clickedLeader = false;
    // Pins (the Overworld's look: a dark disc, the accent ring, the map-pin icon), the name on hover.
    for (const ember::MapLocation& loc : under_.locations()) {
      const ImVec2 c = at(loc.x, loc.y);
      const float r = 20.0f;
      const bool hover = overViewport && std::hypot(mouse.x - c.x, mouse.y - c.y) <= r;
      const float s = hover ? 1.1f : 1.0f;
      dl->AddCircleFilled(c, r * s, IM_COL32(17, 17, 17, 204), 32);
      dl->AddCircle(c, r * s - 1, kAccent, 32, 2.0f);
      iconMapPin(dl, c, 16 * s, IM_COL32(216, 216, 216, 255));
      if (hover) ImGui::SetTooltip("%s", loc.name.c_str());
      if (hover && clicked) clickedPin = &loc;
    }
    // Step hexes (movement open): the Overworld's shape and wash, at the board's own size.
    if (movementOpen_)
      for (const ember::OverworldHex& d : dots) {
        const ember::OverworldCell* cell = under_.cell(d.col, d.row);
        const ImVec2 c = at(cell->x, cell->y);
        const float w = std::max(44.0f, static_cast<float>(under_.hexWidth()) / 100.0f * imgW);
        const float h = std::max(44.0f, w * 2.0f / std::sqrt(3.0f));
        const ImVec2 p[6] = {{c.x, c.y - h / 2}, {c.x + w / 2, c.y - h / 4}, {c.x + w / 2, c.y + h / 4}, {c.x, c.y + h / 2}, {c.x - w / 2, c.y + h / 4}, {c.x - w / 2, c.y - h / 4}};
        const bool hover = overViewport && std::abs(mouse.x - c.x) < w / 2 && std::abs(mouse.y - c.y) < h / 2 - std::abs(mouse.x - c.x) / (w / 2) * h / 4;
        dl->AddConvexPolyFilled(p, 6, hover ? IM_COL32(216, 216, 216, 204) : IM_COL32(216, 216, 216, 122));
        if (hover) dl->AddPolyline(p, 6, kFg, 2.0f, ImDrawFlags_Closed);
        if (hover && clicked) clickedDot = d;
      }
    // The leader: walked hex to hex as on the Overworld, turned by the side the hex lies on.
    {
      double tx = 0, ty = 0;
      under_.hexToWorld(pos.col, pos.row, tx, ty);
      if (partyX_ < 0)
        partyX_ = static_cast<float>(tx), partyY_ = static_cast<float>(ty), fromX_ = partyX_, fromY_ = partyY_, targetX_ = partyX_,
        targetY_ = partyY_, moveAt_ = -10;
      if (std::abs(targetX_ - static_cast<float>(tx)) > 1e-3f || std::abs(targetY_ - static_cast<float>(ty)) > 1e-3f) {
        if (std::abs(static_cast<float>(tx) - targetX_) > 1e-3f) facingLeft_ = static_cast<float>(tx) < targetX_;
        fromX_ = partyX_, fromY_ = partyY_, targetX_ = static_cast<float>(tx), targetY_ = static_cast<float>(ty), moveAt_ = now_;
      }
      constexpr float kWalkSeconds = 1.0f;  // the Overworld's pace
      const float k = std::clamp((now_ - moveAt_) / kWalkSeconds, 0.0f, 1.0f);
      partyX_ = fromX_ + (targetX_ - fromX_) * k, partyY_ = fromY_ + (targetY_ - fromY_) * k;
      const Box box = drawLeader(dl, at(partyX_, partyY_), leader(), k < 1.0f ? now_ - moveAt_ : -1.0f);
      if (overViewport && box.contains(mouse) && clicked) clickedLeader = true;
    }
    if (clicked) {
      const ember::OverworldHex under = under_.worldToHex((mouse.x - o.x) / imgW * 100.0, (mouse.y - o.y) / imgH * 100.0);
      logInfo("underworld click: at ({},{}) movement {}, mouse on hex ({},{}) {} -> {}", pos.col, pos.row, movementOpen_ ? "open" : "closed",
              under.col, under.row, under_.isCell(under.col, under.row) ? "on the board" : "OFF the board",
              clickedDot ? std::format("step hex ({},{})", clickedDot->col, clickedDot->row)
              : clickedLeader ? std::string("leader")
              : clickedPin ? "pin " + clickedPin->id
                           : std::string("nothing"));
    }
    // A step there may bring one of the Underworld's road texts (Overworld::undergroundText; the user, 2026-10-29).
    auto underStep = [&](ember::OverworldHex to) {
      if (!under_.step(save_, to)) return;
      std::random_device device;
      ember::Mulberry32 rng(device());
      if (std::optional<ember::OverworldEvent> ev = world_.undergroundText(save_, rng, test())) event_ = ev;
      write();
    };
    if (clickedDot) {
      movementOpen_ = false;
      underStep(*clickedDot);
    } else if (clickedLeader) {
      movementOpen_ = !movementOpen_;
    } else if (clickedPin && movementOpen_ && std::ranges::find(dots, ember::OverworldHex{clickedPin->col, clickedPin->row}) != dots.end()) {
      movementOpen_ = false;
      underStep({clickedPin->col, clickedPin->row});
    }
  }

  // The status sheet from a portrait (the user, 2026-10-28: "when I click a portrait in the world map I also want it to open up the
  // status screen for that character"): the battle's own sheet (BattleStatus.h) over the party as a fight would seat it, built by
  // the same steps: fresh units of their campaign classes (partyBattle), then the save's levels and XP, promotions, class spells,
  // condition (heroStatesFromSave + applyHeroState) and what they wear (wearFromSave). Mochila / Equipar open the inventory and
  // come back to the sheet; a level-up point is written into the save as a fight writes it.
  void openSheet(const std::string& hero) {
    std::string error;
    const std::optional<ember::Json> party = ember::partyBattle(partyHeroes(), &error);
    if (!party) return logWarn("status sheet: {}", error);
    const std::filesystem::path file = std::filesystem::temp_directory_path() / "ember-party-sheet.battle.json";
    std::ofstream(file, std::ios::binary) << party->dump();
    sheetBattle_ = ember::Battle::load(file);
    if (!sheetBattle_) return;
    const ember::Json levels = save_.value("levels", ember::Json::object()), xp = save_.value("xp", ember::Json::object());
    const ember::Json promotions = save_.value("promotions", ember::Json::object());
    const auto states = ember::heroStatesFromSave(save_);
    std::optional<size_t> at;
    for (size_t i = 0; i < sheetBattle_->units().size(); ++i) {
      ember::BattleUnit& u = sheetBattle_->units()[i];
      if (u.side != "player") continue;
      if (levels.contains(u.name) && levels[u.name].is_number_integer()) ember::raiseToLevel(u, levels[u.name].get<int>(), xp.value(u.name, 0));
      if (promotions.contains(u.name) && promotions[u.name].is_string()) ember::setPromotion(u, promotions[u.name].get<std::string>());
      ember::grantClassSpells(u);
      if (const auto it = states.find(u.name); it != states.end()) MapStage::applyHeroState(u, it->second);
      if (hasInn_) MapStage::wearFromSave(u, save_, innRules_);
      if (u.name == hero) at = i;
    }
    if (at) sheet_.open(*at);
  }
  void drawSheet() {
    if (!sheet_.isOpen() || !sheetBattle_) return;
    ember::BattleStatusSheet::Hooks hooks;
    hooks.portraitWhole = [](const ember::BattleUnit& u) { return u.side == "player"; };
    hooks.portrait = [this](const ember::BattleUnit& u) -> const UiImage* {
      const std::string file = unitPortraitFile(u.name, u.sprite);
      std::error_code ec;
      return file.empty() || !std::filesystem::exists(paths_.artRoot / file, ec) ? nullptr : uiImage(paths_.artRoot / file);
    };
    hooks.spellIcon = [this](const std::string& kind) -> const UiImage* {
      const std::filesystem::path file = MapStage::spellIconFile(paths_.artRoot, kind);
      return file.empty() ? nullptr : uiImage(file);
    };
    hooks.xpToLevel = [](int level) { return MapStage::xpToLevel(level); };
    hooks.allocation = [](const ember::BattleUnit& u) { return std::array<int, 5>{u.pointHp, u.pointAtk, u.pointMag, u.pointDef, u.pointDex}; };
    hooks.adjust = [this](size_t unit, int stat, int delta) { return adjustSheetPoint(unit, stat, delta); };
    if (hasInn_) {
      auto open = [this](size_t unit, bool pack) {
        const std::string name = sheetBattle_->units()[unit].name;
        sheet_.close();
        sheetReturn_ = name;
        inventory_.emplace(name, pack);
      };
      hooks.openPack = [open](size_t unit) { open(unit, true); };
      hooks.openEquip = [open](size_t unit) { open(unit, false); };
      hooks.gearLines = [this](const ember::BattleUnit& u, const char* stat) { return innRules_.gearLines(save_, u.name, stat); };
      hooks.potionTip = [this](const std::string& id) -> std::string {
        const ember::InnPotion* p = innData_.potion(id);
        return p ? p->tooltip : std::string();
      };
      hooks.lockpickTip = [this] { return innData_.lockpickTooltip; };
    }
    hooks.order = [this] {
      std::vector<size_t> order;
      for (size_t i = 0; i < sheetBattle_->units().size(); ++i)
        if (sheetBattle_->units()[i].side == "player") order.push_back(i);
      return order;
    };
    sheet_.draw(*sheetBattle_, paths_.artRoot, hooks);
  }
  // MapStage adjustStatPoint's rule (three points per level past the first; a point on vitality heals as much), the allocation written
  // into the save (statPointAllocations), as a campaign fight writes it; Modo teste into its session save.
  bool adjustSheetPoint(size_t b, int stat, int delta) {
    if (!sheetBattle_ || b >= sheetBattle_->units().size() || stat < 0 || stat > 4) return false;
    ember::BattleUnit& u = sheetBattle_->units()[b];
    if (u.side != "player") return false;
    int* slot[5] = {&u.pointHp, &u.pointAtk, &u.pointMag, &u.pointDef, &u.pointDex};
    const int spent = u.pointHp + u.pointAtk + u.pointMag + u.pointDef + u.pointDex, budget = std::max(0, (u.level - 1) * 3);
    if ((delta > 0 && spent >= budget) || (delta < 0 && *slot[stat] <= 0)) return false;
    const int previousMax = u.maxHp;
    *slot[stat] += delta;
    ember::reapplyGear(u);
    if (stat == 0 && delta > 0) u.hp = std::min(u.maxHp, u.hp + (u.maxHp - previousMax));
    static const char* kNames[5] = {"hp", "atk", "mag", "def", "dex"};
    ember::Json& all = save_["statPointAllocations"];
    if (!all.is_object()) all = ember::Json::object();
    ember::Json& mine = all[u.name];
    if (!mine.is_object()) mine = ember::Json::object();
    if (*slot[stat] > 0) mine[kNames[stat]] = *slot[stat];
    else mine.erase(kNames[stat]);
    write();
    return true;
  }

  void drawViewport(ImVec2 a, ImVec2 b) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const UiImage* map = uiImage(std::filesystem::path(underworld_ ? kUnderworldAtlas : kWorldMap));
    mapDrawn_ = true;
    const float vw = b.x - a.x, vh = b.y - a.y;
    // The Underworld zooms exactly as the Overworld (the user, 2026-10-28: "o sistema de zoom do underworld precisa ser IGUAL
    // ao overworld"): the same stops and the same box; each world keeps its own stop.
    const float zoomPercent = kOverworldZoomStops[underworld_ ? underworldZoom_ : zoom_];
    const float imgW = vw * zoomPercent / 100.0f;
    const float imgH = map ? imgW * static_cast<float>(map->height) / static_cast<float>(map->width) : vh * 0.7f;
    const float contentW = imgW + 16.0f, contentH = imgH + 16.0f;
    const ember::Json& pos = save_["overworldPos"];
    const int col = pos.value("col", 0), row = pos.value("row", 0);
    double px = 0, py = 0;
    ember::Overworld::hexToWorld(col, row, px, py);
    if (underworld_) {
      const ember::OverworldHex u = under_.position(save_);
      under_.hexToWorld(u.col, u.row, px, py);
    }
    // Opens centred on the party (mount), keeps the centre when zooming; each world the first time it shows.
    if (bool& done = underworld_ ? underCentred_ : centred_; !done) {
      centreX_ = static_cast<float>(px / 100.0), centreY_ = static_cast<float>(py / 100.0), done = true, recentre_ = true;
    }
    if (recentre_) {
      scrollX_ = centreX_ * contentW - vw / 2, scrollY_ = centreY_ * contentH - vh / 2;
      recentre_ = false;
    }
    scrollX_ = std::clamp(scrollX_, 0.0f, std::max(0.0f, contentW - vw));
    scrollY_ = std::clamp(scrollY_, 0.0f, std::max(0.0f, contentH - vh));
    // Input over the viewport: drag (moved > 4 px) pans, wheel scrolls (shift: sideways).
    ImGui::SetCursorScreenPos(a);
    ImGui::SetNextItemAllowOverlap();  // the zoom plates drawn after the viewport take their own clicks
    ImGui::InvisibleButton("##viewport", {vw, vh}, ImGuiButtonFlags_MouseButtonLeft);
    const bool overViewport = ImGui::IsItemHovered() && !worldMapZoomBoxHovered(b);
    const ImGuiIO& io = ImGui::GetIO();
    if (ImGui::IsItemActivated()) drag_ = {io.MousePos.x, io.MousePos.y, scrollX_, scrollY_}, dragMoved_ = false;
    if (ImGui::IsItemActive() && drag_) {
      requestGrabCursor();  // Ember world-map-pan-grab: the gauntlet from the press on
      const float dx = io.MousePos.x - drag_->x, dy = io.MousePos.y - drag_->y;
      if (!dragMoved_ && (std::abs(dx) > 4 || std::abs(dy) > 4)) dragMoved_ = true;
      if (dragMoved_) scrollX_ = drag_->sx - dx, scrollY_ = drag_->sy - dy;
    }
    if (ImGui::IsItemDeactivated()) drag_.reset();
    if (overViewport && io.MouseWheel != 0.0f) {
      if (io.KeyShift) scrollX_ -= io.MouseWheel * 100.0f;
      else scrollY_ -= io.MouseWheel * 100.0f;
    }
    // As setas do teclado dão pan no mapa (o usuário, 2026-10-27), enquanto nenhum painel está aberto nem um campo de texto tem o foco.
    if (!io.WantTextInput && !open_ && !party_ && !questLog_ && !options_ && !slots_ && !briefing_ && !fieldSpells_ && !convDialog_) {
      const float pan = 700.0f * std::clamp(io.DeltaTime, 0.0f, 0.1f);
      if (ImGui::IsKeyDown(ImGuiKey_LeftArrow)) scrollX_ -= pan;
      if (ImGui::IsKeyDown(ImGuiKey_RightArrow)) scrollX_ += pan;
      if (ImGui::IsKeyDown(ImGuiKey_UpArrow)) scrollY_ -= pan;
      if (ImGui::IsKeyDown(ImGuiKey_DownArrow)) scrollY_ += pan;
    }
    scrollX_ = std::clamp(scrollX_, 0.0f, std::max(0.0f, contentW - vw));
    scrollY_ = std::clamp(scrollY_, 0.0f, std::max(0.0f, contentH - vh));
    const bool clicked = overViewport && ImGui::IsMouseReleased(ImGuiMouseButton_Left) && !dragMoved_;
    centreX_ = (scrollX_ + vw / 2) / contentW, centreY_ = (scrollY_ + vh / 2) / contentH;

    const ImVec2 o{a.x + 8.0f - scrollX_, a.y + 8.0f - scrollY_};  // the image's corner
    auto at = [&](double xPct, double yPct) { return ImVec2{o.x + static_cast<float>(xPct) / 100.0f * imgW, o.y + static_cast<float>(yPct) / 100.0f * imgH}; };
    dl->PushClipRect(a, b, true);
    // A luz e a névoa (render/WorldMapFx.h): a pintura desenhada pelo passe próprio com a luz da hora / dos seres, a névoa de
    // guerra viva, a névoa baixa / de caverna e o acabamento de cor; com o arquivo em "enabled": false, o desenho de antes.
    const int fxWorld = underworld_ ? 1 : 0;
    const bool fxOn = fx_ && fxConfig_[fxWorld].enabled && fxMap_[fxWorld] != WorldMapFx::kNoTexture;
    if (fxOn) drawLitMap(dl, o, imgW, imgH, a, b);
    else if (map) dl->AddImageRounded(map->texture, o, {o.x + imgW, o.y + imgH}, {0, 0}, {1, 1}, IM_COL32_WHITE, 8.0f);
    if (underworld_) {  // the Atlas: no fog of war, no sky (it is underground); its board, Temple Descend and the leader
      drawUnderworld(dl, o, imgW, imgH, overViewport, clicked);
      dl->PopClipRect();
      return;
    }
    if (!fxOn) {
      drawFog(dl, o, imgW, imgH);
      // Sky tint by the hour.
      const std::string t = ember::Overworld::timeOfDay(ember::Overworld::campaignHour(save_));
      const ImU32 tint = t == "darkNight" ? IM_COL32(5, 12, 43, 173) : t == "brightNight" ? IM_COL32(34, 62, 132, 122) : t == "dusk" ? IM_COL32(166, 67, 39, 82)
                         : t == "dawn" ? IM_COL32(230, 137, 112, 64) : t == "noon" ? IM_COL32(255, 197, 108, 31) : 0;
      if (tint) dl->AddRectFilled(o, {o.x + imgW, o.y + imgH}, tint, 8.0f);
    }
    // Reachable: the party's hex and every neighbour it may step to.
    std::vector<ember::OverworldHex> dots;
    for (const ember::OverworldHex& n : ember::Overworld::neighbors(col, row))
      if (world_.isCell(n.col, n.row) && world_.canStep(save_, {col, row}, n, test())) dots.push_back(n);
    auto walkable = [&](int c, int r) {
      if (c == col && r == row) return true;
      return std::any_of(dots.begin(), dots.end(), [&](const ember::OverworldHex& d) { return d.col == c && d.row == r; });
    };
    const ember::MapLocation* standing = world_.locationAt(col, row);
    std::string clickedLocation;
    std::optional<ember::OverworldHex> clickedDot;
    bool clickedLeader = false;
    const ImVec2 mouse = io.MousePos;
    // Pins.
    std::vector<std::pair<const ember::MapLocation*, std::string>> pins;
    for (const ember::MapLocation& loc : world_.locations()) {
      if (!test() && !world_.locationVisible(loc, save_)) continue;  // GameApp mapVisibleLocations: test mode shows all
      // isExplored: Stone Bridge before O Vau, the Inn, where the party stands, or a hex it has stood on (test: all).
      bool explored = test() || (!completed("vau") && loc.id == "stonebridge") || loc.id == "estalagem" || (standing && standing->id == loc.id);
      for (const ember::Json& h : save_.value("exploredHexes", ember::Json::array()))
        if (h.is_string() && h.get<std::string>() == std::format("{},{}", loc.col, loc.row)) explored = true;
      if (!explored) continue;
      pins.push_back({&loc, locationStatus(loc)});
    }
    for (const auto& [loc, st] : pins) {
      const ImVec2 c = at(loc->x, loc->y);
      const bool reachable = test() || walkable(loc->col, loc->row) || (!completed("vau") && loc->id == "stonebridge");
      const float r = 20.0f;
      const bool hover = std::hypot(mouse.x - c.x, mouse.y - c.y) <= r && overViewport;
      const float s = hover ? 1.1f : 1.0f;
      const int alpha = reachable ? 255 : 178;
      const auto missions = world_.visibleMissionsOf(*loc, save_, test());
      const bool hub = std::any_of(missions.begin(), missions.end(), [](const ember::MapMission* m) { return m->hub; });
      ImU32 border = kAccent;
      if (st == "locked") border = IM_COL32(74, 74, 74, 128);
      else if (st != "done" && hub) {
        const float k = 0.5f - 0.5f * std::cos(now_ / 2.8f * 2 * std::numbers::pi_v<float>);
        border = IM_COL32(static_cast<int>(73 + (201 - 73) * k), static_cast<int>(73 + (160 - 73) * k), static_cast<int>(73 + (112 - 73) * k), 255);
        for (int i = 3; i >= 1; --i) dl->AddCircle(c, r * s + i * 3.0f, IM_COL32(201, 150, 88, static_cast<int>((22 + 28 * k) * 0.6f)), 32, 3.0f);
      }
      if (flashId_ == loc->id && now_ - flashAt_ < 0.5f) {
        const float k = std::sin((now_ - flashAt_) / 0.5f * std::numbers::pi_v<float>);
        border = IM_COL32(214, 60, 60, 255);
        for (int i = 3; i >= 1; --i) dl->AddCircle(c, r + i * 3.0f, IM_COL32(214, 60, 60, static_cast<int>(70 * k)), 32, 3.0f);
      }
      dl->AddCircleFilled(c, r * s, IM_COL32(17, 17, 17, alpha * 204 / 255), 32);
      dl->AddCircle(c, r * s - 1, (border & ~IM_COL32_A_MASK) | (static_cast<ImU32>(((border >> IM_COL32_A_SHIFT) & 0xFF) * alpha / 255) << IM_COL32_A_SHIFT), 32, 2.0f);
      const ImU32 iconInk = st == "locked" ? IM_COL32(180, 180, 180, alpha / 2) : IM_COL32(216, 216, 216, alpha);
      if (st == "locked") iconLock(dl, c, 16 * s, iconInk);
      else if (st == "done") iconCheck(dl, c, 16 * s, iconInk);
      else iconMapPin(dl, c, 16 * s, iconInk);
      if (missions.size() > 1) {
        const ImVec2 bc{c.x + r * s - 2, c.y - r * s + 2};
        dl->AddCircleFilled(bc, 8, IM_COL32(17, 17, 17, 255));
        dl->AddCircle(bc, 8, kBorder);
        const std::string n = std::to_string(missions.size());
        const ImVec2 ts = uiFont(UiFont::Ui)->CalcTextSizeA(10.0f, FLT_MAX, 0, n.c_str());
        dl->AddText(uiFont(UiFont::Ui), 10.0f, {bc.x - ts.x / 2, bc.y - ts.y / 2}, IM_COL32(240, 240, 240, 230), n.c_str());
      }
      if (hover && clicked) clickedLocation = loc->id;
    }
    // Step hexes (movement open): the hex shape, the accent wash, the travel time.
    if (movementOpen_)
      for (const ember::OverworldHex& d : dots) {
        const ember::OverworldCell* cell = world_.cell(d.col, d.row);
        const ImVec2 c = at(cell->x, cell->y);
        const float hs = static_cast<float>(ember::Overworld::hexSize()), floor = 44.0f * hs / 5.0f;
        const float w = std::max(floor, std::sqrt(3.0f) * hs / 100.0f * imgW), h = std::max(floor, 2.0f * hs / 100.0f * imgH);
        const ImVec2 p[6] = {{c.x, c.y - h / 2}, {c.x + w / 2, c.y - h / 4}, {c.x + w / 2, c.y + h / 4}, {c.x, c.y + h / 2}, {c.x - w / 2, c.y + h / 4}, {c.x - w / 2, c.y - h / 4}};
        const bool hover = overViewport && std::abs(mouse.x - c.x) < w / 2 && std::abs(mouse.y - c.y) < h / 2 - std::abs(mouse.x - c.x) / (w / 2) * h / 4;
        dl->AddConvexPolyFilled(p, 6, hover ? IM_COL32(216, 216, 216, 204) : IM_COL32(216, 216, 216, 122));
        if (hover) dl->AddPolyline(p, 6, kFg, 2.0f, ImDrawFlags_Closed);
        const std::string label = ember::Overworld::travelTimeLabel(cell->hours);
        const ImVec2 ts = uiFont(UiFont::Ui)->CalcTextSizeA(12.0f, FLT_MAX, 0, label.c_str());
        dl->AddText(uiFont(UiFont::Ui), 12.0f, {c.x - ts.x / 2 + 1, c.y - ts.y / 2 + 1}, IM_COL32(17, 17, 17, 255), label.c_str());
        dl->AddText(uiFont(UiFont::Ui), 12.0f, {c.x - ts.x / 2, c.y - ts.y / 2}, kFg, label.c_str());
        if (hover && clicked) clickedDot = d;
        if (hover) ImGui::SetTooltip("Saciedade: −%s", std::format("{}", ember::Overworld::travelHungerCost(cell->hours)).c_str());
      }
    // The leader: idle frames of their own sprite, sliding hex to hex (0.5 s), turned the way they last walked.
    {
      double tx = px, ty = py;
      // On a location the leader stands on its pin; on the half grid (hexes 2.5 %) a pin can be a whole hex off its cell's centre,
      // so there the leader stays on the cell, in the middle of its step hexes.
      if (standing && ember::Overworld::hexSize() >= 5.0) tx = standing->x, ty = standing->y;
      // The first frame stands on the hex in idle: the target too (left at -1 it read as a step and played a walk in place).
      if (partyX_ < 0)
        partyX_ = static_cast<float>(tx), partyY_ = static_cast<float>(ty), fromX_ = partyX_, fromY_ = partyY_, targetX_ = partyX_,
        targetY_ = partyY_, moveAt_ = -10;
      // Compared as the floats they are stored in: a double hex centre against its own float copy differs by more than 1e-6 on some
      // hexes ((4,7): 38.97114317 vs 38.97114182), which restarted the walk every frame and left the leader drawn on the hex before
      // while the party (and its step hexes) stood on the new one (the user, 2026-10-28: "o grid saiu do lugar").
      if (std::abs(targetX_ - static_cast<float>(tx)) > 1e-3f || std::abs(targetY_ - static_cast<float>(ty)) > 1e-3f) {
        // Turned by where the hex lies on the painting, never by its column: rows are offset (odd-r), so a step down to the right
        // from an even row keeps its column and left the leader facing the old way, walking backwards (the user, 2026-10-28:
        // "as vezes eles andam de costas quando estou descendo ... é randomico"). Every hex step moves sideways by half a hex.
        if (std::abs(static_cast<float>(tx) - targetX_) > 1e-3f) facingLeft_ = static_cast<float>(tx) < targetX_;
        fromX_ = partyX_, fromY_ = partyY_, targetX_ = static_cast<float>(tx), targetY_ = static_cast<float>(ty), moveAt_ = now_;
        prevCol_ = col;
      }
      // Walked, not slid (the user, 2026-10-21: "walk instead of jumping to a new hex"): a steady pace over kWalkSeconds with the
      // leader's own walk clip playing.
      constexpr float kWalkSeconds = 1.0f;
      const float k = std::clamp((now_ - moveAt_) / kWalkSeconds, 0.0f, 1.0f);
      partyX_ = fromX_ + (targetX_ - fromX_) * k, partyY_ = fromY_ + (targetY_ - fromY_) * k;
      const ImVec2 foot = at(partyX_, partyY_);
      const Box box = drawLeader(dl, foot, leader(), k < 1.0f ? now_ - moveAt_ : -1.0f);
      if (!slowSteps_.empty() && frames_ % 15 == 0)
        logInfo("leader: party at ({},{}), drawn at {:.1f} %, {:.1f} % (target {:.1f}, {:.1f}; hex centre {:.1f}, {:.1f}), walk {:.2f}, box {:.1f}..{:.1f} %, {:.1f}..{:.1f} %",
                col, row, partyX_, partyY_, targetX_, targetY_, px, py, k, (box.a.x - o.x) / imgW * 100, (box.b.x - o.x) / imgW * 100,
                (box.a.y - o.y) / imgH * 100, (box.b.y - o.y) / imgH * 100);
      // walkslow: the next step, clicked as the player would (the leader, then the hex), once the walk before it is over
      if (slowNext_ < slowSteps_.size() && frames_ >= slowLast_ + 45) {
        movementOpen_ = true;
        walkTo(slowSteps_[slowNext_].col, slowSteps_[slowNext_].row);
        if (briefing_) logInfo("walkslow: road battle {} closed (Voltar)", briefing_->id), briefing_.reset();
        event_.reset();
        ++slowNext_, slowLast_ = frames_;
      }
      if (overViewport && box.contains(mouse) && clicked) clickedLeader = true;
    }
    dl->PopClipRect();

    // Clicks: a step hex wins over the leader's button (the leader's own hex is never a step hex, so nothing is lost; a leader
    // drawn over a neighbouring hex used to swallow the click and the party could not walk there).
    // Every click on the map written to the log (the user, 2026-10-28: the walk that "pretends and then locks"; JOGAR-log.txt says
    // what happened: what was clicked, from where to where, whether the step was taken, where the party stands after).
    if (clicked) {
      const int c0 = save_["overworldPos"].value("col", 0), r0 = save_["overworldPos"].value("row", 0);
      // The hex of the board under the mouse (pointy-top, the board's hex size in % of the image, odd-r: overworld.ts worldToHex),
      // and whether it is a cell.
      const double mx = (mouse.x - o.x) / imgW * 100.0, my = (mouse.y - o.y) / imgH * 100.0, hs = ember::Overworld::hexSize();
      const double fq = (std::sqrt(3.0) / 3.0 * mx - my / 3.0) / hs, fr = (2.0 / 3.0 * my) / hs, fs = -fq - fr;
      double rq = std::round(fq), rr = std::round(fr), rs = std::round(fs);
      if (std::abs(rq - fq) > std::abs(rr - fr) && std::abs(rq - fq) > std::abs(rs - fs)) rq = -rr - rs;
      else if (std::abs(rr - fr) > std::abs(rs - fs)) rr = -rq - rs;
      const int hr = static_cast<int>(rr), hc = static_cast<int>(rq) + (hr - (hr & 1)) / 2;
      logInfo("map click: mouse on the map at {:.1f} %, {:.1f} % = hex ({},{}) {}, a neighbour of the party: {}", mx, my, hc, hr,
              world_.isCell(hc, hr) ? "on the board" : "OFF the board",
              std::ranges::any_of(ember::Overworld::neighbors(c0, r0), [&](const ember::OverworldHex& n) { return n.col == hc && n.row == hr; }) ? "yes" : "no");
      logInfo("map click: at ({},{}) movement {} -> {}", c0, r0, movementOpen_ ? "open" : "closed",
              clickedDot ? std::format("step hex ({},{})", clickedDot->col, clickedDot->row)
              : clickedLeader && !(standing && clickedLocation == standing->id) ? std::string("leader")
              : !clickedLocation.empty() ? "pin " + clickedLocation
                                         : std::string("nothing (no step hex, pin or leader under the mouse)"));
    }
    // The pin of the place the party stands on wins over the leader drawn on it (the leader's body above the pin still opens the
    // step hexes): on the half grid the leader stands right on the pin and swallowed every click meant to enter the place.
    const bool ownPin = standing && clickedLocation == standing->id;
    if (clickedDot) walkTo(clickedDot->col, clickedDot->row);
    else if (clickedLeader && !ownPin) movementOpen_ = !movementOpen_;
    else if (!clickedLocation.empty()) {
      for (const auto& [loc, st] : pins)
        if (loc->id == clickedLocation) pinClicked(*loc, st, walkable(loc->col, loc->row), standing);
    }
  }

  void walkTo(int col, int row) {
    if (!movementOpen_) return;
    const ember::Json& pos = save_["overworldPos"];
    if (!world_.canStep(save_, {pos.value("col", 0), pos.value("row", 0)}, {col, row}, test())) return;
    movementOpen_ = false;
    step(col, row);
  }

  void pinClicked(const ember::MapLocation& loc, const std::string& st, bool walkable, const ember::MapLocation* standing) {
    if (!test() && !completed("vau") && loc.id == "stonebridge") return openMission("vau");
    if (standing && standing->id == loc.id) return enterLocation(loc, st);
    // A pin on a neighbouring hex: the step there (its hours, its road event), then the place opens unless the road stopped it.
    if (walkable) {
      movementOpen_ = false;
      step(loc.col, loc.row);
      const ember::Json& pos = save_["overworldPos"];
      if (pos.value("col", 0) == loc.col && pos.value("row", 0) == loc.row && !event_ && !briefing_) enterLocation(loc, st);
      return;
    }
    if (!test()) return showHint("Ande até lá primeiro.");
    // Modo teste: a distant pin is a free jump (teleportOverworld), then the place opens.
    save_ = ember::Overworld::teleport(save_, {loc.col, loc.row});
    write();
    enterLocation(loc, st);
  }

  void enterLocation(const ember::MapLocation& loc, const std::string& st) {
    if (loc.deadlineDay && num(save_, "gameClock", 0) > *loc.deadlineDay) return showHint("Prazo esgotado por aqui.");
    // The Ruins (docs/INTERLUDE.md): shut until Salazar's quests are done; the first legitimate entry starts the Interlude (the present
    // party put aside, the historical one in, its first map pending), a chapter left half-way goes on at its pending map, and once the
    // chapter is completed the Ruins open as any present-day location. Modo teste: the gate open (everything unlocked), the chapter
    // the same, on the session's own save.
    if (interlude_ && loc.id == interlude_->ruinsLocation) {
      if (!ember::ruinsOpen(*interlude_, world_, save_, test())) {
        flashId_ = loc.id, flashAt_ = now_;
        logInfo("ruins: shut ({})", ember::ruinsLockedReason(*interlude_, world_, save_));
        return showHint(tr("As Ruínas estão fechadas: termine antes as missões de Salazar.", "The Ruins are shut: finish Salazar's quests first."));
      }
      const ember::InterludeState state = ember::interludeState(save_);
      if (state == ember::InterludeState::NotStarted) {
        std::string why;
        if (!ember::interludeBegin(*interlude_, save_, &why)) return showHint("Interlúdio: " + why);
        write();
        logInfo("interlude: begun; the historical party is in, {} pending", interlude_->missions.front());
        return openMission(interlude_->missions.front(), true);
      }
      if (state == ember::InterludeState::Active) {
        const std::string id = ember::interludeMission(*interlude_, save_);
        if (!id.empty()) return openMission(id, true);
      }
    }
    if (st == "locked") {
      flashId_ = loc.id, flashAt_ = now_;
      return;
    }
    const auto missions = world_.visibleMissionsOf(loc, save_, test());
    if (missions.size() > 1) {
      open_ = loc.id;
      return;
    }
    if (!missions.empty()) {
      const ember::MapMission* m = missions[0];
      std::vector<std::string> order;
      for (const ember::MapMission* x : world_.missionsOf(loc)) order.push_back(x->id);
      if (m->crossing && world_.missionStatus(m->id, save_, order, test()) == "done") open_ = loc.id;
      else openMission(m->id);
    }
  }

  // A pintura com a luz e a névoa (WorldMapFx): o revelado de hoje (os mesmos círculos de drawFog, numa textura refeita só
  // quando os hexágonos explorados mudam), a hora do relógio suavizada na tela, o ciclo dos seres no Submundo.
  void drawLitMap(ImDrawList* dl, ImVec2 o, float w, float h, ImVec2 viewA, ImVec2 viewB) {
    const WorldMapConfig& c = fxConfig_[underworld_ ? 1 : 0];
    WorldMapFx::Draw d;
    d.rectMin = o, d.rectMax = {o.x + w, o.y + h}, d.viewMin = viewA, d.viewMax = viewB;
    d.map = fxMap_[underworld_ ? 1 : 0], d.mask = fxMask_[underworld_ ? 1 : 0];
    d.underworld = underworld_;
    d.quality = gfxQuality();
    d.time = now_;
    const double days = num(save_, "gameClock", 0), hour = ember::Overworld::campaignHour(save_);
    if (underworld_ && !c.luts.empty()) {
      // O look 2D no ciclo invertido (2026-10-28): a mesma hora do relógio, a floração na noite da superfície.
      const WorldMapFx::LutMix lm = WorldMapFx::lutMix(c, static_cast<float>(hour));
      if (lm.a >= 0 && lm.b >= 0 && size_t(lm.a) < fxLuts_[1].size() && size_t(lm.b) < fxLuts_[1].size()) d.lutA = fxLuts_[1][lm.a], d.lutB = fxLuts_[1][lm.b];
      for (int i = 0; i < WorldMapFx::Draw::kTextures; ++i) d.tex[i] = fxTex_[1][i];
      d.glow = WorldMapFx::bloomAt(c, static_cast<float>(hour));
      d.params = WorldMapFx::underworldLookParams(c, static_cast<float>(hour));
    } else if (underworld_) {
      // O relógio da campanha nunca para: o ciclo corre junto com ele e o Submundo abre no ponto da hora atual.
      d.glow = WorldMapFx::underworldGlow(c, days * 24.0 + hour);
      d.params = WorldMapFx::underworldParams(c, d.glow);
    } else {
      // Sem saltos: a luz anda até a hora do relógio em hourEaseSeconds (o relógio anda em horas inteiras a cada passo).
      const float target = static_cast<float>(hour);
      if (shownHour_ < 0) shownHour_ = target;
      const float dt = std::clamp(now_ - lastHourAt_, 0.0f, 0.1f);
      lastHourAt_ = now_;
      float diff = std::fmod(target - shownHour_ + 36.0f, 24.0f) - 12.0f;  // o caminho mais curto no ciclo
      const float step = c.hourEase > 0.0f ? dt / c.hourEase * 12.0f : 24.0f;
      if (std::abs(diff) <= step) shownHour_ = target;
      else shownHour_ = std::fmod(shownHour_ + (diff > 0 ? step : -step) + 24.0f, 24.0f);
      // O look 2D: as duas LUTs da hora, as máscaras, o grupo na pintura (partyX_ / partyY_ em %) e a luz do grupo (Opções).
      const WorldMapFx::LutMix lm = WorldMapFx::lutMix(c, shownHour_);
      if (lm.a >= 0 && lm.b >= 0 && size_t(lm.a) < fxLuts_[0].size() && size_t(lm.b) < fxLuts_[0].size()) d.lutA = fxLuts_[0][lm.a], d.lutB = fxLuts_[0][lm.b];
      for (int i = 0; i < WorldMapFx::Draw::kTextures; ++i) d.tex[i] = fxTex_[0][i];
      d.partyLight = gamePrefs().partyLight;
      d.params = WorldMapFx::overworldParams(c, shownHour_, {partyX_ / 100.0f, partyY_ / 100.0f});
      updateReveal();
    }
    fx_->draw(dl, d);
  }

  // What one explored hex clears: a soft circle (radius 9 % of the image, solid to 65 %) around it; a hex within two hex steps
  // of an edge of the painting (2 columns from left / right, 2 rows from top / bottom) clears all the way to that edge (the
  // user, 2026-10-28: "as bordas do mapa se revelarem por completo quando explorar as regiões"): the distance is taken to the
  // box from the hex to the edges it is near, so the band beyond it opens with the same soft sides (both edges at a corner).
  struct RevealSpot {
    double x0, x1, y0, y1;
  };
  static RevealSpot revealSpot(double cx, double cy) {
    // two steps of the old 5 % hexes (hexToWorld), kept on the half grid so the edges reveal as before
    constexpr double kEdgeX = 2.0 * 5.0 * 1.7320508075688772, kEdgeY = 2.0 * 7.5;
    return {cx <= kEdgeX ? 0.0 : cx, cx >= 100.0 - kEdgeX ? 100.0 : cx, cy <= kEdgeY ? 0.0 : cy, cy >= 100.0 - kEdgeY ? 100.0 : cy};
  }
  static double revealAlpha(double x, double y, const RevealSpot& s) {
    const double dx = x < s.x0 ? s.x0 - x : x > s.x1 ? x - s.x1 : 0.0, dy = y < s.y0 ? s.y0 - y : y > s.y1 ? y - s.y1 : 0.0;
    const double d = std::hypot(dx, dy) / 9.0;
    return d <= 0.65 ? 1.0 : d >= 1.0 ? 0.0 : 1.0 - (d - 0.65) / 0.35;
  }

  // O revelado (R8, 256 x 256 sobre a pintura inteira): drawFog's own rule (revealSpot / revealAlpha) around every hex the party
  // has stood on, compositing over each other; 1 = sem névoa. Refeito só quando a lista muda.
  void updateReveal() {
    std::vector<RevealSpot> spots;
    size_t key = 1469598103u;
    for (const ember::Json& k : save_.value("exploredHexes", ember::Json::array())) {
      if (!k.is_string()) continue;
      int c = 0, r = 0;
      if (std::sscanf(k.get<std::string>().c_str(), "%d,%d", &c, &r) != 2) continue;
      double x = 0, y = 0;
      ember::Overworld::hexToWorld(c, r, x, y);
      spots.push_back(revealSpot(x, y));
      key = (key ^ static_cast<size_t>(c * 73856093 + r * 19349663)) * 1099511628211u;
    }
    key ^= spots.size();
    if (key == revealKey_) return;
    revealKey_ = key;
    constexpr uint32_t N = 256;
    std::vector<uint8_t> texels(N * N);
    for (uint32_t j = 0; j < N; ++j)
      for (uint32_t i = 0; i < N; ++i) {
        const double x = 100.0 * (i + 0.5) / N, y = 100.0 * (j + 0.5) / N;
        double keep = 1.0;
        for (const RevealSpot& s : spots) {
          keep *= 1.0 - revealAlpha(x, y, s);
          if (keep <= 0.0) break;
        }
        texels[static_cast<size_t>(j * N + i)] = static_cast<uint8_t>(std::lround((1.0 - keep) * 255.0));
      }
    fx_->setReveal(texels, N, N);
  }

  // The fog: dark (rgba 6,6,6,.78) except what every hex the party has stood on clears (revealSpot / revealAlpha), drawn as
  // a grid of shaded cells (Ember's SVG mask, circles compositing over each other).
  // Kept for "enabled": false in content/worldmap/overworld-light-fog.json (and the measurement's baseline).
  void drawFog(ImDrawList* dl, ImVec2 o, float w, float h) {
    constexpr int N = 96;
    std::vector<RevealSpot> spots;
    for (const ember::Json& k : save_.value("exploredHexes", ember::Json::array())) {
      if (!k.is_string()) continue;
      int c = 0, r = 0;
      if (std::sscanf(k.get<std::string>().c_str(), "%d,%d", &c, &r) != 2) continue;
      double x = 0, y = 0;
      ember::Overworld::hexToWorld(c, r, x, y);
      spots.push_back(revealSpot(x, y));
    }
    std::vector<float> alpha((N + 1) * (N + 1));
    for (int j = 0; j <= N; ++j)
      for (int i = 0; i <= N; ++i) {
        const double x = 100.0 * i / N, y = 100.0 * j / N;
        double keep = 1.0;
        for (const RevealSpot& s : spots) keep *= 1.0 - revealAlpha(x, y, s);
        alpha[static_cast<size_t>(j * (N + 1) + i)] = static_cast<float>(0.78 * keep);
      }
    auto col = [&](int i, int j) { return IM_COL32(6, 6, 6, static_cast<int>(255 * alpha[static_cast<size_t>(j * (N + 1) + i)])); };
    for (int j = 0; j < N; ++j)
      for (int i = 0; i < N; ++i) {
        const ImVec2 a{o.x + w * i / N, o.y + h * j / N}, b{o.x + w * (i + 1) / N, o.y + h * (j + 1) / N};
        dl->AddRectFilledMultiColor(a, b, col(i, j), col(i + 1, j), col(i + 1, j + 1), col(i, j + 1));
      }
  }

  // Test (the user, 2026-10-29: "scale down the character and the movement grid by half", then "deixa o personagem 50% maior"):
  // 1 = the old size.
  static constexpr float kMapFigureScale = 0.75f;
  // LeaderMarker: the leader's idle frames (BattleEngine.idleFrame pacing), each cropped to its own box, 48 px tall,
  // its feet on the hex; mirrored when last walking left. Returns the button's box.
  struct Box {
    ImVec2 a, b;
    bool contains(ImVec2 p) const { return p.x >= a.x && p.x <= b.x && p.y >= a.y && p.y <= b.y; }
  };
  // `walkTime` >= 0: walking for that long (its walk clip, the left one when it goes left and has one, else the right one mirrored).
  Box drawLeader(ImDrawList* dl, ImVec2 foot, const std::string& who, float walkTime = -1.0f) {
    // EmberEngine's own heroes (Isolde) have no world-map figure yet: Kael's stands in (their frame tables are the six's only)
    const std::string hero = ownHeroClass(who) ? std::string("Kael") : who;
    struct Sheet {
      const char* hero;
      const char* dir;
      int idle;
    };
    // The heroes' walk clips (the battle's own: frames, rate, frame size and the clip's crop box x, y, w, h). A pattern under
    // content/ is the game's own art; the others are under the art folder's game/.
    struct Clip {
      const char* pattern;  // "{}" = the frame number from 1
      int count;
      float fps, natW, natH, x, y, w, h;
    };
    struct WalkSheet {
      const char* hero;
      Clip right, left;  // left.pattern null = the right one mirrored
    };
    static const WalkSheet kWalks[] = {
        {"Kael", {"sprites/Kael_Final/kael-final-002/move-{}.png", 36, 24.0f, 434, 608, 0, 0, 419, 605}, {}},
        {"Neera", {"sprites/neera/neera-v2-001/move-{}.png", 36, 24.0f, 356, 488, 3, 0, 271, 488},
         {"sprites/neera/neera-v2-001/move-left-{}.png", 36, 24.0f, 414, 486, 100, 0, 314, 486}},
        {"Voss", {"content/sprites/voss-veo/move-v01/move-{}.png", 61, 24.0f, 609, 1189, 5, 6, 602, 1178},
         {"content/sprites/voss-veo/move-left-v01/move-left-{}.png", 106, 24.0f, 653, 1210, 0, 6, 646, 1199}},
        {"Salazar", {"content/sprites/salazar-v3/move-v02/move-{}.png", 49, 12.658f, 444, 614, 6, 0, 351, 610},
         {"content/sprites/salazar-v3/move-left-v02/move-left-{}.png", 36, 18.947f, 406, 612, 51, 0, 350, 608}},
        {"Aldric", {"sprites/aldric/move-{}.png", 36, 24.0f, 396, 584, 10, 0, 386, 582}, {"sprites/aldric/move-left-{}.png", 36, 24.0f, 388, 586, 0, 0, 378, 583}},
        {"Malrec", {"sprites/malrec/move-{}.png", 36, 24.0f, 392, 682, 5, 0, 387, 682}, {}}};
    const WalkSheet* walk = nullptr;
    if (walkTime >= 0.0f)
      for (const WalkSheet& w : kWalks)
        if (hero == w.hero) walk = &w;
    bool mirror = facingLeft_;
    static const Sheet kSheets[] = {{"Kael", "Kael_Final/kael-final-002", 36}, {"Neera", "neera/neera-v2-001/idle-", 36},
                                    {"Salazar", "salazar-final", 36},       {"Aldric", "aldric", 36},                    {"Malrec", "malrec", 36}};
    float left, top, width, height, cw, ch;
    std::filesystem::path framePath;
    if (walk) {
      // Every 3rd frame (every 4th of a long clip) loaded, at the clip's own pace, a forward loop.
      const Clip& c = facingLeft_ && walk->left.pattern ? walk->left : walk->right;
      mirror = facingLeft_ && !walk->left.pattern;
      const int every = c.count > 48 ? 4 : 3, shown = (c.count + every - 1) / every;
      const int frame = 1 + (static_cast<int>(std::floor(walkTime * c.fps / static_cast<float>(every))) % shown) * every;
      left = c.x, top = c.y, width = c.w, height = c.h, cw = c.natW, ch = c.natH;
      std::string name = c.pattern;
      name.replace(name.find("{}"), 2, std::to_string(frame));
      framePath = name.starts_with("content/") ? std::filesystem::path(name) : paths_.artRoot / "game" / name;
    } else if (hero == "Voss") {
      // His own idle (voss-veo, cut from the user's video; the user, 2026-10-21: the map still showed the old black mage): every
      // 4th of its 96 frames at 6 a second (its 24 fps / 4), a forward loop with a hard cut; the clip's crop box, 654 x 1154.
      constexpr int kFrames = 96, kEvery = 4;
      const int frame = 1 + (static_cast<int>(std::floor(now_ * 24.0f / kEvery)) % (kFrames / kEvery)) * kEvery;
      left = 5, top = 5, width = 644, height = 1144, cw = 654, ch = 1154;
      framePath = std::filesystem::path("content/sprites/voss-veo/base-v01") / ("base-" + std::to_string(frame) + ".png");
    } else if (hero == "Salazar") {
      // His idle cut from the user's original sheet (salazar-v3 v02, every clip at 587 px; the user 2026-10-25: "re cut properly"):
      // every 3rd of its 36 frames, back and forth at the old pace (36 / 3 frames a second), the clip's crop box in its 427 x 598 frame.
      constexpr int kFrames = 36, kEvery = 3, kCycle = kFrames * 2 - 2;
      const int step = static_cast<int>(std::floor(now_ * kFrames / 3.0)) % kCycle;
      const int source = step < kFrames ? step : kCycle - step;
      left = 92, top = 1, width = 333, height = 595, cw = 427, ch = 598;
      framePath = std::filesystem::path("content/sprites/salazar-v3/base-v02") / ("base-" + std::to_string(1 + (source / kEvery) * kEvery) + ".png");
    } else {
      const Sheet* sheet = &kSheets[0];
      for (const Sheet& s : kSheets)
        if (hero == s.hero) sheet = &s;
      const int cycle = sheet->idle * 2 - 2;
      const double pace = (sheet->idle >= 24 ? sheet->idle / 3.0 : cycle / 2.6) * (hero == "Malrec" ? 0.9 : 1.0);
      const int stepI = static_cast<int>(std::floor(now_ * pace)) % cycle;
      const int source = stepI < sheet->idle ? stepI : cycle - stepI;
      const int count = sheet->idle == 36 ? 12 : sheet->idle;
      const int frameIndex = (sheet->idle == 36 ? source / 3 : source) % count;
      const int file = sheet->idle == 36 ? 1 + frameIndex * 3 : 1 + frameIndex;
      const ember::Json& bounds = world_.tables()["leaderFrameBounds"][hero];
      if (hero == "Neera" || hero == "Salazar") {
        float l = 1e9f, t = 1e9f, r = -1e9f, bt = -1e9f;
        for (const ember::Json& b : bounds) {
          l = std::min(l, b[0].get<float>()), t = std::min(t, b[1].get<float>());
          r = std::max(r, b[0].get<float>() + b[2].get<float>()), bt = std::max(bt, b[1].get<float>() + b[3].get<float>());
        }
        left = l, top = t, width = r - l, height = bt - t, cw = bounds[0][4].get<float>(), ch = bounds[0][5].get<float>();
      } else {
        const ember::Json& b = bounds[static_cast<size_t>(frameIndex) % bounds.size()];
        left = b[0].get<float>(), top = b[1].get<float>(), width = b[2].get<float>(), height = b[3].get<float>(), cw = b[4].get<float>(), ch = b[5].get<float>();
      }
      framePath = paths_.artRoot / (std::string("game/sprites/") + sheet->dir + (hero == "Neera" ? "" : "/") + std::to_string(file) + ".png");
    }
    const float zoomScale = kOverworldZoomStops[zoom_] / kOverworldZoomStops[0];
    const float markerHeight = 48.0f * zoomScale * (underworld_ ? 1.0f : kMapFigureScale);
    const float scale = markerHeight / height;
    const float boxW = width * scale;
    // The 64 x 48 marker grows with the map zoom, with its bottom kept on the hex.
    const float boxHalfW = std::max(32.0f * zoomScale, boxW / 2);
    const Box button{{foot.x - boxHalfW, foot.y - markerHeight}, {foot.x + boxHalfW, foot.y}};
    if (const UiImage* img = uiImage(framePath)) {
      const float x0 = foot.x - boxW / 2, y0 = foot.y - markerHeight;
      // The crop box of the frame, in texture coordinates.
      float u0 = left / cw, v0 = top / ch, u1 = (left + width) / cw, v1 = (top + height) / ch;
      if (mirror) std::swap(u0, u1);
      // drop-shadow(0 2px 3px rgba(0,0,0,.6))
      dl->AddImage(img->texture, {x0, y0 + 2}, {x0 + boxW, y0 + markerHeight + 2}, {u0, v0}, {u1, v1}, IM_COL32(0, 0, 0, 120));
      dl->AddImage(img->texture, {x0, y0}, {x0 + boxW, y0 + markerHeight}, {u0, v0}, {u1, v1});
    }
    return button;
  }

  // --- the party panel, bottom left -------------------------------------------------------------------------------------

  void drawPartyPanel(ImVec2 size) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImFont* ui = uiFont(UiFont::Ui);
    const std::vector<std::string> heroes = partyHeroes();
    const ember::Json& pos = save_["overworldPos"];
    const ember::MapLocation* standing = world_.locationAt(pos.value("col", 0), pos.value("row", 0));
    std::string line;
    const auto wisp = world_.wispHex();
    if (wisp && !test() && world_.wispForestUncleared(save_) && pos.value("col", 0) == wisp->col && pos.value("row", 0) == wisp->row)
      line = "Limpe a Wisp Forest para poder seguir viagem para o leste.";
    else
      line = movementOpen_ ? "Escolha um hexágono · o tempo depende do terreno" : "Clique em " + leader() + " para mover";
    const float pad = 12.0f;
    const float rowW = static_cast<float>(heroes.size()) * 40.0f + static_cast<float>(std::max<size_t>(1, heroes.size()) - 1) * 12.0f;
    const float textW = ui->CalcTextSizeA(12.0f, FLT_MAX, 0, line.c_str()).x;
    const float w = std::min(size.x - 96.0f, std::max(rowW, textW) + 2 * pad);
    const float h = pad + 12.0f * 1.4f + 8.0f + 48.0f + 8.0f + 8.0f + (standing ? 44.0f : 0.0f) + pad;
    const ImVec2 a{16.0f, size.y - 16.0f - h}, b{a.x + w, size.y - 16.0f};
    if (const UiImage* bg = uiImage(paths_.artRoot / "game/ui/map-status-background.jpg")) imageCover(dl, *bg, a, b);
    dl->AddRectFilled(a, b, IM_COL32(17, 17, 17, 64), 0);
    dl->AddRect(a, b, kBorder, 0);
    // Block input under the panel; the portraits' buttons over it still take their clicks (without the overlap the panel's button
    // kept the hover and no portrait ever opened its status sheet).
    ImGui::SetCursorScreenPos(a);
    ImGui::SetNextItemAllowOverlap();
    ImGui::InvisibleButton("##partypanel", {w, h});
    dl->AddText(ui, 12.0f, {a.x + pad, a.y + pad}, kMuted, line.c_str());
    float x = a.x + pad;
    const float y = a.y + pad + 12.0f * 1.4f + 8.0f;
    for (const std::string& hero : heroes) {
      portrait(dl, hero, {x, y}, {x + 40, y + 48});
      ImGui::SetCursorScreenPos({x, y});
      ImGui::PushID(hero.c_str());
      if (ImGui::InvisibleButton("##portrait", {40, 48})) openSheet(hero);  // its status sheet (Mochila / Equipar inside it)
      if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", hero.c_str());
      ImGui::PopID();
      // HungerBar: 4 px, the first 100 of 120 in the accent (red at 25 or less), the overfed part gold.
      const double full = ember::Overworld::fullness(save_.value("heroHunger", ember::Json::object()).value(hero, ember::Json()));
      const float by = y + 48 + 4;
      dl->AddRectFilled({x, by}, {x + 40, by + 4}, IM_COL32(74, 74, 74, 255), 2.0f);
      dl->AddRectFilled({x, by}, {x + 40 * static_cast<float>(std::min(100.0, full) / 120.0), by + 4}, full <= 25 ? kDanger : kAccent, 2.0f);
      if (full > 100) dl->AddRectFilled({x + 40 * 100.0f / 120.0f, by}, {x + 40 * static_cast<float>(full / 120.0), by + 4}, IM_COL32(201, 162, 39, 255));
      // LifeBar: red, green when poisoned, purple when ill.
      const int maxHp = std::max(1, heroMaxHp(hero));
      const double hp = std::clamp(num(save_.value("unitHp", ember::Json::object()), hero, maxHp), 0.0, static_cast<double>(maxHp));
      const bool poisoned = save_.contains("heroPoisons") && save_["heroPoisons"].contains(hero) && !save_["heroPoisons"][hero].is_null() && save_["heroPoisons"][hero] != false;
      const bool diseased = save_.contains("heroDiseases") && save_["heroDiseases"].value(hero, false);
      const float ly = by + 8;
      dl->AddRectFilled({x, ly}, {x + 40, ly + 4}, IM_COL32(74, 74, 74, 255), 2.0f);
      const float lw = 40 * static_cast<float>(hp / maxHp);
      if (poisoned && diseased) dl->AddRectFilledMultiColor({x, ly}, {x + lw, ly + 4}, IM_COL32(63, 155, 58, 255), IM_COL32(138, 70, 194, 255), IM_COL32(138, 70, 194, 255), IM_COL32(63, 155, 58, 255));
      else dl->AddRectFilled({x, ly}, {x + lw, ly + 4}, poisoned ? IM_COL32(63, 155, 58, 255) : diseased ? IM_COL32(138, 70, 194, 255) : IM_COL32(184, 50, 42, 255), 2.0f);
      x += 52.0f;
    }
    if (standing) {
      const std::string label = "Explorar " + standing->name;
      ImGui::SetCursorScreenPos({a.x + pad, b.y - pad - 44.0f});
      if (ImGui::InvisibleButton("##explore", {ui->CalcTextSizeA(12.0f, FLT_MAX, 0, label.c_str()).x, 44.0f})) enterLocation(*standing, locationStatus(*standing));
      dl->AddText(ui, 12.0f, {a.x + pad, b.y - pad - 28.0f}, kAccent, label.c_str());
    }
  }

  void drawZoom(ImVec2 size) {
    const bool inert = event_ || open_ || questLog_ || party_ || options_ || !save_.value("seenOverworldIntro", false);
    const bool changed = worldMapZoomBox(size, underworld_ ? underworldZoom_ : zoom_, inert, 4, 1, 4);
    if (changed) recentre_ = true;
  }

  // --- dialogs ----------------------------------------------------------------------------------------------------------

  // A modal veil (ember-veil) under a panel; true when the veil itself was clicked.
  bool veil(ImVec2 size, const char* id) {
    ImGui::SetCursorScreenPos({0, 0});
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const bool clicked = ImGui::InvisibleButton(id, size);
    verticalGradient(dl, {0, 0}, size, {{0.0f, IM_COL32(0, 0, 0, 89)}, {1.0f, IM_COL32(0, 0, 0, 184)}});
    return clicked;
  }

  // A panel window centred on the screen, drawn above the map (its own ImGui window, so its buttons win the clicks).
  bool beginPanel(const char* id, ImVec2 size, float width, float maxHeight) {
    ImGui::SetNextWindowPos({size.x / 2, size.y / 2}, ImGuiCond_Always, {0.5f, 0.5f});
    ImGui::SetNextWindowSize({std::min(width, size.x - 32.0f), 0});
    ImGui::SetNextWindowSizeConstraints({0, 0}, {std::min(width, size.x - 32.0f), maxHeight});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {20, 20});
    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(0, 0, 0, 0));
    const bool open = ImGui::Begin(id, nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize);
    const ImVec2 a = ImGui::GetWindowPos(), b{a.x + ImGui::GetWindowSize().x, a.y + ImGui::GetWindowSize().y};
    ImGui::GetWindowDrawList()->ChannelsSplit(2);
    ImGui::GetWindowDrawList()->ChannelsSetCurrent(0);
    emberPanel(ImGui::GetWindowDrawList(), a, b);
    ImGui::GetWindowDrawList()->ChannelsSetCurrent(1);
    return open;
  }
  void endPanel() {
    ImGui::GetWindowDrawList()->ChannelsMerge();
    ImGui::End();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar();
  }
  // Text helpers inside panels.
  static void text(const std::string& s, float size, ImU32 color, UiFont font = UiFont::Ui, float wrap = 0) {
    ImGui::PushFont(uiFont(font), size);
    ImGui::PushStyleColor(ImGuiCol_Text, color);
    if (wrap > 0) ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + wrap);
    ImGui::TextUnformatted(s.c_str());
    if (wrap > 0) ImGui::PopTextWrapPos();
    ImGui::PopStyleColor();
    ImGui::PopFont();
  }
  static void kicker(const std::string& s, float size) {
    const ImVec2 at = ImGui::GetCursorScreenPos();
    emberKicker(ImGui::GetWindowDrawList(), at, size, upperUtf8(s).c_str());
    ImGui::Dummy({1, size * 1.3f});
  }
  static void title(const std::string& s, float size) {
    const ImVec2 at = ImGui::GetCursorScreenPos();
    emberTitle(ImGui::GetWindowDrawList(), at, size, s.c_str());
    ImGui::Dummy({1, size * 1.1f});
  }
  static bool closeButton(const char* id) {
    return emberPlate(id, {36, 36}, [](ImDrawList* d, ImVec2 a, ImVec2 b, ImU32) { iconX(d, {(a.x + b.x) / 2, (a.y + b.y) / 2}, 16, IM_COL32(223, 191, 142, 255)); });
  }
  static void slot(ImVec2 a, ImVec2 b) { emberSlot(ImGui::GetWindowDrawList(), a, b, false); }

  void drawEvent(ImVec2 size) {
    veil(size, "##eventveil");
    beginPanel("##event", size, 384, size.y * 0.8f);
    text(event_->text, 14.0f, kFg, UiFont::Ui, 340);
    ImGui::Dummy({1, 16});
    const ImVec2 at = ImGui::GetCursorScreenPos();
    if (emberBigButton("##eventok", "OK", at, {344, 44}, true, 14.0f)) event_.reset();
    endPanel();
  }

  void drawLocationPanel(ImVec2 size) {
    const ember::MapLocation* loc = nullptr;
    for (const ember::MapLocation& l : world_.locations())
      if (l.id == *open_) loc = &l;
    if (!loc) return open_.reset();
    if (veil(size, "##locveil")) return open_.reset();
    beginPanel("##location", size, 448, size.y * 0.8f);
    const float inner = 408;
    const ImVec2 head = ImGui::GetCursorScreenPos();
    title(loc->name, 20.0f);
    ImGui::SetCursorScreenPos({head.x + inner - 32, head.y});
    if (emberPlate("##locclose", {32, 32}, [](ImDrawList* d, ImVec2 a, ImVec2 b, ImU32) { iconX(d, {(a.x + b.x) / 2, (a.y + b.y) / 2}, 16, IM_COL32(223, 191, 142, 255)); }))
      open_.reset();
    ImGui::SetCursorScreenPos({head.x, head.y + 40});
    std::vector<std::string> order;
    for (const ember::MapMission* m : world_.missionsOf(*loc)) order.push_back(m->id);
    int i = 0;
    std::string pick;
    for (const ember::MapMission* m : world_.visibleMissionsOf(*loc, save_, test())) {
      ++i;
      const std::string st = world_.missionStatus(m->id, save_, order, test());
      const bool optionalCrossing = st == "done" && m->crossing;
      const ember::CampaignMission* cm = campaign_.data().mission(m->id);
      const std::string place = cm ? cm->place : std::string();
      const std::string objective = cm ? cm->objective : std::string();
      const ImVec2 a = ImGui::GetCursorScreenPos();
      ImFont* ui = uiFont(UiFont::Ui);
      const float objH = ui->CalcTextSizeA(16.0f, FLT_MAX, inner - 32, objective.c_str()).y;
      const float h = 12 + 18 + 30 + objH + (optionalCrossing ? 24 + 52 : 0) + 12;
      slot(a, {a.x + inner, a.y + h});
      ImGui::PushID(m->id.c_str());
      ImGui::SetCursorScreenPos({a.x + 12, a.y + 8});
      const bool clicked = ImGui::InvisibleButton("##mission", {inner - 24, 18 + 30 + objH + 8});
      const bool hover = ImGui::IsItemHovered();
      const int fade = st == "locked" ? 102 : 255;
      ImDrawList* dl = ImGui::GetWindowDrawList();
      if (flashId_ == m->id && now_ - flashAt_ < 0.5f) dl->AddRect(a, {a.x + inner, a.y + h}, IM_COL32(214, 60, 60, 200), 0.0f, 2.0f, ImDrawFlags_None);
      const std::string kick = std::format("{:02} · {}{}", i, place, st == "done" ? " · feito" : "");
      float kx = a.x + 16;
      if (st == "locked") iconLock(dl, {kx + 6, a.y + 20}, 12, IM_COL32(223, 191, 142, fade)), kx += 18;
      if (st == "done") iconCheck(dl, {kx + 6, a.y + 20}, 12, kAccent), kx += 18;
      emberKicker(dl, {kx, a.y + 12}, 14.0f, upperUtf8(kick).c_str());
      emberTitle(dl, {a.x + 16, a.y + 32}, 24.0f, m->title.c_str());
      dl->AddText(ui, 16.0f, {a.x + 16, a.y + 62}, IM_COL32(180, 180, 180, fade), objective.c_str(), nullptr, inner - 32);
      if (hover && st != "locked" && !optionalCrossing) dl->AddRect(a, {a.x + inner, a.y + h}, IM_COL32(226, 170, 100, 120), 0.0f, 1.0f, ImDrawFlags_None);
      if (clicked && !optionalCrossing) {
        if (st == "locked") flashId_ = m->id, flashAt_ = now_;
        else pick = m->id;
      }
      if (optionalCrossing) {
        dl->AddText(ui, 14.0f, {a.x + 16, a.y + 66 + objH}, kAccent, "Travessia concluída · escolha como seguir");
        const float bw = (inner - 40) / 2;
        if (emberBigButton("##pass", "PASSAR SEM ENTRAR", {a.x + 16, a.y + 90 + objH}, {bw, 40}, false, 12.0f)) open_.reset();
        if (emberBigButton("##again", "EXPLORAR DE NOVO", {a.x + 24 + bw, a.y + 90 + objH}, {bw, 40}, true, 12.0f)) pick = m->id;
      }
      ImGui::PopID();
      ImGui::SetCursorScreenPos({a.x, a.y + h + 8});
    }
    ImGui::Dummy({inner, 1});
    endPanel();
    if (!pick.empty()) {
      open_.reset();
      openMission(pick);
    }
  }

  // Ember's QuestLogPanel (QuestLog.h, shared with the fight's quest log).
  void drawQuestLog(ImVec2) {
    if (!drawQuestLogPanel(world_.tables()["quests"], world_.tables()["potions"], save_)) questLog_ = false;
  }

  // The Party menu: Grupo (leader, starting formation, affinities) and Skills (road training).
  void drawParty(ImVec2 size) {
    if (veil(size, "##partyveil")) {
      party_ = false;
      return;
    }
    const std::vector<std::string> heroes = partyHeroes();
    beginPanel("##partymenu", size, 768, size.y * 0.82f);
    const float inner = 728;
    const ImVec2 head = ImGui::GetCursorScreenPos();
    kicker(std::format("Grupo · {} {} · Líder: {}", heroes.size(), heroes.size() == 1 ? "membro" : "membros", leader()), 12.0f);
    title("Party", 24.0f);
    ImGui::SetCursorScreenPos({head.x + inner - 36, head.y});
    if (closeButton("##partyclose")) party_ = false;
    ImGui::SetCursorScreenPos({head.x, head.y + 64});
    {
      const ImVec2 at = ImGui::GetCursorScreenPos();
      if (emberBigButton("##tabgroup", "GRUPO", at, {120, 32}, partyTab_ == 0, 12.0f)) partyTab_ = 0;
      if (emberBigButton("##tabskills", "SKILLS", {at.x + 128, at.y}, {120, 32}, partyTab_ == 1, 12.0f)) partyTab_ = 1;
      if (hasCompanions_ && emberBigButton("##tabtalk", upperUtf8(tr("Conversas", "Conversations")).c_str(), {at.x + 256, at.y}, {150, 32}, partyTab_ == 2, 12.0f)) partyTab_ = 2;
      ImGui::SetCursorScreenPos({at.x, at.y + 44});
    }
    ImGui::BeginChild("##partybody", {inner, std::min(size.y * 0.82f - 200, 640.0f)}, 0, ImGuiWindowFlags_NoBackground);
    if (partyTab_ == 1) drawSkillsTab(heroes, inner - 16);
    else if (partyTab_ == 2) drawConversationsTab(heroes, inner - 16);
    else drawGroupTab(heroes, inner - 16);
    if (partyScrollEnd_) ImGui::SetScrollY(ImGui::GetScrollMaxY());  // capture "party:end": the bottom of the tab (Afinidades)
    ImGui::EndChild();
    endPanel();
  }

  // Caminho de classe: from level 15 every hero chooses once between the two prestige classes of its own (the user, 2026-10-19: "vamos
  // colocar para todos, a partir do level 15 podem escolher entre uma das duas classes de prestígio"); the choice goes into
  // save.promotions, which the fights read (world/Spells.cpp setPromotion) and the inn / inventory / world map read for the class.
  void drawPromotions(const std::vector<std::string>& heroes, float inner) {
    struct Path {
      const char *base, *first, *second;
    };
    static const Path kPaths[] = {{"mage", "elementalist", "warlock"},  {"voss", "elementalist", "warlock"}, {"swordsman", "paladin", "heavyKnight"},
                                  {"archer", "ranger", "assassin"},     {"aldric", "sentinel", "templar"},   {"healer", "cleric", "bishop"},
                                  {"conjurer", "sorcerer", "necromancer"}};
    struct Row {
      std::string hero;
      const Path* path;
    };
    std::vector<Row> rows;
    for (const std::string& hero : heroes) {
      std::string base = world_.tables()["baseClass"].value(hero, "");
      if (base.empty())
        if (const char* own = ownHeroClass(hero)) base = own;
      for (const Path& p : kPaths)
        if (base == p.base) {
          rows.push_back({hero, &p});
          break;
        }
    }
    if (rows.empty()) return;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImFont* ui = uiFont(UiFont::Ui);
    kicker(tr("Caminho de classe", "Class path"), 14.0f);
    const ImVec2 a = ImGui::GetCursorScreenPos();
    const float rowH = 64.0f;
    slot(a, {a.x + inner, a.y + 16 + rowH * static_cast<float>(rows.size())});
    float y = a.y + 10;
    for (const Row& row : rows) {
      const std::string& hero = row.hero;
      const int level = static_cast<int>(num(save_.value("levels", ember::Json::object()), hero, 1));
      std::string promo;
      if (save_.contains("promotions") && save_["promotions"].contains(hero) && save_["promotions"][hero].is_string()) promo = save_["promotions"][hero].get<std::string>();
      portrait(dl, hero, {a.x + 12, y}, {a.x + 52, y + 48});
      const std::string cls = promo.empty() ? ember::promotionName(row.path->base) : ember::promotionName(promo);
      const std::string head = std::format("{} · {} {} · {}", hero, tr("Nv", "Lv"), level, cls);
      dl->AddText(ui, 14.0f, {a.x + 62, y + 2}, kFg, head.c_str());
      const std::string firstName = ember::promotionName(row.path->first), secondName = ember::promotionName(row.path->second);
      if (!promo.empty()) {
        dl->AddText(ui, 12.0f, {a.x + 62, y + 24}, kMuted, tr("Classe de prestígio escolhida: fica para o resto da campanha.", "Prestige class chosen: it stays for the rest of the campaign.").c_str());
      } else if (level < 15) {
        const std::string hint = std::format("{} {} / {}.", tr("No nível 15 escolhe o caminho:", "At level 15 you choose the path:"), firstName, secondName);
        dl->AddText(ui, 12.0f, {a.x + 62, y + 24}, kMuted, hint.c_str());
      } else {
        dl->AddText(ui, 12.0f, {a.x + 62, y + 24}, kMuted, tr("Escolha a classe de prestígio (fica para o resto da campanha):", "Choose the prestige class (it stays for the rest of the campaign):").c_str());
        ImGui::PushID(hero.c_str());
        if (emberBigButton("##first", upperUtf8(firstName).c_str(), {a.x + inner - 300, y + 8}, {140, 30}, false, 11.0f)) {
          save_["promotions"][hero] = row.path->first;
          write();
        }
        if (emberBigButton("##second", upperUtf8(secondName).c_str(), {a.x + inner - 150, y + 8}, {140, 30}, false, 11.0f)) {
          save_["promotions"][hero] = row.path->second;
          write();
        }
        ImGui::PopID();
      }
      y += rowH;
    }
    ImGui::SetCursorScreenPos({a.x, a.y + 24 + rowH * static_cast<float>(rows.size())});
  }

  // Conversas de companheiros (Ember CompanionConversations.tsx): the leader, a companion, and that pair's topics (one per
  // affinity tier); a topic opens the conversation, whose first reply changes the affinity for good.
  void drawConversationsTab(const std::vector<std::string>& heroes, float inner) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImFont* ui = uiFont(UiFont::Ui);
    const bool en = uiEnglish();
    const std::string lead = leader();
    kicker(tr("Conversas de companheiros", "Companion conversations"), 14.0f);
    text(en ? std::format("Speak as {}. Each companion has a different relationship with every leader.", lead)
            : std::format("Fale como {}. Cada companheiro tem uma relação diferente com cada líder.", lead),
         14.0f, kMuted, UiFont::Ui, inner - 8);
    ImGui::Dummy({1, 8});
    kicker(tr("Líder do grupo", "Party leader"), 12.0f);
    {
      const ImVec2 a = ImGui::GetCursorScreenPos();
      float x = a.x;
      ImGui::PushID("convleader");
      for (const std::string& hero : heroes) {
        if (emberBigButton(hero.c_str(), upperUtf8(hero).c_str(), {x, a.y}, {108, 32}, hero == lead, 12.0f)) {
          save_["partyLeader"] = hero;
          write();
          convCompanion_.clear();
        }
        x += 114;
      }
      ImGui::PopID();
      ImGui::SetCursorScreenPos({a.x, a.y + 44});
    }
    std::vector<std::string> others;
    for (const std::string& hero : heroes)
      if (hero != lead) others.push_back(hero);
    if (others.empty()) {
      text(tr("As conversas ficam disponíveis quando outro companheiro entra no grupo.", "Conversations become available when another companion joins the party."), 14.0f, kMuted,
           UiFont::Ui, inner - 8);
      return;
    }
    if (std::find(others.begin(), others.end(), convCompanion_) == others.end()) convCompanion_ = others.front();
    kicker(tr("Escolha um companheiro", "Choose a companion"), 12.0f);
    {
      const ImVec2 a = ImGui::GetCursorScreenPos();
      float x = a.x;
      ImGui::PushID("convcompanion");
      for (const std::string& hero : others) {
        if (emberBigButton(hero.c_str(), upperUtf8(hero).c_str(), {x, a.y}, {108, 32}, hero == convCompanion_, 12.0f)) convCompanion_ = hero;
        x += 114;
      }
      ImGui::PopID();
      ImGui::SetCursorScreenPos({a.x, a.y + 48});
    }
    {  // the pair
      const ImVec2 a = ImGui::GetCursorScreenPos();
      portrait(dl, convCompanion_, {a.x, a.y}, {a.x + 56, a.y + 64});
      dl->AddRect({a.x, a.y}, {a.x + 56, a.y + 64}, kBorder, 4.0f);
      dl->AddText(uiFont(UiFont::Display), 20.0f, {a.x + 70, a.y + 4}, kFg, (lead + " & " + convCompanion_).c_str());
      const double points = ember::Companions::affinityScore(save_, lead, convCompanion_);
      std::string shown = std::format("{:.1f}", points);
      if (shown.size() > 2 && shown.substr(shown.size() - 2) == ".0") shown.resize(shown.size() - 2);
      if (!en) std::replace(shown.begin(), shown.end(), '.', ',');
      dl->AddText(ui, 14.0f, {a.x + 70, a.y + 34}, kMuted, std::format("{}: {}/100", en ? "Affinity" : "Afinidade", shown).c_str());
      ImGui::SetCursorScreenPos({a.x, a.y + 80});
    }
    const std::vector<ember::Companions::Topic> topics = companions_.topics(lead, convCompanion_, save_, en);
    for (const ember::Companions::Topic& t : topics) {
      const ImVec2 a = ImGui::GetCursorScreenPos();
      const float h = 66.0f;
      ImGui::PushID(t.tier);
      ImGui::BeginDisabled(!t.unlocked);
      const bool clicked = ImGui::InvisibleButton("##topic", {inner, h});
      ImGui::EndDisabled();
      ImGui::PopID();
      emberSlot(dl, a, {a.x + inner, a.y + h}, t.unlocked && ImGui::IsItemHovered());
      const ImU32 fg = t.unlocked ? kFg : IM_COL32(240, 240, 240, 110), muted = t.unlocked ? kMuted : IM_COL32(180, 180, 180, 110);
      dl->AddText(ui, 12.0f, {a.x + 14, a.y + 8}, muted,
                  std::format("{} · {} {}", companions_.tierName(t.tier, en), t.threshold, en ? "affinity" : "afinidade").c_str());
      dl->AddText(ui, 15.0f, {a.x + 14, a.y + 25}, fg, t.label.c_str());
      const std::string state = !t.unlocked ? tr("Continue construindo essa relação para desbloquear", "Keep building this relationship to unlock")
                                : t.resolved ? tr("Rever conversa", "Revisit conversation")
                                             : tr("Nova conversa", "New conversation");
      dl->AddText(ui, 12.0f, {a.x + 14, a.y + 46}, muted, state.c_str());
      if (clicked) {
        const ember::Json tree = companions_.build(lead, convCompanion_, t.tier, save_, en);
        if (tree.is_object()) {
          convLeader_ = lead;
          convDialog_.emplace(tree, companions_.portraits(), paths_.artRoot);
        }
      }
      ImGui::SetCursorScreenPos({a.x, a.y + h + 8});
    }
    text(tr("Você pode rever conversas anteriores. Só a primeira resposta muda a afinidade. Deixar um assunto para depois não o resolve.",
            "You can revisit earlier conversations. Only the first response changes affinity. Leaving a topic for later does not resolve it."),
         12.0f, kMuted, UiFont::Ui, inner - 8);
    ImGui::Dummy({1, 8});
  }

  // The field-spells menu (OverworldMapScreen's dropdown): Criar Comida e Água for each healer, Warp for each mage of level 7+.
  void drawFieldSpells(ImVec2 size) {
    if (veil(size, "##fieldveil")) {
      fieldSpells_ = false;
      return;
    }
    const bool en = uiEnglish();
    const ember::FieldSpells spells(innData_, world_);
    const std::vector<std::string> healers = spells.healers(save_, test()), mages = spells.mages(save_, test()), hunters = spells.hunters(save_, test());
    const std::vector<std::string> campers = spells.campers(save_, test()), prayers = spells.prayers(save_, test());
    const std::vector<std::string> markers = spells.runeMarkers(save_, test()), recallers = spells.recallers(save_, test());
    const std::vector<const ember::MapLocation*> cities = spells.warpCities(save_);
    beginPanel("##fieldspells", size, 360, size.y * 0.8f);
    const ImVec2 head = ImGui::GetCursorScreenPos();
    kicker(tr("Feitiços de campo", "Field spells"), 14.0f);
    ImGui::SetCursorScreenPos({head.x + 320 - 36, head.y - 6});
    if (closeButton("##fieldclose")) fieldSpells_ = false;
    ImGui::SetCursorScreenPos({head.x, head.y + 34});
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImFont* ui = uiFont(UiFont::Ui);
    const float inner = 320;
    for (const std::string& hero : healers) {
      const int level = spells.levelOf(save_, hero), left = spells.usesLeft(save_, hero, ember::FieldSpells::kFoodTier);
      const ImVec2 a = ImGui::GetCursorScreenPos();
      float textX = a.x;
      if (const UiImage* icon = uiImage(paths_.artRoot / "game/icons/create-food-and-water.png")) {  // Ember's icon (data.ts spellIcon)
        imageCover(dl, *icon, {a.x, a.y - 2}, {a.x + 40, a.y + 38}, 0.5f, 0.5f, IM_COL32(255, 255, 255, 255));
        textX += 48;
      }
      dl->AddText(ui, 15.0f, {textX, a.y}, kFg, tr("Criar Comida e Água", "Create Food and Water").c_str());
      dl->AddText(ui, 12.0f, {textX, a.y + 20}, kMuted,
                  std::format("{} ({} {}) · {} · {}x", hero, en ? "Lv" : "Nv", level, ember::FieldSpells::foodFormula(level, en), left).c_str(), nullptr,
                  inner - 108 - (textX - a.x));
      ImGui::PushID(hero.c_str());
      ImGui::BeginDisabled(!spells.canFeed(save_, hero));
      if (emberBigButton("##cast", upperUtf8(tr("Lançar", "Cast")).c_str(), {a.x + inner - 96, a.y + 6}, {96, 30}, false, 11.0f)) {
        static std::mt19937 rng{std::random_device{}()};
        std::uniform_real_distribution<double> unit(0.0, 1.0);
        if (const std::optional<ember::Json> next = spells.castFood(save_, hero, [&] { return unit(rng); })) {
          save_ = *next;
          write();
          showHint(en ? std::format("{} restored their hunger.", hero) : std::format("{} restaurou a fome.", hero));
        } else {
          showHint(tr("Não foi possível lançar.", "Could not cast."));
        }
      }
      ImGui::EndDisabled();
      ImGui::PopID();
      ImGui::SetCursorScreenPos({a.x, a.y + 62});
      ImGui::Dummy({inner, 4});
    }
    // Caçada (ours, docs/SKILLS_ARCHER.md): the archer hunts: rations for the party, a tier-2 use
    for (const std::string& hero : hunters) {
      const int level = spells.levelOf(save_, hero), left = spells.usesLeft(save_, hero, ember::FieldSpells::kHuntTier);
      const ImVec2 a = ImGui::GetCursorScreenPos();
      float textX = a.x;
      if (const UiImage* icon = uiImage(std::filesystem::path("content/ui/spells/hunt.jpg"))) {  // the user's art, centre-cropped
        imageCover(dl, *icon, {a.x, a.y - 2}, {a.x + 40, a.y + 38}, 0.5f, 0.5f, IM_COL32(255, 255, 255, 255));
        textX += 48;
      }
      dl->AddText(ui, 15.0f, {textX, a.y}, kFg, tr("Caçada", "Hunt").c_str());
      dl->AddText(ui, 12.0f, {textX, a.y + 20}, kMuted,
                  std::format("{} ({} {}) · {} · {}x", hero, en ? "Lv" : "Nv", level, ember::FieldSpells::huntFormula(level, en), left).c_str(), nullptr, inner - 108 - (textX - a.x));
      ImGui::PushID(("hunt" + hero).c_str());
      ImGui::BeginDisabled(!spells.canHunt(save_, hero));
      if (emberBigButton("##hunt", upperUtf8(tr("Caçar", "Hunt")).c_str(), {a.x + inner - 96, a.y + 6}, {96, 30}, false, 11.0f)) {
        static std::mt19937 rng{std::random_device{}()};
        std::uniform_real_distribution<double> unit(0.0, 1.0);
        int rations = 0;
        if (const std::optional<ember::Json> next = spells.castHunt(save_, hero, [&] { return unit(rng); }, &rations)) {
          save_ = *next;
          write();
          showHint(en ? std::format("{} hunted {} {}.", hero, rations, rations == 1 ? "ration" : "rations")
                      : std::format("{} caçou {} {}.", hero, rations, rations == 1 ? "ração" : "rações"));
        } else {
          showHint(tr("Não foi possível caçar.", "Could not hunt."));
        }
      }
      ImGui::EndDisabled();
      ImGui::PopID();
      ImGui::SetCursorScreenPos({a.x, a.y + 62});
      ImGui::Dummy({inner, 4});
    }
    // The Curandeiro's (ours, docs/SKILLS_HEALER.md): Consagrar Acampamento (the party to full HP, a tier-2 use) and Prece do Caminho
    // (the next hexes free of undead ambushes, battles half as likely, a tier-3 use); the user's icons head their rows.
    auto healerRow = [&](const std::string& hero, const char* icon, const std::string& title, const std::string& line, const std::string& button, const char* id,
                         bool enabled, const std::function<void()>& cast) {
      const ImVec2 a = ImGui::GetCursorScreenPos();
      float textX = a.x;
      if (const UiImage* art = uiImage(std::filesystem::path("content/ui/spells") / icon)) {  // the user's art, centre-cropped
        imageCover(dl, *art, {a.x, a.y - 2}, {a.x + 40, a.y + 38}, 0.5f, 0.5f, IM_COL32(255, 255, 255, 255));
        textX += 48;
      }
      dl->AddText(ui, 15.0f, {textX, a.y}, kFg, title.c_str());
      dl->AddText(ui, 12.0f, {textX, a.y + 20}, kMuted, line.c_str(), nullptr, inner - 108 - (textX - a.x));
      ImGui::PushID((id + hero).c_str());
      ImGui::BeginDisabled(!enabled);
      if (emberBigButton("##castrow", upperUtf8(button).c_str(), {a.x + inner - 96, a.y + 6}, {96, 30}, false, 11.0f)) cast();
      ImGui::EndDisabled();
      ImGui::PopID();
      ImGui::SetCursorScreenPos({a.x, a.y + 62});
      ImGui::Dummy({inner, 4});
    };
    for (const std::string& hero : campers) {
      const int level = spells.levelOf(save_, hero), left = spells.usesLeft(save_, hero, ember::FieldSpells::kCampTier);
      healerRow(hero, "consecrateCamp.jpg", tr("Consagrar Acampamento", "Consecrate Camp"),
                std::format("{} ({} {}) · {} · {}x", hero, en ? "Lv" : "Nv", level, tr("todo o grupo com HP cheio", "the whole party to full HP"), left),
                tr("Acampar", "Camp"), "camp", spells.canCamp(save_, hero, test()), [&, hero] {
                  int healed = 0;
                  if (const std::optional<ember::Json> next = spells.castCamp(save_, hero, test(), &healed)) {
                    save_ = *next;
                    write();
                    showHint(en ? std::format("{} consecrated the camp: {} healed to full.", hero, healed)
                                : std::format("{} consagrou o acampamento: {} com HP cheio.", hero, healed));
                  } else {
                    showHint(tr("Não foi possível acampar.", "Could not camp."));
                  }
                });
    }
    for (const std::string& hero : prayers) {
      const int level = spells.levelOf(save_, hero), left = spells.usesLeft(save_, hero, ember::FieldSpells::kPrayerTier);
      const int hexes = ember::FieldSpells::prayerHexes(level), active = ember::FieldSpells::prayerLeft(save_);
      const std::string state = active > 0 ? (en ? std::format(" · active: {} hexes", active) : std::format(" · ativa: {} hexes", active)) : std::string();
      healerRow(hero, "prayerOfTheRoad.jpg", tr("Prece do Caminho", "Prayer of the Road"),
                std::format("{} ({} {}) · {} {} · {}x{}", hero, en ? "Lv" : "Nv", level, hexes, tr("hexes sem mortos-vivos, batalhas pela metade", "hexes: no undead, battles halved"),
                            left, state),
                tr("Rezar", "Pray"), "pray", spells.canPray(save_, hero), [&, hero, hexes] {
                  if (const std::optional<ember::Json> next = spells.castPrayer(save_, hero)) {
                    save_ = *next;
                    write();
                    showHint(en ? std::format("{} prayed: the next {} hexes hold no undead.", hero, ember::FieldSpells::prayerLeft(save_))
                                : std::format("{} rezou: os próximos {} hexes sem mortos-vivos.", hero, ember::FieldSpells::prayerLeft(save_)));
                  } else {
                    showHint(tr("Não foi possível rezar.", "Could not pray."));
                  }
                });
    }
    // The Conjurador's runes (ours, docs/SKILLS_CONJURER.md; Ultima Online's Mark and Recall): Marcar Runa makes a rune of this hex (an
    // item of the party's bag, a tier-2 use); Retornar opens the runes and takes the party to the chosen one (a tier-3 use; the rune stays).
    for (const std::string& hero : markers) {
      const int level = spells.levelOf(save_, hero), left = spells.usesLeft(save_, hero, ember::FieldSpells::kRuneTier);
      const ember::FieldSpells::Rune here = spells.runeHere(save_);
      const bool marked = ember::FieldSpells::runeAt(save_, here.col, here.row);
      healerRow(hero, "markRune.jpg", tr("Marcar Runa", "Mark Rune"),
                std::format("{} ({} {}) · {} · {}x", hero, en ? "Lv" : "Nv", level, marked ? tr("este hex já tem runa", "this hex has a rune") : here.place, left),
                tr("Marcar", "Mark"), "mark", spells.canMark(save_, hero, test()), [&, hero] {
                  if (const std::optional<ember::Json> next = spells.castMarkRune(save_, hero, test())) {
                    save_ = *next;
                    write();
                    showHint(en ? std::format("{} marked a rune here ({} runes in the bag).", hero, ember::FieldSpells::runes(save_).size())
                                : std::format("{} marcou uma runa aqui ({} runas na mochila).", hero, ember::FieldSpells::runes(save_).size()));
                  } else {
                    showHint(tr("Não foi possível marcar (sem uso, sem espaço na mochila ou já há runa aqui).", "Could not mark (no use, no bag room or a rune is here)."));
                  }
                });
    }
    for (const std::string& hero : recallers) {
      const int level = spells.levelOf(save_, hero), left = spells.usesLeft(save_, hero, ember::FieldSpells::kRecallTier);
      const size_t count = ember::FieldSpells::runes(save_).size();
      healerRow(hero, "recall.jpg", tr("Retornar", "Recall"),
                std::format("{} ({} {}) · {} {} · {}x", hero, en ? "Lv" : "Nv", level, count, count == 1 ? tr("runa", "rune") : tr("runas", "runes"), left),
                tr("Runas", "Runes"), "recall", spells.canRecall(save_, hero), [&, hero] { recallHero_ = hero, fieldSpells_ = false; });
    }
    // The Mago Negro's Identificar (ours, docs/SKILLS_MAGE.md): a tier-1 use reveals one unknown item of the party's bags
    {
      const size_t unknownCount = spells.unknownItems(save_).size();
      for (const std::string& hero : spells.identifiers(save_, test())) {
        const int level = spells.levelOf(save_, hero), left = spells.usesLeft(save_, hero, ember::FieldSpells::kIdentifyTier);
        healerRow(hero, "identify.jpg", tr("Identificar", "Identify"),
                  std::format("{} ({} {}) · {} {} · {}x", hero, en ? "Lv" : "Nv", level, unknownCount,
                              unknownCount == 1 ? tr("item desconhecido", "unknown item") : tr("itens desconhecidos", "unknown items"), left),
                  tr("Itens", "Items"), "identify", spells.canIdentify(save_, hero), [&, hero] { identifyHero_ = hero, fieldSpells_ = false; });
      }
    }
    for (const std::string& hero : mages) {
      const int level = spells.levelOf(save_, hero), left = spells.usesLeft(save_, hero, ember::FieldSpells::kWarpTier);
      const ImVec2 a = ImGui::GetCursorScreenPos();
      ImGui::PushID(hero.c_str());
      ImGui::BeginDisabled(left <= 0 || cities.empty());
      if (ImGui::InvisibleButton("##warp", {inner, 44})) warpHero_ = hero, fieldSpells_ = false;
      ImGui::EndDisabled();
      ImGui::PopID();
      emberSlot(dl, a, {a.x + inner, a.y + 44}, ImGui::IsItemHovered());
      if (const UiImage* icon = uiImage(paths_.artRoot / "game/icons/warp-portal.png")) imageContain(dl, *icon, {a.x + 6, a.y + 4}, {a.x + 42, a.y + 40});
      dl->AddText(ui, 15.0f, {a.x + 52, a.y + 12}, kFg, "Warp");
      const std::string who = std::format("{} · {} {} · {}x", hero, en ? "Lv" : "Nv", level, left);
      const ImVec2 ws = ui->CalcTextSizeA(12.0f, FLT_MAX, 0.0f, who.c_str());
      dl->AddText(ui, 12.0f, {a.x + inner - 8 - ws.x, a.y + 15}, kMuted, who.c_str());
      ImGui::SetCursorScreenPos({a.x, a.y + 52});
      ImGui::Dummy({inner, 4});
    }
    if (!mages.empty() && cities.empty()) text(tr("Nenhum outro local visitado.", "No other visited locations."), 12.0f, kMuted);
    if (healers.empty() && mages.empty() && hunters.empty() && campers.empty() && prayers.empty() && markers.empty() && recallers.empty() &&
        spells.identifiers(save_, test()).empty())
      text(tr("Nenhum feitiço de campo disponível.", "No field spells available."), 14.0f, kMuted);
    endPanel();
  }

  // Retornar's runes: a list over the map (place, region, hex, day, the note); a click takes the party there.
  void drawRecallMenu(ImVec2 size) {
    const std::string hero = recallHero_;
    const ember::FieldSpells spells(innData_, world_);
    const std::vector<ember::FieldSpells::Rune> runes = ember::FieldSpells::runes(save_);
    const bool en = uiEnglish();
    if (veil(size, "##recallveil")) {
      recallHero_.clear();
      return;
    }
    beginPanel("##recall", size, 420, size.y * 0.8f);
    const ImVec2 head = ImGui::GetCursorScreenPos();
    kicker(tr("Retornar", "Recall"), 14.0f);
    ImGui::SetCursorScreenPos({head.x + 380 - 36, head.y - 6});
    if (closeButton("##recallclose")) recallHero_.clear();
    ImGui::SetCursorScreenPos({head.x, head.y + 34});
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImFont* ui = uiFont(UiFont::Ui);
    const float inner = 380;
    text(std::format("{} · {}", hero, tr("escolha a runa", "choose the rune")), 12.0f, kMuted);
    ImGui::Dummy({1, 4});
    for (size_t i = 0; i < runes.size(); ++i) {
      const ember::FieldSpells::Rune& r = runes[i];
      const ImVec2 a = ImGui::GetCursorScreenPos();
      ImGui::PushID(static_cast<int>(i));
      const bool clicked = ImGui::InvisibleButton("##rune", {inner, 54});
      const bool hovered = ImGui::IsItemHovered();
      ImGui::PopID();
      emberSlot(dl, a, {a.x + inner, a.y + 54}, hovered);
      if (const UiImage* icon = uiImage(std::filesystem::path("content/ui/spells/rune.png"))) imageContain(dl, *icon, {a.x + 6, a.y + 5}, {a.x + 50, a.y + 49});
      dl->AddText(ui, 14.0f, {a.x + 58, a.y + 6}, kFg, r.place.c_str(), nullptr, inner - 66);
      const std::string line = std::format("{} · ({}, {}) · {} {}{}", r.region.empty() ? tr("estrada", "road") : r.region, r.col, r.row, en ? "Day" : "Dia", r.day,
                                           r.note.empty() ? "" : " · " + r.note);
      dl->AddText(ui, 11.0f, {a.x + 58, a.y + 28}, kMuted, line.c_str(), nullptr, inner - 66);
      ImGui::SetCursorScreenPos({a.x, a.y + 60});
      if (clicked) {
        if (const std::optional<ember::Json> next = spells.castRecall(save_, hero, r.id)) {
          save_ = *next;
          write();
          partyX_ = -1, centred_ = false, prevCol_ = save_["overworldPos"].value("col", 0);  // the party stands there at once
          warpAt_ = now_, warpDuration_ = 2.2f, warpRune_ = true;
          showHint(en ? std::format("{} recalled the party to {}.", hero, r.place) : std::format("{} trouxe o grupo de volta a {}.", hero, r.place));
        } else {
          showHint(tr("Não foi possível retornar.", "Could not recall."));
        }
        recallHero_.clear();
        break;
      }
    }
    if (runes.empty()) text(tr("Nenhuma runa marcada.", "No rune marked."), 12.0f, kMuted);
    ImGui::Dummy({1, 6});
    const ImVec2 b = ImGui::GetCursorScreenPos();
    if (emberBigButton("##recallcancel", upperUtf8(tr("Cancelar", "Cancel")).c_str(), {b.x + inner - 100, b.y}, {100, 32}, false, 11.0f)) recallHero_.clear();
    ImGui::SetCursorScreenPos({b.x, b.y + 40});
    ImGui::Dummy({inner, 1});
    endPanel();
  }

  // Identificar's unknown items: a list over the map (what the player sees of each); a click reveals it for good and spends the use.
  void drawIdentifyMenu(ImVec2 size) {
    const std::string hero = identifyHero_;
    const ember::FieldSpells spells(innData_, world_);
    const std::vector<ember::InnRules::UnknownItem> items = spells.unknownItems(save_);
    const bool en = uiEnglish();
    if (veil(size, "##identifyveil")) {
      identifyHero_.clear();
      return;
    }
    beginPanel("##identify", size, 420, size.y * 0.8f);
    const ImVec2 head = ImGui::GetCursorScreenPos();
    kicker(tr("Identificar", "Identify"), 14.0f);
    ImGui::SetCursorScreenPos({head.x + 380 - 36, head.y - 6});
    if (closeButton("##identifyclose")) identifyHero_.clear();
    ImGui::SetCursorScreenPos({head.x, head.y + 34});
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImFont* ui = uiFont(UiFont::Ui);
    const float inner = 380;
    text(std::format("{} · {} · {}x", hero, tr("escolha o item", "choose the item"), spells.usesLeft(save_, hero, ember::FieldSpells::kIdentifyTier)), 12.0f, kMuted);
    ImGui::Dummy({1, 4});
    for (size_t i = 0; i < items.size(); ++i) {
      const ember::InnRules::UnknownItem& u = items[i];
      const ImVec2 a = ImGui::GetCursorScreenPos();
      ImGui::PushID(static_cast<int>(i));
      const bool clicked = ImGui::InvisibleButton("##unknown", {inner, 46});
      const bool hovered = ImGui::IsItemHovered();
      ImGui::PopID();
      emberSlot(dl, a, {a.x + inner, a.y + 46}, hovered);
      ImFont* mark = uiFont(UiFont::Mark);
      dl->AddText(mark, 26.0f, {a.x + 14, a.y + 8}, IM_COL32(196, 150, 232, 255), "?");
      dl->AddText(ui, 14.0f, {a.x + 48, a.y + 7}, kFg, (en ? u.labelEn : u.label).c_str(), nullptr, inner - 56);
      dl->AddText(ui, 11.0f, {a.x + 48, a.y + 27}, kMuted,
                  (u.kind == "weapon" ? tr("arma", "weapon") : u.kind == "gear" ? tr("equipamento", "gear") : tr("poção", "potion")).c_str(), nullptr, inner - 56);
      ImGui::SetCursorScreenPos({a.x, a.y + 52});
      if (clicked) {
        if (const std::optional<ember::Json> next = spells.castIdentify(save_, hero, u)) {
          save_ = *next;
          write();
          showHint(en ? std::format("{} identified: {}.", hero, u.realName) : std::format("{} identificou: {}.", hero, u.realName));
        } else {
          showHint(tr("Não foi possível identificar.", "Could not identify."));
        }
        identifyHero_.clear();
        break;
      }
    }
    if (items.empty()) text(tr("Nenhum item desconhecido.", "No unknown item."), 12.0f, kMuted);
    ImGui::Dummy({1, 6});
    const ImVec2 b = ImGui::GetCursorScreenPos();
    if (emberBigButton("##identifycancel", upperUtf8(tr("Cancelar", "Cancel")).c_str(), {b.x + inner - 100, b.y}, {100, 32}, false, 11.0f)) identifyHero_.clear();
    ImGui::SetCursorScreenPos({b.x, b.y + 40});
    ImGui::Dummy({inner, 1});
    endPanel();
  }

  // Warp's destination wheel: the explored locations on a ring around the spell's icon.
  void drawWarpMenu(ImVec2 size) {
    const std::string hero = warpHero_;
    const ember::FieldSpells spells(innData_, world_);
    const std::vector<const ember::MapLocation*> cities = spells.warpCities(save_);
    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize(size);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(0, 0, 0, 150));
    ImGui::Begin("##warpmenu", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);
    ImGui::PopStyleColor();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImFont* ui = uiFont(UiFont::Ui);
    const ImVec2 c{size.x / 2, size.y / 2};
    const float radius = std::min(190.0f, std::min(size.x, size.y) * 0.4f);
    ImGui::SetCursorScreenPos({0, 0});
    ImGui::SetNextItemAllowOverlap();  // the city buttons drawn after it take their own clicks
    if (ImGui::InvisibleButton("##warpbg", size)) warpHero_.clear();  // a click outside closes
    dl->AddCircleFilled(c, radius + 60, IM_COL32(19, 19, 19, 242), 96);
    dl->AddCircle(c, radius + 60, IM_COL32(223, 191, 142, 115), 96, 1.5f);
    dl->AddCircleFilled(c, 48, IM_COL32(28, 28, 28, 255), 48);
    dl->AddCircle(c, 48, IM_COL32(223, 191, 142, 128), 48, 1.5f);
    if (const UiImage* icon = uiImage(paths_.artRoot / "game/icons/warp-portal.png")) imageContain(dl, *icon, {c.x - 20, c.y - 36}, {c.x + 20, c.y + 4});
    const ImVec2 ts = ui->CalcTextSizeA(12.0f, FLT_MAX, 0.0f, "Warp");
    dl->AddText(ui, 12.0f, {c.x - ts.x / 2, c.y + 6}, IM_COL32(223, 191, 142, 255), "Warp");
    const ImVec2 hs = ui->CalcTextSizeA(10.0f, FLT_MAX, 0.0f, hero.c_str());
    dl->AddText(ui, 10.0f, {c.x - hs.x / 2, c.y + 22}, kMuted, hero.c_str());
    const bool en = uiEnglish();
    for (size_t i = 0; i < cities.size(); ++i) {
      const float angle = static_cast<float>(i) / static_cast<float>(cities.size()) * 6.2831853f - 1.5707963f;
      const ImVec2 p{c.x + std::cos(angle) * radius * 0.78f, c.y + std::sin(angle) * radius * 0.78f};
      const float w = 118, h = 36;
      ImGui::SetCursorScreenPos({p.x - w / 2, p.y - h / 2});
      ImGui::PushID(static_cast<int>(i));
      const bool clicked = ImGui::InvisibleButton("##city", {w, h});
      ImGui::PopID();
      const bool hovered = ImGui::IsItemHovered();
      dl->AddRectFilled({p.x - w / 2, p.y - h / 2}, {p.x + w / 2, p.y + h / 2}, hovered ? IM_COL32(48, 48, 48, 255) : IM_COL32(40, 40, 40, 255), h / 2);
      dl->AddRect({p.x - w / 2, p.y - h / 2}, {p.x + w / 2, p.y + h / 2}, IM_COL32(223, 191, 142, 150), h / 2, 1.0f, ImDrawFlags_None);
      const ImVec2 ns = ui->CalcTextSizeA(12.0f, FLT_MAX, w - 14, cities[i]->name.c_str());
      dl->AddText(ui, 12.0f, {p.x - ns.x / 2, p.y - ns.y / 2}, IM_COL32(240, 223, 197, 255), cities[i]->name.c_str(), nullptr, w - 14);
      if (clicked) {
        if (const std::optional<ember::Json> next = spells.castWarp(save_, hero, cities[i]->id)) {
          const int level = spells.levelOf(save_, hero);
          save_ = *next;
          write();
          partyX_ = -1, centred_ = false, prevCol_ = save_["overworldPos"].value("col", 0);  // the party stands there at once
          warpAt_ = now_, warpDuration_ = std::min(3.2f, 1.6f + std::floor(std::max(0, level - ember::FieldSpells::kWarpUnlockLevel) / 6.0f) * 0.35f), warpRune_ = false;
          showHint(en ? std::format("{} crossed to {}.", hero, cities[i]->name) : std::format("{} atravessou para {}.", hero, cities[i]->name));
        } else {
          showHint(tr("Não foi possível lançar Warp.", "Could not cast Warp."));
        }
        warpHero_.clear();
        break;
      }
    }
    ImGui::SetCursorScreenPos({c.x - 50, c.y + radius + 76});
    if (emberBigButton("##warpcancel", upperUtf8(tr("Cancelar", "Cancel")).c_str(), {c.x - 50, c.y + radius + 76}, {100, 32}, false, 11.0f)) warpHero_.clear();
    ImGui::End();
  }

  // playWarpGate: the gate's picture forms, holds and closes over the map for the spell's duration.
  void drawWarpVisual(ImVec2 size) {
    if (warpAt_ < 0 || now_ - warpAt_ > warpDuration_) return;
    const float t = (now_ - warpAt_) / std::max(0.1f, warpDuration_);
    const float form = std::min(1.0f, (now_ - warpAt_) / 0.95f);  // scale and fade in over 950 ms
    const float closing = t > 0.7f ? std::min(1.0f, (t - 0.7f) / 0.18f) : 0.0f;
    const float alpha = (0.15f + 0.85f * form) * (1.0f - closing);
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    dl->AddRectFilled({0, 0}, size, IM_COL32(0, 0, 0, static_cast<int>(89 * (1.0f - closing))));
    if (const UiImage* icon = warpRune_ ? uiImage(std::filesystem::path("content/ui/spells/rune.png")) : uiImage(paths_.artRoot / "game/icons/warp-portal.png")) {
      const float h = size.y * 0.58f, w = h * static_cast<float>(icon->width) / static_cast<float>(icon->height);
      const float sx = (0.42f + 0.58f * form) * (1.0f + 0.08f * closing), sy = (0.5f + 0.5f * form) * (1.0f + 0.12f * closing);
      const ImVec2 c{size.x / 2, size.y / 2};
      dl->AddImage(icon->texture, {c.x - w * sx / 2, c.y - h * sy / 2}, {c.x + w * sx / 2, c.y + h * sy / 2}, {0, 0}, {1, 1}, IM_COL32(255, 255, 255, static_cast<int>(255 * alpha)));
    }
  }

  void drawGroupTab(const std::vector<std::string>& heroes, float inner) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImFont* ui = uiFont(UiFont::Ui);
    // Grupo ativo e reserva (the user, 2026-10-30: "active party and reserve party, we can switch which members we are going to use to a
    // max of 6"): a click on a hero moves it to the other row; the maps seat the active row (save "partyActive", Overworld::activeParty).
    kicker("Grupo ativo e reserva", 14.0f);
    {
      const std::vector<std::string> reserve = reserveHeroes();
      const ImVec2 a = ImGui::GetCursorScreenPos();
      slot(a, {a.x + inner, a.y + 236});
      std::optional<std::string> clicked;
      auto row = [&](const std::vector<std::string>& list, float y, const char* id) {
        float x = a.x + 12;
        for (const std::string& hero : list) {
          ImGui::SetCursorScreenPos({x, y});
          ImGui::PushID(id);
          ImGui::PushID(hero.c_str());
          if (ImGui::InvisibleButton("##move", {64, 72})) clicked = hero;
          const bool hover = ImGui::IsItemHovered();
          ImGui::PopID();
          ImGui::PopID();
          emberSlot(dl, {x, y}, {x + 64, y + 72}, hover);
          portrait(dl, hero, {x + 12, y + 4}, {x + 52, y + 52});
          const ImVec2 ts = ui->CalcTextSizeA(12.0f, FLT_MAX, 0, hero.c_str());
          dl->AddText(ui, 12.0f, {x + 32 - ts.x / 2, y + 54}, kFg, hero.c_str());
          x += 72;
        }
      };
      dl->AddText(ui, 14.0f, {a.x + 12, a.y + 10}, kAccent,
                  std::format("Grupo ativo · {} de {} (entra nos mapas)", heroes.size(), ember::Overworld::kActivePartyMax).c_str());
      row(heroes, a.y + 34, "active");
      dl->AddText(ui, 14.0f, {a.x + 12, a.y + 124}, kMuted, "Reserva · clique num herói para trocá-lo de fila");
      row(reserve, a.y + 148, "reserve");
      if (clicked) {
        std::vector<std::string> active = heroes;
        if (const auto it = std::ranges::find(active, *clicked); it != active.end()) {
          if (active.size() > 1) active.erase(it);
          else showHint("O grupo ativo precisa de pelo menos um herói.");
        } else if (active.size() < ember::Overworld::kActivePartyMax) {
          active.push_back(*clicked);
        } else {
          showHint("O grupo ativo já tem 6. Mande alguém para a reserva primeiro.");
        }
        if (active != heroes) {
          save_["partyActive"] = active;
          formation_.clear();  // the formation board is laid again with the new party
          write();
        }
      }
      ImGui::SetCursorScreenPos({a.x, a.y + 244});
    }
    // Líder do grupo
    kicker("Líder do grupo", 14.0f);
    {
      const ImVec2 a = ImGui::GetCursorScreenPos();
      slot(a, {a.x + inner, a.y + 120});
      dl->AddText(ui, 14.0f, {a.x + 12, a.y + 12}, kMuted, "O líder anda pelo mapa e pelas áreas livres.");
      float x = a.x + 12;
      for (const std::string& hero : heroes) {
        ImGui::SetCursorScreenPos({x, a.y + 40});
        ImGui::PushID(hero.c_str());
        const bool chosen = leader() == hero;
        if (ImGui::InvisibleButton("##leader", {64, 72})) {
          save_["partyLeader"] = hero;
          write();
        }
        ImGui::PopID();
        emberSlot(dl, {x, a.y + 40}, {x + 64, a.y + 112}, chosen);
        if (chosen) dl->AddRectFilled({x, a.y + 40}, {x + 3, a.y + 112}, IM_COL32(217, 98, 28, 255));
        portrait(dl, hero, {x + 12, a.y + 44}, {x + 52, a.y + 92});
        const ImVec2 ts = ui->CalcTextSizeA(12.0f, FLT_MAX, 0, hero.c_str());
        dl->AddText(ui, 12.0f, {x + 32 - ts.x / 2, a.y + 94}, chosen ? kAccent : kFg, hero.c_str());
        x += 72;
      }
      ImGui::SetCursorScreenPos({a.x, a.y + 128});
    }
    drawPromotions(heroes, inner);
    // Formação inicial: the radius-2 board (rows 3-4-5-4-3), two clicks swap two cells.
    kicker("Formação inicial", 14.0f);
    {
      if (formation_.empty()) {
        std::vector<std::string> saved;
        for (const ember::Json& v : save_.value("partyFormation", ember::Json::array())) saved.push_back(v.is_string() ? v.get<std::string>() : "");
        formation_.assign(19, "");
        for (size_t i = 0; i < saved.size() && i < 19; ++i)
          if (std::find(heroes.begin(), heroes.end(), saved[i]) != heroes.end()) formation_[i] = saved[i];
        for (const std::string& h : heroes)
          if (std::find(formation_.begin(), formation_.end(), h) == formation_.end())
            if (auto free = std::find(formation_.begin(), formation_.end(), ""); free != formation_.end()) *free = h;
        formationPick_ = -1;
      }
      const ImVec2 a = ImGui::GetCursorScreenPos();
      const float boardH = 24 + 5 * 76 + 24;
      slot(a, {a.x + inner, a.y + 72 + 52 + boardH});
      dl->AddText(ui, 14.0f, {a.x + 12, a.y + 12}, kMuted,
                  "A linha de frente começa nas posições mais próximas do inimigo; a retaguarda, nas mais distantes. Clique em duas casas para "
                  "trocá-las de lugar (vale a casa vazia). Mapas com aberturas perigosas ou posições especiais preservam sua formação própria.",
                  nullptr, inner - 24);
      if (emberBigButton("##saveformation", "SALVAR FORMAÇÃO", {a.x + 12, a.y + 72}, {180, 44}, true, 12.0f)) {
        // cleanPartyFormation: heroes once each, empty cells kept up to the board's size, trailing empties dropped.
        ember::Json out = ember::Json::array();
        std::set<std::string> seen;
        for (const std::string& v : formation_) {
          if (v.empty()) out.push_back("");
          else if (seen.insert(v).second) out.push_back(v);
        }
        while (!out.empty() && out.back() == "") out.erase(out.size() - 1);
        save_["partyFormation"] = out;
        if (test()) {
          write();
          formationSaved_ = "test";
        } else {
          const bool ok = campaign_.saveActive(save_) && campaign_.activeSave().value("partyFormation", ember::Json::array()) == out;
          formationSaved_ = ok ? "saved" : "failed";
        }
      }
      if (formationSaved_ == "test")
        dl->AddText(ui, 14.0f, {a.x + 204, a.y + 86}, kAccent, "✓ Formação salva nesta sessão de teste (o modo teste nunca grava no save real).");
      if (formationSaved_ == "saved") dl->AddText(ui, 14.0f, {a.x + 204, a.y + 86}, kAccent, "✓ Formação salva — conferida no arquivo do save.");
      if (formationSaved_ == "failed") dl->AddText(ui, 14.0f, {a.x + 204, a.y + 86}, kDanger, "Não foi possível salvar a formação. Tente de novo.");
      const float cx = a.x + inner / 2;
      float y = a.y + 124;
      emberKicker(dl, {cx - 36, y}, 12.0f, "INIMIGO");
      {
        const float tx = cx + 34, ty = y + 2;
        dl->AddTriangleFilled({tx, ty + 9}, {tx + 9, ty + 9}, {tx + 4.5f, ty}, IM_COL32(223, 191, 142, 255));
      }
      y += 20;
      const int rows[5][2] = {{0, 3}, {3, 4}, {7, 5}, {12, 4}, {16, 3}};
      const char* labels[5] = {"FRENTE", "", "MEIO", "", "RETAGUARDA"};
      for (int r = 0; r < 5; ++r) {
        const int start = rows[r][0], n = rows[r][1];
        const float rowW = n * 88.0f;
        float x = cx - rowW / 2;
        if (*labels[r]) emberKicker(dl, {cx - 5 * 44.0f - 100, y + 40}, 12.0f, labels[r]);
        for (int k = 0; k < n; ++k) {
          const int cellIndex = start + k;
          const ImVec2 c{x + 44, y + 48};
          ImGui::SetCursorScreenPos({x + 2, y});
          ImGui::PushID(cellIndex);
          if (ImGui::InvisibleButton("##cell", {84, 96})) {
            if (formationPick_ < 0) formationPick_ = cellIndex;
            else {
              const int from = formationPick_;
              formationPick_ = -1;
              if (from != cellIndex && !(formation_[static_cast<size_t>(from)].empty() && formation_[static_cast<size_t>(cellIndex)].empty())) {
                std::swap(formation_[static_cast<size_t>(from)], formation_[static_cast<size_t>(cellIndex)]);
                formationSaved_.clear();
              }
            }
          }
          ImGui::PopID();
          auto hexPts = [&](float w, float h, ImVec2* p) {
            p[0] = {c.x, c.y - h / 2}, p[1] = {c.x + w / 2, c.y - h / 4}, p[2] = {c.x + w / 2, c.y + h / 4};
            p[3] = {c.x, c.y + h / 2}, p[4] = {c.x - w / 2, c.y + h / 4}, p[5] = {c.x - w / 2, c.y - h / 4};
          };
          ImVec2 outer[6], innerP[6];
          hexPts(84, 96, outer), hexPts(80, 92, innerP);
          const std::string& hero = formation_[static_cast<size_t>(cellIndex)];
          dl->AddConvexPolyFilled(outer, 6, formationPick_ == cellIndex ? IM_COL32(217, 98, 28, 255) : hero.empty() ? IM_COL32(190, 150, 95, 64) : IM_COL32(190, 150, 95, 115));
          dl->AddConvexPolyFilled(innerP, 6, IM_COL32(9, 9, 9, 255));
          if (hero.empty()) {
            const ImVec2 t1 = ui->CalcTextSizeA(11.0f, FLT_MAX, 0, "vazia");
            dl->AddText(ui, 11.0f, {c.x - t1.x / 2, c.y - 12}, kMuted, "vazia");
          } else {
            portrait(dl, hero, {c.x - 18, c.y - 34}, {c.x + 18, c.y + 6});
            const ImVec2 t1 = ui->CalcTextSizeA(11.0f, FLT_MAX, 0, hero.c_str());
            dl->AddText(ui, 11.0f, {c.x - t1.x / 2, c.y + 8}, formationPick_ == cellIndex ? IM_COL32(255, 224, 180, 255) : IM_COL32(226, 194, 148, 255), hero.c_str());
          }
          const std::string n1 = std::to_string(cellIndex + 1);
          const ImVec2 t2 = ui->CalcTextSizeA(11.0f, FLT_MAX, 0, n1.c_str());
          dl->AddText(ui, 11.0f, {c.x - t2.x / 2, c.y + 22}, kMuted, n1.c_str());
          x += 88;
        }
        y += 76;
      }
      ImGui::SetCursorScreenPos({a.x, a.y + 72 + 52 + boardH + 8});
    }
    // Afinidades
    kicker("Afinidades", 14.0f);
    {
      auto score = [&](const std::string& x, const std::string& y) {
        const std::string pair = x < y ? x + "|" + y : y + "|" + x;
        return num(save_.value("affinityScores", ember::Json::object()), pair, 0);
      };
      auto grade = [](double p) {
        if (p < 0) return p <= -90 ? tr("Ódio", "Hatred") : p <= -50 ? tr("Hostil", "Hostile") : p <= -25 ? tr("Tensa", "Strained") : tr("Fria", "Cold");
        return p >= 100 ? tr("Alta", "High") : p >= 80 ? tr("Boa", "Good") : p >= 50 ? tr("Média", "Fair") : p >= 25 ? tr("Regular", "Regular") : tr("Ruim", "Poor");
      };
      // the side-by-side bonus, and the penalty below zero (Battle::affinityBonus)
      auto bonus = [](double p) { return p >= 90 ? 8 : p >= 50 ? 5 : p >= 25 ? 2 : p <= -90 ? -8 : p <= -50 ? -5 : p <= -25 ? -2 : 0; };
      const float cardW = (inner - 8) / 2;
      int n = 0;
      float rowTop = ImGui::GetCursorScreenPos().y, rowH = 0;
      const float x0 = ImGui::GetCursorScreenPos().x;
      for (const std::string& h : heroes) {
        const float x = x0 + (n % 2) * (cardW + 8);
        if (n % 2 == 0 && n) rowTop += rowH + 8, rowH = 0;
        const int others = static_cast<int>(heroes.size()) - 1;
        const float h1 = 40 + std::max(1, others) * 56.0f;
        slot({x, rowTop}, {x + cardW, rowTop + h1});
        emberTitle(dl, {x + 12, rowTop + 10}, 18.0f, h.c_str());
        float y = rowTop + 40;
        for (const std::string& o : heroes) {
          if (o == h) continue;
          const double p = score(h, o);
          dl->AddText(ui, 14.0f, {x + 12, y}, kFg, o.c_str());
          const std::string right = std::format("{} · {}/100", grade(p), std::format("{}", std::round(p * 10) / 10));
          const float rw = ui->CalcTextSizeA(14.0f, FLT_MAX, 0, right.c_str()).x;
          dl->AddText(ui, 14.0f, {x + cardW - 12 - rw, y}, kMuted, right.c_str());
          dl->AddRectFilled({x + 12, y + 22}, {x + cardW - 12, y + 28}, kBorder, 3.0f);
          // below zero the bar fills in red with how far it has fallen
          dl->AddRectFilled({x + 12, y + 22}, {x + 12 + (cardW - 24) * static_cast<float>(std::clamp(std::abs(p), 0.0, 100.0) / 100.0), y + 28},
                            p < 0 ? IM_COL32(196, 58, 46, 255) : kAccent, 3.0f);
          const std::string help = bonus(p) < 0 ? std::format("{}: {}% ⓘ", tr("Penalidade por adjacência", "Side-by-side penalty"), bonus(p))
                                                : std::format("{}: +{}% ⓘ{}", tr("Bônus por adjacência", "Side-by-side bonus"), bonus(p),
                                                              p >= 80 ? tr(" · Skill de dupla desbloqueada", " · Duo skill unlocked") : std::string());
          dl->AddText(ui, 12.0f, {x + 12, y + 32}, kMuted, help.c_str());
          y += 56;
        }
        if (others < 1) dl->AddText(ui, 14.0f, {x + 12, y}, kMuted, "As afinidades aparecerão quando outro personagem entrar no grupo.", nullptr, cardW - 24);
        rowH = std::max(rowH, h1);
        ++n;
      }
      ImGui::SetCursorScreenPos({x0, rowTop + rowH + 8});
      ImGui::Dummy({inner, 1});
      text(tr("25: +2% · 50: +5% · 80: skill de dupla · 90: +8% · 100 nas três relações: Ultimate de trio · −25: −2% · −50: −5% · −90: −8%",
              "25: +2% · 50: +5% · 80: duo skill · 90: +8% · 100 in all three bonds: trio Ultimate · −25: −2% · −50: −5% · −90: −8%"),
           12.0f, kMuted, UiFont::Ui, inner);
    }
  }

  void drawSkillsTab(const std::vector<std::string>& heroes, float inner) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImFont* ui = uiFont(UiFont::Ui);
    const ember::Json& t = world_.tables();
    const double stepHours = t.value("travelTrainingHours", 12.0), gain = t.value("skillGain", 0.1), cap = t.value("skillCap", 100.0);
    // Party skill order: weapon skills, then Poison Resistance, then the other resistances and Healing as listed.
    std::vector<std::string> order;
    for (const ember::Json& id : t["skillIds"])
      if (id.get<std::string>().ends_with("Weapon")) order.push_back(id.get<std::string>());
    order.push_back("poisonResistance");
    for (const ember::Json& id : t["skillIds"])
      if (!id.get<std::string>().ends_with("Weapon") && id.get<std::string>() != "poisonResistance") order.push_back(id.get<std::string>());
    order.push_back("dodging");  // Esquiva (ours, not in Ember's list)
    const float cardW = (inner - 8) / 2;
    int n = 0;
    float rowTop = ImGui::GetCursorScreenPos().y, rowH = 0;
    const float x0 = ImGui::GetCursorScreenPos().x;
    for (const std::string& hero : heroes) {
      const float x = x0 + (n % 2) * (cardW + 8);
      if (n % 2 == 0 && n) rowTop += rowH + 8, rowH = 0;
      const std::string training = save_.value("travelTraining", ember::Json::object()).value(hero, "");
      std::vector<std::string> shown;
      for (const std::string& id : order) {
        if (id.ends_with("Weapon") && !heroUsesWeapon(hero, id.substr(0, id.size() - 6))) continue;
        shown.push_back(id);
      }
      const float h = 36 + 22 + static_cast<float>(shown.size()) * 34 + 12;
      slot({x, rowTop}, {x + cardW, rowTop + h});
      emberTitle(dl, {x + 12, rowTop + 10}, 18.0f, hero.c_str());
      std::string line = "Treino na estrada: nenhum — toque em “Treinar” numa skill.";
      if (!training.empty()) {
        const double banked = std::fmod(num(save_.value("travelTrainingHours", ember::Json::object()), hero, 0), stepHours);
        const int left = std::max(1, static_cast<int>(std::ceil(stepHours - banked)));
        line = std::format("Treino na estrada: {} · próximo +0,1 em {}h de viagem",
                           training == "dodging" ? tr("Esquiva", "Dodging") : t["skillNames"].value(training, training), left);
      }
      dl->AddText(ui, 12.0f, {x + 12, rowTop + 36}, training.empty() ? kMuted : kAccent, line.c_str(), nullptr, cardW - 24);
      float y = rowTop + 58;
      for (const std::string& id : shown) {
        const double points = num(save_.value("heroSkills", ember::Json::object()).value(hero, ember::Json::object()), id, 0);
        const bool on = training == id;
        std::string name = id == "dodging" ? tr("Esquiva", "Dodging") : t["skillNames"].value(id, id);
        // both decimals always shown, as in the battle's Skills (the user, 2026-10-27: "display every little 0.01%")
        std::string shownPoints = std::format("{:.2f}", points);
        if (!uiEnglish()) std::replace(shownPoints.begin(), shownPoints.end(), '.', ',');
        if (id == "healing") name += std::format(" · +{}% healing", shownPoints);
        dl->AddText(ui, 14.0f, {x + 12, y}, kFg, name.c_str());
        const std::string value = std::format("{}/{}", shownPoints, cap);
        const float vw = ui->CalcTextSizeA(14.0f, FLT_MAX, 0, value.c_str()).x;
        dl->AddText(ui, 14.0f, {x + cardW - 12 - vw, y}, kMuted, value.c_str());
        ImGui::PushID((hero + id).c_str());
        if (emberBigButton("##train", on ? "TREINANDO" : "TREINAR", {x + cardW - 12 - vw - 8 - 96, y - 4}, {96, 24}, on, 10.0f)) {
          ember::Json& tr = save_["travelTraining"];
          if (!tr.is_object()) tr = ember::Json::object();
          if (on) tr.erase(hero);
          else tr[hero] = id;
          if (!save_["travelTrainingHours"].is_object()) save_["travelTrainingHours"] = ember::Json::object();
          save_["travelTrainingHours"][hero] = 0;
          markTrainingHint();
          write();
        }
        ImGui::PopID();
        dl->AddRectFilled({x + 12, y + 22}, {x + cardW - 12, y + 28}, kBorder, 3.0f);
        dl->AddRectFilled({x + 12, y + 22}, {x + 12 + (cardW - 24) * static_cast<float>(std::clamp(points / cap, 0.0, 1.0)), y + 28}, kAccent, 3.0f);
        y += 34;
      }
      rowH = std::max(rowH, h);
      ++n;
    }
    ImGui::SetCursorScreenPos({x0, rowTop + rowH + 8});
    ImGui::Dummy({inner, 1});
    text(std::format("Treino na estrada: cada personagem pode treinar uma skill por vez enquanto o grupo viaja — +0,1 a cada {}h de viagem. Trocar de skill "
                     "recomeça a contagem.",
                     static_cast<int>(stepHours)),
         12.0f, kMuted, UiFont::Ui, inner);
    (void)gain;
  }

  // weaponSkills.ts weaponTypesForClass: which weapon skills a hero's class trains (from the exported weapon list).
  bool heroUsesWeapon(const std::string& hero, const std::string& type) const {
    const ember::Json& types = world_.tables().contains("classWeaponTypes") ? world_.tables()["classWeaponTypes"] : ember::Json();
    std::string cls = world_.tables()["baseClass"].value(hero, "");
    if (save_.contains("promotions") && save_["promotions"].contains(hero) && save_["promotions"][hero].is_string()) cls = save_["promotions"][hero].get<std::string>();
    if (!types.is_object() || !types.contains(cls)) return true;
    for (const ember::Json& t : types[cls])
      if (t == type) return true;
    return false;
  }

  void markTrainingHint() {
    ember::Json& flags = save_["flags"];
    if (!flags.is_array()) flags = ember::Json::array();
    for (const ember::Json& f : flags)
      if (f == "hint:travel-training") return;
    flags.push_back("hint:travel-training");
  }

  // One-time tip once A Ponte de Pedra is cleared: Party → Skills.
  void drawTrainingHint(ImVec2 size) {
    // Under every other panel (Ember: z-30 against their z-40 veils).
    if (!completed("thebridge") || party_ || event_ || open_ || questLog_ || options_ || !save_.value("seenOverworldIntro", false)) return;
    for (const ember::Json& f : save_.value("flags", ember::Json::array()))
      if (f == "hint:travel-training") return;
    ImGui::SetNextWindowPos({size.x / 2, 144}, ImGuiCond_Always, {0.5f, 0.0f});
    ImGui::SetNextWindowSize({std::min(480.0f, size.x - 32), 0});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {16, 16});
    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(0, 0, 0, 0));
    ImGui::Begin("##traininghint", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize);
    const ImVec2 a = ImGui::GetWindowPos(), b{a.x + ImGui::GetWindowSize().x, a.y + ImGui::GetWindowSize().y};
    ImGui::GetWindowDrawList()->ChannelsSplit(2);
    ImGui::GetWindowDrawList()->ChannelsSetCurrent(0);
    emberPanel(ImGui::GetWindowDrawList(), a, b);
    ImGui::GetWindowDrawList()->ChannelsSetCurrent(1);
    title("Dica · Treino na estrada", 18.0f);
    text("Abra o menu Party e explore a aba Skills. Toque em “Treinar” numa skill de cada personagem: ela sobe sozinha enquanto o grupo viaja, +0,1 a cada 12h de "
         "estrada. Uma skill por vez — nada treina até você escolher.",
         14.0f, kMuted, UiFont::Ui, std::min(448.0f, size.x - 64));
    ImGui::Dummy({1, 8});
    const ImVec2 at = ImGui::GetCursorScreenPos();
    if (emberBigButton("##hintok", "ENTENDI", at, {120, 32}, false, 11.0f)) markTrainingHint(), write();
    if (emberBigButton("##hintopen", "ABRIR PARTY · SKILLS", {at.x + 128, at.y}, {200, 32}, true, 11.0f)) {
      markTrainingHint();
      write();
      party_ = true, partyTab_ = 1, formation_.clear(), formationSaved_.clear();
    }
    ImGui::GetWindowDrawList()->ChannelsMerge();
    ImGui::End();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar();
  }

  // GameApp OverworldIntroScreen: the movement and hunger explainer, once per campaign.
  void drawIntro(ImVec2 size) {
    ImGui::SetCursorScreenPos({0, 0});
    ImGui::InvisibleButton("##introveil", size);
    ImGui::GetWindowDrawList()->AddRectFilled({0, 0}, size, IM_COL32(17, 17, 17, 230));
    beginPanel("##intro", size, 448, size.y * 0.9f);
    kicker("Sistema", 12.0f);
    title("Movimento e fome", 24.0f);
    const float w = 408;
    text("Clique no personagem no mapa para ver os hexágonos que o grupo pode alcançar e escolher para onde ir. Cada passo custa um dia.", 14.0f, kMuted, UiFont::Ui, w);
    ImGui::Dummy({1, 6});
    text("Cada herói tem uma barra de saciedade. Ela desce ao longo da marcha pelo mapa e um pouco a cada ação em combate. Rações (compradas na Estalagem ou "
         "achadas em batalha) e refeições na Estalagem enchem essa barra de volta.",
         14.0f, kMuted, UiFont::Ui, w);
    ImGui::Dummy({1, 6});
    text("Se ela chegar a zero e o grupo continuar sem comer, começa o status de Fome: uma penalidade de −10% em todos os atributos, que piora a cada dia faminto "
         "até um teto de −90%.",
         14.0f, kMuted, UiFont::Ui, w);
    ImGui::Dummy({1, 6});
    text("Fique de olho na barra e mantenha rações na mochila antes de partir.", 14.0f, kMuted, UiFont::Ui, w);
    ImGui::Dummy({1, 10});
    const ImVec2 at = ImGui::GetCursorScreenPos();
    if (emberBigButton("##introok", "ENTENDI", at, {w, 44}, true, 13.0f)) {
      save_["seenOverworldIntro"] = true;
      save_["pendingMission"] = nullptr;
      write();
    }
    endPanel();
  }

  // Opções (EmberMenus.h OptionsDialog): Jogo (Salvar jogo, Carregar jogo, Voltar à tela inicial), Gráficos, Áudio, Idiomas,
  // Acessibilidade.
  void drawOptions(ImVec2 size) {
    if (!optionsDialog_.isOpen()) optionsDialog_.open();
    menu::OptionsDialog::Actions actions;
    // Modo teste has no Salvar jogo (GameApp passes onSaveGame only outside test mode).
    if (!test())
      actions.save = [this] {
        slots_.emplace();
        slots_->open(SlotScreen::Mode::Save, true);
      };
    actions.load = [this] {
      slots_.emplace();
      slots_->open(SlotScreen::Mode::Load, true);
    };
    actions.title = callbacks_.title;
    actions.fullscreen = callbacks_.fullscreen;
    actions.gfx = callbacks_.gfx;
    optionsDialog_.draw(size, actions);
    if (!optionsDialog_.isOpen()) options_ = false;
  }

  // A hero's portrait: THE one (content/hero-portraits.json, Portraits.h), drawn with the same cut as on every screen.
  void portrait(ImDrawList* dl, const std::string& hero, ImVec2 a, ImVec2 b) {
    const HeroPortrait* p = heroPortrait(hero);
    if (!p) return;
    if (const UiImage* img = uiImage(paths_.artRoot / std::filesystem::path(reinterpret_cast<const char8_t*>(p->file.c_str()))))
      drawHeroPortrait(dl, *img, a, b, *p);
  }

  CampaignPaths paths_;
  int drawn_ = 0;          // frames drawn (the loading curtain lifts once the screen is up)
  bool mapDrawn_ = false;  // the world map itself has been drawn
  ember::Campaign& campaign_;
  const ember::Overworld& world_;
  Callbacks callbacks_;
  ember::Json save_;
  std::string notice_, hint_;
  float hintAt_ = -10, now_ = 0;
  std::optional<ember::OverworldEvent> event_;
  std::optional<std::string> open_;  // the location whose mission list is open
  std::optional<MissionEntry> briefing_;
  std::optional<SlotScreen> slots_;
  ember::InnData innData_;
  ember::InnRules innRules_{innData_, world_};
  bool hasInn_ = false;
  std::optional<ember::Battle> sheetBattle_;  // the party the status sheet shows (openSheet)
  ember::BattleStatusSheet sheet_;
  std::string sheetReturn_;  // the hero whose sheet opened the inventory: back to it when it closes
  ember::Underworld under_;  // the Underworld's board (drawUnderworld)
  bool hasUnder_ = false, underCentred_ = false;
  ember::Companions companions_;
  bool hasCompanions_ = false;
  bool fieldSpells_ = false;     // the field-spells menu is open
  std::string warpHero_;         // the mage choosing Warp's destination (the wheel is open)
  std::string recallHero_;       // the conjurer choosing Retornar's rune (the list is open)
  std::string identifyHero_;     // the mage choosing Identificar's item (the list is open)
  bool warpRune_ = false;        // the crossing's picture is the rune's (Retornar), not Warp's gate
  float warpAt_ = -100, warpDuration_ = 0;  // the gate's picture over the map
  std::string convCompanion_, convLeader_;
  std::optional<DialogScreen> convDialog_;
  std::optional<InnScreen> inn_;
  std::optional<InventoryScreen> inventory_;
  std::optional<CutsceneScreen> film_;
  std::function<void()> afterFilm_;
  double previewFilmAt_ = 0;
  bool movementOpen_ = false, questLog_ = false, party_ = false, options_ = false;
  menu::OptionsDialog optionsDialog_;  // Opções (EmberMenus.h), shown while options_
  int partyTab_ = 0;
  bool partyScrollEnd_ = false;  // capture "party:end"
  std::vector<std::string> formation_;
  int formationPick_ = -1;
  std::string formationSaved_;
  ember::Json* testSave_ = nullptr;  // Modo teste: the session's save (see the constructor)
  const ember::InterludeData* interlude_ = nullptr;  // the historical chapter's data (the Ruins' gate, its start), when loaded
  std::string flashId_;
  float flashAt_ = -10;
  int zoom_ = 0;
  int underworldZoom_ = 0;  // the Overworld's first stop, as zoom_
  float scrollX_ = 0, scrollY_ = 0, centreX_ = 0.5f, centreY_ = 0.5f;
  bool centred_ = false, recentre_ = false;
  bool underworld_ = false;                          // the Underworld's Atlas on show instead of the overworld (toggleWorld)
  float otherCentreX_ = 0.5f, otherCentreY_ = 0.5f;  // the view of the world not on show
  struct Drag {
    float x, y, sx, sy;
  };
  std::optional<Drag> drag_;
  bool dragMoved_ = false;
  float partyX_ = -1, partyY_ = 0, fromX_ = 0, fromY_ = 0, targetX_ = -1, targetY_ = -1, moveAt_ = -10;
  int prevCol_ = 0;
  bool facingLeft_ = false;
  int frames_ = 0;
  int releaseUiAt_ = -1;  // --map-ui releaseui: the frame the UI images are released (a capture's test of the loading screen's release)
  // A luz e a névoa das duas telas (render/WorldMapFx.h, shaders/worldmap.hlsl): o passe, os dois arquivos de configuração
  // ([0] mapa-múndi, [1] Submundo), as pinturas e máscaras na GPU, o revelado subido quando os hexágonos explorados mudam,
  // a hora suavizada na tela (sem saltos quando o relógio anda horas inteiras num passo).
  std::optional<WorldMapFx> fx_;
  WorldMapConfig fxConfig_[2];
  uint32_t fxMap_[2] = {WorldMapFx::kNoTexture, WorldMapFx::kNoTexture}, fxMask_[2] = {WorldMapFx::kNoTexture, WorldMapFx::kNoTexture};
  // O look 2D do mapa-múndi (2026-10-28): as LUTs por hora (na ordem de fxConfig_[0].luts) e as máscaras pintadas pelo usuário.
  std::vector<uint32_t> fxLuts_[2];
  uint32_t fxTex_[2][WorldMapFx::Draw::kTextures] = {{WorldMapFx::kNoTexture, WorldMapFx::kNoTexture, WorldMapFx::kNoTexture, WorldMapFx::kNoTexture, WorldMapFx::kNoTexture,
                                                       WorldMapFx::kNoTexture, WorldMapFx::kNoTexture, WorldMapFx::kNoTexture, WorldMapFx::kNoTexture},
                                                      {WorldMapFx::kNoTexture, WorldMapFx::kNoTexture, WorldMapFx::kNoTexture, WorldMapFx::kNoTexture, WorldMapFx::kNoTexture,
                                                       WorldMapFx::kNoTexture, WorldMapFx::kNoTexture, WorldMapFx::kNoTexture, WorldMapFx::kNoTexture}};
  const char* fxOn(int world, int i) const { return fxTex_[world][i] != WorldMapFx::kNoTexture ? "on" : "off"; }
  size_t revealKey_ = 0;  // o que o revelado subido representa (um hash dos hexágonos explorados)
  float shownHour_ = -1;  // a hora da luz na tela (segue o relógio em hourEaseSeconds)
  float lastHourAt_ = 0;
  bool measure_ = false;  // --map-ui measure
  std::vector<ember::OverworldHex> slowSteps_;  // --map-ui walkslow:...
  size_t slowNext_ = 0;
  int slowLast_ = 0;
  double measureUiMs_ = 0, measureCpuMs_ = 0;
  int measureFrames_ = 0, measureCpuFrames_ = 0;
};

}  // namespace ember
