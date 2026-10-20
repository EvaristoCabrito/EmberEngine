#pragma once

// A real Ember map from Engine2 (C:/Engine2/maps): its board and ground in both looks, its water, and its
// scene as Engine2 places it (decoration cards, 3D models, units from the spawns; imported by
// tools/scripts/engine2/export_scene.ts + measure_art.py), with Engine2's camera. D1 port, step by step
// (docs/ENGINE2_AUDIT.md). Engine2 stays read-only: maps and relief grids are read from it; textures,
// sprites and meshes come from the cooked packs (Cooker ground / sprites / meshes).
//
// Left button (Engine2's protected rule): hold 0.5 s, then drag to pan the board; a short click acts in the game.
// Camera (Ember's own orbit camera, 2026-10-15): opens turned -30 deg, tilted 20 deg, 30 deg lens, framing the
// whole board. It always turns around a pivot on the ground and glides to every input. Right-drag turns and
// tilts (5 to 89.5 deg), middle-drag grabs the ground, the wheel zooms toward the cursor, WASD/arrows slide the
// board (the pivot stays on the board, kept in from its edges by 60% of the view: clampGoal); the eye never goes into the
// ground or a 3D piece. The camera bar (top right, Engine2's battle HUD copied 1:1 at the user's request): Normal /
// Tática, tilt 5 deg down / up, turn 15 deg left / right, back to the normal angle. Tática is the old-school 2D view
// (Engine2 CameraRig.flat): straight overhead through a 1 deg lens from far away (no visible perspective), no
// turning; cards and units lie flat facing up just over the highest ground, nearer rows drawn over farther ones.
//
// Combat (world/Battle.h, --battle-rules <map.battle.json> from tools/scripts/engine2/export_battle.ts): the left button
// picks a unit (its card or its hex) and shows its movement reach as Engine2's battle grid does; hovering a reachable hex
// shows the route, clicking it walks the unit there one hex per 0.22 s with Engine2's walk poses (UnitActor); "Fim do
// turno" gives every unit its movement back. Esc lets go of the unit. Foes the picked party unit can hit from where it
// stands are marked (Engine2's target red); hovering one shows Engine2's forecast, clicking attacks: both turn to face,
// the attack pose plays, at its impact Ember's roll lands (world/Combat.h: damage, hit chance, crit, the arcane / staff
// element), the target shows its hit or death pose and the number, then it strikes back when it can.
// Turns are Engine2's: one battle-opening initiative roll per unit (1d20 + class bonus), a round in that order, each
// unit once. The party's unit whose turn it is gets picked; it walks (its movement pool can be split) and strikes once;
// a foe it can reach from any hex of its walk is marked too, and clicking it walks there and strikes. "Fim do turno"
// ends its turn (so does striking with no movement left). Enemies take their turns on their own (Engine2 runAiFor's
// plain branch: strike the best foe they can reach, else close in on the nearest hero; enmity picks their focus).
// Other units can be inspected (their reach) at any time. Vitória / Derrota when one side has nobody standing.
//
// Fog of war (R7, world/FogOfWar.h) on maps that use it (or forced with --fog 1): the party's sight with
// occlusion; enemies out of sight are not drawn; the renderer darkens and masks everything by the mask.
// --scout walks the first party unit to the farthest enemy and back (a hex every 0.6 s), so never-seen,
// remembered and in-sight ground show together while lights move.

#include <SDL3/SDL.h>
#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <optional>
#include <chrono>
#include <numbers>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <format>
#include <limits>
#include <string_view>
#include <utility>
#include <array>
#include <set>

#include "Stage.h"
#include "core/Jobs.h"
#include "core/Log.h"
#include "render/BoardGround.h"
#include "render/DebugUi.h"
#include "render/FogLooks.h"
#include "render/Renderer.h"
#include "rhi/UploadQueue.h"
#include "render/MeshScene.h"
#include "BattleStatus.h"
#include "QuestLog.h"
#include "InventoryScreen.h"
#include "EmberMenus.h"
#include "GameMusic.h"
#include "SpellsMenu.h"
#include "media/AudioMixer.h"
#include "render/SpellEffects.h"
#include "render/SpriteSystem.h"
#include "world/Battle.h"
#include "world/Combat.h"
#include "world/Initiative.h"
#include "world/Inn.h"
#include "world/Loadout.h"
#include "world/Quests.h"
#include "world/Spells.h"
#include "render/UiArt.h"
#include "world/Board.h"
#include "world/FogOfWar.h"
#include "world/TimeOfDay.h"
#include "world/GroundAssets.h"
#include "world/GroundMesh.h"
#include "world/SceneData.h"
#include "render/MeshBuild.h"
#include "world/Architecture.h"
#include "world/SceneBuilder.h"

namespace ember {

struct MapStageOptions {
  std::filesystem::path map;
  std::filesystem::path scene;  // imported scene (optional): decorations, models, units
  uint64_t spriteBudgetMb = 1024;
  std::filesystem::path boardData = "content/engine2/board.json";
  std::filesystem::path engine2Public = "C:/Engine2/public";
  e2::EdgeStyle look = e2::EdgeStyle::Smooth;
  // Captures: camera relative to Engine2's normal view, and lighting layers switched on.
  float zoom = 1.0f;          // distance divided by this
  float turnDegrees = 0.0f;   // added to the normal view's turn
  float tiltDegrees = 0.0f;   // added to its tilt
  glm::vec2 offset{0.0f};     // target moved by this many board widths / depths
  uint32_t layers = 0xFFFFFFFFu;
  // Sun direction override (degrees; elevation < 0 = keep the default): azimuth from +x towards +z.
  float sunAzimuth = 0.0f, sunElevation = -1.0f;
  // Test lights for the proof scene: a torch carried around the party, a spell's light flying from the
  // party's caster to the nearest enemy and flashing on impact.
  bool movingLight = false, spellLight = false;
  // Spell effects test: the hero casts this spell's visual (Engine2 kind, or "all" for each ported one in turn) at the
  // nearest enemy every kFxTestCycle seconds, visuals only (no rules); for captures with --scene-time.
  std::string fxTest;
  // Battle test (runScript): steps run through the battle bar's own calls, each checked when it has played; the
  // run fails (exit 3) if any check fails. "hurt; cast:Salazar:cureWounds:Kael; food:Salazar; potion:Kael:mid:Kael".
  std::string battleScript;
  bool tatica = false;  // open in the 2D view (Tática), for captures
  bool spellsMenu = true;  // the Magias window (every spell's effect version, loose effects; test mode)
  bool choicesPanel = true;  // the Escolhas window (the look switches; test mode)
  bool spellsMenuOpen = false;  // ... opened with every list unfolded (captures)
  // Gameplay measurement: for this many seconds the camera turns a full circle and zooms in and out twice
  // while units animate and lights move and flicker; every frame's main-thread and GPU time is recorded.
  float measureSeconds = 0.0f;
  // Fog of war: -1 = as the map says, 0 off, 1 on. scout: the walk above.
  int fog = -1;
  bool scout = false;
  // Volumetric fog (R7's literal fog, shaders/volume.hlsl): on unless switched off (--volume 0).
  bool volume = true;
  bool airParticles = true;  // the map's wisps and dust motes (Dev Controls "Efeitos atmosféricos" off: none)
  int fogLook = 8;  // the fog look shown first (render/FogLooks.h numbers): 008 = 003 without the veil up close
  int gridLook = 2;  // the panel's "Grade" at the start: 0 OFF, 1 antiga (the marks), 2 = look 001 (--grid-look N)
  int cardStance = 1;  // the panel's "Cards" at the start: 0 em pé (vertical), 1 para a câmera (--cards N)
  int barsFade = 1;    // the panel's "Vida perto" at the start: 0 OFF, 1 bars near the camera fade (--bars-fade N)
  // Atmosfera volumétrica v1: the panel's "Atmosfera" at the start (0 OFF, 1 cinema, 2 alta, 3 desempenho; --atmos N).
  // ON by default at "alta" (user, 2026-10-16: "padrão on"); the choice stays in the panel.
  int atmos = 2;
  // The panel's "Fundo" at the start (-1 as the map says: its painting, or the sky without one; 0 the painting, 1 the
  // sky, 2 OFF = black; --background N): captures that must keep never-seen ground exactly black (check_fog.py) use 2.
  int background = -1;
  // Water (2026-10-19): the panel's "Água" (0 the original = Engine2's diorama water, the default; 1 look 001 "ember";
  // --water N), "Reflexo do céu" (0 OFF, 1 the sky's reflection map; --sky-reflect N) and "Nuvens" (0 OFF, 1 básica; --clouds N).
  int waterLook = 0;
  int skyReflect = 1;
  int clouds = 2;     // "Nuvens": 0 OFF, 1 básica (the sheets), 2 volumétricas (look 001, the default; --clouds N)
  int moonLight = 1;  // the panel's "Luar" (light mode real): 0 físico, 1 cinema (--moonlight N)
  int hour = 0;       // the panel's "Hora": 0 do mapa, 1 ciclo rápido, 2 ciclo lento, 3..10 fixed hours (--hour N)
  // The game's own clock (the campaign's / the test world map's gameHour, 0..23): the sun and sky follow it while the panel's Hora is "do mapa"
  // (the user, 2026-10-20: "the sun must always follow the time of the day the map is set, or during campaign follow the real game time").
  float gameHour = -1.0f;  // -1: the map's own time of day
  int sunDisc = 1;    // the panel's "Sol": 0 real, 1 cinema (the disc the user had; default), 2 look 001 (--sun-disc N)
  int glare = 0;      // "Clarão": 0 OFF (default: the user never asked for it), 1 look 001 (--glare N)
  int shafts = 0;     // "Raios de luz": 0 OFF (default), 1 look 001 (--shafts N)
  // The fog of war's mist bank (the numbered looks) on at the start; --mist 0 leaves only the fog of war's mask (check_fog.py
  // needs never-seen ground exactly black) while the volumetric atmosphere may stay on.
  bool mist = true;
  // Lighting (also in the panel): 0 the old prototype day (every capture before 2026-10-15), 1 Engine2's 3D presets for
  // the map's time of day (the floor), 2 physical sky with Engine2's strengths, 3 real (default): lux, physical camera.
  int lighting = 3;
  // Scene clock pinned to this many seconds (< 0 = real time): identical frames for paired captures.
  float sceneTime = -1.0f;
  // Night: tonight's moon phase (Engine2's moonPhase.ts names; "" = Engine2's stage default: the full moon's art at
  // the night's own strength) and where the moon's disc and light stand (see moonPlacement()); also in the panel.
  std::string moonPhase;
  int moonPath = 0;
  // Every unit's poses (tools/scripts/engine2/export_heroes.ts output; empty = idle only): the map's own units
  // play the pose chosen in the panel, each from its own catalog entry (found by the scene unit's key).
  std::filesystem::path heroes;
  int starLook = 3;  // the star field's saved look (looks/stars/README.md; also in the panel)
  // The battle's rules and units (tools/scripts/engine2/export_battle.ts output; empty = no combat).
  std::filesystem::path battle;
  // A free-roam map (an explore mission: the walkable Inn) set up from the save as GameApp startBattle does: companions not
  // recruited yet are not seated there (`absentNeutrals`, by name), a party leader other than the authored walker walks in
  // its place (`leader`), and a floor connector taken on another map (`arrivedFrom`: that map's mission id) puts the party
  // beside the connector leading back (arriveAtConnector) instead of on the authored spawns.
  std::set<std::string> absentNeutrals;
  std::string leader, arrivedFrom;
  // A dungeon (mapstore keepsDefeatedSpawns): the enemy / neutral spawn ids killed on earlier visits (save.crossingDefeatedSpawns
  // of this mission), never spawned again; and after a floor connector (GameApp preserveKnockouts) the heroes whose carried HP
  // is 0 stay out of this floor.
  std::set<std::string> defeatedSpawns;
  bool preserveKnockouts = false;
  // The party (campaignParty.ts: "campaign progression defines the party; editor spawns only provide entry positions"): the
  // heroes the save has recruited (none set = every hero in the battle file: Modo teste), the others leave, and a neutral of a
  // recruited hero's name (a prisoner already freed) is not seated. Then the Party screen's Formação inicial (save.partyFormation:
  // 19 board cells front to back) lays them out at the map's entry; `formation` off = the file's own places.
  std::optional<std::set<std::string>> partyRoster;
  std::vector<std::string> partyFormation;
  bool formation = false;
  // wispCrossing.ts routeWispCrossing: the Wisp Forest crossing's way on. Until the crossing is finished its exits lead to the
  // carnivorous plant's lair (a floor connector up to wisp-forest-crossing-boss); once finished they lead outside (dungeon exits).
  bool wispCrossingDone = false;
  // A campaign fight: each hero's level and XP from the save (save.ts levels / xp), by name.
  std::map<std::string, std::pair<int, int>> heroProgress;
  // ... and each hero's promotion (save.ts promotions: "warlock" / "elementalist"), by name: its class, class name and
  // spell list in the fight (e2::setPromotion). Also --promote Name=class for tests.
  std::map<std::string, std::string> heroClass;
  // ... and what each hero brings in from the save (Engine2 startBattle's roster: unitHp, bags, heroDiseases,
  // heroPoisons, heroHunger and the starving penalty).
  struct HeroState {
    std::optional<int> hp;
    std::optional<std::array<int, 7>> bag;  // in e2::kPotionIds order
    bool starterBag = false;                // the save has bags but none for it: its class's starting bag
    bool diseased = false;
    std::string poison;                     // heroPoisons' tier, empty = not poisoned
    std::optional<double> fullness;
    double hungerPenalty = 0;
    std::array<int, 5> points{};            // statPointAllocations: hp, atk, mag, def, dex
    int lockpick = 0;                       // bags[name].lockpick
  };
  std::map<std::string, HeroState> heroState;
  std::map<std::string, double> affinityScores;  // the save's party affinities (pair "A|B" -> 0..100), when a campaign fight starts
  // The world's tables, for the status sheet's Mochila / Equipar (the party's inventory over a fight).
  const e2::Overworld* world = nullptr;
  // The map editor's preview (MapEditorStage.h): the draft and its scene given in memory instead of `map` / `scene`,
  // and a left click on a hex handed to editorClick instead of played; only the camera bar is drawn.
  std::optional<e2::MapDraft> draftData;
  std::optional<e2::SceneData> sceneData;
  std::function<void(int x, int y)> editorClick;
  // Engine2's MapPreviewCanvas gestures: a press on a unit or a decoration (the right button, or the left one when
  // editorGrab says so) picks it up instead of turning the camera or painting; on release, editorRelease gets the hex
  // under the cursor and whether the pointer travelled 6 px (a drag) or not (a plain pick-up).
  std::function<bool(int x, int y, bool left)> editorGrab;
  std::function<void(std::optional<std::pair<int, int>> hex, bool moved)> editorRelease;
};

// What a fight leaves for the campaign save (campaign.ts finishBattle's heroes part): each hero's level, XP and HP, the
// potions left, disease, poison and hunger.
struct HeroOutcome {
  std::string name;
  int level = 1, xp = 0, hp = 0;
  int campHp = 0;            // after a victory: the HP it keeps (persistVictory: half the missing healed, or carried past a floor connector)
  std::array<int, 7> bag{};  // in e2::kPotionIds order
  bool diseased = false;
  std::string poison;        // empty = not poisoned
  std::optional<double> fullness;
  std::optional<double> dodging;  // its Esquiva as the fight leaves it (ours)
};

class MapStage final : public Stage {
 public:
  MapStage(VulkanContext& context, Renderer& renderer, AssetManager& assets, JobSystem& jobs, const MapStageOptions& options)
      : renderer_(renderer),
        uploads_(context),
        ground_(context, renderer.bindless(), assets, uploads_),
        sprites_(context, renderer.bindless(), assets, uploads_, {options.spriteBudgetMb << 20}),
        meshes_(context, renderer.bindless(), assets, uploads_, {.geometryBytes = 256ull << 20}),
        assets_(assets),
        look_(options.look),
        options_(options),
        jobs_(jobs) {
    yaw_ += glm::radians(options.turnDegrees);
    lightingMode_ = std::clamp(options.lighting, 0, 3);
    moonPath_ = std::clamp(options.moonPath, 0, 2);
    starLook_ = std::clamp(options.starLook, 1, kStarLooks);
    for (int i = 0; i < static_cast<int>(std::size(e2::kMoonPhases)); ++i)
      if (options.moonPhase == e2::kMoonPhases[i]) moonPhase_ = i;
    for (uint32_t i = 0; i < fogLooks().size(); ++i)
      if (options.fogLook == std::atoi(fogLooks()[i].number)) fogLook_ = i;
    pitch_ += glm::radians(options.tiltDegrees);
    flat_ = options.tatica;
    atmosChoice_ = std::clamp(options.atmos, 0, 3);
    gridLook_ = std::clamp(options.gridLook, 0, 2);
    cardStance_ = std::clamp(options.cardStance, 0, 1);
    barsFade_ = std::clamp(options.barsFade, 0, 1);
    waterLook_ = std::clamp(options.waterLook, 0, 1);
    skyReflect_ = std::clamp(options.skyReflect, 0, 1);
    clouds_ = std::clamp(options.clouds, 0, 2);
    dayCycle_ = std::clamp(options.hour, 0, 10);
    gameHour_ = options.gameHour;
    moonLight_ = std::clamp(options.moonLight, 0, 1);
    sunSize_ = std::clamp(options.sunDisc, 0, 2);
    glare_ = std::clamp(options.glare, 0, 1);
    shafts_ = std::clamp(options.shafts, 0, 1);
    spellsMenu_.load("content/fx/spell-looks.json", "content/fx.sprites.json");
    if (!starSky_.load("content/sky/stars.json")) logWarn("stars: content/sky/stars.json missing: the star look 'real' has no stars");
    layers_ = options.layers;
    data_ = e2::BoardData::load(options.boardData);
    std::optional<e2::MapDraft> draft = options.draftData ? options.draftData : e2::MapDraft::load(options.map);
    if (!data_ || !draft) {
      failed_ = true;
      return;
    }
    board_ = std::make_unique<e2::Board>(*data_, std::move(*draft));
    if (!board_->loadRelief(options.engine2Public) || !ground_.build(*board_)) {
      failed_ = true;
      return;
    }
    const BoardGround::Stats s = ground_.stats();
    logInfo("map {} '{}': {}x{} hexes, smooth {} triangles, hard {} triangles, {} baked layers, {} textures, {:.1f} MB geometry",
            board_->draft().id, board_->draft().title, board_->layout().cols, board_->layout().rows, s.triangles[0], s.triangles[1],
            s.layers, s.textures, s.geometryBytes / 1048576.0);
    // Engine2's CameraRig.frame: centre on the board, far enough back to see it whole.
    const e2::Board::Bounds b = board_->bounds();
    double avgY = 0;
    for (const e2::Cell& c : board_->cells()) avgY += c.groundY;
    avgY /= static_cast<double>(board_->cells().size());
    target_ = {static_cast<float>((b.x0 + b.x1) / 2), static_cast<float>(avgY), static_cast<float>((b.z0 + b.z1) / 2)};
    meanGround_ = static_cast<float>(avgY);
    // Atmosfera volumétrica v1: the regional profiles; the map's own by its key (the first profile naming a piece of
    // it), else the default.
    atmosProfiles_ = Volumetrics::loadProfiles("content/atmosphere/v1/profiles.json");
    if (atmosProfiles_.empty()) atmosProfiles_.push_back(Volumetrics::Profile{});
    // The climate the map itself carries (the Map Editor's "Clima" field) wins over the key's.
    if (!board_->draft().atmosphereProfile.empty())
      for (size_t i = 0; i < atmosProfiles_.size(); ++i)
        if (atmosProfiles_[i].name == board_->draft().atmosphereProfile) climaChoice_ = static_cast<int>(i);
    for (size_t i = 0; i < atmosProfiles_.size() && climaChoice_ == 0; ++i)
      for (const std::string& piece : atmosProfiles_[i].maps)
        if (!piece.empty() && board_->draft().id.find(piece) != std::string::npos) {
          climaChoice_ = static_cast<int>(i);
          break;
        }
    renderer_.volumetrics().setProfile(atmosProfiles_[static_cast<size_t>(climaChoice_)], 0.0f);
    renderer_.volumetrics().invalidateHistory();
    boardWidth_ = static_cast<float>(b.x1 - b.x0);
    boardDepth_ = static_cast<float>(b.z1 - b.z0);
    // The sun's shadow covers the board's bounding sphere (ground from y = 0 to its highest cell).
    double top = 0, low = 1e9;
    for (const e2::Cell& c : board_->cells()) top = std::max(top, c.groundY), low = std::min(low, c.groundY);
    boardTop_ = static_cast<float>(top);
    groundLow_ = static_cast<float>(low);
    boardCenter_ = {target_.x, static_cast<float>(top * 0.5), target_.z};
    boardRadius_ = 0.5f * std::sqrt(boardWidth_ * boardWidth_ + boardDepth_ * boardDepth_ + static_cast<float>(top * top)) + 1.0f;
    target_.x += options.offset.x * boardWidth_;
    target_.z += options.offset.y * boardDepth_;
    if (options.fog == 1 || (options.fog < 0 && board_->draft().fog)) fog_.emplace(*board_);

    if (!options.scene.empty() || options.sceneData) {
      scene_ = options.sceneData ? options.sceneData : e2::SceneData::load(options.scene);
      if (!scene_) {
        failed_ = true;
        return;
      }
      // A map placing EmberEngine's own 3D props (content/models, ids "ember-*"): export_scene.ts knows only Engine2's ids and
      // leaves them out, so its scene is built here by SceneBuilder (the same as export_scene.ts's on Engine2's maps: Cooker
      // check-scene-builder). Every other map keeps its exported scene.
      if (!options.sceneData && hasEmberProps()) {
        std::map<std::string, int> skipped;
        scene_ = e2::buildScene(*board_, architectureCatalog(), &skipped);
        logInfo("scene {}: built by SceneBuilder (EmberEngine props); {} kinds left out", options.scene.generic_string(), skipped.size());
      }
      sprites_.start();
      // 3D models: Engine2's GLBs, cooked by Cooker meshes ("/game/models/x.glb" -> "meshes/models/x.mesh").
      for (const e2::SceneModel& m : scene_->models) {
        std::string asset = m.file.substr(std::string("/game/").size());
        asset = "meshes/" + asset.substr(0, asset.size() - 4) + ".mesh";
        if (!modelMeshes_.contains(asset)) modelMeshes_[asset] = meshes_.add(assetId(asset));
      }
      buildArchitecture();
      logInfo("scene {}: {} cards, {} models, {} units, {} architecture pieces ({} built)", scene_->map, scene_->cards.size(),
              scene_->models.size(), scene_->units.size(), scene_->architecture, arch_.size());
      arrangeScene();
      if (options_.partyRoster)  // the campaign's party: the heroes not recruited leave, a freed hero is no longer seated as a neutral
        std::erase_if(scene_->units, [&](const e2::SceneUnit& u) {
          return (u.side == "player" && !options_.partyRoster->contains(u.name)) || (u.side == "neutral" && options_.partyRoster->contains(u.name));
        });
      initUnits();
      setUpParty();
      if (!options.heroes.empty()) setUpPoses(options.heroes);
      if (!options.battle.empty()) setUpBattle(options.battle);
      pinUnitArt();
      preloadHud();
      if (std::string own = e2::emberBackdrop(board_->draft().id); !own.empty()) scene_->backdrop = std::move(own);  // the user's art
      if (!scene_->backdrop.empty()) ground_.requestTexture(e2::backdropAsset(scene_->backdrop));
      // A map with a painting opens with it aligned to the sky (4; the user's "si por favor", 2026-10-19); "imagem até o horizonte"
      // (3) stays an experiment in the panel ("you will need some smarter edition for this to work"); without a painting, the sky.
      // Every other map opens on the real sky (the user, 2026-10-19: "joga o ceu em todos eu vou ir resolvendo um por um em termos de
      // fundo, chão etc"; the cover-fit painting made the board look like it hung in limbo when the camera tilted up); the painting
      // stays in the panel's Fundo.
      // Ashen Forest Crossing keeps its own painting (the user's background v02, 2026-10-19: "ashen forest background is missing"):
      // the cover-fit painting, as it opened before the sky became the default elsewhere.
      // Every map with a painting opens with it just below the real sky's horizon (the user, 2026-10-20: "activate the background image
      // for O Vau just below the sky horizon, you should do that for all background images, dont touch Aldeia"); a room (indoor) keeps the
      // sky it had. Aldeia is untouched: its own panoramas below.
      // Every map with fog of war opens on the sky (the user, 2026-10-20, Ashen Forest Crossing: "qualquer lugar com esse nevoa" as in
      // the photo: the dense fog over the sky, as on 2026-10-19 21:49; a painting showed through the fog). The fog itself is untouched;
      // maps without fog unchanged; the panel's Fundo still offers the painting.
      background_ = options.background >= 0 ? std::min(options.background, 4)
                    : fog_.has_value()                                                              ? 1
                    : !scene_->backdrop.empty() && (aldeia() || scene_->environment != "indoor") ? 4
                                                                                                  : 1;
      if (!scene_->backdrop.empty()) backdropFit_ = loadBackdropFit(scene_->backdrop);
      if (!backdropFit_.ground.empty()) ground_.requestTexture(e2::backdropAsset(backdropFit_.ground));
      // The map's own panoramas (backdrop-fit.json "panoramas" by map id; the panel's Fundo after its five entries): aldeia only.
      panoramas_ = loadPanoramas(board_->draft().id);
      for (const std::string& file : panoramas_) {
        ground_.requestTexture(e2::backdropAsset(file));
        panoramaFits_.push_back(loadBackdropFit(file));
        if (!panoramaFits_.back().ground.empty()) ground_.requestTexture(e2::backdropAsset(panoramaFits_.back().ground));
      }
      // The newest panorama opens first on its map (aldeia: the user's fire panorama, 2026-10-19 "TERMINA").
      if (options.background < 0 && !panoramas_.empty()) background_ = 4 + static_cast<int>(panoramas_.size());
      if (scene_->environment != "indoor")  // the map's night, or any night of the 24 h cycle
        for (const char* phase : e2::kMoonPhases) ground_.requestTexture(e2::skyMoonAsset(phase));  // 0.1 MB each
    }
  }

  ~MapStage() override {
    if (fogBusy_) jobs_.wait(fogJob_);  // the sight job uses this stage's board and fog
  }

  // Where a slow frame's stage time went: laps named in frame(), logged when the whole passes 50 ms (the hunt for the rare
  // 140-220 ms hitch, 2026-10-19).
  struct FrameLaps {
    using Clock = std::chrono::steady_clock;
    Clock::time_point start = Clock::now(), last = start;
    std::array<std::pair<const char*, double>, 24> laps{};
    size_t count = 0;
    void lap(const char* name) {
      const Clock::time_point now = Clock::now();
      if (count < laps.size()) laps[count++] = {name, std::chrono::duration<double, std::milli>(now - last).count()};
      last = now;
    }
    ~FrameLaps() {
      const double total = std::chrono::duration<double, std::milli>(Clock::now() - start).count();
      if (total <= 50.0) return;
      std::string parts;
      for (size_t i = 0; i < count; ++i)
        if (laps[i].second >= 1.0) parts += std::format("{} {:.1f}, ", laps[i].first, laps[i].second);
      logWarn("slow stage frame: {:.1f} ms = {}rest {:.1f}", total, parts,
              std::chrono::duration<double, std::milli>(Clock::now() - last).count());
    }
  };

  void frame(Renderer& renderer, VkExtent2D size) override {
    FrameLaps laps;
    if (hudPreload_) {
      hudPreloaded_ = hudPreload_->update();
      if (hudPreloaded_.first >= hudPreloaded_.second) hudPreload_.reset();
    }
    ground_.update();
    laps.lap("ground");
    if (scene_) {
      sprites_.update();
      laps.lap("sprites");
      meshes_.update();
      laps.lap("meshes");
      if (!modelsPlaced_ && meshes_.ready()) placeModels();
    }
    uploads_.flush();
    laps.lap("uploads");
    const float time = options_.sceneTime >= 0.0f ? options_.sceneTime
                                                 : std::chrono::duration<float>(std::chrono::steady_clock::now() - start_).count();
    updateFog(time);
    laps.lap("fog");
    const float aspect = static_cast<float>(size.width) / static_cast<float>(size.height);
    if (distance_ <= 0.0f) {
      const float tanHalf = std::tan(glm::radians(kFov * 0.5f));
      // One camera rule for every map (the user, 2026-10-19: "esse aqui tem que ser o máximo de zoom out do jogo em TODOS os
      // mapas"): the farthest zoom is aldeia's (its 30-column board x 2.2), the same distance whatever the board's size.
      // ... in the board's own proportion (the user, 2026-10-20, on O Vau: "não pode deixar dar tanto zoom out nos mapas, TODOS"): the same distance
      // for a small board left it a dot on the screen. The farthest zoom is 2.2 x the distance that frames the whole board (aldeia's
      // 112.42), never beyond aldeia's.
      const float fitDistance = std::max(boardWidth_ / (2 * tanHalf * aspect), boardDepth_ * 1.35f) * 1.05f;
      maxDistance_ = std::min(kMaxDistance, std::max(2.2f * fitDistance, kMinDistance * 4.0f));
      distance_ = std::min(maxDistance_, fitDistance) / options_.zoom;
      // Every map opens at the same zoom, over the party (the user, 2026-10-19: "esse deve ser o zoom padrão em que todos os mapas
      // se iniciam com a camera em cima do grupo, caso estiverem separados na maior concentração de membros"). Captures that frame
      // the board themselves (--offset) keep the whole-board view.
      if (options_.offset.x == 0.0f && options_.offset.y == 0.0f)
        if (const std::optional<glm::vec3> focus = partyFocus()) {
          target_ = *focus;
          distance_ = std::clamp(kStartDistance / options_.zoom, kMinDistance, maxDistance_);
          clampGoal();
        }
    }
    // Measurement: a full turn and two zooms in and out (to 35% of the framing distance) over the run.
    float yaw = yaw_, distance = distance_;
    if (measuring_) {
      const float t = std::chrono::duration<float>(std::chrono::steady_clock::now() - measureStart_).count();
      const float k = std::min(t / options_.measureSeconds, 1.0f);
      yaw = yaw_ + k * 2.0f * std::numbers::pi_v<float>;
      distance = distance_ * glm::mix(0.35f, 1.0f, 0.5f + 0.5f * std::cos(k * 4.0f * std::numbers::pi_v<float>));
      if (t >= options_.measureSeconds) finishMeasure();
    }
    // The orbit camera: yaw_ / pitch_ / distance_ / target_ are where the user sends it; cam_ follows them with a
    // critically damped glide every frame, so every input (drag, wheel, keys, the bar) moves without a jump.
    const float dt = std::min(std::chrono::duration<float>(std::chrono::steady_clock::now() - lastFrame_).count(), 0.1f);
    lastFrame_ = std::chrono::steady_clock::now();
    aspect_ = aspect;
    if (!captured_) {
      captured_ = true, homeYaw_ = yaw_, homePitch_ = pitch_, homeTarget_ = target_, homeDistance_ = distance_;
      cam_ = {target_, yaw_, pitch_, distance_};
    }
    if (!measuring_) panKeys(dt);
    updatePanHold();
    now_ = time;
    laps.lap("camera");
    updateUnits(dt);
    laps.lap("units");
    updateExchange();
    laps.lap("exchange");
    updateQuestPickups();
    if (talkAfterWalk_ >= 0 && !busy()) talkRequest_ = std::exchange(talkAfterWalk_, -1);  // walked up to the NPC: they talk
    runScript();
    laps.lap("script");
    updateRoaming();
    laps.lap("roaming");
    updateTurns();
    laps.lap("turns");
    updateHover();
    laps.lap("hover");
    clampGoal();
    if (measuring_) {
      cam_ = {target_, yaw, pitch_, distance};  // measurements and captures: exactly the asked view
    } else {
      const float k = 1.0f - std::exp(-dt * kGlide);
      cam_.target += (target_ - cam_.target) * k;
      cam_.yaw += (yaw_ - cam_.yaw) * k;
      cam_.pitch += (pitch_ - cam_.pitch) * k;
      cam_.distance *= std::pow(distance_ / cam_.distance, k);  // zoom glides evenly in scale
    }
    // The ground under the pivot (the camera always turns around a point on the board), and the eye never inside
    // the ground or a 3D piece: the tilt rises just enough to clear it.
    cam_.target.y = pivotHeight(cam_.target);
    for (int i = 0; i < 8 && !flat_; ++i) {
      const glm::vec3 eye = eyeOf(eyeView(cam_));
      const float floor = clearanceAt(eye.x, eye.z) + kEyeClearance;
      if (eye.y >= floor) break;
      const float need = std::asin(std::clamp((floor - cam_.target.y) / cam_.distance, -1.0f, 1.0f));
      cam_.pitch = std::min(std::max(cam_.pitch, need + 0.002f), kMaxPitch);
      if (pitch_ < cam_.pitch) pitch_ = cam_.pitch;  // the goal does not pull it back into the ground
    }
    // Tática: straight down from far away through the long lens, framing what the normal lens frames at this distance;
    // the top of the screen is the board's far side (-z). Its near plane hugs the board (Engine2: distance - 200).
    const float flatDistance = cam_.distance * std::tan(glm::radians(kFov * 0.5f)) / std::tan(glm::radians(kFlatFov * 0.5f));
    const Camera camera = flat_ ? Camera::lookAt(cam_.target + glm::vec3(0.0f, flatDistance, 0.0f), cam_.target, glm::radians(kFlatFov), aspect,
                                                 std::max(kNear, flatDistance - 200.0f), {0.0f, 0.0f, -1.0f})
                                : Camera::lookAt(eyeOf(eyeView(cam_)), lookTarget(cam_), glm::radians(kFov), aspect, kNear);
    lastCamera_ = camera;
    drawList_.clear();
    frontCards_.clear();
    laps.lap("goal+eye");
    if (scene_ && sprites_.ready()) buildCards(camera, time);
    laps.lap("cards");
    renderer.setScene(camera, drawList_);
    renderer.setFrontCards(frontCards_);
    lights_.clear();
    if (scene_) buildLights(time);
    laps.lap("lights");
    effectList_.clear();
    fxLights_.clear();
    if (scene_ && sprites_.ready()) {
      runFxTest(time);
      syncZones();
      syncWebs();
      syncVigils();
      syncMirrors();
      syncStatus();
      // Numbers held back for an effect still in the air (a Meteoro's fall).
      for (size_t i = 0; i < pendingFx_.size();) {
        if (time >= pendingFx_[i].first) showEffects(pendingFx_[i].second), pendingFx_.erase(pendingFx_.begin() + static_cast<std::ptrdiff_t>(i));
        else ++i;
      }
      // Tática: the effects lie face up just above every flat card (cards: boardTop + z x the row step).
      spellFx_.build(time, camera, flat_, boardTop_ + 0.2f, effectList_, fxLights_);
    }
    const size_t spellLightsFrom = lights_.size();  // the effects' and the casters' lights: the brightest casts the spell shadow
    for (const SpellEffects::Light& l : fxLights_)
      lights_.push_back(engine2Light(offCard(l.at, camera.forward), l.color, l.intensity * kE2Strength, std::hypot(l.radius, l.height) * 2.5f, 1.25f));
    // Engine2 Battle3D syncCastLight: a point of light at a caster's hands while it casts (Malrec: blue on every spell, yellow on
    // his attacks and counters). It stands 0.6 of the card up and 0.45 in front of the card, toward the camera, so it lights the
    // caster himself (the user, 2026-10-18: "a luz tem que ser em MALREC não no chão"; inside the card's own plane it lit only the
    // ground). In at 5 / s, out at 2.5 / s, Engine2's flicker.
    for (size_t i = 0; i < glows_.size();) {
      const UnitGlow& g = glows_[i];
      if (time > g.end + 0.4f || g.ui >= unitCard_.size()) {
        glows_.erase(glows_.begin() + static_cast<std::ptrdiff_t>(i));
        continue;
      }
      const CardRect& c = unitCard_[g.ui];
      if (c.drawn) {
        const float glow = std::clamp((time - g.start) * 5.0f, 0.0f, 1.0f) * std::clamp(1.0f - (time - g.end) * 2.5f, 0.0f, 1.0f);
        const glm::vec3 hands = c.origin + cardUp(c) * (0.6f * c.height) - camera.forward * 0.45f;
        const float flick = 0.92f + std::sin(time * 1000.0f / 70.0f) * 0.08f;
        lights_.push_back(engine2Light(hands, g.color, kCastLight * glow * flick, kCastLightReach, kCastLightDecay));
      }
      ++i;
    }
    // Every spell casts shadows from its light (the user, 2026-10-18): the brightest of this frame's effect and casting lights draws
    // the cards into its own shadow map (Renderer::setLocalShadow); the ground and the 3D pieces darken behind every card.
    {
      size_t best = lights_.size();
      float bestFlux = 0.0f;
      for (size_t k = spellLightsFrom; k < lights_.size(); ++k) {
        const glm::vec3& c = lights_[k].color;
        const float flux = 0.2126f * c.r + 0.7152f * c.g + 0.0722f * c.b;
        if (flux > bestFlux) best = k, bestFlux = flux;
      }
      if (best < lights_.size() && board_ && !flat_) {
        const glm::vec3 at = lights_[best].position;
        renderer.setLocalShadow(at, lights_[best].range, static_cast<float>(e2::groundHeightAt(*board_, look_, at.x, at.z)));
      } else {
        renderer.setLocalShadow(std::nullopt, 0.0f, 0.0f);
      }
    }
    // Engine2 UNIT_LIGHT_DEFS: units that carry their own light (the familiars, the swamp calf), on the body, walking with it.
    for (size_t i = 0; scene_ && i < scene_->units.size() && i < unitCard_.size(); ++i) {
      const auto def = kUnitLights.find(scene_->units[i].classId);
      if (def == kUnitLights.end() || !unitCard_[i].drawn) continue;
      if (unitBattle_[i] >= 0 && !battle_->units()[static_cast<size_t>(unitBattle_[i])].alive) continue;
      const UnitLightDef& d = def->second;
      const glm::vec3 feet = unitWorld(i);
      const float flick = flicker(time, static_cast<float>(i) * 1.7f, d.flicker);
      // Engine2 CarriedLight: strength x 3.2, decay 1.25, cut off at hypot(radius, 1) x 2.5, one unit above the feet.
      for (const glm::vec3& o : unitLightSpots(i, d))  // (x, z, radius) around the feet
        lights_.push_back(engine2Light(feet + glm::vec3(o.x, 1.0f, o.y), d.color, d.intensity * kE2Strength * flick, std::hypot(o.z, 1.0f) * 2.5f, 1.25f));
    }
    laps.lap("effects");
    renderer.setEffects(effectList_);
    renderer.setVfx(spellFx_.vfx());
    renderer.setLights(lights_);
    laps.lap("hand-over");
    Renderer::Lighting lighting = mapLighting(time);
    // aldeia's sky at the end of the day (the user, 2026-10-19: "ajuste a cor do azul no céu, até as nuvens reagem, menos o céu"):
    // exposed for the board, the blue stayed as light as at noon while the clouds took the sunset's colour. As the sun sinks under
    // 15 deg the sky behind the board (and its reflection) darkens into a dusk blue, 1.5 stops at 2 deg; the glow by the sun stays
    // bright, the board's light is unchanged. Every other map unchanged.
    if (lightingMode_ == 3 && aldeia() && !moonNight_) {
      const float elevation = glm::degrees(std::asin(std::clamp(lighting.sunDirection.y, -1.0f, 1.0f)));
      lighting.skyScale *= glm::mix(0.35f, 1.0f, glm::smoothstep(2.0f, 15.0f, elevation));
    }
    if (lightingMode_ == 3) {
      // The physical camera (ARCHITECTURE 7.6 step A): EV100 from the light metered on the board (incident meter:
      // EV100 = log2(E / 2.5)), darker at night by up to 2.5 stops so night reads as night, and the eye adapting to
      // it (about a second); exposure = 1 / (1.2 x 2^EV) as a camera's.
      const float ev = std::log2(std::max(meterLux_, 1e-4f) / 2.5f);
      // "Luar: cinema": the night stops are capped at one, or the camera would take back what the brighter moon gave
      // (measured 2026-10-19: with the full darkening the moonlit ground stayed black beside the lanterns).
      float darkening = nightDarkening(ev);
      // "Luar: cinema": capped at 2 stops (at 1, measured 2026-10-19 on cemiterio: meter 4.4 lux, exposure 0.24, the night came out
      // as an overcast day with a pale blue sky; at 2 the sky is navy and the moonlit ground still reads).
      if (moonNight_ && moonLight_ == 1) darkening = std::min(darkening, 2.0f);
      const float target = ev + darkening;
      if (std::isnan(adaptedEv_) || measuring_) adaptedEv_ = target;
      adaptedEv_ += (target - adaptedEv_) * (1.0f - std::exp(-dt * kAdaptPerSecond));
      lighting.exposure = 1.0f / (1.2f * std::exp2(adaptedEv_));
      if (readyFrames_ == 30)  // once, for the record of a capture
        logInfo("camera: meter {:.3f} lux, EV {:.2f}, darkening {:.2f} stops, exposure {:.5f}, key {:.2f} lux at elevation {:.1f} deg",
                meterLux_, ev, darkening, lighting.exposure, lighting.sunIntensity, glm::degrees(std::asin(std::clamp(lighting.sunDirection.y, -1.0f, 1.0f))));
      lighting.litScale = std::max(meterLux_, 1e-4f) * 1.2f / 2.5f;  // exposure x this = 1 at the metered EV
    }
    exposure_ = lighting.exposure;
    if (options_.sunElevation >= 0.0f) {
      const float az = glm::radians(options_.sunAzimuth), el = glm::radians(options_.sunElevation);
      lighting.sunDirection = {std::cos(el) * std::cos(az), std::sin(el), std::cos(el) * std::sin(az)};
    }
    // The sun's disc (panel "Sol"): the real 0.26 deg radius, or the moon's small disc with a corona (the user, 2026-10-19:
    // "e o sol?": the real one is a dot nobody finds).
    // Look 001 (2026-10-19, "um sol digno de um triple A", "eu quero uma versão com tudo"): the cinema disc the user already had
    // (the moon's small size), now with limb darkening, a corona and the air's aureole (atmosphere.hlsli sunLook001); at night
    // the look adds a halo round the moon.
    if (lighting.sunDisc) lighting.sunRadius = sunSize_ >= 1 ? glm::radians(kMoonSizes[0]) : 0.0f;
    lighting.sunLook = sunSize_ == 2 ? 1u : 0u;
    // Tática looks from far above: no air between the eye and the board (Engine2 drops its distance fog there).
    if (flat_) lighting.kmPerUnit = 0.0f;
    renderer.setMeshes(scene_ ? meshes_.view() : MeshSceneView{}, lighting);
    renderer.setTerrain(ground_.view(look_));
    {
      // The battle grid: the old marks (panel "Grade" antiga) or a numbered look (looks/grid); the editor's rings
      // always go through the marks.
      const int cols = board_ ? board_->layout().cols : 0, rows = board_ ? board_->layout().rows : 0;
      groundMarks_.clear();
      if (gridLook_ == 1) collectMarks(groundMarks_);
      groundMarks_.insert(groundMarks_.end(), editorMarks_.begin(), editorMarks_.end());
      renderer.setGroundMarks(groundMarks_, cols, rows);
      gridCells_.clear();
      if (gridLook_ >= 2) collectGrid(gridCells_);
      renderer.setGrid(gridCells_, cols, rows, gridLook_ >= 2 ? static_cast<uint32_t>(gridLook_ - 1) : 0u, now_);
    }
    {
      WaterView water = ground_.water(look_);
      water.look = static_cast<uint32_t>(waterLook_);  // panel "Água": 0 the original, 1 ember (look 001)
      renderer.setWater(water);
      // Panel "Água 3D", always drawn: 0 the original (Engine2's ThreeWater: shader look 1), 1 rio (the board's original
      // material on the sheet: 0), 2 rio ember (look 001 on the sheet: 2).
      Water3DView water3d = ground_.water3d(look_);
      static constexpr uint32_t kWater3dShader[3] = {1u, 0u, 2u};
      water3d.look = kWater3dShader[std::clamp(water3dLook_, 0, 2)];
      renderer.setWater3d(water3d);
    }
    // The sky's reflection map and the cloud sheet (panel "Reflexo do céu", "Nuvens"); the sheet sits well above the board.
    renderer.setSkyReflection(skyReflect_ == 1);
    {
      Renderer::Clouds clouds;
      clouds.enabled = clouds_ >= 1;
      clouds.volumetric = clouds_ == 2;  // look 001: the real 3D layer (cloudsvol.hlsli)
      if (clouds.volumetric) {
        // Bigger clouds with wider clearings than the sheets' (a layer seen at the game's low angle closes up otherwise); the
        // sheets ("básica") keep their own numbers.
        clouds.coverage = 0.32f;
        clouds.scale = 420.0f;
      }
      clouds.altitude += meanGround_;  // the sheet's height above the board's mean ground
      clouds.base += meanGround_;
      clouds.top += meanGround_;
      renderer.setClouds(clouds);
      // The real night sky (render/StarSky.h): the catalogue turned to this hour's sidereal time (the cycle's hour, or 22 h on a
      // map whose own time is night, 12 h by day: the stars then hide behind the daylight anyway).
      if (starSky_.loaded() && starLook_ == 4 && !starsOff_) {
        const bool mapNight = scene_ && e2::timeOfDay(scene_->timeOfDay).moon;
        const float hour = cycleOn() ? cycleHour(now_) : mapNight ? 22.0f + std::fmod(now_ / 600.0f, 2.0f) : 12.0f;
        const float lst = starSky_.siderealHours(hour);
        const std::vector<GpuStar> stars = starSky_.gpuStars(lst);
        glm::vec4 clouds4[2]{};
        const auto& mc = starSky_.clouds();
        for (size_t k = 0; k < std::min<size_t>(2, mc.size()); ++k)
          clouds4[k] = glm::vec4(starSky_.worldDirection(mc[k].ra, mc[k].dec, lst), glm::radians(mc[k].radiusDegrees));
        renderer.setStars(stars, starSky_.galacticPole(lst), starSky_.galacticCentre(lst), 1.0f, clouds4);
      } else {
        const glm::vec4 none[2]{};
        renderer.setStars({}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, 0.0f, none);
      }
      // Only against the real sky (Fundo = céu): with a painting behind the board the shader took the painting's own fire for
      // the sun and washed the whole scene in glare (the user, 2026-10-19: "super bright and sun on the sunset?").
      Renderer::SunGlare glare;
      glare.flare = glare_ == 1 && background_ == 1;
      glare.shafts = shafts_ == 1 && background_ == 1;
      renderer.setSunGlare(glare);
    }
    renderer.setSunShadowBounds(boardCenter_, boardRadius_);
    renderer.setLightingLayers(layers_);
    {
      uint32_t bg = kNoTextureIndex;  // the sky (or the clear colour)
      if (background_ == 2) bg = Renderer::kBlackBackground;
      const bool painting = background_ != 1 && background_ != 2 && scene_ && !scene_->backdrop.empty();
      if (painting) bg = ground_.textureIndex(e2::backdropAsset(scene_->backdrop));
      const int panorama = background_ - 5;  // 5.. = the map's own panoramas, aligned to the sky like 4, keeping their own sky
      const bool isPanorama = panorama >= 0 && panorama < static_cast<int>(panoramas_.size());
      if (isPanorama) bg = ground_.textureIndex(e2::backdropAsset(panoramas_[static_cast<size_t>(panorama)]));
      if (painting && background_ >= 4) {
        // The painting aligned to the sky (backdrop-fit.json): its own sun glow turned under the real key light.
        const BackdropFit& fit = isPanorama ? panoramaFits_[static_cast<size_t>(panorama)] : backdropFit_;
        const float sunAz = std::atan2(lighting.sunDirection.z, lighting.sunDirection.x);
        const float centre = sunAz - (fit.glowColumn - 0.5f) * glm::radians(fit.spanDegrees);
        // The ground around the board: the map's top-down ground image (backdrop-fit.json "ground") on a flat plane a little under
        // its lowest ground, the painting beyond `groundFar`; without one, the painting all the way down.
        Renderer::BackdropPlane plane;
        // At the board's own base, so the board stands on it (the user, 2026-10-19: "o tabuleiro tem que encostar na imagem do chão,
        // no momento parece que ele flutua"; it was 1.5 under the lowest ground): the hard / tabletop walls end at min(0, lowest
        // ground) (GroundMesh), the smooth look's cut edge drops kEdgeLip (0.35) under its ground.
        plane.y = look_ == e2::EdgeStyle::Smooth ? groundLow_ - 0.35f : std::min(0.0f, groundLow_);
        if (!fit.ground.empty()) plane.texture = ground_.textureIndex(e2::backdropAsset(fit.ground));
        plane.tile = fit.groundTile;
        plane.from = fit.groundFromDegrees;
        // Tática looks straight down from far away: the ground image is the whole view ("desse ângulo é essa imagem e nada mais").
        plane.far = flat_ ? 1.0e9f : fit.groundFar;
        // "Céu do panorama" (the user, 2026-10-19: "libera o céu para eu ver como fica"): 0 the image's own smoke sky, 1 the real sky
        // above the rooftops (everything above 30% of the image is sky; down to the ruins its sky is matted out by colour).
        const bool ownSky = isPanorama && fit.keepSky && panoramaSky_ == 0;
        const float skyTop = isPanorama && fit.keepSky && panoramaSky_ == 1 ? 0.30f : fit.skyTop;
        // Every map but Aldeia: the regular painting from its horizon row down, the real sky above (mode 5), with the fit's row or the default one.
        const bool whole = fit.whole || (!aldeia() && !isPanorama);
        renderer.setBackground(bg, whole ? 5u : fit.alphaSky ? 4u : ownSky ? 3u : 2u, {fit.horizon, fit.bottom, fit.degreesPerHeight, fit.bottomDegrees},
                               {centre, skyTop, fit.spanDegrees, fit.horizonDegrees}, plane);
      } else {
        renderer.setBackground(bg, background_ == 3 ? 1u : 0u);
      }
    }
    renderer.setFogOfWar(fogReady_ && !noFog_ ? &fogMask_ : nullptr, maskVersion_, &fogTexels_);
    Renderer::VolumetricFog volume;
    // The froxel haze belongs to the old lighting (mode 0, where every saved fog look was captured); under the
    // brighter physical light it reads as a milky veil (2026-10-15), so it stays off there.
    volume.enabled = options_.volume && lightingMode_ == 0 && !flat_;
    volume.meanGround = meanGround_;
    // The froxel fog is the air's haze in sight; the fog of war's bank is drawn by the fog bank pass.
    volume.boundaryDensity = 0.0f;
    renderer.setVolumetricFog(volume);
    // Atmosfera volumétrica v1: the panel's choice (Dev Controls' "FX atmosféricos" off = off), the region's profile
    // reached over 1.5 s, Tática's readability scale.
    {
      Volumetrics& atmos = renderer.volumetrics();
      atmos.setEnabled(options_.volume && atmosChoice_ > 0);
      static const Volumetrics::Quality kQualities[4] = {Volumetrics::Quality::High, Volumetrics::Quality::Cinematic, Volumetrics::Quality::High,
                                                          Volumetrics::Quality::Performance};
      atmos.setQuality(kQualities[std::clamp(atmosChoice_, 0, 3)]);
      atmos.setProfile(atmosProfiles_[static_cast<size_t>(std::clamp<int>(climaChoice_, 0, static_cast<int>(atmosProfiles_.size()) - 1))], 1.5f);
      atmos.setScene(meanGround_, flat_);
    }
    // The fog of war's mist: the numbered look on show (render/FogLooks.h; chosen in the Map panel).
    Renderer::FogBank bank;
    bank.enabled = options_.volume && options_.mist && fog_.has_value() && !noFog_;
    bank.look = fogLook_;
    bank.scene.boardCenter = {boardCenter_.x, boardCenter_.z};
    bank.scene.boardRadius = boardRadius_;
    bank.scene.meanGround = meanGround_;
    bank.scene.groundLow = groundLow_;
    bank.scene.groundHigh = boardTop_;
    const e2::Board::Bounds bb = board_->bounds();
    bank.scene.board = {bb.x0, bb.z0, bb.x1, bb.z1};
    bank.scene.partyCenter = partyCenter();
    renderer.setFogBank(bank);
    renderer.setBloom(bloomOn_ ? (board_->draft().bloomIntensity > 0.0f ? board_->draft().bloomIntensity : 0.16f) : 0.0f);
    renderer.setCardLean(cardStance_ == 1);
    renderer.waitFor(uploads_.timeline(),
                     std::max({ground_.uploadWaitValue(), sprites_.uploadWaitValue(), meshes_.uploadWaitValue()}));
    if (ground_.ready() && sceneReady() && (!fog_ || fogReady_)) ++readyFrames_;
    if (options_.measureSeconds > 0.0f && !measuring_ && !measured_ && readyFrames_ >= 30) {
      measuring_ = true;
      measureStart_ = std::chrono::steady_clock::now();
    }
  }

  void endFrame(double busyMs) override {
    if (!measuring_) return;
    if (++measureFrames_ <= 4) return;  // the GPU result is a few frames old
    double gpu = 0.0;
    for (const auto& r : renderer_.gpuTimer().results()) {
      if (std::string_view(r.name) == "frame") gpu = r.milliseconds;
      auto it = std::find_if(passes_.begin(), passes_.end(), [&](const auto& p) { return p.first == r.name; });
      if (it == passes_.end()) it = passes_.insert(passes_.end(), {std::string(r.name), 0.0});
      it->second += r.milliseconds;
      auto& worst = passWorst_[std::string(r.name)];
      worst.first = std::max(worst.first, r.milliseconds);
      ++worst.second;
    }
    busy_.push_back(busyMs);
    gpu_.push_back(gpu);
    if (fogUpdatedThisFrame_) fogUpdateMs_.push_back(lastFogMs_);
    fogUpdatedThisFrame_ = false;
  }

  // Mouse: right-drag turns and tilts around the pivot, middle-drag grabs the ground and slides the board, the wheel
  // zooms toward the point under the cursor. The left button stays free for the game. Clicks on the panel and the
  // camera bar never reach here (ImGui takes them).
  void event(const SDL_Event& e) override {
    if (e.type == SDL_EVENT_MOUSE_MOTION) mouse_ = {e.motion.x, e.motion.y}, mouseWindow_ = e.motion.windowID;
    if (inputHeld) {  // a conversation or the Inn's menus over the map (NpcTalk.h): nothing reaches the board
      leftDown_.reset(), endPan();
      return;
    }
    // The map editor: a press on a unit or a decoration holds it (MapPreviewCanvas onPointerDownCapture); the camera
    // and the brush never see that press.
    if (options_.editorGrab && e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && (e.button.button == SDL_BUTTON_RIGHT || e.button.button == SDL_BUTTON_LEFT) &&
        !(ImGui::GetCurrentContext() && ImGui::GetIO().WantCaptureMouse)) {
      const bool left = e.button.button == SDL_BUTTON_LEFT;
      if (const std::optional<e2::BattleHex> hex = pick(e.button.windowID, e.button.x, e.button.y); hex && options_.editorGrab(hex->x, hex->y, left)) {
        editorHold_ = EditorHold{e.button.button, e.button.x, e.button.y, false};
        return;
      }
    }
    if (editorHold_) {
      if (e.type == SDL_EVENT_MOUSE_MOTION) {
        if (std::hypot(e.motion.x - editorHold_->x, e.motion.y - editorHold_->y) >= 6.0f) editorHold_->moved = true;
        return;
      }
      if (e.type == SDL_EVENT_MOUSE_BUTTON_UP && e.button.button == editorHold_->button) {
        const std::optional<e2::BattleHex> hex = pick(e.button.windowID, e.button.x, e.button.y);
        const bool moved = editorHold_->moved;
        editorHold_.reset();
        if (options_.editorRelease) options_.editorRelease(hex ? std::optional<std::pair<int, int>>({hex->x, hex->y}) : std::nullopt, moved);
        return;
      }
    }
    // Left button, Engine2's protected rule (cameraRig): held still for 0.5 s it arms panning (the grabbing hand) and
    // the board then follows the drag; a short click (released before, within 5 px) is a game click.
    if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_LEFT)
      leftDown_ = LeftPress{e.button.x, e.button.y, std::chrono::steady_clock::now()};
    if (e.type == SDL_EVENT_MOUSE_BUTTON_UP && e.button.button == SDL_BUTTON_LEFT) {
      const bool wasPanning = panning_;
      endPan();
      if (leftDown_ && !wasPanning && std::hypot(e.button.x - leftDown_->x, e.button.y - leftDown_->y) <= kClickSlop)
        click(e.button.windowID, e.button.x, e.button.y);
      leftDown_.reset();
    }
    if (e.type == SDL_EVENT_MOUSE_MOTION && panning_ && (e.motion.state & SDL_BUTTON_LMASK)) {
      const std::optional<glm::vec3> a = groundUnder(e.motion.windowID, e.motion.x - e.motion.xrel, e.motion.y - e.motion.yrel);
      const std::optional<glm::vec3> b = groundUnder(e.motion.windowID, e.motion.x, e.motion.y);
      if (a && b) target_ += glm::vec3{a->x - b->x, 0.0f, a->z - b->z}, cam_.target += glm::vec3{a->x - b->x, 0.0f, a->z - b->z};
    }
    // F1..F12: the battle bar's sockets (Ctrl+F1 is Modo teste's numbers)
    if (e.type == SDL_EVENT_KEY_DOWN && !e.key.repeat && e.key.scancode >= SDL_SCANCODE_F1 && e.key.scancode <= SDL_SCANCODE_F12 &&
        !(e.key.mod & (SDL_KMOD_CTRL | SDL_KMOD_ALT | SDL_KMOD_SHIFT)))
      pressHotbar(static_cast<int>(e.key.scancode - SDL_SCANCODE_F1));
    if (e.type == SDL_EVENT_KEY_DOWN && !e.key.repeat && e.key.key == SDLK_ESCAPE) {
      if (hotbarEdit_) hotbarEdit_ = false;
      else if (battleInventory_) battleInventory_.reset();
      else if (statusSheet_.isOpen()) statusSheet_.closeConditionOrSheet();
      else if (questLog_) questLog_ = false;
      else if (logFull_) logFull_ = false;
      else if (pendingFoe_ >= 0) pendingFoe_ = -1;
      else if (spellArmed_) spellArmed_.reset();
      else if (!spellMode_.empty()) spellMode_.clear(), spellCells_.clear(), spellAim_.clear();
      else if (offHandMode_) offHandMode_ = false, refreshReach();
      else if (attackMode_) attackMode_ = false;
      else if (selected_ >= 0 && result_.empty()) select(-1);
      else if (!menuOptions_.isOpen()) pauseOpen_ = !pauseOpen_;  // nothing picked: the battle menu
    }
    if (e.type == SDL_EVENT_MOUSE_MOTION && (e.motion.state & SDL_BUTTON_RMASK) && !flat_) {  // Tática never turns
      yaw_ -= e.motion.xrel * kTurnPerPixel;
      pitch_ = std::clamp(pitch_ + e.motion.yrel * kTurnPerPixel, minPitch(), kMaxPitch);
    }
    if (e.type == SDL_EVENT_MOUSE_MOTION && (e.motion.state & SDL_BUTTON_MMASK)) {
      // The ground point under the cursor stays under it (the board follows the hand).
      const std::optional<glm::vec3> a = groundUnder(e.motion.windowID, e.motion.x - e.motion.xrel, e.motion.y - e.motion.yrel);
      const std::optional<glm::vec3> b = groundUnder(e.motion.windowID, e.motion.x, e.motion.y);
      if (a && b) target_ += glm::vec3{a->x - b->x, 0.0f, a->z - b->z}, cam_.target += glm::vec3{a->x - b->x, 0.0f, a->z - b->z};
    }
    if (e.type == SDL_EVENT_MOUSE_WHEEL && e.wheel.y != 0.0f) {
      const float next = std::clamp(distance_ * std::pow(kZoomStep, e.wheel.y), kMinDistance, maxDistance_);
      // Toward the cursor: the pivot moves to the cursor's ground point by the share of the distance zoomed.
      if (const std::optional<glm::vec3> p = groundUnder(e.wheel.windowID, e.wheel.mouse_x, e.wheel.mouse_y)) {
        const float share = 1.0f - next / distance_;
        target_.x += (p->x - target_.x) * share;
        target_.z += (p->z - target_.z) * share;
      }
      distance_ = next;
    }
  }

  // The left button held still long enough arms the pan (called every frame). Engine2's gauntlet shows while the
  // board is held: the armed left button, or the middle button's grab.
  void updatePanHold() {
    if (leftDown_ && !panning_ && std::chrono::steady_clock::now() - leftDown_->at >= std::chrono::milliseconds(kHoldMs)) panning_ = true;
    if (panning_ || (SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON_MMASK)) requestGrabCursor();
  }
  void endPan() { panning_ = false; }

  // Arrows / WASD slide the board along the view (level, whatever the tilt), faster when zoomed out; Shift doubles.
  void panKeys(float dt) {
    if (ImGui::GetCurrentContext() && ImGui::GetIO().WantTextInput) return;  // only typing into a text field takes the keys
    const bool* k = SDL_GetKeyboardState(nullptr);
    if (!k) return;
    const float yaw = flat_ ? 0.0f : yaw_;  // Tática: up on screen is the board's far side
    const glm::vec3 forward{-std::sin(yaw), 0.0f, -std::cos(yaw)}, right{std::cos(yaw), 0.0f, -std::sin(yaw)};
    glm::vec3 move{0.0f};
    if (k[SDL_SCANCODE_W] || k[SDL_SCANCODE_UP]) move += forward;
    if (k[SDL_SCANCODE_S] || k[SDL_SCANCODE_DOWN]) move -= forward;
    if (k[SDL_SCANCODE_D] || k[SDL_SCANCODE_RIGHT]) move += right;
    if (k[SDL_SCANCODE_A] || k[SDL_SCANCODE_LEFT]) move -= right;
    if (glm::length(move) < 1e-4f) return;
    const float fast = k[SDL_SCANCODE_LSHIFT] || k[SDL_SCANCODE_RSHIFT] ? 2.0f : 1.0f;
    target_ += glm::normalize(move) * std::max(distance_, 6.0f) * kPanPerSecond * fast * dt;
  }

  // The board never leaves the screen: the pivot stays on the board, kept in from its edges by 60% of the view's half height at
  // the pivot (the user, 2026-10-19: "não pode deixar sair tanto assim do tabuleiro": with the edge allowed to reach the screen's
  // centre, a corner left three quarters of the screen off the board); the farther the zoom, the nearer the middle (never past
  // it). Tilt and zoom stay in range.
  void clampGoal() {
    pitch_ = std::clamp(pitch_, minPitch(), kMaxPitch);
    distance_ = std::clamp(distance_, kMinDistance, maxDistance_);
    const e2::Board::Bounds b = board_->bounds();
    const float inset = distance_ * std::tan(glm::radians(kFov * 0.5f)) * kPanInset;
    const float halfX = static_cast<float>(b.x1 - b.x0) * 0.5f, halfZ = static_cast<float>(b.z1 - b.z0) * 0.5f;
    const float midX = static_cast<float>(b.x0 + b.x1) * 0.5f, midZ = static_cast<float>(b.z0 + b.z1) * 0.5f;
    const float reachX = std::max(halfX - inset, 0.0f), reachZ = std::max(halfZ - inset, 0.0f);
    target_.x = std::clamp(target_.x, midX - reachX, midX + reachX);
    target_.z = std::clamp(target_.z, midZ - reachZ, midZ + reachZ);
  }
  static constexpr float kPanInset = 0.6f;  // the pivot's distance from the board's edges, in view half heights

  struct OrbitView {
    glm::vec3 target{0.0f};
    float yaw = 0.0f, pitch = 0.0f, distance = 1.0f;
  };
  static glm::vec3 eyeOf(const OrbitView& v) {
    return v.target + glm::vec3{v.distance * std::cos(v.pitch) * std::sin(v.yaw), v.distance * std::sin(v.pitch),
                                v.distance * std::cos(v.pitch) * std::cos(v.yaw)};
  }
  // The view the eye stands at: never under kMinEyePitch (the ground); the tilt below it becomes a lift of the line of sight.
  static OrbitView eyeView(OrbitView v) {
    v.pitch = std::max(v.pitch, kMinEyePitch);
    return v;
  }
  // Where the camera looks: the pivot, or, under kMinEyePitch, a point lifted by the missing tilt about the eye's right axis
  // (the board sinks in the frame, the sky takes the rest).
  static glm::vec3 lookTarget(const OrbitView& v) {
    const float lift = std::max(kMinEyePitch - v.pitch, 0.0f);
    const glm::vec3 eye = eyeOf(eyeView(v));
    const glm::vec3 forward = glm::normalize(v.target - eye);
    if (lift <= 0.0f) return v.target;
    const glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));
    const glm::vec3 up = glm::cross(right, forward);
    return eye + (forward * std::cos(lift) + up * std::sin(lift)) * v.distance;
  }
  float groundAt(float x, float z) const { return static_cast<float>(board_->cellAt(x, z).groundY); }
  // The pivot's height: the ground averaged over about a hex around it, so it glides over steps instead of jumping.
  float pivotHeight(glm::vec3 p) const {
    float sum = 0.0f;
    for (int i = 0; i < 7; ++i) {
      const float a = static_cast<float>(i) * 1.0472f, r = i == 6 ? 0.0f : 1.2f;
      sum += groundAt(p.x + r * std::cos(a), p.z + r * std::sin(a));
    }
    return sum / 7.0f;
  }
  // The highest solid thing at (x, z): the ground on the board, and the top of any 3D piece standing there.
  float clearanceAt(float x, float z) const {
    const e2::Board::Bounds b = board_->bounds();
    float top = x < b.x0 || x > b.x1 || z < b.z0 || z > b.z1 ? groundLow_ : groundAt(x, z);
    for (const PieceBox& p : pieceBoxes_)
      if (std::abs(x - p.x) < p.radius && std::abs(z - p.z) < p.radius) top = std::max(top, p.top);
    return top;
  }
  // The ground point under a window position (window coordinates), on the pivot's level; none above the horizon.
  std::optional<glm::vec3> groundUnder(SDL_WindowID id, float mx, float my) const {
    int w = 0, h = 0;
    SDL_Window* window = SDL_GetWindowFromID(id);
    if (!window || !SDL_GetWindowSize(window, &w, &h) || w <= 0 || h <= 0) return std::nullopt;
    const float tanHalf = std::tan(glm::radians((flat_ ? kFlatFov : kFov) * 0.5f));
    const float nx = 2.0f * mx / static_cast<float>(w) - 1.0f, ny = 1.0f - 2.0f * my / static_cast<float>(h);
    const glm::vec3 up = glm::cross(lastCamera_.right, lastCamera_.forward);
    const glm::vec3 dir = glm::normalize(lastCamera_.forward + lastCamera_.right * (nx * tanHalf * aspect_) + up * (ny * tanHalf));
    if (dir.y > -1e-3f) return std::nullopt;
    const float t = (cam_.target.y - lastCamera_.position.y) / dir.y;
    return lastCamera_.position + dir * t;
  }

  // A dropdown: `count` choices named by name(i); true when the choice changed.
  template <typename Name>
  static bool dropdown(const char* label, int& current, int count, Name name) {
    bool changed = false;
    if (ImGui::BeginCombo(label, name(current).c_str())) {
      for (int i = 0; i < count; ++i) {
        const bool selected = i == current;
        if (ImGui::Selectable(name(i).c_str(), selected)) current = i, changed = true;
        if (selected) ImGui::SetItemDefaultFocus();
      }
      ImGui::EndCombo();
    }
    return changed;
  }

  // The window shows only the choices, in plain Portuguese words, large (user, 2026-10-15: "the only information I
  // want on that screen are the CHOICES, very visible"; no numbers, no statistics). Look numbers stay in the code
  // and the looks/ READMEs.
  // Engine2's ember-plate (src/ember-ui/styles.css): a near-black gradient (rgba 28,28,28,.94 to 10,10,10,.95), a 1 px
  // gold border (190,150,95,.45) inside a 1 px black ring, an 8 px inner shadow, a soft drop shadow 6 px down, 2 px
  // corners; on hover the text turns #ffe0b4, the border (226,170,100,.85) and an orange glow (255,110,40,.3) come on.
  // paint(drawList, min, max, ink) draws the face. True when clicked.
  template <typename Paint>
  static bool plate(const char* id, ImVec2 size, Paint paint) {
    const bool clicked = ImGui::InvisibleButton(id, size);
    const bool hover = ImGui::IsItemHovered();
    const ImVec2 a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    // Drop shadow (0 6px 18px rgba(0,0,0,.6)) and, on hover, the glow (0 0 14px rgba(255,110,40,.3)): blurred by layers.
    for (int i = 6; i >= 1; --i) {
      const float spread = 1.5f * static_cast<float>(i);
      dl->AddRectFilled({a.x - spread, a.y + 6.0f - spread}, {b.x + spread, b.y + 6.0f + spread}, IM_COL32(0, 0, 0, 14), 2.0f + spread);
    }
    if (hover)
      for (int i = 5; i >= 1; --i) {
        const float spread = 1.4f * static_cast<float>(i);
        dl->AddRectFilled({a.x - spread, a.y - spread}, {b.x + spread, b.y + spread}, IM_COL32(255, 110, 40, 12), 2.0f + spread);
      }
    dl->AddRect({a.x - 1.0f, a.y - 1.0f}, {b.x + 1.0f, b.y + 1.0f}, IM_COL32(0, 0, 0, 255), 3.0f);  // 0 0 0 1px #000
    const ImU32 top = IM_COL32(28, 28, 28, 240), bottom = IM_COL32(10, 10, 10, 242);
    dl->AddRectFilledMultiColor(a, b, top, top, bottom, bottom);
    for (int i = 0; i < 4; ++i) {  // inset 0 0 8px rgba(0,0,0,.85)
      const float d = 1.0f + 2.0f * static_cast<float>(i);
      dl->AddRect({a.x + d, a.y + d}, {b.x - d, b.y - d}, IM_COL32(0, 0, 0, 90 - 20 * i), 1.0f, 2.0f);
    }
    dl->AddRect(a, b, hover ? IM_COL32(226, 170, 100, 217) : IM_COL32(190, 150, 95, 115), 2.0f);
    paint(dl, a, b, hover ? IM_COL32(255, 224, 180, 255) : IM_COL32(240, 240, 240, 255));
    return clicked;
  }

  // A lucide icon at 14 px (Engine2: size-3.5), centred on the button.
  static auto barIcon(const char* name) {
    return [name](ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 ink) { uiIcon(dl, name, {(a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f}, 14.0f, ink); };
  }
  // Text with Engine2's 0.14 em letter spacing (tracking-[0.14em]), one glyph at a time.
  static float spacedWidth(const char* text) {
    float w = 0.0f;
    int glyphs = 0;
    for (const char* p = text; *p;) {
      const char* next = p + 1;
      while ((*next & 0xC0) == 0x80) ++next;  // whole UTF-8 sequence
      w += ImGui::CalcTextSize(p, next).x;
      ++glyphs;
      p = next;
    }
    return w + kPlateText * 0.14f * static_cast<float>(std::max(0, glyphs - 1));
  }
  static void drawSpaced(ImDrawList* dl, ImVec2 at, ImU32 ink, const char* text) {
    for (const char* p = text; *p;) {
      const char* next = p + 1;
      while ((*next & 0xC0) == 0x80) ++next;
      dl->AddText(at, ink, p, next);
      at.x += ImGui::CalcTextSize(p, next).x + kPlateText * 0.14f;
      p = next;
    }
  }

  // The camera bar, top right: Engine2's battle HUD (GameApp.tsx), copied 1:1 at the user's request (2026-10-15).
  // Normal / Tática (the 2D view), tilt down / up 5 deg, turn left / right 15 deg, back to the normal angle (leaves
  // Tática; zoom and pan stay, like Engine2's resetAngle). Each click glides.
  void drawCameraBar() {
    const ImGuiIO& io = ImGui::GetIO();
    // The window's padding (room for the shadow) sits outside the 8 px inset.
    ImGui::SetNextWindowPos({io.DisplaySize.x - kBarInset + kPlateGlow, kBarInset - kPlateGlow}, ImGuiCond_Always, {1.0f, 0.0f});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {kPlateGlow, kPlateGlow});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {kBarGap, kBarGap});
    ImGui::Begin("##camerabar", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBackground |
                     ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoSavedSettings);
    ImGui::PushFont(nullptr, kPlateText);
    // Engine2: h-7 px-2, 10 px uppercase, letter spacing 0.14 em.
    const char* label = flat_ ? "TÁTICA" : "NORMAL";
    if (plate("##view", {spacedWidth(label) + 16.0f, kPlate}, [&](ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 ink) {
          const float w = spacedWidth(label);
          drawSpaced(dl, {std::round((a.x + b.x - w) * 0.5f), std::round((a.y + b.y - ImGui::GetTextLineHeight()) * 0.5f)}, ink, label);
        }))
      setFlat(!flat_);
    ImGui::SameLine();
    if (plate("##down", {kPlate, kPlate}, barIcon("chevron-down"))) pitch_ = std::clamp(pitch_ - kTiltStep, minPitch(), kMaxPitch);
    ImGui::SameLine();
    if (plate("##up", {kPlate, kPlate}, barIcon("chevron-up"))) pitch_ = std::clamp(pitch_ + kTiltStep, minPitch(), kMaxPitch);
    ImGui::SameLine();
    if (plate("##left", {kPlate, kPlate}, barIcon("chevron-left"))) yaw_ += kTurnStep;
    ImGui::SameLine();
    if (plate("##right", {kPlate, kPlate}, barIcon("chevron-right"))) yaw_ -= kTurnStep;
    ImGui::SameLine();
    if (plate("##reset", {kPlate, kPlate}, barIcon("rotate-ccw"))) {
      if (flat_) setFlat(false);
      // The shortest way round to the normal turn; zoom and pan stay.
      const float twoPi = 2.0f * std::numbers::pi_v<float>;
      yaw_ = homeYaw_ + twoPi * std::round((yaw_ - homeYaw_) / twoPi);
      pitch_ = homePitch_;
    }
    if (!options_.editorClick) {  // a battle: Engine2's Som and Opções (onPause: the battle's Esc menu) end the bar
      ImGui::SameLine();
      GamePrefs& prefs = gamePrefs();
      if (plate("##sound", {kPlate, kPlate}, barIcon(prefs.muted ? "volume-x" : "volume-2"))) prefs.muted = !prefs.muted, prefs.save();
      ImGui::SameLine();
      const std::string opt = upperUtf8(tr("Opções", "Options"));
      if (plate("##options", {spacedWidth(opt.c_str()) + 16.0f, kPlate}, [&](ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 ink) {
            const float w = spacedWidth(opt.c_str());
            drawSpaced(dl, {std::round((a.x + b.x - w) * 0.5f), std::round((a.y + b.y - ImGui::GetTextLineHeight()) * 0.5f)}, ink, opt.c_str());
          }) &&
          result_.empty() && !menuOptions_.isOpen())
        pauseOpen_ = true;
    }
    ImGui::PopFont();
    ImGui::End();
    ImGui::PopStyleVar(3);
  }

  // Tática on / off (Engine2 DioramaView.setFlat + Battle3D): leaving it lands on the normal view's angle at once;
  // zoom and pan stay where they are.
  void setFlat(bool on) {
    if (on == flat_) return;
    flat_ = on;
    renderer_.volumetrics().invalidateHistory();  // a camera cut: the fog's history would smear across it
    if (on) return;
    const float twoPi = 2.0f * std::numbers::pi_v<float>;
    yaw_ = homeYaw_ + twoPi * std::round((yaw_ - homeYaw_) / twoPi);
    pitch_ = homePitch_;
    cam_.yaw = yaw_, cam_.pitch = pitch_;
  }

  void drawPanel() override {
    if (!board_) return;
    if (hudHidden) return;  // the Inn's menus cover the whole screen (NpcTalk.h)
    if (!result_.empty() && now_ - resultAt_ > kResultDelay) return drawResultScreen();
    if (options_.editorClick) return drawCameraBar();  // the map editor's preview: its own panels around it
    drawAirParticles();
    drawMarks();
    drawCameraBar();
    drawTurnBar();
    drawPauseMenu();
    if (options_.choicesPanel) drawChoices();
    if (options_.spellsMenu && scene_ && menuCaster() >= 0) {
      spellsMenu_.draw([this](const std::string& kind, int look) { testCast(static_cast<size_t>(menuCaster()), kind, look, now_); },
                       [this](const std::string& clip) {
                         const size_t caster = static_cast<size_t>(menuCaster());
                         spellFx_.play(clip, unitSpot(nearestTarget(caster, false)), now_);
                       },
                       options_.spellsMenuOpen);
    }
  }

  // The Escolhas window: the look switches (only in Modo teste's battles; a campaign battle shows only the game).
  void drawChoices() {
    ImGui::SetNextWindowPos({8, 44}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowCollapsed(true, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowBgAlpha(0.9f);
    ImGui::Begin("Escolhas", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing);
    ImGui::PushFont(nullptr, 16.0f);  // 24 px at 1080 (the UI canvas is zoomed: DebugUi::kUiHeight)
    ImGui::PushItemWidth(150.0f);
    if (!poseChoices_.empty())
      dropdown("Animação", poseChoice_, static_cast<int>(poseChoices_.size()), [this](int i) { return poseChoices_[static_cast<size_t>(i)].second; });
    {
      int look = static_cast<int>(look_);
      static const char* kLooks[3] = {"suave", "hexágonos", "mesa"};
      if (dropdown("Chão", look, 3, [](int i) { return std::string(kLooks[i]); })) look_ = static_cast<e2::EdgeStyle>(look);
    }
    {
      // 3 = the painting up to the world's horizon, the real sky above it (the user, 2026-10-19: "a imagem de fundo pode
      // terminar no horizonte onde começa o céu").
      // 4 = the painting aligned to the sky: cut at its horizon, its own sky matted out, turned under the sun (backdrop-fit.json).
      static const char* kBackgrounds[5] = {"imagem do mapa", "céu", "OFF", "imagem até o horizonte", "imagem alinhada ao céu"};
      dropdown("Fundo", background_, 5 + static_cast<int>(panoramas_.size()),
               [](int i) { return i < 5 ? std::string(kBackgrounds[i]) : std::format("panorama {:02}", i - 4); });
      if (background_ >= 5) {
        static const char* kPanoramaSky[2] = {"da imagem", "real"};
        dropdown("Céu do panorama", panoramaSky_, 2, [](int i) { return std::string(kPanoramaSky[i]); });
      }
    }
    {
      // Cards vertical (they converge under the tilted camera: "crooked"), or leaning back with the camera's tilt so
      // every card stands upright on screen (sprites.hlsl VSMain).
      if (lightingMode_ == 3 && !(scene_ && scene_->environment == "indoor")) {
        static const char* kHours[11] = {"do mapa", "ciclo rápido", "ciclo lento", "amanhecer", "manhã", "meio-dia",
                                         "tarde", "pôr do sol", "crepúsculo", "noite", "madrugada"};
        dropdown("Hora", dayCycle_, 11, [](int i) { return std::string(kHours[i]); });
      }
      static const char* kStance[2] = {"em pé", "para a câmera"};
      dropdown("Cards", cardStance_, 2, [](int i) { return std::string(kStance[i]); });
    }
    if (battle_) {
      // The Mago Negro's path (test mode): the party's black mages as Mago Negro, Elementalista or Bruxo, live (the
      // campaign chooses it on the world map's Party screen and brings it in the save).
      for (e2::BattleUnit& u : battle_->units()) {
        if (u.side != "player" || !(u.classId == "mage" || u.classId == "voss" || u.classId == "elementalist" || u.classId == "warlock")) continue;
        static const char* kPaths[3] = {"Mago Negro", "Elementalista", "Bruxo"};
        static const char* kIds[3] = {"mage", "elementalist", "warlock"};
        int path = u.classId == "elementalist" ? 1 : u.classId == "warlock" ? 2 : 0;
        if (dropdown(u.name.c_str(), path, 3, [](int i) { return std::string(kPaths[i]); })) e2::setPromotion(u, kIds[path]);
      }
      // The battle grid: the old marks, or a saved look (looks/grid/README.md).
      static const char* kGrid[3] = {"OFF", "antiga", "001"};
      dropdown("Grade", gridLook_, 3, [](int i) { return std::string(kGrid[i]); });
      // Health bars and numbers of the units near the camera: shown always, or fading away as their card fills the
      // screen (drawHealthBars).
      static const char* kBars[2] = {"OFF", "somem"};
      dropdown("Vida perto", barsFade_, 2, [](int i) { return std::string(kBars[i]); });
    }
    {
      // The board's water: the original (Engine2's diorama material, the default) or look 001 "ember" (looks/water/README.md).
      // Every water stays selectable, the original is the default (the user, 2026-10-19: "NUNCA TROCAR O ASSET PADRÃO").
      static const char* kWater[2] = {"original", "ember"};
      dropdown("Água", waterLook_, 2, [](int i) { return std::string(kWater[i]); });
    }
    if (board_ && board_->draft().water3d.any()) {
      // No OFF: the 3D water is always drawn (the user, 2026-10-19: "só existe água 3d, não pode ficar off").
      static const char* kWater3d[3] = {"original", "rio", "rio ember"};
      dropdown("Água 3D", water3dLook_, 3, [](int i) { return std::string(kWater3d[i]); });
    }
    {
      // The sky's reflection map on every specular surface (water first), and the cloud sheet (sky, reflections, shadows).
      static const char* kReflect[2] = {"OFF", "céu real"};
      dropdown("Reflexo do céu", skyReflect_, 2, [](int i) { return std::string(kReflect[i]); });
      static const char* kClouds[3] = {"OFF", "básica", "volumétricas"};
      dropdown("Nuvens", clouds_, 3, [](int i) { return std::string(kClouds[i]); });
      // The lens's glare round the sun / moon and the crepuscular rays from it (sunglare.hlsl, look 001).
      static const char* kLook[2] = {"OFF", "001"};
      dropdown("Clarão", glare_, 2, [](int i) { return std::string(kLook[i]); });
      dropdown("Raios de luz", shafts_, 2, [](int i) { return std::string(kLook[i]); });
    }
    {
      static const char* kModes[4] = {"protótipo", "Engine2", "física", "real"};
      dropdown("Luz", lightingMode_, 4, [](int i) { return std::string(kModes[i]); });
    }
    {
      int bloom = bloomOn_ ? 1 : 0;
      if (dropdown("Brilho", bloom, 2, [](int i) { return std::string(i == 0 ? "OFF" : "Engine2"); })) bloomOn_ = bloom == 1;
    }
    if (options_.volume) {
      // Atmosfera volumétrica v1 (docs/ATMOSPHERE.md): OFF, or its quality; then the region's profile.
      static const char* kAtmos[4] = {"OFF", "cinema", "alta", "desempenho"};
      dropdown("Atmosfera", atmosChoice_, 4, [](int i) { return std::string(kAtmos[i]); });
      if (atmosChoice_ > 0 && atmosProfiles_.size() > 1)
        dropdown("Clima", climaChoice_, static_cast<int>(atmosProfiles_.size()), [this](int i) { return atmosProfiles_[static_cast<size_t>(i)].label; });
    }
    {
      static const char* kLayers[4] = {"sol / lua", "sombras", "luz do céu", "tochas e fogo"};
      for (uint32_t i = 0; i < 4; ++i) {
        bool on = (layers_ >> i) & 1u;
        if (i) ImGui::SameLine();
        if (ImGui::Checkbox(kLayers[i], &on)) layers_ ^= 1u << i;
      }
    }
    if (scene_ && (!e2::timeOfDay(scene_->timeOfDay).moon || cycleOn()) && scene_->environment != "indoor") {
      static const char* kSun[3] = {"real", "cinema", "001"};
      dropdown("Sol", sunSize_, 3, [](int i) { return std::string(kSun[i]); });
    }
    if (scene_ && (e2::timeOfDay(scene_->timeOfDay).moon || cycleOn())) {
      static const char* kPaths[3] = {"Engine2", "atrás", "na frente"};
      int moon = moonOff_ ? 0 : moonPath_ + 1;
      if (dropdown("Lua", moon, 4, [](int i) { return i == 0 ? std::string("OFF") : std::string(kPaths[i - 1]); })) {
        moonOff_ = moon == 0;
        if (!moonOff_) moonPath_ = moon - 1;
      }
      // moonPhase.ts MOON_PHASE_LABEL, in kMoonPhases order
      static const char* kPhases[9] = {"nova", "crescente", "quarto cresc.", "gibosa cresc.", "cheia",
                                       "gibosa ming.", "quarto ming.", "minguante", "sangue"};
      int phase = moonPhase_ + 1;
      if (dropdown("Fase da lua", phase, 10,
                   [](int i) { return i == 0 ? std::string("padrão") : std::string(kPhases[i - 1]); }))
        moonPhase_ = phase - 1;
      static const char* kSizes[4] = {"pequena", "média", "grande", "enorme"};
      dropdown("Tamanho da lua", moonSize_, 4, [](int i) { return std::string(kSizes[i]); });
      if (lightingMode_ == 3) {
        static const char* kMoonLight[2] = {"físico", "cinema"};
        dropdown("Luar", moonLight_, 2, [](int i) { return std::string(kMoonLight[i]); });
      }
      static const char* kStars[kStarLooks] = {"quadradas", "poucas", "muitas", "real"};
      int star = starsOff_ ? 0 : starLook_;
      if (dropdown("Estrelas", star, kStarLooks + 1, [](int i) { return i == 0 ? std::string("OFF") : std::string(kStars[i - 1]); })) {
        starsOff_ = star == 0;
        if (!starsOff_) starLook_ = star;
      }
    }
    if (fog_ && options_.volume) {
      // First "no fog at all" (no mist and no fog of war: the whole map seen, every unit drawn), then the saved
      // mist looks (looks/fog/README.md, in order).
      static const char* kFog[9] = {"nuvens", "lençol", "bancos", "bloco", "bloco + giro", "mist4", "mist4 + giro", "bancos sem véu", "bancos, céu livre"};
      int look = noFog_ ? 0 : static_cast<int>(fogLook_) + 1;
      const int count = static_cast<int>(fogLooks().size());
      if (dropdown("Névoa", look, count + 1, [count](int i) {
            if (i == 0) return std::string("OFF");
            return count == 9 ? std::string(kFog[i - 1]) : std::string(fogLooks()[i - 1].note);
          })) {
        noFog_ = look == 0;
        if (!noFog_) fogLook_ = static_cast<uint32_t>(look - 1);
      }
    }
    ImGui::PopItemWidth();
    ImGui::PopFont();
    ImGui::End();
  }

  // Where the camera looks (the map editor carries it over when its preview is rebuilt for an edit).
  struct View {
    glm::vec3 target{0.0f};
    float yaw = 0, pitch = 0, distance = 0;
    bool flat = false;
  };
  View view() const { return {target_, yaw_, pitch_, distance_, flat_}; }
  // The map editor: the hex under the mouse (none over the panels or off the board), its rings, and whether a unit or
  // decoration is held by the mouse right now.
  std::optional<std::pair<int, int>> hexUnderMouse() const {
    if (mouse_.x < 0.0f || (ImGui::GetCurrentContext() && ImGui::GetIO().WantCaptureMouse)) return std::nullopt;
    const std::optional<e2::BattleHex> hex = pick(mouseWindow_, mouse_.x, mouse_.y);
    return hex ? std::optional<std::pair<int, int>>({hex->x, hex->y}) : std::nullopt;
  }
  void setEditorMarks(std::vector<Renderer::GroundMark> marks) { editorMarks_ = std::move(marks); }
  bool editorHolding() const { return editorHold_.has_value(); }
  std::optional<glm::vec3> lastPickPoint() const { return lastPickPoint_; }
  void editorDropHold() { editorHold_.reset(); }
  // The camera bar's turn (Engine2's preview buttons ↶ ↷ ↓ ↑ and Reset).
  void editorTurn(float degrees) { yaw_ += glm::radians(degrees); }
  void editorTilt(float degrees) { pitch_ = std::clamp(pitch_ + glm::radians(degrees), minPitch(), kMaxPitch); }
  void setView(const View& v) {
    target_ = v.target, yaw_ = v.yaw, pitch_ = v.pitch, distance_ = v.distance, flat_ = v.flat;
    cam_ = {target_, yaw_, pitch_, distance_};
    captured_ = true, homeYaw_ = yaw_, homePitch_ = pitch_, homeTarget_ = target_, homeDistance_ = distance_;
  }
  bool failed() const { return failed_; }

  // The player asked to leave the fight (the battle menu's Desistir / Encerrar teste, or after the battle's end: back
  // where it came from), or to fight it again.
  bool wantsTitle() const { return titleRequested_; }
  // The battle menu's "Tela inicial": straight to the title screen.
  bool wantsHome() const { return homeRequested_; }
  // GameApp's music while the battle is on screen (its result screen included): the mission's track, silent while the
  // Esc menu is open (pauseMusic) or a film plays over it (musicHeld, set by BattleTalk).
  Music music() const override {
    if (pauseOpen_ || musicHeld) return {};
    return music_;
  }
  bool musicHeld = false;
  // A conversation or one of the Inn's menus is over the map (NpcTalk.h): the board takes no input; with hudHidden the
  // battle's own panels are not drawn either (the menu covers the whole screen).
  bool inputHeld = false, hudHidden = false;
  // The track while this map is on screen, replacing the battle file's (the walkable Inn's floors keep the Inn's theme).
  void setMusic(Music m) { music_ = std::move(m); }
  // An NPC's conversation asked for (Engine2 handleCell's dialog branch: a click on a neutral who has one; in free roam the
  // leader walks up beside it first): its tree and who speaks, taken once.
  struct Talk {
    e2::Json tree;
    std::string name;
  };
  std::optional<Talk> takeTalk() {
    if (!talkRequest_ || !battle_ || *talkRequest_ >= battle_->units().size()) return talkRequest_.reset(), std::nullopt;
    const size_t b = *std::exchange(talkRequest_, std::nullopt);
    const auto it = npcDialog_.find(b);
    if (it == npcDialog_.end()) return std::nullopt;
    talked_ = true;
    return Talk{it->second, battle_->units()[b].name};
  }
  // The units on the board, by name and sprite (DialogOverlay's characters: a line's portrait falls back to its speaker's).
  std::vector<std::pair<std::string, std::string>> characters() const {
    std::vector<std::pair<std::string, std::string>> out;
    if (battle_)
      for (const e2::BattleUnit& u : battle_->units()) out.emplace_back(u.name, u.sprite);
    return out;
  }
  // A floor connector taken (the finish prompt's "Sim" on a connector to another map): that map's mission id, else empty.
  const std::string& wantsFloor() const { return floorRequest_; }
  void clearFloor() { floorRequest_.clear(); }
  // The map's own id (its draft's: the mission id a floor connector names) and whether it is a free-roam map.
  std::string draftId() const { return board_ ? board_->draft().id : std::string(); }
  bool explores() const { return explores_; }
  // The Map Editor's Testar battle: its menu says "Encerrar teste" (GameApp playtest).
  void setPlaytest(bool on) { playtest_ = on; }
  // Opções (EmberMenus.h): Tela cheia and the game's graphics switches (DevGfxSettings.h), applied here at once when
  // changed, the way Engine2's renderer read them every frame.
  void setMenus(std::function<void()> fullscreen, DevGfx* gfx) { fullscreen_ = std::move(fullscreen), gfx_ = gfx; }
  void applyGfx(const DevGfx& g) {
    layers_ = g.realShadows ? layers_ | 2u : layers_ & ~2u;
    layers_ = g.localLights ? layers_ | 8u : layers_ & ~8u;
    options_.volume = g.atmosphericFx, options_.airParticles = g.atmosphericFx;
  }
  // --- Save / Load in battle (GameApp's battle menu Save and Load, Engine2's BattleSnapshot) ---
  // The campaign this fight belongs to: its slots, the campaign mission id and the map's key (a fight saved on another
  // map file is not resumed). `testMode`: Modo teste writes nothing, so its menu has no Save.
  void setCampaign(e2::Campaign* campaign, std::string missionId, std::string mapKey, bool testMode) {
    campaign_ = campaign, missionId_ = std::move(missionId), mapKey_ = std::move(mapKey), testMode_ = testMode;
    // The Inn quests' pickups lying on this mission's map (GameApp startBattle: quests.ts activePickupsFor; never in Modo teste).
    if (battle_ && campaign_ && !testMode_ && !playtest_ && options_.world)
      battle_->setQuestPickups(e2::activeQuestPickups(questTable("quests"), campaign_->activeSave(), missionId_));
    // A campaign fight: each hero wears what the save has it wearing (Engine2 spawnUnit's gearStatBonus on HP / ATK / MAG / DEF /
    // DEX / movement), its HP as the save carries it (full when it carries none).
    if (!campaign_ || testMode_ || playtest_ || !battle_ || !ensureInnRules()) return;
    const e2::Json save = campaign_->activeSave();
    static const e2::Loadouts arms = [] {
      e2::Loadouts l;
      l.load("content/engine2/weapons.json");
      return l;
    }();
    for (e2::BattleUnit& u : battle_->units()) {
      if (u.side != "player" || u.summoned || !u.alive) continue;
      // ... and wields what it has equipped: main hand, off hand, their reach, staff magic and mastery (e2::equipFromSave)
      if (!e2::equipFromSave(u, arms, save)) logWarn("{}: class {} has no weapon data: it keeps the battle file's weapon", u.name, u.classId);
      // ... and its Esquiva (ours: save.heroSkills[hero].dodging)
      if (const e2::Json skills = save.value("heroSkills", e2::Json::object()); skills.is_object() && skills.contains(u.name) &&
                                                                             skills[u.name].is_object() && skills[u.name].contains("dodging") &&
                                                                             skills[u.name]["dodging"].is_number()) {
        const double v = skills[u.name]["dodging"].get<double>();
        bool set = false;
        for (auto& [id, value] : u.skills)
          if (id == "dodging") value = v, set = true;
        if (!set) u.skills.emplace_back("dodging", v);
      }
      const bool full = u.hp >= u.maxHp;
      const e2::InnRules::GearBonus g = innRules_->gearBonus(save, u.name, u.classId);
      u.gearHp = g.hp, u.gearAtk = g.atk, u.gearMag = g.mag, u.gearDef = g.def, u.gearDex = g.dex, u.gearMov = g.mov;
      e2::reapplyGear(u);
      u.hp = full ? u.maxHp : std::min(u.maxHp, u.hp);
    }
  }
  // The fight as Engine2's BattleSnapshot (captureSnapshot): an action in flight spends the acting unit's action (and,
  // an enemy's or one with no movement left, its turn), so it is neither replayed nor lost on load.
  e2::Json captureSnapshot() const {
    if (!battle_) return e2::Json();
    e2::Json snap = battle_->snapshot();
    const int active = battle_->activeUnit();
    const bool inFlight = exchange_ || enemyTurn_ || pendingStrike_ >= 0 ||
                          std::any_of(unitWalk_.begin(), unitWalk_.end(), [](const Walk& w) { return !w.points.empty(); });
    if (active >= 0 && inFlight) {
      const e2::BattleUnit& u = battle_->units()[static_cast<size_t>(active)];
      for (e2::Json& su : snap["units"])
        if (su.value("id", std::string()) == u.id) {
          su["acted"] = true;
          if (u.side == "enemy" || u.mov - u.moveUsed <= 0) su["moved"] = true;
        }
    }
    snap["missionId"] = missionId_;
    snap["mapKey"] = mapKey_;
    snap["introDialogDone"] = true;
    snap["phase"] = active >= 0 && battle_->units()[static_cast<size_t>(active)].side == "enemy" ? "enemy" : "player";
    snap["activeUnitId"] = active >= 0 ? e2::Json(battle_->units()[static_cast<size_t>(active)].id) : e2::Json();
    snap["turnBegan"] = active >= 0 && active == turnUnit_;
    if (fog_) {  // the explored hexes, one bit each, row-major, base64 (snapshotExplored)
      const e2::HexLayout& L = board_->layout();
      std::vector<uint8_t> bits(static_cast<size_t>((L.cols * L.rows + 7) / 8), 0);
      for (int r = 0; r < L.rows; ++r)
        for (int c = 0; c < L.cols; ++c)
          if (fog_->state(c, r) != e2::kFogUnseen) bits[static_cast<size_t>((r * L.cols + c) / 8)] |= static_cast<uint8_t>(1u << ((r * L.cols + c) % 8));
      snap["explored"] = base64(bits);
      e2::Json awake = e2::Json::array();
      for (const int b : awake_) awake.push_back(battle_->units()[static_cast<size_t>(b)].id);
      snap["awake"] = awake;
    }
    return snap;
  }
  // A saved fight resumed on this battle, before its first frame (applySnapshot); false (nothing changed) when it is
  // another mission's, was saved on another map file, or its units are not this battle's.
  bool resume(const e2::Json& snap) {
    if (!battle_ || !snap.is_object()) return false;
    if (snap.value("missionId", std::string()) != missionId_ || (!mapKey_.empty() && snap.value("mapKey", mapKey_) != mapKey_)) return false;
    if (!battle_->applySnapshot(snap)) return false;
    syncSummons();  // the familiars summoned before the save get their cards back
    for (size_t i = 0; i < unitBattle_.size(); ++i) {
      if (unitBattle_[i] < 0) continue;
      const e2::BattleUnit& u = battle_->units()[static_cast<size_t>(unitBattle_[i])];
      unitPos_[i] = anchorWorld(u, {u.x, u.y});
      unitMoved_[i] = 1;
      unitFacing_[i] = u.facing < 0 ? -1 : 1;
      if (!u.alive) unitDiedAt_[i] = -1000.0f;  // the fallen of the saved fight are gone
    }
    std::unordered_map<std::string, int> byId;
    for (size_t b = 0; b < battle_->units().size(); ++b) byId[battle_->units()[b].id] = static_cast<int>(b);
    awake_.clear();
    if (snap.contains("awake") && snap["awake"].is_array())
      for (const e2::Json& id : snap["awake"])
        if (id.is_string())
          if (const auto it = byId.find(id.get<std::string>()); it != byId.end()) awake_.insert(it->second);
    if (fog_ && snap.contains("explored") && snap["explored"].is_string()) {
      const std::vector<uint8_t> bits = unbase64(snap["explored"].get<std::string>());
      const e2::HexLayout& L = board_->layout();
      pendingExplored_.assign(static_cast<size_t>(L.cols * L.rows), 0);
      for (int r = 0; r < L.rows; ++r)
        for (int c = 0; c < L.cols; ++c) {
          const size_t bit = static_cast<size_t>(r * L.cols + c);
          if (bit / 8 < bits.size() && (bits[bit / 8] >> (bit % 8) & 1u)) pendingExplored_[static_cast<size_t>(e2::indexOf(L, c, r))] = 1;
        }
    }
    // A turn that had begun is not begun again (Engine2 skipStartOfTurn: no second echo or burn).
    turnUnit_ = snap.value("turnBegan", false) ? battle_->activeUnit() : -1;
    pickActiveOnResume_ = turnUnit_ >= 0;  // the turn is not begun again, but its hero must be picked again
    levelSeen_.clear();  // the saved levels are not level-ups
    tip_ = tr("Combate retomado.", "Battle resumed."), tipAt_ = now_;
    return true;
  }
  // The battle menu's Load chose a slot: the game goes where that save stands (GameApp enterFromSave).
  bool wantsLoad() const { return loadRequested_; }

  // Captures: the battle menu or Opções open.
  void previewMenu(const std::string& which) {
    if (which == "menu") pauseOpen_ = true;
    else if (which == "options") menuOptions_.open();
    else if (which == "log") showLog_ = true, logJustOpened_ = true, logKeep_ = true;  // a capture: the turns the script plays do not close it
    else if (which == "fulllog") logFull_ = true, logFullJustOpened_ = true;
    else if (which == "quests") questLog_ = true;
    else if (which == "hotbar" && battle_) {  // the first hero's
      for (size_t i = 0; i < battle_->units().size() && !hotbarEdit_; ++i)
        if (battle_->units()[i].side == "player") hotbarEdit_ = true, hotbarHero_ = static_cast<int>(i);
    }
    else if (which == "status" && battle_) openStatus(battle_->activeUnit() >= 0 ? battle_->activeUnit() : 0);
    else if (which.starts_with("inventory:") && battle_) {  // "inventory:<name>[:doll]": the Mochila (or Equipar) over the fight
      const std::string rest = which.substr(10);
      const bool doll = rest.ends_with(":doll");
      const std::string name = doll ? rest.substr(0, rest.size() - 5) : rest;
      for (size_t i = 0; i < battle_->units().size(); ++i)
        if (battle_->units()[i].name == name) {
          openInventory(i, !doll);
          break;
        }
    } else if (which.starts_with("synced:") && battle_) {  // "synced:<name>": the inventory's write-back run once, then that unit's sheet
      openInventory(0, true);
      syncInventory();
      battleInventory_.reset();
      for (size_t i = 0; i < battle_->units().size(); ++i)
        if (battle_->units()[i].name == which.substr(7)) {
          openStatus(static_cast<int>(i));
          break;
        }
    } else if (which.starts_with("status:") && battle_) {  // "status:<name>": that unit's sheet
      for (size_t i = 0; i < battle_->units().size(); ++i)
        if (battle_->units()[i].name == which.substr(7) && battle_->units()[i].alive) {
          openStatus(static_cast<int>(i));
          break;
        }
    }
  }
  // "victory" / "defeat" once the fight is over, else empty.
  const std::string& battleResult() const { return result_; }
  // What the fight adds to the quests: the pickups found, and this mission's quest targets that fell (GameApp persistVictory).
  std::vector<std::string> questFound() const { return battle_ ? battle_->questFound() : std::vector<std::string>{}; }
  std::vector<std::string> questKills() const {
    std::vector<std::string> out;
    if (!battle_ || missionId_.empty()) return out;
    for (const std::string& target : e2::questKillTargets(questTable("quests"), missionId_))
      if (std::any_of(battle_->units().begin(), battle_->units().end(),
                      [&](const e2::BattleUnit& u) { return u.side == "enemy" && !u.alive && !u.summoned && u.name == target; }))
        out.push_back(target);
    return out;
  }
  // A line for the combat log from outside the battle's own rules (a conversation's affinity change).
  void battleLog(std::string line) {
    if (battle_) battle_->pushLog(std::move(line));
  }
  // A conversation or film is open over the battle: nobody's turn moves on until it closes.
  bool holdTurns = false;
  // The party's affinities as the fight leaves them (friendly fire, healing, fighting side by side), for the save.
  std::map<std::string, double> affinityScoresNow() const { return battle_ ? battle_->affinityScores() : std::map<std::string, double>(); }
  // GameApp persistVictory crossingDefeatedSpawns: the enemy / neutral spawn ids that fell in this fight (summons aside).
  std::vector<std::string> fallenSpawns() const {
    std::vector<std::string> out;
    if (battle_)
      for (const e2::BattleUnit& u : battle_->units())
        if ((u.side == "enemy" || u.side == "neutral") && !u.alive && !u.summoned && !absent_.contains(u.id)) out.push_back(u.id);
    return out;
  }
  // The waypoint the mission was ended on was a floor connector (data.ts isFloorConnector(hud.activeExit)): no rest after it.
  bool floorTaken() const { return finishExit_ && finishExit_->kind == "connector"; }
  // ... and the floor it leads to (empty: nowhere to go on).
  std::string floorTarget() const { return floorTaken() ? finishExit_->targetMapId : std::string(); }
  // GameApp persistVictory's HP for a hero after a victory: past a floor connector the battle's HP carries on (fallen: 0, out of
  // the next floor); after any other victory the camp heals half of what is missing up to the class's HP at its level now
  // (statsFor(class, level).hp), and the fallen come back at half of it.
  int maxHpAt(const e2::BattleUnit& u, int level) const {
    const int hp = options_.world ? options_.world->classHp(u.classId, level) : 0;
    return hp > 0 ? hp : u.maxHp;
  }
  int campHp(const e2::BattleUnit& u) const {
    const int battle = u.alive ? u.hp : 0;
    if (floorTaken()) return battle;
    const int max = maxHpAt(u, u.level);
    return u.alive ? std::min(max, battle + static_cast<int>(std::ceil((max - battle) * 0.5))) : std::max(1, static_cast<int>(std::ceil(max * 0.5)));
  }
  std::vector<HeroOutcome> heroOutcomes() const {
    std::vector<HeroOutcome> out;
    if (!battle_) return out;
    for (const e2::BattleUnit& u : battle_->units())
      if (u.side == "player" && !u.summoned && !absent_.contains(u.id)) {
        HeroOutcome h{u.name, u.level, u.xp, u.alive ? u.hp : 0};
        h.campHp = campHp(u);
        std::copy(std::begin(u.bag), std::end(u.bag), h.bag.begin());
        h.diseased = u.diseased;
        h.poison = u.poisoned ? (u.poisonTier.empty() ? "lesser" : u.poisonTier) : "";
        h.fullness = u.fullness;
        for (const auto& [id, v] : u.skills)
          if (id == "dodging") h.dodging = v;
        out.push_back(std::move(h));
      }
    return out;
  }
  // A hero as the save brings it (Engine2 spawnUnit): its bag, disease, poison, hunger; disease and the starving
  // penalty taken off its stats (reapplyGear's sums); its HP capped by the save's.
  static void applyHeroState(e2::BattleUnit& u, const MapStageOptions::HeroState& s) {
    if (s.bag) std::copy(s.bag->begin(), s.bag->end(), u.bag);
    // spawnUnit: roster.bags[name] ?? (a healer: EMPTY_BAG, else STARTING_BAG)
    static constexpr int kStartingBag[7] = {2, 2, 1, 1, 1, 0, 0};  // weak mid potent disease manaSmall manaMid manaLarge
    if (!s.bag && s.starterBag)
      for (int k = 0; k < 7; ++k) u.bag[k] = u.classId == "healer" ? 0 : kStartingBag[k];
    u.lockpick = s.lockpick;
    u.pointHp = s.points[0], u.pointAtk = s.points[1], u.pointMag = s.points[2], u.pointDef = s.points[3], u.pointDex = s.points[4];
    u.diseased = s.diseased;
    u.poisoned = !s.poison.empty(), u.poisonTier = s.poison;
    u.fullness = s.fullness;
    u.hungerPenaltyPct = s.hungerPenalty;
    e2::spawnConditions(u);
    u.hp = s.hp && *s.hp > 0 ? std::min(u.maxHp, *s.hp) : u.maxHp;
  }
  // Rations Criar Comida e Água conjured in this fight (Engine2 lootRations).
  int lootRations() const { return battle_ ? battle_->lootRations() : 0; }
  // What the chests opened in this fight gave (Engine2 lootEmber / lootWeapons / lootEquipment), folded into the save on a victory.
  int lootEmber() const { return battle_ ? battle_->lootEmber() : 0; }
  const std::vector<std::string>& lootWeapons() const {
    static const std::vector<std::string> none;
    return battle_ ? battle_->lootWeapons() : none;
  }
  const std::vector<std::string>& lootEquipment() const {
    static const std::vector<std::string> none;
    return battle_ ? battle_->lootEquipment() : none;
  }
  bool wantsRetry() const { return retryRequested_; }
  const MapStageOptions& options() const { return options_; }

  bool done() const override {
    if (failed_) return true;
    if (!options_.battleScript.empty()) return scriptDone_ && readyFrames_ >= 30;
    return options_.measureSeconds > 0.0f ? measured_ : readyFrames_ >= 30;
  }
  bool passed() const override { return !failed_ && ground_.ready() && measurePassed_ && !scriptFailed_; }

 private:
  static constexpr float kFov = 30.0f, kNear = 0.5f;  // Engine2's normal lens
  static constexpr float kFlatFov = 1.0f;             // Engine2 FLAT_FOV: Tática's long lens
  static constexpr float kFlatRowStep = 0.002f;       // Engine2 FLAT_ROW_STEP: Tática's height step per unit of depth
  // The camera bar (Engine2 GameApp.tsx + ember-plate): 28 px plates (size-7), 4 px apart (gap-1), 8 px from the
  // corner (inset-x-2, top 0.5rem), 10 px text; the window's padding leaves room for the shadow and glow.
  static constexpr float kPlate = 28.0f, kBarGap = 4.0f, kBarInset = 8.0f, kPlateText = 12.0f, kPlateGlow = 10.0f;
  static constexpr float kFlameEmit = 2.5f;           // Engine2 FLAME_EMIT
  // Light mode "real" (ARCHITECTURE 7.6 step A).
  static constexpr float kSunLux = 128000.0f;       // the sun above the atmosphere
  static constexpr float kFullMoonLux = 0.5f;       // a full moon (real 0.25, lifted so moonlit ground reads)
  // "Luar: cinema" (the user, 2026-10-19: "gostaria que luz do luar também"): under the camera's night darkening the physical
  // moon never showed on the ground (a capture of cemiterio: black beside the lanterns), so the cinema moon lights like a
  // film's, ~60x the real one; the meter and the night stops still keep night darker than day.
  static constexpr float kCinemaMoonLux = 16.0f;
  static constexpr float kIndoorFillLux = 3.0f, kIndoorAmbientLux = 1.5f;
  static constexpr float kLumensPerUnit = 12.0f;    // Engine2 intensity x reach^2 -> lumens
  static constexpr float kNightStops = 2.5f;        // the camera's darkening at night
  static constexpr float kAdaptPerSecond = 1.5f;    // eye adaptation rate

  // Median, 99th percentile and worst; frames over the 60 Hz budget.
  struct Spread {
    double median = 0, p99 = 0, worst = 0;
    size_t over = 0;
  };
  static Spread spread(std::vector<double> v) {
    Spread s;
    if (v.empty()) return s;
    std::sort(v.begin(), v.end());
    s.median = v[v.size() / 2];
    s.p99 = v[std::min(v.size() - 1, v.size() * 99 / 100)];
    s.worst = v.back();
    s.over = static_cast<size_t>(std::count_if(v.begin(), v.end(), [](double x) { return x > 1000.0 / 60.0; }));
    return s;
  }

  void finishMeasure() {
    measuring_ = false;
    measured_ = true;
    const Spread cpu = spread(busy_), gpu = spread(gpu_);
    const size_t n = busy_.size();
    measurePassed_ = n > 0 && cpu.worst <= 1000.0 / 60.0 && gpu.worst <= 1000.0 / 60.0;
    logInfo("measure {}: {} frames over {:.0f} s (camera turn + zoom, {} units animating, {} lights)", scene_ ? scene_->map : std::string("map"), n,
            options_.measureSeconds, scene_ ? scene_->units.size() : 0, lights_.size());
    logInfo("  main thread: median {:.2f}, 99th {:.2f}, worst {:.2f} ms; {} frames over 16.7 ms", cpu.median, cpu.p99, cpu.worst, cpu.over);
    logInfo("  GPU:         median {:.2f}, 99th {:.2f}, worst {:.2f} ms; {} frames over 16.7 ms", gpu.median, gpu.p99, gpu.worst, gpu.over);
    std::string line = "  GPU passes (average):";
    for (const auto& [name, sum] : passes_) line += std::format(" {} {:.2f}", name, sum / std::max<size_t>(1, n));
    logInfo("{}", line);
    line = "  GPU passes (worst, frames run):";
    for (const auto& [name, w] : passWorst_) line += std::format(" {} {:.2f} ({})", name, w.first, w.second);
    logInfo("{}", line);
    if (fog_) {
      const Spread fu = spread(fogUpdateMs_);
      logInfo("  fog of war: worst {} enclosed never-seen holes at once", worstHoles_);
      const Spread fd = spread(fogDeliverMs_);
      logInfo("  fog of war: {} mask updates (sight + mask on a worker): median {:.2f}, worst {:.2f} ms", fogUpdateMs_.size(), fu.median,
              fu.worst);
      logInfo("  fog of war: taking each result on the main thread: median {:.2f}, worst {:.2f} ms", fd.median, fd.worst);
    }
    logInfo("  {}", measurePassed_ ? "PASS: every frame within 16.7 ms" : "FAIL: frames over the 16.7 ms budget");
  }

  // Engine2's battle loading steps (GameApp battleLoadCaption, LoadingScreens.h): 0 the characters (every sprite clip's
  // header and small version on the GPU: what the cards draw from), 1 the field (ground, the 3D pieces and their
  // textures, models placed, the first fog-of-war mask, a first frame with full-quality cards).
  Loading loading() const override {
    if (scene_ && !sprites_.ready()) {
      const auto [done, total] = sprites_.startupProgress();
      return {0, done, total, false};
    }
    if (scene_) {
      const auto [pinsDone, pins] = sprites_.pinProgress();
      const uint32_t hudDone = static_cast<uint32_t>(hudPreloaded_.first), hud = hudPreload_ ? static_cast<uint32_t>(hudPreloaded_.second) : hudDone;
      // the characters (every pose they can play) and the HUD's pictures
      if (pinsDone < pins || hudDone < hud) return {0, pinsDone + hudDone, pins + hud, false};
    }
    const auto [meshesDone, meshesTotal] = meshes_.progress();
    const uint32_t ground = ground_.ready() ? 1u : 0u, placed = !scene_ || modelsPlaced_ ? 1u : 0u;
    const uint32_t fog = !fog_ || fogReady_ ? 1u : 0u, drawn = !scene_ || fullFrames_ > 0 ? 1u : 0u;
    Loading l{1, meshesDone + ground + placed + fog + drawn, meshesTotal + 4u, false};
    l.ready = l.done >= l.total;
    return l;
  }

  bool sceneReady() const {
    if (!scene_) return true;
    const auto [pinsDone, pins] = sprites_.pinProgress();
    return sprites_.ready() && pinsDone >= pins && !hudPreload_ && modelsPlaced_ && fullFrames_ > 0;
  }

  float groundOf(float smooth, float hard) const { return look_ == e2::EdgeStyle::Smooth ? smooth : hard; }

  // Engine2's flickerAt (src/ember/lighting.ts).
  static float flicker(float time, float seed, float amount) {
    if (amount <= 0.0f) return 1.0f;
    const float v = 0.06f * std::sin(time * 5.3f + seed) + 0.045f * std::sin(time * 8.9f + seed * 1.7f) +
                    0.03f * std::sin(time * 13.7f + seed * 2.9f) + 0.03f * std::sin(time * 2.1f + seed * 0.6f);
    return 1.0f + v * amount;
  }

  // Ember's 3D architecture (world/Architecture.h = Engine2 decor.ts addArchitecture over Ember's createWallGeometry):
  // one mesh per piece, built here in the cooked format (render/MeshBuild) so it is culled, lit and shadowed like the
  // models; its wall texture from build/meshes/architecture.pak (Cooker architecture). Walls, closed doors and secret
  // doors block sight; open doorways do not.
  struct ArchPiece {
    MeshId mesh = 0;
    float x = 0, z = 0, groundSmooth = 0, groundHard = 0, half = 0, top = 0;
    bool blocks = false;
    std::vector<std::pair<float, float>> footprint;  // sight samples (world x, z) under its top faces
  };
  static const e2::EditorCatalog& architectureCatalog() {
    static const e2::EditorCatalog catalog = [] {
      e2::EditorCatalog c;
      c.load("content/engine2/editor-catalog.json");
      return c;
    }();
    return catalog;
  }
  static const e2::ArchitectureDefs& architectureDefs() {
    static const e2::ArchitectureDefs defs = [] {
      e2::ArchitectureDefs d;
      if (!d.load("content/engine2/editor-catalog.json")) logError("architecture: content/engine2/editor-catalog.json has no architectureDefs");
      return d;
    }();
    return defs;
  }
  void buildArchitecture() {
    arch_.clear();
    if (scene_->pieces.empty()) return;
    const e2::EditorCatalog& catalog = architectureCatalog();
    const e2::ArchitectureDefs& defs = architectureDefs();
    const e2::HexLayout& L = board_->layout();
    const double sq3 = std::sqrt(3.0);
    auto inBoard = [&](int c, int r) { return c >= 0 && r >= 0 && c < L.cols && r < L.rows; };
    // architectureWorld: every row at the same x, no odd-row half-hex shift, so walls run straight
    auto archAt = [&](int c, int r) { return std::pair<double, double>{sq3 * c - L.ox, board_->cell(c, r).z}; };
    std::set<std::pair<int, int>> joins;  // the cells a wall joins (architecture, not rocks): anchors, an entrance's two cells
    for (const e2::SceneArchitecture& p : scene_->pieces) {
      const e2::ArchitectureDef* def = defs.find(p.id);
      const e2::CatalogDecoration* cd = catalog.decoration(p.id);
      if (!def || !cd || !def->connects) continue;
      if (p.id == "watchtower-stone-open-door-2hex")
        for (const e2::FootOffset& f : e2::placedFootprint(*cd, p.x, p.y, p.rot)) joins.insert({p.x + f.dx, p.y + f.dy});
      else
        joins.insert({p.x, p.y});
    }
    size_t missing = 0;
    for (const e2::SceneArchitecture& p : scene_->pieces) {
      const e2::ArchitectureDef* def = defs.find(p.id);
      const e2::CatalogDecoration* cd = catalog.decoration(p.id);
      if (!def || !cd) {
        ++missing;
        continue;
      }
      double x = 0, z = 0;
      int n = 0;
      for (const e2::FootOffset& f : e2::placedFootprint(*cd, p.x, p.y, p.rot))
        if (inBoard(p.x + f.dx, p.y + f.dy)) {
          const auto [ax, az] = archAt(p.x + f.dx, p.y + f.dy);
          x += ax, z += az, ++n;
        }
      if (!n) continue;
      x /= n, z /= n;
      std::vector<std::pair<double, double>> connections;
      for (const auto& [c, r] : {std::pair{p.x - 1, p.y}, std::pair{p.x + 1, p.y}, std::pair{p.x, p.y - 1}, std::pair{p.x, p.y + 1}})
        if (inBoard(c, r) && joins.contains({c, r})) {
          const auto [nx, nz] = archAt(c, r);
          connections.push_back({nx - x, -(nz - z)});
        }
      const e2::three::Geometry g = e2::pieceGeometry(*def, p.rot, connections, {x, -z});
      // flat shading (Engine2's material): each triangle's own normal; equal corners welded
      MeshBuildPart part;
      std::map<std::array<float, 8>, uint32_t> weld;
      float minY = std::numeric_limits<float>::max(), maxY = -minY, minX = minY, maxX = maxY, minZ = minY, maxZ = maxY;
      ArchPiece piece;
      for (size_t t = 0; t + 2 < g.count(); t += 3) {
        const glm::vec3 a{g.position[t * 3], g.position[t * 3 + 1], g.position[t * 3 + 2]}, b{g.position[t * 3 + 3], g.position[t * 3 + 4], g.position[t * 3 + 5]},
            c{g.position[t * 3 + 6], g.position[t * 3 + 7], g.position[t * 3 + 8]};
        const glm::vec3 cross = glm::cross(b - a, c - a);
        if (glm::length(cross) <= 0.0f) continue;
        const glm::vec3 normal = glm::normalize(cross);
        if (normal.y > 0.5f) {  // a top face: sight is blocked under it
          const float x0 = std::min({a.x, b.x, c.x}), x1 = std::max({a.x, b.x, c.x}), z0 = std::min({a.z, b.z, c.z}), z1 = std::max({a.z, b.z, c.z});
          for (float sx = x0; sx <= x1 + 1e-4f; sx += 0.25f)
            for (float sz = z0; sz <= z1 + 1e-4f; sz += 0.25f) piece.footprint.push_back({static_cast<float>(x) + sx, static_cast<float>(z) + sz});
        }
        for (size_t k = 0; k < 3; ++k) {
          const size_t v = t + k;
          // three's TextureLoader flips the image (v up from its bottom row); cooked textures start at the top
          const std::array<float, 8> key{g.position[v * 3], g.position[v * 3 + 1], g.position[v * 3 + 2], normal.x, normal.y, normal.z,
                                         g.uv[v * 2], 1.0f - g.uv[v * 2 + 1]};
          auto [it, fresh] = weld.try_emplace(key, static_cast<uint32_t>(part.positions.size() / 3));
          if (fresh) {
            part.positions.insert(part.positions.end(), {key[0], key[1], key[2]});
            part.normals.insert(part.normals.end(), {key[3], key[4], key[5]});
            part.uvs.insert(part.uvs.end(), {key[6], key[7]});
            part.tangents.insert(part.tangents.end(), {1.0f, 0.0f, 0.0f, 1.0f});
            part.colors.insert(part.colors.end(), {1.0f, 1.0f, 1.0f, 1.0f});
            minX = std::min(minX, key[0]), maxX = std::max(maxX, key[0]), minY = std::min(minY, key[1]), maxY = std::max(maxY, key[1]);
            minZ = std::min(minZ, key[2]), maxZ = std::max(maxZ, key[2]);
          }
          part.indices.push_back(it->second);
        }
      }
      if (part.indices.empty()) continue;
      // MeshStandardMaterial({map, color: map ? white : the piece's colour, roughness, metalness}) (three colours are sRGB)
      const bool metal = def->doorStyle == "iron" || def->doorStyle == "steel";
      const uint32_t hex = metal ? (def->doorStyle == "iron" ? 0x555b60u : 0x9ca7b0u) : def->model3d == "door" ? 0x925b32u : def->model3d == "doorway" ? 0xb7a27cu : 0x8b8b86u;
      auto linear = [](uint32_t c) {
        const float s = static_cast<float>(c) / 255.0f;
        return s <= 0.04045f ? s / 12.92f : std::pow((s + 0.055f) / 1.055f, 2.4f);
      };
      MeshMaterial m{};
      const bool textured = !def->wallTexture.empty();
      for (int k = 0; k < 3; ++k) m.baseColor[k] = textured ? 1.0f : linear((hex >> (16 - 8 * k)) & 255u);
      m.baseColor[3] = 1.0f;
      m.metallic = metal ? 0.5f : 0.0f;
      m.roughness = metal ? 0.52f : 0.94f;
      m.normalScale = 1.0f, m.occlusionStrength = 1.0f, m.alphaCutoff = 0.5f;
      if (textured) m.baseColorTexture = assetId(e2::architectureTextureAsset(def->wallTexture));
      MeshBuild build;
      if (!build.addPart(part, 0, false, textured, false)) continue;
      std::vector<uint8_t> bytes = build.finish({m}, part.indices.size() / 3);
      if (bytes.empty()) continue;
      piece.mesh = meshes_.addBuilt(std::move(bytes));
      piece.x = static_cast<float>(x), piece.z = static_cast<float>(z);
      piece.groundSmooth = static_cast<float>(e2::groundHeightAt(*board_, e2::EdgeStyle::Smooth, x, z));
      piece.groundHard = static_cast<float>(e2::groundHeightAt(*board_, e2::EdgeStyle::Hard, x, z));
      piece.half = std::max({std::abs(minX), std::abs(maxX), std::abs(minZ), std::abs(maxZ)});
      piece.top = maxY;
      piece.blocks = def->model3d == "wall" || def->model3d == "door" || def->model3d == "secretDoor";
      arch_.push_back(std::move(piece));
    }
    if (missing) logWarn("architecture: {} pieces not in the editor catalog", missing);
  }

  // Engine2's models.ts: scaled uniformly to their height, base centred on the anchor, turned by rotY.
  void placeModels() {
    modelsPlaced_ = true;
    std::vector<InstanceGpu> instances;
    pieceBoxes_.clear();
    for (const e2::SceneModel& m : scene_->models) {
      if (lockOpened(m.id, "doorDecor", m.x, m.z)) continue;  // the door was picked open: its closed model is gone
      std::string asset = m.file.substr(std::string("/game/").size());
      asset = "meshes/" + asset.substr(0, asset.size() - 4) + ".mesh";
      const MeshHeader* h = meshes_.header(modelMeshes_.at(asset));
      if (!h) {
        logWarn("scene: model {} is not in the mesh pack", asset);
        continue;
      }
      const float k = m.height / std::max(h->boundsMax[1] - h->boundsMin[1], 1e-4f);
      const glm::vec3 offset{-(h->boundsMin[0] + h->boundsMax[0]) * 0.5f * k, -h->boundsMin[1] * k, -(h->boundsMin[2] + h->boundsMax[2]) * 0.5f * k};
      const float c = std::cos(m.rotY), sn = std::sin(m.rotY);
      // three.js rotation about y: x' = x cos + z sin, z' = -x sin + z cos
      const glm::vec3 t{c * offset.x + sn * offset.z + m.x, offset.y + groundOf(m.groundSmooth, m.groundHard), -sn * offset.x + c * offset.z + m.z};
      InstanceGpu inst{};
      inst.rows[0] = {c * k, 0, sn * k, t.x};
      inst.rows[1] = {0, k, 0, t.y};
      inst.rows[2] = {-sn * k, 0, c * k, t.z};
      inst.mesh = modelMeshes_.at(asset);
      inst.scale = k;
      instances.push_back(inst);
      const float half = 0.5f * k * std::max(h->boundsMax[0] - h->boundsMin[0], h->boundsMax[2] - h->boundsMin[2]);
      pieceBoxes_.push_back({m.x, m.z, half, t.y + m.height});
    }
    modelCount_ = static_cast<uint32_t>(instances.size());
    // Ember's architecture: each piece's own mesh, stood on the ground the board shows.
    for (const ArchPiece& a : arch_) {
      const float y = groundOf(a.groundSmooth, a.groundHard);
      InstanceGpu inst{};
      inst.rows[0] = {1, 0, 0, a.x};
      inst.rows[1] = {0, 1, 0, y};
      inst.rows[2] = {0, 0, 1, a.z};
      inst.mesh = a.mesh;
      inst.scale = 1;
      instances.push_back(inst);
      pieceBoxes_.push_back({a.x, a.z, a.half, y + a.top});
    }
    meshes_.setInstances(instances);
    placedLook_ = look_;
  }

  // One card: the whole art spans w x h world units with its bottom centre at `local` (card-local x along
  // `right`, y up, z along the facing); the cooked clip holds a trimmed part of it.
  void addCard(SpriteClipId clip, glm::vec3 anchor, glm::vec3 right, float w, float h, glm::vec3 local, bool mirror, uint32_t frame,
               uint32_t flags, const SpriteInstance* emit) {
    const SpriteClipHeader& c = sprites_.header(clip);
    const float sw = static_cast<float>(c.sourceWidth), sh = static_cast<float>(c.sourceHeight);
    const float cx = (static_cast<float>(c.cropX) + c.contentWidth * 0.5f) / sw - 0.5f;
    const float cy = 1.0f - (static_cast<float>(c.cropY) + c.contentHeight) / sh;
    const float x = (local.x + cx * w) * (mirror ? -1.0f : 1.0f);
    const bool flat = (flags & kSpriteFlat) != 0;  // lying face up: its height runs toward -z
    const glm::vec3 facing = flat ? glm::vec3(0.0f, 1.0f, 0.0f) : glm::vec3(-right.z, 0.0f, right.x);
    const glm::vec3 up = flat ? glm::vec3(0.0f, 0.0f, -1.0f) : glm::vec3(0.0f, 1.0f, 0.0f);
    SpriteInstance s{};
    s.origin = anchor + right * x + up * (local.y + cy * h) + facing * local.z;
    s.width = w * c.contentWidth / sw;
    s.height = h * c.contentHeight / sh;
    // A fight's character (pinned) is held by the sprite system as loaded before the fight: Alta or Leve, never streamed.
    if (!sprites_.isPinned(clip)) sprites_.request(clip, SpriteSystem::kOnScreen);
    const SpriteSystem::Texture tex = sprites_.texture(clip);
    if (tex.index == kNoTexture) return;
    if (tex.full) ++fullFrames_;
    s.textureIndex = tex.index;
    s.layer = std::min(frame, c.frames - 1);
    s.flags = flags | (mirror ? kSpriteMirrored : 0u);
    s.uvScale = {static_cast<float>(c.contentWidth) / c.width, static_cast<float>(c.contentHeight) / c.height};
    s.right = {right.x, right.z};
    if (emit) {
      // The light's place on the art (full-image uv, v up) in this texture's uv, and texture uv -> card heights.
      s.emitColor = emit->emitColor;
      s.emitLevel = emit->emitLevel;
      s.emitAt = {(emit->emitAt.x * sw - c.cropX) / c.contentWidth * s.uvScale.x, ((1.0f - emit->emitAt.y) * sh - c.cropY) / c.contentHeight * s.uvScale.y,
                  (static_cast<float>(c.contentWidth) / sw) / s.uvScale.x * (w / h)};
      s.emitScaleV = (static_cast<float>(c.contentHeight) / sh) / s.uvScale.y;
    }
    drawList_.push_back(s);
  }

  // Whether the decoration `id` standing at world (x, z) is a lock of this kind already opened (the battle's locks sit on hexes).
  bool lockOpened(const std::string& id, const char* kind, float x, float z) const {
    if (!battle_ || !board_) return false;
    bool any = false;
    for (const e2::Battle::Lock& l : battle_->locks()) any = any || l.open;
    if (!any) return false;
    const auto [c, r] = e2::hexAt(board_->layout(), x, z);
    for (const e2::Battle::Lock& l : battle_->locks())
      if (l.open && l.kind == kind && l.id == id && l.x == c && l.y == r) return true;
    return false;
  }

  // "Arrombar" (Engine2 useLockpick): the picked hero spends a Gazua on the chest or door beside it.
  void openLock() {
    if (!battle_ || selected_ < 0 || !myTurn() || busy()) return;
    const size_t ui = static_cast<size_t>(selected_), ab = static_cast<size_t>(unitBattle_[ui]);
    if (battle_->units()[ab].acted) return;
    std::vector<e2::BattleEffect> fx;
    if (!e2::useLockpick(*battle_, ab, rng_, fx)) return;
    showEffects(fx);
    e2::BattleUnit& au = battle_->units()[ab];
    if (!au.summoned) au.fullness = std::max(0.0, std::min(120.0, au.fullness.value_or(100.0)) - 2.0);  // finishAction: every action costs 2 of fullness
    tip_ = battle_->log().empty() ? std::string() : battle_->log().back(), tipAt_ = now_;
    if (modelsPlaced_) placeModels();  // an opened door's closed model leaves
    if (!au.alive || au.mov - au.moveUsed <= 0) {
      if (au.side == "player") au.moved = true;
      return select(-1);
    }
    refreshReach();
  }

  // Engine2's chest-loot popup: who opened it, the Ember and what else it held; stays until the player says Ok.
  void drawChestLoot() {
    if (!battle_ || !battle_->chestLoot()) return;
    const e2::Battle::ChestLoot& loot = *battle_->chestLoot();
    const ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos({io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.42f}, ImGuiCond_Always, {0.5f, 0.5f});
    ImGui::SetNextWindowSize({380.0f, 0.0f});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {18.0f, 14.0f});
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {8.0f, 8.0f});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(14, 14, 14, 244));
    ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(226, 150, 70, 190));
    ImGui::Begin("##chestloot", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::PushFont(uiFont(UiFont::Ui), 16.0f);
    ImGui::TextUnformatted((loot.unit + tr(" arrombou o baú", " picked the chest's lock")).c_str());
    ImGui::PopFont();
    ImGui::PushFont(uiFont(UiFont::Ui), 15.0f);
    {  // Engine2's GoldAmount: the pile of coins, then "+N Gold"
      const ImVec2 at = ImGui::GetCursorScreenPos();
      const float coin = 26.0f;
      if (const UiImage* gold = uiImage(options_.engine2Public / "game/icons/gold-coins-pile.png")) imageContain(ImGui::GetWindowDrawList(), *gold, {at.x, at.y}, {at.x + coin, at.y + coin});
      ImGui::SetCursorScreenPos({at.x + coin + 8.0f, at.y + (coin - ImGui::GetTextLineHeight()) * 0.5f});
      ImGui::TextUnformatted(("+" + std::to_string(loot.ember) + " Gold").c_str());
      ImGui::SetCursorScreenPos({at.x, at.y + coin + 4.0f});
    }
    ensureInnData();
    for (const e2::Battle::ChestLoot::Item& item : loot.items) {  // each find with its own icon, as Engine2's popup draws them
      std::string icon;
      if (item.kind == "potion") icon = "game/icons/potion-" + item.id + ".png";
      else if (item.kind == "rations") icon = "game/icons/rations.png";
      else if (item.kind == "weapon") icon = innData_.weapon(item.id) ? innData_.weapon(item.id)->icon : "";
      else if (const e2::InnGear* gear = innData_.gear(item.id)) icon = gear->icon;
      if (const size_t q = icon.find('?'); q != std::string::npos) icon.resize(q);
      if (!icon.empty() && icon[0] == '/') icon.erase(0, 1);
      const UiImage* img = icon.empty() ? nullptr : uiImage(options_.engine2Public / std::filesystem::path(reinterpret_cast<const char8_t*>(icon.c_str())));
      const ImVec2 at = ImGui::GetCursorScreenPos();
      const float row = 40.0f;
      if (img) ImGui::GetWindowDrawList()->AddImage(img->texture, {at.x, at.y}, {at.x + row, at.y + row});
      ImGui::SetCursorScreenPos({at.x + row + 10.0f, at.y + (row - ImGui::GetTextLineHeight()) * 0.5f});
      ImGui::TextUnformatted(item.name.c_str());
      ImGui::SetCursorScreenPos({at.x, at.y + row + 4.0f});
    }
    ImGui::PopFont();
    if (emberButton("##chestok", "Ok", true, true)) battle_->clearChestLoot();
    ImGui::End();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(3);
  }

  void buildCards(const Camera& camera, float time) {
    missing_ = 0;
    fullFrames_ = 0;
    if (placedLook_ != look_ && modelsPlaced_) placeModels();
    // Decorations: Engine2's DecorLayer. Cards stand square to the hex rows (LOCK_PROPS) or along their own
    // turn (barricades); the art's opaque base sits on the anchor; Ember's layer step moves them along their facing.
    for (const e2::SceneCard& d : scene_->cards) {
      if (d.onFloor) continue;  // waypoints lying on the floor: not drawn yet
      if (lockOpened(d.id, "chest", d.x, d.z)) continue;  // a chest opened with a Gazua is gone (Engine2 drops its decoration)
      const std::string name = std::filesystem::path(d.image).stem().string();
      const SpriteClipId clip = sprites_.find("decor", name);
      if (clip == kNoSpriteClip) {
        ++missing_;
        continue;
      }
      // Tática (Engine2 DecorLayer.face with flatTop): every card lies flat square to the screen, just over the highest
      // ground, each row of depth a step higher so nearer rows cover farther ones; no layer step, no shadow.
      const float yaw = flat_ ? 0.0f : d.lockedYaw.value_or(0.0f);
      const glm::vec3 right{std::cos(yaw), 0.0f, -std::sin(yaw)};
      const glm::vec3 anchor{d.x, flat_ ? boardTop_ + d.z * kFlatRowStep : groundOf(d.groundSmooth, d.groundHard), d.z};
      const glm::vec3 local{(0.5f - d.art.baseU) * d.w, -d.art.baseV * d.h, flat_ ? 0.0f : d.nudge};
      SpriteInstance emit{};
      if (d.light) {
        emit.emitColor = {d.light->color[0], d.light->color[1], d.light->color[2]};
        emit.emitLevel = kFlameEmit * flicker(time, d.light->seed, d.light->flicker);
        if (lightingMode_ == 3) emit.emitLevel *= flameGlow() / std::max(exposure_, 1e-9f);
        emit.emitAt = {d.art.flameFound ? d.art.flameU : 0.5f, d.art.flameFound ? d.art.flameV : 0.7f, 0.0f};
      }
      const uint32_t flags = flat_ ? kSpriteLit | kSpriteFlat : kSpriteLit | (d.castShadow ? kSpriteCastsShadow : 0u);
      addCard(clip, anchor, right, d.w, d.h, local, d.mirror, 0, flags, d.light ? &emit : nullptr);
    }
    // Units: the pose chosen in the panel from each unit's own catalog entry (its idle when it has none). They turn
    // to face the camera; Engine2's box for the pose, its foot line on the ground, the frame paced by Engine2's
    // framePace. One-shot poses (attacks, casts) play through, hold half a second, and start again.
    const std::string& chosen = poseChoices_.empty() ? kIdle : poseChoices_[static_cast<size_t>(poseChoice_)].first;
    glm::vec3 right = camera.right;
    right.y = 0.0f;
    right = glm::length(right) > 0.0f && !flat_ ? glm::normalize(right) : glm::vec3(1, 0, 0);
    hiddenUnits_ = 0;
    for (CardRect& c : unitCard_) c.drawn = false;
    for (size_t ui = 0; ui < scene_->units.size(); ++ui) {
      const e2::SceneUnit& u = scene_->units[ui];
      // Fog of war: a unit not in the party's sight is not drawn at all (and casts no shadow).
      if (fog_ && !noFog_ && u.side != "player") {
        const auto [c, r] = e2::hexAt(board_->layout(), unitPos_[ui].x, unitPos_[ui].y);
        // The states the shown mask was painted from (all hidden until the first mask arrives).
        const size_t i = static_cast<size_t>(e2::indexOf(board_->layout(), c, r));
        if (!litHex(i)) {
          ++hiddenUnits_;
          noteShown(ui, false, c, r, i);
          continue;
        }
        noteShown(ui, true, c, r, i);
      }
      // The scene's own idle unless the unit's catalog entry has the chosen pose. The mirror stays the scene's:
      // its side's facing against the unit's drawn direction, which is the same for every pose.
      // A walking unit shows Engine2's walk for its direction (UnitActor.walkShown), timed from the walk's start.
      const e2::Hero* entry = ui < unitCatalog_.size() ? unitCatalog_[ui] : nullptr;
      const bool walking = !unitWalk_[ui].points.empty();
      std::string shown = kIdle;
      const e2::HeroPose* p = nullptr;
      // The fallen: their death plays out, holds, then they are gone.
      if (ui < unitDiedAt_.size() && unitDiedAt_[ui] >= 0.0f && time - unitDiedAt_[ui] > poseSeconds(ui, "death") / unitShot_[ui].rate + kCorpseSeconds) continue;
      OneShot shotNow;  // this frame's (the stored one may move on to its next pose below)
      const OneShot* shot = nullptr;
      if (ui < unitShot_.size() && !unitShot_[ui].pose.empty()) shotNow = unitShot_[ui], shot = &shotNow;
      if (shot && entry) {
        // Engine2 UnitActor.play: the left-facing cut when it faces left and has one.
        const std::string left = shot->pose + "Left";
        const e2::HeroPose* l = unitFacing_[ui] == -1 ? entry->find(left) : nullptr;
        p = l && !l->pace.empty() ? l : entry->find(shot->pose);
        if (p && p->pace.empty()) p = nullptr;
        if (p) shown = p == l ? left : shot->pose;
        const bool over = shot->until >= 0.0f ? time > shot->until : time - shot->start > poseSeconds(ui, shot->pose) / shot->rate;
        if (!shot->hold && over) {
          OneShot& s = unitShot_[ui];
          if (!s.then.empty() && time < s.thenUntil) s = {s.then, time, false, s.rate, s.thenUntil};  // drawn from the next frame
          else s.pose.clear();
        }
      } else if (walking && entry) {
        p = walkPose(*entry, unitWalk_[ui].dir, unitFacing_[ui], u.sprite, shown);
      } else if (chosen != kIdle && entry) {
        p = entry->find(chosen);
        if (p && p->pace.empty()) p = nullptr;
        if (p) shown = chosen;
      }
      const SpriteClipId clip = p ? sprites_.find(p->set, p->clip) : sprites_.find(u.set, u.clip);
      if (clip == kNoSpriteClip) {
        ++missing_;
        if (ui < shownLast_.size() && shownLast_[ui] != 2) logWarn("unit {} not drawn: no sprite clip for pose '{}'", u.name, shown), shownLast_[ui] = 2;
        continue;
      }
      uint32_t frame = 0;
      if (p) {
        // A walk is timed by the ground covered, not by the clock, and the whole clip plays (nothing is cut): each
        // full stride (both legs) spans kWalkStride world units for every character, heroes and foes alike, and the clip
        // holds `strides` of them (walk-strides.json), so all step at one cadence (the user: everyone at the same pace;
        // a stride scaled by the card's height gave Voss 0.72 s a stride against Kael's 0.44). Malrec's clip holds 3, Aldric's 2.
        float walkClock = 0.0f;
        if (walking && !shot) {
          const double cycle = p->doneAt >= 0 ? static_cast<double>(p->doneAt) : paceLoop(p->pace, p->frames).period;
          walkClock = static_cast<float>(unitWalk_[ui].travelled / (p->strides * kWalkStride) * cycle / 240.0);
        }
        const float clock = shot ? std::max(0.0f, time - shot->start) * shot->rate : walking ? walkClock : time;
        if (shot || p->doneAt >= 0) {
          // A one-shot pose plays through once and keeps its last frame (replayed after 0.5 s when it is the chosen pose).
          const size_t tick = shot ? std::min(static_cast<size_t>(clock * 240.0f), p->doneAt >= 0 ? static_cast<size_t>(p->doneAt) : p->pace.size() - 1)
                                   : static_cast<size_t>(clock * 240.0f) % (static_cast<size_t>(p->doneAt) + 120);
          frame = static_cast<uint32_t>(p->pace[std::min(tick, p->pace.size() - 1)]);
        } else {
          frame = loopFrame(p->pace, p->frames, clock * 240.0);
        }
      } else if (!u.pace.empty()) {
        frame = loopFrame(u.pace, u.frames, time * 240.0);
      }
      float boxW = p ? p->boxW : u.boxW, boxH = p ? p->boxH : u.boxH;
      const float foot = p ? p->foot : u.foot;
      glm::vec3 anchor = static_cast<int>(ui) == hero_ && path_.size() > 1 ? heroAt_ : unitWorld(ui);
      // The hit react: knocked back along the blow, squashed (a little wider as it is shorter).
      float squash = 1.0f;
      anchor += recoilOffset(ui, time, boxH, squash) + leapOffset(ui, time);
      boxW *= 1.0f + (1.0f - squash) * 0.6f, boxH *= squash;
      // Engine2's mirroring: the art's facing against the unit's, except cuts drawn for the left (and directional
      // walks while moving), which are never mirrored.
      const bool leftCut = shown == "walkLeft" || shown == "attackLeft" || shown == "attack2Left" || shown == "castLeft" ||
                           (walking && kDirectionalWalk.contains(u.sprite));
      // Engine2 UnitActor.drawnReversed: Kael Final's idle is drawn turned left, but his walk and attacks face right
      // (and the old Kael's attacks), so those poses are not reversed: reading his idle for them walked him backwards.
      static const std::unordered_set<std::string> kAttackPoses = {"attack", "attackLeft", "attack2", "attack2Left", "attackShort",
                                                                   "cast", "castLeft", "heal", "counter"};
      const bool attackPose = kAttackPoses.contains(shown);
      const bool drawnRight = (u.sprite == "kaelFinal" && (walking || attackPose)) || (u.sprite == "defaultWarrior" && attackPose);
      const bool reversed = unitReversed_[ui] != 0 && !drawnRight;
      const bool mirror = !leftCut && ((unitFacing_[ui] == -1) != reversed);
      // Tática (Engine2 UnitActor.update with flatTop): flat over its own row's cards by half a row step, no shadow.
      if (flat_) anchor.y = boardTop_ + anchor.z * kFlatRowStep + kFlatRowStep * 0.5f;
      const uint32_t flags = flat_ ? kSpriteLit | kSpriteFlat : kSpriteLit | kSpriteFront | kSpriteCastsShadow;
      const size_t cardsBefore = drawList_.size();
      // Engine2 centerBody: a sheet that drifts sideways mid-swing (Neera's off-hand dagger) is held on its hex.
      const float drift = p && frame < p->bodyShift.size() ? p->bodyShift[frame] * boxW : 0.0f;
      // Engine2 UNIT_GLOW: art painted as glowing lights itself (the Cobalt Blue Deer's crystal antlers: colour, level 0.85, its
      // own mask in sprites.hlsl, flagged by a negative emitAt.z).
      // The familiars glow the same way (the user, 2026-10-18, Engine2's screenshot: the familiar bright, lit, among dark stone).
      SpriteInstance glow{};
      const bool glows = u.sprite == "cobalt-blue-deer" || u.classId == "familiar" || u.classId == "familiar2" ||
                         u.classId == "familiar3" || u.classId == "familiar4";
      if (glows) {
        glow.emitColor = {0.55f, 0.62f, 1.0f};
        glow.emitLevel = 0.85f * (lightingMode_ == 3 ? flameGlow() / std::max(exposure_, 1e-9f) : 1.0f);
      }
      addCard(clip, anchor, right, boxW, boxH, {-drift, -foot, 0.0f}, mirror, frame, flags, glows ? &glow : nullptr);
      if (glows && drawList_.size() > cardsBefore) drawList_.back().emitAt.z = -1.0f;
      // Effects cover every unit (the user, 2026-10-18: "tudo precisa desenhar na frente dos personagens"; Renderer draws
      // the units' depth only after the effects), the caster included while it faces us; a caster turned away (its
      // "castBack" still) stays in front of what leaves its hands: its card is drawn again over the effects.
      if (drawList_.size() > cardsBefore && shown == "castBack") frontCards_.push_back(drawList_.back());
      unitCard_[ui] = {anchor - (flat_ ? glm::vec3(0.0f, 0.0f, -foot) : glm::vec3(0.0f, foot, 0.0f)), boxW, boxH, flat_, true};
      {  // the body for picking: the frame's trimmed picture, its middle 60 % (addCard's placement of the content box)
        const SpriteClipHeader& ch = sprites_.header(clip);
        const float sw = static_cast<float>(ch.sourceWidth), sh = static_cast<float>(ch.sourceHeight);
        const float cx = (static_cast<float>(ch.cropX) + ch.contentWidth * 0.5f) / sw - 0.5f;
        const float cy = 1.0f - (static_cast<float>(ch.cropY) + ch.contentHeight) / sh;
        CardRect& cr = unitCard_[ui];
        cr.bodyX = (-drift + cx * boxW) * (mirror ? -1.0f : 1.0f);
        cr.bodyBottom = cy * boxH;
        cr.bodyTop = cr.bodyBottom + boxH * ch.contentHeight / sh;
        cr.bodyHalf = 0.3f * boxW * ch.contentWidth / sw;
      }
    }
    const glm::vec3 eye = camera.position, fwd = camera.forward;
    std::sort(drawList_.begin(), drawList_.end(),
              [&](const SpriteInstance& a, const SpriteInstance& b) { return glm::dot(a.origin - eye, fwd) > glm::dot(b.origin - eye, fwd); });
  }

  // Engine2's CarriedLight: three.js point light, decay 1.25, cut off at hypot(radius, height) x 2.5, colour x
  // intensity x 3.2 x flicker.
  LocalLight carried(glm::vec3 at, const std::array<float, 3>& color, float intensity, float radius, float height, float flick) const {
    LocalLight l;
    l.position = at;
    if (lightingMode_ == 3) {
      // Real light: Engine2's intensity and reach read as a luminous flux (lantern ~900 lm, fire ~1200 lm, forge
      // ~1700 lm, candle ~200 lm), an isotropic source of flux / 4pi candela falling off as 1 / d^2 (lux), its
      // range where it drops under 0.03 lux.
      const float reach = radius / 1.3f;  // Engine2 LIGHT_RADIUS_MUL is already in the scene's radius
      const float lumens = intensity * reach * reach * kLumensPerUnit;
      const float candela = lumens / (4.0f * std::numbers::pi_v<float>);
      l.color = glm::vec3(color[0], color[1], color[2]) * candela * flick;
      l.decay = 2.0f;
      l.range = std::clamp(std::sqrt(candela / 0.03f), 2.0f, 48.0f);
      return l;
    }
    l.range = std::hypot(radius, height) * 2.5f;
    l.color = glm::vec3(color[0], color[1], color[2]) * intensity * 3.2f * flick;
    l.decay = 1.25f;
    return l;
  }

  void buildLights(float time) {
    // A map light sits on its flame as the card draws it (the user, 2026-10-18: the lights were "completely off set"):
    // the card's axis and layer step (addCard), its picture's bottom edge, then up the card along the same lean as
    // sprites.hlsl VSMain when the cards lean to the camera; Engine2's floor of minHeight over the ground kept.
    glm::vec3 lean{0.0f, 1.0f, 0.0f};
    if (cardStance_ == 1) {
      const glm::vec3 f = lastCamera_.forward;
      const glm::vec3 v = glm::vec3(0.0f, 1.0f, 0.0f) - f * f.y;
      if (glm::dot(v, v) > 1e-6f) lean = glm::normalize(v);
    }
    for (const e2::SceneCard& d : scene_->cards) {
      if (!d.light) continue;
      const float yaw = d.lockedYaw.value_or(0.0f);
      const glm::vec3 right{std::cos(yaw), 0.0f, -std::sin(yaw)};
      const float height = std::max(d.light->minHeight, (d.art.flameV - d.art.baseV) * d.h);
      const float side = (d.art.flameU - d.art.baseU) * d.w * (d.mirror ? -1.0f : 1.0f);
      const glm::vec3 base = glm::vec3(d.x, groundOf(d.groundSmooth, d.groundHard), d.z);
      glm::vec3 at = base + right * side + glm::vec3(0, height, 0);
      const SpriteClipId clip = flat_ || !sprites_.ready() ? kNoSpriteClip : sprites_.find("decor", std::filesystem::path(d.image).stem().string());
      if (clip != kNoSpriteClip) {
        const SpriteClipHeader& c = sprites_.header(clip);
        const float bottom = 1.0f - (static_cast<float>(c.cropY) + c.contentHeight) / static_cast<float>(c.sourceHeight);
        const float flameV = d.art.flameFound ? d.art.flameV : 0.7f;
        const glm::vec3 facing{-right.z, 0.0f, right.x};
        at = base + right * side + facing * d.nudge + glm::vec3(0.0f, (bottom - d.art.baseV) * d.h, 0.0f) + lean * ((flameV - bottom) * d.h);
        at.y = std::max(at.y, base.y + d.light->minHeight);
      }
      lights_.push_back(carried(at, d.light->color, d.light->intensity, d.light->radius, height, flicker(time, d.light->seed, d.light->flicker)));
    }
    if (hero_ < 0) return;
    const e2::SceneUnit* hero = &scene_->units[static_cast<size_t>(hero_)];
    const glm::vec3 base = heroAt_;
    if (options_.movingLight) {
      // A torch (Engine2's NORMAL_FIRE: colour (1, 0.58, 0.26), intensity 8, radius 3.6, full flicker) carried
      // around the hero at hand height.
      const float a = time * 0.6f;
      const glm::vec3 at = base + glm::vec3(std::cos(a) * 2.5f, 1.6f, std::sin(a) * 2.5f);
      lights_.push_back(carried(at, {1.0f, 0.58f, 0.26f}, 8.0f, 3.6f, 1.6f, flicker(time, 1.7f, 1.0f)));
    }
    if (options_.spellLight) {
      // A violet arcane bolt from the hero to the nearest enemy: 1.2 s of flight, 0.5 s of impact flash.
      const e2::SceneUnit* target = nullptr;
      float best = 1e30f;
      for (const e2::SceneUnit& u : scene_->units) {
        if (u.side != "enemy") continue;
        const float d = glm::length(glm::vec2(u.x - base.x, u.z - base.z));
        if (d < best) best = d, target = &u;
      }
      if (target) {
        const glm::vec3 to{target->x, groundOf(target->groundSmooth, target->groundHard), target->z};
        const float cycle = std::fmod(time, 2.0f);
        const std::array<float, 3> violet{0.62f, 0.32f, 1.0f};
        if (cycle < 1.2f) {
          const float t = cycle / 1.2f;
          const glm::vec3 at = glm::mix(base, to, t) + glm::vec3(0, 1.8f + std::sin(t * 3.14159f) * 1.2f, 0);
          lights_.push_back(carried(at, violet, 10.0f, 3.5f, 1.8f, 1.0f));
        } else if (cycle < 1.7f) {
          const float k = 1.0f - (cycle - 1.2f) / 0.5f;
          lights_.push_back(carried(to + glm::vec3(0, 1.4f, 0), violet, 24.0f * k * k, 4.5f, 1.4f, 1.0f));
        }
      }
    }
  }

  // The moon tonight (ARCHITECTURE 7.5): it crosses the sky in Engine2's 600 s (moon.ts, stage.ts
  // MOON_CROSSING_SECONDS), starting at Engine2's 0.35 of the way. Engine2 drew the disc ahead of the normal view
  // but lit the board from the mirrored side (stage.ts moveMoon: light from ahead lit only the backs of units);
  // with a real sky the two cannot both hold, so three placements are kept, chosen in the panel until the user picks:
  //   0 Engine2's: disc ahead, low enough to show above the board; light from Engine2's side (26-43 deg up)
  //   1 high: disc and light together on Engine2's light (behind the normal view; the orbit camera never sees it)
  //   2 low ahead: disc and light together ahead and low (in view when the camera tilts low; units lit from behind)
  struct MoonPlacement {
    glm::vec3 light{0.0f, 1.0f, 0.0f}, disc{0.0f, 1.0f, 0.0f};  // towards each
  };
  MoonPlacement moonPlacement(float time) const {
    constexpr float kCrossingSeconds = 600.0f, kStart = 0.35f;
    // The low path (ours): the disc's lower edge 2 deg above the horizon at rise and set, 6 deg more at its highest,
    // so the whole disc always shows above the board, whatever its size.
    const float radius = glm::radians(kMoonSizes[moonSize_]);
    const float lowRise = radius + glm::radians(2.0f), lowHigh = radius + glm::radians(8.0f);
    const float progress = std::fmod(kStart + time / kCrossingSeconds, 1.0f);
    const e2::MoonPose pose = e2::moonPose(progress);
    const float height = std::sin(std::numbers::pi_v<float> * progress);
    float d[3];
    e2::lightDirection(pose.azimuth + std::numbers::pi_v<float>, pose.elevation, d);
    const glm::vec3 behind{d[0], d[1], d[2]};
    e2::lightDirection(pose.azimuth, lowRise + (lowHigh - lowRise) * height, d);
    const glm::vec3 lowAhead{d[0], d[1], d[2]};
    switch (moonPath_) {
      case 1: return {behind, behind};
      case 2: return {lowAhead, lowAhead};
      default: return {behind, lowAhead};
    }
  }

  // The map's light (ARCHITECTURE 7.5) in the chosen mode. Time of day picks sun or moon and where it stands;
  // the map's own sliders set the strengths (Engine2's 3D ignored them); the atmosphere colours the light.
  Renderer::Lighting mapLighting(float time) const {
    using e2::TimeOfDayLight;
    Renderer::Lighting l;
    if (lightingMode_ == 0) return l;  // the old prototype day
    const TimeOfDayLight& t = e2::timeOfDay(scene_ ? scene_->timeOfDay : std::string());
    const char* phase = moonPhase_ < 0 ? "full" : e2::kMoonPhases[moonPhase_];
    const e2::Moonlight moon = moonPhase_ < 0 ? e2::Moonlight{} : e2::moonlightFor(phase);
    MoonPlacement placed;
    if (t.moon) {
      placed = moonPlacement(time);
      // The light comes from the disc (the user, 2026-10-19: "a lua não aparenta emanar a luz ... as sombras na direção oposta
      // da lua": Engine2's preset lit from one side while the disc hung on another); with no disc, Engine2's side.
      if (!moonOff_) placed.light = placed.disc;
      l.sunDirection = placed.light;
    } else {
      float dir[3];
      e2::lightDirection(t.stageAzimuth, t.elevationDeg >= 0.0f ? glm::radians(t.elevationDeg) : t.stageElevation, dir);
      l.sunDirection = glm::normalize(glm::vec3(dir[0], dir[1], dir[2]));
    }
    if (lightingMode_ == 3 && cycleOn() && !(scene_ && scene_->environment == "indoor")) return cycleLighting(time, moon, phase);
    if (lightingMode_ == 3) return realLighting(t, moon, phase, placed, time);
    if (lightingMode_ == 1) {  // Engine2's 3D stage, as it drew (at night: the moon's light from Engine2's side)
      if (t.moon) l.sunDirection = glm::normalize(moonPlacement(time).light);
      l.sunColor = Renderer::srgb(t.moon && moon.color ? moon.color : t.stageSun);
      l.sunIntensity = t.stageSunIntensity * (t.moon ? 1.5f * moon.strength : 1.0f);  // MOON_BRIGHTNESS x tonight's phase
      l.skyColor = Renderer::srgb(t.moon && moon.sky ? moon.sky : t.stageSky);
      l.groundColor = Renderer::srgb(t.stageGround);
      l.ambientIntensity = t.stageHemi;
      return l;
    }
    const float key = (scene_ && scene_->sunIntensity ? *scene_->sunIntensity : t.key) * (t.moon ? moon.strength : 1.0f);
    const float ambient = scene_ && scene_->ambientIntensity ? *scene_->ambientIntensity : t.ambient;
    const glm::vec3 keyColor = t.moon ? Renderer::srgb(moon.color ? moon.color : t.keyColor) : glm::vec3(1.0f);
    l.sunIntensity = key;
    l.ambientIntensity = ambient;
    l.skyColor = Renderer::srgb(t.moon && moon.sky ? moon.sky : t.skyColor);
    if (scene_ && scene_->environment == "indoor") {  // no sky indoors: the tuned colours on the hemisphere
      l.sunColor = Renderer::srgb(t.moon && moon.color ? moon.color : t.keyColor);
      l.groundColor = Renderer::srgb(t.stageGround);
      return l;
    }
    // The key light through the air: its colour (luminance 1; the map's strength sets how bright).
    glm::vec3 c = atmosphereTransmittance(AtmosphereFrame{}, l.sunDirection) * keyColor;
    const float lum = 0.2126f * c.r + 0.7152f * c.g + 0.0722f * c.b;
    l.sunColor = lum > 1e-6f ? c / lum : keyColor;
    l.atmosphere = true;
    l.skyIlluminance = keyColor * key;
    l.skyScale = t.moon ? 0.04f : 1.0f;  // a moonlit sky is dark
    if (t.moon) {
      // Night: stars, no sun disc, and the moon's disc with tonight's art. Its size is Engine2's disc as seen from
      // its normal view (moon.ts RADIUS 0.14 board half-sizes, ~3 half-sizes from the camera: ~2.7 deg radius; the
      // real moon's 0.26 would leave the phase unreadable); its brightness Engine2's GLOW (2.3 x the art).
      l.sunDisc = false;
      l.stars = starsOff_ ? 0.0f : 1.0f;
      l.starLook = static_cast<uint32_t>(starLook_);
      l.moonDirection = placed.disc;
      l.moonTexture = ground_.textureIndex(e2::skyMoonAsset(phase));
      l.moonRadius = moonOff_ ? 0.0f : glm::radians(kMoonSizes[moonSize_]);  // off: no disc (its light stays: "sol / lua")
      l.moonGlow = 2.3f;
    }
    return l;
  }

  // Light mode "real" (ARCHITECTURE 7.6 step A): sun, sky, moon and lamps on one physical scale (lux), the map's
  // sliders as multipliers on it. The sun outside the air gives 128,000 lux; the atmosphere's transmittance takes its
  // share and colours it; the physical sky (Hillaire tables) lit by the same sun gives the sky's own ambient. A full moon
  // gives 0.5 lux (twice the real 0.25, the usual art lift so moonlit ground still reads), times tonight's phase.
  // Dawn and dusk put the sun low (6 and 4 degrees) so they look like dawn and dusk. Indoors: a dim fill (3 lux) and
  // ambient (1.5 lux), so the torches carry the room. The camera meters what lands on the board (meterLux_).
  Renderer::Lighting realLighting(const e2::TimeOfDayLight& t, const e2::Moonlight& moon, const char* phase, const MoonPlacement& placed,
                                 float /*time*/) const {
    Renderer::Lighting l;
    const float keyMul = scene_ && scene_->sunIntensity ? *scene_->sunIntensity / t.key : 1.0f;
    const float ambientMul = scene_ && scene_->ambientIntensity && t.ambient > 0.0f ? *scene_->ambientIntensity / t.ambient : 1.0f;
    const bool indoor = scene_ && scene_->environment == "indoor";
    moonNight_ = t.moon && !indoor;
    if (t.moon) {
      l.sunDirection = placed.light;
    } else {
      const std::string_view name = t.name;
      // aldeia's dusk sits lower (the user, 2026-10-19: "acertasse a luz do sol para fim do dia": at 4 deg the air left it 32,000 lux,
      // white, and the camera only 1.1 stops under the meter, a clear day): at 2.5 deg the air reddens and dims it, the shadows
      // lengthen and the camera darkens by itself. Every other map keeps its 4 deg (one map at a time).
      const float duskDegrees = aldeia() ? 2.5f : 4.0f;
      const float elevation = name == "dawn" ? glm::radians(6.0f) : name == "dusk" ? glm::radians(duskDegrees) : t.stageElevation;
      float dir[3];
      e2::lightDirection(t.stageAzimuth, elevation, dir);
      l.sunDirection = glm::normalize(glm::vec3(dir[0], dir[1], dir[2]));
    }
    if (options_.sunElevation >= 0.0f) {
      // --sun AZ EL: forced here, before the air's colour and the meter read the direction (2026-10-19: forced only after
      // mapLighting, a noon sun on the dusk map was exposed for dusk and the picture washed out).
      const float az = glm::radians(options_.sunAzimuth), el = glm::radians(options_.sunElevation);
      l.sunDirection = {std::cos(el) * std::cos(az), std::sin(el), std::cos(el) * std::sin(az)};
    }
    const glm::vec3 keyColor = t.moon ? Renderer::srgb(moon.color ? moon.color : t.keyColor) : glm::vec3(1.0f);
    const float moonLux = moonLight_ == 1 ? kCinemaMoonLux : kFullMoonLux;  // panel "Luar": físico / cinema
    const float outside = (t.moon ? moonLux * moon.strength : kSunLux) * keyMul;  // the key light above the air
    const float sinElevation = std::max(l.sunDirection.y, 0.0f);
    if (indoor) {
      l.sunColor = Renderer::srgb(t.moon && moon.color ? moon.color : t.keyColor);
      l.sunIntensity = kIndoorFillLux * keyMul;
      l.skyColor = Renderer::srgb(t.moon && moon.sky ? moon.sky : t.skyColor);
      l.groundColor = Renderer::srgb(t.stageGround);
      l.ambientIntensity = kIndoorAmbientLux * ambientMul;
      meterLux_ = l.sunIntensity * std::max(sinElevation, 0.3f) + l.ambientIntensity;
      return l;
    }
    // Through the air: the key light's colour (luminance 1) and how much of it reaches the ground.
    const glm::vec3 c = atmosphereTransmittance(AtmosphereFrame{}, l.sunDirection) * keyColor;
    const float lum = 0.2126f * c.r + 0.7152f * c.g + 0.0722f * c.b;
    l.sunColor = lum > 1e-6f ? c / lum : keyColor;
    l.sunIntensity = outside * lum;
    l.atmosphere = true;
    l.skyIlluminance = keyColor * outside;
    l.skyScale = 1.0f;
    l.physicalSky = true;
    l.ambientMul = ambientMul;
    l.ambientIntensity = 0.0f;  // (not read: the sky gives the ambient)
    // The meter: the key light on level ground plus the sky (about a fifth of the key that gets through).
    meterLux_ = l.sunIntensity * std::max(sinElevation, 0.05f) + 0.2f * l.sunIntensity * ambientMul;
    if (t.moon) {
      l.sunDisc = false;
      l.stars = starsOff_ ? 0.0f : 1.0f;
      l.starLook = static_cast<uint32_t>(starLook_);
      l.moonDirection = placed.disc;
      l.moonTexture = ground_.textureIndex(e2::skyMoonAsset(phase));
      l.moonRadius = moonOff_ ? 0.0f : glm::radians(kMoonSizes[moonSize_]);
      // The disc's radiance follows the moon's light (the physical sky's exposure is set by that light), never below
      // Engine2's glow: a bright disc under either "Luar" whose phase art still reads (at 0.9 x it burned to a white
      // "super moon": the user, 2026-10-19).
      l.moonGlow = std::max(2.3f, outside * 0.35f);
    }
    return l;
  }

  // Ciclo de 24 h (the user, 2026-10-19: "o melhor céu possível com 24 horas de transição"; the panel's "Hora", "do mapa" by default
  // so no map changes): the sun rises at 6, stands 62 deg high at noon and sets at 18 on a path sweeping the whole sky; the moon is
  // opposite it, also lighting the sky (the atmosphere's second light, Renderer::Lighting::moonIlluminance), so dusk glows out while
  // the moon rises and the stars come in with the dark (atmosphere.hlsli). The board's key light is the sun while it is up, the moon
  // after: the swap falls when both sit on the horizon and light almost nothing. The meter follows twilight (a tenth for every 2.4 deg
  // the sun sinks), so the camera opens up through dusk.
  // The sky follows an hour: a chosen one / a cycle (the panel's Hora), else the game's clock when the stage was given one.
  bool cycleOn() const { return dayCycle_ > 0 || gameHour_ >= 0.0f; }
  float cycleHour(float time) const {
    if (dayCycle_ == 0 && gameHour_ >= 0.0f) return gameHour_;
    // amanhecer / pôr do sol: the disc ~6 deg up (at 6.0 / 18.0 it sits half under the horizon and the camera, metering
    // the dark ground, blows the sky out: the user's "super bright", 2026-10-19); crepúsculo: the sun 6 deg down.
    static constexpr float kFixed[8] = {6.35f, 9.0f, 12.0f, 15.0f, 17.65f, 18.6f, 22.0f, 3.0f};
    if (dayCycle_ == 1) return std::fmod(6.0f + time * 24.0f / 120.0f, 24.0f);    // ciclo rápido: a day in 2 minutes
    if (dayCycle_ == 2) return std::fmod(6.0f + time * 24.0f / 1440.0f, 24.0f);   // ciclo lento: a day in 24 minutes
    return kFixed[std::clamp(dayCycle_ - 3, 0, 7)];
  }
  Renderer::Lighting cycleLighting(float time, const e2::Moonlight& moon, const char* phase) const {
    Renderer::Lighting l;
    const float pi = std::numbers::pi_v<float>;
    const float hour = cycleHour(time);
    const float elevation = glm::radians(62.0f) * std::sin(2.0f * pi * (hour - 6.0f) / 24.0f);
    const float azimuth = -0.95f + pi * (hour - 12.0f) / 12.0f;
    float d[3];
    e2::lightDirection(azimuth, elevation, d);
    const glm::vec3 sun = glm::normalize(glm::vec3(d[0], d[1], d[2]));
    e2::lightDirection(azimuth + pi, -elevation * 0.9f, d);
    const glm::vec3 moonDir = glm::normalize(glm::vec3(d[0], d[1], d[2]));
    const glm::vec3 moonColor = Renderer::srgb(moon.color ? moon.color : 0x9fb4ffu);
    const float moonLux = (moonLight_ == 1 ? kCinemaMoonLux : kFullMoonLux) * moon.strength;
    const auto through = [](glm::vec3 dir, glm::vec3 colour, float& lum) {
      const glm::vec3 c = atmosphereTransmittance(AtmosphereFrame{}, dir) * colour;
      lum = 0.2126f * c.r + 0.7152f * c.g + 0.0722f * c.b;
      return lum > 1e-6f ? c / lum : colour;
    };
    l.atmosphere = true;
    l.physicalSky = true;
    l.ambientMul = 1.0f;
    l.ambientIntensity = 0.0f;
    l.skyScale = 1.0f;
    l.skySunDirection = sun;
    l.skyIlluminance = glm::vec3(kSunLux);
    // The night sky reads deep navy (the ground stays moonlit): the moon lights the sky at a tenth of its key, so the exposure
    // that shows the moonlit board does not turn the night sky into a blue day (measured 2026-10-19: at full share it did).
    l.moonIlluminance = moonColor * moonLux * 0.1f;
    l.sunDisc = true;  // the stars come out as the sun goes down (atmosphere.hlsli)
    l.stars = 0.0f;
    l.starLook = static_cast<uint32_t>(starLook_);
    const bool day = sun.y > 0.0f;
    moonNight_ = !day;
    float sunLum = 0.0f, moonLum = 0.0f;
    const glm::vec3 sunColor = through(day ? sun : glm::normalize(glm::vec3(sun.x, 0.001f, sun.z)), glm::vec3(1.0f), sunLum);
    const glm::vec3 moonLit = through(moonDir.y > 0.0f ? moonDir : glm::normalize(glm::vec3(moonDir.x, 0.001f, moonDir.z)), moonColor, moonLum);
    // The meter's floor at the horizon: the light of a sun 2 deg up (through the air at exactly 0 deg almost nothing arrives:
    // the meter read ~30 lux at 18:00 and the camera blew the sunset sky out to white).
    float horizonLum = 0.0f;
    through(glm::normalize(glm::vec3(sun.x, 0.035f, sun.z)), glm::vec3(1.0f), horizonLum);
    const float dayMeterAtHorizon = kSunLux * horizonLum * 0.25f;
    if (day) {
      l.sunDirection = sun;
      l.sunColor = sunColor;
      l.sunIntensity = kSunLux * sunLum;
      meterLux_ = std::max(l.sunIntensity * std::max(sun.y, 0.05f) + 0.2f * l.sunIntensity, dayMeterAtHorizon);
    } else {
      l.sunDirection = moonDir.y > 0.05f ? moonDir : glm::normalize(glm::vec3(moonDir.x, 0.05f, moonDir.z));
      l.sunColor = moonLit;
      l.sunIntensity = moonLux * moonLum;
      const float twilight = dayMeterAtHorizon * std::pow(10.0f, glm::degrees(elevation) / 2.4f);
      meterLux_ = twilight + l.sunIntensity * (std::max(moonDir.y, 0.05f) + 0.2f);
    }
    if (moonDir.y > -0.05f) {
      l.moonDirection = moonDir;
      l.moonTexture = ground_.textureIndex(e2::skyMoonAsset(phase));
      l.moonRadius = moonOff_ ? 0.0f : glm::radians(kMoonSizes[moonSize_]);
      l.moonGlow = std::max(2.3f, moonLux * 0.35f);
    }
    return l;
  }

  // How many stops darker than the meter the camera exposes (Unreal's exposure compensation curve by scene
  // brightness): none in full daylight, up to 0.75 as the sun sinks (dawn, dusk), then 2.5 by a moonlit night and more
  // on a darker one, so dusk reads as dusk and night as night, not as a dim day.
  // Dusk: 2 stops under the incident reading (the user, 2026-10-19: "a claridade no céu não condiz com pôr do sol": metered on
  // the ground the camera opened 6 stops from noon while the sky had fallen 3, and the sunset sky came out paler than the
  // day's; a camera at sunset exposes for the sky and lets the ground go into silhouette).
  static float nightDarkening(float ev100) {
    return 2.0f * std::clamp((14.0f - ev100) / 4.0f, 0.0f, 1.0f) + kNightStops * std::clamp((10.0f - ev100) / 12.5f, 0.0f, 1.3f);
  }
  // The painted flames' glow on screen: faint in daylight, full at night (from the camera's EV).
  float flameGlow() const {
    if (std::isnan(adaptedEv_)) return 1.0f;
    return glm::mix(0.15f, 1.0f, std::clamp((12.0f - adaptedEv_) / 12.0f, 0.0f, 1.0f));
  }

  // Where the party stands (the swirl turns round it): the middle of the living player units.
  glm::vec2 partyCenter() const {
    if (!scene_) return {boardCenter_.x, boardCenter_.z};
    glm::vec2 sum{0.0f};
    int n = 0;
    for (size_t i = 0; i < scene_->units.size(); ++i) {
      const e2::SceneUnit& u = scene_->units[i];
      if (u.side != "player") continue;
      sum += static_cast<int>(i) == hero_ ? glm::vec2(heroAt_.x, heroAt_.z) : unitPos_[i];
      ++n;
    }
    return n ? sum / static_cast<float>(n) : glm::vec2(boardCenter_.x, boardCenter_.z);
  }

  // The map's units find their own catalog entry by key. The panel's poses: those at least one of the map's units
  // has, in the catalog's order of first appearance with its label, the idle ("Parado") first.
  void setUpPoses(const std::filesystem::path& path) {
    std::optional<std::vector<e2::Hero>> catalog = e2::loadHeroes(path);
    if (!catalog) return;
    catalog_ = std::move(*catalog);
    std::unordered_map<std::string, const e2::Hero*> byKey;
    for (const e2::Hero& h : catalog_)
      if (!h.key.empty()) byKey.emplace(h.key, &h);
    unitCatalog_.assign(scene_->units.size(), nullptr);
    std::vector<std::string> unknown;
    for (size_t i = 0; i < scene_->units.size(); ++i) {
      const auto it = byKey.find(scene_->units[i].key);
      if (it != byKey.end()) unitCatalog_[i] = it->second;
      else unknown.push_back(scene_->units[i].name + " (" + scene_->units[i].key + ")");
      // Art EmberEngine replaced (content/sprite-overrides.json "sets", e.g. Voss cut from video): the scene file still
      // names Engine2's idle, so the unit takes the catalog's.
      const e2::HeroPose* idle = unitCatalog_[i] ? unitCatalog_[i]->find(kIdle) : nullptr;
      e2::SceneUnit& su = scene_->units[i];
      if (idle && !idle->pace.empty() && idle->set != su.set) {
        su.set = idle->set, su.clip = idle->clip, su.boxW = idle->boxW, su.boxH = idle->boxH, su.foot = idle->foot;
        su.frames = idle->frames, su.pace = idle->pace, su.natW = su.natH = 0;
      }
    }
    for (const e2::Hero& h : catalog_) {
      if (std::find(unitCatalog_.begin(), unitCatalog_.end(), &h) == unitCatalog_.end()) continue;
      for (const e2::HeroPose& p : h.poses)
        if (std::none_of(poseChoices_.begin(), poseChoices_.end(), [&](const auto& q) { return q.first == p.pose; }))
          poseChoices_.emplace_back(p.pose, p.label.empty() ? p.pose : p.label);
    }
    const auto idle = std::find_if(poseChoices_.begin(), poseChoices_.end(), [](const auto& q) { return q.first == kIdle; });
    if (idle != poseChoices_.end()) std::rotate(poseChoices_.begin(), idle, idle + 1);
    else if (!poseChoices_.empty()) poseChoices_.insert(poseChoices_.begin(), {kIdle, "Parado"});
    logInfo("poses: {} of {} map units in the catalog, {} poses between them", scene_->units.size() - unknown.size(), scene_->units.size(),
            poseChoices_.size());
    for (const std::string& u : unknown) logWarn("poses: {} not in the catalog: idle only", u);
  }

  // Every pose the map's units can play is loaded before the fight opens and held to its end (the user, 2026-10-18: a pose
  // streamed in on its first frame drew from the 64 px tail, a blur at every change of animation). The cast with two
  // versions (content/sprite-quality.json) follows Opções > Gráficos > Personagens: Alta (full resolution) / Leve (half).
  void pinUnitArt() {
    sprites_.setLight(gamePrefs().characters == "leve");
    for (size_t i = 0; i < scene_->units.size(); ++i) pinArt(scene_->units[i], i < unitCatalog_.size() ? unitCatalog_[i] : nullptr);
  }
  void pinArt(const e2::SceneUnit& u, const e2::Hero* entry) {
    const bool two = twoVersionCast().contains(entry ? entry->key : u.key);
    sprites_.pin(u.set, u.clip, two);
    if (entry)
      for (const e2::HeroPose& p : entry->poses) sprites_.pin(p.set, p.clip, two);
  }
  // The battle HUD's pictures (every unit's portrait, each hero's 12 sockets: spell art up to 784 x 1168) decoded on a
  // worker during the loading screen (the hunt for the battle hitch, 2026-10-19: read on the main thread the first time the
  // bar showed them, 45-90 ms).
  void preloadHud() {
    if (!battle_) return;
    std::vector<std::filesystem::path> files;
    const auto add = [&](const std::filesystem::path& f) {
      if (!f.empty() && !uiImageLoaded(f) && std::find(files.begin(), files.end(), f) == files.end()) files.push_back(f);
    };
    for (size_t b = 0; b < battle_->units().size(); ++b) {
      const e2::BattleUnit& u = battle_->units()[b];
      add(portraitFileOf(u));
      if (u.side == "player")
        for (const std::string& kind : hotbarOf(b))
          if (!kind.empty()) add(spellIconFileOf(kind));
    }
    if (!files.empty()) hudPreload_ = std::make_unique<UiPreload>(std::move(files));
  }

  // The catalog keys with Alta / Leve sprites: content/sprite-quality.json's cast, villains and bosses.
  static const std::unordered_set<std::string>& twoVersionCast() {
    static const std::unordered_set<std::string> keys = [] {
      std::unordered_set<std::string> out;
      std::ifstream in("content/sprite-quality.json", std::ios::binary);
      const nlohmann::json j = in ? nlohmann::json::parse(in, nullptr, false) : nlohmann::json();
      if (!j.is_object()) logWarn("sprites: content/sprite-quality.json missing or damaged: nobody has two versions");
      for (const char* list : {"cast", "villains", "bosses"})
        if (j.is_object() && j.contains(list) && j[list].is_array())
          for (const nlohmann::json& k : j[list])
            if (k.is_string()) out.insert(k.get<std::string>());
      return out;
    }();
    return keys;
  }

  // Every scene unit's live state: where it stands, its facing, its art's own facing (from the scene's mirror rule).
  void initUnits() {
    const size_t n = scene_->units.size();
    unitPos_.resize(n), unitMoved_.assign(n, 0), unitFacing_.resize(n), unitReversed_.resize(n);
    unitWalk_.assign(n, {}), unitCard_.assign(n, {}), unitBattle_.assign(n, -1);
    unitShot_.assign(n, {}), unitDiedAt_.assign(n, -1.0f), unitRecoil_.assign(n, {}), unitLeap_.assign(n, {});
    for (size_t i = 0; i < n; ++i) {
      const e2::SceneUnit& u = scene_->units[i];
      unitPos_[i] = {u.x, u.z};
      unitFacing_[i] = u.facing < 0 ? -1 : 1;
      unitReversed_[i] = u.mirror != (u.facing == -1) ? 1 : 0;
    }
  }

  // A familiar that stepped out of a summoning (Engine2 castSummonFamiliar pushes it into the units): it gets a scene unit with its
  // class's own art (the catalog entry keyed by its class id) and every per-unit array grows by one.
  void syncSummons() {
    if (!battle_ || !scene_) return;
    for (size_t b = 0; b < battle_->units().size(); ++b) {
      if (std::find(unitBattle_.begin(), unitBattle_.end(), static_cast<int>(b)) != unitBattle_.end()) continue;
      const e2::BattleUnit& bu = battle_->units()[b];
      if (!bu.summoned || !bu.alive || bu.side != "player") continue;
      const e2::Hero* entry = nullptr;
      for (const e2::Hero& h : catalog_)
        if (h.key == bu.classId) entry = &h;
      const e2::HeroPose* idle = entry ? entry->find("idle") : nullptr;
      if (!idle) {
        logWarn("summon: {} has no art in the catalog ({})", bu.name, bu.classId);
        continue;
      }
      e2::SceneUnit su;
      su.side = bu.side, su.name = bu.name, su.classId = bu.classId, su.key = bu.classId, su.sprite = bu.sprite;
      su.set = idle->set, su.clip = idle->clip, su.boxW = idle->boxW, su.boxH = idle->boxH, su.foot = idle->foot;
      su.frames = idle->frames, su.pace = idle->pace, su.facing = 1, su.mirror = entry->reversed;
      const glm::vec2 at = anchorWorld(bu, {bu.x, bu.y});
      su.x = at.x, su.z = at.y;
      pinArt(su, entry);
      scene_->units.push_back(std::move(su));
      unitCatalog_.push_back(entry);
      unitPos_.push_back(at);
      unitMoved_.push_back(1);
      unitFacing_.push_back(1);
      unitReversed_.push_back(entry->reversed ? 1 : 0);
      unitWalk_.push_back({});
      unitCard_.push_back({});
      unitBattle_.push_back(static_cast<int>(b));
      unitShot_.push_back({});
      unitDiedAt_.push_back(-1.0f);
      unitRecoil_.push_back({});
      while (unitSounds_.size() <= b) unitSounds_.push_back(e2::Json::object());
    }
  }

  bool hasEmberProps() const {
    return std::any_of(board_->draft().decorations.begin(), board_->draft().decorations.end(),
                       [](const e2::MapDecoration& d) { return d.id.starts_with("ember-"); });
  }
  // EmberEngine's own props set to "Bloquear caminho": export_battle.ts (Engine2's data.ts) has no footprint for them, so
  // their cells are made solid here as Engine2's hexprops makes a solid decoration's (impassable, cost 99, stops shots).
  void blockEmberProps() {
    const e2::EditorCatalog& catalog = architectureCatalog();
    const e2::HexLayout& L = board_->layout();
    size_t cells = 0;
    for (const e2::MapDecoration& d : board_->draft().decorations) {
      if (!d.blocksPath || !d.id.starts_with("ember-")) continue;
      const e2::CatalogDecoration* cd = catalog.decoration(d.id);
      if (!cd) continue;
      for (const e2::FootOffset& f : e2::placedFootprint(*cd, d.x, d.y, d.rot)) {
        const int x = d.x + f.dx, y = d.y + f.dy;
        if (!e2::inBoard(L, x, y)) continue;
        e2::BattleCell c = battle_->cell(x, y);
        c.passable = false, c.moveCost = 99, c.blocked = true, c.shot.blocksShot = true;
        battle_->setCell(y * L.cols + x, c);
        ++cells;
      }
    }
    if (cells) logInfo("battle: {} cells made solid under EmberEngine props", cells);
  }
  // The battle: each scene unit finds its battle unit by side and name, in spawn order.
  void setUpBattle(const std::filesystem::path& path) {
    battle_ = e2::Battle::load(path);
    loadSounds(path);
    if (!battle_) return;
    blockEmberProps();
    explores_ = battle_->explore();
    // Every map with fog of war is explored freely until the party sees an enemy (the user, 2026-10-17), like an explore mission.
    roams_ = explores_ || fog_.has_value();
    if (roams_ && !battle_->roaming()) battle_->resumeRoaming();
    battle_->setWaypointsOnly(roams_);  // and it ends only at a waypoint: no "Encerrar missão" when every foe is down
    arrangeBattle();
    if (options_.partyRoster) {  // the same on the battle's units (their sounds follow them), each hero once
      std::vector<e2::BattleUnit>& units = battle_->units();
      while (unitSounds_.size() < units.size()) unitSounds_.push_back(e2::Json::object());
      std::set<std::string> seen;
      for (size_t b = 0; b < units.size();) {
        const e2::BattleUnit& u = units[b];
        const bool leaves = (u.side == "player" && !u.summoned && (!options_.partyRoster->contains(u.name) || !seen.insert(u.name).second)) ||
                            (u.side == "neutral" && options_.partyRoster->contains(u.name));
        if (!leaves) {
          ++b;
          continue;
        }
        units.erase(units.begin() + static_cast<std::ptrdiff_t>(b));
        unitSounds_.erase(unitSounds_.begin() + static_cast<std::ptrdiff_t>(b));
      }
    }
    routeWispCrossing();
    for (e2::BattleUnit& u : battle_->units())
      if (const auto it = options_.heroProgress.find(u.name); it != options_.heroProgress.end() && u.side == "player")
        e2::raiseToLevel(u, it->second.first, it->second.second);
    // Promotions (the save's, or --promote): the hero's class, class name and spell list; every unit of a promoted class
    // (Engine2's own Bruxo enemies too) knows our spells of its level.
    for (e2::BattleUnit& u : battle_->units()) {
      if (const auto it = options_.heroClass.find(u.name); it != options_.heroClass.end() && u.side == "player") e2::setPromotion(u, it->second);
      e2::grantClassSpells(u);
    }
    battle_->setAffinityScores(options_.affinityScores);  // the save's party affinities (none: no bonus yet)
    // The save's condition of each hero (Engine2 spawnUnit: hunger and disease off its stats, HP capped, its bag).
    for (e2::BattleUnit& u : battle_->units()) {
      const auto it = options_.heroState.find(u.name);
      if (it == options_.heroState.end() || u.side != "player") continue;
      applyHeroState(u, it->second);
    }
    // A dungeon (GameApp startBattle + the BattleEngine constructor): the enemies and neutrals killed on an earlier visit stay dead
    // (save.crossingDefeatedSpawns, by spawn id), and after a floor connector the heroes who fell on the floor before stay out of
    // this one (preserveKnockouts: their carried HP is 0). Both are gone from the start: not drawn, no turn, no body.
    for (e2::BattleUnit& u : battle_->units()) {
      bool gone = u.side != "player" && options_.defeatedSpawns.contains(u.id);
      if (u.side == "player" && options_.preserveKnockouts)
        if (const auto it = options_.heroState.find(u.name); it != options_.heroState.end() && it->second.hp && *it->second.hp == 0) gone = true;
      if (!gone) continue;
      u.alive = false, u.hp = 0;
      absent_.insert(u.id);
    }
    if (!absent_.empty()) logInfo("battle: {} units absent from the start (killed on an earlier visit, or fell on the floor before)", absent_.size());
    if (battle_->cols() != board_->layout().cols || battle_->rows() != board_->layout().rows) {
      logError("battle {}: {}x{} does not fit the map's {}x{} board", path.string(), battle_->cols(), battle_->rows(), board_->layout().cols,
               board_->layout().rows);
      battle_.reset();
      return;
    }
    std::vector<uint8_t> taken(battle_->units().size(), 0);
    size_t matched = 0;
    for (size_t i = 0; i < scene_->units.size(); ++i)
      for (size_t b = 0; b < battle_->units().size(); ++b)
        if (!taken[b] && battle_->units()[b].side == scene_->units[i].side && battle_->units()[b].name == scene_->units[i].name) {
          unitBattle_[i] = static_cast<int>(b), taken[b] = 1, ++matched;
          break;
        }
    logInfo("battle {}: {} of {} units in play", battle_->map(), matched, scene_->units.size());
    for (size_t i = 0; i < unitBattle_.size(); ++i)
      if (unitBattle_[i] >= 0 && absent_.contains(battle_->units()[static_cast<size_t>(unitBattle_[i])].id)) unitDiedAt_[i] = -1000.0f;
    findConversations();
    if (!options_.arrivedFrom.empty()) arriveAtConnector();
    else if (options_.formation && !explores_) placeFormation();
    for (e2::BattleUnit& u : battle_->units()) e2::applyEmberInitiative(u);  // our bonuses, not Engine2's (world/Initiative.h)
    rng_ = e2::Mulberry32(static_cast<uint32_t>(std::chrono::steady_clock::now().time_since_epoch().count()));
    battle_->rollInitiative(rng_);
    for (const e2::BattleUnit& u : battle_->units()) startLevel_.emplace_back(u.level, u.xp);
    if (!options_.battleScript.empty()) holdTurns = true;  // --battle-script plays every action itself
  }

  int sceneUnitOf(int battleUnit) const {
    for (size_t i = 0; i < unitBattle_.size(); ++i)
      if (unitBattle_[i] == battleUnit && battleUnit >= 0) return static_cast<int>(i);
    return -1;
  }

  // Where a unit's feet are now: its spawn's measured ground until it moves, then the ground under it.
  glm::vec3 unitWorld(size_t i) const {
    const e2::SceneUnit& u = scene_->units[i];
    const glm::vec2 p = unitPos_[i];
    const float y = unitMoved_[i] ? static_cast<float>(e2::groundHeightAt(*board_, look_, p.x, p.y)) : groundOf(u.groundSmooth, u.groundHard);
    return {p.x, y, p.y};
  }

  // Where a battle unit's feet stand with its anchor on a hex: the middle of its body's front row (Ember's footprint
  // centroid, as the map importer places the spawns).
  glm::vec2 anchorWorld(const e2::BattleUnit& u, e2::BattleHex at) const {
    glm::vec2 sum{0.0f};
    int n = 0;
    for (const e2::BattleHex& p : e2::Battle::footprint(u, at.x, at.y)) {
      if (p.y != at.y || !battle_->inBounds(p.x, p.y)) continue;
      const e2::Cell& c = board_->cell(p.x, p.y);
      sum += glm::vec2(static_cast<float>(c.x), static_cast<float>(c.z));
      ++n;
    }
    if (!n) {
      const e2::Cell& c = board_->cell(at.x, at.y);
      return {static_cast<float>(c.x), static_cast<float>(c.z)};
    }
    return sum / static_cast<float>(n);
  }

  // Engine2 UnitActor.update for walking units: one hex every 0.22 s; facing and walk direction relative to the camera.
  void updateUnits(float dt) {
    if (!scene_) return;
    const float yaw = flat_ ? 0.0f : cam_.yaw;
    for (size_t i = 0; i < unitWalk_.size(); ++i) {
      Walk& w = unitWalk_[i];
      if (w.points.empty()) continue;
      glm::vec2& p = unitPos_[i];
      const glm::vec2 d = w.points.front() - p;
      const float dist = glm::length(d);
      const float screenX = d.x * std::cos(yaw) - d.y * std::sin(yaw);
      if (std::abs(screenX) > 0.05f) unitFacing_[i] = screenX > 0.0f ? 1 : -1;
      const float away = -(d.x * std::sin(yaw) + d.y * std::cos(yaw));
      if (dist > 1e-4f) w.dir = std::abs(away) > dist * 0.5f ? (away > 0.0f ? kWalkBack : kWalkFront) : kWalkSide;
      const float move = std::min(dist, std::sqrt(3.0f) / stepSeconds() * dt);
      if (dist > 1e-4f) p += d / dist * move;
      w.travelled += move;
      unitMoved_[i] = 1;
      if (dist - move < 1e-3f) {
        w.points.erase(w.points.begin());
        if (!w.hexes.empty()) w.hexes.erase(w.hexes.begin());
      }
      if (w.points.empty() && !w.fadeSound.empty()) AudioMixer::get().fadeSfx(std::exchange(w.fadeSound, {}));
    }
    // A hero's level-up (levelUp: its cue once per level gained).
    if (battle_) {
      std::vector<e2::BattleUnit>& all = battle_->units();
      levelSeen_.resize(all.size(), -1);
      for (size_t b = 0; b < all.size(); ++b) {
        if (levelSeen_[b] >= 0 && all[b].level > levelSeen_[b] && all[b].side == "player") playCues(levelUpSound_);
        levelSeen_[b] = all[b].level;
      }
    }
    if (hero_ >= 0 && path_.size() <= 1) heroAt_ = unitWorld(static_cast<size_t>(hero_));  // the test lights follow it
  }

  // Engine2 UnitActor.walkShown: a still for the direction, else the up / down cycle, else the side walk (its own left
  // cut when facing left; the Lancer's cuts are swapped).
  static const e2::HeroPose* walkPose(const e2::Hero& h, int dir, int facing, const std::string& sprite, std::string& shown) {
    auto has = [&](const char* pose) -> const e2::HeroPose* {
      const e2::HeroPose* p = h.find(pose);
      if (!p || p->pace.empty()) return nullptr;
      shown = pose;
      return p;
    };
    static const char* kStill[3] = {"walkBack", "walkFront", "walkSide"};
    if (const e2::HeroPose* p = has(kStill[dir])) return p;
    if (dir == kWalkBack)
      if (const e2::HeroPose* p = has("walkUp")) return p;
    if (dir == kWalkFront)
      if (const e2::HeroPose* p = has("walkDown")) return p;
    const bool wantLeft = sprite == "lancer" ? facing == 1 : facing == -1;
    if (wantLeft)
      if (const e2::HeroPose* p = has("walkLeft")) return p;
    if (const e2::HeroPose* p = has("walk")) return p;
    shown = kIdle;
    return nullptr;
  }

  // The hex under the cursor: a drawn unit's card first (nearest wins), else the ground the view ray meets.
  // A card's up as drawn (sprites.hlsl VSMain): flat in Tática; vertical, or leaning back with the camera's tilt
  // (panel "Cards": para a câmera) so it stands upright on screen; for picking, the health bars, the popups.
  struct CardRect;
  glm::vec3 cardUp(const CardRect& c) const {
    if (c.flat) return {0.0f, 0.0f, -1.0f};
    if (cardStance_ == 1) {
      const glm::vec3 f = lastCamera_.forward;
      const glm::vec3 v = glm::vec3(0.0f, 1.0f, 0.0f) - f * f.y;
      if (glm::dot(v, v) > 1e-6f) return glm::normalize(v);
    }
    return {0.0f, 1.0f, 0.0f};
  }

  // `cards` false: the ground under the cursor only (a transparent corner of a tall card must not steal a walk click).
  std::optional<e2::BattleHex> pick(SDL_WindowID id, float mx, float my, bool cards = true) const {
    int w = 0, h = 0;
    SDL_Window* window = SDL_GetWindowFromID(id);
    if (!window || !SDL_GetWindowSize(window, &w, &h) || w <= 0 || h <= 0) return std::nullopt;
    return pickAt(w, h, mx, my, cards);
  }
  std::optional<e2::BattleHex> pickAt(int w, int h, float mx, float my, bool cards = true) const {
    const ImVec2 cursor{mx, my};
    lastPickPoint_.reset();
    float best = 1e30f;
    std::optional<e2::BattleHex> hit;
    const glm::vec3 right = glm::length(glm::vec3(lastCamera_.right.x, 0.0f, lastCamera_.right.z)) > 0.0f && !flat_
                                ? glm::normalize(glm::vec3(lastCamera_.right.x, 0.0f, lastCamera_.right.z))
                                : glm::vec3(1.0f, 0.0f, 0.0f);
    for (size_t i = 0; cards && i < unitCard_.size(); ++i) {
      const CardRect& c = unitCard_[i];
      if (!c.drawn || (unitBattle_[i] < 0 && !(options_.editorClick && scene_ && i < scene_->units.size()))) continue;
      const glm::vec3 up = cardUp(c);
      // The figure's body, not the whole card (most of a card is air): the trimmed picture's middle band.
      const glm::vec3 mid = c.origin + right * c.bodyX;
      const glm::vec3 corners[4] = {mid - right * c.bodyHalf + up * c.bodyBottom, mid + right * c.bodyHalf + up * c.bodyBottom,
                                    mid + right * c.bodyHalf + up * c.bodyTop, mid - right * c.bodyHalf + up * c.bodyTop};
      ImVec2 q[4];
      bool ok = true;
      for (int k = 0; k < 4 && ok; ++k)
        if (const std::optional<ImVec2> s = screenOf(corners[k], static_cast<float>(w), static_cast<float>(h))) q[k] = *s;
        else ok = false;
      if (!ok || !insideQuad(q, cursor)) continue;
      const float d = glm::length(c.origin - lastCamera_.position);
      if (d >= best) continue;
      best = d;
      if (unitBattle_[i] >= 0) {
        hit = e2::BattleHex{battle_->units()[static_cast<size_t>(unitBattle_[i])].x, battle_->units()[static_cast<size_t>(unitBattle_[i])].y};
      } else {  // the map editor's preview (no battle): the hex the unit stands on
        const auto [col, row] = e2::hexAt(board_->layout(), scene_->units[i].x, scene_->units[i].z);
        hit = e2::BattleHex{col, row};
      }
    }
    if (hit) return hit;
    // The ground: march the view ray down from just above the highest ground, then refine where it goes under.
    const float tanHalf = std::tan(glm::radians((flat_ ? kFlatFov : kFov) * 0.5f));
    const float nx = 2.0f * mx / static_cast<float>(w) - 1.0f, ny = 1.0f - 2.0f * my / static_cast<float>(h);
    const glm::vec3 camUp = glm::cross(lastCamera_.right, lastCamera_.forward);
    const glm::vec3 dir = glm::normalize(lastCamera_.forward + lastCamera_.right * (nx * tanHalf * aspect_) + camUp * (ny * tanHalf));
    if (dir.y > -1e-4f) return std::nullopt;
    const glm::vec3 o = lastCamera_.position;
    // Board::bounds() is the rectangle of the hex CENTRES: the outer half of every edge hex lies beyond it, so a click on
    // the board's rim (and a centre on the bound itself) found no ground and could not send a unit there. The whole edge
    // hexes count: half a hex's width on the sides, a radius above and below.
    const e2::Board::Bounds b = board_->bounds();
    const double padX = e2::kSqrt3 * e2::kHexRadius * 0.5 + 0.02, padZ = e2::kHexRadius - 0.6 + 0.02;
    auto under = [&](float t) {
      const glm::vec3 p = o + dir * t;
      if (p.x < b.x0 - padX || p.x > b.x1 + padX || p.z < b.z0 - padZ || p.z > b.z1 + padZ) return false;
      return p.y <= static_cast<float>(board_->cellAt(p.x, p.z).groundY);
    };
    float t = std::max(0.0f, (o.y - (boardTop_ + 2.0f)) / -dir.y);
    const float end = (o.y - (groundLow_ - 2.0f)) / -dir.y;
    for (; t < end; t += 0.15f)
      if (under(t)) {
        float lo = t - 0.15f, hi = t;
        for (int k = 0; k < 12; ++k) (under((lo + hi) * 0.5f) ? hi : lo) = (lo + hi) * 0.5f;
        const glm::vec3 p = o + dir * hi;
        lastPickPoint_ = p;  // the map editor's water brush: the exact ground point clicked
        const auto [c, r] = e2::hexAt(board_->layout(), p.x, p.z);
        if (c < 0 || r < 0 || c >= board_->layout().cols || r >= board_->layout().rows) return std::nullopt;
        return e2::BattleHex{c, r};
      }
    return std::nullopt;
  }

  static bool insideQuad(const ImVec2 (&q)[4], ImVec2 p) {
    int sign = 0;
    for (int k = 0; k < 4; ++k) {
      const ImVec2 a = q[k], b = q[(k + 1) % 4];
      const float cross = (b.x - a.x) * (p.y - a.y) - (b.y - a.y) * (p.x - a.x);
      const int s = cross > 0.0f ? 1 : cross < 0.0f ? -1 : 0;
      if (s && sign && s != sign) return false;
      if (s) sign = s;
    }
    return true;
  }

  // A world point on screen (window coordinates), or none behind the eye.
  std::optional<ImVec2> screenOf(glm::vec3 p, float w, float h) const {
    const glm::vec4 clip = lastCamera_.viewProjection() * glm::vec4(p, 1.0f);
    if (clip.w <= 1e-4f) return std::nullopt;
    return ImVec2{(clip.x / clip.w * 0.5f + 0.5f) * w, (clip.y / clip.w * 0.5f + 0.5f) * h};
  }

  void updateHover() {
    hover_.reset();
    if (scriptHover_) hover_ = scriptHover_;  // a scripted cursor (captures of the grid)
    if (!battle_ || mouse_.x < 0.0f) return;
    if (ImGui::GetCurrentContext() && ImGui::GetIO().WantCaptureMouse) return;
    int w = 0, h = 0;
    SDL_Window* window = SDL_GetWindowFromID(mouseWindow_);
    if (!window || !SDL_GetWindowSize(window, &w, &h) || w <= 0 || h <= 0) return;
    hover_ = spellMode_.empty() ? pickWalk(w, h, mouse_.x, mouse_.y) : aimPickAt(w, h, mouse_.x, mouse_.y);
  }
  // A hex of the picked unit's reach: walking there is what a click / hover means.
  bool inReach(e2::BattleHex h) const {
    return std::any_of(reach_.begin(), reach_.end(), [&](const e2::ReachCell& c) { return c.x == h.x && c.y == h.y; });
  }
  // Walking (no spell open): the ground under the cursor wins over every card whenever the picked unit can walk there
  // (the user, 2026-10-18: the route never formed behind other units); else a body under the cursor, else the ground.
  std::optional<e2::BattleHex> pickWalk(int w, int h, float mx, float my) const {
    if (selected_ >= 0 && myTurn()) {
      const e2::BattleUnit& u = battle_->units()[static_cast<size_t>(unitBattle_[static_cast<size_t>(selected_)])];
      if (const std::optional<e2::BattleHex> ground = pickAt(w, h, mx, my, false); ground && !(ground->x == u.x && ground->y == u.y) && inReach(*ground))
        return ground;
    }
    return pickAt(w, h, mx, my);
  }
  // A spell being aimed: a foe's card under the cursor aims at its hex; a friend's card (the caster's own too) never hides
  // the ground behind it (a click on a foe standing behind a friend picked the friend: Mãos Flamejantes burnt the party).
  std::optional<e2::BattleHex> aimPickAt(int w, int h, float mx, float my) const {
    const std::optional<e2::BattleHex> card = pickAt(w, h, mx, my);
    if (card && selected_ >= 0) {
      const int who = battle_->unitAt(card->x, card->y);
      const e2::BattleUnit& me = battle_->units()[static_cast<size_t>(unitBattle_[static_cast<size_t>(selected_)])];
      if (who >= 0 && battle_->units()[static_cast<size_t>(who)].side != me.side) return card;
    }
    const std::optional<e2::BattleHex> ground = pickAt(w, h, mx, my, false);
    return ground ? ground : card;
  }

  // The left button: a reachable hex walks the picked player unit there; a unit picks it; anything else lets go.
  void click(SDL_WindowID id, float mx, float my) {
    int w = 0, h = 0;
    SDL_Window* window = SDL_GetWindowFromID(id);
    if (!window || !SDL_GetWindowSize(window, &w, &h) || w <= 0 || h <= 0) return;
    clickAt(w, h, mx, my);
  }
  // The click on a screen of w x h (the battle script's "click" step comes through here too).
  void clickAt(int w, int h, float mx, float my) {
    if (options_.editorClick) {
      if (const std::optional<e2::BattleHex> hex = pickAt(w, h, mx, my)) options_.editorClick(hex->x, hex->y);
      return;
    }
    if (!battle_ || !scene_) return;
    const std::optional<e2::BattleHex> hex = spellMode_.empty() ? pickWalk(w, h, mx, my) : pickAt(w, h, mx, my);
    if (busy()) return;  // a walk, a strike or an enemy's turn is playing
    if (!spellMode_.empty()) {
      if (const std::optional<e2::BattleHex> aim = aimPickAt(w, h, mx, my); aim && myTurn()) aimSpell(*aim);
      return;
    }
    pendingFoe_ = -1;  // a staged strike is dropped by a click anywhere but on a foe (Engine2 handleCell)
    if (!hex) return select(-1);
    // A marked foe: the click only stages the strike (Engine2 stageAttack); the panel shows its hit chance and asks.
    if (const int foe = sceneUnitOf(battle_->unitAt(hex->x, hex->y)); foe >= 0 && myTurn())
      for (const auto& [t, from] : targets_)
        if (t == foe) {
          pendingFoe_ = foe;
          tip_.clear();
          return;
        }
    // An NPC with a conversation (Engine2 handleCell's dialog branch): in free roam the leader walks up beside it first.
    if (const int npc = battle_->unitAt(hex->x, hex->y); npc >= 0 && npcDialog_.contains(static_cast<size_t>(npc)) &&
                                                          battle_->units()[static_cast<size_t>(npc)].alive)
      return talkTo(static_cast<size_t>(npc));
    if (myTurn()) {
      const e2::BattleUnit& u = battle_->units()[static_cast<size_t>(unitBattle_[static_cast<size_t>(selected_)])];
      const bool own = hex->x == u.x && hex->y == u.y;
      auto reachable = [&](e2::BattleHex h) {
        return std::any_of(reach_.begin(), reach_.end(), [&](const e2::ReachCell& c) { return c.x == h.x && c.y == h.y; });
      };
      if (!own && reachable(*hex)) return walkTo(static_cast<size_t>(selected_), *hex);
      // A card's rectangle is taller than the body: a click on the ground behind a friend's card is still a walk, not
      // a jump of the selection to that friend.
      if (!own)
        if (const std::optional<e2::BattleHex> ground = pickAt(w, h, mx, my, false); ground && !(ground->x == u.x && ground->y == u.y) && reachable(*ground))
          return walkTo(static_cast<size_t>(selected_), *ground);
      // Two left clicks on the unit being played end its turn (a single one keeps it picked).
      if (own && !battle_->roaming()) {
        const bool twice = lastOwnClick_ >= 0.0f && now_ - lastOwnClick_ <= kDoubleClick && lastOwnClickUnit_ == selected_;
        lastOwnClick_ = twice ? -1.0f : now_, lastOwnClickUnit_ = selected_;
        if (twice) endUnitTurn();
        return;
      }
    }
    const int ui = sceneUnitOf(battle_->unitAt(hex->x, hex->y));
    if (ui >= 0 && ui != selected_ && unitCard_[static_cast<size_t>(ui)].drawn && battle_->units()[static_cast<size_t>(unitBattle_[static_cast<size_t>(ui)])].alive)
      return select(ui);
    select(-1);
  }

  void select(int ui) {
    selected_ = ui;
    if (ui >= 0 && !logKeep_) showLog_ = false;  // a unit picked: its status shows, not the log
    spellMode_.clear(), spellCells_.clear(), spellAim_.clear();
    spellArmed_.reset(), pendingFoe_ = -1;
    attackMode_ = false;
    offHandMode_ = false;
    refreshReach();
  }

  // The battle bar picked a spell: every hex it may be aimed at is marked.
  void startSpell(const std::string& kind) {
    if (selected_ < 0) return;
    if (spellMode_ == kind) {  // the same socket again lets it go
      spellMode_.clear(), spellCells_.clear(), spellAim_.clear();
      return;
    }
    spellMode_ = kind;
    spellCells_.clear();
    spellAim_.clear();
    spellZone_.clear();
    spellArmed_.reset(), pendingFoe_ = -1;
    const size_t b = static_cast<size_t>(unitBattle_[static_cast<size_t>(selected_)]);
    const e2::BattleUnit& u = battle_->units()[b];
    // Criar Comida e Água has no aim: it is cast at once, around the caster.
    if (e2::spellInstant(kind)) return commitSpell({u.x, u.y});
    // Varredura, Expulsar Mortos-Vivos: its area around the caster is shown at once and waits for Confirmar (Engine2
    // startSweep / startTurnUndead arm the spell on the caster's own hex).
    if (e2::spellSelfArea(kind)) {
      spellAim_.push_back({u.x, u.y});
      spellZone_ = e2::spellArea(*battle_, b, kind, {u.x, u.y});
      spellArmed_ = e2::BattleHex{u.x, u.y};
      return;
    }
    spellPreviewAim_.reset(), hitsKey_.clear();
    std::vector<e2::BattleHex> valid;
    for (int y = 0; y < battle_->rows(); ++y)
      for (int x = 0; x < battle_->cols(); ++x)
        if (e2::Battle::distance({u.x, u.y}, {x, y}) <= 9 && aimValid(b, {x, y})) valid.push_back({x, y});
    // The locks go on units only: a hex of bare ground is shown by the zone, never as a target (the user, 2026-10-17, Tiro
    // Perfurante: every hex locked, the board one violet sheet, "como eu vou mirar com essa bosta").
    std::vector<int> standing(static_cast<size_t>(battle_->cols()) * static_cast<size_t>(battle_->rows()), -1);
    for (size_t i = 0; i < battle_->units().size(); ++i) {
      const e2::BattleUnit& t = battle_->units()[i];
      if (i == b || !t.alive || t.hidden || !unitOnShow(t)) continue;
      for (const e2::BattleHex& p : e2::Battle::footprint(t, t.x, t.y))
        if (battle_->inBounds(p.x, p.y)) standing[static_cast<size_t>(p.y) * static_cast<size_t>(battle_->cols()) + static_cast<size_t>(p.x)] = static_cast<int>(i);
    }
    auto unitOn = [&](e2::BattleHex c) {
      return battle_->inBounds(c.x, c.y) ? standing[static_cast<size_t>(c.y) * static_cast<size_t>(battle_->cols()) + static_cast<size_t>(c.x)] : -1;
    };
    if (e2::spellAimsDirection(kind)) {
      // A line or a cone through any hex: no zone. The locks = the foes some aim reaches; the grid draws the line under the
      // cursor, and toward the nearest of those foes while the cursor is off the board.
      std::vector<std::pair<int, e2::BattleHex>> foes;  // unit, the aim that reaches it (its own hex when that does)
      for (const e2::BattleHex& aim : valid)
        for (const e2::BattleHex& c : aimArea(b, aim)) {
          const int who = unitOn(c);
          if (who < 0 || battle_->units()[static_cast<size_t>(who)].side == u.side) continue;
          auto it = std::find_if(foes.begin(), foes.end(), [&](const auto& f) { return f.first == who; });
          if (it == foes.end()) foes.push_back({who, aim});
          else if (aim == c) it->second = aim;
        }
      int nearest = -1;
      for (size_t i = 0; i < foes.size(); ++i) {
        const e2::BattleUnit& f = battle_->units()[static_cast<size_t>(foes[i].first)];
        spellAim_.push_back({f.x, f.y});
        const e2::BattleUnit* n = nearest < 0 ? nullptr : &battle_->units()[static_cast<size_t>(foes[static_cast<size_t>(nearest)].first)];
        if (!n || e2::Battle::distance({u.x, u.y}, {f.x, f.y}) < e2::Battle::distance({u.x, u.y}, {n->x, n->y})) nearest = static_cast<int>(i);
      }
      if (nearest >= 0) spellPreviewAim_ = foes[static_cast<size_t>(nearest)].second;
      return;
    }
    for (const e2::BattleHex& c : valid)
      if (unitOn(c) >= 0) spellAim_.push_back(c);
    // The zone the grid shows: where the spell reaches (a potion: the caster and the hexes around it), whether or not
    // something stands there (the user: "how do I even know where to target?").
    if (potionOf(kind)) {
      spellZone_.push_back({u.x, u.y});
      for (const e2::BattleHex& h : e2::Battle::neighbors(u.x, u.y))
        if (battle_->inBounds(h.x, h.y)) spellZone_.push_back(h);
    } else {
      spellZone_ = e2::spellZone(*battle_, b, kind);
    }
  }

  // A potion being aimed is spellMode_ "potion:<id>" (Engine2 usePotion: the actor itself or an adjacent ally).
  static std::optional<std::string> potionOf(const std::string& mode) {
    return mode.rfind("potion:", 0) == 0 ? std::optional<std::string>(mode.substr(7)) : std::nullopt;
  }
  bool aimValid(size_t b, e2::BattleHex hex, std::string* why = nullptr) const {
    if (potionOf(spellMode_)) return e2::potionTargetValid(*battle_, b, hex, why);
    return e2::spellAimValid(*battle_, b, spellMode_, hex, why);
  }
  std::vector<e2::BattleHex> aimArea(size_t b, e2::BattleHex hex) const {
    return potionOf(spellMode_) ? std::vector<e2::BattleHex>{hex} : e2::spellArea(*battle_, b, spellMode_, hex);
  }
  // Who the spell aimed at `hex` would hit, friends included (the cast itself run on a copy of the battle, so its own
  // rules decide; the Tiro Perfurante's line hits the party too). Kept while the aim stays on the same hex.
  const std::vector<size_t>& aimHits(size_t b, e2::BattleHex hex) const {
    const std::string key = std::format("{}:{}:{}:{}", spellMode_, b, hex.x, hex.y);
    if (key == hitsKey_) return hits_;
    hitsKey_ = key, hits_.clear();
    if (potionOf(spellMode_) || e2::spellTargetCount(*battle_, b, spellMode_) != 1) return hits_;
    e2::Battle copy = *battle_;
    if (const std::optional<std::vector<e2::BattleStep>> steps = e2::castSpell(copy, b, spellMode_, {hex}, nullptr, false))
      for (const e2::BattleStep& s : *steps) {
        std::vector<size_t> ids = s.ids;
        if (s.type == e2::BattleStep::Type::Combat) ids.push_back(s.def);
        for (size_t id : ids)
          if (id != b && id < battle_->units().size() && std::find(hits_.begin(), hits_.end(), id) == hits_.end()) hits_.push_back(id);
      }
    return hits_;
  }

  // A hex clicked while a spell is aimed: one more target (Míssil Mágico takes one per missile), then the cast.
  void aimSpell(e2::BattleHex hex) {
    const size_t b = static_cast<size_t>(unitBattle_[static_cast<size_t>(selected_)]);
    std::string why;
    if (!aimValid(b, hex, &why)) {
      tip_ = why, tipAt_ = now_;
      return;
    }
    if (const std::optional<std::string> potion = potionOf(spellMode_)) {
      std::optional<e2::BattleStep> step = e2::usePotion(*battle_, b, *potion, hex, &why);
      spellMode_.clear(), spellCells_.clear(), spellAim_.clear();
      if (!step) {
        tip_ = why, tipAt_ = now_;
        return;
      }
      tip_ = e2::potionName(*potion), tipAt_ = now_;
      return play({*step}, static_cast<size_t>(selected_));
    }
    // Engine2 handleCell: every click first aims the spell (the panel then asks Confirmar / Cancelar); clicking the same
    // aimed hex again casts, as a tap always did. Only the pick that completes the cast asks; the others just count.
    if (static_cast<int>(spellCells_.size()) + 1 >= e2::spellTargetCount(*battle_, b, spellMode_) &&
        !(spellArmed_ && spellArmed_->x == hex.x && spellArmed_->y == hex.y)) {
      spellArmed_ = hex;
      return;
    }
    commitSpell(hex);
  }

  // The aimed hex is confirmed: the spell is cast (Engine2 confirmSpell).
  void commitSpell(e2::BattleHex hex) {
    const size_t b = static_cast<size_t>(unitBattle_[static_cast<size_t>(selected_)]);
    std::string why;
    spellArmed_.reset();
    spellCells_.push_back(hex);
    if (static_cast<int>(spellCells_.size()) < e2::spellTargetCount(*battle_, b, spellMode_)) return;
    std::optional<std::vector<e2::BattleStep>> steps = e2::castSpell(*battle_, b, spellMode_, spellCells_, &why);
    const std::string kind = spellMode_;
    spellMode_.clear(), spellCells_.clear(), spellAim_.clear();
    if (!steps) {
      tip_ = why, tipAt_ = now_;
      return;
    }
    play(std::move(*steps), static_cast<size_t>(selected_));
  }

  // Confirmar on the staged strike: what the click used to run. The unit whose turn it is strikes the foe, walking first
  // to the hex it strikes from.
  void confirmStrike() {
    const int foe = pendingFoe_;
    pendingFoe_ = -1;
    if (foe < 0 || selected_ < 0 || !myTurn() || busy()) return;
    for (const auto& [t, from] : targets_)
      if (t == foe) {
        const e2::BattleUnit& u = battle_->units()[static_cast<size_t>(unitBattle_[static_cast<size_t>(selected_)])];
        if (from.x == u.x && from.y == u.y)
          return attack(static_cast<size_t>(selected_), static_cast<size_t>(foe),
                        offHandWanted(static_cast<size_t>(unitBattle_[static_cast<size_t>(selected_)]), static_cast<size_t>(unitBattle_[static_cast<size_t>(foe)])));
        pendingStrike_ = foe;
        return walkTo(static_cast<size_t>(selected_), from);
      }
  }

  // The mission's end (Engine2 evaluateEnd / confirmFinish): winning only makes ending it an option. A hero on a waypoint offers it
  // (an escape marker with its DEX chance), and so does a won rout / boss mission.
  void confirmFinishNow() {
    const e2::Battle::Exit* exit = battle_->activeExit();
    const std::optional<e2::Battle::Exit> on = exit ? std::optional<e2::Battle::Exit>(*exit) : std::nullopt;
    const e2::FinishOutcome o = e2::confirmFinish(*battle_, rng_);
    if (o.ended) finishExit_ = on;  // the waypoint taken (hud.activeExit at the result screen)
    if (o.escapeFailed) {
      const e2::BattleUnit& u = battle_->units()[o.hero];
      tip_ = u.name + tr(" não conseguiu fugir — o combate continua.", " could not get away: the fight goes on."), tipAt_ = now_;
      battle_->pushLog(tip_);
      winDismissed_ = true;
      select(-1);
    } else if (o.ended) {
      tip_ = tr("Missão encerrada.", "Mission ended."), tipAt_ = now_;
    }
  }
  void drawFinishPrompt() {
    if (!battle_ || !result_.empty() || winDismissed_ || !battle_->winAvailable() || busy()) return;
    size_t hero = 0;
    const e2::Battle::Exit* exit = battle_->activeExit(&hero);
    std::string text, yes, no, floorExit;
    if (exit && exit->kind == "escape") {
      const std::string chance = std::format("{:g}", std::round(battle_->fleeChance(hero) * 100.0) / 100.0);
      text = tr("Encontraram uma rota de fuga. Desejam tentar escapar? (" + chance + "% de chance)", "You found a way out. Try to escape? (" + chance + "% chance)");
      yes = tr("Tentar escapar", "Try to escape"), no = tr("Ficar", "Stay");
    } else if (exit && exit->kind == "connector" && !exit->targetMapId.empty()) {
      const bool up = connectorUp(*exit);
      text = up ? tr("Encontraram uma passagem para subir. Deseja subir?", "You found a way up. Go up?")
                : tr("Encontraram uma passagem para descer. Deseja descer?", "You found a way down. Go down?");
      if (exit->returnConnector && exit->connectorDirection.empty() && !innFloor())
        text = tr("Encontraram a passagem de volta. Deseja voltar?", "You found the way back. Go back?");
      yes = tr("Sim", "Yes"), no = tr("Não", "No");
      floorExit = exit->targetMapId;
    } else if (exit) {
      text = exit->kind == "dungeon" ? tr("Encontraram a saída da masmorra. Desejam sair?", "You found the dungeon exit. Leave?")
                                     : tr("Encontraram uma passagem para o próximo andar. Deseja avançar?", "You found a way to the next floor. Go on?");
      yes = tr("Sair", "Leave"), no = tr("Ficar", "Stay");
    } else {
      text = tr("Todos os inimigos caíram. Encerrar a missão?", "Every enemy has fallen. End the mission?");
      yes = tr("Encerrar missão", "End mission"), no = tr("Continuar explorando", "Keep exploring");
    }
    const ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos({io.DisplaySize.x * 0.5f, io.DisplaySize.y - 132.0f - 12.0f}, ImGuiCond_Always, {0.5f, 1.0f});
    ImGui::SetNextWindowSize({520.0f, 0.0f});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {16.0f, 12.0f});
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {8.0f, 8.0f});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(14, 14, 14, 242));
    ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(226, 150, 70, 170));
    ImGui::Begin("##finishprompt", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing);
    ImGui::PushFont(uiFont(UiFont::Ui), 15.0f);
    ImGui::PushTextWrapPos(488.0f);
    ImGui::TextUnformatted(text.c_str());
    ImGui::PopTextWrapPos();
    ImGui::PopFont();
    if (emberButton("##finishyes", yes.c_str(), true, true)) {
      // On a free-roam map (the walkable Inn) a floor connector leads straight onto the linked floor. In a fight it ends the
      // mission as Engine2's confirmFinish does: victory, then the result screen's "Subir" / "Descer" / "Avançar" / "Voltar"
      // (GameApp startBattle(target, ..., preserveKnockouts, arrivedFrom)).
      if (!floorExit.empty() && explores_) floorRequest_ = floorExit, winDismissed_ = true;
      else confirmFinishNow();
    }
    ImGui::SameLine();
    if (emberButton("##finishno", no.c_str(), false, true)) winDismissed_ = true;
    ImGui::End();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(3);
  }

  // Engine2's confirm panel (GameApp data-attack-confirm / data-skill-confirm): what is about to happen and its hit chance,
  // with Confirmar and Cancelar, above the battle bar. A click on a foe stages the strike; an aimed spell waits here.
  void drawConfirm() {
    if (!battle_ || selected_ < 0 || !myTurn() || busy() || !result_.empty()) return;
    const size_t ab = static_cast<size_t>(unitBattle_[static_cast<size_t>(selected_)]);
    std::string title;
    std::optional<double> hit;
    bool ready = true;
    if (pendingFoe_ >= 0) {
      const auto t = std::find_if(targets_.begin(), targets_.end(), [&](const auto& q) { return q.first == pendingFoe_; });
      if (t == targets_.end()) return void(pendingFoe_ = -1);
      const size_t db = static_cast<size_t>(unitBattle_[static_cast<size_t>(pendingFoe_)]);
      title = tr("Atacar ", "Attack ") + battle_->units()[db].name;
      hit = forecastFor(ab, db, t->second).hitOut;
    } else if (!spellMode_.empty() && spellArmed_) {
      title = potionOf(spellMode_) ? e2::potionName(*potionOf(spellMode_)) : e2::spellName(spellMode_);
      ready = aimValid(ab, *spellArmed_);
      // Weapon skills roll the weapon's own hit chance on a foe (Engine2 spellHitChance); no damage is predicted.
      if (e2::spellWeaponAbility(spellMode_))
        if (const int fb = battle_->unitAt(spellArmed_->x, spellArmed_->y);
            fb >= 0 && battle_->units()[static_cast<size_t>(fb)].side != battle_->units()[ab].side && battle_->units()[static_cast<size_t>(fb)].alive) {
          const e2::BattleUnit& u = battle_->units()[ab];
          hit = e2::forecast(*battle_, ab, {u.x, u.y}, static_cast<size_t>(fb)).hitOut;
        }
      // The Lanceiro's aimed at a hex (a throw, a drag) or an ally (the swap): the foe the cast would strike; the vault's blow
      // always comes from behind (+10, as a rear attack).
      if (!hit && ready && e2::spellWeaponAbility(spellMode_))
        for (size_t id : aimHits(ab, *spellArmed_))
          if (battle_->units()[id].side != battle_->units()[ab].side) {
            const e2::BattleUnit& u = battle_->units()[ab];
            hit = e2::forecast(*battle_, ab, spellMode_ == "swapPost" ? *spellArmed_ : e2::BattleHex{u.x, u.y}, id).hitOut;
            break;
          }
      if (hit && spellMode_ == "poleVault") hit = std::min(100.0, *hit + 10.0);
    } else {
      return;
    }
    const ImGuiIO& io = ImGui::GetIO();
    const float width = 320.0f;
    ImGui::SetNextWindowPos({io.DisplaySize.x * 0.5f, io.DisplaySize.y - 132.0f - 12.0f}, ImGuiCond_Always, {0.5f, 1.0f});
    ImGui::SetNextWindowSize({width, 0.0f});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {16.0f, 12.0f});
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {8.0f, 6.0f});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(14, 14, 14, 242));
    ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(226, 150, 70, 170));
    ImGui::Begin("##confirm", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                                           ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_AlwaysAutoResize);
    auto centred = [&](const char* text, ImFont* font, float size, ImU32 ink) {
      const float w = font->CalcTextSizeA(size, FLT_MAX, 0.0f, text).x;
      ImGui::SetCursorPosX(std::max(0.0f, (width - w) * 0.5f));
      ImGui::PushFont(font, size);
      ImGui::PushStyleColor(ImGuiCol_Text, ink);
      ImGui::TextUnformatted(text);
      ImGui::PopStyleColor();
      ImGui::PopFont();
    };
    centred(title.c_str(), uiFont(UiFont::Ui), 14.0f, IM_COL32(226, 150, 70, 255));
    if (hit) {  // two decimals (the user, 2026-10-18: "82,59 %"), a decimal comma in Portuguese
      std::string pt = std::format("{:.2f}", *hit);
      std::replace(pt.begin(), pt.end(), '.', ',');
      centred(tr(pt + " % de acerto", std::format("{:.2f} % to hit", *hit)).c_str(), uiFont(UiFont::Mark), 34.0f, IM_COL32(244, 232, 214, 255));
    }
    // Fogo amigo: the aimed spell would hit friends too (allowed; it costs their affinity with the caster).
    if (pendingFoe_ < 0 && spellArmed_ && ready) {
      std::string friends;
      for (size_t id : aimHits(ab, *spellArmed_))
        if (battle_->units()[id].side == battle_->units()[ab].side) friends += (friends.empty() ? "" : ", ") + battle_->units()[id].name;
      if (!friends.empty()) {
        centred(tr("Fogo amigo: ", "Friendly fire: ").append(friends).c_str(), uiFont(UiFont::Ui), 14.0f, IM_COL32(255, 112, 92, 255));
        centred(tr("acerta o grupo e custa afinidade", "hits the party and costs affinity").c_str(), uiFont(UiFont::Ui), 12.0f, IM_COL32(214, 170, 150, 255));
      }
    }
    const std::string yes = tr("Confirmar", "Confirm"), no = tr("Cancelar", "Cancel");
    auto buttonW = [&](const std::string& l) { return spacedTextWidth(uiFont(UiFont::Mark), 16.0f, l.c_str(), 0.06f) + 28.0f; };
    ImGui::SetCursorPosX(std::max(0.0f, (width - 32.0f - buttonW(yes) - buttonW(no) - ImGui::GetStyle().ItemSpacing.x) * 0.5f) + 16.0f);
    if (emberButton("##confirmyes", yes.c_str(), true, ready)) {
      if (pendingFoe_ >= 0) confirmStrike();
      else if (spellArmed_) commitSpell(*spellArmed_);
    }
    ImGui::SameLine();
    if (emberButton("##confirmno", no.c_str(), false, true)) {
      if (pendingFoe_ >= 0) pendingFoe_ = -1;
      else spellMode_.clear(), spellCells_.clear(), spellAim_.clear(), spellArmed_.reset();  // cancelSkillConfirm: the spell is put away
    }
    ImGui::End();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(3);
  }

  void refreshReach() {
    reach_.clear(), reachPaths_.clear(), targets_.clear();
    if (!battle_ || selected_ < 0 || unitBattle_[static_cast<size_t>(selected_)] < 0) return;
    const size_t b = static_cast<size_t>(unitBattle_[static_cast<size_t>(selected_)]);
    const e2::BattleUnit& u = battle_->units()[b];
    if (!u.alive) return;
    // Its reach: the movement left for the unit whose turn it is, a whole pool for one being inspected.
    e2::Battle look = *battle_;
    if (!myTurn()) look.units()[b].moveUsed = 0;
    reach_ = look.reachable(b, true);
    reachPaths_ = look.reachable(b, false);  // routes may pass a friend's hex (Engine2: walk reach unpruned)
    // The foes it can strike from some hex of its reach, each from the first such hex (Engine2 attackableEnemies),
    // and that the party can see; only for the party's unit whose turn it is and that has not struck yet.
    if (!myTurn() || u.acted || battle_->roaming()) return;
    for (const e2::ReachCell& c : reach_)
      for (size_t i = 0; i < scene_->units.size(); ++i) {
        const int fb = unitBattle_[i];
        if (fb < 0 || !unitCard_[i].drawn) continue;
        const e2::BattleUnit& f = battle_->units()[static_cast<size_t>(fb)];
        if (!f.alive || f.side == u.side) continue;
        if (std::any_of(targets_.begin(), targets_.end(), [&](const auto& t) { return t.first == static_cast<int>(i); })) continue;
        if (e2::canHitFrom(*battle_, b, {c.x, c.y}, static_cast<size_t>(fb))) targets_.push_back({static_cast<int>(i), e2::BattleHex{c.x, c.y}});
      }
    // The off-hand weapon (a dagger beside a bow) reaches what the bow cannot, standing where it is; with "Mão Secundária"
    // armed those are the only targets.
    if (u.combat.offHandWeapon && !u.combat.offHandShield) {
      for (size_t i = 0; i < scene_->units.size(); ++i) {
        const int fb = unitBattle_[i];
        if (fb < 0 || !unitCard_[i].drawn) continue;
        const e2::BattleUnit& f = battle_->units()[static_cast<size_t>(fb)];
        if (!f.alive || f.side == u.side || !e2::canOffHandHit(*battle_, b, {u.x, u.y}, static_cast<size_t>(fb))) continue;
        const auto have = std::find_if(targets_.begin(), targets_.end(), [&](const auto& t) { return t.first == static_cast<int>(i); });
        if (have == targets_.end()) targets_.push_back({static_cast<int>(i), e2::BattleHex{u.x, u.y}});
        else if (u.combat.arrow) have->second = e2::BattleHex{u.x, u.y};  // in the dagger's reach it strikes from here
      }
    }
    if (offHandMode_)
      targets_.erase(std::remove_if(targets_.begin(), targets_.end(),
                                    [&](const auto& t) { return !e2::canOffHandAct(*battle_, b, {u.x, u.y}, static_cast<size_t>(unitBattle_[static_cast<size_t>(t.first)])); }),
                     targets_.end());
  }

  // The party's unit whose turn it is, picked.
  bool myTurn() const {
    if (!battle_ || selected_ < 0 || unitBattle_[static_cast<size_t>(selected_)] < 0) return false;
    if (battle_->roaming()) {  // free roam: no turns, the party's picked unit leads
      const e2::BattleUnit& u = battle_->units()[static_cast<size_t>(unitBattle_[static_cast<size_t>(selected_)])];
      return u.side == "player" && u.alive && result_.empty();
    }
    const int active = battle_->activeUnit();
    return active == unitBattle_[static_cast<size_t>(selected_)] && battle_->units()[static_cast<size_t>(active)].side == "player" && result_.empty();
  }

  // Something is playing that input must not cut into: a walk, a strike, an enemy's turn, the battle's end.
  bool busy() const {
    if (exchange_ || enemyTurn_ || !result_.empty() || sightPause()) return true;
    return std::any_of(unitWalk_.begin(), unitWalk_.end(), [](const Walk& w) { return !w.points.empty(); });
  }

  // Engine2's turn loop (tick / beginUnitTurn / startNewRound): whoever is next in the round acts; when everyone has
  // gone a new round starts. The party's unit is picked and waits for the player; an enemy plays its own turn.
  void updateTurns() {
    if (!battle_ || !scene_ || !result_.empty() || holdTurns || battle_->roaming()) return;
    const bool walking = std::any_of(unitWalk_.begin(), unitWalk_.end(), [](const Walk& w) { return !w.points.empty(); });
    if (exchange_ || walking || sightPause()) return;
    // A strike the player asked for once the walk to its hex is over.
    if (pendingStrike_ >= 0) {
      const int foe = pendingStrike_;
      pendingStrike_ = -1;
      if (selected_ >= 0 && battle_->units()[static_cast<size_t>(unitBattle_[static_cast<size_t>(foe)])].alive)
        return attack(static_cast<size_t>(selected_), static_cast<size_t>(foe));
    }
    // An enemy's turn: its walk is over; now its strike, then its turn ends.
    if (enemyTurn_) {
      EnemyTurn t = *enemyTurn_;
      enemyTurn_.reset();
      // The Vigília da Sentinela (docs/SKILLS_LANCER.md): before its blow or its cast, every sentinel whose stance covers one of
      // its targets thrusts first; an attacker that falls never strikes.
      if (!t.struck && !t.interrupted && battle_->units()[static_cast<size_t>(unitBattle_[t.unit])].alive) {
        t.interrupted = true;
        std::vector<size_t> targets;
        if (t.target >= 0 && unitBattle_[static_cast<size_t>(t.target)] >= 0) targets.push_back(static_cast<size_t>(unitBattle_[static_cast<size_t>(t.target)]));
        for (const e2::BattleStep& st : t.steps) {
          targets.insert(targets.end(), st.ids.begin(), st.ids.end());
          if (st.type == e2::BattleStep::Type::Combat) targets.push_back(st.def);
        }
        if (playInterrupts(static_cast<size_t>(unitBattle_[t.unit]), targets)) {
          enemyTurn_ = t;
          return;
        }
      }
      if (!t.steps.empty() && !t.struck && battle_->units()[static_cast<size_t>(unitBattle_[t.unit])].alive) {
        t.struck = true;
        std::vector<e2::BattleStep> steps = t.steps;
        enemyTurn_ = t;
        play(std::move(steps), t.unit);
        return;
      }
      if (t.target >= 0 && !t.struck && battle_->units()[static_cast<size_t>(unitBattle_[static_cast<size_t>(t.target)])].alive &&
          battle_->units()[static_cast<size_t>(unitBattle_[t.unit])].alive) {
        t.struck = true;
        enemyTurn_ = t;
        attack(t.unit, static_cast<size_t>(t.target));
        return;
      }
      battle_->units()[static_cast<size_t>(unitBattle_[t.unit])].moved = true;
    }
    if (result_ = battle_->result(); !result_.empty()) {
      resultAt_ = now_;
      return select(-1);
    }
    int active = battle_->activeUnit();
    if (active < 0) {
      battle_->startNewRound();
      turnUnit_ = -1;
      active = battle_->activeUnit();
      if (active < 0) return;
    }
    if (active == turnUnit_) {
      // A saved fight resumed mid-turn: the hero whose turn it is stands picked again (else Esperar / Fim do turno and the
      // actions stay dead until the player clicks it).
      if (pickActiveOnResume_) {
        pickActiveOnResume_ = false;
        const int ui = sceneUnitOf(active);
        if (ui >= 0 && battle_->units()[static_cast<size_t>(active)].side == "player") {
          undo_.clear();
          select(ui);
        }
      }
      return;
    }
    pickActiveOnResume_ = false;
    turnUnit_ = active;
    battle_->beginTurn(static_cast<size_t>(active));
    {
      // The start of its own turn: a lightning echo, the ground's flames, our zones and conditions, a Meteoro it called.
      std::vector<e2::BattleEffect> fx;
      const bool echo = battle_->units()[static_cast<size_t>(active)].shock.has_value();
      // Engine2 beginUnitTurn: those effects, then stun, the web's sleep roll, sleep and a flight in fear
      const e2::TurnStart turn = e2::beginUnitTurn(*battle_, static_cast<size_t>(active), rng_, fx);
      if (const int ui = sceneUnitOf(active); echo && ui >= 0 && !fx.empty()) spellFx_.echo(unitSpot(static_cast<size_t>(ui)), now_);
      // A Meteoro that fell: the rock takes 0.9 s to land; the numbers wait for it.
      const std::vector<e2::Battle::Zone> fallen = battle_->takeLanded();
      for (const e2::Battle::Zone& z : fallen) spellFx_.meteorFall(zoneSpots(z), now_, now_ + 0.9f);
      if (fallen.empty()) showEffects(fx);
      else pendingFx_.push_back({now_ + 0.9f, fx});
      if (!battle_->units()[static_cast<size_t>(active)].alive) {
        battle_->units()[static_cast<size_t>(active)].moved = true;
        return;
      }
      if (turn.kind != e2::TurnStart::Kind::Normal) {  // it loses the turn (stunned, asleep) or runs from what it fears
        const int ui = sceneUnitOf(active);
        if (ui >= 0 && !turn.tip.empty()) tip_ = battle_->units()[static_cast<size_t>(active)].name + turn.tip, tipAt_ = now_;
        if (ui >= 0 && turn.kind == e2::TurnStart::Kind::Fled && turn.fleePath.size() > 1) fleeAlong(static_cast<size_t>(ui), turn.fleePath);
        return;
      }
    }
    const int ui = sceneUnitOf(active);
    const e2::BattleUnit& u = battle_->units()[static_cast<size_t>(active)];
    if (ui < 0) {  // a unit with no card in this scene sits its turns out
      battle_->units()[static_cast<size_t>(active)].moved = true;
      return;
    }
    if (u.side == "player") {
      undo_.clear();
      return select(ui);
    }
    // Under fog an enemy that has not seen the party yet holds its ground (Engine2 wakeIfSeesParty; here: once the
    // party sees its hex).
    if (fog_ && !noFog_ && !awake_.contains(active)) {
      const size_t i = static_cast<size_t>(e2::indexOf(board_->layout(), u.x, u.y));
      if (!litHex(i)) {
        battle_->units()[static_cast<size_t>(active)].moved = true;
        return;
      }
      awake_.insert(active);
    }
    select(-1);
    if (std::optional<e2::AiCast> cast = e2::planEnemyCast(*battle_, static_cast<size_t>(active))) {
      EnemyTurn t;
      t.unit = static_cast<size_t>(ui);
      t.steps = cast->steps;
      enemyTurn_ = t;
      if (cast->path.size() > 1) walkAlong(static_cast<size_t>(ui), cast->path);
      return;
    }
    const e2::AiPlan plan = battle_->planEnemy(static_cast<size_t>(active));
    EnemyTurn t;
    t.unit = static_cast<size_t>(ui);
    t.target = plan.target >= 0 ? sceneUnitOf(plan.target) : -1;
    enemyTurn_ = t;
    if (plan.path.size() > 1) walkAlong(static_cast<size_t>(ui), plan.path);
  }

  // --- Free roam (an explore mission, Engine2 mission.explore, and any map with fog of war: the user, 2026-10-17): no turns
  // until the party meets an enemy ------------------------------------------------------------------------------------------
  // The user's rule (2026-10-16): the party walks freely, its picked unit leading and the others following in
  // a line; an enemy coming into the party's sight starts the fight, in initiative order, with every unit's own
  // movement; once no enemy is in sight (fallen or out of view) the party roams again (the user, 2026-10-17: "free
  // movement switch back and forth"). Engine2 roamed the whole mission; its free reach stays (Battle::reachable while
  // roaming), and like Engine2 the reach is not washed over the board (a box says it is free movement).
  static constexpr int kRoamSight = e2::kSightRadius;  // without fog of war: an enemy this many hexes from the party is met (the same 12 as the fog's sight; was 6)

  // The living enemies the party sees right now (fog: on a hex in sight; without fog: within kRoamSight of a hero).
  std::vector<int> enemiesInSight() const {
    const e2::HexLayout& L = board_->layout();
    // The party's eyes where its feet are now (mid-walk too).
    std::vector<e2::BattleHex> eyes;
    for (size_t i = 0; i < scene_->units.size(); ++i) {
      const int b = unitBattle_[i];
      if (b < 0 || battle_->units()[static_cast<size_t>(b)].side != "player" || !battle_->units()[static_cast<size_t>(b)].alive) continue;
      const auto [c, r] = e2::hexAt(L, unitPos_[i].x, unitPos_[i].y);
      eyes.push_back({c, r});
    }
    const bool fogged = fog_ && !noFog_;
    std::vector<int> met;
    for (size_t b = 0; b < battle_->units().size(); ++b) {
      const e2::BattleUnit& e = battle_->units()[b];
      if (!e.alive || e.side != "enemy") continue;
      bool seen = false;
      if (fogged) {
        const size_t i = static_cast<size_t>(e2::indexOf(L, e.x, e.y));
        seen = litHex(i);  // what is drawn is in sight: an enemy on lit ground starts the fight, as the player sees it
      } else {
        seen = std::any_of(eyes.begin(), eyes.end(), [&](e2::BattleHex h) { return e2::Battle::distance(h, {e.x, e.y}) <= kRoamSight; });
      }
      if (seen) met.push_back(static_cast<int>(b));
    }
    return met;
  }

  void updateRoaming() {
    if (!battle_ || !scene_ || !roams_ || !result_.empty()) return;
    if (fog_ && !noFog_ && !fogReady_) return;  // the fog's first sight decides whether the map opens in free roam or a fight
    if (!battle_->roaming()) {
      // The fight is over once no enemy is in sight, dead or only out of view (the user, 2026-10-17: the party does not hunt a
      // foe it cannot see turn by turn): free roam again, between actions (nothing walking, striking or playing an enemy's turn).
      if (busy() || pendingStrike_ >= 0) return;
      if (!enemiesInSight().empty()) return;
      battle_->resumeRoaming();
      for (e2::BattleUnit& u : battle_->units()) u.moveUsed = 0, u.acted = false, u.moved = false;
      undo_.clear();
      turnUnit_ = -1;
      select(-1);  // the leader is picked again below, next frame
      // No announcement (the user, 2026-10-17: "não existe anúncio"): the box is free movement again, that is all.
      logInfo("explore: no foe in sight: free roam again");
      return;
    }
    const std::vector<int> met = enemiesInSight();
    if (met.empty()) {
      // Someone of the party always leads: the last leader, else the party's first.
      const bool leading = selected_ >= 0 && unitBattle_[static_cast<size_t>(selected_)] >= 0 && myTurn();
      if (!leading && !busy()) {
        int pick = roamLeader_;
        auto fit = [&](int ui) {
          if (ui < 0 || ui >= static_cast<int>(scene_->units.size()) || unitBattle_[static_cast<size_t>(ui)] < 0) return false;
          const e2::BattleUnit& u = battle_->units()[static_cast<size_t>(unitBattle_[static_cast<size_t>(ui)])];
          return u.side == "player" && u.alive;
        };
        for (int ui = 0; !fit(pick) && ui < static_cast<int>(scene_->units.size()); ++ui) pick = ui;
        if (fit(pick)) select(pick);
      }
      if (selected_ >= 0 && myTurn()) roamLeader_ = selected_;
      return;
    }
    // Met: every walk stops on the hex it is stepping onto (when nobody stands there), the enemies in sight wake, and
    // the fight starts from the top of the initiative order with everyone's own movement whole.
    battle_->endRoaming();
    for (size_t i = 0; i < unitWalk_.size(); ++i) {
      Walk& w = unitWalk_[i];
      if (w.points.empty() || w.hexes.empty() || unitBattle_[i] < 0) continue;
      e2::BattleUnit& u = battle_->units()[static_cast<size_t>(unitBattle_[i])];
      const e2::BattleHex next = w.hexes.front();
      const int there = battle_->unitAt(next.x, next.y);
      if (there >= 0 && there != unitBattle_[i]) continue;  // a friend stands there: it walks on to its own hex
      u.x = next.x, u.y = next.y;
      w.points.resize(1), w.hexes.resize(1);
    }
    for (e2::BattleUnit& u : battle_->units()) u.moveUsed = 0, u.acted = false, u.moved = false;
    for (const int b : met) awake_.insert(b);
    undo_.clear();
    turnUnit_ = -1;
    select(-1);
    // The user (2026-10-18): the movement pauses and "Inimigos avistados!" shows, then the turns begin (drawModeBanner,
    // sightPause). Going back to free roam says nothing.
    modeSwitchAt_ = now_;
    logInfo("explore: {} in sight: the roaming ends, round {} begins", battle_->units()[static_cast<size_t>(met.front())].name, battle_->round());
  }

  // The party follows its leader in a line (the user's rule, 2026-10-17): the nearest hero takes the hex one step behind
  // the leader on its route, the next the one behind that, and so on; each joins the leader's footsteps where that is
  // shortest and walks them. A route shorter than the party leaves the rest on the free hexes nearest where the leader set
  // off. Never on the Inn's floors: there everyone goes their own way. `route` is the leader's walk (start first),
  // `leaderReach` the reach it was planned on (the cost of each of its hexes).
  void followLeader(size_t leader, const std::vector<e2::BattleHex>& route, const std::vector<e2::ReachCell>& leaderReach) {
    if (innFloor() || route.size() < 2 || unitBattle_[leader] < 0) return;
    const e2::BattleUnit& lu = battle_->units()[static_cast<size_t>(unitBattle_[leader])];
    if (lu.x != route.back().x || lu.y != route.back().y) return;  // its walk was cut short (a wound, a warp gate): nobody follows
    auto costIn = [](const std::vector<e2::ReachCell>& reach, e2::BattleHex h) -> std::optional<int> {
      for (const e2::ReachCell& c : reach)
        if (c.x == h.x && c.y == h.y) return c.cost;
      return std::nullopt;
    };
    struct Follower {
      size_t ui, b;
      int distance;
    };
    std::vector<Follower> party;
    for (size_t i = 0; i < scene_->units.size(); ++i) {
      if (i == leader || unitBattle_[i] < 0) continue;
      const size_t b = static_cast<size_t>(unitBattle_[i]);
      e2::BattleUnit& f = battle_->units()[b];
      if (!f.alive || f.side != "player") continue;
      // Still on its way from the last click: it sets off again from the hex its card is stepping onto.
      if (Walk& w = unitWalk_[i]; !w.points.empty() && !w.hexes.empty()) {
        const int there = battle_->unitAt(w.hexes.front().x, w.hexes.front().y);
        if (there < 0 || there == static_cast<int>(b)) {
          f.x = w.hexes.front().x, f.y = w.hexes.front().y;
          w.points.resize(1), w.hexes.resize(1);
        }
      }
      party.push_back({i, b, e2::Battle::distance({f.x, f.y}, route.front())});
    }
    std::stable_sort(party.begin(), party.end(), [](const Follower& a, const Follower& c) { return a.distance < c.distance; });
    // Planned on a copy of the battle: the leader already at the end of its route, followers off the board until placed.
    e2::Battle look = *battle_;
    for (const Follower& f : party) look.units()[f.b].alive = false;
    std::set<std::pair<int, int>> taken{{route.back().x, route.back().y}};
    size_t slot = route.size() - 1;  // the next hex behind the leader on its route
    for (const Follower& f : party) {
      e2::BattleUnit& fu = look.units()[f.b];
      fu.alive = true;
      const std::vector<e2::ReachCell> walk = look.reachable(f.b, false);
      const std::vector<e2::ReachCell> stops = look.reachable(f.b, true);
      auto open = [&](e2::BattleHex h) { return !taken.contains({h.x, h.y}) && costIn(stops, h).has_value(); };
      std::optional<size_t> onRoute;
      while (slot > 0 && !onRoute) {
        --slot;
        if (open(route[slot])) onRoute = slot;
      }
      std::optional<e2::BattleHex> target;
      if (onRoute) {
        target = route[*onRoute];
      } else {  // the route is used up: the free hex nearest where the leader set off
        int best = 0;
        for (const e2::ReachCell& c : stops) {
          const e2::BattleHex h{c.x, c.y};
          if (!open(h)) continue;
          const int score = 64 * e2::Battle::distance(h, route.front()) + e2::Battle::distance(h, route.back());
          if (!target || score < best) target = h, best = score;
        }
      }
      if (!target) continue;
      taken.insert({target->x, target->y});
      fu.x = target->x, fu.y = target->y;
      std::vector<e2::BattleHex> path;
      if (onRoute) {
        // Where it joins the leader's footsteps: the cheapest way to some hex j of the route plus the route from j on.
        const std::optional<int> end = costIn(leaderReach, route[*onRoute]);
        std::optional<size_t> join;
        int bestCost = 0;
        for (size_t j = 0; j <= *onRoute; ++j) {
          const std::optional<int> to = costIn(walk, route[j]);
          const std::optional<int> at = costIn(leaderReach, route[j]);
          if (!to || !at || !end) continue;
          const int cost = *to + (*end - *at);
          if (!join || cost < bestCost) join = j, bestCost = cost;
        }
        if (join) {
          path = e2::Battle::path(walk, route[*join].x, route[*join].y);
          if (!path.empty()) path.insert(path.end(), route.begin() + static_cast<std::ptrdiff_t>(*join) + 1, route.begin() + static_cast<std::ptrdiff_t>(*onRoute) + 1);
        }
      }
      if (path.empty()) path = e2::Battle::path(walk, target->x, target->y);
      if (path.size() >= 2) walkAlong(f.ui, path);
    }
  }

  // Walk a unit along a route (start first): the battle's state moves at once, the walk then plays it.
  void walkAlong(size_t ui, const std::vector<e2::BattleHex>& planned) {
    const size_t b = static_cast<size_t>(unitBattle_[ui]);
    e2::BattleUnit& u = battle_->units()[b];
    std::vector<e2::BattleEffect> fx;
    // A bleeding unit opens its wound once per own turn when it walks (applyBleedingActionDamage)
    if (!e2::bleedOnAction(*battle_, b, true, rng_, fx)) {
      showEffects(fx);
      u.moved = true;
      return;
    }
    // Its cost along the route (the step costs of the reach it was planned on).
    for (const e2::ReachCell& c : battle_->reachable(b, false))
      if (c.x == planned.back().x && c.y == planned.back().y) u.moveUsed += c.cost;
    // Hex by hex: a flame underfoot burns, a warp gate carries it across (the route is cut / extended to what it walked)
    std::vector<e2::BattleHex> route = planned;
    walkHexes(b, route, fx);
    pendingFx_.push_back({now_ + stepSeconds() * static_cast<float>(route.size()), fx});
    Walk w;
    for (size_t k = 1; k < route.size(); ++k) w.points.push_back(anchorWorld(u, route[k])), w.hexes.push_back(route[k]);
    const auto [hx, hy] = e2::heading(route[route.size() - 2], route.back());
    u.faceDx = hx, u.faceDy = hy;
    w.start = now_;
    // startSeq's walk cue (Cultist V2 by its first step's side), faded when the walk ends if it is a monster's own.
    const e2::Json& walk = sounds(b, route.size() > 1 && route[1].x < route[0].x ? "walkLeft" : "walk");
    playCues(walk);
    w.fadeSound = sounds(b, "walkFade") == e2::Json(true) && walk.is_array() && !walk.empty() ? walk[0].value("file", "") : "";
    unitWalk_[ui] = std::move(w);
  }

  // A unit running from what it fears: its state moves at once (a flame underfoot burns it on the way), the card follows.
  void fleeAlong(size_t ui, std::vector<e2::BattleHex> route) {
    const size_t b = static_cast<size_t>(unitBattle_[ui]);
    std::vector<e2::BattleEffect> fx;
    walkHexes(b, route, fx);
    if (route.size() > 1) animateAlong(ui, route, stepSeconds());
    pendingFx_.push_back({now_ + stepSeconds() * static_cast<float>(route.size()), fx});
  }

  // The rules of a walk, hex by hex (Engine2 stepActive "move"): the unit stands on each hex in turn, a hazard there hurts
  // it (and may kill it: the walk stops there), a warp gate carries it across (it comes out beside the other). `route` is
  // cut to what it really walked; the unit is left on its last hex.
  void walkHexes(size_t b, std::vector<e2::BattleHex>& route, std::vector<e2::BattleEffect>& fx) {
    e2::BattleUnit& u = battle_->units()[b];
    for (size_t k = 1; k < route.size(); ++k) {
      u.x = route[k].x, u.y = route[k].y;
      e2::stepOnHex(*battle_, b, rng_, fx);
      if (!u.alive) {
        route.resize(k + 1);
        return;
      }
      if (const std::optional<e2::BattleHex> across = e2::warpCrossing(*battle_, b)) {
        route.resize(k + 1);  // the card walks to the gate; the unit is on the far side at once (the card follows when it arrives)
        u.x = across->x, u.y = across->y;
        e2::BattleEffect moved;
        moved.unit = b, moved.moved = true, moved.pushed = true;
        fx.push_back(moved);
        return;
      }
    }
  }

  // A unit's card walked along a route the rules already played (a charge, a flight in fear): no state changes here.
  float animateAlong(size_t ui, const std::vector<e2::BattleHex>& route, float secondsPerHex) {
    if (route.size() < 2) return 0.0f;
    const size_t b = static_cast<size_t>(unitBattle_[ui]);
    const e2::BattleUnit& u = battle_->units()[b];
    Walk w;
    for (size_t k = 1; k < route.size(); ++k) w.points.push_back(anchorWorld(u, route[k])), w.hexes.push_back(route[k]);
    w.start = now_;
    unitWalk_[ui] = std::move(w);
    return secondsPerHex * static_cast<float>(route.size() - 1);
  }

  // --- The exchange: an attack and, when it can, the counter (Engine2 BattleEngine's combat queue for a plain attack). ---

  // A pose played once over the unit's own (attack, hit, death), from `start` on the scene clock.
  struct OneShot {
    std::string pose;
    float start = 0.0f;
    bool hold = false;   // keeps its last frame (death)
    float rate = 1.0f;   // its pace (Velocidade)
    float until = -1.0f; // >= 0: held until this time instead of its own length (a still)
    std::string then;    // played next, as this one ends, until `thenUntil` (the cast pose, then its back still)
    float thenUntil = -1.0f;
  };
  // An action in play (Engine2's queue): its steps one after another (a strike and its counter, a spell's hits, a
  // heal), each a pose with the rules applied at its impact.
  struct Exchange {
    std::vector<e2::BattleStep> steps;
    size_t step = 0;
    bool counter = false;  // the defender's strike back is playing
    float impact = 0.0f, end = 0.0f;
    bool landed = false;   // this step's roll is in
    size_t owner = 0;      // the scene unit whose action this is
    bool affinityDone = false;  // the action's first step is over: its neighbours' affinity was built
    bool bolt = false;     // this strike is an Arcano bolt (CombatFx): it lands when the bolt arrives
    std::string element;   // the bolt's element (its palette)
    bool arrow = false;    // this strike is a bow shot: the arrow flies and lands on the hit
    bool interrupt = false;  // a sentinel's thrust out of its turn (Vigília da Sentinela): nothing of the owner's turn is spent
  };

  // What a unit's blows look like (CombatFx): its weapon's type (the heroes'), else its class (Engine2 exports no weapon
  // for enemies: milícia, the Butcher, captains, brigands, rogues and assassins carry blades; pikemen and lancers spears;
  // beasts, trolls, the dead and the rest strike blunt).
  enum class WeaponStyle { Blunt, Blade, Thrust };
  static WeaponStyle weaponStyle(const e2::BattleUnit& u) {
    const std::string& w = u.weaponType;
    if (w == "sword" || w == "dagger" || w == "axe" || w == "blade" || w == "scimitar" || w == "katana") return WeaponStyle::Blade;
    if (w == "spear" || w == "lance" || w == "pike" || w == "polearm" || w == "halberd") return WeaponStyle::Thrust;
    if (!w.empty()) return WeaponStyle::Blunt;
    static const char* kBlades[] = {"miliciaV2", "milicia", "theButcher", "captain", "brigand", "rogue", "assassin", "swordsman"};
    static const char* kSpears[] = {"basePikeman", "pikeman", "lancer"};
    for (const char* c : kBlades)
      if (u.classId == c) return WeaponStyle::Blade;
    for (const char* c : kSpears)
      if (u.classId == c) return WeaponStyle::Thrust;
    return WeaponStyle::Blunt;
  }
  // When the arrow leaves the bow in a unit's attack pose (seconds of the pose's own clock, before its pace), from the
  // first art frame without the arrow on the string (looked up frame by frame in Engine2's art, 2026-10-17): Neera's
  // atk / atk-left 27 of 36, the crossbowman's atk 21 (the bolt's streak is drawn in 21-22), the bandit crossbowman's 18,
  // the recruit archer's 3 of 4 (Engine2's short-cut hit frame). Unknown art: none (the pose's impact share stays).
  std::optional<float> arrowRelease(size_t ui, const std::string& pose) const {
    const e2::Hero* entry = ui < unitCatalog_.size() ? unitCatalog_[ui] : nullptr;
    if (!entry) return std::nullopt;
    const e2::HeroPose* left = unitFacing_[ui] == -1 ? entry->find(pose + "Left") : nullptr;
    const e2::HeroPose* p = left && !left->pace.empty() ? left : entry->find(pose);
    if (!p || p->pace.empty()) return std::nullopt;
    static const std::pair<const char*, int> kRelease[] = {{"neera/neera-v2-001:atk", 27}, {"neera/neera-v2-001:atk-left", 27},
                                                           {"BaseCrossbowman:atk", 21},    {"BrigandCrossbowman:atk", 18},
                                                           {"archerRecruit:atk", 3}};
    const std::string id = p->set + ":" + p->clip;
    for (const auto& [name, frame] : kRelease) {
      if (id != name) continue;
      for (size_t tick = 0; tick < p->pace.size(); ++tick)
        if (p->pace[tick] >= frame - 1) return static_cast<float>(tick) / 240.0f;
    }
    return std::nullopt;
  }
  // A strike is made with the off-hand weapon (Engine2 offHandStrike): the unit's own off-hand attack (its step carries the
  // weapon's dice), or a counter by a defender whose off-hand weapon reaches the attacker.
  bool strikeIsOffHand(const e2::BattleStep& s, bool counter) const {
    if (s.type != e2::BattleStep::Type::Combat) return false;
    if (!counter) return s.customDice;
    const e2::BattleUnit& actor = battle_->units()[s.def];
    const e2::BattleUnit& target = battle_->units()[s.att];
    return actor.combat.offHandWeapon && e2::Battle::distance({actor.x, actor.y}, {target.x, target.y}) <= actor.combat.offMaxRange;
  }

  // A strike is a bow shot (Combat.cpp resolveStrike arrowShot: an archer's plain attack or counter, never off-hand).
  bool strikeIsArrow(const e2::BattleStep& s, bool counter) const {
    if (s.type != e2::BattleStep::Type::Combat || (!counter && !s.kind.empty())) return false;
    const e2::BattleUnit& actor = battle_->units()[counter ? s.def : s.att];
    return actor.combat.arrow && !strikeIsOffHand(s, counter);
  }

  // A strike's blow is an arcane bolt (Combat.cpp resolveStrike: an arcane caster's plain attack or counter, never an
  // arrow, never an off-hand counter); its element (the staff's, else arcane).
  bool strikeIsBolt(const e2::BattleStep& s, bool counter, std::string& element) const {
    if (s.type != e2::BattleStep::Type::Combat || (!counter && !s.kind.empty())) return false;
    const e2::BattleUnit& actor = battle_->units()[counter ? s.def : s.att];
    if (strikeIsOffHand(s, counter) || actor.combat.arrow || !(actor.combat.arcane || actor.combat.staff)) return false;
    element = actor.combat.staff ? actor.combat.staffElement : "arcane";
    return true;
  }

  // How long a unit's pose plays (its one-shot end, else its loop), seconds; 0.8 s when it has none.
  float poseSeconds(size_t ui, const std::string& pose) const {
    const e2::Hero* entry = ui < unitCatalog_.size() ? unitCatalog_[ui] : nullptr;
    const e2::HeroPose* p = entry ? entry->find(pose) : nullptr;
    if (!p || p->pace.empty()) return 0.8f;
    return static_cast<float>(p->doneAt >= 0 ? p->doneAt : static_cast<int>(p->pace.size())) / 240.0f;
  }
  // How far into a unit's strike pose the blow lands: the pose's own moment (art cut from video), else kImpactShare.
  float poseImpact(size_t ui, const std::string& pose) const {
    const e2::Hero* entry = ui < unitCatalog_.size() ? unitCatalog_[ui] : nullptr;
    const e2::HeroPose* p = entry ? entry->find(pose) : nullptr;
    return p && p->impact >= 0 ? p->impact : kImpactShare;
  }
  // A blow between two units side by side (the struck one's nearest hex, for bodies bigger than one hex).
  bool strikeBeside(const e2::BattleStep& s) const {
    const e2::BattleUnit& a = battle_->units()[s.att];
    const e2::BattleUnit& d = battle_->units()[s.def];
    int nearest = 1 << 20;
    for (const e2::BattleHex& h : e2::Battle::footprint(d, d.x, d.y))
      for (const e2::BattleHex& g : e2::Battle::footprint(a, a.x, a.y)) nearest = std::min(nearest, e2::Battle::distance(g, h));
    return nearest <= 1;
  }

  // Turn a unit to face a hex: on screen (its card) and on the board (Engine2 faceSpriteToward).
  void faceToward(size_t ui, size_t otherUi) {
    const size_t b = static_cast<size_t>(unitBattle_[ui]), o = static_cast<size_t>(unitBattle_[otherUi]);
    e2::BattleUnit& u = battle_->units()[b];
    const e2::BattleUnit& v = battle_->units()[o];
    if (u.x == v.x && u.y == v.y) return;
    const auto [hx, hy] = e2::heading({u.x, u.y}, {v.x, v.y});
    u.faceDx = hx, u.faceDy = hy;
    const glm::vec2 d = unitPos_[otherUi] - unitPos_[ui];
    const float yaw = flat_ ? 0.0f : cam_.yaw;
    const float screenX = d.x * std::cos(yaw) - d.y * std::sin(yaw);
    if (std::abs(screenX) > 0.05f) unitFacing_[ui] = screenX > 0.0f ? 1 : -1;
  }

  // `offHand`: the blow is the off-hand weapon's (Engine2 commitOffHandAction: its own dice, its own sheet).
  void attack(size_t attackerUi, size_t defenderUi, bool offHand = false) {
    e2::BattleStep strike;
    strike.type = e2::BattleStep::Type::Combat;
    strike.att = static_cast<size_t>(unitBattle_[attackerUi]);
    strike.def = static_cast<size_t>(unitBattle_[defenderUi]);
    if (offHand) {  // the dagger's own dice, or the shield's bash (a 70% chance to stun)
      if (const std::optional<e2::BattleStep> off = e2::offHandStep(*battle_, strike.att, strike.def)) strike = *off;
    }
    offHandMode_ = false;
    play({strike}, attackerUi);
  }

  // Whether the plain attack of the picked unit on this foe, from where it stands, is made with the off-hand weapon: the
  // foe is within that weapon's reach and the player armed "Mão Secundária", or the unit wields a bow (Engine2: a bow user
  // with a dagger strikes with it by default whenever the foe is in its reach, it always outhits the arrow).
  bool offHandWanted(size_t unitBattle, size_t foeBattle) const {
    const e2::BattleUnit& u = battle_->units()[unitBattle];
    if (!u.combat.offHandWeapon && !u.combat.offHandShield) return false;
    if (!e2::canOffHandAct(*battle_, unitBattle, {u.x, u.y}, foeBattle)) return false;
    return offHandMode_ || (u.combat.arrow && u.combat.offHandWeapon);
  }
  // The forecast of a plain attack (or of the off-hand action when it is the one wanted): a shield's bash is the main weapon's blow
  // without its mastery (Engine2 makeForecast useWeaponSkill false).
  e2::Forecast forecastFor(size_t unitBattle, size_t foeBattle, e2::BattleHex from) const {
    const bool off = offHandFrom(unitBattle, foeBattle, from);
    const bool shield = off && battle_->units()[unitBattle].combat.offHandShield;
    return e2::forecast(*battle_, unitBattle, from, foeBattle, off && !shield, !shield);
  }
  // The forecast's off-hand flag for a strike from `from`: only the hex the unit stands on can be struck with the off hand.
  bool offHandFrom(size_t unitBattle, size_t foeBattle, e2::BattleHex from) const {
    const e2::BattleUnit& u = battle_->units()[unitBattle];
    return from.x == u.x && from.y == u.y && offHandWanted(unitBattle, foeBattle);
  }

  // `interrupt`: a sentinel's thrust before an enemy's blow (Vigília da Sentinela): it is nobody's action (no bleed roll, no
  // hunger, the owner's turn untouched).
  void play(std::vector<e2::BattleStep> steps, size_t ownerUi, bool interrupt = false) {
    if (steps.empty()) return;
    Exchange x;
    x.steps = std::move(steps);
    x.owner = ownerUi;
    x.interrupt = interrupt;
    exchange_ = std::move(x);
    reach_.clear(), reachPaths_.clear(), targets_.clear();
    spellMode_.clear(), spellCells_.clear(), spellAim_.clear();
    spellArmed_.reset(), pendingFoe_ = -1;
    // A bleeding unit takes 1D8 whenever it acts (Engine2 applyBleedingActionDamage): once for the whole action.
    if (!interrupt) {
      const size_t ob = static_cast<size_t>(unitBattle_[ownerUi]);
      std::vector<e2::BattleEffect> fx;
      const bool alive = e2::bleedOnAction(*battle_, ob, false, rng_, fx);
      showEffects(fx);
      if (!alive) {  // bled out before the blow: the action is dropped
        exchange_.reset();
        battle_->units()[ob].moved = true;
        select(-1);
        return;
      }
    }
    startStep(*exchange_);
  }

  // The step's pose starts: the striker turns to its target (and a struck foe to the striker).
  void startStep(Exchange& x) {
    const e2::BattleStep& s = x.steps[x.step];
    const int striker = sceneUnitOf(static_cast<int>(x.counter ? s.def : s.att));
    using StepType = e2::BattleStep::Type;
    const bool support = s.type == StepType::Heal || s.type == StepType::CureDisease || s.type == StepType::Food || s.type == StepType::Potion;
    int target = -1;
    if (s.type == StepType::Food || s.type == StepType::Move || s.type == StepType::Effect) target = -1;  // around the caster / a walk: nobody to turn to
    else if (s.type != StepType::Spell) target = sceneUnitOf(static_cast<int>(x.counter ? s.att : s.def));
    else if (!s.ids.empty()) target = sceneUnitOf(static_cast<int>(s.ids[0]));
    if (striker >= 0 && target >= 0 && striker != target) {
      faceToward(static_cast<size_t>(striker), static_cast<size_t>(target));
      if (!support && !s.rearSure) faceToward(static_cast<size_t>(target), static_cast<size_t>(striker));  // Salto de Vara: struck from behind, unseen
    }
    // A potion is drunk or handed over with no pose of its own (Engine2 applyPotion); the cures and the food play the
    // healing pose.
    std::string pose = s.type == StepType::Combat ? (strikeIsOffHand(s, x.counter) ? "attackShort" : "attack") : s.type == StepType::Potion ? "" : e2::spellPose(s.kind);
    if (pose == "attackShort" && striker >= 0 && !hasPose(static_cast<size_t>(striker), pose)) pose = "attack";  // no off-hand sheet: the main one
    if (striker >= 0 && !pose.empty() && !hasPose(static_cast<size_t>(striker), pose)) pose = hasPose(static_cast<size_t>(striker), "cast") ? "cast" : "attack";
    // Neera's Casting animation plays on every skill she uses, the bow skills too (the user, 2026-10-19); her plain attacks and
    // counters keep the shot. A skill is any step with a kind that is not a counter, a potion or a walk.
    if (striker >= 0 && !x.counter && !s.kind.empty() && s.type != StepType::Potion && s.type != StepType::Move && !pose.empty() &&
        hasPose(static_cast<size_t>(striker), "cast")) {
      const size_t su = static_cast<size_t>(striker);
      if (su < unitCatalog_.size() && unitCatalog_[su] && unitCatalog_[su]->key == "neera") pose = "cast";
    }
    // A counter on a foe beside it: the unit's staff blow when it has one (Voss; the user, 2026-10-17: "he will counter with
    // his staff when the enemy is close ranged, and ... the regular mage ATT when in distance"). No bolt flies from it.
    const bool staffCounter = s.type == StepType::Combat && x.counter && striker >= 0 &&
                              hasPose(static_cast<size_t>(striker), "counterStaff") && strikeBeside(s);
    if (staffCounter) pose = "counterStaff";
    const float length = (striker >= 0 && !pose.empty() ? poseSeconds(static_cast<size_t>(striker), pose) : 0.6f) / actionRate();
    x.impact = now_ + length * (striker >= 0 && !pose.empty() ? poseImpact(static_cast<size_t>(striker), pose) : kImpactShare);
    x.end = now_ + length + kExchangeGap / actionRate();
    x.landed = false;
    if (s.type == StepType::Move && (s.kind == "leapingStrike" || s.kind == "poleVault") && striker >= 0 && s.path.size() >= 2) {
      // Salto Devastador: the rules stand the unit on the landing at once; the card flies there on an arc (leapOffset) while
      // the trail of steel light follows it and the crater opens as it lands (SpellEffects::leapFlight). Salto de Vara (the
      // Lanceiro's): the same flight, higher, over the foe the next step strikes, the lance planted beside it (vaultFlight).
      const bool vault = s.kind == "poleVault";
      const size_t ui = static_cast<size_t>(striker);
      const e2::BattleUnit& bu = battle_->units()[s.att];
      const SpellEffects::Spot takeOff = unitSpot(ui);
      const glm::vec2 from = unitPos_[ui], to = anchorWorld(bu, s.path.back());
      const float hexes = static_cast<float>(e2::Battle::distance(s.path.front(), s.path.back()));
      const float flight = (vault ? 0.5f : 0.34f + 0.05f * hexes) / actionRate();
      unitLeap_[ui] = {now_, now_ + flight, from, to, vault ? 2.0f : 1.2f + 0.25f * hexes};
      unitPos_[ui] = to, unitWalk_[ui] = Walk{}, unitMoved_[ui] = 1;
      const glm::vec2 d = to - from;
      const float yaw = flat_ ? 0.0f : cam_.yaw;
      if (const float screenX = d.x * std::cos(yaw) - d.y * std::sin(yaw); std::abs(screenX) > 0.05f) unitFacing_[ui] = screenX > 0.0f ? 1 : -1;
      if (vault) {
        const int foe = x.step + 1 < x.steps.size() ? sceneUnitOf(static_cast<int>(x.steps[x.step + 1].def)) : -1;
        spellFx_.vaultFlight(takeOff, foe >= 0 ? unitSpot(static_cast<size_t>(foe)) : hexSpot(s.path.back()), hexSpot(s.path.back()), now_, now_ + flight, striker);
      } else {
        spellFx_.leapFlight(takeOff, hexSpot(s.path.back()), now_, now_ + flight, striker);
      }
      x.impact = now_;
      x.end = now_ + flight + 0.04f;
      return;
    }
    if (s.type == StepType::Spell && s.kind == "leapingStrike") {  // the Salto's shockwave: lands at once after the blow, no pose of its own
      x.impact = now_;
      x.end = now_ + 0.3f / actionRate();
      return;
    }
    if (s.type == StepType::Move) {  // a charge: the rules play at once (the unit is on its last hex), the card dashes the route
      const float dash = striker >= 0 ? animateAlong(static_cast<size_t>(striker), s.path, stepSeconds() * 0.5f) : 0.0f;
      x.impact = now_;
      x.end = now_ + dash + kExchangeGap / actionRate();
      return;
    }
    if (striker >= 0 && !pose.empty()) unitShot_[static_cast<size_t>(striker)] = {pose, now_, false, actionRate()};
    // The casting light (Engine2 Battle3D syncCastLight): every caster while its cast pose plays, in its spell's colour (Malrec,
    // Salazar and Voss in their own); swings, shoves and arrow shots are not casts (NOT_CASTS). Malrec alone also lights his attacks
    // and counters, in yellow.
    if (striker >= 0 && !pose.empty()) {
      static const std::unordered_set<std::string> kNotCasts = {"doubleStrike", "cleave", "piercingThrust", "sweep", "trip", "shoulderSmash",
                                                                "stampede", "tendrilSwipe", "longShot", "bloodyShot", "multiShot", "piercing",
                                                                "bullRush", "shieldBash", "executionerStrike"};
      const size_t su = static_cast<size_t>(striker);
      const std::string hero = su < unitCatalog_.size() && unitCatalog_[su] ? unitCatalog_[su]->key : std::string();
      const bool malrec = hero == "malrec";
      const bool attack = s.type == StepType::Combat || x.counter || pose.rfind("attack", 0) == 0 || pose.rfind("counter", 0) == 0 ||
                          (!s.kind.empty() && e2::spellPose(s.kind) == "attack");  // a weapon skill played in the cast pose (Neera's) is no spell light
      const bool cast = !attack && s.type != StepType::Potion && s.type != StepType::Move && !kNotCasts.contains(s.kind);
      const bool heal = s.type == StepType::Heal || s.type == StepType::CureDisease || s.type == StepType::Food;
      if (cast || (attack && malrec)) glows_.push_back({su, now_, now_ + length, castTint(s.kind, heal, hero, attack)});
    }
    // A spell waits for its caster's whole cast pose (the user, 2026-10-17: "as magias esperem a animação dos sprites de
    // casting antes de aparecerem em todos os casos"): its effect, its sound and its hit come after the pose. Weapon skills,
    // plain blows and potions keep the blow inside the pose.
    const bool magic = striker >= 0 && !x.counter && !s.kind.empty() && !pose.empty() && s.type != StepType::Combat &&
                       s.type != StepType::Potion && e2::spellPose(s.kind) != "attack";
    const float fxStart = magic ? now_ + length : now_;
    auto afterPose = [&](e2::Json cues) {
      if (magic && cues.is_array())
        for (e2::Json& c : cues) c["delay"] = c.value("delay", 0.0f) + length;
      return cues;
    };
    // The Arcano attack: energy gathers at the hand while the pose winds up, the bolt leaves at the pose's release and
    // the blow lands (the roll, the numbers, the hit react) when it arrives.
    x.bolt = striker >= 0 && target >= 0 && !staffCounter && strikeIsBolt(s, x.counter, x.element);
    if (x.bolt) {
      const SpellEffects::Spot from = unitSpot(static_cast<size_t>(striker)), to = unitSpot(static_cast<size_t>(target));
      const float release = x.impact;
      x.impact = release + CombatFx::flightSeconds(from.feet, to.feet);
      x.end = std::max(x.end, x.impact + 0.6f + kExchangeGap / actionRate());
      spellFx_.combat().bolt(from.feet, cardHeightOf(from), to.feet, cardHeightOf(to), now_, release, x.impact, CombatPalette::of(x.element.c_str()));
    }
    // A bow's plain attack or counter shoots the arrow Tiro Longo already flies (SpellEffects::arrow), landing on the hit.
    x.arrow = !x.bolt && striker >= 0 && target >= 0 && strikeIsArrow(s, x.counter);
    if (x.arrow) {
      // It leaves the bow on the frame the art lets it go (arrowRelease), and lands one flight later (the flight
      // SpellEffects::arrow keeps: 1/22 s per unit, 0.12..0.4 s); the archer's release sound with it.
      const SpellEffects::Spot from = unitSpot(static_cast<size_t>(striker)), to = unitSpot(static_cast<size_t>(target));
      if (const std::optional<float> release = arrowRelease(static_cast<size_t>(striker), pose)) {
        const float flight = std::clamp(glm::length(to.body - from.body) / 22.0f, 0.12f, 0.4f);
        x.impact = now_ + *release / actionRate() + flight;
        x.end = std::max(x.end, x.impact + 0.5f + kExchangeGap / actionRate());
        e2::Json cues = sounds(x.counter ? s.def : s.att, "release");
        if (cues.is_array())
          for (e2::Json& c : cues) c["delay"] = c.value("delay", 0.0f) + *release / actionRate();
        playCues(cues);
      }
      spellFx_.cast("longShot", std::max(spellLook("longShot", static_cast<size_t>(striker)), 1), unitSpot(static_cast<size_t>(striker)),
                    {unitSpot(static_cast<size_t>(target))}, now_, x.impact, striker);
    }
    // Its sound as the pose begins (engine.ts startSeq; a counter's cue as the counter begins, stepCombat; the cures and
    // Criar Comida e Água: healBy).
    if (striker >= 0) {
      const size_t sb = x.counter ? s.def : s.att;
      if (s.type == StepType::Combat) playCues(sounds(sb, x.counter ? "counter" : "attack"));
      else if (s.type == StepType::Heal || s.type == StepType::CureDisease || s.type == StepType::Food) playCues(afterPose(sounds(sb, "heal")));
      else if (const e2::Json& spells = sounds(sb, "spells"); spells.is_object() && spells.contains(s.kind)) playCues(afterPose(spells[s.kind]));
      else if (s.kind == "iceStorm" && spells.is_object()) {  // a cultist's Tempestade de Gelo (ours: Engine2 records none for it): its own cast
        for (const char* other : {"lightning", "magicMissile", "frost"})
          if (spells.contains(other)) {
            playCues(afterPose(spells[other]));
            break;
          }
      }
    }
    // The spell's visual, laid out so it lands on the impact (a counter or a plain attack has none: its blow's sparks
    // come when it lands).
    if (striker >= 0 && !x.counter && !s.kind.empty()) {
      std::vector<SpellEffects::Spot> spots;
      const e2::Battle::IceZone* storm = s.kind == "iceStorm" && !s.tiles.empty() ? iceZoneOf(s.tiles.front(), battle_->units()[s.att].id) : nullptr;
      if (storm) {  // Tempestade de Gelo: the storm covers the field the rules just laid (the step aims its centre only)
        for (const e2::BattleHex& h : storm->cells) spots.push_back(hexSpot(h));
      } else if (s.kind == "warCry" || s.kind == "steelWhirlwind" || s.kind == "empowerSummons" || s.kind == "sacrificeBond") {  // Grito de Guerra: the allies it rallies (the caster among them); Redemoinho: the foes it reaches; the Conjurador's: its summons
        for (size_t id : s.ids)
          if (const int ui = sceneUnitOf(static_cast<int>(id)); ui >= 0) spots.push_back(unitSpot(static_cast<size_t>(ui)));
      } else if (s.kind == "turnUndead") {  // Expulsar Mortos-Vivos: the area's hexes; only the undead standing in it keep their card height (they are struck)
        for (const e2::BattleHex& h : s.tiles) {
          SpellEffects::Spot spot = hexSpot(h);
          const bool undead = std::any_of(battle_->units().begin(), battle_->units().end(),
                                          [&](const e2::BattleUnit& u) { return u.alive && u.undead && u.x == h.x && u.y == h.y; });
          if (!undead) spot.cardHeight = 0.0f;
          spots.push_back(spot);
        }
      } else if (s.kind == "circleOfHealing" || s.kind == "judgment" || s.kind == "boneLance" || s.kind == "detonateSummon" || s.kind == "nightmare") {
        // the Curandeiro's and the Conjurador's areas: the hexes; only the units it reaches (`ids`, and the summon that blows up) keep their card height
        for (const e2::BattleHex& h : s.tiles) {
          SpellEffects::Spot spot = hexSpot(h);
          auto standsHere = [&](size_t id) {
            const e2::BattleUnit& u = battle_->units()[id];
            return u.alive && u.x == h.x && u.y == h.y;
          };
          const bool reached = std::any_of(s.ids.begin(), s.ids.end(), standsHere) || (s.sacrifice && standsHere(*s.sacrifice));
          if (!reached) spot.cardHeight = 0.0f;
          spots.push_back(spot);
        }
      } else if (s.type == StepType::Spell || s.type == StepType::Effect) {
        for (const e2::BattleHex& h : s.tiles) spots.push_back(hexSpot(h));
      } else if (s.type == StepType::Food) {
        for (size_t id : s.ids)  // everyone it feeds
          if (const int ui = sceneUnitOf(static_cast<int>(id)); ui >= 0) spots.push_back(unitSpot(static_cast<size_t>(ui)));
      } else if (target >= 0) {
        spots.push_back(unitSpot(static_cast<size_t>(target)));
      }
      // A version with Engine2's own timeline (Míssil Mágico 02) lands when it hits, as Engine2's rules wait for it.
      const SpellEffects::Spot casterSpot = unitSpot(static_cast<size_t>(striker));
      const int look = spellLook(s.kind, static_cast<size_t>(striker));
      if (magic) {  // the effect starts as the pose ends and runs its own lead to the hit
        x.impact = fxStart + spellFx_.leadIn(s.kind, look, casterSpot, spots);
        x.end = std::max(x.end, x.impact + 0.8f + kExchangeGap / actionRate());
      } else if (const float delay = spellFx_.impactDelay(s.kind, look, casterSpot, spots); delay > 0.0f) {
        x.impact = now_ + delay;
        x.end = std::max(x.end, x.impact + 0.8f + kExchangeGap);
      }
      // Teia dos Sonhos 02: the units standing in the zone are wrapped; the web stands for as long as the rules keep the zone (syncWebs).
      const e2::Battle::WebZone* webZone = s.kind == "webOfDreams" && look == 2 ? webZoneOf(s.tiles) : nullptr;
      const int webBefore = spellFx_.newestWeb();
      if (webZone) spellFx_.webUnits(webUnitsIn(*webZone));
      if (s.type == StepType::Potion && !spots.empty()) {
        // its colour by what it does (Engine2 POTIONS effect)
        const std::string effect = s.potion == "disease" ? "disease" : s.potion.starts_with("mana") ? "mana" : "heal";
        spellFx_.potion(effect, look, spots.front(), now_, x.impact, striker);
      } else if (spellFx_.cast(s.kind, look, casterSpot, spots, fxStart, x.impact, striker) && storm) {
        iceFx_.push_back({storm->center, storm->casterId, spellFx_.newestStorm()});
      }
      if (webZone && spellFx_.newestWeb() != webBefore) {
        spellFx_.webStand(spellFx_.newestWeb());
        webFx_.push_back({webZone->center, webZone->radius, spellFx_.newestWeb()});
      }
      if (s.kind == "warCry" && !s.fearIds.empty()) {  // ... and its dark pulse on the foes it frightens
        std::vector<SpellEffects::Spot> foes;
        for (size_t id : s.fearIds)
          if (const int ui = sceneUnitOf(static_cast<int>(id)); ui >= 0) foes.push_back(unitSpot(static_cast<size_t>(ui)));
        spellFx_.warCryFear(foes, x.impact);
      }
      // A spell that leaves the caster's hands (Mãos Flamejantes, Sopro Venenoso; since 2026-10-18 also Míssil Mágico, Bola
      // de Fogo, Veneno Cáustico, Frost: the user, "para o resto usar a mesma regra do burning hands"; not Relâmpago, it
      // falls from the sky) not aimed at the camera: after the cast pose the caster turns its back to us (its "castBack"
      // still, the user's own picture) and the spell leaves its hands into the board, held until the effect is over.
      // Aimed toward the camera it stays as it is.
      const bool fromHands = s.kind == "burningHands" || s.kind == "poisonBreath" || s.kind == "magicMissile" || s.kind == "magicMissileV2" ||
                             s.kind == "fireball" || s.kind == "causticVenom" || s.kind == "frost";
      if (magic && fromHands && !spots.empty() && !flat_ && hasPose(static_cast<size_t>(striker), "castBack")) {
        glm::vec2 d{0.0f};
        for (const SpellEffects::Spot& t : spots) d += glm::vec2(t.feet.x - casterSpot.feet.x, t.feet.z - casterSpot.feet.z);
        d /= static_cast<float>(spots.size());
        const float yaw = cam_.yaw;
        // Into the screen and to the side both turn it (the user, 2026-10-17: "mesmo deste angulo ele precisa virar de costas",
        // then "de lado fica de costas tbm"); only an aim coming at the camera keeps the front: within 60 degrees of straight
        // toward it (a walk's front cut, updateUnits).
        const float away = -(d.x * std::sin(yaw) + d.y * std::cos(yaw));
        if (glm::length(d) > 1e-3f && away >= -0.5f * glm::length(d)) {
          OneShot& shot = unitShot_[static_cast<size_t>(striker)];
          shot.then = "castBack";
          shot.thenUntil = std::max(spellFx_.castEnd(static_cast<int>(striker)), x.impact + 0.5f);
          logInfo("{}: {} not at the camera, back turned after the cast pose ({:.2f} s)", scene_->units[static_cast<size_t>(striker)].name, s.kind, length);
        }
      }
    }
  }

  // The zones the rules keep (burning ground, the Mortalha de Sombras, a Meteoro's mark) drawn for as long as they
  // last: each one's standing effect starts when it appears and fades when the rules drop it.
  void syncZones() {
    if (!battle_) return;
    syncIceStorms();
    for (const e2::Battle::Zone& z : battle_->zones())
      if (!zoneFx_.contains(z.id)) zoneFx_[z.id] = spellFx_.zoneStart(z.kind, z.element, zoneSpots(z), now_);
    for (auto it = zoneFx_.begin(); it != zoneFx_.end();) {
      const int id = it->first;
      const bool alive = std::any_of(battle_->zones().begin(), battle_->zones().end(), [&](const e2::Battle::Zone& z) { return z.id == id; });
      if (alive) {
        ++it;
      } else {
        spellFx_.zoneEnd(it->second, now_);
        it = zoneFx_.erase(it);
      }
    }
  }
  // Teia dos Sonhos 02 (vfx/DreamWeb): the web stays over its zone for as long as the rules keep it (Engine2 draws the net and the
  // floor for every live webZone), its occupants following who stands in it now; a fight resumed with webs on the board gets
  // them already formed.
  const e2::Battle::WebZone* webZoneOf(const std::vector<e2::BattleHex>& cells) const {
    const e2::Battle::WebZone* found = nullptr;
    for (const e2::Battle::WebZone& z : battle_->webZones())
      if (z.cells.size() == cells.size() && std::equal(z.cells.begin(), z.cells.end(), cells.begin())) found = &z;  // the newest
    return found;
  }
  std::vector<SpellEffects::WebUnit> webUnitsIn(const e2::Battle::WebZone& z) const {
    std::vector<SpellEffects::WebUnit> in;
    for (size_t i = 0; i < battle_->units().size(); ++i) {
      const e2::BattleUnit& u = battle_->units()[i];
      if (!u.alive || std::none_of(z.cells.begin(), z.cells.end(), [&](const e2::BattleHex& c) { return c.x == u.x && c.y == u.y; })) continue;
      if (const int ui = sceneUnitOf(static_cast<int>(i)); ui >= 0) in.push_back({static_cast<int>(i), unitSpot(static_cast<size_t>(ui))});
    }
    return in;
  }
  void syncWebs() {
    if (!battle_) return;
    const auto& zones = battle_->webZones();
    auto same = [](const WebFx& f, const e2::Battle::WebZone& z) { return f.center == z.center && f.radius == z.radius; };
    std::vector<bool> taken(webFx_.size(), false);
    const bool resumed = !webSynced_;
    webSynced_ = true;
    for (const e2::Battle::WebZone& z : zones) {
      size_t at = webFx_.size();
      for (size_t i = 0; i < webFx_.size(); ++i)
        if (!taken[i] && same(webFx_[i], z)) {
          at = i;
          break;
        }
      if (at == webFx_.size()) {
        if (!resumed || spellsMenu_.look("webOfDreams", false) != 2 || !battle_->inBounds(z.center.x, z.center.y)) continue;
        std::vector<SpellEffects::Spot> spots;
        for (const e2::BattleHex& h : z.cells)
          if (battle_->cell(h.x, h.y).terrain != "void") spots.push_back(hexSpot(h));
        if (spots.empty()) continue;
        const int before = spellFx_.newestWeb();
        spellFx_.webUnits(webUnitsIn(z));
        spellFx_.cast("webOfDreams", 2, hexSpot(z.center), spots, now_ - 2.7f, now_ - 1.85f);
        if (spellFx_.newestWeb() == before) continue;
        spellFx_.webStand(spellFx_.newestWeb());
        webFx_.push_back({z.center, z.radius, spellFx_.newestWeb()});
        taken.push_back(true);
        continue;
      }
      taken[at] = true;
      spellFx_.webUnits(webFx_[at].fx, webUnitsIn(z));
    }
    for (size_t i = webFx_.size(); i-- > 0;)
      if (!taken[i]) {
        spellFx_.webEnd(webFx_[i].fx, now_);
        webFx_.erase(webFx_.begin() + static_cast<std::ptrdiff_t>(i));
      }
  }
  // The Vigília da Sentinela's standing effect on every sentinel in its stance (LancerFx::vigil), following it, gone when it lifts.
  void syncVigils() {
    if (!battle_) return;
    for (size_t i = 0; i < battle_->units().size(); ++i) {
      const e2::BattleUnit& u = battle_->units()[i];
      const int ui = sceneUnitOf(static_cast<int>(i));
      const bool on = u.alive && u.cond.vigilPct > 0 && ui >= 0;
      const auto it = vigilFx_.find(static_cast<int>(i));
      if (on) {
        const SpellEffects::Spot sp = unitSpot(static_cast<size_t>(ui));
        if (it == vigilFx_.end()) vigilFx_[static_cast<int>(i)] = spellFx_.lancer().vigilStart(sp.feet, sp.cardHeight, (static_cast<float>(u.cond.vigilRadius) + 0.5f) * 1.7320508f, now_);
        else spellFx_.lancer().vigilMove(it->second, sp.feet);
      } else if (it != vigilFx_.end()) {
        spellFx_.lancer().vigilEnd(it->second, now_);
        vigilFx_.erase(it);
      }
    }
  }
  // Poison and disease on every unit drawn that has them (StatusFx: one effect per condition, both layered on a unit with both),
  // following it, faded out when the rules lift the condition, the unit dies or its card is not drawn (an enemy out of sight).
  void syncStatus() {
    if (!battle_) return;
    for (size_t i = 0; i < battle_->units().size(); ++i) {
      const e2::BattleUnit& u = battle_->units()[i];
      const int ui = sceneUnitOf(static_cast<int>(i));
      const bool shown = u.alive && ui >= 0 && static_cast<size_t>(ui) < unitCard_.size() && unitCard_[static_cast<size_t>(ui)].drawn;
      for (int c = 0; c < 2; ++c) {
        const bool on = shown && (c == 0 ? u.poisoned : u.diseased);
        const int key = static_cast<int>(i) * 2 + c;
        const auto it = statusFx_.find(key);
        if (on) {
          const SpellEffects::Spot sp = unitSpot(static_cast<size_t>(ui));
          if (it == statusFx_.end())
            statusFx_[key] = spellFx_.status().start(c == 0 ? StatusFx::Kind::Poison : StatusFx::Kind::Disease, sp.feet, sp.cardHeight, now_);
          else spellFx_.status().move(it->second, sp.feet, sp.cardHeight);
        } else if (it != statusFx_.end()) {
          spellFx_.status().end(it->second, now_);
          statusFx_.erase(it);
        }
      }
    }
  }
  // Imagem Espelhada's standing illusions (ConjurerFx::mirror*) beside every unit that has them, following it, one fewer when a copy is
  // struck, gone when they end.
  void syncMirrors() {
    if (!battle_) return;
    for (size_t i = 0; i < battle_->units().size(); ++i) {
      const e2::BattleUnit& u = battle_->units()[i];
      const int ui = sceneUnitOf(static_cast<int>(i));
      const bool on = u.alive && u.cond.mirrorCopies > 0 && ui >= 0;
      const auto it = mirrorFx_.find(static_cast<int>(i));
      if (on) {
        const SpellEffects::Spot sp = unitSpot(static_cast<size_t>(ui));
        if (it == mirrorFx_.end()) mirrorFx_[static_cast<int>(i)] = spellFx_.conjurer().mirrorStart(sp.feet, sp.cardHeight, u.cond.mirrorCopies, now_);
        else spellFx_.conjurer().mirrorUpdate(it->second, sp.feet, u.cond.mirrorCopies, now_);
      } else if (it != mirrorFx_.end()) {
        if (ui >= 0) spellFx_.conjurer().mirrorUpdate(it->second, unitSpot(static_cast<size_t>(ui)).feet, 0, now_);  // the last copies shatter
        spellFx_.conjurer().mirrorEnd(it->second, now_);
        mirrorFx_.erase(it);
      }
    }
  }
  // The sentinels' thrusts before `attacker`'s blow on `targets` (e2::vigilInterrupts), played as one exchange of their own.
  bool playInterrupts(size_t attacker, const std::vector<size_t>& targets) {
    const std::vector<e2::BattleStep> steps = e2::vigilInterrupts(*battle_, attacker, targets);
    if (steps.empty()) return false;
    const int owner = sceneUnitOf(static_cast<int>(steps.front().att));
    if (owner < 0) return false;
    tip_ = battle_->units()[steps.front().att].name + tr(" interrompe o ataque!", " interrupts the attack!"), tipAt_ = now_;
    play(steps, static_cast<size_t>(owner), true);
    return exchange_.has_value();
  }
  glm::vec3 feetAt(glm::vec2 p) const { return {p.x, groundAt(p.x, p.y), p.y}; }
  // The Lanceiro's board moves (docs/SKILLS_LANCER.md): a body thrown, pulled, dragged or swapped stands on its new hex at
  // once in the rules; its card flies there on an arc (unitLeap_, the effect's `arc`) and the lance's effect follows it
  // (SpellEffects::lancerFlight / swapFlight). Returns when the last flight ends (now_ when nothing flies).
  float animateFlights(const std::vector<e2::BattleEffect>& fx, const e2::BattleStep& s) {
    float latest = now_;
    struct Mover {
      size_t ui;
      glm::vec3 from, to;
      float arc, finish;
    };
    std::vector<Mover> movers;
    for (const e2::BattleEffect& e : fx) {
      if (!e.pushed || e.arc < 0.0f) continue;
      const int ui = sceneUnitOf(static_cast<int>(e.unit));
      if (ui < 0) continue;
      const size_t u = static_cast<size_t>(ui);
      const e2::BattleUnit& bu = battle_->units()[e.unit];
      const glm::vec2 from = unitPos_[u], to = anchorWorld(bu, {bu.x, bu.y});
      const float hexes = glm::length(to - from) / 1.7320508f;
      const bool swap = s.kind == "swapPost" || s.kind == "placeSwap";
      const float seconds = (s.kind == "spearToss" ? 0.42f + 0.06f * hexes : swap ? 0.42f : 0.2f + 0.11f * hexes) / actionRate();
      unitLeap_[u] = {now_, now_ + seconds, from, to, e.arc};
      unitPos_[u] = to, unitWalk_[u] = Walk{}, unitMoved_[u] = 1;
      if (s.kind == "impaleDrag" || swap) {  // the runners face the way they go
        const glm::vec2 d = to - from;
        const float yaw = flat_ ? 0.0f : cam_.yaw;
        if (const float screenX = d.x * std::cos(yaw) - d.y * std::sin(yaw); std::abs(screenX) > 0.05f) unitFacing_[u] = screenX > 0.0f ? 1 : -1;
      }
      movers.push_back({u, feetAt(from), feetAt(to), e.arc, now_ + seconds});
      latest = std::max(latest, now_ + seconds);
    }
    if (movers.empty()) return latest;
    auto spotAt = [&](size_t u, glm::vec3 feet) {
      SpellEffects::Spot sp = unitSpot(u);
      sp.body = feet + (sp.body - sp.feet), sp.feet = feet;
      return sp;
    };
    const int striker = sceneUnitOf(static_cast<int>(s.att));
    if (s.kind == "swapPost" || s.kind == "placeSwap") {
      // the dash's effect came with the cast (castEffect, at the impact); only the cards fly here
    } else if (s.kind == "impaleDrag") {
      const Mover *foe = nullptr, *lancer = nullptr;
      for (const Mover& m : movers) {
        if (static_cast<int>(m.ui) == striker) lancer = &m;
        else foe = &m;
      }
      if (foe && striker >= 0)
        spellFx_.lancerFlight(s.kind, spotAt(foe->ui, foe->from), spotAt(foe->ui, foe->to), foe->arc, now_, foe->finish,
                              lancer ? spotAt(lancer->ui, lancer->from) : unitSpot(static_cast<size_t>(striker)),
                              lancer ? spotAt(lancer->ui, lancer->to) : unitSpot(static_cast<size_t>(striker)), false);
    } else {
      const bool collided = std::any_of(fx.begin(), fx.end(), [](const e2::BattleEffect& e) { return e.delay > 0.0f; });
      for (const Mover& m : movers)
        spellFx_.lancerFlight(s.kind, spotAt(m.ui, m.from), spotAt(m.ui, m.to), m.arc, now_, m.finish,
                              striker >= 0 ? unitSpot(static_cast<size_t>(striker)) : spotAt(m.ui, m.from), SpellEffects::Spot{}, collided);
    }
    return latest;
  }
  // The Tempestade de Gelo fields (Battle::iceZones, Engine2's iceStormZones). A storm starts with its cast (startStep); it
  // fades when the rules drop its field (battleSpellOverlay3d removeBlizzard). The fields already on the board at the first
  // frame (a resumed fight) get theirs then, as Engine2 spawns one for every live field.
  const e2::Battle::IceZone* iceZoneOf(e2::BattleHex centre, const std::string& caster) const {
    const e2::Battle::IceZone* found = nullptr;
    for (const e2::Battle::IceZone& z : battle_->iceZones())
      if (z.center == centre && z.casterId == caster) found = &z;  // the newest
    return found;
  }
  void syncIceStorms() {
    if (!iceSynced_) {
      iceSynced_ = true;
      for (const e2::Battle::IceZone& z : battle_->iceZones()) {
        int caster = -1;
        for (size_t i = 0; i < battle_->units().size(); ++i)
          if (battle_->units()[i].id == z.casterId) caster = sceneUnitOf(static_cast<int>(i));
        std::vector<SpellEffects::Spot> spots;
        for (const e2::BattleHex& h : z.cells) spots.push_back(hexSpot(h));
        const int look = caster >= 0 ? spellLook("iceStorm", static_cast<size_t>(caster)) : SpellEffects::engine2Look("iceStorm");
        if (!spots.empty() && spellFx_.cast("iceStorm", look, spots.front(), spots, now_, now_ + 0.05f))
          iceFx_.push_back({z.center, z.casterId, spellFx_.newestStorm()});
      }
    }
    std::erase_if(iceFx_, [&](const IceFx& t) {
      if (iceZoneOf(t.center, t.caster)) return false;
      spellFx_.stormEnd(t.fx, now_);
      return true;
    });
  }
  // A zone's hexes as spots, the centre first.
  std::vector<SpellEffects::Spot> zoneSpots(const e2::Battle::Zone& z) const {
    std::vector<SpellEffects::Spot> spots;
    if (battle_->inBounds(z.centre.x, z.centre.y)) spots.push_back(hexSpot(z.centre));
    for (int y = 0; y < battle_->rows(); ++y)
      for (int x = 0; x < battle_->cols(); ++x) {
        const e2::BattleHex h{x, y};
        if (h == z.centre || !z.covers(h) || battle_->cell(x, y).terrain == "void") continue;
        spots.push_back(hexSpot(h));
      }
    return spots;
  }

  // Where an effect meets a scene unit: its feet and the middle of its body (its card as drawn, else Engine2's chest
  // height 0.9).
  SpellEffects::Spot unitSpot(size_t ui) const {
    const glm::vec3 feet = unitWorld(ui);
    const CardRect& c = unitCard_[ui];
    const float body = c.drawn && !c.flat ? std::clamp(c.height * 0.45f, 0.5f, 1.4f) : 0.9f;
    return {feet, feet + glm::vec3(0.0f, body, 0.0f), c.drawn && !c.flat ? c.height : 0.0f};
  }
  // A spot's card height (a hex with nobody on it: a body's, from its body point).
  static float cardHeightOf(const SpellEffects::Spot& s) { return s.cardHeight > 0.0f ? s.cardHeight : std::max(1.2f, (s.body.y - s.feet.y) / 0.45f); }
  // ... and a hex: the unit standing on it, else its ground.
  SpellEffects::Spot hexSpot(e2::BattleHex h) const {
    for (size_t i = 0; i < unitBattle_.size(); ++i) {
      if (unitBattle_[i] < 0) continue;
      const e2::BattleUnit& u = battle_->units()[static_cast<size_t>(unitBattle_[i])];
      if (u.alive && u.x == h.x && u.y == h.y) return unitSpot(i);
    }
    const e2::Cell& c = board_->cell(h.x, h.y);
    const float x = static_cast<float>(c.x), z = static_cast<float>(c.z);
    const glm::vec3 feet{x, static_cast<float>(e2::groundHeightAt(*board_, look_, x, z)), z};
    return {feet, feet + glm::vec3(0.0f, 0.9f, 0.0f)};
  }

  // --fx-test: the hero casts the visual at the nearest enemy at the start of every cycle (once per cycle, so a fixed
  // --scene-time shows one moment of it).
  static constexpr float kFxTestCycle = 3.0f;
  void runFxTest(float time) {
    if (options_.fxTest.empty() || hero_ < 0) return;
    const int cycle = static_cast<int>(std::floor(time / kFxTestCycle));
    if (cycle == fxTestCycle_) return;
    fxTestCycle_ = cycle;
    static const char* kKinds[] = {"magicMissile",   "lightning",     "frost",          "cureMinor",      "longShot",      "piercing",    "shock",
                                   "doubleStrike",   "markOfRuin",    "creepingPlague", "lifeDrain",      "shroudOfShadows", "hemorrhage", "profaneSilence",
                                   "blackPestilence", "curseOfDecay", "soulHarvest",    "devouringVoid",  "pillarOfFlame", "iceLance",    "thunderstorm",
                                   "meteor",         "absoluteZero",  "chainLightning", "eruption",       "glacialGale",   "wrathOfTheSkies", "primordialEmber",
                                   "steelWhirlwind", "guardBreak",    "warCry",         "leapingStrike",
                                   "venomArrow",     "arrowRain",     "explosiveArrow", "cripplingShot",  "arrowStorm",    "deathShot",
                                   "spearToss",      "hookPull",      "poleVault",      "disarm",         "swapPost",      "impaleDrag",  "sentinelVigil",
                                   "circleOfHealing", "resurrection", "judgment",
                                   "placeSwap",      "boneLance",     "mirrorImage",    "detonateSummon", "sacrificeBond", "nightmare",   "empowerSummons"};
    // KIND or KIND:VERSION (the version number of the Magias menu; default the menu's choice, at least 01).
    std::string kind = options_.fxTest;
    int look = 0;
    if (const size_t colon = kind.find(':'); colon != std::string::npos) look = std::atoi(kind.c_str() + colon + 1), kind.resize(colon);
    if (kind == "all") kind = kKinds[static_cast<size_t>(cycle) % std::size(kKinds)];
    if (look <= 0) look = std::max(spellLook(kind, static_cast<size_t>(hero_)), 1);
    logInfo("fx test: {} {:02} at {:.1f} s", kind, look, static_cast<float>(cycle) * kFxTestCycle);
    if (testCombat(static_cast<size_t>(hero_), kind, static_cast<float>(cycle) * kFxTestCycle)) return;
    testCast(static_cast<size_t>(hero_), kind, look, static_cast<float>(cycle) * kFxTestCycle);
  }

  // --battle-script: each step played through the battle bar's own calls (select, startSpell, aimSpell; the turn loop
  // held), then checked once its action has played. Steps (names are the units' own):
  //   hurt                      e2::hurtScene: every unit at half HP, some sick / poisoned, all hungry, uses halved
  //   cast:CASTER:KIND:TARGET   the target is first stood beside the caster
  //   food:CASTER               Criar Comida e Água (its allies within 2 hexes fed)
  //   potion:ACTOR:ID:TARGET    the target is first stood beside the actor (or the actor itself)
  //   refuse:<a step>           the game must refuse it (nothing played, nothing spent)
  int sceneUnitNamed(const std::string& name, bool aliveOnly = true) const {
    for (size_t i = 0; i < scene_->units.size(); ++i)
      if (unitBattle_[i] >= 0 && battle_->units()[static_cast<size_t>(unitBattle_[i])].name == name &&
          (!aliveOnly || battle_->units()[static_cast<size_t>(unitBattle_[i])].alive))
        return static_cast<int>(i);
    return -1;
  }
  bool placeBeside(size_t ui, size_t other) {
    e2::BattleUnit& u = battle_->units()[static_cast<size_t>(unitBattle_[ui])];
    const e2::BattleUnit& o = battle_->units()[static_cast<size_t>(unitBattle_[other])];
    if (e2::Battle::distance({u.x, u.y}, {o.x, o.y}) == 1) return true;
    for (const e2::BattleHex& h : e2::Battle::neighbors(o.x, o.y)) {
      if (!battle_->inBounds(h.x, h.y) || !battle_->cell(h.x, h.y).passable || battle_->unitAt(h.x, h.y) >= 0) continue;
      u.x = h.x, u.y = h.y;
      const e2::Cell& c = board_->cell(h.x, h.y);
      unitPos_[ui] = {static_cast<float>(c.x), static_cast<float>(c.z)};
      unitWalk_[ui] = Walk{};
      return true;
    }
    return false;
  }
  void runScript() {
    if (options_.battleScript.empty() || scriptDone_ || !battle_ || !scene_ || readyFrames_ < 10) return;
    holdTurns = true;
    if (busy()) return;
    if (scriptCheck_) {
      const std::string why = scriptCheck_();
      scriptCheck_ = nullptr;
      if (why.empty()) logInfo("battle script: {}: PASS", scriptLabel_);
      else logError("battle script: {}: FAIL ({})", scriptLabel_, why), scriptFailed_ = true;
    }
    std::vector<std::string> steps;
    for (size_t at = 0; at <= options_.battleScript.size();) {
      size_t end = options_.battleScript.find(';', at);
      if (end == std::string::npos) end = options_.battleScript.size();
      std::string s = options_.battleScript.substr(at, end - at);
      s.erase(0, s.find_first_not_of(' '));
      s.erase(s.find_last_not_of(' ') + 1);
      if (!s.empty()) steps.push_back(s);
      at = end + 1;
    }
    if (scriptStep_ >= steps.size()) {
      scriptDone_ = true;
      logInfo("battle script: {} ({} steps)", scriptFailed_ ? "FAIL" : "PASS", steps.size());
      return;
    }
    scriptLabel_ = steps[scriptStep_++];
    std::vector<std::string> p;
    for (size_t at = 0; at <= scriptLabel_.size();) {
      size_t end = scriptLabel_.find(':', at);
      if (end == std::string::npos) end = scriptLabel_.size();
      p.push_back(scriptLabel_.substr(at, end - at));
      at = end + 1;
    }
    auto fail = [&](const std::string& why) { logError("battle script: {}: FAIL ({})", scriptLabel_, why), scriptFailed_ = true; };
    const bool refuse = p[0] == "refuse";
    if (refuse) p.erase(p.begin());
    if (p.empty()) return fail("empty step");
    if (p[0] == "hurt") {
      // the scene's wounds, sickness and hunger; the spell uses it halves are given back (the steps cast them)
      std::vector<std::array<int, 10>> uses;
      for (const e2::BattleUnit& v : battle_->units()) uses.push_back(std::to_array(v.spells));
      e2::hurtScene(*battle_);
      for (size_t i = 0; i < uses.size(); ++i) std::copy(uses[i].begin(), uses[i].end(), battle_->units()[i].spells);
      return logInfo("battle script: hurt: every unit at half HP, some sick / poisoned, everyone hungry");
    }
    if (p[0] == "wipe") {  // every enemy fallen: the end is on offer (Engine2 winAvailable)
      for (e2::BattleUnit& v : battle_->units())
        if (v.side == "enemy") v.alive = false, v.hp = 0;
      return logInfo("battle script: wipe: win rule {}, end on offer {}", battle_->win(), battle_->winAvailable());
    }
    if (p[0] == "summon" && p.size() > 2) {  // "summon:<unit>:<kind>": the unit's familiar called onto a free hex beside it (the rules' cast, no uses spent)
      const int gi = sceneUnitNamed(p[1]);
      if (gi < 0) return fail("no such unit");
      const size_t ab = static_cast<size_t>(unitBattle_[static_cast<size_t>(gi)]);
      const e2::BattleUnit& v = battle_->units()[ab];
      std::optional<e2::BattleHex> free;
      for (const e2::BattleHex& h : e2::Battle::neighbors(v.x, v.y))
        if (battle_->inBounds(h.x, h.y) && battle_->cell(h.x, h.y).passable && battle_->unitAt(h.x, h.y) < 0 && e2::spellAimValid(*battle_, ab, p[2], h)) free = h;
      if (!free) return fail("no free hex beside the summoner");
      std::string why;
      const std::optional<std::vector<e2::BattleStep>> steps = e2::castSpell(*battle_, ab, p[2], {*free}, &why, false);
      if (!steps) return fail("the summon was refused: " + why);
      std::vector<e2::BattleEffect> fx;
      e2::playSteps(*battle_, *steps, rng_, fx);
      syncSummons();
      return logInfo("battle script: summon {} {}: {} units now", p[1], p[2], battle_->units().size());
    }
    if (p[0] == "kill" && p.size() > 1) {  // "kill:<unit>": the unit falls where it stands (its card gone), for a Ressurreição
      const int gi = sceneUnitNamed(p[1]);
      if (gi < 0) return fail("no such unit");
      e2::BattleUnit& v = battle_->units()[static_cast<size_t>(unitBattle_[static_cast<size_t>(gi)])];
      v.alive = false, v.hp = 0;
      unitDiedAt_[static_cast<size_t>(gi)] = -1000.0f;
      unitShot_[static_cast<size_t>(gi)] = {};
      return logInfo("battle script: kill {}: fallen at ({}, {})", p[1], v.x, v.y);
    }
    if (p[0] == "sick" && p.size() > 2) {  // "sick:<unit>:poison|disease": the unit poisoned (Veneno Menor) or diseased, to see the effect
      const int gi = sceneUnitNamed(p[1]);
      if (gi < 0) return fail("no such unit");
      e2::BattleUnit& v = battle_->units()[static_cast<size_t>(unitBattle_[static_cast<size_t>(gi)])];
      if (p[2] == "disease") v.diseased = true;
      else v.poisoned = true, v.poisonTier = v.poisonTier.empty() ? "lesser" : v.poisonTier;
      return logInfo("battle script: sick {} {}: poisoned {} diseased {}", p[1], p[2], v.poisoned, v.diseased);
    }
    if (p[0] == "goto" && p.size() > 3) {  // "goto:<unit>:<x>:<y>": the unit set down on a hex (a waypoint, for captures)
      const int gi = sceneUnitNamed(p[1]);
      if (gi < 0) return fail("no such unit");
      e2::BattleUnit& v = battle_->units()[static_cast<size_t>(unitBattle_[static_cast<size_t>(gi)])];
      v.x = std::atoi(p[2].c_str()), v.y = std::atoi(p[3].c_str());
      unitPos_[static_cast<size_t>(gi)] = anchorWorld(v, {v.x, v.y});
      unitWalk_[static_cast<size_t>(gi)] = Walk{};
      return logInfo("battle script: goto {} ({}, {}): exit {}", p[1], v.x, v.y, battle_->activeExit() ? battle_->activeExit()->kind : "none");
    }
    if (p[0] == "lockpick" && p.size() > 2) {  // "lockpick:<unit>:<lock>": the unit set beside the lock with a Gazua, picked, and the lock opened (":hold" = not opened)
      const int gi = sceneUnitNamed(p[1]);
      const size_t li = static_cast<size_t>(std::atoi(p[2].c_str()));
      if (gi < 0 || li >= battle_->locks().size()) return fail("no such unit or lock");
      const e2::Battle::Lock& lock = battle_->locks()[li];
      const size_t ab = static_cast<size_t>(unitBattle_[static_cast<size_t>(gi)]);
      e2::BattleUnit& v = battle_->units()[ab];
      bool placed = false;
      for (const e2::BattleHex& h : e2::Battle::neighbors(lock.x, lock.y)) {
        if (!battle_->inBounds(h.x, h.y) || !battle_->cell(h.x, h.y).passable || (battle_->unitAt(h.x, h.y) >= 0 && battle_->unitAt(h.x, h.y) != static_cast<int>(ab))) continue;
        v.x = h.x, v.y = h.y;
        unitPos_[static_cast<size_t>(gi)] = anchorWorld(v, {v.x, v.y});
        unitWalk_[static_cast<size_t>(gi)] = Walk{};
        placed = true;
        break;
      }
      if (!placed) return fail("no free hex beside the lock");
      v.lockpick = std::max(v.lockpick, 1), v.acted = false, v.moved = false, v.moveUsed = 0;
      for (size_t i : battle_->turnOrder()) {  // its turn: everyone before it in the order has gone
        if (i == ab) break;
        battle_->units()[i].moved = true;
      }
      select(gi);
      if (p.size() > 3 && p[3] == "hold") return logInfo("battle script: {} beside lock {} with a Gazua, not opened", p[1], li);  // the Arrombar button on screen
      openLock();
      return logInfo("battle script: lockpick {} on lock {} ({} at {}, {}): open {}, Ember found {}", p[1], li, lock.kind, lock.x, lock.y, battle_->locks()[li].open, battle_->lootEmber());
    }
    if (p[0] == "finish") {
      // the finish prompt's yes (drawFinishPrompt), then what the result screen says: its line, its way on, each hero's HP after it
      const e2::Battle::Exit* on = battle_->activeExit();
      if (!battle_->winAvailable()) return fail("the end is not on offer");
      if (on && on->kind == "connector" && !on->targetMapId.empty() && explores_) floorRequest_ = on->targetMapId;
      else confirmFinishNow();
      std::string heroes;
      for (const HeroOutcome& h : heroOutcomes()) heroes += std::format(" {} {}->{}", h.name, h.hp, h.campHp);
      return logInfo("battle script: finish on {} ({}): result '{}', line '{}', way on '{}' ({}), fallen spawns {}, HP after:{}", on ? on->id : "no waypoint",
                     on ? on->kind : "-", battle_->result(), resultLine(), floorTarget().empty() ? floorRequest_ : floorTarget(),
                     floorTarget().empty() ? "-" : advanceLabel(), fallenSpawns().size(), heroes);
    }
    if (p[0] == "aim" && p.size() > 2) {
      // the cursor held on a hex from now on (the grid's route / target under it, for captures)
      scriptHover_ = e2::BattleHex{std::atoi(p[1].c_str()), std::atoi(p[2].c_str())};
      return logInfo("battle script: aim {} {}", scriptHover_->x, scriptHover_->y);
    }
    if (p[0] == "skills" && p.size() > 1) {  // "skills:<unit>": its status sheet with the Skills window opened over it
      const int who = sceneUnitNamed(p[1]);
      if (who < 0 || unitBattle_[static_cast<size_t>(who)] < 0) return fail("no such unit");
      openStatus(unitBattle_[static_cast<size_t>(who)]);
      statusSheet_.openSkills();
      return logInfo("battle script: skills {}", p[1]);
    }
    if (p[0] == "status" && p.size() > 1) {  // "status:<unit>": its status sheet opened (as a click on its portrait does)
      const int who = sceneUnitNamed(p[1]);
      if (who < 0 || unitBattle_[static_cast<size_t>(who)] < 0) return fail("no such unit");
      openStatus(unitBattle_[static_cast<size_t>(who)]);
      return logInfo("battle script: status {}", p[1]);
    }
    if (p[0] == "ui" && p.size() > 2) {  // "ui:<x>:<y>": a left click on the interface at a point of the 720-tall UI canvas
      ImGuiIO& io = ImGui::GetIO();
      io.AddMousePosEvent(static_cast<float>(std::atof(p[1].c_str())), static_cast<float>(std::atof(p[2].c_str())));
      io.AddMouseButtonEvent(0, true);
      io.AddMouseButtonEvent(0, false);  // the event queue trickles: down and up land on separate frames
      return logInfo("battle script: ui click {} {}", p[1], p[2]);
    }
    if (p[0] == "wheel" && p.size() > 3) {  // "wheel:<x>:<y>:<notches>": the mouse wheel over a point of the UI canvas (negative = down)
      ImGuiIO& io = ImGui::GetIO();
      io.AddMousePosEvent(static_cast<float>(std::atof(p[1].c_str())), static_cast<float>(std::atof(p[2].c_str())));
      io.AddMouseWheelEvent(0.0f, static_cast<float>(std::atof(p[3].c_str())));
      return logInfo("battle script: wheel {} at {} {}", p[3], p[1], p[2]);
    }
    if (p[0] == "sheet") {  // "sheet": what the status sheet shows now (open, unit, the unit's points)
      if (!statusSheet_.isOpen()) return logInfo("battle script: sheet closed");
      const e2::BattleUnit& v = battle_->units()[statusSheet_.unit()];
      return logInfo("battle script: sheet open on {}: points hp {} atk {} mag {} def {} dex {}, inventory {}", v.name, v.pointHp, v.pointAtk, v.pointMag,
                     v.pointDef, v.pointDex, battleInventory_ ? "open" : "closed");
    }
    if (p[0] == "click" && p.size() > 1) {
      // "click:<x>:<y>" or "click:<unit>": a left click on the hex's centre on screen, through the window's own click
      // (pick, cards first)
      e2::BattleHex at{-1, -1};
      if (p.size() > 2) at = {std::atoi(p[1].c_str()), std::atoi(p[2].c_str())};
      else if (const int who = sceneUnitNamed(p[1]); who >= 0 && unitBattle_[static_cast<size_t>(who)] >= 0)
        at = {battle_->units()[static_cast<size_t>(unitBattle_[static_cast<size_t>(who)])].x, battle_->units()[static_cast<size_t>(unitBattle_[static_cast<size_t>(who)])].y};
      if (!battle_->inBounds(at.x, at.y)) return fail("off the board");
      const int w = 1920, h = std::max(1, static_cast<int>(std::lround(1920.0f / std::max(aspect_, 0.1f))));
      const e2::Cell& c = board_->cell(at.x, at.y);
      const glm::vec3 centre{static_cast<float>(c.x), static_cast<float>(e2::groundHeightAt(*board_, look_, static_cast<float>(c.x), static_cast<float>(c.z))),
                             static_cast<float>(c.z)};
      const std::optional<ImVec2> s = screenOf(centre, static_cast<float>(w), static_cast<float>(h));
      if (!s || s->x < 0 || s->y < 0 || s->x >= w || s->y >= h) return fail("off screen");
      const std::optional<e2::BattleHex> picked = spellMode_.empty() ? pickAt(w, h, s->x, s->y) : aimPickAt(w, h, s->x, s->y);
      tip_.clear();
      clickAt(w, h, s->x, s->y);
      return logInfo("battle script: click {} {}: picked {}, spell '{}' armed {}{}, playing {}{}", at.x, at.y,
                     picked ? std::format("{},{}", picked->x, picked->y) : "nothing", spellMode_,
                     spellArmed_ ? std::format("{},{}", spellArmed_->x, spellArmed_->y) : "no", spellArmed_ ? std::format(" ({} hexes in its area)", aimArea(static_cast<size_t>(unitBattle_[static_cast<size_t>(selected_)]), *spellArmed_).size()) : "",
                     exchange_ ? "yes" : "no", tip_.empty() ? "" : " - " + tip_);
    }
    if (p[0] == "talk" && p.size() > 1) {
      // "talk:<npc>": a left click on the NPC as the player makes it (the leader walks up in free roam), then the conversation opens
      const int who = sceneUnitNamed(p[1]);
      if (who < 0 || unitBattle_[static_cast<size_t>(who)] < 0) return fail("no such unit");
      if (!npcDialog_.contains(static_cast<size_t>(unitBattle_[static_cast<size_t>(who)]))) return fail("has no conversation");
      talked_ = false;
      talkTo(static_cast<size_t>(unitBattle_[static_cast<size_t>(who)]));
      scriptCheck_ = [this, name = p[1]]() -> std::string {
        if (!talked_ && !talkRequest_) return name + "'s conversation never opened";
        for (const e2::BattleUnit& u : battle_->units())
          if (u.side == "player" && !u.summoned) return logInfo("battle script: talked to {}, {} at ({}, {})", name, u.name, u.x, u.y), std::string();
        return "";
      };
      return logInfo("battle script: talk {}", p[1]);
    }
    if (p[0] == "stairs") {  // "stairs": the finish prompt's "Sim" on the floor connector a hero stands on
      const e2::Battle::Exit* x = battle_->activeExit();
      if (!x || x->kind != "connector" || x->targetMapId.empty()) return fail("no hero on a floor connector");
      floorRequest_ = x->targetMapId;
      return logInfo("battle script: stairs: {} to {}", connectorUp(*x) ? "up" : "down", floorRequest_);
    }
    if (p[0] == "confirm") {
      // the confirm panel's Confirmar (drawConfirm), then, once it has played, who lost HP
      if (!spellArmed_ && pendingFoe_ < 0) return fail("nothing waiting for Confirmar");
      std::vector<int> hpBefore;
      for (const e2::BattleUnit& v : battle_->units()) hpBefore.push_back(v.hp);
      const std::string kind = spellMode_;
      const int caster = selected_ >= 0 ? unitBattle_[static_cast<size_t>(selected_)] : -1;
      const int usesBefore = caster >= 0 && !kind.empty() ? e2::spellUsesLeft(*battle_, static_cast<size_t>(caster), kind) : 0;
      tip_.clear();
      if (pendingFoe_ >= 0) confirmStrike();
      else commitSpell(*spellArmed_);
      if (!exchange_) return fail("nothing played: " + tip_);
      scriptCheck_ = [this, hpBefore, kind, caster, usesBefore]() -> std::string {
        std::string hurt;
        bool foe = false;
        for (size_t i = 0; i < battle_->units().size() && i < hpBefore.size(); ++i) {
          const e2::BattleUnit& v = battle_->units()[i];
          if (v.hp == hpBefore[i]) continue;
          hurt += std::format(" {} ({}) {} -> {}", v.name, v.side, hpBefore[i], v.hp);
          if (caster >= 0 && v.side != battle_->units()[static_cast<size_t>(caster)].side && v.hp < hpBefore[i]) foe = true;
        }
        const int uses = caster >= 0 && !kind.empty() ? e2::spellUsesLeft(*battle_, static_cast<size_t>(caster), kind) : 0;
        logInfo("battle script: confirm {}: uses {} -> {}; HP changed:{}", kind, usesBefore, uses, hurt.empty() ? " nobody" : hurt);
        if (!kind.empty() && uses != usesBefore - 1) return std::format("uses {} -> {}", usesBefore, uses);
        return foe ? "" : "no foe hurt";
      };
      return logInfo("battle script: confirm: playing");
    }
    if (p[0] == "where") {
      // every unit's hex and whether the party still roams (free-roam captures)
      std::string list;
      for (size_t i = 0; i < scene_->units.size(); ++i)
        if (unitBattle_[i] >= 0) {
          const e2::BattleUnit& u = battle_->units()[static_cast<size_t>(unitBattle_[i])];
          list += std::format(" {} ({}, {}) mov {}{}", u.name, u.x, u.y, u.mov, u.alive ? "" : " dead");
        }
      return logInfo("battle script: where: {} round {}:{}", battle_->roaming() ? "roaming" : "turns", battle_->round(), list);
    }
    if (p[0] == "press" && p.size() > 1) {
      // "press:flee[:miss]" / "press:wait": the HUD's own button pressed while the action row is drawn (drawHudBar), the
      // rest of the row drawn after it in the same frame, as a click does; ":miss" makes the escape roll fail.
      scriptPress_ = p[1], scriptFleeMiss_ = p.size() > 2 && p[2] == "miss";
      scriptCheck_ = [this]() -> std::string {
        if (!scriptPress_.empty()) return "button '" + std::exchange(scriptPress_, {}) + "' not on offer";
        logInfo("battle script: pressed, picked {}, leaving {}", selected_, titleRequested_);
        return "";
      };
      return logInfo("battle script: press {}", p[1]);
    }
    const int ci = p.size() > 1 ? sceneUnitNamed(p[1]) : -1;
    if (ci < 0) return fail("no such unit");
    if (p[0] == "picktest") {
      // the ground pick against every hex of the board: each hex's centre projected to the screen and picked back (the
      // click that walks a unit); a hex that picks another is a place the player cannot send a unit to
      const int w = 1920, h = std::max(1, static_cast<int>(std::lround(1920.0f / std::max(aspect_, 0.1f))));
      int total = 0, wrong = 0, offscreen = 0, shown = 0;
      std::string worst;
      for (int y = 0; y < battle_->rows(); ++y)
        for (int x = 0; x < battle_->cols(); ++x) {
          const e2::Cell& c = board_->cell(x, y);
          const glm::vec3 centre{static_cast<float>(c.x), static_cast<float>(e2::groundHeightAt(*board_, look_, static_cast<float>(c.x), static_cast<float>(c.z))),
                                 static_cast<float>(c.z)};
          const std::optional<ImVec2> at = screenOf(centre, static_cast<float>(w), static_cast<float>(h));
          if (!at || at->x < 0 || at->y < 0 || at->x >= w || at->y >= h) {
            ++offscreen;
            continue;
          }
          ++total;
          const std::optional<e2::BattleHex> got = pickAt(w, h, at->x, at->y, false);
          if (got && got->x == x && got->y == y) continue;
          ++wrong;
          if (shown++ < 12)
            worst += std::format(" ({},{} h{:.1f})->({},{} h{:.1f})", x, y, centre.y, got ? got->x : -1, got ? got->y : -1,
                                 got ? board_->cell(got->x, got->y).groundY : 0.0);
        }
      logInfo("battle script: picktest: {} hexes on screen, {} pick another hex, {} off screen.{}", total, wrong, offscreen, worst);
      return;
    }
    if (p[0] == "walk" && p.size() > 3) {
      // the unit picked and walked to a hex (while roaming the party follows it)
      select(ci);
      const e2::BattleHex to{std::atoi(p[2].c_str()), std::atoi(p[3].c_str())};
      if (!std::any_of(reach_.begin(), reach_.end(), [&](const e2::ReachCell& c) { return c.x == to.x && c.y == to.y; })) return fail("not in its reach");
      walkTo(static_cast<size_t>(ci), to);
      const auto walking = std::count_if(unitWalk_.begin(), unitWalk_.end(), [](const Walk& w) { return !w.points.empty(); });
      return logInfo("battle script: walk {} to {} {}: {} walking", p[1], to.x, to.y, walking);
    }
    const size_t cb = static_cast<size_t>(unitBattle_[static_cast<size_t>(ci)]);
    if (p[0] == "ai") {
      // "ai:<enemy>": its AI plays a turn now (planEnemyCast, as an enemy's turn does); passes when it cast a spell.
      e2::BattleUnit& v = battle_->units()[cb];
      v.moved = false, v.acted = false, v.moveUsed = 0;
      select(-1);
      const int tier4 = v.spells[3];
      std::optional<e2::AiCast> cast = e2::planEnemyCast(*battle_, cb);
      if (!cast) return fail("its AI cast nothing");
      if (cast->path.size() > 1) {  // set down where its walk ends (the script holds the turn loop that plays a walk then a cast)
        v.x = cast->path.back().x, v.y = cast->path.back().y;
        unitPos_[static_cast<size_t>(ci)] = anchorWorld(v, {v.x, v.y});
        unitWalk_[static_cast<size_t>(ci)] = Walk{};
      }
      play(cast->steps, static_cast<size_t>(ci));
      const std::string kind = cast->steps.empty() ? std::string() : cast->steps.front().kind;
      scriptCheck_ = [this, cb, kind, tier4]() -> std::string {
        if (kind != "iceStorm") return "";
        if (battle_->iceZones().empty()) return "no field laid";
        return battle_->units()[cb].spells[3] == tier4 - 1 ? "" : "its use was not spent";
      };
      return logInfo("battle script: ai {}: casts {}", p[1], kind);
    }
    if (p[0] == "far" && p.size() > 3) {
      // "far:<unit>:<other>:<n>[:<through>]": the unit set down n hexes from another on a straight free line (through the
      // neighbour named, else the first direction that fits): a foe for the hook, a body for a throw to land against
      const int other = sceneUnitNamed(p[2]);
      const int through = p.size() > 4 ? sceneUnitNamed(p[4]) : -1;
      const int n = std::atoi(p[3].c_str());
      if (other < 0 || n < 1 || (p.size() > 4 && through < 0)) return fail("no such unit");
      const e2::BattleUnit& o = battle_->units()[static_cast<size_t>(unitBattle_[static_cast<size_t>(other)])];
      auto cube = [](e2::BattleHex h) { const int q = h.x - (h.y - (h.y & 1)) / 2; return std::pair<int, int>{q, h.y}; };
      auto oddr = [](int q, int r) { return e2::BattleHex{q + (r - (r & 1)) / 2, r}; };
      const auto [oq, orr] = cube({o.x, o.y});
      std::vector<e2::BattleHex> dirs;
      if (through >= 0) {
        const e2::BattleUnit& t = battle_->units()[static_cast<size_t>(unitBattle_[static_cast<size_t>(through)])];
        dirs.push_back({t.x, t.y});
      } else {
        for (const e2::BattleHex& h : e2::Battle::neighbors(o.x, o.y)) dirs.push_back(h);
      }
      for (const e2::BattleHex& d : dirs) {
        const auto [dq, dr] = cube(d);
        bool clear = true;
        e2::BattleHex at{o.x, o.y};
        for (int k = 1; k <= n && clear; ++k) {
          at = oddr(oq + (dq - oq) * k, orr + (dr - orr) * k);
          const bool occupied = battle_->inBounds(at.x, at.y) && battle_->unitAt(at.x, at.y) >= 0;
          const bool own = through >= 0 && k == 1;  // the neighbour the line runs through stands on its hex
          if (!battle_->inBounds(at.x, at.y) || !battle_->cell(at.x, at.y).passable || (occupied && !own)) clear = false;
        }
        if (!clear) continue;
        e2::BattleUnit& v = battle_->units()[cb];
        v.x = at.x, v.y = at.y;
        unitPos_[static_cast<size_t>(ci)] = anchorWorld(v, {v.x, v.y});
        unitWalk_[static_cast<size_t>(ci)] = Walk{};
        return logInfo("battle script: far {} {} hexes from {} at ({}, {})", p[1], n, p[2], v.x, v.y);
      }
      return fail("no free line that long");
    }
    if (p[0] == "vigil" && p.size() > 2) {
      // "vigil:<attacker>:<target>": the attacker stood beside the target is about to strike it; a sentinel's stance (Vigília da
      // Sentinela) must interrupt with its thrust first
      const int fi = sceneUnitNamed(p[2]);
      if (fi < 0) return fail("no such target");
      if (!placeBeside(static_cast<size_t>(ci), static_cast<size_t>(fi))) return fail("no free hex beside the target");
      const size_t tb = static_cast<size_t>(unitBattle_[static_cast<size_t>(fi)]);
      const int hpBefore = battle_->units()[cb].hp;
      if (!playInterrupts(cb, {tb})) return fail("no sentinel interrupted");
      scriptCheck_ = [this, cb, hpBefore]() -> std::string {
        const e2::BattleUnit& a = battle_->units()[cb];
        logInfo("battle script: vigil: the attacker {} hp {} -> {}{}", a.name, hpBefore, a.hp, a.alive ? "" : " (dead)");
        if (a.hp < hpBefore || !a.alive) return "";
        logInfo("battle script: the sentinel's thrust missed");
        return "";
      };
      return logInfo("battle script: vigil {} -> {}: a sentinel interrupts", p[1], p[2]);
    }
    if (p[0] == "near" && p.size() > 2) {
      // the unit placed beside another (a foe in reach of a strike, for captures)
      const int other = sceneUnitNamed(p[2]);
      if (other < 0) return fail("no such unit");
      if (!placeBeside(static_cast<size_t>(ci), static_cast<size_t>(other))) return fail("no free hex beside it");
      return logInfo("battle script: near {} {}", p[1], p[2]);
    }
    if (p[0] == "learn" && p.size() > 2) {
      // "learn:<unit>:<kind>": a spell its class does not have, for a test (two uses of its tier when it has none)
      e2::BattleUnit& v = battle_->units()[cb];
      if (std::find(v.spellList.begin(), v.spellList.end(), p[2]) == v.spellList.end()) v.spellList.push_back(p[2]);
      if (const int tier = e2::spellTierNumber(p[2]); tier > 0 && v.spells[tier - 1] < 1) v.spells[tier - 1] = 2;
      return logInfo("battle script: learn {} {}: {} uses", p[1], p[2], e2::spellUsesLeft(*battle_, cb, p[2]));
    }
    if (p[0] == "flee") {  // "flee:<unit>": the unit picked and Fugir combate tried (logs why not, its chance, the roll's outcome)
      select(ci);
      const e2::BattleUnit& me = battle_->units()[cb];
      const bool can = canFlee(me);
      if (!can) return logInfo("battle script: flee {}: not offered (fleeable {}, hex {},{} of {}x{}, moved {})", p[1], fleeable(), me.x, me.y, battle_->cols(), battle_->rows(), me.moved);
      const double chance = fleeChance(me);
      const bool out = attemptFlee();
      return logInfo("battle script: flee {}: offered at {:.2f}% -> {} (turn spent {}, leaving {})", p[1], chance, out ? "escaped" : "failed", me.moved, titleRequested_);
    }
    if (p[0] == "summon" && p.size() > 2) {
      // "summon:<unit>:<kind>": the unit picked as its turn casts the summon on the first free hex beside it
      const size_t cu = static_cast<size_t>(unitBattle_[static_cast<size_t>(ci)]);
      for (size_t i : battle_->turnOrder()) {
        if (i == cu) break;
        battle_->units()[i].moved = true;
      }
      e2::BattleUnit& me = battle_->units()[cu];
      me.moved = false, me.acted = false, me.moveUsed = 0;
      select(ci);
      startSpell(p[2]);
      std::optional<e2::BattleHex> free;
      for (const e2::BattleHex& n : e2::Battle::neighbors(me.x, me.y))
        if (!free && aimValid(cu, n)) free = n;
      if (!free) return fail("no free hex beside it");
      aimSpell(*free);
      aimSpell(*free);
      return logInfo("battle script: summon {} {}: {}", p[1], p[2], exchange_ ? "playing" : "not cast: " + tip_);
    }
    if ((p[0] == "stage" || p[0] == "arm") && p.size() > 2) {
      // Engine2's confirm panel for captures: "stage:<unit>:<foe>" = the unit picked as its turn with a strike staged on
      // the foe (stood beside it); "arm:<unit>:<kind>:<target>" = the spell aimed at the target (stood beside), waiting
      // for Confirmar.
      const int other = sceneUnitNamed(p[p[0] == "arm" && p.size() > 3 ? 3 : 2]);
      if (other < 0) return fail("no such target");
      if (other != ci && !placeBeside(static_cast<size_t>(other), static_cast<size_t>(ci))) return fail("no free hex beside the unit");
      for (size_t i : battle_->turnOrder()) {
        if (i == static_cast<size_t>(unitBattle_[static_cast<size_t>(ci)])) break;
        battle_->units()[i].moved = true;
      }
      e2::BattleUnit& me = battle_->units()[static_cast<size_t>(unitBattle_[static_cast<size_t>(ci)])];
      me.moved = false, me.acted = false, me.moveUsed = 0;
      select(ci);
      if (p[0] == "stage") {
        if (!std::any_of(targets_.begin(), targets_.end(), [&](const auto& t) { return t.first == other; })) return fail("not a target in its reach");
        pendingFoe_ = other;
        return logInfo("battle script: stage {} -> {}: strike staged, waiting for Confirmar", p[1], p[2]);
      }
      startSpell(p[2]);
      const e2::BattleUnit& t = battle_->units()[static_cast<size_t>(unitBattle_[static_cast<size_t>(other)])];
      aimSpell({t.x, t.y});
      return logInfo("battle script: arm {} {}: {}", p[1], p[2], spellArmed_ ? "armed, waiting for Confirmar" : "not armed: " + tip_);
    }
    if (p[0] == "pick" || p[0] == "spell" || p[0] == "attack") {
      // the unit picked as its turn: its reach, the foes it can strike (the units before it in the order have gone);
      // "spell:<unit>:<kind>" then opens the spell's aim, "attack:<unit>" the weapon's
      for (size_t i : battle_->turnOrder()) {
        if (i == cb) break;
        battle_->units()[i].moved = true;
      }
      battle_->units()[cb].moved = false, battle_->units()[cb].acted = false, battle_->units()[cb].moveUsed = 0;
      select(ci);
      if (p[0] == "spell" && p.size() > 2) startSpell(p[2]);
      if (p[0] == "attack") attackMode_ = true;
      return logInfo("battle script: {} {}: {} hexes in reach, {} targets, aim {} of zone {}", p[0], p[1], reach_.size(), targets_.size(),
                     spellAim_.size(), spellZone_.size());
    }
    e2::BattleUnit& caster = battle_->units()[cb];
    caster.acted = false, caster.moved = false;
    if (p[0] == "strike" && p.size() > 2) {
      // the unit's plain attack on a foe stood beside it (the Arcano bolt for a mage, a weapon blow otherwise)
      const int fi = sceneUnitNamed(p[2]);
      if (fi < 0) return fail("no such target");
      if (!placeBeside(static_cast<size_t>(fi), static_cast<size_t>(ci))) return fail("no free hex beside the attacker");
      select(ci);
      const bool off = offHandWanted(static_cast<size_t>(unitBattle_[static_cast<size_t>(ci)]), static_cast<size_t>(unitBattle_[static_cast<size_t>(fi)]));
      attack(static_cast<size_t>(ci), static_cast<size_t>(fi), off);
      if (!exchange_) return fail("nothing played");
      return logInfo("battle script: strike {} -> {}: {} (impact in {:.2f} s)", p[1], p[2],
                     off ? "off-hand blow" : exchange_->bolt ? "Arcano bolt (" + exchange_->element + ")" : "weapon blow", exchange_->impact - now_);
    }
    int ti = ci;
    if ((p[0] == "cast" && p.size() > 3) || (p[0] == "potion" && p.size() > 3)) {
      ti = sceneUnitNamed(p[3], p[2] != "resurrection");  // a Ressurreição is cast on a fallen one
      if (ti < 0) return fail("no such target");
      // the Gancho takes a foe 2+ hexes away: one stood that far by a "far" step before is left where it is
      const bool keep = p[2] == "hookPull" && e2::Battle::distance({caster.x, caster.y}, {battle_->units()[static_cast<size_t>(unitBattle_[static_cast<size_t>(ti)])].x,
                                                                                            battle_->units()[static_cast<size_t>(unitBattle_[static_cast<size_t>(ti)])].y}) >= 2;
      if (ti != ci && !keep && !placeBeside(static_cast<size_t>(ti), static_cast<size_t>(ci))) return fail("no free hex beside the caster");
    }
    const size_t tb = static_cast<size_t>(unitBattle_[static_cast<size_t>(ti)]);
    const e2::BattleUnit before = battle_->units()[tb];
    const e2::BattleUnit casterBefore = caster;
    const int rationsBefore = battle_->lootRations();
    std::vector<std::pair<size_t, double>> fedBefore;  // allies within 2 hexes and their hunger
    for (size_t i = 0; i < battle_->units().size(); ++i) {
      const e2::BattleUnit& v = battle_->units()[i];
      if (v.alive && v.side == caster.side && e2::Battle::distance({v.x, v.y}, {caster.x, caster.y}) <= 2) fedBefore.push_back({i, v.fullness.value_or(100)});
    }
    select(ci);
    std::string kind;
    if (p[0] == "cast" && p.size() > 3) kind = p[2];
    else if (p[0] == "food") kind = "createFoodAndWater";
    else if (p[0] == "potion" && p.size() > 3) kind = "potion:" + p[2];
    else return fail("unknown step");
    const int usesBefore = p[0] == "potion" ? 0 : e2::spellUsesLeft(*battle_, cb, kind);
    startSpell(kind);
    // The Lanceiro's throw and drag are aimed at a hex past the target: the farthest valid one on the line through it.
    e2::BattleHex aimHex{battle_->units()[tb].x, battle_->units()[tb].y};
    if (kind == "spearToss" || kind == "impaleDrag") {
      const e2::BattleHex from{caster.x, caster.y};
      // first the hexes exactly on the ray from the caster through the target (cube steps), then any whose line passes it
      auto cube = [](e2::BattleHex h) { return std::pair<int, int>{h.x - (h.y - (h.y & 1)) / 2, h.y}; };
      const auto [fq, fr] = cube(from);
      const auto [tq, tr] = cube(aimHex);
      const int dq = tq - fq, dr = tr - fr;
      int best = -1;
      e2::BattleHex found = aimHex;
      for (const e2::BattleHex& h : spellZone_) {
        const auto [hq, hr] = cube(h);
        const int k = e2::Battle::distance(from, h);
        if (k > 0 && hq - fq == dq * k && hr - fr == dr * k && k > best) best = k, found = h;
      }
      if (best < 0)
        for (const e2::BattleHex& h : spellZone_) {
          const std::vector<e2::BattleHex> line = e2::hexLine(from, h);
          if (!std::any_of(line.begin(), line.end(), [&](const e2::BattleHex& c) { return c == aimHex; })) continue;
          if (const int dist = e2::Battle::distance(from, h); dist > best) best = dist, found = h;
        }
      if (best < 0 && !spellZone_.empty()) found = spellZone_.front();
      aimHex = found;
    }
    // every target the spell asks for, on the same hex (Míssil Mágico's missiles, Fúria dos Céus' bolts may repeat)
    for (int k = 0; k < 8 && !spellMode_.empty(); ++k)  // a self-centred skill (Varredura, Redemoinho) is armed on the caster's own hex
      aimSpell(spellArmed_ ? *spellArmed_ : aimHex);
    if (refuse) {
      if (exchange_) return fail("played, should have been refused");
      const e2::BattleUnit& c = battle_->units()[cb];
      if (std::equal(std::begin(c.bag), std::end(c.bag), std::begin(casterBefore.bag)) && std::equal(std::begin(c.spells), std::end(c.spells), std::begin(casterBefore.spells)) &&
          !c.acted)
        return logInfo("battle script: {}: PASS (refused: {})", scriptLabel_, tip_);
      return fail("refused but something was spent");
    }
    if (!exchange_) return fail("nothing played: " + tip_);
    if (!spellFx_.active(now_ + 0.5f)) return fail("no effect playing");
    scriptCheck_ = [this, kind, cb, tb, before, casterBefore, usesBefore, rationsBefore, fedBefore]() -> std::string {
      const e2::BattleUnit& t = battle_->units()[tb];
      const e2::BattleUnit& c = battle_->units()[cb];
      if (!c.acted) return "the action did not spend the turn";
      if (const std::optional<std::string> potion = potionOf(kind)) {
        const int slot = e2::potionIndex(*potion);
        if (c.bag[slot] != casterBefore.bag[slot] - 1) return std::format("bag {} -> {}", casterBefore.bag[slot], c.bag[slot]);
        if (*potion == "disease") return t.diseased || t.poisoned ? "still sick" : "";
        if (potion->starts_with("mana")) {
          for (int k = 0; k < 10; ++k)
            if (t.spells[k] > before.spells[k]) return "";
          return "no spell use restored";
        }
        return t.hp > before.hp ? "" : std::format("hp {} -> {}", before.hp, t.hp);
      }
      if (e2::spellUsesLeft(*battle_, cb, kind) != usesBefore - 1) return std::format("uses {} -> {}", usesBefore, e2::spellUsesLeft(*battle_, cb, kind));
      if (kind == "cureDisease") return t.diseased || t.poisoned ? "still sick" : "";
      // Ours: a zone laid (the Mortalha, a Meteoro waiting), a condition left, or the hit itself.
      if (kind == "shroudOfShadows" || kind == "meteor") return battle_->zones().empty() ? "no zone laid" : "";
      if (kind == "iceStorm") return battle_->iceZones().empty() ? "no field laid" : "";  // it hurts at turn starts, not now
      if (kind == "markOfRuin") return t.cond.markTurns > 0 ? "" : "not marked";
      if (kind == "creepingPlague" || kind == "blackPestilence") return t.poisoned || !t.alive ? "" : "not poisoned";
      if (kind == "hemorrhage") return t.cond.bleedTurns > 0 || !t.alive ? "" : "not bleeding";
      if (kind == "profaneSilence") return t.cond.silenceTurns > 0 || !t.alive ? "" : "not silenced";
      if (kind == "curseOfDecay") return t.cond.decayTurns > 0 || !t.alive ? "" : "not cursed";
      if (kind == "glacialGale") return t.hp < before.hp || t.x != before.x || t.y != before.y || !t.alive ? "" : "neither hurt nor pushed";
      // The Guerreiro's: a weapon roll may miss (the uses and the turn were spent above); the conditions when it lands.
      if (kind == "warCry") return t.cond.rallyPct > 0 ? "" : "not rallied";
      if (kind == "guardBreak") return t.cond.sunderPct > 0 || !t.alive ? "" : t.hp < before.hp ? "hurt but the guard not broken" : (logInfo("battle script: {} missed", kind), std::string());
      if (kind == "steelWhirlwind" || kind == "leapingStrike") {
        if (t.hp < before.hp || !t.alive) return "";
        logInfo("battle script: {} missed", kind);
        return "";
      }
      // The Arqueiro's: the bow may miss; the riders when it lands.
      if (kind == "venomArrow") return t.poisoned || !t.alive ? "" : t.hp < before.hp ? "hurt but not poisoned" : (logInfo("battle script: {} missed", kind), std::string());
      if (kind == "cripplingShot") return t.cond.hobblePct > 0 || !t.alive ? "" : t.hp < before.hp ? "hurt but not hobbled" : (logInfo("battle script: {} missed", kind), std::string());
      if (kind == "arrowRain" || kind == "explosiveArrow" || kind == "arrowStorm" || kind == "deathShot") {
        if (t.hp < before.hp || !t.alive) return "";
        logInfo("battle script: {} missed", kind);
        return "";
      }
      // The Lanceiro's: the lance may miss; the board move and the riders when it lands.
      if (kind == "spearToss" || kind == "hookPull" || kind == "impaleDrag") {
        const bool moved = t.x != before.x || t.y != before.y;
        logInfo("battle script: {}: {} {} ({}, {}) -> ({}, {}), hp {} -> {}{}; the caster at ({}, {})", kind, t.name, moved ? "moved" : "stayed", before.x, before.y,
                t.x, t.y, before.hp, t.hp, t.alive ? "" : " (dead)", c.x, c.y);
        for (size_t k = battle_->log().size() > 9 ? battle_->log().size() - 9 : 0; k < battle_->log().size(); ++k) logInfo("battle script: log | {}", battle_->log()[k]);
        if (moved) return kind == "hookPull" && !t.cond.staggered ? "pulled but not off balance" : "";
        if (!t.alive) return "";  // killed by the blow itself: nobody left to move
        if (t.hp < before.hp) return "hurt but not moved";
        logInfo("battle script: {} missed", kind);
        return "";
      }
      if (kind == "poleVault") {
        if (c.x == casterBefore.x && c.y == casterBefore.y) return "the caster did not vault";
        logInfo("battle script: {} vaulted from ({}, {}) to ({}, {})", kind, casterBefore.x, casterBefore.y, c.x, c.y);
        if (t.hp < before.hp || !t.alive) return "";
        logInfo("battle script: {} missed", kind);
        return "";
      }
      if (kind == "disarm") {
        logInfo("battle script: disarm: {} hp {} -> {}{}, atk {} -> {}, Desarmado -{}% for {} turns", t.name, before.hp, t.hp, t.alive ? "" : " (dead)", before.atk, t.atk,
                t.cond.disarmPct, t.cond.disarmTurns);
        return t.cond.disarmPct > 0 || !t.alive ? "" : t.hp < before.hp ? "hurt but not disarmed" : (logInfo("battle script: {} missed", kind), std::string());
      }
      if (kind == "swapPost") {
        if (!(c.x == before.x && c.y == before.y && t.x == casterBefore.x && t.y == casterBefore.y)) return "places not changed";
        logInfo("battle script: {}: {} now at ({}, {}), {} at ({}, {})", kind, c.name, c.x, c.y, t.name, t.x, t.y);
        return "";
      }
      if (kind == "sentinelVigil") return c.cond.vigilPct > 0 ? "" : "no vigil on the caster";
      // The Conjurador's
      if (kind == "placeSwap") {
        if (!(c.x == before.x && c.y == before.y && t.x == casterBefore.x && t.y == casterBefore.y)) return "places not changed";
        return "";
      }
      if (kind == "mirrorImage") return t.cond.mirrorCopies > 0 ? "" : "no illusions";
      if (kind == "detonateSummon") return !t.alive ? "" : "the summon still stands";
      if (kind == "sacrificeBond") return c.cond.bondPct > 0 ? "" : "no bond on the caster";
      if (kind == "empowerSummons") {
        for (const e2::BattleUnit& v : battle_->units())
          if (v.alive && v.summoned && v.summonerId == c.id && v.cond.empowerPct > 0) return "";
        return "no summon empowered";
      }
      // The Curandeiro's: the fallen stands again; the circle heals the hurt target (the `hurt` step before it)
      if (kind == "resurrection") {
        logInfo("battle script: resurrection: {} alive {}, hp {} -> {}, moved {} acted {}", t.name, t.alive, before.hp, t.hp, t.moved, t.acted);
        return t.alive && t.hp > 0 ? "" : "still fallen";
      }
      if (kind == "createFoodAndWater") {
        for (const auto& [i, full] : fedBefore) {
          const e2::BattleUnit& v = battle_->units()[i];
          if (v.fullness.value_or(100) <= full || v.hungerPenaltyPct != 0.0) return v.name + " not fed";
        }
        // rations from level 3 on (createFoodAndWaterPower)
        return c.level < 3 || battle_->lootRations() > rationsBefore ? "" : "no rations conjured";
      }
      if (!spellOnFriend(kind)) return t.hp < before.hp || !t.alive ? std::string() : std::format("hp {} -> {} (not hurt)", before.hp, t.hp);
      return t.hp > before.hp ? std::string() : std::format("hp {} -> {}", before.hp, t.hp);
    };
    logInfo("battle script: {}: cast", scriptLabel_);
  }

  // The unit the Spells menu casts from: the picked one, else the party's first.
  int menuCaster() const { return selected_ >= 0 ? selected_ : hero_; }
  // The nearest living unit on the other side (on its own side for a heal), the caster itself when none.
  size_t nearestTarget(size_t caster, bool friendly) const {
    const std::string& side = scene_->units[caster].side;
    int best = -1;
    float bestDistance = 1e30f;
    for (size_t i = 0; i < scene_->units.size(); ++i) {
      if (i == caster || (scene_->units[i].side == side) != friendly) continue;
      if (unitBattle_.size() > i && unitBattle_[i] >= 0 && !battle_->units()[static_cast<size_t>(unitBattle_[i])].alive) continue;
      const float d = glm::length(unitWorld(i) - unitWorld(caster));
      if (d < bestDistance) bestDistance = d, best = static_cast<int>(i);
    }
    return best < 0 ? caster : static_cast<size_t>(best);
  }
  // A spell's visual (version `look`) from `caster` at its nearest target, visuals only (no rules): the Spells menu's
  // Testar and --fx-test. Lines (Frost, piercing shots) reach three hexes past the target.
  // The Lanceiro's (ours, visuals only): the lance's blow on the nearest foe and the board move it would make, the cards hopping
  // in place (the rules move nobody here), the flights' effects to the hexes the move would reach.
  void testLancer(size_t hero, const std::string& kind, float start) {
    constexpr float kHexW = 1.7320508f;
    const size_t foe = nearestTarget(hero, false);
    const SpellEffects::Spot me = unitSpot(hero), them = unitSpot(foe);
    glm::vec3 d = them.feet - me.feet;
    d.y = 0.0f;
    d = glm::dot(d, d) > 1e-6f ? glm::normalize(d) : glm::vec3(0.0f, 0.0f, 1.0f);
    auto spotAt = [&](const SpellEffects::Spot& s, glm::vec3 feet) {
      SpellEffects::Spot o = s;
      o.body = feet + (s.body - s.feet), o.feet = feet;
      return o;
    };
    auto onGround = [&](glm::vec3 p) {
      p.y = groundAt(p.x, p.z);
      return p;
    };
    auto hop = [&](size_t ui, float from, float to, float arc) {
      if (ui < unitLeap_.size()) unitLeap_[ui] = {from, to, unitPos_[ui], unitPos_[ui], arc};
    };
    const int owner = static_cast<int>(hero);
    const float impact = start + 0.4f;
    if (kind == "spearToss") {
      spellFx_.cast(kind, 1, me, {them}, start, impact, owner);
      const glm::vec3 land = onGround(them.feet + d * (2.0f * kHexW));
      spellFx_.lancerFlight(kind, them, spotAt(them, land), 1.9f, impact, impact + 0.5f, me, SpellEffects::Spot{}, false);
      hop(foe, impact, impact + 0.5f, 1.9f);
    } else if (kind == "hookPull") {
      spellFx_.cast(kind, 1, me, {them}, start, impact, owner);
      const glm::vec3 dest = onGround(me.feet + d * kHexW);
      spellFx_.lancerFlight(kind, them, spotAt(them, dest), 0.0f, impact, impact + 0.35f, me, SpellEffects::Spot{}, false);
    } else if (kind == "poleVault") {
      const glm::vec3 landing = onGround(them.feet + d * kHexW);
      spellFx_.vaultFlight(me, them, spotAt(them, landing), start, start + 0.5f, owner);
      hop(hero, start, start + 0.5f, 2.0f);
      spellFx_.cast(kind, 1, spotAt(me, landing), {them}, start + 0.5f, start + 0.8f, owner);
    } else if (kind == "disarm") {
      spellFx_.cast(kind, 1, me, {them}, start, impact, owner);
      spellFx_.disarmed(them, me, impact);
    } else if (kind == "swapPost") {
      const size_t ally = nearestTarget(hero, true);
      const SpellEffects::Spot al = unitSpot(ally);
      spellFx_.swapFlight(me, al, impact, impact + 0.42f);
      hop(hero, impact, impact + 0.42f, 0.55f);
      if (ally != hero) hop(ally, impact, impact + 0.42f, 0.55f);
      spellFx_.cast(kind, 1, me, {them}, impact + 0.5f, impact + 0.8f, owner);
    } else if (kind == "impaleDrag") {
      spellFx_.cast(kind, 1, me, {them}, start, impact, owner);
      const glm::vec3 foeTo = onGround(them.feet + d * (2.0f * kHexW)), meTo = onGround(them.feet + d * kHexW);
      spellFx_.lancerFlight(kind, them, spotAt(them, foeTo), 0.0f, impact, impact + 0.45f, me, spotAt(me, meTo), false);
    } else if (kind == "sentinelVigil") {
      spellFx_.cast(kind, 1, me, {me}, start, impact, owner);
      const int id = spellFx_.lancer().vigilStart(me.feet, me.cardHeight, 1.5f * kHexW, impact);
      spellFx_.lancer().vigilEnd(id, start + kFxTestCycle - 0.5f);
      spellFx_.cast("sentinelStrike", 1, me, {them}, impact + 1.2f, impact + 1.5f, owner);
    } else {
      spellFx_.cast("sentinelStrike", 1, me, {them}, start, impact, owner);
    }
  }
  void testCast(size_t caster, const std::string& kind, int look, float start) {
    if (e2::emberSpellClass(kind) == "aldric" || kind == "sentinelStrike") return testLancer(caster, kind, start);
    const size_t hero = caster;
    const size_t target = nearestTarget(caster, spellOnFriend(kind));
    std::vector<SpellEffects::Spot> spots{unitSpot(target)};
    // Ours: the spell's own shape on the board (its area round the target, its line, its cone), when a battle is on.
    if (battle_ && !e2::emberSpellClass(kind).empty() && unitBattle_[hero] >= 0 && unitBattle_[target] >= 0) {
      const e2::BattleUnit& t = battle_->units()[static_cast<size_t>(unitBattle_[target])];
      const e2::BattleHex aim = kind == "eruption" ? e2::BattleHex{battle_->units()[static_cast<size_t>(unitBattle_[hero])].x,
                                                                   battle_->units()[static_cast<size_t>(unitBattle_[hero])].y}
                                                   : e2::BattleHex{t.x, t.y};
      std::vector<e2::BattleHex> area = e2::spellArea(*battle_, static_cast<size_t>(unitBattle_[hero]), kind, aim);
      if (kind == "chainLightning" || kind == "wrathOfTheSkies")  // a few more bodies for the chain / the bolts
        for (size_t i = 0; i < scene_->units.size() && area.size() < 4; ++i)
          if (i != hero && i != target && unitBattle_[i] >= 0 && scene_->units[i].side != scene_->units[hero].side &&
              battle_->units()[static_cast<size_t>(unitBattle_[i])].alive)
            area.push_back({battle_->units()[static_cast<size_t>(unitBattle_[i])].x, battle_->units()[static_cast<size_t>(unitBattle_[i])].y});
      if (!area.empty()) {
        spots.clear();
        for (const e2::BattleHex& h : area) spots.push_back(hexSpot(h));
      }
      if (kind == "meteor") {  // Testar shows the fall itself, not only the mark
        spellFx_.meteorFall(spots, start + 0.6f, start + 1.5f);
      }
      if (kind == "warCry") {  // the allies standing in its radius (the hero among them), not the bare hexes
        spots.clear();
        for (size_t i = 0; i < scene_->units.size(); ++i) {
          if (unitBattle_[i] < 0 || scene_->units[i].side != scene_->units[hero].side) continue;
          const e2::BattleUnit& v = battle_->units()[static_cast<size_t>(unitBattle_[i])];
          if (v.alive && std::any_of(area.begin(), area.end(), [&](const e2::BattleHex& h) { return h.x == v.x && h.y == v.y; })) spots.push_back(unitSpot(i));
        }
      }
      if (kind == "leapingStrike") {  // the flight to the landing first (a foe out of range: four hexes along the line to it), the blow as it lands
        const e2::BattleUnit& hb = battle_->units()[static_cast<size_t>(unitBattle_[hero])];
        std::optional<e2::BattleHex> landing;
        if (area.size() > 1) landing = area[1];
        else if (const std::vector<e2::BattleHex> line = e2::hexLine({hb.x, hb.y}, aim); line.size() > 5) landing = line[4];
        if (landing) {
          spellFx_.leapFlight(unitSpot(hero), hexSpot(*landing), start, start + 0.45f, static_cast<int>(hero));
          spots = {hexSpot(area[0])};
          spellFx_.cast(kind, look, unitSpot(hero), spots, start + 0.45f, start + 0.75f, static_cast<int>(hero));
          return;
        }
      }
    } else if (kind == "frost" || kind == "piercing") {
      // A line of hexes from the hero through the target (three more hexes beyond it).
      const glm::vec3 a = unitWorld(hero), b = unitWorld(target);
      for (int k = 1; k <= 3; ++k) {
        const glm::vec3 p = b + (b - a) * (0.35f * static_cast<float>(k)) / std::max(glm::length(b - a), 1e-3f) * 1.73f;
        spots.push_back({{p.x, groundAt(p.x, p.z), p.z}, {p.x, groundAt(p.x, p.z) + 0.9f, p.z}});
      }
    }
    spellFx_.cast(kind, look, unitSpot(hero), spots, start, start + 0.6f, static_cast<int>(hero));
    if (weaponStrikeKind(kind))
      spellFx_.combat().impact(CombatFx::Kind::Physical, spots.front().feet, cardHeightOf(spots.front()), unitWorld(hero), start + 0.6f);
  }
  // --fx-test hit | hitHeavy | slash | thrust | arrowHit | arcaneAttack: the combat's own effects alone (no rules): a weapon blow, a heavy one, the
  // Arcano attack (conjure 0.45 s, the bolt, the impact), each with the struck card's recoil.
  bool testCombat(size_t hero, const std::string& kind, float start) {
    if (kind != "hit" && kind != "hitHeavy" && kind != "slash" && kind != "thrust" && kind != "arrowHit" && kind != "arcaneAttack") return false;
    const size_t target = nearestTarget(hero, false);
    const SpellEffects::Spot from = unitSpot(hero), to = unitSpot(target);
    float impact = start + 0.4f;
    if (kind == "arcaneAttack") {
      const float release = start + 0.45f;
      impact = release + CombatFx::flightSeconds(from.feet, to.feet);
      spellFx_.combat().bolt(from.feet, cardHeightOf(from), to.feet, cardHeightOf(to), start, release, impact);
      spellFx_.combat().impact(CombatFx::Kind::Magical, to.feet, cardHeightOf(to), from.feet, impact);
      logInfo("fx test: Arcano bolt released at {:.2f} s, lands at {:.2f} s", release, impact);
    } else {
      spellFx_.combat().impact(kind == "hitHeavy" ? CombatFx::Kind::Heavy : kind == "slash" ? CombatFx::Kind::Slash : kind == "thrust" ? CombatFx::Kind::Thrust
                               : kind == "arrowHit" ? CombatFx::Kind::ArrowHit
                                                                                           : CombatFx::Kind::Physical,
                               to.feet, cardHeightOf(to), from.feet, impact);
    }
    glm::vec3 dir = to.feet - from.feet;
    dir.y = 0.0f;
    if (target < unitRecoil_.size() && target != hero)
      unitRecoil_[target] = {impact, glm::dot(dir, dir) > 1e-6f ? glm::normalize(dir) : glm::vec3(0.0f), kind == "hitHeavy"};
    return true;
  }
  static bool weaponStrikeKind(const std::string& kind) {
    return kind == "doubleStrike" || kind == "longShot" || kind == "piercing" || kind == "piercingThrust";
  }
  // The Arqueiro's skills (ours, docs/SKILLS_ARCHER.md): arrows (RangerFx), the arrow's hit on each struck body.
  static bool archerKind(const std::string& kind) {
    return kind == "venomArrow" || kind == "arrowRain" || kind == "explosiveArrow" || kind == "cripplingShot" || kind == "arrowStorm" || kind == "deathShot";
  }
  // The effect version a scene unit's spell plays (the Magias menu: heroes and enemies choose apart).
  int spellLook(const std::string& kind, size_t ui) const { return spellsMenu_.look(kind, scene_->units[ui].side != "player"); }

  bool hasPose(size_t ui, const std::string& pose) const {
    const e2::Hero* entry = ui < unitCatalog_.size() ? unitCatalog_[ui] : nullptr;
    return entry && entry->find(pose);
  }

  // What a step did, on screen: numbers, misses, heals, the struck unit's hit or death pose.
  void showEffects(const std::vector<e2::BattleEffect>& fx) {
    for (const e2::BattleEffect& e : fx) {
      const int ui = sceneUnitOf(static_cast<int>(e.unit));
      if (ui < 0) continue;
      const size_t u = static_cast<size_t>(ui);
      if (e.miss) {  // "Errou", or what took the blow instead (Imagem Espelhada: "Ilusão")
        popups_.push_back({u, e.text.empty() ? "Errou" : e.text, e.text.empty() ? IM_COL32(200, 200, 200, 255) : IM_COL32(196, 150, 232, 255), now_, false});
        continue;
      }
      if (e.moved && !e.pushed && e.amount == 0 && !e.died) continue;  // a charge's walk: nothing to say
      if (e.revived) {  // Ressurreição: the card stands again where it fell, in its idle
        unitDiedAt_[u] = -1.0f;
        unitShot_[u] = {};
        const e2::BattleUnit& bu = battle_->units()[e.unit];
        unitPos_[u] = anchorWorld(bu, {bu.x, bu.y});
        unitWalk_[u] = Walk{};
      }
      if (e.heal) {
        // a heal's +HP, else what a cure, a meal or a mana potion did ("curado", "+2 rações", "+3 magia")
        popups_.push_back({u, e.text.empty() ? "+" + std::to_string(e.amount) : e.text, IM_COL32(170, 226, 150, 255), now_, false});
        continue;
      }
      if (e.pushed) {  // Vendaval Glacial: the card goes to its new hex at once
        const e2::BattleUnit& bu = battle_->units()[e.unit];
        unitPos_[u] = anchorWorld(bu, {bu.x, bu.y});
        unitWalk_[u] = Walk{};
        unitMoved_[u] = 1;
        if (e.amount == 0 && !e.died && !e.status && e.text.empty()) continue;  // only its card moved
      }
      if (e.status && !e.died) {  // a condition's word (our spells): violet, no hit pose
        popups_.push_back({u, e.text, IM_COL32(196, 150, 232, 255), now_, false});
        continue;
      }
      popups_.push_back({u, e.text.empty() ? std::to_string(e.amount) : e.text, e.crit || !e.text.empty() ? IM_COL32(255, 214, 102, 255) : IM_COL32(255, 255, 255, 255), now_,
                         e.crit || !e.text.empty()});
      // The struck unit's own hit / death cues (the engine's per-frame watch of hitAt / diedAt).
      if (!e.died || sounds(e.unit, "hitOnDeath") != e2::Json(false)) playCues(sounds(e.unit, "hit"));
      if (e.died) playCues(sounds(e.unit, "death"));
      if (e.died) {
        unitShot_[u] = {"death", now_, true, actionRate()};
        unitDiedAt_[u] = now_;
      } else {
        unitShot_[u] = {"hit", now_, false, actionRate()};
      }
    }
  }

  // Every frame: each step's impact lands, a strike's counter follows, then the next step; at the end the action is
  // over (Engine2 finishAction).
  void updateExchange() {
    if (!exchange_) return;
    Exchange& x = *exchange_;
    const e2::BattleStep& s = x.steps[x.step];
    if (!x.landed && now_ >= x.impact) {
      x.landed = true;
      // stepCombat's strike cue (Kael's long swing) and emitMissileFx's arrow release (Tiro Longo).
      if (s.type == e2::BattleStep::Type::Combat) playCues(sounds(x.counter ? s.def : s.att, "strike"));
      else if (s.kind == "longShot") playCues(sounds(s.att, "release"));
      std::vector<e2::BattleEffect> fx;
      if (s.type == e2::BattleStep::Type::Combat) {
        if (battle_->units()[s.att].alive && battle_->units()[s.def].alive) e2::resolveStrike(*battle_, s, x.counter, rng_, fx);
      } else {
        e2::resolveStep(*battle_, s, rng_, fx);
      }
      syncSummons();  // a familiar that appeared with this step gets its card
      // The Lanceiro's board moves: bodies flying to their new hexes, and what lands only once a flight is over (a collision's
      // numbers, `delay`).
      const float flightEnd = animateFlights(fx, s);
      {
        std::vector<e2::BattleEffect> later;
        for (size_t i = 0; i < fx.size();) {
          if (fx[i].delay > 0.0f) later.push_back(fx[i]), fx.erase(fx.begin() + static_cast<std::ptrdiff_t>(i));
          else ++i;
        }
        if (!later.empty()) pendingFx_.push_back({std::max(flightEnd, now_ + later.front().delay / actionRate()), later});
      }
      showEffects(fx);
      if (s.disarmPct > 0)  // Desarmar landed: the weapon knocked out of its hands flies off
        for (const e2::BattleEffect& e : fx)
          if (e.status && (e.text.starts_with("Desarmado") || e.text.starts_with("Disarmed")))
            if (const int fu = sceneUnitOf(static_cast<int>(e.unit)), su = sceneUnitOf(static_cast<int>(s.att)); fu >= 0 && su >= 0)
              spellFx_.disarmed(unitSpot(static_cast<size_t>(fu)), unitSpot(static_cast<size_t>(su)), now_);
      // A missed Investida Touro shoves nobody aside: the rest of the charge is dropped (its walk and its other blows).
      if (std::any_of(fx.begin(), fx.end(), [](const e2::BattleEffect& e) { return e.abortRush; })) {
        const size_t actor = s.att;
        // erased in place after this step, so `s` stays valid
        x.steps.erase(std::remove_if(x.steps.begin() + static_cast<long>(x.step) + 1, x.steps.end(),
                                     [&](const e2::BattleStep& q) {
                                       return q.att == actor && (q.type == e2::BattleStep::Type::Move || (q.type == e2::BattleStep::Type::Combat && q.kind == "bullRush"));
                                     }),
                      x.steps.end());
      }
      // The impact (CombatFx) and the struck body's reaction, on the same instant as the numbers. A weapon, an arrow or
      // a blade skill: the physical impact (heavy on a critical, a kill or a third of the body's life); the Arcano bolt:
      // the magical one (its energy comes apart on a miss). Spells bring their own burst; every blow that hurts knocks
      // the card back.
      const bool bowSkill = archerKind(s.kind);  // the Arqueiro's (ours): arrows, so the arrow's hit lands on each struck body
      const bool magic = s.type == e2::BattleStep::Type::Spell && s.kind != "longShot" && s.kind != "piercing" && s.kind != "piercingThrust" && !bowSkill;
      const int striker = sceneUnitOf(static_cast<int>(x.counter ? s.def : s.att));
      for (const e2::BattleEffect& e : fx) {
        const int ui = sceneUnitOf(static_cast<int>(e.unit));
        if (ui < 0 || e.heal || e.status) continue;
        const size_t u = static_cast<size_t>(ui);
        const SpellEffects::Spot struck = unitSpot(u);
        const glm::vec3 from = striker >= 0 && striker != ui ? unitWorld(static_cast<size_t>(striker)) : struck.feet - glm::vec3(0.0f, 0.0f, 1.0f);
        const e2::BattleUnit& bu = battle_->units()[e.unit];
        const bool heavy = e.crit || e.died || (bu.maxHp > 0 && e.amount * 10 >= bu.maxHp * 3);
        if (x.bolt) {
          spellFx_.combat().impact(e.miss ? CombatFx::Kind::MagicalMiss : CombatFx::Kind::Magical, struck.feet, cardHeightOf(struck), from, now_,
                                   CombatPalette::of(x.element.c_str()));
        } else if ((x.arrow || bowSkill) && !e.miss) {
          spellFx_.combat().impact(heavy ? CombatFx::Kind::HeavyArrowHit : CombatFx::Kind::ArrowHit, struck.feet, cardHeightOf(struck), from, now_);
        } else if (!magic && !e.miss && striker >= 0) {
          // By the striker's weapon: a blade cuts, a spear stabs, a blunt weapon (or a beast's weight) knocks.
          const WeaponStyle style = weaponStyle(battle_->units()[static_cast<size_t>(unitBattle_[static_cast<size_t>(striker)])]);
          const CombatFx::Kind kind = style == WeaponStyle::Blade   ? (heavy ? CombatFx::Kind::HeavySlash : CombatFx::Kind::Slash)
                                      : style == WeaponStyle::Thrust ? CombatFx::Kind::Thrust
                                      : heavy                        ? CombatFx::Kind::Heavy
                                                                     : CombatFx::Kind::Physical;
          spellFx_.combat().impact(kind, struck.feet, cardHeightOf(struck), from, now_);
        }
        if (!e.miss && e.amount > 0) {
          glm::vec3 dir = struck.feet - from;
          dir.y = 0.0f;
          unitRecoil_[u] = {now_, glm::dot(dir, dir) > 1e-6f ? glm::normalize(dir) : glm::vec3(0.0f), heavy};
        }
      }
    }
    if (now_ < x.end) return;
    if (s.type == e2::BattleStep::Type::Combat && !x.counter && e2::strikesBack(*battle_, s)) {
      x.counter = true;
      startStep(x);
      return;
    }
    // The step is over (Engine2 finishCombat -> finishAction): the action is done, and fighting side by side builds affinity (+0.1)
    if (!x.affinityDone && !x.interrupt && s.type != e2::BattleStep::Type::Move) {
      x.affinityDone = true;
      e2::gainAdjacentAffinity(*battle_, static_cast<size_t>(unitBattle_[x.owner]));
    }
    if (x.step + 1 < x.steps.size()) {
      ++x.step;
      x.counter = false;
      startStep(x);
      return;
    }
    if (x.interrupt) {  // a sentinel's thrust out of its turn: nobody's action ends here
      exchange_.reset();
      return;
    }
    // Engine2 finishAction: it has acted; its turn ends unless it is a hero with movement left (it stays picked).
    e2::BattleUnit& au = battle_->units()[static_cast<size_t>(unitBattle_[x.owner])];
    // Every action costs a hero 2 points of fullness (Engine2 finishAction: drainHunger, ACTION_HUNGER_COST)
    if (au.side == "player" && !au.summoned) au.fullness = std::max(0.0, std::min(120.0, au.fullness.value_or(100.0)) - 2.0);
    au.acted = true;
    exchange_.reset();
    if (!au.alive || au.side != "player" || au.mov - au.moveUsed <= 0) {
      if (au.side == "player") au.moved = true;
      return select(-1);
    }
    refreshReach();
  }

  // The move is decided at once (the battle's state), the walk then plays it hex by hex.
  void walkTo(size_t ui, e2::BattleHex to) {
    const size_t b = static_cast<size_t>(unitBattle_[ui]);
    const std::vector<e2::BattleHex> route = e2::Battle::path(reachPaths_, to.x, to.y);
    if (route.size() < 2) return;
    if (!std::any_of(reach_.begin(), reach_.end(), [&](const e2::ReachCell& c) { return c.x == to.x && c.y == to.y; })) return;
    const e2::BattleUnit& u = battle_->units()[b];
    const bool leads = battle_->roaming() && myTurn() && static_cast<int>(ui) == selected_;
    if (myTurn() && static_cast<int>(ui) == selected_ && !leads)  // Cancelar takes this walk back
      undo_.push_back(BeforeWalk{ui, u.x, u.y, u.moveUsed, u.acted, u.faceDx, u.faceDy, unitPos_[ui], unitFacing_[ui], unitMoved_[ui]});
    walkAlong(ui, route);  // the battle's heading becomes the last step's (Engine2 stepMove), for attacks from behind
    if (leads) followLeader(ui, route, reachPaths_);
    refreshReach();        // what is left of its movement, shown once it arrives
  }

  // The battle grid (Battle3D gridMarks' colours): the movement wash and edges, the route, the picked unit's marker
  // and the cursor; fills and hex borders, radii in hex radii. Painted on the ground by the terrain pass
  // (Renderer::setGroundMarks), so units, trees and houses in front cover them; never on ground not yet seen.
  void collectMarks(std::vector<Renderer::GroundMark>& marks) const {
    if (!battle_ || !scene_ || !board_) return;
    const e2::HexLayout& layout = board_->layout();
    auto add = [&](e2::BattleHex at, ImU32 color, float rOut, float rIn = 0.0f) {
      if (!e2::inBoard(layout, at.x, at.y)) return;
      marks.push_back({static_cast<uint32_t>(e2::indexOf(layout, at.x, at.y)), color, rOut, rIn});
    };
    auto border = [](float r) { return r * kBorderInner; };
    const bool walking = selected_ >= 0 && !unitWalk_[static_cast<size_t>(selected_)].points.empty();
    if (selected_ >= 0 && !walking && !spellMode_.empty()) {
      // Engine2's spell aim: every hex it may be aimed at washed, the hovered aim's area bright, chosen targets ringed.
      const size_t b = static_cast<size_t>(unitBattle_[static_cast<size_t>(selected_)]);
      for (const e2::BattleHex& c : spellAim_) add(c, IM_COL32(176, 124, 232, 34), 1.0f), add(c, IM_COL32(176, 124, 232, 110), 0.94f, border(0.94f));
      if (hover_) {
        if (aimValid(b, *hover_))
          for (const e2::BattleHex& c : aimArea(b, *hover_)) add(c, IM_COL32(214, 168, 255, 96), 1.0f);
        add(*hover_, IM_COL32(8, 12, 16, 242), 0.985f, border(0.985f));
        add(*hover_, aimValid(b, *hover_) ? IM_COL32(236, 214, 255, 255) : IM_COL32(231, 133, 115, 242), 0.94f, border(0.94f));
      }
      for (const e2::BattleHex& c : spellCells_) add(c, IM_COL32(255, 214, 102, 255), 0.7f, border(0.7f));
      const e2::BattleUnit& u = battle_->units()[b];
      add({u.x, u.y}, IM_COL32(220, 226, 235, 71), 0.94f, border(0.94f));
    } else if (selected_ >= 0 && !walking) {
      const bool player = scene_->units[static_cast<size_t>(selected_)].side == "player";
      const e2::BattleUnit& u = battle_->units()[static_cast<size_t>(unitBattle_[static_cast<size_t>(selected_)])];
      // The reach (tacticalGridStyle): the movement wash with its edges for the party, the enemy's wash for a foe.
      for (const e2::ReachCell& c : reach_) {
        if (c.x == u.x && c.y == u.y) continue;
        if (attackMode_ || (player && battle_->roaming())) break;  // "Atacar": only the foes it can strike; free roam: no wash
        add({c.x, c.y}, player ? IM_COL32(112, 174, 220, 20) : IM_COL32(231, 133, 115, 31), 1.0f);
        if (player) add({c.x, c.y}, IM_COL32(112, 174, 220, 92), 0.94f, border(0.94f));
      }
      // The route to the hovered hex.
      if (player && hover_ && !(hover_->x == u.x && hover_->y == u.y) &&
          std::any_of(reach_.begin(), reach_.end(), [&](const e2::ReachCell& c) { return c.x == hover_->x && c.y == hover_->y; })) {
        const std::vector<e2::BattleHex> route = e2::Battle::path(reachPaths_, hover_->x, hover_->y);
        for (size_t i = 1; i < route.size(); ++i) {
          const bool last = i + 1 == route.size();
          if (last) {
            add(route[i], IM_COL32(8, 12, 16, 242), 0.985f, border(0.985f));
            add(route[i], IM_COL32(220, 226, 235, 31), 1.01f, border(1.01f));
            add(route[i], IM_COL32(220, 226, 235, 255), 0.94f, border(0.94f));
          } else {
            add(route[i], IM_COL32(8, 12, 16, 242), 0.16f);
            add(route[i], IM_COL32(220, 226, 235, 255), 0.12f);
          }
        }
      }
      // The picked unit's marker (Engine2's active-turn highlight).
      const e2::BattleHex at{u.x, u.y};
      if (player) {
        add(at, IM_COL32(220, 226, 235, 3), 1.035f, border(1.035f));
        add(at, IM_COL32(220, 226, 235, 6), 1.01f, border(1.01f));
        add(at, IM_COL32(220, 226, 235, 10), 0.985f, border(0.985f));
      }
      add(at, IM_COL32(12, 20, 25, 217), 0.98f, border(0.98f));
      add(at, player ? IM_COL32(220, 226, 235, 71) : IM_COL32(231, 133, 115, 255), 0.94f, border(0.94f));
    }
    // The foes in reach of a strike (Engine2 GRID_ENEMY_TARGET: its fill and border).
    for (const auto& [t, from] : targets_) {
      const e2::BattleUnit& f = battle_->units()[static_cast<size_t>(unitBattle_[static_cast<size_t>(t)])];
      for (const e2::BattleHex& p : e2::Battle::footprint(f, f.x, f.y)) {
        add(p, IM_COL32(110, 0, 8, 217), 1.0f);
        add(p, IM_COL32(110, 0, 8, 255), 0.94f, border(0.94f));
      }
    }
    // The cursor: red where nothing can stand.
    if (hover_) {
      const bool blocked = !battle_->cell(hover_->x, hover_->y).passable;
      add(*hover_, IM_COL32(8, 12, 16, 242), 0.985f, border(0.985f));
      add(*hover_, blocked ? IM_COL32(231, 133, 115, 242) : IM_COL32(220, 226, 235, 255), 0.94f, border(0.94f));
    }
    if (fog_ && !noFog_ && fogReady_)
      std::erase_if(marks, [&](const Renderer::GroundMark& m) { return m.cell < fogStates_.size() && fogStates_[m.cell] == e2::kFogUnseen; });
  }

  // The battle grid for the numbered looks (Renderer::GridCell per hex, looks/grid/README.md): the same states as
  // collectMarks, as flags; the terrain pass draws the look itself. Never on ground not yet seen.
  void collectGrid(std::vector<Renderer::GridCell>& cells) const {
    if (!battle_ || !scene_ || !board_) return;
    const e2::HexLayout& layout = board_->layout();
    cells.assign(static_cast<size_t>(layout.cols) * static_cast<size_t>(layout.rows), Renderer::GridCell{});
    auto at = [&](e2::BattleHex h) -> Renderer::GridCell* {
      if (!e2::inBoard(layout, h.x, h.y)) return nullptr;
      return &cells[static_cast<size_t>(e2::indexOf(layout, h.x, h.y))];
    };
    auto flag = [&](e2::BattleHex h, uint32_t f) {
      if (Renderer::GridCell* c = at(h)) c->flags |= f;
    };
    // A foe in reach of a strike is marked on every hex of its body (Engine2 boardOverlayLayers: atkTiles.push(...footprint(foe))).
    auto flagBody = [&](const e2::BattleUnit& u, uint32_t f) {
      for (const e2::BattleHex& p : e2::Battle::footprint(u, u.x, u.y)) flag(p, f);
    };
    const bool walking = selected_ >= 0 && !unitWalk_[static_cast<size_t>(selected_)].points.empty();
    if (selected_ >= 0 && !walking && !spellMode_.empty()) {
      // The spell's zone (where it reaches), the hexes it may be aimed at right now, the hovered aim's area.
      const size_t b = static_cast<size_t>(unitBattle_[static_cast<size_t>(selected_)]);
      for (const e2::BattleHex& c : spellZone_) flag(c, Renderer::kGridSpellAim);
      const bool direction = e2::spellAimsDirection(spellMode_);
      for (const e2::BattleHex& c : spellAim_) flag(c, (direction ? 0u : Renderer::kGridSpellAim) | Renderer::kGridSpellTarget);
      // The line / cone: under the cursor; else the one aimed and waiting for Confirmar (the cursor went to the button);
      // else toward the nearest foe it reaches while the cursor is off the board. Everyone it would hit is locked: foes in
      // violet, friends in red (a line or a cone that crosses the party hurts it).
      std::optional<e2::BattleHex> aimed;
      if (hover_ && aimValid(b, *hover_)) aimed = hover_;
      else if (spellArmed_ && aimValid(b, *spellArmed_)) aimed = spellArmed_;
      else if (direction && !hover_ && spellPreviewAim_) aimed = spellPreviewAim_;
      if (aimed) {
        for (const e2::BattleHex& c : aimArea(b, *aimed)) flag(c, Renderer::kGridSpellArea);
        for (size_t id : aimHits(b, *aimed)) {
          const e2::BattleUnit& t = battle_->units()[id];
          if (t.alive && !t.hidden && unitOnShow(t)) flag({t.x, t.y}, t.side == battle_->units()[b].side ? Renderer::kGridTarget : Renderer::kGridSpellTarget);
        }
      }
      if (hover_) flag(*hover_, Renderer::kGridHover | (aimValid(b, *hover_) ? 0u : Renderer::kGridHoverBlocked));
      for (const e2::BattleHex& c : spellCells_) flag(c, Renderer::kGridSpellChosen);
      const e2::BattleUnit& u = battle_->units()[b];
      flag({u.x, u.y}, Renderer::kGridActive);
    } else if (selected_ >= 0 && !walking) {
      const bool player = scene_->units[static_cast<size_t>(selected_)].side == "player";
      const e2::BattleUnit& u = battle_->units()[static_cast<size_t>(unitBattle_[static_cast<size_t>(selected_)])];
      // The reach: the party's movement, or a foe's threat zone; each hex fainter towards the limit of the movement.
      // "Atacar": instead, the hexes its weapon reaches from where it stands, and the foes it can strike.
      if (attackMode_) {
        const bool arrow = u.maxRange > 1;
        for (int y = 0; y < battle_->rows(); ++y)
          for (int x = 0; x < battle_->cols(); ++x) {
            const int d = e2::Battle::distance({u.x, u.y}, {x, y});
            if (d < u.minRange || d > u.maxRange) continue;
            if (d > 1 && !e2::clearShot(*battle_, {u.x, u.y}, {x, y}, arrow)) continue;
            flag({x, y}, Renderer::kGridStrikeZone);
          }
      } else if (!(player && battle_->roaming())) {  // free roam reaches the whole floor: no wash (the plate says so instead)
        int maxCost = 1;
        for (const e2::ReachCell& c : reach_) maxCost = std::max(maxCost, c.cost);
        for (const e2::ReachCell& c : reach_) {
          Renderer::GridCell* cell = at({c.x, c.y});
          if (!cell) continue;
          cell->flags |= player ? Renderer::kGridReach : Renderer::kGridReachFoe;
          cell->extra |= static_cast<uint32_t>(std::clamp(255 * c.cost / maxCost, 0, 255)) << 8;
        }
      }
      // The route to the hovered hex: steps numbered from the unit's own hex (1).
      if (player && hover_ && !(hover_->x == u.x && hover_->y == u.y) &&
          std::any_of(reach_.begin(), reach_.end(), [&](const e2::ReachCell& c) { return c.x == hover_->x && c.y == hover_->y; })) {
        const std::vector<e2::BattleHex> route = e2::Battle::path(reachPaths_, hover_->x, hover_->y);
        for (size_t i = 0; i < route.size() && i < 255; ++i)
          if (Renderer::GridCell* cell = at(route[i])) {
            cell->extra = (cell->extra & ~0xFFu) | static_cast<uint32_t>(i + 1);
            if (i + 1 == route.size()) cell->flags |= Renderer::kGridRouteEnd;
          }
      }
      flag({u.x, u.y}, player ? Renderer::kGridActive : Renderer::kGridActiveFoe);
    }
    // The foes in reach of a strike.
    for (const auto& [t, from] : targets_) {
      const e2::BattleUnit& f = battle_->units()[static_cast<size_t>(unitBattle_[static_cast<size_t>(t)])];
      flagBody(f, Renderer::kGridTarget);
    }
    // The cursor: marked where nothing can stand.
    if (hover_ && spellMode_.empty()) {
      const bool blocked = !battle_->cell(hover_->x, hover_->y).passable;
      flag(*hover_, Renderer::kGridHover | (blocked ? Renderer::kGridHoverBlocked : 0u));
    }
    if (fog_ && !noFog_ && fogReady_)
      for (size_t i = 0; i < cells.size() && i < fogStates_.size(); ++i)
        if (fogStates_[i] == e2::kFogUnseen) cells[i] = Renderer::GridCell{};
  }

  // Over the scene: the numbers popping off struck units and the strike forecast.
  void drawMarks() {
    if (!battle_ || !scene_) return;
    const ImGuiIO& io = ImGui::GetIO();
    drawPopups(ImGui::GetBackgroundDrawList(), io.DisplaySize.x, io.DisplaySize.y);
  }

  // The numbers over a struck unit: up 0.6 units over 1.2 s while they fade (crits larger and gold; "Errou" for a miss).
  void drawPopups(ImDrawList* dl, float w, float h) {
    std::erase_if(popups_, [&](const Popup& p) { return now_ - p.start > kPopupSeconds; });
    for (const Popup& p : popups_) {
      const CardRect& c = unitCard_[p.unit];
      const float k = (now_ - p.start) / kPopupSeconds;
      const glm::vec3 up = cardUp(c);
      const std::optional<ImVec2> at = screenOf(c.origin + up * (c.height + 0.6f * k), w, h);
      if (!at) continue;
      const float size = p.big ? 34.0f : 26.0f;
      ImGui::PushFont(nullptr, size);
      const ImVec2 t = ImGui::CalcTextSize(p.text.c_str());
      const ImU32 alpha = static_cast<ImU32>(255.0f * std::clamp(1.5f - 1.5f * k, 0.0f, 1.0f));
      const ImVec2 pos{at->x - t.x * 0.5f, at->y - t.y};
      dl->AddText(nullptr, size, {pos.x + 2, pos.y + 2}, (alpha << IM_COL32_A_SHIFT), p.text.c_str());
      dl->AddText(nullptr, size, pos, (p.color & ~IM_COL32_A_MASK) | (alpha << IM_COL32_A_SHIFT), p.text.c_str());
      ImGui::PopFont();
    }
  }

  // Engine2's forecast for the hovered target: the strike and the counter, as an ember plate by the cursor.
  void drawForecast() {
    if (!hover_ || selected_ < 0 || exchange_) return;
    const int foe = sceneUnitOf(battle_->unitAt(hover_->x, hover_->y));
    const auto target = std::find_if(targets_.begin(), targets_.end(), [&](const auto& t) { return t.first == foe; });
    if (foe < 0 || target == targets_.end()) return;
    const size_t ab = static_cast<size_t>(unitBattle_[static_cast<size_t>(selected_)]), db = static_cast<size_t>(unitBattle_[static_cast<size_t>(foe)]);
    const e2::BattleUnit &a = battle_->units()[ab], &d = battle_->units()[db];
    const e2::Forecast f = forecastFor(ab, db, target->second);  // from the hex it would strike from
    const ImVec2 cursor = ImGui::GetIO().MousePos;  // canvas units (mouse_ is in window pixels)
    ImGui::SetNextWindowPos({cursor.x + 18.0f, cursor.y + 18.0f}, ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {12.0f, 10.0f});
    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(14, 14, 14, 240));
    ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(190, 150, 95, 115));
    ImGui::Begin("##forecast", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoInputs |
                                            ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoSavedSettings);
    ImGui::PushFont(nullptr, 20.0f);
    ImGui::TextColored(ImVec4(1.0f, 0.88f, 0.7f, 1.0f), "%s  ->  %s", a.name.c_str(), d.name.c_str());
    ImGui::Text("Vida %d / %d", d.hp, d.maxHp);
    ImGui::Text("Dano %d  ·  %.0f%%%s", f.dmgOut, f.hitOut, f.kill ? "  ·  mata" : "");
    if (f.canCounter) ImGui::Text("Contra-ataque %d  ·  %.0f%%", f.dmgBack, f.hitBack);
    else ImGui::TextDisabled("Sem contra-ataque");
    ImGui::PopFont();
    ImGui::End();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar();
  }

  // --- Engine2's battle HUD (GameApp.tsx footer .ember-hud-bar, battleHud3d.ts health bars, the turn order) -----------

  // Engine2 ThreeAtmosphereV2: the map's rising wisps (wispIntensity x 2000, wispSpeed, wispColor) and the dust
  // motes always in the air (brighter in sunlight); additive and over everything, as there (no depth test).
  void drawAirParticles() {
    if (!scene_ || flat_ || !options_.airParticles) return;
    const e2::MapDraft& d = board_->draft();
    static std::vector<float> wispSeeds, dustSeeds;
    if (wispSeeds.empty()) {
      e2::Mulberry32 a(0x5eed2), b(0x5eed1);
      for (int i = 0; i < 2000 * 4; ++i) wispSeeds.push_back(static_cast<float>(a()));
      for (int i = 0; i < 280 * 4; ++i) dustSeeds.push_back(static_cast<float>(b()));
    }
    const ImGuiIO& io = ImGui::GetIO();
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    const float w = io.DisplaySize.x, h = io.DisplaySize.y;
    const e2::Board::Bounds bb = board_->bounds();
    auto disc = [&](glm::vec3 p, float size, ImU32 rgb, float alpha) {
      const std::optional<ImVec2> at = screenOf(p, w, h);
      const std::optional<ImVec2> side = screenOf(p + glm::vec3(size, 0.0f, 0.0f), w, h);
      if (!at || !side || alpha <= 0.003f) return;
      const float drawn = std::hypot(side->x - at->x, side->y - at->y);
      // Near the camera a wisp is a big yellow blot over the board (the user, 2026-10-17): the same fade as the health
      // bars. A world unit is `drawn / size` pixels here; kAirCardHeight of them is what a unit's card would measure, and
      // its share of the screen's height runs through the bars' thresholds (kBarsFadeFrom .. kBarsFadeTo).
      {  // always: no choice in the panel (the user: "quero que simplesmente funcione dessa maneira sempre")
        const float share = drawn / std::max(size, 1e-4f) * kAirCardHeight / std::max(h, 1.0f);
        alpha *= 1.0f - glm::smoothstep(kBarsFadeFrom, kBarsFadeTo, share);
        if (alpha <= 0.003f) return;
      }
      const float r = std::max(1.0f, drawn);
      // a soft disc: smoothstep(1, 0, d)^2 as three rings
      for (int k = 0; k < 3; ++k) {
        const float rr = r * (1.0f - 0.3f * k), aa = alpha * (0.25f + 0.25f * k);
        dl->AddCircleFilled(*at, rr, (rgb & 0x00FFFFFFu) | (static_cast<ImU32>(std::clamp(aa, 0.0f, 1.0f) * 255.0f) << IM_COL32_A_SHIFT), 12);
      }
    };
    // Wisps: each rises 1.6 hexes from the ground over its life, wavering, fading in and out.
    const int wisps = static_cast<int>(std::round(std::clamp(d.wispIntensity, 0.0f, 1.0f) * 2000.0f));
    if (wisps > 0) {
      const float t = now_ * d.wispSpeed, size = 0.1f, rise = 1.6f, pad = 3.0f;
      const float ox = static_cast<float>(bb.x0) - pad, oz = static_cast<float>(bb.z0) - pad;
      const float ax = static_cast<float>(bb.x1 - bb.x0) + 2 * pad, az = static_cast<float>(bb.z1 - bb.z0) + 2 * pad;
      const ImU32 rgb = IM_COL32((d.wispColor >> 16) & 255, (d.wispColor >> 8) & 255, d.wispColor & 255, 0);
      for (int i = 0; i < wisps; ++i) {
        const float* sd = &wispSeeds[static_cast<size_t>(i) * 4];
        float phase = sd[3] + t * 0.055f * (0.6f + sd[2] * 0.8f);
        phase -= std::floor(phase);
        const float x = ox + sd[0] * ax + std::sin(t * 0.7f + sd[2] * 40.0f) * size * 2.5f * phase;
        const float z = oz + sd[1] * az + std::cos(t * 0.5f + sd[3] * 30.0f) * size * 2.5f * phase;
        const float y = groundAt(std::clamp(x, static_cast<float>(bb.x0), static_cast<float>(bb.x1)), std::clamp(z, static_cast<float>(bb.z0), static_cast<float>(bb.z1))) + phase * rise;
        const float alpha = std::sin(phase * 3.14159f) * (0.6f + 0.4f * std::sin(t * 3.0f + sd[2] * 60.0f)) * 0.9f;
        disc({x, y, z}, size * (0.7f + sd[2] * 0.6f), rgb, alpha);
      }
    }
    // Dust motes drifting with the wind over the board, twinkling.
    {
      const float t = now_, size = 0.06f, height = 2.2f;
      const float ox = static_cast<float>(bb.x0) - 2.0f, oz = static_cast<float>(bb.z0) - 2.0f;
      const float ax = static_cast<float>(bb.x1 - bb.x0) + 4.0f, az = static_cast<float>(bb.z1 - bb.z0) + 4.0f;
      const float sun = layers_ & 1u ? 0.6f : 0.0f;  // brighter in sunlight
      for (int i = 0; i < 280; ++i) {
        const float* sd = &dustSeeds[static_cast<size_t>(i) * 4];
        float wx = sd[0] * ax + 0.12f * t * (0.6f + sd[2] * 0.8f) + std::sin(t * 0.21f + sd[3] * 31.0f) * size * 3.0f;
        float wz = sd[1] * az + 0.05f * t * (0.6f + sd[2] * 0.8f) + std::cos(t * 0.17f + sd[2] * 23.0f) * size * 3.0f;
        wx = std::fmod(wx, ax), wz = std::fmod(wz, az);
        const float x = ox + wx, z = oz + wz;
        const float ground = groundAt(std::clamp(x, static_cast<float>(bb.x0), static_cast<float>(bb.x1)), std::clamp(z, static_cast<float>(bb.z0), static_cast<float>(bb.z1)));
        const float y = ground + height * (0.15f + sd[2] * 0.85f) + std::sin(t * 0.35f + sd[3] * 19.0f) * size * 2.0f;
        const float twinkle = 0.5f + 0.5f * std::sin(t * (0.5f + sd[3] * 0.9f) + sd[2] * 40.0f);
        const float edge = std::clamp(std::min({wx, ax - wx, wz, az - wz}) / (std::min(ax, az) * 0.08f), 0.0f, 1.0f);
        const float alpha = (0.25f + 0.75f * twinkle * twinkle) * edge * (0.22f + 0.4f * sun);
        disc({x, y, z}, size * (0.55f + sd[3] * 0.9f), IM_COL32(255, 242, 217, 0), alpha * 0.6f);
      }
    }
  }

  // GameApp portraitFor: a hero's own portrait, an enemy's head-and-shoulders cut, else its sprite's first frame.
  const UiImage* portraitOf(const e2::BattleUnit& u) const {
    const std::filesystem::path file = portraitFileOf(u);
    return file.empty() ? nullptr : uiImage(file);
  }
  std::filesystem::path portraitFileOf(const e2::BattleUnit& u) const {
    static const std::pair<const char*, const char*> kHero[] = {
        {"defaultWarrior", "game/portraits/kael.png"}, {"kaelFinal", "game/portraits/kael-final-face-001.jpg"},
        {"neera", "game/portraits/NeeraProperSide-profile.jpg"}, {"voss", "game/portraits/voss.png"},
        {"salazar", "game/portraits/salazar-v2-bust.jpg"}, {"aldric", "game/portraits/aldric-profile-001.jpg"},
        {"defaultLancer", "game/portraits/aldric-profile-001.jpg"}, {"sandoval", "game/portraits/sandoval-001.jpg"},
        {"conjurer", "game/portraits/conjurer-002.png"}, {"malrec", "game/portraits/conjurer-002.png"},
        {"theButcher", "game/portraits/the-butcher-portrait-001.jpg"}, {"familiar3", "game/portraits/familiar3-profile.jpeg"},
        {"familiar", "game/portraits/familiar1-profile.png"}};
    const std::filesystem::path root = options_.engine2Public;
    for (const auto& [sprite, file] : kHero)
      if (u.sprite == sprite) return root / file;
    std::error_code error;
    const std::filesystem::path cut = root / "game/portraits/sprites" / (u.sprite + ".png");
    if (std::filesystem::exists(cut, error)) return cut;
    const std::filesystem::path frame = root / "game/sprites" / u.sprite / "1.png";
    if (std::filesystem::exists(frame, error)) return frame;
    return {};
  }

  // GameApp slotIcon / data.ts spellIcon. EmberEngine's own spells (the Mago Negro's promotions) take the user's art,
  // content/ui/spells/<kind>.jpg (portrait, drawn centre-cropped), before any Engine2 icon of the same kind.
  const UiImage* spellIconOf(const std::string& kind) const {
    const std::filesystem::path file = spellIconFileOf(kind);
    return file.empty() ? nullptr : uiImage(file);
  }
  // The file spellIconOf draws for `kind`, or none.
  std::filesystem::path spellIconFileOf(const std::string& kind) const {
    if (!e2::emberSpellClass(kind).empty()) {  // ours: the user's art when there is one (none yet for some: the initials)
      const std::filesystem::path own = std::filesystem::path("content/ui/spells") / (kind + ".jpg");
      std::error_code ec;
      if (std::filesystem::exists(own, ec)) return own;
    }
    if (const std::optional<std::string> potion = potionOf(kind)) return options_.engine2Public / "game/icons" / ("potion-" + *potion + ".png");
    static const std::pair<const char*, const char*> kIcons[] = {
        {"doubleStrike", "refresh-001/spells/cleave-crossed-blades.png"}, {"cleave", "refresh-001/spells/cleave.png"},
        {"fireball", "refresh-001/spells/fireball.png"}, {"frost", "frost.jpg"}, {"iceStorm", "ice-storm.jpg"},
        {"causticVenom", "refresh-001/spells/caustic-venom.png"}, {"minorVenom", "refresh-001/spells/caustic-venom.png"},
        {"divineBolt", "divine-bolt.jpg"}, {"lightning", "refresh-001/spells/lightning.png"}, {"lightningTier3", "refresh-001/spells/lightning.png"},
        {"shock", "refresh-001/spells/lightning.png"}, {"magicMissile", "refresh-001/spells/magic-missile.png"},
        {"longShot", "refresh-001/spells/long-shot.png"}, {"bloodyShot", "bloody-shot-002.jpg"}, {"piercing", "refresh-001/spells/piercing.png"},
        {"cureMinor", "refresh-006/healing/cure-light-new-006.png"}, {"cureWounds", "refresh-007/healing/cure-medium-golden-bowl-007.png"},
        {"cureDisease", "refresh-007/healing/cure-disease-green-lantern-007.png"}, {"piercingThrust", "refresh-001/spells/piercing-thrust.png"},
        {"sweep", "refresh-001/spells/sweep.png"}, {"trip", "refresh-001/spells/trip.png"}, {"summonFamiliar", "refresh-001/spells/summon-familiar.png"},
        {"phantasmalForce", "phantasmal-force.png"}, {"summonFamiliar2", "summon-familiar2.png"}, {"summonFamiliar3", "summon-familiar3.png"},
        {"summonFamiliar4", "summon-familiar4.png"}, {"summonZombieDog", "summon-zombie-dog-002.png"}, {"webOfDreams", "refresh-001/spells/web-of-dreams.png"},
        {"warp", "warp-portal.png"}, {"lifeDrain", "refresh-001/spells/life-drain.png"}, {"familiarLifeDrain", "refresh-001/spells/life-drain.png"}, {"multiShot", "refresh-006/combat/multi-shot-006.png"},
        {"cureLight", "refresh-006/healing/cure-light-new-006.png"}, {"bless", "refresh-001/spells/bless.png"}, {"provoke", "provoke.jpg"},
        {"bullRush", "bull-rush.png"}, {"executionerStrike", "executioner-strike-002.png"}, {"shieldBash", "shield-bash.png"},
        {"burningHands", "burning-hands.png"}, {"poisonBreath", "poison-breath.png"}, {"createFoodAndWater", "create-food-and-water.png"},
        {"turnUndead", "turn-undead.jpg"}};
    for (const auto& [k, file] : kIcons)
      if (kind == k) return options_.engine2Public / "game/icons" / file;
    return {};
  }

  // .ember-btn.ember-btn-sm: Pirata One, primary (the ember gradient) or ghost (dark iron, an ember underline on hover).
  // `icon` (a file under Engine2's game/icons, e.g. "lockpick.png") is drawn before the label.
  bool emberButton(const char* id, const char* label, bool primary, bool enabled, float height = 32.0f, const char* icon = nullptr) {
    ImFont* font = uiFont(UiFont::Mark);
    const float size = 16.0f;
    const UiImage* iconImage = icon ? uiImage(options_.engine2Public / "game/icons" / icon) : nullptr;
    const float iconSide = iconImage ? std::round(height * 0.66f) : 0.0f, iconGap = iconImage ? 6.0f : 0.0f;
    const float w = spacedTextWidth(font, size, label, 0.06f) + 28.0f + iconSide + iconGap;
    ImGui::BeginDisabled(!enabled);
    const bool clicked = ImGui::InvisibleButton(id, {w, height});
    const bool hover = enabled && ImGui::IsItemHovered();
    ImGui::EndDisabled();
    const ImVec2 a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const int fade = enabled ? 255 : 102;
    auto k = [&](int r, int g, int bl) {
      const float m = hover ? 1.1f : 1.0f;
      return IM_COL32(std::min(255, static_cast<int>(r * m)), std::min(255, static_cast<int>(g * m)), std::min(255, static_cast<int>(bl * m)), fade);
    };
    for (int i = 4; i >= 1; --i) dl->AddRectFilled({a.x - i, a.y + i * 2.0f}, {b.x + i, b.y + i * 2.0f}, IM_COL32(0, 0, 0, enabled ? 28 : 12), 3.0f);
    dl->AddRect({a.x - 1, a.y - 1}, {b.x + 1, b.y + 1}, IM_COL32(0, 0, 0, fade), 2.0f);
    if (primary) {
      if (hover) for (int i = 3; i >= 1; --i) dl->AddRect({a.x - i * 3.0f, a.y - i * 3.0f}, {b.x + i * 3.0f, b.y + i * 3.0f}, IM_COL32(255, 120, 35, 22), 4.0f, 3.0f);
      verticalGradient(dl, a, b, {{0.0f, k(0xd9, 0x62, 0x1c)}, {0.55f, k(0x9c, 0x2f, 0x10)}, {1.0f, k(0x56, 0x17, 0x08)}});
      dl->AddLine({a.x + 1, a.y + 1}, {b.x - 1, a.y + 1}, IM_COL32(255, 225, 160, enabled ? 140 : 50));
      dl->AddRect(a, b, IM_COL32(255, 200, 130, enabled ? 128 : 50), 2.0f);
    } else {
      verticalGradient(dl, a, b, {{0.0f, k(0x23, 0x23, 0x23)}, {0.55f, k(0x0a, 0x0a, 0x0a)}, {1.0f, k(0x1a, 0x1a, 0x1a)}});
      dl->AddRect(a, b, hover ? IM_COL32(226, 170, 100, 216) : IM_COL32(190, 150, 95, enabled ? 128 : 60), 2.0f);
      if (hover) {
        const float l = a.x + (b.x - a.x) * 0.18f, r = b.x - (b.x - a.x) * 0.18f, y = b.y - 3.0f;
        dl->AddRectFilledMultiColor({l, y}, {(l + r) * 0.5f, y + 1}, IM_COL32(226, 104, 30, 0), IM_COL32(226, 104, 30, 255), IM_COL32(226, 104, 30, 255), IM_COL32(226, 104, 30, 0));
        dl->AddRectFilledMultiColor({(l + r) * 0.5f, y}, {r, y + 1}, IM_COL32(226, 104, 30, 255), IM_COL32(226, 104, 30, 0), IM_COL32(226, 104, 30, 0), IM_COL32(226, 104, 30, 255));
      }
    }
    const float tw = spacedTextWidth(font, size, label, 0.06f);
    const float left = std::round((a.x + b.x - tw - iconSide - iconGap) * 0.5f);
    if (iconImage) {
      const float y = std::round((a.y + b.y - iconSide) * 0.5f);
      imageContain(dl, *iconImage, {left, y}, {left + iconSide, y + iconSide}, IM_COL32(255, 255, 255, fade));
    }
    const ImVec2 p{left + iconSide + iconGap, std::round((a.y + b.y - size) * 0.5f)};
    const ImU32 ink = primary ? IM_COL32(255, 241, 216, fade) : hover ? IM_COL32(255, 224, 180, 255) : IM_COL32(226, 194, 148, fade);
    spacedText(dl, font, size, {p.x, p.y + 1}, IM_COL32(0, 0, 0, fade), label, 0.06f, false);
    spacedText(dl, font, size, p, ink, label, 0.06f, false);
    return clicked && enabled;
  }

  // .ember-socket: a spell's icon with its uses left in the corner (lit while it is being aimed). `key` >= 0: the battle
  // bar's socket F<key + 1>, its key in the top-left corner; an empty `kind` is an empty socket.
  bool spellSocket(const char* id, const std::string& kind, int uses, bool enabled, bool active, float side = 36.0f, int key = -1) {
    ImGui::BeginDisabled(!enabled);
    const bool clicked = ImGui::InvisibleButton(id, {side, side});
    const bool hover = enabled && ImGui::IsItemHovered();
    ImGui::EndDisabled();
    const ImVec2 a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    verticalGradient(dl, a, b, {{0.0f, IM_COL32(12, 12, 12, 255)}, {1.0f, IM_COL32(5, 5, 5, 255)}});
    const int fade = enabled ? 255 : 102;
    const auto keyLabel = [&] {
      if (key < 0) return;
      const std::string k = std::format("F{}", key + 1);
      ImFont* font = uiFont(UiFont::Ui);
      dl->AddText(font, 10.0f, {a.x + 3.0f, a.y + 2.0f}, IM_COL32(0, 0, 0, 230), k.c_str());
      dl->AddText(font, 10.0f, {a.x + 2.0f, a.y + 1.0f}, IM_COL32(226, 194, 148, kind.empty() ? 150 : 255), k.c_str());
    };
    if (kind.empty()) {
      dl->AddRect(a, b, hover ? IM_COL32(226, 170, 100, 160) : IM_COL32(190, 150, 95, 56), 2.0f);
      keyLabel();
      return false;
    }
    const float inset = std::round(side * 6.0f / 36.0f);
    if (const UiImage* icon = spellIconOf(kind)) {
      imageCover(dl, *icon, {a.x + inset, a.y + inset}, {b.x - inset, b.y - inset}, 0.5f, 0.5f, IM_COL32(255, 255, 255, fade));  // object-cover
    } else if (!e2::emberSpellClass(kind).empty()) {
      // Ours have no Engine2 icon yet: the element's colour behind the spell's initials.
      static const std::pair<const char*, ImU32> kElementColors[] = {{"darkness", IM_COL32(92, 48, 150, 255)}, {"poison", IM_COL32(60, 130, 40, 255)},
                                                                     {"fire", IM_COL32(170, 70, 20, 255)},     {"ice", IM_COL32(60, 120, 170, 255)},
                                                                     {"lightning", IM_COL32(90, 110, 180, 255)}, {"ember", IM_COL32(190, 90, 25, 255)},
                                                                     {"steel", IM_COL32(95, 105, 120, 255)},    {"wood", IM_COL32(110, 85, 50, 255)}};
      ImU32 back = IM_COL32(60, 60, 60, 255);
      const std::string name = e2::spellName(kind);
      static const std::map<std::string, const char*> kElementOf = {
          {"markOfRuin", "darkness"},  {"creepingPlague", "poison"}, {"lifeDrain", "darkness"}, {"shroudOfShadows", "darkness"}, {"hemorrhage", "darkness"},
          {"profaneSilence", "darkness"}, {"blackPestilence", "poison"}, {"curseOfDecay", "darkness"}, {"soulHarvest", "darkness"}, {"devouringVoid", "darkness"},
          {"pillarOfFlame", "fire"},   {"iceLance", "ice"},          {"thunderstorm", "lightning"}, {"meteor", "fire"}, {"absoluteZero", "ice"},
          {"chainLightning", "lightning"}, {"eruption", "fire"},     {"glacialGale", "ice"},    {"wrathOfTheSkies", "lightning"}, {"primordialEmber", "ember"},
          // the Guerreiro's and the Arqueiro's (no icon yet: steel / wood)
          {"steelWhirlwind", "steel"}, {"guardBreak", "steel"}, {"warCry", "steel"}, {"leapingStrike", "steel"},
          {"venomArrow", "wood"}, {"arrowRain", "wood"}, {"explosiveArrow", "wood"}, {"cripplingShot", "wood"}, {"arrowStorm", "wood"}, {"deathShot", "wood"},
          {"spearToss", "steel"}, {"hookPull", "steel"}, {"poleVault", "steel"}, {"disarm", "steel"}, {"swapPost", "steel"}, {"impaleDrag", "steel"},
          {"sentinelVigil", "ember"}};
      if (const auto it = kElementOf.find(kind); it != kElementOf.end())
        for (const auto& [el, c] : kElementColors)
          if (std::string(el) == it->second) back = c;
      dl->AddRectFilled({a.x + 5, a.y + 5}, {b.x - 5, b.y - 5}, (back & ~IM_COL32_A_MASK) | (static_cast<ImU32>(fade) << IM_COL32_A_SHIFT), 4.0f);
      // initials: the first letter of the first two words
      std::string initials;
      bool wordStart = true;
      for (unsigned char ch : name) {
        if (ch == ' ') wordStart = true;
        else if (wordStart && initials.size() < 2 && ch != 'd' && ch != 'e') initials.push_back(static_cast<char>(ch)), wordStart = false;
        else wordStart = false;
      }
      ImFont* mark = uiFont(UiFont::Mark);
      const ImVec2 ts = mark->CalcTextSizeA(16.0f, FLT_MAX, 0.0f, initials.c_str());
      dl->AddText(mark, 16.0f, {std::round((a.x + b.x - ts.x) * 0.5f), std::round((a.y + b.y - ts.y) * 0.5f) - 1.0f}, IM_COL32(255, 240, 220, fade), initials.c_str());
    }
    if (active) for (int i = 3; i >= 1; --i) dl->AddRect({a.x - i * 2.0f, a.y - i * 2.0f}, {b.x + i * 2.0f, b.y + i * 2.0f}, IM_COL32(255, 110, 30, 40), 3.0f, 2.0f);
    dl->AddRect(a, b, active ? IM_COL32(226, 104, 30, 255) : hover ? IM_COL32(226, 170, 100, 216) : IM_COL32(190, 150, 95, 89), 2.0f);
    keyLabel();
    const std::string count = std::to_string(uses);
    ImFont* font = uiFont(UiFont::Ui);
    const ImVec2 t = font->CalcTextSizeA(12.0f, FLT_MAX, 0.0f, count.c_str());
    const ImVec2 c{b.x - t.x - 2.0f, b.y - t.y + 2.0f};
    dl->AddRectFilled({c.x - 2, c.y - 1}, {b.x + 3, b.y + 3}, IM_COL32(28, 28, 28, 255), 2.0f);
    dl->AddRect({c.x - 2, c.y - 1}, {b.x + 3, b.y + 3}, IM_COL32(74, 74, 74, 255), 2.0f);
    dl->AddText(font, 12.0f, c, IM_COL32(240, 240, 240, fade), count.c_str());
    if (hover) {
      ImGui::BeginTooltip();
      ImGui::PushFont(uiFont(UiFont::Ui), 15.0f);
      if (const std::optional<std::string> potion = potionOf(kind)) {
        ImGui::TextUnformatted(e2::potionName(*potion).c_str());
        ImGui::TextDisabled("%s", tr("você ou um aliado ao lado · gasta a ação", "yourself or an adjacent ally · uses the action").c_str());
      } else {
        ImGui::TextUnformatted(e2::spellName(kind).c_str());
        if (!e2::spellPorted(kind)) ImGui::TextDisabled("ainda não existe no Ember");
        else if (selected_ >= 0 && !e2::emberSpellClass(kind).empty()) {
          const std::string about = e2::spellTooltip(battle_->units()[static_cast<size_t>(unitBattle_[static_cast<size_t>(selected_)])], kind);
          if (!about.empty()) ImGui::TextDisabled("%s", about.c_str());
        }
      }
      ImGui::PopFont();
      ImGui::EndTooltip();
    }
    return clicked && enabled;
  }

  // The unit the bar describes: the one under the cursor, else the one picked, else whoever's turn it is.
  int hudUnit() const {
    if (hover_) {
      const int b = battle_->unitAt(hover_->x, hover_->y);
      const int ui = sceneUnitOf(b);
      if (ui >= 0 && unitCard_[static_cast<size_t>(ui)].drawn && battle_->units()[static_cast<size_t>(b)].alive) return b;
    }
    if (selected_ >= 0 && unitBattle_[static_cast<size_t>(selected_)] >= 0) return unitBattle_[static_cast<size_t>(selected_)];
    return battle_->activeUnit();
  }

  static std::string sheetLine(const e2::BattleUnit& u) {
    const std::string range = u.minRange == u.maxRange ? std::to_string(u.maxRange) : std::format("{}-{}", u.minRange, u.maxRange);
    return std::format("AT {} · MAG {} · DF {} · DEX {} · Mov {} · Alc {}", u.atk, u.mag, u.def, u.dex, u.mov, range);
  }

  // The combat log inside the HUD bar: the newest line at the bottom, following new lines while the reader is at the bottom.
  void drawHudLog(ImVec2 from, ImVec2 to) {
    ImGui::SetCursorScreenPos(from);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(0, 0, 0, 0));
    ImGui::BeginChild("##hudlog", {to.x - from.x, to.y - from.y}, false);
    const float maxY = ImGui::GetScrollMaxY();
    if (!logJustOpened_) logPinned_ = maxY <= 0.0f || ImGui::GetScrollY() >= maxY - 8.0f;
    const std::vector<std::string>& lines = battle_->log();
    if (lines.empty()) {
      ImGui::TextDisabled("Nada aconteceu ainda.");
    } else {
      for (const std::string& line : lines) {
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(180, 180, 180, 255));
        ImGui::TextWrapped("%s", line.c_str());
        ImGui::PopStyleColor();
      }
    }
    if (logPinned_ || logJustOpened_) ImGui::SetScrollHereY(1.0f);
    logJustOpened_ = false;
    ImGui::EndChild();
    ImGui::PopStyleColor();
  }

  // "Abrir log": the whole combat log full screen, opened at the newest line; a click anywhere (or Esc) closes it.
  void drawFullLog() {
    if (!logFull_ || !battle_) return;
    const ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::SetNextWindowFocus();  // above the panels, every frame while it is open
    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(8, 8, 8, 232));
    ImGui::Begin("##fullog", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);
    ImGui::PushFont(uiFont(UiFont::Ui), 18.0f);
    ImGui::TextColored(ImColor(240, 240, 240), "%s", tr("Log de combate", "Combat log").c_str());
    ImGui::SameLine();
    ImGui::TextDisabled("  %s", tr("Clique em qualquer lugar para fechar", "Click anywhere to close").c_str());
    ImGui::Separator();
    ImGui::BeginChild("##fullogbody", {0, 0}, false);
    const std::vector<std::string>& lines = battle_->log();
    if (lines.empty()) ImGui::TextDisabled("Nada aconteceu ainda.");
    for (const std::string& line : lines) ImGui::TextWrapped("%s", line.c_str());
    if (logFullJustOpened_) ImGui::SetScrollHereY(1.0f);
    ImGui::EndChild();
    if (!logFullJustOpened_ && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows)) logFull_ = false;
    logFullJustOpened_ = false;
    ImGui::PopFont();
    ImGui::End();
    ImGui::PopStyleColor();
  }

  // --- Engine2's Inn quests in a fight (world/Quests.h): the pickups on the ground, collected by walking onto them, and the
  // quest log (QuestLog.h) following the fight's finds and kills live. Nothing is announced on screen: the combat log and the
  // quest log say it (the user, 2026-10-17).
  e2::Json questTable(const char* key) const {
    if (!options_.world) return e2::Json::array();
    const e2::Json& t = options_.world->tables();
    return t.contains(key) && t[key].is_array() ? t[key] : e2::Json::array();
  }
  // engine.ts collectQuestPickups (after every walk): a hero whose walk is over on a pickup's hex picks it up.
  void updateQuestPickups() {
    if (!battle_ || battle_->questPickups().empty() || !result_.empty()) return;
    const auto settled = [&](size_t b) {
      const int s = sceneUnitOf(static_cast<int>(b));
      return s < 0 || static_cast<size_t>(s) >= unitWalk_.size() || unitWalk_[static_cast<size_t>(s)].points.empty();
    };
    for (const e2::QuestPickup& p : battle_->collectQuestPickups(settled)) logInfo("quest pickup {} found at ({}, {})", p.key, p.x, p.y);
  }
  // Engine2 drawQuestPickups: a small pulsing gold glint lying on each pickup's hex (not a hex outline), hidden under the fog of
  // war until the hex has been seen.
  void drawQuestPickups() {
    if (!battle_ || !board_ || battle_->questPickups().empty()) return;
    const ImGuiIO& io = ImGui::GetIO();
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    const float w = io.DisplaySize.x, h = io.DisplaySize.y;
    const e2::HexLayout& L = board_->layout();
    const float pulse = 0.75f + std::sin(now_ * 3.4f) * 0.25f;
    for (const e2::QuestPickup& p : battle_->questPickups()) {
      if (!e2::inBoard(L, p.x, p.y)) continue;
      const size_t i = static_cast<size_t>(e2::indexOf(L, p.x, p.y));
      if (fog_ && !noFog_ && (i >= fogStates_.size() || fogStates_[i] == e2::kFogUnseen)) continue;
      const float cx = static_cast<float>(e2::hexX(L, p.x, p.y)), cz = static_cast<float>(e2::hexZ(L, p.y));
      const float y = static_cast<float>(e2::groundHeightAt(*board_, look_, cx, cz)) + 0.02f;
      auto diamond = [&](float r, ImU32 color) {  // lying on the ground: its long axis along the board's depth, as Engine2's
        const std::optional<ImVec2> a = screenOf({cx, y, cz - r}, w, h), b = screenOf({cx + 0.7f * r, y, cz}, w, h);
        const std::optional<ImVec2> c = screenOf({cx, y, cz + r}, w, h), d = screenOf({cx - 0.7f * r, y, cz}, w, h);
        if (a && b && c && d) dl->AddQuadFilled(*a, *b, *c, *d, color);
      };
      for (int k = 3; k >= 1; --k)  // the glow (shadowBlur 0.45 hex x pulse)
        diamond(0.2f * (1.0f + 0.5f * static_cast<float>(k) * pulse), IM_COL32(255, 208, 96, static_cast<int>(0.85f * pulse * 255.0f / static_cast<float>(2 * k + 1))));
      diamond(0.2f, IM_COL32(255, 226, 140, 242));
    }
  }
  void drawQuestLog() {
    if (!questLog_) return;
    if (!options_.world) return void(questLog_ = false);
    e2::Json save = gearSave();  // the campaign's save (Modo teste: a new one), with this fight's finds and kills folded in
    e2::foldQuestProgress(save, questFound(), questKills());
    if (!drawQuestLogPanel(questTable("quests"), questTable("potions"), save)) questLog_ = false;
  }

  // --- Engine2's character status sheet (BattleStatus.h), opened by "Status" for the unit the bar describes ----------------

  void openStatus(int battleUnit) {
    if (battleUnit >= 0 && battle_) statusSheet_.open(static_cast<size_t>(battleUnit));
  }

  // The party's inventory over the fight (Engine2: the status sheet's Mochila / Equipar open the backpack / paper doll with the
  // live battle patched into the save: the heroes' potions, HP, conditions). A Modo teste fight shows a brand-new save's gear.
  bool ensureInnRules() {
    if (!options_.world) return false;
    if (!innRules_) {
      if (!ensureInnData()) return false;
      innRules_.emplace(innData_, *options_.world);
    }
    return true;
  }
  // The weapons' and gear's names and icons (content/engine2/inn.json), loaded once: the Mochila and the chest-loot popup read them.
  bool ensureInnData() {
    if (!innDataLoaded_) innDataLoaded_ = innData_.load("content/engine2/inn.json");
    return innDataLoaded_;
  }
  e2::Json gearSave() const { return campaign_ ? (testMode_ || playtest_ ? campaign_->emptySave() : campaign_->activeSave()) : e2::Json::object(); }

  void openInventory(size_t unit, bool backpack) {
    if (!campaign_ || !ensureInnRules() || !battle_ || unit >= battle_->units().size()) return;
    inventorySave_ = gearSave();
    for (const e2::BattleUnit& u : battle_->units()) {
      if (u.side != "player") continue;
      e2::Json& bag = inventorySave_["bags"][u.name];
      if (!bag.is_object()) bag = e2::Json::object();
      for (int k = 0; k < 7; ++k) bag[e2::kPotionIds[k]] = u.bag[k];
      bag["lockpick"] = u.lockpick;
      inventorySave_["unitHp"][u.name] = u.hp;
      if (u.fullness) inventorySave_["heroHunger"][u.name] = *u.fullness;
    }
    statusSheet_.close();
    battleInventory_.emplace(battle_->units()[unit].name, backpack);
  }

  // What the inventory changed, into the fight: the worn pieces' stats, the potions, HP, disease and poison of every hero (the
  // main-hand weapon's dice and reach come with the next fight); a campaign save keeps it.
  void syncInventory() {
    if (!battle_ || !innRules_) return;
    if (campaign_ && !testMode_ && !playtest_) campaign_->saveActive(inventorySave_);
    for (e2::BattleUnit& u : battle_->units()) {
      if (u.side != "player") continue;
      const e2::InnRules::GearBonus g = innRules_->gearBonus(inventorySave_, u.name, u.classId);
      u.gearHp = g.hp, u.gearAtk = g.atk, u.gearMag = g.mag, u.gearDef = g.def, u.gearDex = g.dex, u.gearMov = g.mov;
      const e2::Json& bag = inventorySave_["bags"][u.name];
      if (bag.is_object()) {
        for (int k = 0; k < 7; ++k) u.bag[k] = bag.value(e2::kPotionIds[k], u.bag[k]);
        u.lockpick = bag.value("lockpick", u.lockpick);
      }
      const e2::Json& diseases = inventorySave_["heroDiseases"];
      u.diseased = diseases.is_object() && diseases.contains(u.name) && diseases[u.name] == true;
      const e2::Json& poisons = inventorySave_["heroPoisons"];
      const bool poisoned = poisons.is_object() && poisons.contains(u.name) && poisons[u.name].is_string();
      u.poisoned = poisoned, u.poisonTier = poisoned ? poisons[u.name].get<std::string>() : std::string();
      const int hp = inventorySave_["unitHp"].is_object() && inventorySave_["unitHp"].contains(u.name) ? inventorySave_["unitHp"][u.name].get<int>() : u.hp;
      e2::reapplyGear(u);
      u.hp = std::min(u.maxHp, hp);
    }
  }

  void drawBattleInventory() {
    if (!battleInventory_ || !innRules_ || !options_.world) return;
    const ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::SetNextWindowFocus();
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(0, 0, 0, 0));
    ImGui::Begin("##battleinventory", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollWithMouse);
    InventoryScreen::Context context{inventorySave_, [this] { syncInventory(); }, *innRules_, innData_, *options_.world, options_.engine2Public};
    context.allHeroes = testMode_ || playtest_;
    const bool closed = battleInventory_->draw(io.DisplaySize, context);
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
    if (closed) battleInventory_.reset();
  }

  void drawStatusSheet() {
    if (!statusSheet_.isOpen() || !battle_) return;
    BattleStatusSheet::Hooks hooks;
    // The close-up photos only on the status sheet (the user, 2026-10-18: "zoom só no status"); every other screen keeps the originals.
    hooks.portraitWhole = [](const e2::BattleUnit& u) { return u.side == "player"; };  // the party's 2:3 box (the user, 2026-10-18)
    hooks.portrait = [this](const e2::BattleUnit& u) -> const UiImage* {
      static const std::pair<const char*, const char*> kClose[] = {{"kaelFinal", "content/ui/portraits/kael-close-v01.jpg"}, {"neera", "content/ui/portraits/neera-close-v01.jpg"},
                                                                   {"voss", "content/ui/portraits/voss-close-v01.jpg"},
                                                                   {"aldric", "content/ui/portraits/aldric-close-v02.jpg"},
                                                                   {"defaultLancer", "content/ui/portraits/aldric-close-v02.jpg"}};
      for (const auto& [sprite, file] : kClose)
        if (u.sprite == sprite && std::filesystem::exists(file))
          if (const UiImage* close = uiImage(file)) return close;
      return portraitOf(u);
    };
    hooks.spellIcon = [this](const std::string& kind) { return spellIconOf(kind); };
    hooks.xpToLevel = [](int level) { return xpToLevel(level); };
    hooks.allocation = [](const e2::BattleUnit& u) { return std::array<int, 5>{u.pointHp, u.pointAtk, u.pointMag, u.pointDef, u.pointDex}; };
    hooks.adjust = [this](size_t unit, int stat, int delta) { return adjustStatPoint(unit, stat, delta); };
    if (campaign_ && options_.world && ensureInnRules()) {
      hooks.openPack = [this](size_t unit) { openInventory(unit, true); };
      hooks.openEquip = [this](size_t unit) { openInventory(unit, false); };
    }
    hooks.gearLines = [this](const e2::BattleUnit& u, const char* stat) {
      if (!campaign_ || !ensureInnRules()) return std::vector<std::string>{};
      return innRules_->gearLines(gearSave(), u.name, stat);
    };
    hooks.potionTip = [this](const std::string& id) -> std::string {
      if (!ensureInnRules()) return {};
      const e2::InnPotion* p = innData_.potion(id);
      return p ? p->tooltip : std::string();
    };
    hooks.lockpickTip = [this]() { return ensureInnRules() ? innData_.lockpickTooltip : std::string(); };
    // The arrows browse the opened unit's own side only (the user, 2026-10-18): a hero's sheet goes through the party; a foe's
    // (opened by clicking it) through the foes the party can see.
    hooks.order = [this] {
      std::vector<size_t> order;
      const std::string side = battle_->units()[statusSheet_.unit()].side;
      for (size_t i : battle_->turnOrder()) {
        const e2::BattleUnit& v = battle_->units()[i];
        if (!v.alive || v.side != side) continue;
        if (side != "player") {
          const int ui = sceneUnitOf(static_cast<int>(i));
          if (ui < 0 || !unitCard_[static_cast<size_t>(ui)].drawn) continue;  // not in sight
        }
        order.push_back(i);
      }
      return order;
    };
    statusSheet_.draw(*battle_, options_.engine2Public, hooks);
  }

  // Engine2 adjustStatPoint (+ GameApp onAdjustStatPoint): one permanent level-up point put on, or taken off, VIT / ATK /
  // MAG / DEF / DEX of a hero (three per level of the hero's level in the fight); the stats follow at once (a point on
  // vitality heals as much), and a campaign fight writes the allocation into the save. Modo teste keeps it in the fight.
  bool adjustStatPoint(size_t b, int stat, int delta) {
    if (!battle_ || b >= battle_->units().size() || stat < 0 || stat > 4 || !result_.empty()) return false;
    e2::BattleUnit& u = battle_->units()[b];
    if (u.side != "player" || !u.alive) return false;
    int* slot[5] = {&u.pointHp, &u.pointAtk, &u.pointMag, &u.pointDef, &u.pointDex};
    const int spent = u.pointHp + u.pointAtk + u.pointMag + u.pointDef + u.pointDex, budget = std::max(0, (u.level - 1) * 3);
    if ((delta > 0 && spent >= budget) || (delta < 0 && *slot[stat] <= 0)) return false;
    const int previousMax = u.maxHp;
    *slot[stat] += delta;
    e2::reapplyGear(u);
    if (stat == 0 && delta > 0) u.hp = std::min(u.maxHp, u.hp + (u.maxHp - previousMax));
    static const char* kNames[5] = {"hp", "atk", "mag", "def", "dex"};
    std::string label = kNames[stat];
    for (char& c : label) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    battle_->pushLog(std::format("{}: {} {}.", u.name, label, delta > 0 ? "+1" : "−1"));
    if (campaign_ && !testMode_) {
      e2::Json save = campaign_->activeSave();
      e2::Json& all = save["statPointAllocations"];
      if (!all.is_object()) all = e2::Json::object();
      e2::Json& mine = all[u.name];
      if (!mine.is_object()) mine = e2::Json::object();
      if (*slot[stat] > 0) mine[kNames[stat]] = *slot[stat];
      else mine.erase(kNames[stat]);
      campaign_->saveActive(std::move(save));
    }
    return true;
  }

  // --- The battle bar's 12 sockets, F1..F12, per hero (ours; the user, 2026-10-18) ------------------------------------------
  // Each socket holds a spell / skill kind or "potion:<id>" ("" = empty). A hero with no layout of its own gets its spells and
  // then the potions it carries, in order. A campaign keeps each hero's layout in the save ("hotbars": name -> 12 entries);
  // Modo teste keeps it for the session, never in the bank. "Atalhos" opens the editor (drag into a socket).
  static constexpr int kHotbarSlots = 12;
  using Hotbar = std::array<std::string, kHotbarSlots>;
  static std::map<std::string, Hotbar>& sessionHotbars() {
    static std::map<std::string, Hotbar> kept;
    return kept;
  }
  bool hotbarInSave() const { return campaign_ && !testMode_ && !playtest_; }
  // What the hero can put in a socket now: its spells and skills, then the potions it carries.
  std::vector<std::string> hotbarChoices(size_t b) const {
    const e2::BattleUnit& u = battle_->units()[b];
    std::vector<std::string> out(u.spellList.begin(), u.spellList.end());
    if (u.side == "player")
      for (int k = 0; k < 7; ++k)
        if (u.bag[k] > 0) out.push_back(std::string("potion:") + e2::kPotionIds[k]);
    return out;
  }
  Hotbar& hotbarOf(size_t b) {
    const e2::BattleUnit& u = battle_->units()[b];
    if (const auto it = hotbars_.find(u.name); it != hotbars_.end()) return it->second;
    Hotbar bar{};
    bool stored = false;
    if (hotbarInSave()) {
      const e2::Json save = campaign_->activeSave();
      if (const auto all = save.find("hotbars"); all != save.end() && all->is_object() && all->contains(u.name) && (*all)[u.name].is_array()) {
        const e2::Json& list = (*all)[u.name];
        for (size_t i = 0; i < bar.size() && i < list.size(); ++i)
          if (list[i].is_string()) bar[i] = list[i].get<std::string>();
        stored = true;
      }
    } else if (const auto it = sessionHotbars().find(u.name); it != sessionHotbars().end()) {
      bar = it->second, stored = true;
    }
    if (!stored) {
      const std::vector<std::string> choices = hotbarChoices(b);
      for (size_t i = 0; i < bar.size() && i < choices.size(); ++i) bar[i] = choices[i];
    }
    return hotbars_[u.name] = bar;
  }
  void storeHotbar(size_t b) {
    const e2::BattleUnit& u = battle_->units()[b];
    const Hotbar& bar = hotbarOf(b);
    if (!hotbarInSave()) return void(sessionHotbars()[u.name] = bar);
    e2::Json save = campaign_->activeSave();
    e2::Json& all = save["hotbars"];
    if (!all.is_object()) all = e2::Json::object();
    all[u.name] = e2::Json(std::vector<std::string>(bar.begin(), bar.end()));
    campaign_->saveActive(std::move(save));
  }
  // A socket's entry as the hero has it now: its uses left and whether it can be used (nullopt: the hero no longer has it).
  struct SocketState {
    int uses = 0;
    bool usable = false;
  };
  std::optional<SocketState> socketState(size_t b, const std::string& kind, bool canAct) const {
    if (kind.empty()) return std::nullopt;
    const e2::BattleUnit& u = battle_->units()[b];
    if (const std::optional<std::string> potion = potionOf(kind)) {
      for (int k = 0; k < 7; ++k)
        if (*potion == e2::kPotionIds[k]) return SocketState{u.bag[k], canAct && u.bag[k] > 0};
      return std::nullopt;
    }
    if (std::find(u.spellList.begin(), u.spellList.end(), kind) == u.spellList.end()) return std::nullopt;
    const int uses = e2::spellUsesLeft(*battle_, b, kind);
    return SocketState{uses, canAct && e2::spellPorted(kind) && uses > 0};
  }
  // F1..F12: the socket's spell / potion, as a click on it.
  void pressHotbar(int slot) {
    if (hotbarEdit_ || pauseOpen_ || battleInventory_ || statusSheet_.isOpen() || !battle_ || battle_->roaming() || !myTurn() || busy()) return;
    const size_t me = static_cast<size_t>(unitBattle_[static_cast<size_t>(selected_)]);
    const e2::BattleUnit& u = battle_->units()[me];
    const std::string kind = hotbarOf(me)[static_cast<size_t>(slot)];
    if (const std::optional<SocketState> s = socketState(me, kind, !u.acted); s && s->usable) startSpell(kind);
  }

  // "Atalhos": the hero's 12 sockets on top, everything it can put in them below; drag one into a socket (a socket dragged
  // onto another swaps them; a right click empties it).
  void drawHotbarEditor() {
    if (!hotbarEdit_ || !battle_) return;
    const int ub = hotbarHero_;  // the hero whose bar was open (the cursor over another unit does not switch it)
    if (ub < 0 || ub >= static_cast<int>(battle_->units().size()) || battle_->units()[static_cast<size_t>(ub)].side != "player") return void(hotbarEdit_ = false);
    const size_t b = static_cast<size_t>(ub);
    const e2::BattleUnit& u = battle_->units()[b];
    Hotbar& bar = hotbarOf(b);
    const ImGuiIO& io = ImGui::GetIO();
    const float side = 40.0f, gap = 8.0f;
    const float width = kHotbarSlots * side + (kHotbarSlots - 1) * gap + 32.0f;
    ImGui::SetNextWindowPos({io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.45f}, ImGuiCond_Always, {0.5f, 0.5f});
    ImGui::SetNextWindowSize({width, 0.0f});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {16.0f, 14.0f});
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {gap, 8.0f});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(14, 14, 14, 246));
    ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(226, 150, 70, 170));
    ImGui::SetNextWindowFocus();  // over the Escolhas / Magias panels
    ImGui::Begin("##hotbaredit", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const auto heading = [&](const std::string& s, float size, ImU32 ink) {
      const ImVec2 p = ImGui::GetCursorScreenPos();
      dl->AddText(uiFont(UiFont::Ui), size, p, ink, s.c_str());
      ImGui::Dummy({1.0f, size + 2.0f});
    };
    heading(tr("Atalhos de ", "Shortcuts of ") + u.name, 17.0f, IM_COL32(226, 150, 70, 255));
    heading(tr("Arraste uma magia, habilidade ou poção para um socket (F1 a F12). Botão direito esvazia o socket.",
               "Drag a spell, skill or potion onto a socket (F1 to F12). Right click empties a socket."),
            13.0f, IM_COL32(180, 180, 180, 255));
    bool changed = false;
    for (int i = 0; i < kHotbarSlots; ++i) {
      if (i > 0) ImGui::SameLine();
      const std::optional<SocketState> s = socketState(b, bar[static_cast<size_t>(i)], true);
      spellSocket(std::format("##edit{}", i).c_str(), s ? bar[static_cast<size_t>(i)] : std::string(), s ? s->uses : 0, true, false, side, i);
      if (s && ImGui::BeginDragDropSource()) {
        ImGui::SetDragDropPayload("hotbar-slot", &i, sizeof i);
        ImGui::TextUnformatted(socketName(bar[static_cast<size_t>(i)]).c_str());
        ImGui::EndDragDropSource();
      }
      if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("hotbar-choice")) {
          const std::string kind(static_cast<const char*>(p->Data), static_cast<size_t>(p->DataSize));
          for (std::string& other : bar)
            if (other == kind) other.clear();  // one socket per entry
          bar[static_cast<size_t>(i)] = kind, changed = true;
        }
        if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("hotbar-slot")) {
          std::swap(bar[static_cast<size_t>(i)], bar[static_cast<size_t>(*static_cast<const int*>(p->Data))]), changed = true;
        }
        ImGui::EndDragDropTarget();
      }
      if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right) && !bar[static_cast<size_t>(i)].empty()) bar[static_cast<size_t>(i)].clear(), changed = true;
    }
    ImGui::Dummy({1.0f, 4.0f});
    heading(tr("Disponíveis", "Available"), 14.0f, IM_COL32(226, 194, 148, 255));
    const std::vector<std::string> choices = hotbarChoices(b);
    int n = 0;
    for (const std::string& kind : choices) {
      if (n % kHotbarSlots != 0) ImGui::SameLine();
      const std::optional<SocketState> s = socketState(b, kind, true);
      spellSocket(std::format("##choice{}", n).c_str(), kind, s ? s->uses : 0, true, false, side);
      if (ImGui::BeginDragDropSource()) {
        ImGui::SetDragDropPayload("hotbar-choice", kind.data(), kind.size());
        ImGui::TextUnformatted(socketName(kind).c_str());
        ImGui::EndDragDropSource();
      }
      ++n;
    }
    if (choices.empty()) heading(tr("Nada para pôr nos sockets.", "Nothing to put in the sockets."), 13.0f, IM_COL32(150, 150, 150, 255));
    ImGui::Dummy({1.0f, 4.0f});
    const float closeW = spacedTextWidth(uiFont(UiFont::Mark), 16.0f, tr("Fechar", "Close").c_str(), 0.06f) + 28.0f;
    ImGui::SetCursorPosX(width - 16.0f - closeW);
    if (emberButton("##hotbarclose", tr("Fechar", "Close").c_str(), true, true)) hotbarEdit_ = false;
    ImGui::End();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(3);
    if (changed) storeHotbar(b);
  }
  std::string socketName(const std::string& kind) const {
    if (const std::optional<std::string> potion = potionOf(kind)) return e2::potionName(*potion);
    return e2::spellName(kind);
  }

  void drawTurnBar() {
    if (!battle_) return;
    e2::setCombatLogEnglish(uiEnglish());  // the lines made from now on follow the interface language
    drawTurnPlate();
    drawQuestPickups();
    drawHealthBars();
    drawTurnOrder();
    drawHudBar();
    drawConfirm();
    drawHotbarEditor();
    drawFinishPrompt();
    drawChestLoot();
    drawFullLog();
    drawStatusSheet();
    drawBattleInventory();
    drawQuestLog();
  }

  // GameApp's footer: portrait, name · level · XP, class, HP bar, the sheet line or the forecast; then the actions.
  void drawHudBar() {
    const ImGuiIO& io = ImGui::GetIO();
    const float height = 132.0f;
    ImGui::SetNextWindowPos({0.0f, io.DisplaySize.y - height});
    ImGui::SetNextWindowSize({io.DisplaySize.x, height});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {10.0f, 8.0f});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {6.0f, 4.0f});
    ImGui::Begin("##hudbar", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBackground |
                                          ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 a = ImGui::GetWindowPos(), b{a.x + io.DisplaySize.x, a.y + height};
    for (int i = 10; i >= 1; --i) dl->AddRectFilled({a.x, a.y - i * 2.4f}, {b.x, a.y}, IM_COL32(0, 0, 0, 9));  // 0 -10px 24px
    verticalGradient(dl, a, b, {{0.0f, IM_COL32(24, 24, 24, 255)}, {1.0f, IM_COL32(10, 10, 10, 255)}});
    dl->AddLine({a.x, a.y - 1}, {b.x, a.y - 1}, IM_COL32(0, 0, 0, 255));
    dl->AddLine(a, {b.x, a.y}, IM_COL32(190, 150, 95, 115));
    {  // the ember seam along the top, 20% in from each side
      const float l = a.x + io.DisplaySize.x * 0.2f, r = b.x - io.DisplaySize.x * 0.2f, m = (l + r) * 0.5f;
      for (int g = 3; g >= 0; --g) {
        const int al = g == 0 ? 255 : 40;
        dl->AddRectFilledMultiColor({l, a.y - g}, {m, a.y + 1 + g}, IM_COL32(226, 104, 30, 0), IM_COL32(226, 104, 30, al), IM_COL32(226, 104, 30, al), IM_COL32(226, 104, 30, 0));
        dl->AddRectFilledMultiColor({m, a.y - g}, {r, a.y + 1 + g}, IM_COL32(226, 104, 30, al), IM_COL32(226, 104, 30, 0), IM_COL32(226, 104, 30, 0), IM_COL32(226, 104, 30, al));
      }
    }
    ImFont* text = uiFont(UiFont::Ui);
    const int ub = hudUnit();
    if (const int act = battle_->activeUnit(); act != hudActive_) showLog_ = showLog_ && (hudActive_ == -2 || logKeep_), hudActive_ = act;  // a new turn: back to the sheet
    const float rowY = a.y + 8.0f;
    if (ub >= 0) {
      const e2::BattleUnit& u = battle_->units()[static_cast<size_t>(ub)];
      float x = a.x + 10.0f;
      if (const UiImage* face = portraitOf(u)) {
        const ImVec2 p{x, rowY}, q{x + 56.0f, rowY + 80.0f};
        dl->AddRectFilled(p, q, IM_COL32(17, 17, 17, 255), 6.0f);
        imageCover(dl, *face, p, q, 0.5f, 0.3f);
        dl->AddRect(p, q, IM_COL32(74, 74, 74, 255), 6.0f);
        // a click on the portrait opens the status sheet of that unit (any side)
        ImGui::SetCursorScreenPos(p);
        if (ImGui::InvisibleButton("##portraitstatus", {q.x - p.x, q.y - p.y})) openStatus(ub);
        if (ImGui::IsItemHovered()) {
          dl->AddRect(p, q, IM_COL32(226, 104, 30, 255), 6.0f, 2.0f, 0);
          ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
          ImGui::SetTooltip("%s", tr("Ver status", "View status").c_str());
        }
        x = q.x + 10.0f;
      }
      const float right = b.x - 10.0f;
      // The info area is the combat log's switch (GameApp: a click on the stat sheet opens the log, a click on the log closes it).
      ImGui::SetCursorScreenPos({x, rowY});
      if (ImGui::InvisibleButton("##logtoggle", {right - x, 24.0f})) showLog_ = !showLog_, logPinned_ = true, logJustOpened_ = showLog_;
      if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", showLog_ ? "Fechar o log e ver o status" : "Abrir o log de combate");
      ImGui::PushFont(text, 14.0f);
      const std::string name = std::format("{} · Nv {}", u.name, u.level);
      dl->AddText(text, 15.0f, {x, rowY + 2.0f}, IM_COL32(240, 240, 240, 255), name.c_str());
      float nx = x + text->CalcTextSizeA(15.0f, FLT_MAX, 0.0f, name.c_str()).x + 6.0f;
      if (u.side == "player") {
        const std::string xp = u.level >= 30 ? "· NÍVEL MÁX." : std::format("· {}/{} XP", u.xp, xpToLevel(u.level));
        dl->AddText(text, 12.0f, {nx, rowY + 4.5f}, IM_COL32(180, 180, 180, 255), xp.c_str());
      }
      const std::string cls = u.className.empty() ? u.classId : u.className;
      const float cw = text->CalcTextSizeA(12.0f, FLT_MAX, 0.0f, cls.c_str()).x;
      dl->AddText(text, 12.0f, {right - cw, rowY + 4.5f}, u.side == "enemy" ? IM_COL32(181, 74, 50, 255) : IM_COL32(180, 180, 180, 255), cls.c_str());
      if (showLog_) {
        drawHudLog({x, rowY + 26.0f}, {right, rowY + 80.0f});
      } else {
      // HP bar (.hp-fill: accent for the party, danger for foes)
      const std::string hp = std::format("{}/{}", u.hp, u.maxHp);
      const float hw = text->CalcTextSizeA(12.0f, FLT_MAX, 0.0f, hp.c_str()).x;
      const float barY = rowY + 28.0f, barR = right - hw - 8.0f;
      dl->AddRectFilled({x, barY}, {barR, barY + 6.0f}, IM_COL32(74, 74, 74, 255), 3.0f);
      const float f = u.maxHp > 0 ? std::clamp(static_cast<float>(u.hp) / static_cast<float>(u.maxHp), 0.0f, 1.0f) : 0.0f;
      if (f > 0.0f) dl->AddRectFilled({x, barY}, {x + (barR - x) * f, barY + 6.0f}, u.side == "enemy" ? IM_COL32(181, 74, 50, 255) : IM_COL32(216, 216, 216, 255), 3.0f);
      dl->AddText(text, 12.0f, {right - hw, barY - 4.0f}, IM_COL32(240, 240, 240, 255), hp.c_str());
      // the sheet line, or the strike's forecast while a marked foe is under the cursor
      std::string line = sheetLine(u);
      if (selected_ >= 0 && hover_ && !targets_.empty()) {
        const int foe = sceneUnitOf(battle_->unitAt(hover_->x, hover_->y));
        const auto t = std::find_if(targets_.begin(), targets_.end(), [&](const auto& q) { return q.first == foe; });
        if (foe >= 0 && t != targets_.end()) {
          const size_t ab = static_cast<size_t>(unitBattle_[static_cast<size_t>(selected_)]), db = static_cast<size_t>(unitBattle_[static_cast<size_t>(foe)]);
          const e2::Forecast fc = forecastFor(ab, db, t->second);
          line = fc.canCounter ? std::format("{:.0f}% em {} · contra {:.0f}%", fc.hitOut, battle_->units()[db].name, fc.hitBack)
                               : std::format("{:.0f}% em {} · sem contra", fc.hitOut, battle_->units()[db].name);
        }
      }
      dl->AddText(text, 13.0f, {x, barY + 12.0f}, IM_COL32(180, 180, 180, 255), line.c_str());
      if (!tip_.empty() && now_ - tipAt_ < 3.0f) dl->AddText(text, 12.0f, {x, barY + 30.0f}, IM_COL32(224, 189, 128, 255), tip_.c_str());
      else if (const std::string cond = e2::conditionsText(u); !cond.empty())  // what ails it (our spells' conditions)
        dl->AddText(text, 12.0f, {x, barY + 30.0f}, IM_COL32(196, 150, 232, 255), cond.c_str());
      }
      ImGui::PopFont();
    } else {
      dl->AddText(text, 12.0f, {a.x + 10.0f, rowY + 4.0f}, IM_COL32(180, 180, 180, 255),
                  battle_->activeUnit() >= 0 && battle_->units()[static_cast<size_t>(battle_->activeUnit())].side == "enemy"
                      ? "O inimigo age…"
                      : "Clique num aliado ou num inimigo para ver o status.");
    }
    // the actions row; free roam (Engine2's explore missions) shows how to play it instead
    if (battle_->roaming()) drawExploreRow(a, b);
    else drawActionsRow(a, b);
    const bool mine = myTurn() && !busy();
    const bool turns = !battle_->roaming();  // free roam has no turns to end
    const float endW = spacedTextWidth(uiFont(UiFont::Mark), 16.0f, "Fim do turno", 0.06f) + 28.0f;
    {  // the combat log: its switch, and (while it shows) the whole log full screen
      const std::string openLabel = tr("Abrir log", "Open log");
      const char* logLabel = showLog_ ? "Resumo" : "Log";
      const float openW = spacedTextWidth(uiFont(UiFont::Mark), 16.0f, openLabel.c_str(), 0.06f) + 28.0f;
      const float logW = spacedTextWidth(uiFont(UiFont::Mark), 16.0f, logLabel, 0.06f) + 28.0f;
      // (no "Status" here: the portrait opens the sheet; the user, 2026-10-18)
      const float endX = io.DisplaySize.x - endW - 20.0f, openX = endX - 6.0f - openW, logX = (showLog_ ? openX : endX) - 6.0f - logW;
      // the quest log (Engine2's world-map "Missões", here in the fight too)
      const std::string questLabel = tr("Missões", "Quests");
      const float questW = options_.world ? spacedTextWidth(uiFont(UiFont::Mark), 16.0f, questLabel.c_str(), 0.06f) + 28.0f : 0.0f;
      const float questX = questW > 0.0f ? logX - 6.0f - questW : logX;
      hudRightEdge_ = questX;  // the actions row's sockets stop short of it (next frame)
      // Engine2's footer: "Encerrar missão" / "Usar waypoint" once the end is on offer
      if (battle_->winAvailable() && result_.empty()) {
        const std::string finishLabel = battle_->activeExit() ? tr("Usar waypoint", "Use waypoint") : tr("Encerrar missão", "End mission");
        const float finishW = spacedTextWidth(uiFont(UiFont::Mark), 16.0f, finishLabel.c_str(), 0.06f) + 28.0f;
        hudRightEdge_ = questX - 6.0f - finishW;
        ImGui::SameLine(questX - 6.0f - finishW);
        if (emberButton("##finishbtn", finishLabel.c_str(), true, !busy())) {
          if (battle_->activeExit()) winDismissed_ = false;
          else confirmFinishNow();
        }
      }
      if (questW > 0.0f) {
        ImGui::SameLine(questX);
        if (emberButton("##questbtn", questLabel.c_str(), false, true)) questLog_ = true;
      }
      ImGui::SameLine(logX);
      if (emberButton("##logbtn", logLabel, false, true)) showLog_ = !showLog_, logPinned_ = true, logJustOpened_ = showLog_;
      if (showLog_) {
        ImGui::SameLine(openX);
        if (emberButton("##openlog", openLabel.c_str(), false, true)) logFull_ = true, logFullJustOpened_ = true;
      }
    }
    ImGui::SameLine(io.DisplaySize.x - endW - 20.0f);
    if (!turns) {  // free roam: an explore mission's "Sair" (Engine2 onQuit) in place of Fim do turno; nothing on a fight's map
      if (explores_ && emberButton("##leave", tr("Sair", "Leave").c_str(), true, !busy())) titleRequested_ = true;
    } else if (emberButton("##endturn", "Fim do turno", false, mine && turns)) {
      endUnitTurn();
    }
    ImGui::End();
    ImGui::PopStyleVar(3);
  }

  // The battle bar's actions: Atacar, the off-hand, Arrombar, Esperar, Cancelar, Fugir combate, the spells and potions.
  void drawActionsRow(ImVec2 a, ImVec2 b) {
    // the actions row
    ImGui::SetCursorScreenPos({a.x + 10.0f, b.y - 32.0f - 10.0f});
    const bool mine = myTurn() && !busy();
    // The actor's battle index, read once: Esperar and a failed Fugir combate let go of the unit (select(-1)) mid-row, and the
    // rest of the row is still drawn this frame (reading unitBattle_[selected_] after that indexed -1: the crash of 2026-10-17).
    const size_t me = mine ? static_cast<size_t>(unitBattle_[static_cast<size_t>(selected_)]) : 0;
    const e2::BattleUnit* actor = mine ? &battle_->units()[me] : nullptr;
    const bool canAct = actor && !actor->acted;
    // Atacar always opens (the grid shows the weapon's zone even with nobody in it), so the player sees its reach.
    if (emberButton("##attack", "Atacar", true, canAct && spellMode_.empty())) attackMode_ = !attackMode_, offHandMode_ = false, refreshReach();
    if (actor && (actor->combat.offHandWeapon || actor->combat.offHandShield)) {  // Engine2's "Mão Secundária": the dagger or katar strikes, or the shield bashes
      ImGui::SameLine();
      if (emberButton("##offhand", tr("Mão Secundária", "Off-hand").c_str(), offHandMode_, canAct && spellMode_.empty())) {
        offHandMode_ = !offHandMode_;
        attackMode_ = false;
        refreshReach();
      }
    }
    if (actor && canAct && actor->lockpick > 0 && battle_->adjacentLock(me) >= 0) {
      ImGui::SameLine();  // Engine2's "Arrombar": a Gazua opens the chest or door beside the hero
      if (emberButton("##lockpick", tr("Arrombar", "Pick lock").c_str(), false, spellMode_.empty(), 32.0f, "lockpick.png")) openLock();
    }
    ImGui::SameLine();
    const bool turns = !battle_->roaming();  // free roam has no turns to end
    const auto scriptPressed = [this](const char* id) { return scriptPress_ == id ? (scriptPress_.clear(), true) : false; };
    if (emberButton("##wait", "Esperar", false, mine && turns) || (mine && turns && scriptPressed("wait"))) endUnitTurn();
    ImGui::SameLine();
    // Cancelar: leaves the attack / spell aim; else takes back the last walk (one per click, each giving back only
    // its own movement). A walk made before an action cannot be taken back once the unit has acted.
    const bool aiming = !spellMode_.empty() || attackMode_ || offHandMode_ || pendingFoe_ >= 0;
    const bool canUndo = actor && !undo_.empty() && undo_.back().unit == static_cast<size_t>(selected_) && undo_.back().acted == actor->acted;
    if (emberButton("##cancel", "Cancelar", false, mine && (aiming || canUndo))) {
      if (aiming) {
        spellMode_.clear(), spellCells_.clear(), spellAim_.clear();
        spellArmed_.reset(), pendingFoe_ = -1;
        attackMode_ = false;
        offHandMode_ = false;
        refreshReach();
      } else {
        undoMove();
      }
    }
    // Fugir combate (Engine2 GameApp "fleeable" + engine canAttemptFlee / attemptFlee): a random encounter only, by the hero whose turn it
    // is once any hex of its body is on the battlefield's edge; the roll is 60 + DEX / 3 % (DEX with the affinity bonus).
    if (actor && canFlee(*actor)) {
      ImGui::SameLine();
      const std::string label = tr("Fugir combate · ", "Flee battle · ") + std::format("{:.0f}%", fleeChance(*actor));
      if (emberButton("##flee", label.c_str(), false, canAct && !aiming) || (canAct && !aiming && scriptPressed("flee"))) attemptFlee();
    }
    if (actor) {  // a party unit (myTurn)
      // The 12 sockets (F1..F12) and "Atalhos"; they shrink when the row is crowded (Mão Secundária, Arrombar, Fugir).
      const float spacing = ImGui::GetStyle().ItemSpacing.x;
      const float editW = spacedTextWidth(uiFont(UiFont::Mark), 16.0f, tr("Atalhos", "Shortcuts").c_str(), 0.06f) + 28.0f;
      const float from = ImGui::GetItemRectMax().x + spacing, room = hudRightEdge_ - 12.0f - from - editW - spacing;
      const float side = std::clamp(std::floor(room / kHotbarSlots - spacing), 24.0f, 36.0f);
      const Hotbar& bar = hotbarOf(me);
      for (int i = 0; i < kHotbarSlots; ++i) {
        ImGui::SameLine();
        const std::string& kind = bar[static_cast<size_t>(i)];
        const std::optional<SocketState> s = socketState(me, kind, canAct);
        if (spellSocket(std::format("##slot{}", i).c_str(), s ? kind : std::string(), s ? s->uses : 0, s && s->usable, s && spellMode_ == kind, side, i))
          startSpell(kind);
      }
      ImGui::SameLine();
      if (emberButton("##hotbaredit", tr("Atalhos", "Shortcuts").c_str(), hotbarEdit_, true)) hotbarEdit_ = !hotbarEdit_, hotbarHero_ = static_cast<int>(me);
    }
  }

  // Free roam (GameApp's explore row): how to play it; a Gazua still opens a chest or door beside the leader (Arrombar).
  void drawExploreRow(ImVec2 a, ImVec2 b) {
    ImGui::SetCursorScreenPos({a.x + 10.0f, b.y - 32.0f - 10.0f});
    if (myTurn() && !busy()) {
      const size_t me = static_cast<size_t>(unitBattle_[static_cast<size_t>(selected_)]);
      if (battle_->units()[me].lockpick > 0 && battle_->adjacentLock(me) >= 0) {
        if (emberButton("##lockpick", tr("Arrombar", "Pick lock").c_str(), false, true, 32.0f, "lockpick.png")) openLock();
        ImGui::SameLine();
      }
    }
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const std::string hint = tr("Clique no chão para andar · clique em alguém para conversar.", "Click the ground to walk · click someone to talk.");
    ImGui::GetWindowDrawList()->AddText(uiFont(UiFont::Ui), 13.0f, {p.x, p.y + 9.0f}, IM_COL32(180, 180, 180, 255), hint.c_str());
    ImGui::Dummy({uiFont(UiFont::Ui)->CalcTextSizeA(13.0f, FLT_MAX, 0, hint.c_str()).x, 32.0f});
  }

  static int xpToLevel(int level) {
    const int c = std::max(1, std::min(29, level));
    return c >= 28 ? 600 : c >= 23 ? 500 : c >= 18 ? 400 : c >= 13 ? 300 : c >= 8 ? 225 : c >= 3 ? 150 : 100;
  }

  // Cancelar: the last walk taken back: its hex, heading, the movement it spent and where it stood on screen.
  void undoMove() {
    if (undo_.empty() || selected_ < 0) return;
    const BeforeWalk t = undo_.back();
    undo_.pop_back();
    e2::BattleUnit& u = battle_->units()[static_cast<size_t>(unitBattle_[t.unit])];
    u.x = t.x, u.y = t.y, u.moveUsed = t.moveUsed, u.faceDx = t.faceDx, u.faceDy = t.faceDy;
    unitWalk_[t.unit] = Walk{};
    unitPos_[t.unit] = t.pos, unitFacing_[t.unit] = t.facing, unitMoved_[t.unit] = t.leftSpawn;
    refreshReach();
  }

  // --- Fugir combate ---
  bool fleeable() const {
    return missionId_.rfind("random", 0) == 0 || mapKey_.rfind("random", 0) == 0 || std::filesystem::path(options_.map).stem().string().rfind("random", 0) == 0;
  }
  bool canFlee(const e2::BattleUnit& u) const {
    if (!fleeable() || !battle_ || !result_.empty() || u.side != "player" || !u.alive || u.moved || u.summoned) return false;
    for (const e2::BattleHex& c : e2::Battle::footprint(u, u.x, u.y))
      if (c.x <= 0 || c.y <= 0 || c.x >= battle_->cols() - 1 || c.y >= battle_->rows() - 1) return true;
    return false;
  }
  double fleeChance(const e2::BattleUnit& u) const {
    const size_t b = static_cast<size_t>(&u - battle_->units().data());
    return std::min(100.0, 60.0 + e2::fighterAt(*battle_, b, u.x, u.y).dex / e2::kDexDivisor);  // dexterity.ts dexEscapeChance
  }
  // Rolls the escape for the picked hero: a hit takes the party back where the fight came from (no victory, nothing completed); a
  // miss spends the hero's turn and the foes carry on.
  bool attemptFlee() {
    if (selected_ < 0 || !battle_) return false;
    e2::BattleUnit& u = battle_->units()[static_cast<size_t>(unitBattle_[static_cast<size_t>(selected_)])];
    if (!canFlee(u)) return false;
    std::random_device device;
    e2::Mulberry32 rng(device());
    const bool out = !std::exchange(scriptFleeMiss_, false) && rng() * 100.0 < fleeChance(u);
    const std::string line = out ? tr(u.name + " encontrou uma saída! O grupo foge do combate.", u.name + " found a way out! The party flees the battle.")
                                 : tr(u.name + " não conseguiu fugir — o combate continua.", u.name + " failed to flee: the battle goes on.");
    battle_->pushLog(line);
    tip_ = line, tipAt_ = now_;
    if (out) {
      titleRequested_ = true;
      return true;
    }
    u.moved = true;
    select(-1);
    return false;
  }

  void endUnitTurn() {
    if (selected_ < 0) return;
    battle_->units()[static_cast<size_t>(unitBattle_[static_cast<size_t>(selected_)])].moved = true;
    select(-1);
  }

  // battleHud3d.ts healthBars: a bar and the HP over each unit's head, sized by its hex on screen.
  void drawHealthBars() {
    const ImGuiIO& io = ImGui::GetIO();
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    const float w = io.DisplaySize.x, h = io.DisplaySize.y;
    for (size_t i = 0; i < scene_->units.size(); ++i) {
      const int b = unitBattle_[i];
      if (b < 0 || !unitCard_[i].drawn) continue;
      const e2::BattleUnit& u = battle_->units()[static_cast<size_t>(b)];
      if (!u.alive) continue;
      const CardRect& c = unitCard_[i];
      const glm::vec3 up = cardUp(c);
      const glm::vec3 head = c.origin + up * c.height;
      const std::optional<ImVec2> at = screenOf(head, w, h);
      const std::optional<ImVec2> side = screenOf(head + glm::vec3(1.0f, 0.0f, 0.0f), w, h);
      const std::optional<ImVec2> deep = screenOf(head + glm::vec3(0.0f, 0.0f, 1.0f), w, h);
      if (!at || !side || !deep) continue;
      // Near the camera the bar and the number would block the view (the user, 2026-10-16): a unit whose card fills
      // more than kBarsFadeFrom of the screen's height fades them, gone at kBarsFadeTo; back as it moves away.
      float near = 1.0f;
      if (barsFade_ == 1) {
        const std::optional<ImVec2> feet = screenOf(c.origin, w, h);
        if (feet) {
          const float share = std::abs(feet->y - at->y) / std::max(h, 1.0f);
          near = 1.0f - glm::smoothstep(kBarsFadeFrom, kBarsFadeTo, share);
        }
      }
      if (near <= 0.01f) continue;
      auto faded = [near](ImU32 color) {
        const ImU32 a = static_cast<ImU32>(std::round(static_cast<float>(color >> IM_COL32_A_SHIFT) * near));
        return (color & ~IM_COL32_A_MASK) | (a << IM_COL32_A_SHIFT);
      };
      const float px = std::max(std::hypot(side->x - at->x, side->y - at->y), std::hypot(deep->x - at->x, deep->y - at->y));
      const float cell = px * 1.7320508f;
      const float gap = cell * 0.12f;
      const float width = cell * (u.size >= 4 ? 1.35f : u.size == 2 ? 0.9f : 0.62f);
      const float height = std::max(3.0f, cell * 0.07f);
      const float left = at->x - width * 0.5f, top = at->y - gap - height;
      const int alpha = u.moved && u.side == "player" ? 230 : 255;
      dl->AddRectFilled({left - 1, top - 1}, {left + width + 1, top + height + 1}, faded(IM_COL32(12, 11, 10, 209 * alpha / 255)));
      dl->AddRectFilled({left, top}, {left + width, top + height}, faded(IM_COL32(44, 40, 36, alpha)));
      const ImU32 fill = u.side == "player" ? IM_COL32(200, 196, 188, alpha) : u.side == "neutral" ? IM_COL32(95, 158, 82, alpha) : IM_COL32(181, 74, 50, alpha);
      const float f = u.maxHp > 0 ? std::clamp(static_cast<float>(u.hp) / static_cast<float>(u.maxHp), 0.0f, 1.0f) : 0.0f;
      if (f > 0.0f) dl->AddRectFilled({left, top}, {left + width * f, top + height}, faded(fill));
      if (cell >= 32.0f) {
        ImFont* font = uiFont(UiFont::Ui);
        // Fixed steps, all baked the first time (the hunt for the battle hitch, 2026-10-19: a size that followed the zoom
        // made ImGui bake new glyphs and re-upload its font atlas as the camera came near, 21-50 ms each time).
        static constexpr float kSteps[] = {13.0f, 15.0f, 18.0f, 22.0f, 27.0f, 33.0f, 40.0f, 48.0f};
        if (!hpSizesBaked_) {
          for (const float s : kSteps) font->CalcTextSizeA(s, FLT_MAX, 0.0f, "0123456789");
          hpSizesBaked_ = true;
        }
        float size = kSteps[0];
        for (const float s : kSteps)
          if (s <= cell * 0.3f) size = s;
        const std::string hp = std::to_string(u.hp);
        const ImVec2 t = font->CalcTextSizeA(size, FLT_MAX, 0.0f, hp.c_str());
        const ImVec2 p{at->x - t.x * 0.5f, top - 1.0f - t.y};
        for (int dx = -1; dx <= 1; ++dx)
          for (int dy = -1; dy <= 1; ++dy)
            if (dx || dy) dl->AddText(font, size, {p.x + dx, p.y + dy}, faded(IM_COL32(12, 11, 10, 230)), hp.c_str());
        dl->AddText(font, size, p, faded(IM_COL32(240, 235, 227, 255)), hp.c_str());
      }
    }
  }

  // GameApp's "Ordem": a small button top left that unfolds this round's order.
  void drawTurnOrder() {
    ImGui::SetNextWindowPos({8.0f, 8.0f});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0.0f, 0.0f});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {2.0f, 4.0f});
    ImGui::Begin("##order", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove |
                                         ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoSavedSettings);
    ImFont* font = uiFont(UiFont::Ui);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    auto chip = [&](const char* id, const std::string& label, ImU32 ink, ImU32 bg, bool button) {
      const ImVec2 t = font->CalcTextSizeA(13.0f, FLT_MAX, 0.0f, label.c_str());
      const ImVec2 size{t.x + 14.0f, t.y + 9.0f};
      bool clicked = false;
      if (button) clicked = ImGui::InvisibleButton(id, size);
      else ImGui::Dummy(size);
      const ImVec2 a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax();
      dl->AddRectFilled(a, b, bg, 6.0f);
      dl->AddRect(a, b, IM_COL32(255, 255, 255, 38), 6.0f);
      dl->AddText(font, 13.0f, {a.x + 7.0f, a.y + 4.5f}, ink, label.c_str());
      return clicked;
    };
    if (chip("##ordem", "Ordem", IM_COL32(203, 213, 225, 255), IM_COL32(17, 27, 34, 242), true)) showTurnOrder_ = !showTurnOrder_;
    if (showTurnOrder_) {
      const int active = battle_->activeUnit();
      int n = 1;
      bool first = true;
      for (size_t b : battle_->turnOrder()) {
        const e2::BattleUnit& u = battle_->units()[b];
        if (!u.alive) continue;
        if (!first) ImGui::SameLine();
        first = false;
        const bool now = static_cast<int>(b) == active;
        const ImU32 ink = now ? IM_COL32(255, 224, 180, 255) : u.moved ? IM_COL32(100, 116, 139, 255) : u.side == "enemy" ? IM_COL32(231, 133, 115, 255) : IM_COL32(203, 213, 225, 255);
        chip(std::format("##q{}", b).c_str(), std::format("{} {}", n++, u.name), ink, now ? IM_COL32(37, 49, 59, 242) : IM_COL32(17, 27, 34, 217), false);
      }
    }
    ImGui::End();
    ImGui::PopStyleVar(3);
  }

  // GameApp BattleScreen's Esc menu (EmberMenus.h BattleMenu) and the Opções window it opens. GameApp's Regras button
  // (Como jogar over the battle) is left out (user, 2026-10-16).
  void drawPauseMenu() {
    const ImVec2 screen = ImGui::GetIO().DisplaySize;
    if (battleSlots_ && campaign_) {  // GameApp's saveSlots screen over the battle (slotReturnScreen "battle")
      ImGui::SetNextWindowPos({0, 0});
      ImGui::SetNextWindowSize(screen);
      ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
      ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
      ImGui::Begin("##battleslots", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBackground |
                                                 ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollWithMouse);
      bool back = false;
      const int chosen = battleSlots_->draw(screen, 16.0f, *campaign_, options_.engine2Public, back, &slotSave_);
      ImGui::End();
      ImGui::PopStyleVar(2);
      if (back) battleSlots_.reset();
      else if (chosen >= 0) {
        if (battleSlots_->mode() == SlotScreen::Mode::Load) loadRequested_ = true;
        else pauseOpen_ = false;  // saved: back to the fight
        battleSlots_.reset();
      }
      return;
    }
    if (pauseOpen_ && result_.empty() && !menuOptions_.isOpen()) {
      switch (battleMenu_.draw(screen, options_.engine2Public, speed_, playtest_, campaign_ && !testMode_ && !missionId_.empty(), campaign_ != nullptr)) {
        case menu::BattleMenu::Action::Save:
          // The campaign as it stands with this fight in it (GameApp's save-slot write from the battle).
          slotSave_ = campaign_->activeSave();
          slotSave_["pendingMission"] = missionId_;
          slotSave_["battle"] = captureSnapshot();
          for (const HeroOutcome& h : heroOutcomes()) {
            slotSave_["levels"][h.name] = h.level;
            slotSave_["xp"][h.name] = h.xp;
            slotSave_["unitHp"][h.name] = h.hp;
          }
          battleSlots_.emplace();
          battleSlots_->open(SlotScreen::Mode::Save, true);
          break;
        case menu::BattleMenu::Action::Load:
          battleSlots_.emplace();
          battleSlots_->open(SlotScreen::Mode::Load, true);
          break;
        case menu::BattleMenu::Action::Resume: pauseOpen_ = false; break;
        case menu::BattleMenu::Action::Options: menuOptions_.open(); break;
        case menu::BattleMenu::Action::Quit: titleRequested_ = true; break;
        case menu::BattleMenu::Action::Title: homeRequested_ = true; break;
        case menu::BattleMenu::Action::None: break;
      }
    }
    menu::OptionsDialog::Actions actions;
    actions.fullscreen = fullscreen_;
    actions.gfx = gfx_;
    actions.gfxChanged = [this] {
      if (gfx_) applyGfx(*gfx_);
    };
    menuOptions_.draw(screen, actions);
  }

  // GameApp ResultScreen: the mission's art, "Vitória · T3" / "Derrota", its title and line, each hero's growth (level,
  // XP bar, HP after the fight), then the way on.
  // GameApp ResultScreen's body on a victory, by the waypoint the mission was ended on.
  std::string resultLine() const {
    if (finishExit_ && board_->draft().id == kWispCrossing && finishExit_->targetMapId == kWispBoss)
      return tr("A trilha leva ao covil da Planta Carnívora. Derrotem-na para concluir a travessia.",
                "The trail leads to the Carnivorous Plant's lair. Defeat it to finish the crossing.");
    if (finishExit_ && finishExit_->kind == "dungeon") return tr("Vocês encontraram a saída da masmorra.", "You found the dungeon exit.");
    if (finishExit_ && finishExit_->id == "escape-exit") return tr("Vocês escaparam a tempo.", "You got away in time.");
    if (floorTaken()) {
      if (board_->draft().id == "estalagem") return tr("Subir para o Segundo andar", "Up to the Second floor");
      if (board_->draft().id == "estalagem-andar-2") return tr("Descer para o Primeiro andar", "Down to the First floor");
      return connectorUp(*finishExit_) ? tr("Subindo para o andar superior.", "Going up to the floor above.")
                                       : tr("Descendo para o andar inferior.", "Going down to the floor below.");
    }
    return tr("O campo ficou em silêncio.", "The field fell silent.");
  }
  // ... and its "advance" button (floorConnectorDirection always names a way: Subir / Descer).
  std::string advanceLabel() const {
    if (board_->draft().id == kWispCrossing && finishExit_ && finishExit_->targetMapId == kWispBoss) return tr("ENFRENTAR A PLANTA", "FACE THE PLANT");
    return connectorUp(*finishExit_) ? tr("SUBIR", "GO UP") : tr("DESCER", "GO DOWN");
  }
  void drawResultScreen() {
    const ImGuiIO& io = ImGui::GetIO();
    const ImVec2 size = io.DisplaySize;
    const float rem = 16.0f * std::clamp(size.y / 900.0f, 0.85f, 2.0f);
    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize(size);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::Begin("##result", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoSavedSettings);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const bool win = result_ == "victory";
    const float k = std::clamp((now_ - resultAt_ - kResultDelay) / 0.5f, 0.0f, 1.0f);  // fades in
    dl->AddRectFilled({0, 0}, size, IM_COL32(17, 17, 17, static_cast<int>(255 * k)));
    if (const std::filesystem::path art = briefArtFile(board_->draft().id, options_.engine2Public); !art.empty())
      if (const UiImage* image = uiImage(art)) {
        imageCover(dl, *image, {0, 0}, size, 0.5f, 0.5f, IM_COL32(255, 255, 255, static_cast<int>(255 * k)));
        verticalGradient(dl, {0, 0}, size, {{0.0f, IM_COL32(17, 17, 17, 51)}, {0.5f, IM_COL32(17, 17, 17, 102)}, {1.0f, IM_COL32(17, 17, 17, 230)}});
      }
    const float x = rem, z = 0.75f;  // the screen's own zoom: 0.75
    float y = 2.0f * rem;
    const std::string kicker = win ? std::format("VITÓRIA · T{}", battle_->round()) : std::format("DERROTA · T{}", battle_->round());
    emberKicker(dl, {x, y}, 0.875f * rem, kicker.c_str(), !win);
    y += 0.875f * rem * 1.4f + 0.5f * rem;
    const std::string title = board_->draft().title.empty() ? board_->draft().id : board_->draft().title;
    emberTitle(dl, {x, y}, 3.0f * rem, title.c_str());
    y += 3.0f * rem * 1.1f + 0.5f * rem;
    ImFont* ui = uiFont(UiFont::Ui);
    dl->AddText(ui, 1.125f * rem, {x, y}, IM_COL32(180, 180, 180, 255), win ? resultLine().c_str() : tr("A linha quebrou.", "The line broke.").c_str());
    y += 1.125f * rem * 1.5f + 1.5f * rem * z;
    // growth: one trough per hero
    const float rowW = std::min(34.0f * rem, size.x - 2.0f * rem);
    for (size_t i = 0; i < battle_->units().size(); ++i) {
      const e2::BattleUnit& u = battle_->units()[i];
      if (u.side != "player" || u.summoned || i >= startLevel_.size() || absent_.contains(u.id)) continue;
      const auto [fromLevel, fromXp] = startLevel_[i];
      const float rowH = 4.6f * rem;
      const ImVec2 p{x, y}, q{x + rowW, y + rowH};
      emberSlot(dl, p, q);
      float ty = p.y + 0.6f * rem;
      std::string head = u.level != fromLevel ? std::format("{} · Nv {} → {}", u.name, fromLevel, u.level) : std::format("{} · Nv {}", u.name, u.level);
      if (!u.alive) head += " · caiu";
      dl->AddText(ui, 1.125f * rem, {p.x + 0.75f * rem, ty}, IM_COL32(240, 240, 240, 255), head.c_str());
      ty += 1.125f * rem * 1.35f;
      // the XP bar: what it had (dim) and what it earned (bright)
      const float barW = 12.0f * rem, barH = 0.4f * rem;
      const int next = xpToLevel(u.level);
      dl->AddRectFilled({p.x + 0.75f * rem, ty + 0.3f * rem}, {p.x + 0.75f * rem + barW, ty + 0.3f * rem + barH}, IM_COL32(74, 74, 74, 255), 2.0f);
      const float had = u.level == fromLevel ? std::clamp(static_cast<float>(fromXp) / next, 0.0f, 1.0f) : 0.0f;
      const float now = std::clamp(static_cast<float>(u.xp) / next, 0.0f, 1.0f);
      if (now > 0.0f)
        dl->AddRectFilled({p.x + 0.75f * rem, ty + 0.3f * rem}, {p.x + 0.75f * rem + barW * now, ty + 0.3f * rem + barH}, IM_COL32(226, 140, 60, 255), 2.0f);
      if (had > 0.0f)
        dl->AddRectFilled({p.x + 0.75f * rem, ty + 0.3f * rem}, {p.x + 0.75f * rem + barW * had, ty + 0.3f * rem + barH}, IM_COL32(216, 216, 216, 255), 2.0f);
      const std::string xp = std::format("{}/{} XP{}", u.xp, next, u.level != fromLevel ? " · subiu" : "");
      dl->AddText(ui, 0.875f * rem, {p.x + 0.75f * rem + barW + 0.5f * rem, ty}, IM_COL32(180, 180, 180, 255), xp.c_str());
      ty += 0.875f * rem * 1.45f;
      // GameApp ResultScreen's HP lines: the fight's HP, then what the camp gives (resting) or what goes on to the next floor
      std::string hpLine = std::format("{} {}/{}", tr("Combate:", "Battle:"), u.alive ? u.hp : 0, maxHpAt(u, fromLevel));
      if (win) {
        const int camp = campHp(u), max = maxHpAt(u, u.level);
        if (floorTaken())
          hpLine += !u.alive ? tr(" · Fora do próximo andar", " · Out of the next floor")
                             : std::format(" · {} {} HP · {}", tr("Próximo andar:", "Next floor:"), camp, tr("sem descanso", "no rest"));
        else if (!u.alive)
          hpLine += std::format(" · {} {} HP ({} {})", tr("Descanso: revive com", "Rest: back with"), camp, tr("metade de", "half of"), max);
        else
          hpLine += (camp > u.hp ? std::format(" · {} +{} HP", tr("Descanso:", "Rest:"), camp - u.hp) : tr(" · Descanso: sem feridas", " · Rest: no wounds")) +
                    std::format(" · {} {}/{}", tr("Acampamento:", "Camp:"), camp, max);
      }
      dl->AddText(ui, 0.875f * rem, {p.x + 0.75f * rem, ty}, IM_COL32(180, 180, 180, 255), hpLine.c_str());
      y += rowH + 0.5f * rem;
    }
    // the way on
    const float bw = std::min(16.0f * rem, (size.x - 3.0f * rem) * 0.5f), bh = 3.0f * rem;
    const float by = size.y - rem - bh;
    if (!win) {
      if (emberBigButton("##retry", "TENTAR DE NOVO", {x, by}, {bw, bh}, true, rem)) retryRequested_ = true;
      if (emberBigButton("##map", "MAPA", {x + bw + rem, by}, {bw, bh}, false, rem)) titleRequested_ = true;
    } else if (const std::string floor = floorTarget(); !floor.empty()) {
      // GameApp onAdvance: a floor connector goes straight into the linked floor (never the world map)
      if (emberBigButton("##advance", advanceLabel().c_str(), {x, by}, {bw, bh}, true, rem)) floorRequest_ = floor;
      if (emberBigButton("##map", "MAPA", {x + bw + rem, by}, {bw, bh}, false, rem)) titleRequested_ = true;
    } else if (emberBigButton("##map", "MAPA", {x, by}, {bw, bh}, true, rem)) {
      titleRequested_ = true;
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
  }

  // Free roam's box (top centre, in place of the turn plate): the party is in a free-movement area, how to move in it.
  void drawFreeMovePlate() {
    const ImGuiIO& io = ImGui::GetIO();
    const std::string title = tr("Movimento livre", "Free movement");
    std::vector<std::string> lines;
    if (innFloor()) {
      lines.push_back(tr("Clique no chão para andar · clique em alguém para conversar.", "Click the ground to walk · click someone to talk."));
    } else {
      lines.push_back(tr("Clique no chão e o grupo segue o líder até lá.", "Click the ground and the party follows the leader there."));
      lines.push_back(tr("Um inimigo à vista começa o combate por turnos.", "An enemy in sight starts turn-based combat."));
    }
    ImFont* ui = uiFont(UiFont::Ui);
    constexpr float kTitle = 18.0f, kLine = 13.0f;
    ImGui::PushFont(nullptr, kTitle);
    const ImVec2 t = ImGui::CalcTextSize(title.c_str());
    ImGui::PopFont();
    float width = t.x;
    for (const std::string& l : lines) width = std::max(width, ui->CalcTextSizeA(kLine, FLT_MAX, 0, l.c_str()).x);
    const float height = t.y + 6.0f + static_cast<float>(lines.size()) * (kLine + 4.0f);
    ImGui::SetNextWindowPos({io.DisplaySize.x * 0.5f, kBarInset}, ImGuiCond_Always, {0.5f, 0.0f});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {kPlateGlow, kPlateGlow});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::Begin("##freemove", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove |
                                            ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoFocusOnAppearing |
                                            ImGuiWindowFlags_NoSavedSettings);
    plate("##freemovebox", {width + 32.0f, height + 16.0f}, [&](ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32) {
      float y = a.y + 8.0f;
      ImGui::PushFont(nullptr, kTitle);
      dl->AddText({std::round((a.x + b.x - t.x) * 0.5f), std::round(y)}, IM_COL32(255, 214, 160, 255), title.c_str());
      ImGui::PopFont();
      y += t.y + 6.0f;
      for (const std::string& l : lines) {
        const float lw = ui->CalcTextSizeA(kLine, FLT_MAX, 0, l.c_str()).x;
        dl->AddText(ui, kLine, {std::round((a.x + b.x - lw) * 0.5f), std::round(y)}, IM_COL32(214, 214, 214, 255), l.c_str());
        y += kLine + 4.0f;
      }
    });
    ImGui::End();
    ImGui::PopStyleVar(2);
  }

  // The moment free roam turns into a fight: everything holds for kSightPauseSeconds under a big plate mid-screen, then the
  // turns begin (the user, 2026-10-18: "dar uma pausa no movimento e dizer Inimigos avistados"); going back to free roam
  // says nothing.
  static constexpr float kModeBannerSeconds = 2.0f, kSightPauseSeconds = 1.8f;
  bool sightPause() const { return now_ - modeSwitchAt_ >= 0.0f && now_ - modeSwitchAt_ < kSightPauseSeconds; }
  void drawModeBanner() {
    const float age = now_ - modeSwitchAt_;
    if (age < 0.0f || age > kModeBannerSeconds || !result_.empty()) return;
    const ImGuiIO& io = ImGui::GetIO();
    const std::string text = tr("Inimigos avistados!", "Enemies sighted!");
    ImGui::SetNextWindowPos({io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.3f}, ImGuiCond_Always, {0.5f, 0.5f});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {kPlateGlow, kPlateGlow});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::Begin("##modebanner", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove |
                                              ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoFocusOnAppearing |
                                              ImGuiWindowFlags_NoSavedSettings);
    ImGui::PushFont(nullptr, 30.0f);
    const ImVec2 t = ImGui::CalcTextSize(text.c_str());
    const ImU32 ink = IM_COL32(255, 150, 120, 255);
    plate("##modebannerbox", {t.x + 56.0f, t.y + 24.0f}, [&](ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32) {
      dl->AddText({std::round((a.x + b.x - t.x) * 0.5f), std::round((a.y + b.y - t.y) * 0.5f)}, ink, text.c_str());
    });
    ImGui::PopFont();
    ImGui::End();
    ImGui::PopStyleVar(2);
  }

  void drawTurnPlate() {
    const ImGuiIO& io = ImGui::GetIO();
    drawModeBanner();
    if (result_.empty() && battle_->roaming()) return drawFreeMovePlate();
    std::string text;
    if (result_ == "victory") text = tr("Vitória", "Victory");
    else if (result_ == "defeat") text = tr("Derrota", "Defeat");
    else if (const int a = battle_->activeUnit(); a >= 0)
      text = std::format("{} {}  ·  {}", tr("Rodada", "Round"), battle_->round(),
                         std::vformat(tr("vez de {}", "{}'s turn"), std::make_format_args(battle_->units()[static_cast<size_t>(a)].name)));
    if (text.empty()) return;
    const bool end = !result_.empty();
    ImGui::SetNextWindowPos({io.DisplaySize.x * 0.5f, end ? io.DisplaySize.y * 0.4f : kBarInset}, ImGuiCond_Always, {0.5f, end ? 0.5f : 0.0f});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {kPlateGlow, kPlateGlow});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::Begin("##turnplate", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove |
                                             ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoFocusOnAppearing |
                                             ImGuiWindowFlags_NoSavedSettings);
    const float size = end ? 40.0f : 18.0f;
    ImGui::PushFont(nullptr, size);
    const ImVec2 t = ImGui::CalcTextSize(text.c_str());
    plate("##turnplatebox", {t.x + (end ? 64.0f : 28.0f), t.y + (end ? 28.0f : 12.0f)}, [&](ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 ink) {
      dl->AddText({std::round((a.x + b.x - t.x) * 0.5f), std::round((a.y + b.y - t.y) * 0.5f)}, ink, text.c_str());
    });
    ImGui::PopFont();
    ImGui::End();
    ImGui::PopStyleVar(2);
  }

  // The party: the first player unit is the hero (test lights, the scout); its walk goes to the farthest enemy.
  // --- The walkable Inn and other free-roam maps: who is there, where the party comes in, who talks ---------------------

  static std::string lowerAscii(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
  }
  // GameApp startBattle's `seated` and `led` on the scene: companions not in the party leave; a leader other than the
  // authored walker takes its place with its own art (the leader seated on this map, else its catalog entry, idle).
  void arrangeScene() {
    std::vector<e2::SceneUnit>& units = scene_->units;
    std::erase_if(units, [&](const e2::SceneUnit& u) { return u.side == "neutral" && options_.absentNeutrals.contains(u.name); });
    if (options_.leader.empty()) return;
    const auto walker = std::find_if(units.begin(), units.end(), [](const e2::SceneUnit& u) { return u.side == "player"; });
    if (walker == units.end() || walker->name == options_.leader) return;
    const size_t w = static_cast<size_t>(walker - units.begin());
    const auto seated = std::find_if(units.begin(), units.end(), [&](const e2::SceneUnit& u) { return u.side == "neutral" && u.name == options_.leader; });
    e2::SceneUnit led = units[w];
    if (seated != units.end()) {
      const e2::SceneUnit& s = *seated;
      led.classId = s.classId, led.key = s.key, led.sprite = s.sprite, led.set = s.set, led.clip = s.clip, led.facing = s.facing, led.mirror = s.mirror;
      led.boxW = s.boxW, led.boxH = s.boxH, led.foot = s.foot, led.natW = s.natW, led.natH = s.natH, led.frames = s.frames, led.pace = s.pace;
    } else {
      const std::optional<std::vector<e2::Hero>> catalog = options_.heroes.empty() ? std::nullopt : e2::loadHeroes(options_.heroes);
      const e2::Hero* entry = nullptr;
      if (catalog)
        for (const e2::Hero& h : *catalog)
          if (h.key == lowerAscii(options_.leader)) entry = &h;
      const e2::HeroPose* idle = entry ? entry->find(kIdle) : nullptr;
      if (!idle) {
        logWarn("party: leader {} has no art here (not seated, not in the catalog): {} walks", options_.leader, led.name);
        return;
      }
      led.key = entry->key, led.set = idle->set, led.clip = idle->clip, led.boxW = idle->boxW, led.boxH = idle->boxH, led.foot = idle->foot;
      led.frames = idle->frames, led.pace = idle->pace, led.facing = 1, led.mirror = entry->reversed, led.natW = led.natH = 0;
    }
    led.name = options_.leader;
    units[w] = std::move(led);
    std::erase_if(units, [&](const e2::SceneUnit& u) { return u.side == "neutral" && u.name == options_.leader; });
    logInfo("party: {} leads (walks in the authored walker's place)", options_.leader);
  }
  // The same on the battle's own units (their sounds follow them).
  void arrangeBattle() {
    std::vector<e2::BattleUnit>& units = battle_->units();
    while (unitSounds_.size() < units.size()) unitSounds_.push_back(e2::Json::object());
    auto drop = [&](size_t b) {
      units.erase(units.begin() + static_cast<std::ptrdiff_t>(b));
      unitSounds_.erase(unitSounds_.begin() + static_cast<std::ptrdiff_t>(b));
    };
    for (size_t b = units.size(); b-- > 0;)
      if (units[b].side == "neutral" && options_.absentNeutrals.contains(units[b].name)) drop(b);
    if (options_.leader.empty()) return;
    const auto walker = std::find_if(units.begin(), units.end(), [](const e2::BattleUnit& u) { return u.side == "player" && !u.summoned; });
    if (walker == units.end() || walker->name == options_.leader) return;
    const size_t w = static_cast<size_t>(walker - units.begin());
    const auto seated = std::find_if(units.begin(), units.end(), [&](const e2::BattleUnit& u) { return u.side == "neutral" && u.name == options_.leader; });
    if (seated == units.end()) {
      // Not seated on this map: the walker carries the leader's name (its art from the catalog, arrangeScene).
      if (std::any_of(scene_->units.begin(), scene_->units.end(), [&](const e2::SceneUnit& u) { return u.side == "player" && u.name == options_.leader; }))
        units[w].name = options_.leader;
      return;
    }
    e2::BattleUnit& s = *seated;
    s.side = "player", s.x = units[w].x, s.y = units[w].y;
    const size_t from = static_cast<size_t>(seated - units.begin());
    std::swap(units[w], units[from]);
    std::swap(unitSounds_[w], unitSounds_[from]);
    drop(from);
  }
  // Each NPC's conversation: the map's neutral spawn of that name on that hex (Engine2 handleCell reads the unit's own).
  void findConversations() {
    npcDialog_.clear();
    for (size_t b = 0; b < battle_->units().size(); ++b) {
      const e2::BattleUnit& u = battle_->units()[b];
      if (u.side != "neutral") continue;
      for (const e2::MapSpawn& s : board_->draft().neutralSpawns)
        if (s.name == u.name && s.x == u.x && s.y == u.y && s.dialog.is_object()) {
          npcDialog_[b] = s.dialog;
          break;
        }
    }
    if (!npcDialog_.empty()) logInfo("conversations: {} NPCs talk", npcDialog_.size());
  }
  // GameApp arriveAtConnector: the party beside the connector that leads back where it came from, in walking order out from
  // it (the terrain's own passability, no blocking decoration; never on a waypoint or any enemy / neutral spawn's body, the dead
  // of an earlier visit included); the heroes who fell on the floor before do not count; the authored spawns when there is
  // not room enough.
  void arriveAtConnector() {
    const auto& exits = battle_->exits();
    const auto back = std::find_if(exits.begin(), exits.end(), [&](const e2::Battle::Exit& x) {
      return x.kind == "connector" && x.targetMapId == options_.arrivedFrom && !x.cells.empty();
    });
    if (back == exits.end()) return logWarn("arrival: no connector back to {} on this map", options_.arrivedFrom);
    std::vector<size_t> party;
    std::set<std::pair<int, int>> waypoints, bodies;
    for (size_t b = 0; b < battle_->units().size(); ++b) {
      const e2::BattleUnit& u = battle_->units()[b];
      if (u.side == "player") {
        if (!u.summoned && !absent_.contains(u.id)) party.push_back(b);
      } else {
        for (const e2::BattleHex& h : e2::Battle::footprint(u, u.x, u.y)) bodies.insert({h.x, h.y});
      }
    }
    for (const e2::Battle::Exit& x : exits)
      for (const e2::BattleHex& c : x.cells) waypoints.insert({c.x, c.y});
    auto walkable = [&](int x, int y) {
      if (x < 0 || y < 0 || x >= battle_->cols() || y >= battle_->rows()) return false;
      const e2::BattleCell& c = battle_->cell(x, y);
      const auto t = board_->data().terrain.find(c.terrain);  // TERRAIN[terrain].passable: the tile itself, decorations aside
      return t != board_->data().terrain.end() && t->second.passable && !c.blocked;
    };
    std::vector<e2::BattleHex> cells, queue{{back->x, back->y}};
    std::set<std::pair<int, int>> seen{{back->x, back->y}};
    for (size_t q = 0; q < queue.size() && cells.size() < party.size(); ++q)
      for (const e2::BattleHex& n : e2::Battle::neighbors(queue[q].x, queue[q].y)) {
        if (seen.contains({n.x, n.y}) || !walkable(n.x, n.y)) continue;
        seen.insert({n.x, n.y});
        queue.push_back(n);
        if (!waypoints.contains({n.x, n.y}) && !bodies.contains({n.x, n.y}) && cells.size() < party.size()) cells.push_back(n);
      }
    if (cells.size() < party.size()) return logWarn("arrival: not room enough beside the connector back to {}", options_.arrivedFrom);
    // applyPartyFormation on those cells: the hero first in the formation takes the cell nearest an enemy, the last the farthest
    if (options_.formation) {
      std::stable_sort(party.begin(), party.end(), [&](size_t a, size_t b) { return formationRank(battle_->units()[a].name) < formationRank(battle_->units()[b].name); });
      std::stable_sort(cells.begin(), cells.end(), [&](const e2::BattleHex& a, const e2::BattleHex& b) { return enemyDistance(a) < enemyDistance(b); });
    }
    for (size_t k = 0; k < party.size(); ++k) {
      e2::BattleUnit& u = battle_->units()[party[k]];
      u.x = cells[k].x, u.y = cells[k].y;
      if (const int ui = sceneUnitOf(static_cast<int>(party[k])); ui >= 0) unitPos_[static_cast<size_t>(ui)] = anchorWorld(u, cells[k]), unitMoved_[static_cast<size_t>(ui)] = 1;
    }
    logInfo("arrival: {} beside the connector back to {}", party.size(), options_.arrivedFrom);
  }
  // A click on an NPC who talks: in free roam the leader walks to the free hex beside it its reach gets to cheapest, then they
  // talk; already beside it (or nowhere free to stand) they talk from where the leader is.
  void talkTo(size_t npc) {
    const e2::BattleUnit& n = battle_->units()[npc];
    if (battle_->roaming() && myTurn()) {
      const e2::BattleUnit& me = battle_->units()[static_cast<size_t>(unitBattle_[static_cast<size_t>(selected_)])];
      const std::vector<e2::BattleHex> around = e2::Battle::neighbors(n.x, n.y);
      if (std::none_of(around.begin(), around.end(), [&](const e2::BattleHex& h) { return h.x == me.x && h.y == me.y; })) {
        std::optional<e2::BattleHex> best;
        double bestCost = 1e18;
        for (const e2::BattleHex& h : around)
          for (const e2::ReachCell& c : reach_)
            if (c.x == h.x && c.y == h.y && c.cost < bestCost && battle_->unitAt(h.x, h.y) < 0) best = h, bestCost = c.cost;
        if (best) {
          walkTo(static_cast<size_t>(selected_), *best);
          talkAfterWalk_ = static_cast<int>(npc);
          return;
        }
      }
    }
    talkRequest_ = npc;
  }
  // --- the party's Formação inicial ---
  // The Party screen's board (OverworldStage: radius 2, rows 3-4-5-4-3 front to back, left to right): the saved cells, then every
  // hero they leave out in the first empty cells, in the heroes' own order (as the screen shows it).
  std::vector<std::string> formationCells() const {
    static constexpr const char* kHeroes[] = {"Kael", "Neera", "Voss", "Salazar", "Aldric", "Malrec"};
    std::vector<std::string> cells(19);
    std::set<std::string> placed;
    for (size_t i = 0; i < options_.partyFormation.size() && i < cells.size(); ++i)
      if (!options_.partyFormation[i].empty() && placed.insert(options_.partyFormation[i]).second) cells[i] = options_.partyFormation[i];
    for (const char* h : kHeroes)
      if (!placed.contains(h))
        if (const auto free = std::find(cells.begin(), cells.end(), std::string()); free != cells.end()) *free = h, placed.insert(h);
    return cells;
  }
  size_t formationRank(const std::string& hero) const {
    const std::vector<std::string> cells = formationCells();
    return static_cast<size_t>(std::find(cells.begin(), cells.end(), hero) - cells.begin());
  }
  // Hexes to the nearest enemy still standing (a board-wide number without one).
  int enemyDistance(e2::BattleHex at) const {
    int best = 1 << 20;
    for (const e2::BattleUnit& u : battle_->units())
      if (u.side == "enemy" && u.alive) best = std::min(best, e2::Battle::distance(at, {u.x, u.y}));
    return best;
  }
  // The party set down by its Formação inicial at the map's entry (the first party spawn the editor placed: the map only gives
  // where the party comes in), the board's front row turned toward the nearest enemy. A cell that is not free ground (off the
  // board, a wall, water, a decoration, a body, a waypoint, or cut off from the entry) gives way to the free cell nearest to it and
  // to the entry, so a map too tight for the whole board keeps the group together.
  void placeFormation() {
    std::vector<e2::BattleUnit>& units = battle_->units();
    std::map<std::string, size_t> heroes;
    for (size_t b = 0; b < units.size(); ++b)
      if (units[b].side == "player" && !units[b].summoned && units[b].alive) heroes[units[b].name] = b;
    if (heroes.empty()) return;
    const std::vector<e2::MapSpawn>& spawns = board_->draft().playerSpawns;
    // A party the map splits on purpose (a scenario: its spawns fall into separate groups, none within 4 hexes of another group,
    // Engine2's own "spread out" distance) stays split: every hero where the map puts it.
    {
      std::vector<int> group(spawns.size(), -1);
      int groups = 0;
      for (size_t s = 0; s < spawns.size(); ++s) {
        if (group[s] >= 0) continue;
        std::vector<size_t> stack{s};
        group[s] = groups;
        while (!stack.empty()) {
          const size_t a = stack.back();
          stack.pop_back();
          for (size_t b = 0; b < spawns.size(); ++b)
            if (group[b] < 0 && e2::Battle::distance({spawns[a].x, spawns[a].y}, {spawns[b].x, spawns[b].y}) <= 4) group[b] = groups, stack.push_back(b);
        }
        ++groups;
      }
      if (groups > 1) return logInfo("formation: the map splits the party into {} groups: every hero keeps the map's place", groups);
    }
    e2::BattleHex entry = spawns.empty() ? e2::BattleHex{units[heroes.begin()->second].x, units[heroes.begin()->second].y} : e2::BattleHex{spawns.front().x, spawns.front().y};
    // free ground, and what stands on it
    std::set<std::pair<int, int>> blocked;
    for (const e2::BattleUnit& u : units)
      if (u.side != "player" && u.alive)
        for (const e2::BattleHex& h : e2::Battle::footprint(u, u.x, u.y)) blocked.insert({h.x, h.y});
    for (const e2::Battle::Exit& x : battle_->exits())
      for (const e2::BattleHex& c : x.cells) blocked.insert({c.x, c.y});
    auto ground = [&](int x, int y) { return battle_->inBounds(x, y) && battle_->cell(x, y).passable && !battle_->cell(x, y).blocked; };
    // the ground the entry reaches, with its walking distance from the entry (from the nearest ground when the entry is not)
    std::map<std::pair<int, int>, int> reach;
    {
      std::vector<e2::BattleHex> queue{entry};
      std::set<std::pair<int, int>> seen{{entry.x, entry.y}};
      for (size_t q = 0; q < queue.size() && !ground(entry.x, entry.y); ++q)
        for (const e2::BattleHex& n : e2::Battle::neighbors(queue[q].x, queue[q].y)) {
          if (!battle_->inBounds(n.x, n.y) || !seen.insert({n.x, n.y}).second) continue;
          if (ground(n.x, n.y)) {
            entry = n;
            break;
          }
          queue.push_back(n);
        }
      if (!ground(entry.x, entry.y)) return logWarn("formation: no ground near the entry; the party keeps the file's places");
      queue = {entry};
      reach[{entry.x, entry.y}] = 0;
      for (size_t q = 0; q < queue.size(); ++q)
        for (const e2::BattleHex& n : e2::Battle::neighbors(queue[q].x, queue[q].y))
          if (ground(n.x, n.y) && !reach.contains({n.x, n.y})) reach[{n.x, n.y}] = reach[{queue[q].x, queue[q].y}] + 1, queue.push_back(n);
    }
    // the board's cells as axial offsets (pointy-top rows, front = up), turned by k sixths to face the nearest enemy
    static constexpr int kRows[5][2] = {{0, 3}, {3, 4}, {7, 5}, {12, 4}, {16, 3}};
    auto slotAxial = [](int i) {
      for (int r = 0; r < 5; ++r)
        if (i >= kRows[r][0] && i < kRows[r][0] + kRows[r][1]) {
          const int R = r - 2, k = i - kRows[r][0];
          return std::pair<int, int>{k - (4 - std::abs(R) + R) / 2, R};
        }
      return std::pair<int, int>{0, 0};
    };
    auto turn = [](std::pair<int, int> a, int k) {  // cube (q, r, s) -> (-r, -s, -q), k times
      for (int t = 0; t < k; ++t) a = {-a.second, a.first + a.second};
      return a;
    };
    auto pixel = [](double q, double r) { return glm::dvec2(q + r * 0.5, r * std::sqrt(3.0) * 0.5); };
    auto offsetPixel = [](e2::BattleHex h) { return glm::dvec2(h.x + (h.y & 1) * 0.5, h.y * std::sqrt(3.0) * 0.5); };
    int facing = 0;
    std::optional<e2::BattleHex> foe;
    for (const e2::BattleUnit& u : units)
      if (u.side == "enemy" && u.alive && (!foe || e2::Battle::distance(entry, {u.x, u.y}) < e2::Battle::distance(entry, *foe))) foe = e2::BattleHex{u.x, u.y};
    if (foe) {
      const glm::dvec2 want = offsetPixel(*foe) - offsetPixel(entry);
      double best = -1e18;
      for (int k = 0; k < 6; ++k) {
        const auto front = turn(slotAxial(1), k);
        const double d = glm::dot(pixel(front.first, front.second), want);
        if (d > best) best = d, facing = k;
      }
    }
    const int q0 = entry.x - (entry.y - (entry.y & 1)) / 2, r0 = entry.y;
    const std::vector<std::string> cells = formationCells();
    std::set<std::pair<int, int>> taken;
    int moved = 0;
    for (int i = 0; i < static_cast<int>(cells.size()); ++i) {
      const auto it = heroes.find(cells[static_cast<size_t>(i)]);
      if (it == heroes.end()) continue;
      const auto [dq, dr] = turn(slotAxial(i), facing);
      const int r = r0 + dr, q = q0 + dq;
      const e2::BattleHex want{q + (r - (r & 1)) / 2, r};
      auto free = [&](const std::pair<int, int>& c) { return reach.contains(c) && !blocked.contains(c) && !taken.contains(c); };
      std::pair<int, int> at{want.x, want.y};
      if (!free(at)) {
        ++moved;
        int bestCost = 1 << 30;
        for (const auto& [c, d] : reach)
          if (free(c)) {
            const int cost = e2::Battle::distance({c.first, c.second}, want) + d;
            if (cost < bestCost) bestCost = cost, at = c;
          }
        if (bestCost == (1 << 30)) continue;  // nowhere left: the file's place
      }
      taken.insert(at);
      e2::BattleUnit& u = units[it->second];
      u.x = at.first, u.y = at.second;
      if (const int ui = sceneUnitOf(static_cast<int>(it->second)); ui >= 0)
        unitPos_[static_cast<size_t>(ui)] = anchorWorld(u, {u.x, u.y}), unitMoved_[static_cast<size_t>(ui)] = 1;
    }
    logInfo("formation: {} heroes at the entry ({}, {}), front turned {} sixths toward {}; {} moved to the nearest free ground", heroes.size(),
            entry.x, entry.y, facing, foe ? std::format("the enemy at ({}, {})", foe->x, foe->y) : std::string("no enemy"), moved);
  }
  // wispCrossing.ts routeWispCrossing: on the Wisp Forest crossing every dungeon exit and every connector to the lair leads to the
  // carnivorous plant's lair (a floor connector up) until the crossing is finished, then outside (a dungeon exit, two hexes wide
  // as Engine2's dungeon-exit decoration is).
  static constexpr const char* kWispCrossing = "wisp-forest-crossing";
  static constexpr const char* kWispBoss = "wisp-forest-crossing-boss";
  void routeWispCrossing() {
    if (!board_ || board_->draft().id != kWispCrossing) return;
    for (e2::Battle::Exit& x : battle_->exits()) {
      if (x.kind != "dungeon" && x.targetMapId != kWispBoss) continue;
      x.returnConnector = false;
      if (options_.wispCrossingDone) {
        x.id = "dungeon-exit", x.kind = "dungeon", x.targetMapId.clear(), x.connectorDirection.clear();
        x.cells = {{x.x, x.y}, {x.x + 1, x.y}};
      } else {
        x.id = "floor-connector", x.kind = "connector", x.targetMapId = kWispBoss, x.connectorDirection = "up";
        x.cells = {{x.x, x.y}};
      }
    }
  }
  // The walkable Inn's two floors (Engine2's result screen names their stairs "Subir para o Segundo andar" / "Descer para o
  // Primeiro andar"); any other connector goes the way floorConnectorDirection says.
  bool innFloor() const { return board_ && board_->draft().id.rfind("estalagem", 0) == 0; }
  bool connectorUp(const e2::Battle::Exit& x) const {
    if (innFloor() && x.connectorDirection.empty()) return !x.returnConnector;
    if (!x.connectorDirection.empty()) return x.connectorDirection == "up";
    if (x.id == "stone-stairs-up-001") return true;
    if (x.id == "stone-stairs-down-001") return false;
    return x.returnConnector;
  }

  void setUpParty() {
    for (size_t i = 0; i < scene_->units.size(); ++i)
      if (scene_->units[i].side == "player") {
        hero_ = static_cast<int>(i);
        break;
      }
    if (hero_ < 0) return;
    const e2::SceneUnit& h = scene_->units[static_cast<size_t>(hero_)];
    heroAt_ = {h.x, groundOf(h.groundSmooth, h.groundHard), h.z};
    if (!options_.scout) return;
    const auto [hc, hr] = e2::hexAt(board_->layout(), h.x, h.z);
    e2::HexPoint goal{hc, hr};
    int best = -1;
    for (const e2::SceneUnit& u : scene_->units) {
      if (u.side != "enemy") continue;
      const auto [c, r] = e2::hexAt(board_->layout(), u.x, u.z);
      const int d = e2::hexDistance({hc, hr}, {c, r});
      if (d > best) best = d, goal = {c, r};
    }
    path_ = e2::hexLine({hc, hr}, goal);
    logInfo("scout: {} walks {} hexes from ({}, {}) to ({}, {}) and back", h.name, path_.size() - 1, hc, hr, goal.c, goal.r);
  }

  // What stands on the board and blocks sight: 3D models over their footprint, standing cards at least
  // kBlockHeight tall and kBlockWidth wide along their width; 3D models at least kBlockWidth across. (Walls and doors are not imported yet: they block nothing.)
  // A card's width is its drawn art, not its image box (the user, 2026-10-18: trees left holes everywhere): the box is
  // 1.86 wide for a tree whose trunk fills a third of it, so every tree, stump and bush walled off three hexes.
  // content/engine2/card-sight.json (tools/scripts/engine2/card_sight.py) = the opaque share of each image's width.
  static float cardArtShare(const std::string& image) {
    static const std::unordered_map<std::string, float> shares = [] {
      std::unordered_map<std::string, float> out;
      std::ifstream in("content/engine2/card-sight.json");
      const nlohmann::json j = in ? nlohmann::json::parse(in, nullptr, false) : nlohmann::json();
      if (j.is_object())
        for (const auto& [key, value] : j.items())
          if (value.is_number()) out[key] = value.get<float>();
      if (out.empty()) logError("fog of war: content/engine2/card-sight.json missing: cards block by their whole image box");
      return out;
    }();
    const auto it = shares.find(image);
    return it == shares.end() ? 1.0f : it->second;
  }

  void addOccluders() {
    for (const ArchPiece& a : arch_)
      if (a.blocks)
        for (const auto& [x, z] : a.footprint) fog_->block(x, z, a.top);
    for (const e2::SceneModel& m : scene_->models) {
      std::string asset = m.file.substr(std::string("/game/").size());
      asset = "meshes/" + asset.substr(0, asset.size() - 4) + ".mesh";
      const MeshHeader* h = meshes_.header(modelMeshes_.at(asset));
      if (!h) continue;
      const float k = m.height / std::max(h->boundsMax[1] - h->boundsMin[1], 1e-4f);
      const float hx = (h->boundsMax[0] - h->boundsMin[0]) * 0.5f * k, hz = (h->boundsMax[2] - h->boundsMin[2]) * 0.5f * k;
      if (2.0f * std::max(hx, hz) < e2::kBlockWidth) continue;  // a thin model (a post) does not block
      const float c = std::cos(m.rotY), sn = std::sin(m.rotY);
      // The footprint (rotated box, centred on the anchor as placeModels centres it), sampled every 0.25.
      for (float a = -hx; a <= hx; a += 0.25f)
        for (float b = -hz; b <= hz; b += 0.25f) fog_->block(m.x + c * a + sn * b, m.z - sn * a + c * b, m.height);
      fog_->block(m.x, m.z, m.height);
    }
    for (const e2::SceneCard& d : scene_->cards) {
      const float drawn = d.w * cardArtShare(d.image);
      if (d.onFloor || d.h < e2::kBlockHeight || drawn < e2::kBlockWidth) continue;  // thin props do not block
      const float yaw = d.lockedYaw.value_or(0.0f);
      const glm::vec2 right{std::cos(yaw), -std::sin(yaw)};
      const float half = drawn * 0.5f;
      for (float a = -half; a <= half; a += 0.25f) fog_->block(d.x + right.x * a, d.z + right.y * a, d.h);
    }
  }

  // The scout's path index after `step` steps: out to the goal, back, again.
  int walkAt(int step) const {
    const int n = static_cast<int>(path_.size()) - 1, c = step % (2 * n);
    return c <= n ? c : 2 * n - c;
  }

  // Sight: replay every step the scout has taken since the last frame, then paint the mask when anything changed.
  void updateFog(float time) {
    if (hero_ >= 0 && path_.size() > 1) {
      const float stepTime = 0.6f, s = time / stepTime;
      const int step = static_cast<int>(s);
      const int at = walkAt(step), next = walkAt(step + 1);
      const e2::HexLayout& L = board_->layout();
      const float f = s - static_cast<float>(step);
      const glm::vec2 a{static_cast<float>(e2::hexX(L, path_[at].c, path_[at].r)), static_cast<float>(e2::hexZ(L, path_[at].r))};
      const glm::vec2 b{static_cast<float>(e2::hexX(L, path_[next].c, path_[next].r)), static_cast<float>(e2::hexZ(L, path_[next].r))};
      const glm::vec2 p = glm::mix(a, b, f);
      heroAt_ = {p.x, static_cast<float>(e2::groundHeightAt(*board_, look_, p.x, p.y)), p.y};
      // The scout stands on the hex its feet are over.
      const auto [hc, hr] = e2::hexAt(L, p.x, p.y);
      heroHex_ = {hc, hr};
    }
    if (!fog_ || !scene_ || !modelsPlaced_) return;
    bool changed = false;
    if (!occludersAdded_) {
      addOccluders();
      occludersAdded_ = true;
      changed = true;
    }
    // The party's eyes: each living player unit's hex (the scout's where it is now).
    std::vector<e2::HexPoint> party;
    for (size_t i = 0; i < scene_->units.size(); ++i) {
      const e2::SceneUnit& u = scene_->units[i];
      if (u.side != "player") continue;
      if (static_cast<int>(i) == hero_ && path_.size() > 1) {
        party.push_back(heroHex_);
        continue;
      }
      if (battle_ && unitBattle_[i] >= 0 && !battle_->units()[static_cast<size_t>(unitBattle_[i])].alive) continue;
      const auto [c, r] = e2::hexAt(board_->layout(), unitPos_[i].x, unitPos_[i].y);
      party.push_back({c, r});
    }
    if (changed || party != lastParty_) {
      // Every hex walked since the last relight sees in turn (a pinned clock replays the whole walk).
      if (hero_ >= 0 && path_.size() > 1 && options_.sceneTime >= 0.0f && !replayed_) {
        replayed_ = true;
        const int steps = static_cast<int>(time / 0.6f);
        for (int s = 0; s < steps; ++s) {
          std::vector<e2::HexPoint> eyes = party;
          eyes[0] = path_[walkAt(s)];  // the hero is the first player unit
          fogQueue_.push_back(std::move(eyes));
        }
      }
      fogQueue_.push_back(party);
      lastParty_ = party;
    }
    // Sight and the mask are worked out on a worker (6-10 ms on aldeia015); the main thread never waits on it.
    // While a job runs it alone touches fog_; its mask reaches the renderer the frame after it finishes.
    if (fogBusy_ && fogJob_.done()) {
      const auto started = std::chrono::steady_clock::now();
      fogBusy_ = false;
      fogMask_ = std::move(fogNext_);
      fogTexels_.swap(fogNextTexels_);
      ++maskVersion_;
      fogReady_ = true;
      const e2::HexLayout& L = board_->layout();
      fogStates_.resize(static_cast<size_t>(L.cols * L.rows));
      fogCounts_ = {0, 0, 0};
      for (int r = 0; r < L.rows; ++r)
        for (int c = 0; c < L.cols; ++c) {
          const uint8_t s = fog_->state(c, r);
          fogStates_[static_cast<size_t>(e2::indexOf(L, c, r))] = s;
          ++fogCounts_[s];
        }
      fogLit_ = fog_->displayStates(fogJobEyes_.back());  // what the mask paints lit: what stands there is drawn
      countHoles(time);
      lastFogMs_ = fogJobMs_;
      fogDeliverMs_.push_back(std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count());
      fogUpdatedThisFrame_ = true;
    }
    // The fog's clock (real seconds): ground the party left stays lit kLingerSeconds, then darkens. Checked once a second;
    // when some ground's time ran out, the party's sight runs again on the worker and the mask is painted without it.
    const double fogClock = std::chrono::duration<double>(std::chrono::steady_clock::now() - fogClockStart_).count();
    if (!fogBusy_) fog_->setClock(fogClock);
    if (!fogBusy_ && !pendingExplored_.empty()) fog_->restoreExplored(std::exchange(pendingExplored_, {}));  // a resumed fight
    if (!fogBusy_ && fogQueue_.empty() && !lastParty_.empty() && fogClock - fogLingerCheck_ >= 1.0) {
      if (fog_->lingerEnded(fogLingerCheck_)) fogQueue_.push_back(lastParty_);
      fogLingerCheck_ = fogClock;
    }
    if (!fogBusy_ && !fogQueue_.empty()) {
      // Every party position queued since the last job relights in order, so what is remembered is exactly
      // what one relight per change would leave; the mask is painted once, for the latest.
      fogJobEyes_ = std::move(fogQueue_);
      fogQueue_.clear();
      fogBusy_ = true;
      jobs_.run([this] { runFogJob(); }, &fogJob_);
    }
  }

  // On a worker: relight every queued party position, then paint the mask for the last one.
  void runFogJob() {
    const auto started = std::chrono::steady_clock::now();
    if (!sightLogged_) {
      sightLogged_ = true;
      // How much the line of sight hides from each party unit where it starts.
      const e2::HexLayout& L = board_->layout();
      for (const e2::SceneUnit& u : scene_->units) {
        if (u.side != "player") continue;
        const auto [c, r] = e2::hexAt(L, u.x, u.z);
        int inRange = 0, seen = 0;
        for (int y = 0; y < L.rows; ++y)
          for (int x = 0; x < L.cols; ++x)
            if (e2::hexDistance({c, r}, {x, y}) <= e2::kSightRadius) ++inRange, seen += fog_->sightReaches({c, r}, {x, y}) ? 1 : 0;
        logInfo("sight: {} at ({}, {}) sees {} of the {} hexes in range ({} hidden by ground or what stands on it)", u.name, c, r, seen,
                inRange, inRange - seen);
      }
    }
    for (const std::vector<e2::HexPoint>& eyes : fogJobEyes_) relight(eyes);
    fogNext_ = fog_->paint(*board_, fogJobEyes_.back());
    Renderer::fogTexels(fogNext_, fogNextTexels_);  // the GPU's half floats, so the main thread only copies
    fogJobMs_ = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
  }

  // Holes: groups of never-seen hexes enclosed by explored ground (touching neither the board's edge nor the
  // big never-seen area). Each shows as a black pit in the middle of the map.
  void countHoles(float time) {
    const e2::HexLayout& L = board_->layout();
    std::vector<int> group(static_cast<size_t>(L.cols * L.rows), -1);
    std::vector<std::pair<int, int>> holeList;
    holes_ = 0;
    holeHexes_ = 0;
    for (int r = 0; r < L.rows; ++r)
      for (int c = 0; c < L.cols; ++c) {
        if (fog_->state(c, r) != e2::kFogUnseen || group[static_cast<size_t>(e2::indexOf(L, c, r))] >= 0) continue;
        std::vector<std::pair<int, int>> stack{{c, r}}, members;
        group[static_cast<size_t>(e2::indexOf(L, c, r))] = 1;
        bool edge = false;
        while (!stack.empty()) {
          const auto [x, y] = stack.back();
          stack.pop_back();
          members.push_back({x, y});
          if (x == 0 || y == 0 || x == L.cols - 1 || y == L.rows - 1) edge = true;
          for (const auto& [nx, ny] : e2::neighbors(L, x, y)) {
            int& g = group[static_cast<size_t>(e2::indexOf(L, nx, ny))];
            if (g < 0 && fog_->state(nx, ny) == e2::kFogUnseen) g = 1, stack.push_back({nx, ny});
          }
        }
        if (!edge && members.size() <= 12) {
          ++holes_, holeHexes_ += static_cast<int>(members.size());
          holeList.insert(holeList.end(), members.begin(), members.end());
        }
      }
    worstHoles_ = std::max(worstHoles_, holes_);
    // Where they are: each new set of holes goes to captures/fog/holes.txt with the whole board's states
    // (. never seen, r remembered, v in sight, H hole, S the scout), for tools/scripts/draw_fog_holes.py.
    std::sort(holeList.begin(), holeList.end());
    if (holeList.empty() || holeList == lastHoles_) return;
    lastHoles_ = holeList;
    std::FILE* file = std::fopen("captures/fog/holes.txt", holesLogged_ ? "a" : "w");
    if (!file) return;
    holesLogged_ = true;
    std::fprintf(file, "time %.2f scout %d %d holes %d\n", time, heroHex_.c, heroHex_.r, holes_);
    for (int r = 0; r < L.rows; ++r) {
      for (int c = 0; c < L.cols; ++c) {
        char ch = ".rv"[fog_->state(c, r)];
        if (std::binary_search(holeList.begin(), holeList.end(), std::make_pair(c, r))) ch = 'H';
        if (path_.size() > 1 && heroHex_.c == c && heroHex_.r == r) ch = 'S';
        std::fputc(ch, file);
      }
      std::fputc('\n', file);
    }
    std::fclose(file);
  }

  void relight(const std::vector<e2::HexPoint>& party) {
    std::vector<e2::Eye> eyes;
    for (const e2::HexPoint& p : party) eyes.push_back({p, e2::kSightRadius});
    fog_->relight(eyes);
  }

  // How a looping pose repeats, read off Engine2's 6 s sample of its frame pacing (ticks of 1/240 s). The cycle is
  // rarely a whole number of ticks (Neera's idle: 36 frames at 10.204 fps = 846.7) and can be longer than the sample
  // (Malrec's idle bounces over 6.5 s), so it is measured from the runs of frames; wrapping at the sample's end cut
  // those loops short every 6 s (Neera jumped from frame 25 back to 0).
  struct PaceLoop {
    enum Kind { kSample, kForward, kBounce } kind = kSample;
    double period = 0;    // ticks per cycle
    double perFrame = 0;  // kForward / kBounce: ticks per frame step
  };

  const PaceLoop& paceLoop(const std::vector<int>& pace, int frames) {
    auto it = loops_.find(&pace);
    if (it != loops_.end()) return it->second;
    PaceLoop loop{PaceLoop::kSample, static_cast<double>(pace.size()), 0.0};
    std::vector<std::pair<int, size_t>> runs;  // frame, first tick
    for (size_t i = 0; i < pace.size(); ++i)
      if (runs.empty() || runs.back().first != pace[i]) runs.push_back({pace[i], i});
    const size_t n = runs.size();
    if (n >= 3) {
      // The shortest shift that lays the run sequence on itself; the period runs from run 1 (run 0 may begin
      // mid-frame) over every whole cycle the sample holds.
      bool found = false;
      for (size_t q = 1; q + 1 < n && !found; ++q) {
        bool same = true;
        for (size_t j = 0; j + q < n && same; ++j) same = runs[j].first == runs[j + q].first;
        const size_t k = (n - 2) / q;
        if (same && k >= 1) {
          loop.period = static_cast<double>(runs[1 + k * q].second - runs[1].second) / static_cast<double>(k);
          found = true;
        }
      }
      if (!found) {
        // A cycle longer than the sample: carry its steps on, forward round the clip or bouncing between its ends.
        const double perFrame = static_cast<double>(runs[n - 1].second - runs[1].second) / static_cast<double>(n - 2);
        const int f = std::max(frames, 1), bounceLen = 2 * f - 2;
        bool forward = true, bounce = f > 1;
        for (size_t j = 0; j < n; ++j) {
          forward = forward && (j == 0 || (runs[j - 1].first + 1) % f == runs[j].first);
          const int s = bounce ? static_cast<int>(j % static_cast<size_t>(bounceLen)) : 0;
          bounce = bounce && runs[j].first == (s < f ? s : bounceLen - s);
        }
        if (forward) loop = {PaceLoop::kForward, f * perFrame, perFrame};
        else if (bounce) loop = {PaceLoop::kBounce, bounceLen * perFrame, perFrame};
        else logWarn("map: a looping pose's pacing is neither a repeat nor a forward / bouncing cycle; it wraps at 6 s");
      }
    }
    return loops_[&pace] = loop;
  }

  uint32_t loopFrame(const std::vector<int>& pace, int frames, double tick) {
    const PaceLoop& loop = paceLoop(pace, frames);
    const double t = std::fmod(std::max(tick, 0.0), loop.period);
    if (loop.kind == PaceLoop::kSample) return static_cast<uint32_t>(pace[std::min(static_cast<size_t>(t), pace.size() - 1)]);
    const int f = std::max(frames, 1);
    if (loop.kind == PaceLoop::kForward) return static_cast<uint32_t>(static_cast<int>(t / loop.perFrame) % f);
    const int bounceLen = 2 * f - 2, s = std::min(static_cast<int>(t / loop.perFrame), bounceLen - 1);
    return static_cast<uint32_t>(s < f ? s : bounceLen - s);
  }

  Renderer& renderer_;
  UploadQueue uploads_;
  BoardGround ground_;
  SpriteSystem sprites_;
  SpellEffects spellFx_{sprites_};  // spell and strike visuals (the fx clips)
  std::map<int, int> zoneFx_;       // the rules' zone id -> its standing effect (SpellEffects::zoneStart)
  struct IceFx {
    e2::BattleHex center;
    std::string caster;
    int fx;
  };
  std::vector<IceFx> iceFx_;        // the Tempestade de Gelo fields drawn, by their centre and caster (syncIceStorms)
  struct WebFx {
    e2::BattleHex center;
    int radius;
    int fx;
  };
  std::vector<WebFx> webFx_;        // the Teia dos Sonhos 02 webs drawn, by their zone (syncWebs)
  bool webSynced_ = false;
  bool iceSynced_ = false;
  std::vector<std::pair<float, std::vector<e2::BattleEffect>>> pendingFx_;  // numbers waiting for an effect to land
  SpellsMenu spellsMenu_;           // the Magias window: each spell's effect version, loose effects
  std::vector<EffectInstance> effectList_;
  std::vector<SpellEffects::Light> fxLights_;
  int fxTestCycle_ = -1;
  // runScript
  size_t scriptStep_ = 0;
  bool scriptDone_ = false, scriptFailed_ = false, scriptStarted_ = false;
  std::optional<e2::BattleHex> scriptHover_;  // "aim:x:y": the cursor held on a hex
  std::function<std::string()> scriptCheck_;  // the playing step's check (empty = passed)
  std::string scriptPress_;                   // "press:<button>": the HUD button the next drawHudBar presses
  bool scriptFleeMiss_ = false;               // "press:flee:miss": that escape roll fails
  std::string scriptLabel_;
  MeshScene meshes_;
  AssetManager& assets_;
  std::optional<e2::SceneData> scene_;
  inline static const std::string kIdle = "idle";
  std::vector<e2::Hero> catalog_;                  // every unit's poses (setUpPoses)
  std::vector<const e2::Hero*> unitCatalog_;       // per scene unit: its catalog entry (null = idle only)
  std::vector<std::pair<std::string, std::string>> poseChoices_;  // the panel's "Animação": pose id, label
  int poseChoice_ = 0;
  int background_ = 0;  // the panel's "Fundo": 0 the map's painted backdrop, 1 the sky, 2 black, 3 the painting up to the horizon + the sky,
                        // 4 the painting aligned to the sky
  // How the map's painting sits against the sky (content/engine2/backdrop-fit.json; resolve.hlsl backgroundAt mode 2).
  struct BackdropFit {
    float horizon = 0.4f, bottom = 1.0f, skyTop = 0.08f, degreesPerHeight = 30.0f, bottomDegrees = 40.0f, spanDegrees = 150.0f, glowColumn = 0.5f;
    float horizonDegrees = -6.0f;  // where the painting's horizon row sits: degrees of elevation (negative = below the real horizon)
    std::string ground;            // the map's top-down ground image around the board (content/backdrops); "" = none
    bool keepSky = false;          // panoramas: keep the image's own sky (no matte), the real sky only above its top edge
    bool alphaSky = false;         // the image's sky is truly transparent (its alpha): the real sky shows through it (mode 4)
    bool whole = false;            // the regular image, opaque, its horizon row on the world's horizon, nothing blended (mode 5)
    float groundTile = 120.0f, groundFar = 300.0f;
    float groundFromDegrees = 10.0f;  // the ground image takes over from this many degrees below the horizon  // its width in world units; the painting takes over beyond this distance
  };
  BackdropFit backdropFit_;
  std::vector<std::string> panoramas_;    // the map's own panoramas (backdrop-fit.json "panoramas"), the panel's "panorama NN"
  std::vector<BackdropFit> panoramaFits_;
  int panoramaSky_ = 1;  // the panel's "Céu do panorama": 0 the image's own sky, 1 the real sky above the rooftops
  static std::vector<std::string> loadPanoramas(const std::string& mapId) {
    std::vector<std::string> out;
    std::ifstream in("content/engine2/backdrop-fit.json");
    const nlohmann::json j = in ? nlohmann::json::parse(in, nullptr, false) : nlohmann::json();
    if (j.is_object() && j.contains("panoramas") && j["panoramas"].is_object() && j["panoramas"].contains(mapId) && j["panoramas"][mapId].is_array())
      for (const nlohmann::json& f : j["panoramas"][mapId])
        if (f.is_string()) out.push_back(f.get<std::string>());
    return out;
  }
  static BackdropFit loadBackdropFit(const std::string& painting) {
    BackdropFit fit;
    std::ifstream in("content/engine2/backdrop-fit.json");
    const nlohmann::json j = in ? nlohmann::json::parse(in, nullptr, false) : nlohmann::json();
    const auto read = [&](const nlohmann::json& e) {
      if (!e.is_object()) return;
      fit.horizon = e.value("horizon", fit.horizon);
      fit.bottom = e.value("bottom", fit.bottom);
      fit.skyTop = e.value("skyTop", fit.skyTop);
      fit.degreesPerHeight = e.value("degreesPerHeight", fit.degreesPerHeight);
      fit.bottomDegrees = e.value("bottomDegrees", fit.bottomDegrees);
      fit.horizonDegrees = e.value("horizonDegrees", fit.horizonDegrees);
      fit.ground = e.value("ground", fit.ground);
      fit.keepSky = e.value("keepSky", fit.keepSky);
      fit.alphaSky = e.value("alphaSky", fit.alphaSky);
      fit.whole = e.value("whole", fit.whole);
      fit.groundTile = e.value("groundTile", fit.groundTile);
      fit.groundFar = e.value("groundFar", fit.groundFar);
      fit.groundFromDegrees = e.value("groundFromDegrees", fit.groundFromDegrees);
      fit.spanDegrees = e.value("spanDegrees", fit.spanDegrees);
      fit.glowColumn = e.value("glowColumn", fit.glowColumn);
    };
    if (j.is_object()) {
      if (j.contains("default")) read(j["default"]);
      const std::string name = std::filesystem::path(painting).filename().string();
      if (j.contains("paintings") && j["paintings"].is_object() && j["paintings"].contains(name)) read(j["paintings"][name]);
    }
    return fit;
  }
  // The panel's "Água 3D": 0 the original (Engine2's ThreeWater, the default), 1 rio (the board's original material),
  // 2 rio ember (look 001); never off.
  int water3dLook_ = 0;
  // The panel's "Água": 0 the original (Engine2's diorama water, the default: the user, 2026-10-19, "NUNCA TROCAR O ASSET
  // PADRÃO"), 1 look 001 "ember" (shaders/water_look001.hlsli), an option only.
  int waterLook_ = 0;
  int skyReflect_ = 1;   // the panel's "Reflexo do céu": 0 OFF (the SH sky), 1 the sky's reflection map (skyreflect.hlsl)
  int clouds_ = 2;       // the panel's "Nuvens": 0 OFF, 1 básica (shaders/clouds.hlsli), 2 volumétricas (cloudsvol.hlsli, look 001)
  int glare_ = 0;        // the panel's "Clarão": 0 OFF (default), 1 look 001 (shaders/sunglare.hlsl)
  int shafts_ = 0;       // the panel's "Raios de luz": 0 OFF (default), 1 look 001
  // Atmosfera volumétrica v1 (render/Volumetrics.h, docs/ATMOSPHERE.md): the panel's "Atmosfera" (0 OFF, 1 cinema,
  // 2 alta, 3 desempenho) and "Clima" (index into atmosProfiles_; the map's own profile by its key at first).
  int atmosChoice_ = 2, climaChoice_ = 0;
  std::vector<Volumetrics::Profile> atmosProfiles_;
  std::unordered_map<std::string, MeshId> modelMeshes_;
  std::unordered_map<const std::vector<int>*, PaceLoop> loops_;
  std::vector<SpriteInstance> drawList_;
  std::vector<SpriteInstance> frontCards_;  // casters drawn again over their own spell (Renderer::setFrontCards)
  std::vector<LocalLight> lights_;
  bool modelsPlaced_ = false;
  e2::EdgeStyle placedLook_ = e2::EdgeStyle::Smooth;
  uint32_t modelCount_ = 0, missing_ = 0, fullFrames_ = 0;
  std::chrono::steady_clock::time_point start_ = std::chrono::steady_clock::now();
  bool measuring_ = false, measured_ = false, measurePassed_ = true;
  std::chrono::steady_clock::time_point measureStart_;
  uint32_t measureFrames_ = 0;
  std::vector<double> busy_, gpu_;
  std::vector<std::pair<std::string, double>> passes_;
  std::optional<e2::BoardData> data_;
  std::unique_ptr<e2::Board> board_;
  e2::EdgeStyle look_;
  MapStageOptions options_;
  bool failed_ = false;
  int readyFrames_ = 0;
  glm::vec3 target_{0.0f};
  float boardWidth_ = 1.0f, boardDepth_ = 1.0f;
  float yaw_ = glm::radians(-30.0f), pitch_ = glm::radians(20.0f);
  float distance_ = 0.0f, maxDistance_ = 40.0f;
  bool hpSizesBaked_ = false;  // drawHealthBars: the HP number's glyphs at every step size are in the font atlas
  std::unique_ptr<UiPreload> hudPreload_;            // preloadHud: the HUD's pictures being decoded (gone once all are in)
  std::pair<size_t, size_t> hudPreloaded_{0, 0};
  // The orbit camera: the goal above (yaw_ ... target_), the view on show gliding to it, its limits and steps.
  OrbitView cam_;
  Camera lastCamera_{};
  float aspect_ = 1.0f;
  struct PieceBox {
    float x, z, radius, top;  // a 3D piece's footprint (square around its centre) and top, world units
  };
  std::vector<PieceBox> pieceBoxes_;
  std::vector<ArchPiece> arch_;  // Ember's walls, doors and gates (buildArchitecture)
  static constexpr float kGlide = 10.0f;  // 1/s: ~90% of a move in 0.23 s, no overshoot
  // -8 deg to 89.5 deg (straight down). The eye itself never goes below kMinEyePitch (5 deg, nearly level: it would enter the
  // ground); under it the extra tilt lifts the line of sight instead, so the board sinks to the bottom of the frame and the sky
  // fills the rest (the user's mock-up, 2026-10-19: "baixar todo o mapa desse jeito", the houses of the painting on the horizon).
  static constexpr float kMinPitch = -0.1396f, kMinEyePitch = 0.0873f, kMaxPitch = 1.5621f;
  // ONLY the aldeia looks up to -8 deg (the user, 2026-10-19: "somente a aldeia, só um por vez"); every other map keeps 5 deg.
  bool aldeia() const { return board_ && board_->draft().id == "aldeia"; }
  bool ashenForest() const { return board_ && board_->draft().id == "ashen-forest-crossing"; }
  // The camera as before, on every map, only the tilt reaches 80 deg up into the sky (the user, 2026-10-19: "eu quero como estava
  // antes de agora e com vista para o céu só isso"); the eye still never goes under the ground (eyeView).
  float minPitch() const { return kSkyMinPitch; }
  static constexpr float kSkyMinPitch = -1.3963f;
  float gameHour_ = -1.0f;  // MapStageOptions::gameHour
  int dayCycle_ = 0;  // the panel's "Hora": 0 do mapa, 1 ciclo rápido, 2 ciclo lento, 3.. a fixed hour (cycleHour; --hour N)
  static constexpr float kTiltStep = 0.0873f, kTurnStep = 0.2618f;  // Engine2's bar: 5 deg; 15 deg
  static constexpr float kTurnPerPixel = 0.005f, kZoomStep = 0.85f, kPanPerSecond = 0.9f;
  static constexpr float kMinDistance = 3.0f, kEyeClearance = 0.8f;
  static constexpr float kMaxDistance = 112.42f;  // sqrt(3) x 29.5 (aldeia's board width) x 2.2
  static constexpr float kStartDistance = 25.0f;  // the user's chosen opening zoom (aldeia's old framing 56.3 x 0.85^5)
  static constexpr float kGroupRadius = 8.0f;     // party members within this of each other count as one group

  // Where the camera opens: the party's members standing in play; if they are apart, the middle of the biggest group (the
  // member with the most others within kGroupRadius, then the mean of that group). None in play: nothing (the board's centre).
  std::optional<glm::vec3> partyFocus() const {
    if (!scene_) return std::nullopt;
    std::vector<glm::vec3> at;
    for (size_t i = 0; i < scene_->units.size(); ++i) {
      if (scene_->units[i].side != "player") continue;
      if (battle_) {
        const int b = unitBattle_[i];
        if (b < 0) continue;
        const e2::BattleUnit& u = battle_->units()[static_cast<size_t>(b)];
        if (!u.alive || absent_.contains(u.id)) continue;
      }
      at.push_back(unitWorld(i));
    }
    if (at.empty()) return std::nullopt;
    size_t best = 0, bestCount = 0;
    float bestSpread = 0.0f;
    for (size_t i = 0; i < at.size(); ++i) {
      size_t count = 0;
      float spread = 0.0f;
      for (const glm::vec3& p : at) {
        const float d = glm::distance(glm::vec2(p.x, p.z), glm::vec2(at[i].x, at[i].z));
        if (d <= kGroupRadius) ++count, spread += d;
      }
      if (count > bestCount || (count == bestCount && spread < bestSpread)) best = i, bestCount = count, bestSpread = spread;
    }
    glm::vec3 sum{0.0f};
    for (const glm::vec3& p : at)
      if (glm::distance(glm::vec2(p.x, p.z), glm::vec2(at[best].x, at[best].z)) <= kGroupRadius) sum += p;
    return sum / static_cast<float>(bestCount);
  }
  bool captured_ = false;  // the first view is remembered: the bar's reset goes back to its angle
  bool flat_ = false;      // Tática: the old-school 2D view (the bar's Normal / Tática)
  // Left-button pan (Engine2 cameraRig: HOLD_MS 500, CLICK_SLOP 5).
  struct LeftPress {
    float x = 0.0f, y = 0.0f;
    std::chrono::steady_clock::time_point at;
  };
  std::optional<LeftPress> leftDown_;
  // The map editor's held unit / decoration (editorGrab): which button, where it went down, whether it travelled.
  struct EditorHold {
    uint8_t button = 0;
    float x = 0.0f, y = 0.0f;
    bool moved = false;
  };
  std::optional<EditorHold> editorHold_;
  std::vector<Renderer::GroundMark> editorMarks_;  // the editor's rings (hover, the selected decoration)
  mutable std::optional<glm::vec3> lastPickPoint_;  // where the last pick() met the ground (none: a unit's card)
  bool panning_ = false;
  static constexpr int kHoldMs = 500;
  static constexpr float kClickSlop = 5.0f;
  static constexpr float kDoubleClick = 0.4f;  // seconds between the two clicks that end a turn
  float lastOwnClick_ = -1.0f;                 // scene clock of the last click on the unit being played
  int lastOwnClickUnit_ = -1;
  // Combat (world/Battle.h): the battle, each scene unit's battle unit and live state, the picked unit and its reach.
  std::optional<e2::Battle> battle_;
  std::vector<int> unitBattle_;        // per scene unit: its battle unit, -1 = none
  std::vector<glm::vec2> unitPos_;     // per scene unit: where its feet are now (x, z)
  // The party's unit before each walk this turn, latest last (Cancelar takes them back one at a time).
  struct BeforeWalk {
    size_t unit = 0;
    int x = 0, y = 0, moveUsed = 0;
    bool acted = false;
    std::optional<double> faceDx, faceDy;
    glm::vec2 pos{0.0f};
    int facing = 1;
    uint8_t leftSpawn = 0;
  };
  std::vector<BeforeWalk> undo_;
  std::vector<uint8_t> unitMoved_;     // it has left its spawn: its height is the ground under it
  std::vector<int> unitFacing_;        // 1 right, -1 left, on screen
  std::vector<uint8_t> unitReversed_;  // its art faces left (Engine2 drawnReversed)
  static constexpr int kWalkBack = 0, kWalkFront = 1, kWalkSide = 2;  // Engine2 WalkDir: up the map, down it, along a row
  // A light born on a unit's body (an effect's glow at a caster's or a target's chest) sits in the plane of its camera-facing card,
  // where the card takes none of it and only the ground lights up (the user, 2026-10-18: the same fix as Malrec's casting light, for
  // every light on a unit): it moves 0.45 in front of that card, toward the camera, as Engine2's casting light stands.
  glm::vec3 offCard(const glm::vec3& p, const glm::vec3& forward) const {
    for (size_t i = 0; i < unitCard_.size(); ++i) {
      const CardRect& c = unitCard_[i];
      if (!c.drawn || c.flat) continue;
      const glm::vec3 up = cardUp(c), d = p - c.origin;
      const float along = glm::dot(d, up);
      if (along < 0.0f || along > c.height) continue;
      const glm::vec3 side = d - up * along;
      const float depth = glm::dot(side, -forward);
      if (std::abs(depth) > 0.35f || glm::length(side + forward * depth) > c.width * 0.5f) continue;
      return p - forward * (0.45f - depth);
    }
    return p;
  }
  // Engine2 Battle3D: CAST_LIGHT 9, its PointLight reaching 6 at decay 1.8.
  static constexpr float kCastLight = 9.0f, kCastLightReach = 6.0f, kCastLightDecay = 1.8f;
  // Engine2 CarriedLight STRENGTH: a light of strength 1 (LightDef) is 3.2 in three's units, "as much light again as the scene
  // already has there" one hex out.
  static constexpr float kE2Strength = 3.2f;
  // An Engine2 point light (three's intensity, distance, decay) as Engine2 meant it: relative to the light already on the board.
  // Under real light (lux, the camera's exposure) the casters', familiars' and spells' lights scale with the metered scene light
  // (meterLux_ = Engine2's 3.2): as lux they vanished on a day map, a lantern at noon (the user, 2026-10-18: "não mudou nada").
  LocalLight engine2Light(glm::vec3 at, const std::array<float, 3>& color, float intensity, float distance, float decay) const {
    LocalLight l;
    l.position = at;
    l.range = distance;
    l.decay = decay;
    const float scale = lightingMode_ == 3 ? std::max(meterLux_, 1e-4f) / kE2Strength : 1.0f;
    l.color = glm::vec3(color[0], color[1], color[2]) * intensity * scale;
    return l;
  }
  struct UnitLightDef {  // Engine2 lighting.ts LightDef: linear colour, strength, reach in hex radii, flicker
    std::array<float, 3> color;
    float intensity, radius, flicker;
  };
  inline static const std::unordered_map<std::string, UnitLightDef> kUnitLights = {
      {"familiar", {{0.68f, 0.82f, 1.0f}, 1.5f, 1.0f, 0.08f}},  {"familiar2", {{0.64f, 0.76f, 1.0f}, 1.8f, 2.0f, 0.08f}},
      {"familiar4", {{0.68f, 0.82f, 1.0f}, 1.8f, 2.0f, 0.08f}}, {"familiar3", {{1.0f, 0.08f, 0.1f}, 0.7f, 2.0f, 0.04f}},
      {"swampBlueCalf", {{0.55f, 0.75f, 1.0f}, 1.8f, 3.0f, 0.3f}},
  };
  // Engine2 UnitActor's carried lights, as (x, z, radius) from the feet: Familiar Titã one per body hex (centred on its front
  // row), Familiar Maior three (its body and the two hexes behind it), everyone else one.
  std::vector<glm::vec3> unitLightSpots(size_t ui, const UnitLightDef& d) const {
    const std::string& cls = scene_->units[ui].classId;
    if (cls == "familiar4")
      return {{0.0f, 0.0f, 3.0f}, {-std::sqrt(3.0f) / 2.0f, -1.5f, 2.0f}, {std::sqrt(3.0f) / 2.0f, -1.5f, 2.0f}};
    if (cls == "familiar3" && unitBattle_[ui] >= 0) {
      const e2::BattleUnit& u = battle_->units()[static_cast<size_t>(unitBattle_[ui])];
      std::vector<glm::vec3> spots;
      float front = 0.0f;
      int n = 0;
      const e2::Cell& at = board_->cell(u.x, u.y);
      for (const e2::BattleHex& p : e2::Battle::footprint(u, u.x, u.y)) {
        if (!battle_->inBounds(p.x, p.y)) continue;
        const e2::Cell& c = board_->cell(p.x, p.y);
        const float x = static_cast<float>(c.x - at.x), z = static_cast<float>(c.z - at.z);
        spots.push_back({x, z, d.radius});
        if (p.y == u.y) front += x, ++n;
      }
      for (glm::vec3& s : spots) s.x -= n ? front / static_cast<float>(n) : 0.0f;
      return spots;
    }
    return {{0.0f, 0.0f, d.radius}};
  }
  // Engine2 Battle3D CAST_TINT / MALREC_*: the casting light's colour (sRGB hex, three's linear colour). The heroes cast in their
  // own colour whatever the spell (the user, 2026-10-18): Malrec blue, Salazar yellow, Voss purple.
  static std::array<float, 3> castTint(const std::string& kind, bool heal, const std::string& hero, bool attack) {
    static const std::unordered_map<std::string, uint32_t> kTint = {
        {"fireball", 0xff8a3c}, {"burningHands", 0xff8a3c}, {"frost", 0x9fd6ff}, {"iceStorm", 0x9fd6ff}, {"lightning", 0xb8d2ff},
        {"shock", 0xb8d2ff}, {"causticVenom", 0x86ff7c}, {"minorVenom", 0x86ff7c}, {"poisonBreath", 0x86ff7c}, {"lifeDrain", 0xff5a6e},
        {"magicMissile", 0xc7b4ff}, {"magicMissileV2", 0xc7b4ff}, {"phantasmalForce", 0xc7b4ff}, {"fantomForce", 0xc7b4ff},
        {"webOfDreams", 0xd8c8ff}, {"warp", 0xb48cff}};
    uint32_t h = 0xffe6c0;
    if (attack) h = 0xffd45a;
    else if (hero == "malrec") h = 0x4f9dff;
    else if (hero == "salazar") h = 0xffd45a;
    else if (hero == "voss") h = 0xa066ff;
    else if (heal) h = 0xffd98a;
    else if (const auto t = kTint.find(kind); t != kTint.end()) h = t->second;
    const auto lin = [](uint32_t v) {
      const float x = static_cast<float>(v & 255u) / 255.0f;
      return x <= 0.04045f ? x / 12.92f : std::pow((x + 0.055f) / 1.055f, 2.4f);
    };
    return {lin(h >> 16), lin(h >> 8), lin(h)};
  }
  struct UnitGlow {  // a unit's coloured point of light for the length of its pose
    size_t ui;
    float start, end;
    std::array<float, 3> color;
  };
  std::vector<UnitGlow> glows_;
  struct Walk {
    std::vector<glm::vec2> points;  // the hexes still ahead (feet positions)
    std::vector<e2::BattleHex> hexes;  // the same hexes on the board (a roaming walk stops on the next one)
    float start = 0.0f;            // scene clock when it set off
    float travelled = 0.0f;        // ground covered since (world units): the walk cycle's phase
    int dir = kWalkSide;
    std::string fadeSound;          // its walk cue, faded out when it arrives (a monster's own)
  };
  std::vector<Walk> unitWalk_;
  struct CardRect {  // a unit's card as drawn this frame, for picking
    glm::vec3 origin{0.0f};  // bottom centre
    float width = 0.0f, height = 0.0f;
    bool flat = false, drawn = false;
    // The figure's body inside the card (the user, 2026-10-18: cards in front stole the hexes and the targets behind them):
    // the frame's trimmed picture (the pack's crop box), narrowed to its middle: x = its centre from the origin along
    // `right`, bottom / top above the origin, half = half its width. Picking tests this, never the whole card.
    float bodyX = 0.0f, bodyBottom = 0.0f, bodyTop = 0.0f, bodyHalf = 0.0f;
  };
  std::vector<CardRect> unitCard_;
  bool offHandMode_ = false;         // "Mão Secundária" armed: the off-hand weapon is the one that strikes
  std::map<size_t, e2::Json> npcDialog_;      // battle unit -> its conversation (findConversations)
  int talkAfterWalk_ = -1;                     // the NPC the leader is walking up to, to talk once there
  std::optional<size_t> talkRequest_;          // a conversation asked for (takeTalk)
  std::string floorRequest_;                   // a floor connector taken: the linked map's mission id (wantsFloor)
  std::set<std::string> absent_;               // battle unit ids gone from the start (dead on an earlier visit, fallen on the floor before)
  std::optional<e2::Battle::Exit> finishExit_; // the waypoint the mission was ended on (the result screen's text and its way on)
  bool talked_ = false;                        // a conversation was taken (the battle script's "talk" check)
  bool explores_ = false;                      // an explore mission (it roamed when it opened)
  bool roams_ = false;                         // free roam until an enemy is in sight: an explore mission or any map with fog of war
  bool winDismissed_ = false;        // the end was on offer and the player kept playing (the footer button brings it back)
  bool pickActiveOnResume_ = false;  // set by resume(): pick the active hero once the turn loop first runs
  BattleStatusSheet statusSheet_;                                    // the character status sheet
  e2::InnData innData_;                                              // the Mochila / Equipar over the fight
  bool innDataLoaded_ = false;
  std::optional<e2::InnRules> innRules_;
  std::optional<InventoryScreen> battleInventory_;
  e2::Json inventorySave_;
  bool showLog_ = false, logPinned_ = true, logJustOpened_ = false;  // the combat log in the HUD bar
  bool logFull_ = false, logFullJustOpened_ = false;                 // ... and full screen
  bool logKeep_ = false;
  int hudActive_ = -2;                                               // the active unit the HUD last saw (a new turn closes the log)
  std::map<std::string, Hotbar> hotbars_;                            // each hero's 12 sockets this fight (hotbarOf)
  bool hotbarEdit_ = false;                                          // "Atalhos" open
  int hotbarHero_ = -1;                                              // ... for this battle unit
  float hudRightEdge_ = 1.0e9f;                                      // where the bar's right-hand buttons begin (last frame)
  int selected_ = -1;                     // the picked scene unit
  std::vector<e2::ReachCell> reach_;      // where it can stop
  std::vector<e2::ReachCell> reachPaths_; // every hex it can pass (its routes)
  std::optional<e2::BattleHex> hover_;    // the hex under the cursor
  std::vector<std::pair<int, e2::BattleHex>> targets_;  // scene units the unit whose turn it is can strike, and from where
  int pendingStrike_ = -1;                // the foe to strike once the walk to its hex is over
  int turnUnit_ = -1;                     // the battle unit whose turn has begun
  struct EnemyTurn {
    size_t unit = 0;    // scene unit
    int target = -1;    // scene unit it strikes after its walk
    bool struck = false;
    bool interrupted = false;           // the sentinels' thrusts (Vigília) were played before its blow
    std::vector<e2::BattleStep> steps;  // or the spell it casts after its walk
  };
  std::optional<EnemyTurn> enemyTurn_;
  std::map<int, int> vigilFx_;  // battle unit in its Vigília da Sentinela -> its standing effect (LancerFx::vigilStart)
  std::map<int, int> mirrorFx_;  // battle unit with Imagem Espelhada's illusions -> their standing effect (ConjurerFx::mirrorStart)
  std::map<int, int> statusFx_;  // battle unit x 2 (+1 = disease) -> its poison / disease effect (StatusFx::start)
  std::unordered_set<int> awake_;         // enemies that have seen the party (fog)
  int roamLeader_ = -1;                   // free roam: the scene unit the party follows
  float modeSwitchAt_ = -100.0f;          // free roam's last turn into a fight (the banner)
  std::string result_;                    // "victory" / "defeat" once the battle is over
  bool pauseOpen_ = false, titleRequested_ = false, retryRequested_ = false;
  // Engine2's menus over the battle (EmberMenus.h) and what they act on.
  bool homeRequested_ = false, playtest_ = false;
  // Save / Load in battle: the campaign, this fight's mission and map key, the slot screen and the save it writes.
  e2::Campaign* campaign_ = nullptr;
  std::string missionId_, mapKey_;
  bool questLog_ = false;  // the quest log open over the fight
  bool testMode_ = false, loadRequested_ = false;
  std::optional<SlotScreen> battleSlots_;
  e2::Json slotSave_;
  std::vector<uint8_t> pendingExplored_;  // a resumed fight's explored hexes, for the fog's next relight
  static std::string base64(const std::vector<uint8_t>& bytes) {
    static const char* const k = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    for (size_t i = 0; i < bytes.size(); i += 3) {
      const uint32_t n = static_cast<uint32_t>(bytes[i]) << 16 | (i + 1 < bytes.size() ? static_cast<uint32_t>(bytes[i + 1]) << 8 : 0) |
                         (i + 2 < bytes.size() ? bytes[i + 2] : 0);
      out += k[n >> 18 & 63], out += k[n >> 12 & 63];
      out += i + 1 < bytes.size() ? k[n >> 6 & 63] : '=';
      out += i + 2 < bytes.size() ? k[n & 63] : '=';
    }
    return out;
  }
  static std::vector<uint8_t> unbase64(const std::string& text) {
    std::vector<uint8_t> out;
    uint32_t acc = 0;
    int bits = 0;
    for (const char ch : text) {
      int v = ch >= 'A' && ch <= 'Z' ? ch - 'A' : ch >= 'a' && ch <= 'z' ? ch - 'a' + 26 : ch >= '0' && ch <= '9' ? ch - '0' + 52 : ch == '+' ? 62 : ch == '/' ? 63 : -1;
      if (v < 0) continue;
      acc = acc << 6 | static_cast<uint32_t>(v), bits += 6;
      if (bits >= 8) bits -= 8, out.push_back(static_cast<uint8_t>(acc >> bits & 0xFF));
    }
    return out;
  }
  menu::BattleMenu battleMenu_;
  menu::OptionsDialog menuOptions_;
  menu::BattleMenu::Speed speed_ = menu::BattleMenu::Speed::Normal;  // engine.ts speedMode, "normal" each battle
  std::function<void()> fullscreen_;
  DevGfx* gfx_ = nullptr;
  // Engine2's sound for this battle (export_battle.ts: each unit's cues as Engine2 picks them, the level-up cue, the
  // mission's music); a battle file from before them stays silent.
  std::vector<e2::Json> unitSounds_;  // per battle unit (summoned units have none)
  e2::Json levelUpSound_ = e2::Json::array();
  Music music_;
  std::vector<int> levelSeen_;
  void loadSounds(const std::filesystem::path& file) {
    std::ifstream in(file, std::ios::binary);
    const e2::Json j = in ? e2::Json::parse(in, nullptr, false) : e2::Json();
    if (!j.is_object()) return;
    if (const auto units = j.find("units"); units != j.end() && units->is_array())
      for (const e2::Json& u : *units) unitSounds_.push_back(u.value("sounds", e2::Json::object()));
    if (const auto sounds = j.find("sounds"); sounds != j.end() && sounds->is_object()) levelUpSound_ = sounds->value("levelUp", e2::Json::array());
    music_ = music::fromBattle(j, music_);
  }
  const e2::Json& sounds(size_t battleUnit, const char* key) const {
    static const e2::Json none;
    if (battleUnit >= unitSounds_.size()) return none;
    const auto it = unitSounds_[battleUnit].find(key);
    return it == unitSounds_[battleUnit].end() ? none : *it;
  }
  // audio.ts's players for a list of cues (file, volume, start offset, delay, exclusive or layered).
  static void playCues(const e2::Json& list) {
    if (!list.is_array()) return;
    for (const e2::Json& c : list)
      AudioMixer::get().playSfx(c.value("file", std::string()), c.value("volume", 0.55f), c.value("exclusive", true), c.value("offset", 0.0f), c.value("delay", 0.0f));
  }
  float stepSeconds() const { return speed_ == menu::BattleMenu::Speed::Slow ? 0.36f : speed_ == menu::BattleMenu::Speed::Fast ? 0.12f : kStepSeconds; }
  float actionRate() const { return speed_ == menu::BattleMenu::Speed::Slow ? 0.4f : speed_ == menu::BattleMenu::Speed::Fast ? 1.0f : 0.65f; }
  bool bloomOn_ = true;  // panel "Brilho" (Engine2's full-scene bloom)
  float resultAt_ = 0.0f;                  // when the battle ended (the result screen follows after a breath)
  static constexpr float kResultDelay = 1.8f;
  std::vector<std::pair<int, int>> startLevel_;  // per battle unit: level and XP when the battle began
  std::string spellMode_;                 // the spell being aimed (battle bar), empty = none
  std::vector<e2::BattleHex> spellCells_; // its targets chosen so far
  std::optional<e2::BattleHex> spellArmed_;  // the hex aimed, waiting for Confirmar / Cancelar (Engine2 spellArmed)
  int pendingFoe_ = -1;                      // the foe a strike is staged on, waiting for Confirmar / Cancelar (stageAttack)
  std::vector<e2::BattleHex> spellAim_;   // every hex it may be aimed at
  std::vector<e2::BattleHex> spellZone_;  // where it reaches, target or not (the grid's zone; e2::spellZone)
  std::optional<e2::BattleHex> spellPreviewAim_;  // a line / cone spell: the aim at the nearest foe it reaches (cursor off the board)
  mutable std::string hitsKey_;              // aimHits' cache: the spell, caster and hex it was worked out for
  mutable std::vector<size_t> hits_;
  bool attackMode_ = false;               // "Atacar": only the foes it can strike are marked
  bool showTurnOrder_ = false;
  std::string tip_;                       // why a click did nothing (Engine2's tip line)
  float tipAt_ = -10.0f;
  std::vector<OneShot> unitShot_;         // per scene unit: a one-shot pose over its own
  std::vector<float> unitDiedAt_;         // per scene unit: when it fell (-1 = alive)
  // The struck body's reaction (2026-10-17, the hit react; the impact's light and particles are CombatFx's): the card
  // is knocked back along the blow, squashed a little, and comes back; a heavy blow knocks it further, dips it and it
  // staggers once before it settles. Drawn only (the board position never moves).
  struct Recoil {
    float start = -10.0f;
    glm::vec3 dir{0.0f};  // the blow's way on the ground
    bool heavy = false;
  };
  std::vector<Recoil> unitRecoil_;
  // Salto Devastador (2026-10-18): the card flies from `from` to `to` on an arc `height` high over [start, end] while the
  // board position is already the landing (the rules moved it at once). Drawn only.
  struct Leap {
    float start = -10.0f, end = -10.0f;
    glm::vec2 from{0.0f}, to{0.0f};
    float height = 0.0f;
  };
  std::vector<Leap> unitLeap_;
  glm::vec3 leapOffset(size_t ui, float time) const {
    if (ui >= unitLeap_.size()) return glm::vec3(0.0f);
    const Leap& l = unitLeap_[ui];
    if (time < l.start || time > l.end || l.end <= l.start) return glm::vec3(0.0f);
    const float t = (time - l.start) / (l.end - l.start);
    const float s = t * t * (3.0f - 2.0f * t);  // eased: off the ground hard, down hard
    const glm::vec2 back = (l.from - l.to) * (1.0f - s);
    const float ground = (groundAt(l.from.x, l.from.y) - groundAt(l.to.x, l.to.y)) * (1.0f - s);
    return {back.x, ground + l.height * 4.0f * s * (1.0f - s), back.y};
  }
  // Where the card is pushed now and how much it is squashed (1 = not).
  glm::vec3 recoilOffset(size_t ui, float time, float height, float& squash) const {
    squash = 1.0f;
    if (ui >= unitRecoil_.size()) return glm::vec3(0.0f);
    const Recoil& r = unitRecoil_[ui];
    const float t = (time - r.start) * actionRate();
    if (t < 0.0f || t > (r.heavy ? 0.9f : 0.5f)) return glm::vec3(0.0f);
    // An impulse response: out to its peak at tau, back without overshooting; a heavy blow's stagger is a second,
    // smaller push a beat later.
    const auto bump = [](float x, float tau) { return x <= 0.0f ? 0.0f : (x / tau) * std::exp(1.0f - x / tau); };
    const float reach = r.heavy ? 0.3f : 0.13f;
    const float k = r.heavy ? bump(t, 0.085f) + 0.35f * bump(t - 0.24f, 0.09f) : bump(t, 0.055f);
    squash = 1.0f - (r.heavy ? 0.06f : 0.035f) * std::min(k, 1.0f);
    return r.dir * (reach * k) + glm::vec3(0.0f, r.heavy ? -0.04f * height * std::min(k, 1.0f) : 0.0f, 0.0f);
  }
  std::optional<Exchange> exchange_;
  struct Popup {
    size_t unit = 0;
    std::string text;
    ImU32 color = 0;
    float start = 0.0f;
    bool big = false;
  };
  std::vector<Popup> popups_;
  e2::Mulberry32 rng_{1};
  static constexpr float kImpactShare = 0.55f;   // a strike lands this far into the attack pose
  static constexpr float kExchangeGap = 0.15f;   // a breath between the strike and the counter
  static constexpr float kCorpseSeconds = 1.5f;  // the fallen lie this long after their death pose
  static constexpr float kPopupSeconds = 1.2f;
  glm::vec2 mouse_{-1.0f};
  SDL_WindowID mouseWindow_ = 0;
  float now_ = 0.0f;  // this frame's scene clock
  static constexpr float kStepSeconds = 0.22f;      // Engine2 STEP_SECONDS: one hex of walking
  static constexpr float kWalkStride = 3.5f;        // ground one full stride covers, world units, every unit alike (0.44 s at 0.22 s a hex)
  static constexpr float kBorderInner = 0.47f / 0.5f;  // Engine2 BORDER_INNER: a hex border's inner edge
  std::vector<Renderer::GroundMark> groundMarks_;  // this frame's battle grid (collectMarks)
  std::vector<Renderer::GridCell> gridCells_;      // ... for the numbered looks (collectGrid)
  int gridLook_ = 2;  // panel "Grade": 0 OFF, 1 antiga (the marks), 2.. = look 001.. (looks/grid/README.md)
  int cardStance_ = 1;  // panel "Cards": 0 em pé (vertical), 1 para a câmera (leaning back with the tilt, upright on screen)
  int barsFade_ = 1;    // panel "Vida perto": 0 OFF (bars always shown), 1 the bars of units near the camera fade away
  static constexpr float kBarsFadeFrom = 0.30f, kBarsFadeTo = 0.45f;  // the card's share of the screen's height
  static constexpr float kAirCardHeight = 2.0f;                         // world units: a unit's card, the yardstick of the particles' fade
  // Engine2 DIR_ACTION_WALK: sprites whose walks are drawn for each side, never mirrored while moving.
  inline static const std::unordered_set<std::string> kDirectionalWalk = {"aldric", "defaultLancer", "lancer", "sandoval", "theButcher",
      "familiar2", "familiar3", "cultist-v2", "militia-v2", "salazar", "cobalt-blue-deer", "neera"};
  float homeYaw_ = 0.0f, homePitch_ = 0.0f, homeDistance_ = 0.0f;
  glm::vec3 homeTarget_{0.0f};
  std::chrono::steady_clock::time_point lastFrame_ = std::chrono::steady_clock::now();
  glm::vec3 boardCenter_{0.0f};
  float boardRadius_ = 0.0f;
  uint32_t layers_ = 0xFFFFFFFFu;
  // Fog of war and the party.
  std::optional<e2::FogOfWar> fog_;
  e2::FogMask fogMask_;
  bool fogReady_ = false, replayed_ = false, fogUpdatedThisFrame_ = false, occludersAdded_ = false;
  std::vector<uint8_t> fogStates_;  // per hex, as of the mask on show (main thread)
  // The hunt for the vanishing wolf (the user, 2026-10-19): an enemy going hidden or back on show is logged once with what decided it.
  std::vector<uint8_t> shownLast_;  // per scene unit: 0 unknown, 1 shown, 3 hidden by fog, 2 no clip (logged)
  void noteShown(size_t ui, bool shown, int c, int r, size_t i) {
    if (shownLast_.size() != scene_->units.size()) shownLast_.assign(scene_->units.size(), 0);
    const uint8_t now = shown ? 1 : 3;
    if (shownLast_[ui] == now) return;
    if (shownLast_[ui] != 0)
      logInfo("fog: {} at ({}, {}) {}: sight state {}, lit {}, walking {}", scene_->units[ui].name, c, r, shown ? "on show again" : "hidden",
              i < fogStates_.size() ? static_cast<int>(fogStates_[i]) : -1, i < fogLit_.size() ? static_cast<int>(fogLit_[i]) : -1, !unitWalk_[ui].points.empty());
    shownLast_[ui] = now;
  }
  std::vector<uint8_t> fogLit_;     // per hex: kFogVisible where the mask on show is lit (in sight, still lingering, beside the party)
  // Whether a hex (its index) is drawn lit on the mask on show: an enemy standing on it is drawn, with its bar and locks.
  bool litHex(size_t i) const { return i < fogLit_.size() && fogLit_[i] == e2::kFogVisible; }
  // Whether a battle unit is on show: always without fog and for the party's own, else its hex lit.
  bool unitOnShow(const e2::BattleUnit& t) const {
    if (!fog_ || noFog_ || t.side == "player") return true;
    return litHex(static_cast<size_t>(e2::indexOf(board_->layout(), t.x, t.y)));
  }
  // The sight job (runFogJob): while fogBusy_, only the worker touches fog_, fogJobEyes_, fogNext_, fogJobMs_.
  JobSystem& jobs_;
  JobCounter fogJob_;
  bool fogBusy_ = false, sightLogged_ = false;
  std::chrono::steady_clock::time_point fogClockStart_ = std::chrono::steady_clock::now();  // the fog's linger clock
  double fogLingerCheck_ = 0.0;
  std::vector<std::vector<e2::HexPoint>> fogQueue_, fogJobEyes_;  // party positions waiting / being relit, in order
  e2::FogMask fogNext_;
  std::vector<uint16_t> fogTexels_, fogNextTexels_;  // fogMask_ / fogNext_ packed for the GPU (Renderer::fogTexels)
  double fogJobMs_ = 0.0;
  std::vector<double> fogDeliverMs_;  // main-thread cost of taking a finished job's result
  uint64_t maskVersion_ = 0;
  std::vector<e2::HexPoint> path_, lastParty_;
  int hero_ = -1;
  glm::vec3 heroAt_{0.0f};
  e2::HexPoint heroHex_;
  float meanGround_ = 0.0f, boardTop_ = 0.0f, groundLow_ = 0.0f;
  uint32_t fogLook_ = 8;  // index into fogLooks(): look 009 (008 with the sky and painting left clear) unless --fog-look says otherwise
  int lightingMode_ = 3;   // 0 old prototype day, 1 Engine2 presets, 2 physical sky (Engine2 strengths), 3 real (lux + camera)
  // Light mode "real": the camera's exposure, adapted towards the scene's (EV100), and what was metered.
  float adaptedEv_ = std::numeric_limits<float>::quiet_NaN();
  mutable float meterLux_ = 1.0f;
  mutable bool moonNight_ = false;  // light mode real: the key light is the moon, outdoors ("Luar" applies)
  float exposure_ = 1.0f;  // the exposure in use (last frame's), for the painted flames' glow
  int moonPath_ = 0;       // moonPlacement()
  int moonLight_ = 1;      // the panel's "Luar" (light mode real): 0 físico (kFullMoonLux), 1 cinema (kCinemaMoonLux)
  static constexpr int kStarLooks = 4;  // saved star looks (looks/stars/README.md); 4 = the real sky (render/StarSky.h)
  int starLook_ = 4;
  StarSky starSky_;  // content/sky/stars.json: the real stars, turned to the hour
  int moonPhase_ = -1;  // index in e2::kMoonPhases; -1 = Engine2's default (the full moon's art, the night's strength)
  // The disc's angular radius (degrees), chosen in the panel. 2.7 = Engine2's disc as seen from its normal view; the
  // user found it "millions of km away" (2026-10-15), so the panel starts at the large one.
  static constexpr float kMoonSizes[4] = {2.7f, 6.0f, 10.0f, 15.0f};
  int moonSize_ = 0;  // panel "Tamanho da lua": pequena (2.7 deg radius, 10 x the real) by default: média and grande read as a
                      // "super moon" with the cinema glow (the user, 2026-10-19)
  int sunSize_ = 1;   // panel "Sol": 0 real (0.26 deg), 1 cinema (the moon's small disc + corona; default), 2 look 001; --sun-disc N
  std::array<int, 3> fogCounts_{};
  int holes_ = 0, holeHexes_ = 0, worstHoles_ = 0;
  std::vector<std::pair<int, int>> lastHoles_;
  bool holesLogged_ = false;
  uint32_t hiddenUnits_ = 0;
  bool noFog_ = false;  // "sem névoa" in the panel: no mist and no fog of war (sight still runs underneath)
  bool moonOff_ = false, starsOff_ = false;  // "sem lua no céu" / "sem estrelas" in the panel
  double lastFogMs_ = 0.0;
  std::vector<double> fogUpdateMs_;
  std::map<std::string, std::pair<double, size_t>> passWorst_;
};

}  // namespace ember
