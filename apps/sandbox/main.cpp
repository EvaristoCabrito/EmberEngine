// Sandbox: opens the game window and runs the renderer.
//   Sandbox.exe                         the whole screen (F1 toggles the performance overlay; --windowed or --size: a window)
//   Sandbox.exe --capture out.png       off-screen: renders, saves a PNG, exits
//                [--size 1920x1080] [--frames 3] [--time 1.5] [--no-validation]
//   Sandbox.exe --stream test.pak [--verify <content dir>] [--io-depth N]
//        Streaming test: mounts the archive, requests every asset in it in one frame and streams them
//        while rendering, then reports load time, throughput and main-thread frame times. With
//        --verify, every loaded asset is compared byte for byte with its source file. --io-depth sets
//        how many reads the disk gets at once.
//        With --capture it saves the frame once the test ends and exits with code 3 if it failed;
//        in a window, R runs it again.
//   Sandbox.exe --battle --meshes meshes.pak --sprites battle.pak [--budget MB] [--content all|3d|cards] [--cost-split 0|1|2]
//        (--cost-split, measurement only: 1 task shaders only, 2 mesh shaders without rasterisation;
//         --unpaced: frames back to back instead of 60 Hz, so GPU times are not stretched by power saving)
//        M3 exit test: a 128 x 104-hex battle at 10x the measured Ember densities, timed in three
//        camera phases (see apps/sandbox/BattleStage.h). With --capture it exits with code 3 if it failed.
//   Sandbox.exe --compare <pakA> <meshA asset> <pakB> <meshB asset> <out dir>
//        Proxy-quality images: mesh A (reference = full detail, original = engine LOD) vs mesh B (proxy), at
//        several widths; tools/compare_meshes.py measures silhouette and normal error.
//   Sandbox.exe --map <Engine2 map.json> --ground <ground.pak> [--look smooth|hard|tabletop] [--board <board.json>] [--engine2 <public dir>]
//        D1 port: a real Ember map from Engine2, its board and ground (docs/ENGINE2_AUDIT.md).
//        [--scene <scene.json> --sprites <battle.pak> --meshes <props.pak>]: its decorations, 3D models and units.
//        Captures: [--zoom F] [--turn DEG] [--tilt DEG] [--offset X Z] (board widths/depths) [--layers MASK]
//        (lighting layers on: 1 sun, 2 shadows, 4 indirect; shaders/lighting.hlsli) [--sun AZIMUTH ELEVATION] (degrees)
//        [--moving-light] [--spell-light] (test lights; 8 in --layers = local lights)
//        [--fx-test KIND|all] (the hero casts a spell's visual at the nearest enemy every 3 s: magicMissile, lightning,
//        frost, cureMinor, longShot, piercing, shock, doubleStrike; visuals only) [--tatica] (opens in the 2D view)
//        [--spells-menu-open] (the Magias window opened, every list unfolded)
//        [--measure SECONDS]: camera turn + zoom with everything animating; frame times; exit 3 if any frame > 16.7 ms.
//        Fog of war (R7): [--fog 0|1] (default: the map's own flag) [--scout] (the first party unit walks to the
//        farthest enemy and back). Pixel checks: [--scene-time T] (scene clock pinned) [--no-ui] [--debug-view 3]
//        (the fog mask each surface read), then tools/scripts/check_fog.py. [--volume 0]: no volumetric fog;
//        [--fog-look N]: which saved fog look (render/FogLooks.h; in a window, the Map panel's dropdown).
//        [--grid-look N]: the battle grid (panel "Grade"): 0 OFF, 1 the old marks, 2 look 001 (looks/grid/README.md).
//        [--cards N]: 0 vertical cards, 1 (default) cards leaning back with the camera's tilt, upright on screen (panel "Cards").
//        [--water N]: the board's water (panel "Água"): 0 Engine2's, 1 (default) look 001 "ember" (looks/water/README.md).
//        [--sky-reflect N]: 0 OFF, 1 (default) the sky's reflection map on water and specular surfaces (panel "Reflexo do céu").
//        [--clouds N]: 0 OFF, 1 the basic cloud sheet, 2 (default) the volumetric clouds, look 001: sky, reflections, shadows (panel "Nuvens").
//        [--glare N] [--shafts N]: 0 (default) OFF, 1 the sun's lens glare / its crepuscular rays, look 001 (panels "Clarão", "Raios de luz").
//        [--moonlight N]: light mode real, at night: 0 the physical moon (0.5 lux), 1 (default) the cinema moon (panel "Luar").
//        [--sun-disc N]: the sun's disc in the sky: 0 the real 0.26 deg radius, 1 (default) the moon's small disc with a corona (panel "Sol").
//        [--bars-fade N]: 0 health bars always shown, 1 (default) the bars and numbers of units near the camera fade (panel "Vida perto").
//        [--lighting 0|1|2|3]: old prototype day / Engine2's presets / physical sky with Engine2's strengths / real
//        (default: lux, physical camera, eye adaptation, inverse-square lamps; also in the panel as "Luz").
//        Night maps: [--moon-phase NAME] (Engine2's moonPhase.ts: new .. waningCrescent, blood) [--moon-path 0|1|2]
//        (where the disc and its light stand: MapStage.h moonPlacement) [--star-look N] (looks/stars).
//        [--heroes <roster.json>]: every unit's poses (tools/scripts/engine2/export_heroes.ts); the map's own units
//        play the pose chosen in the panel ("Animação"). [--battle-rules <map.battle.json>]: combat (tools/scripts/
//        engine2/export_battle.ts): pick a unit, its reach, walk it (MapStage.h). In a window
//        every setting is a dropdown / checkbox in the Map panel (no keyboard shortcuts).
//   Sandbox.exe --meshes meshes.pak [--grid N]
//        Mesh scene (M3 bring-up): every cooked mesh in the archive on an N x N grid, drawn GPU-driven.
//   Sandbox.exe --sprites roster.pak [--budget MB]
//        Sprite streaming test (M2): plays every clip of every character, a few at a time, under a GPU
//        budget (default 512 MB) smaller than the roster. With --capture it runs at 60 Hz, saves the
//        frame once every clip has played and exits with code 3 if it failed.
// A capture exits with code 2 if the Vulkan validation layer reported any error.

#include <SDL3/SDL.h>
#include <imgui.h>

#include <algorithm>
#include <atomic>
#include <charconv>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <future>
#include <memory>
#include <string>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "assets/AssetManager.h"
#include "core/Jobs.h"
#include "core/Log.h"
#include "debug/PerfOverlay.h"
#include "platform/IoQueue.h"
#include "platform/Window.h"
#include "render/DebugUi.h"
#include "render/Renderer.h"
#include "render/SpriteSystem.h"
#include "rhi/Swapchain.h"
#include "rhi/UploadQueue.h"
#include "BattleStage.h"
#include "MeshCompare.h"
#include "MapStage.h"
#include "HeroHandoff.h"
#include "LoadingScreens.h"
#include "TitleStage.h"
#include "media/AudioMixer.h"
#include "media/VideoPlayer.h"
#include "OverworldStage.h"
#include "BattleTalk.h"
#include "NpcTalk.h"
#include "GameMusic.h"
#include "MapEditorStage.h"
#include "world/Locais.h"
#include "TestModeStage.h"
#include "world/Campaign.h"
#include "world/Interlude.h"
#include "world/Overworld.h"
#include "MeshStage.h"
#include "SpriteParade.h"
#include "Stage.h"
#include "rhi/VulkanContext.h"

namespace {

using namespace ember;
using Clock = std::chrono::steady_clock;

constexpr double kFrameBudgetMs = 1000.0 / 60.0;

struct Options {
  std::string capture;
  std::string stream;
  std::string verify;
  std::string sprites;
  uint64_t spriteBudgetMb = 512;
  std::string meshes;
  uint32_t meshGrid = 8;
  bool battle = false;
  std::string battleContent = "all";
  uint32_t costSplit = 0;
  bool unpaced = false;
  float mapZoom = 1.0f, mapTurn = 0.0f, mapTilt = 0.0f, mapOffsetX = 0.0f, mapOffsetZ = 0.0f;
  uint32_t lightingLayers = 0xFFFFFFFFu;
  float sunAzimuth = 0.0f, sunElevation = -1.0f;
  bool movingLight = false, spellLight = false;
  std::string fxTest;
  std::string battleScript;  // --battle-script: MapStage::runScript's steps
  std::string cutscene;      // --cutscene: on-map cutscene scripts played on the map (MapStage::runCutsceneQueue)
  float cutsceneAuto = 0.0f; // --cutscene-auto SECONDS: its dialogue lines dismissed by themselves
  // --promote Name=class[:level],...: a hero's promotion ("warlock" / "elementalist") and, optionally, its level for the
  // fight (tests of the promotions' spells; the campaign reads the save's promotions instead).
  std::map<std::string, std::string> promote;
  std::map<std::string, int> promoteLevel;
  // --arrived-from <mission id>: the party comes in by the floor connector back to that map (MapStageOptions::arrivedFrom);
  // --leader <hero>: that hero walks a free-roam map in the authored walker's place (MapStageOptions::leader).
  std::string arrivedFrom, leader;
  // --defeated-spawns <id,id,...>: those enemy / neutral spawns dead from an earlier visit (MapStageOptions::defeatedSpawns);
  // --knocked-out <Hero,Hero>: those heroes fell on the floor before and stay out (carried HP 0 + preserveKnockouts).
  std::string defeatedSpawns, knockedOut;
  // --formation "Kael,,Neera,...": the Party screen's 19 board cells for a mission capture (front to back; empty = an empty cell).
  std::string formation;
  bool tatica = false;
  bool spellsMenuOpen = false;
  float measureSeconds = 0.0f;
  // --capture-sequence FROM:EVERY:COUNT (with --capture <prefix>): inside the one run, from frame FROM every EVERY
  // frames, COUNT frames are saved as <prefix>-NNN.png (temporal checks: ghosting, shimmer, history; with --measure
  // the camera turns while they are taken). The final --capture still happens.
  int sequenceFrom = -1, sequenceEvery = 1, sequenceCount = 0;
  int fog = -1;
  bool scout = false, noUi = false, volume = true;
  bool windowed = false;  // --windowed or --size: a window instead of the whole screen
  int fogLook = 8;
  int gridLook = 2;  // --grid-look N: the battle grid (panel "Grade"): 0 OFF, 1 the old marks, 2 look 001
  int cardStance = 1;  // --cards N: 0 vertical cards, 1 leaning back with the camera's tilt (panel "Cards")
  int barsFade = 1;    // --bars-fade N: 0 bars always shown, 1 the bars of units near the camera fade (panel "Vida perto")
  int actionCamera = 1;  // --action-camera N: 0 OFF, 1 cinema (panel "Câmera de ação": the shots of every blow, shot and spell)
  int floorImage = 1;  // --floor N: maps whose draft lays its painting under the board: 0 OFF, 1 on (panel "Chão do vão")
  int windFog = 1;     // --wind-fog N: maps whose draft names a wind fog: 0 OFF, 1 on (panel "Névoa do vento")
  int windowLight = -1;  // --window-light N: indoor maps' "Luz das janelas": -1 as the map says, 0 OFF, 1 real
  int atmos = -1;  // --atmos N: Atmosfera volumétrica v1 at the start (0 OFF, 1 cinema, 2 alta, 3 desempenho; -1 the map's own)
  int background = -1;  // --background N: the panel's "Fundo" (-1 as the map says, 0 the painting, 1 the sky, 2 OFF = black, 3 the painting up to
                        // the horizon + the sky, 4 the painting aligned to the sky)
  int waterLook = 0;    // --water N: the panel's "Água" (0 the original = the default, 1 look 001 "ember", 2 look 002 "correnteza",
                        // 3 look 003 "decente")
  int water3dLook = 0;  // --water3d N: the panel's "Água 3D" (0 the original = the default .. 4 "rio decente" = look 003)
  int skyReflect = 1;   // --sky-reflect N: the panel's "Reflexo do céu" (0 OFF, 1 the sky's reflection map)
  int clouds = 2;       // --clouds N: the panel's "Nuvens" (0 OFF, 1 básica, 2 volumétricas)
  int glare = 0;        // --glare N: the panel's "Clarão" (0 OFF, 1 look 001)
  int shafts = 0;       // --shafts N: the panel's "Raios de luz" (0 OFF, 1 look 001)
  float gameHour = -1.0f;  // --game-hour H: the sky follows this hour of the game clock (0..23), as a campaign fight does
  int moonLight = 1;    // --moonlight N: the panel's "Luar" (0 físico, 1 cinema)
  int hour = 0;         // --hour N: the panel's "Hora" (0 do mapa, 1 ciclo rápido, 2 ciclo lento, 3..10 amanhecer .. madrugada)
  int sunDisc = 1;      // --sun-disc N: the panel's "Sol" (0 real, 1 cinema, 2 look 001)
  bool mist = true;    // --mist 0: the fog of war's mist bank off (its mask and the volumetric atmosphere stay)
  int lighting = 3;
  float sceneTime = -1.0f;
  std::string moonPhase;
  int moonPath = 0;
  int starLook = 4;  // --star-look N: 1..3 the saved random fields, 4 (default) the real sky (content/sky/stars.json)
  std::string heroes;
  std::string battleRules;
  std::string devGfx;  // captures: Dev Controls' switches from this file (DevGfx.h) applied to the map
  std::string editorScript;  // captures: clicks on the Map Editor's board (MapEditorStage::previewEdits)
  // The title screen (Engine2's, TitleStage.h): its mission list is every imported scene in `scenes` whose map is in
  // `maps` (default: Engine2's maps/ember beside its public folder).
  bool title = false;
  std::string saveBank;     // the campaign bank file (default: %LOCALAPPDATA%\Ember\ember-save-bank.json)
  std::string mapUi;        // captures on the world map: a panel opened (OverworldStage::preview)
  int titlePick = -1;       // captures: that slot chosen (0-based) on the --title-screen slots
  std::string titleScreen;  // captures: "new" / "load" open the save slots ("" the title itself); "help[:tab[:scroll]]" /
                            // "options": Como jogar / Opções over the title
  std::string lang;         // captures: the interface, dialogues and subtitles in "pt" / "en" (Opções' languages, not saved)
  std::string resume;       // captures: a saved fight (BattleSnapshot JSON) resumed on the battle (MapStage::resume)
  bool curtain = false;     // captures: Engine2's loading curtain over the battle / world map as it loads (LoadingScreens.h)
  std::string scenes = "build/engine2-scenes", maps;
  uint32_t debugView = 0;
  std::string scene;
  std::string map, ground, look = "smooth", boardData = "content/engine2/board.json", gameAssets = "content/game/public";
  std::vector<std::string> compare;  // pakA assetA pakB assetB outDir  // measurement: frames back to back, so the GPU never idles and keeps its full clock
  bool budgetSet = false;
  uint32_t width = 1600, height = 900;
  int frames = 3;
  uint32_t ioDepth = 64;
  float time = 1.0f;
  bool validation = true;
};

// "1920x1080"
void parseSize(std::string_view text, Options& o) {
  const size_t x = text.find('x');
  const bool ok = x != std::string_view::npos &&
                  std::from_chars(text.data(), text.data() + x, o.width).ec == std::errc{} &&
                  std::from_chars(text.data() + x + 1, text.data() + text.size(), o.height).ec == std::errc{};
  if (!ok || !o.width || !o.height) fatal(std::format("Bad --size {}", text));
}

Options parse(int argc, char** argv) {
  Options o;
  for (int i = 1; i < argc; ++i) {
    const std::string_view arg = argv[i];
    const bool hasValue = i + 1 < argc;
    if (arg == "--capture" && hasValue) o.capture = argv[++i];
    else if (arg == "--stream" && hasValue) o.stream = argv[++i];
    else if (arg == "--verify" && hasValue) o.verify = argv[++i];
    else if (arg == "--sprites" && hasValue) o.sprites = argv[++i];
    else if (arg == "--meshes" && hasValue) o.meshes = argv[++i];
    else if (arg == "--battle") o.battle = true;
    else if (arg == "--map" && hasValue) o.map = argv[++i];
    else if (arg == "--ground" && hasValue) o.ground = argv[++i];
    else if (arg == "--scene" && hasValue) o.scene = argv[++i];
    else if (arg == "--look" && hasValue) o.look = argv[++i];
    else if (arg == "--zoom" && hasValue) o.mapZoom = std::max(0.1f, static_cast<float>(std::atof(argv[++i])));
    else if (arg == "--turn" && hasValue) o.mapTurn = static_cast<float>(std::atof(argv[++i]));
    else if (arg == "--tilt" && hasValue) o.mapTilt = static_cast<float>(std::atof(argv[++i]));
    else if (arg == "--offset" && i + 2 < argc) o.mapOffsetX = static_cast<float>(std::atof(argv[++i])), o.mapOffsetZ = static_cast<float>(std::atof(argv[++i]));
    else if (arg == "--sun" && i + 2 < argc) o.sunAzimuth = static_cast<float>(std::atof(argv[++i])), o.sunElevation = static_cast<float>(std::atof(argv[++i]));
    else if (arg == "--moving-light") o.movingLight = true;
    else if (arg == "--game-hour" && hasValue) o.gameHour = static_cast<float>(std::atof(argv[++i]));
    else if (arg == "--measure" && hasValue) o.measureSeconds = static_cast<float>(std::atof(argv[++i]));
    else if (arg == "--spell-light") o.spellLight = true;
    else if (arg == "--fx-test" && hasValue) o.fxTest = argv[++i];
    else if (arg == "--arrived-from" && hasValue) o.arrivedFrom = argv[++i];
    else if (arg == "--defeated-spawns" && hasValue) o.defeatedSpawns = argv[++i];
    else if (arg == "--knocked-out" && hasValue) o.knockedOut = argv[++i];
    else if (arg == "--formation" && hasValue) o.formation = argv[++i];
    else if (arg == "--leader" && hasValue) o.leader = argv[++i];
    else if (arg == "--promote" && hasValue) {
      std::string list = argv[++i];
      for (size_t at = 0; at <= list.size();) {
        size_t comma = list.find(',', at);
        if (comma == std::string::npos) comma = list.size();
        const std::string item = list.substr(at, comma - at);
        at = comma + 1;
        const size_t eq = item.find('=');
        if (eq == std::string::npos) continue;
        std::string cls = item.substr(eq + 1);
        if (const size_t colon = cls.find(':'); colon != std::string::npos) {
          o.promoteLevel[item.substr(0, eq)] = std::atoi(cls.c_str() + colon + 1);
          cls.resize(colon);
        }
        o.promote[item.substr(0, eq)] = cls;
      }
    }
    else if (arg == "--battle-script" && hasValue) {
      // "@<file>": the steps read from a UTF-8 file (the command line reaches argv in the ANSI code page: "Cão" never matched)
      o.battleScript = argv[++i];
      if (o.battleScript.starts_with('@')) {
        std::ifstream in(std::filesystem::path(o.battleScript.substr(1)), std::ios::binary);
        o.battleScript.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
        while (!o.battleScript.empty() && (o.battleScript.back() == '\n' || o.battleScript.back() == '\r')) o.battleScript.pop_back();
      }
    }
    else if (arg == "--tatica") o.tatica = true;
    else if (arg == "--spells-menu-open") o.spellsMenuOpen = true;
    else if (arg == "--fog" && hasValue) o.fog = std::atoi(argv[++i]);
    else if (arg == "--scout") o.scout = true;
    else if (arg == "--volume" && hasValue) o.volume = std::atoi(argv[++i]) != 0;
    else if (arg == "--fog-look" && hasValue) o.fogLook = std::atoi(argv[++i]);
    else if (arg == "--grid-look" && hasValue) o.gridLook = std::atoi(argv[++i]);
    else if (arg == "--cards" && hasValue) o.cardStance = std::atoi(argv[++i]);
    else if (arg == "--bars-fade" && hasValue) o.barsFade = std::atoi(argv[++i]);
    else if (arg == "--action-camera" && hasValue) o.actionCamera = std::atoi(argv[++i]);
    else if (arg == "--floor" && hasValue) o.floorImage = std::atoi(argv[++i]);
    else if (arg == "--wind-fog" && hasValue) o.windFog = std::atoi(argv[++i]);
    else if (arg == "--window-light" && hasValue) o.windowLight = std::atoi(argv[++i]);
    else if (arg == "--atmos" && hasValue) o.atmos = std::atoi(argv[++i]);
    else if (arg == "--background" && hasValue) o.background = std::atoi(argv[++i]);
    else if (arg == "--water" && hasValue) o.waterLook = std::atoi(argv[++i]);
    else if (arg == "--water3d" && hasValue) o.water3dLook = std::atoi(argv[++i]);
    else if (arg == "--sky-reflect" && hasValue) o.skyReflect = std::atoi(argv[++i]);
    else if (arg == "--clouds" && hasValue) o.clouds = std::atoi(argv[++i]);
    else if (arg == "--glare" && hasValue) o.glare = std::atoi(argv[++i]);
    else if (arg == "--shafts" && hasValue) o.shafts = std::atoi(argv[++i]);
    else if (arg == "--moonlight" && hasValue) o.moonLight = std::atoi(argv[++i]);
    else if (arg == "--hour" && hasValue) o.hour = std::atoi(argv[++i]);
    else if (arg == "--sun-disc" && hasValue) o.sunDisc = std::atoi(argv[++i]);
    else if (arg == "--mist" && hasValue) o.mist = std::atoi(argv[++i]) != 0;
    else if (arg == "--lighting" && hasValue) o.lighting = std::atoi(argv[++i]);
    else if (arg == "--scene-time" && hasValue) o.sceneTime = static_cast<float>(std::atof(argv[++i]));
    else if (arg == "--moon-phase" && hasValue) o.moonPhase = argv[++i];
    else if (arg == "--moon-path" && hasValue) o.moonPath = std::atoi(argv[++i]);
    else if (arg == "--star-look" && hasValue) o.starLook = std::atoi(argv[++i]);
    else if (arg == "--heroes" && hasValue) o.heroes = argv[++i];
    else if (arg == "--battle-rules" && hasValue) o.battleRules = argv[++i];
    else if (arg == "--cutscene" && hasValue) o.cutscene = argv[++i];
    else if (arg == "--cutscene-auto" && hasValue) o.cutsceneAuto = static_cast<float>(std::atof(argv[++i]));
    else if (arg == "--dev-gfx" && hasValue) o.devGfx = argv[++i];
    else if (arg == "--editor-script" && hasValue) o.editorScript = argv[++i];
    else if (arg == "--title") o.title = true;
    else if (arg == "--save-bank" && hasValue) o.saveBank = argv[++i];
    else if (arg == "--map-ui" && hasValue) o.mapUi = argv[++i];
    else if (arg == "--title-pick" && hasValue) o.titlePick = std::atoi(argv[++i]);
    else if (arg == "--title-screen" && hasValue) o.titleScreen = argv[++i], o.title = true;
    else if (arg == "--lang" && hasValue) o.lang = argv[++i];
    else if (arg == "--resume" && hasValue) o.resume = argv[++i];
    else if (arg == "--scenes" && hasValue) o.scenes = argv[++i];
    else if (arg == "--maps" && hasValue) o.maps = argv[++i];
    else if (arg == "--debug-view" && hasValue) o.debugView = static_cast<uint32_t>(std::atoi(argv[++i]));
    else if (arg == "--no-ui") o.noUi = true;
    else if (arg == "--curtain") o.curtain = true;
    else if (arg == "--windowed") o.windowed = true;
    else if (arg == "--layers" && hasValue) o.lightingLayers = static_cast<uint32_t>(std::strtoul(argv[++i], nullptr, 0));
    else if (arg == "--board" && hasValue) o.boardData = argv[++i];
    else if (arg == "--game-assets" && hasValue) o.gameAssets = argv[++i];
    else if (arg == "--content" && hasValue) o.battleContent = argv[++i];
    else if (arg == "--unpaced") o.unpaced = true;
    else if (arg == "--capture-sequence" && hasValue) {
      const std::string v = argv[++i];
      const size_t a = v.find(':'), b = a == std::string::npos ? a : v.find(':', a + 1);
      if (a == std::string::npos || b == std::string::npos) fatal("--capture-sequence FROM:EVERY:COUNT");
      o.sequenceFrom = std::atoi(v.c_str());
      o.sequenceEvery = std::max(1, std::atoi(v.c_str() + a + 1));
      o.sequenceCount = std::atoi(v.c_str() + b + 1);
    }
    else if (arg == "--compare" && i + 5 < argc) o.compare.assign(argv + i + 1, argv + i + 6), i += 5;
    else if (arg == "--cost-split" && hasValue) o.costSplit = static_cast<uint32_t>(std::atoi(argv[++i]));
    else if (arg == "--grid" && hasValue) o.meshGrid = static_cast<uint32_t>(std::max(1, std::atoi(argv[++i])));
    else if (arg == "--budget" && hasValue) o.spriteBudgetMb = static_cast<uint64_t>(std::max(16, std::atoi(argv[++i]))), o.budgetSet = true;
    else if (arg == "--size" && hasValue) parseSize(argv[++i], o), o.windowed = true;
    else if (arg == "--frames" && hasValue) o.frames = std::atoi(argv[++i]);
    else if (arg == "--io-depth" && hasValue) o.ioDepth = static_cast<uint32_t>(std::max(1, std::atoi(argv[++i])));
    else if (arg == "--time" && hasValue) o.time = static_cast<float>(std::atof(argv[++i]));
    else if (arg == "--no-validation") o.validation = false;
    else fatal(std::format("Unknown argument {}", arg));
  }
  if (o.battle && (o.meshes.empty() || o.sprites.empty())) fatal("--battle needs --meshes <pak> and --sprites <pak>");
  if (!o.map.empty() && o.ground.empty()) fatal("--map needs --ground <ground.pak> (Cooker ground)");
  if (o.look != "smooth" && o.look != "hard" && o.look != "tabletop") fatal("--look is smooth, hard or tabletop");
  // The battle draws every decor card and 72 units at once: a larger sprite budget unless given.
  if (o.battle && !o.budgetSet) o.spriteBudgetMb = 1536;
  return o;
}

double msSince(Clock::time_point start) {
  return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}

// --- Streaming test ------------------------------------------------------------------------------

class StreamTest {
 public:
  StreamTest(AssetManager& assets, const IoQueue& io, std::string verifyRoot)
      : assets_(assets), io_(io), verifyRoot_(std::move(verifyRoot)), ids_(assets.mountedAssets()) {}

  bool done() const { return phase_ == Phase::Done; }
  bool passed() const { return done() && failed_ == 0 && mismatches_ == 0 && worstBusy_ <= kFrameBudgetMs; }

  void restart() {
    for (AssetHandle handle : handles_) assets_.release(handle);
    handles_.clear();
    phase_ = Phase::Warmup;
    frames_ = 0;
    baselineBusy_.clear();
    streamingBusy_.clear();
  }

  // Before the asset manager's update. The request burst happens here, inside the measured frame.
  void beginFrame() {
    if (phase_ == Phase::Warmup && frames_ >= kWarmupFrames) {
      bytesAtStart_ = io_.stats().bytesRead;
      start_ = Clock::now();
      handles_.reserve(ids_.size());
      for (AssetId id : ids_) handles_.push_back(assets_.load(id));
      requestMs_ = msSince(start_);
      phase_ = Phase::Streaming;
    }
  }

  // After the frame, with the main thread's working time for it.
  void endFrame(double busyMs) {
    ++frames_;
    if (phase_ == Phase::Warmup) baselineBusy_.push_back(busyMs);
    if (phase_ != Phase::Streaming) return;
    streamingBusy_.push_back(busyMs);
    const AssetManager::Stats stats = assets_.stats();
    if (stats.queued + stats.loading > 0) return;
    loadSeconds_ = std::chrono::duration<double>(Clock::now() - start_).count();
    bytesRead_ = io_.stats().bytesRead - bytesAtStart_;
    finish(stats);
  }

  void drawPanel() const {
    ImGui::SetNextWindowPos({12, 420}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowBgAlpha(0.82f);
    ImGui::Begin("Streaming test  [R]", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing);
    ImGui::Text("%zu assets in archive", ids_.size());
    if (phase_ == Phase::Warmup) {
      ImGui::Text("warming up (%d/%d frames)", frames_, kWarmupFrames);
    } else if (phase_ == Phase::Streaming) {
      const AssetManager::Stats stats = assets_.stats();
      ImGui::ProgressBar(ids_.empty() ? 1.0f : static_cast<float>(stats.ready + stats.failed) / static_cast<float>(ids_.size()),
                         {320, 0});
    } else {
      ImGui::Text("%s", passed() ? "PASS" : "FAIL");
      for (const std::string& line : report_) ImGui::TextUnformatted(line.c_str());
    }
    ImGui::End();
  }

 private:
  enum class Phase { Warmup, Streaming, Done };
  static constexpr int kWarmupFrames = 60;

  static double percentile(std::vector<double> values, double p) {
    if (values.empty()) return 0.0;
    std::sort(values.begin(), values.end());
    return values[std::min(values.size() - 1, static_cast<size_t>(p * static_cast<double>(values.size())))];
  }

  void finish(const AssetManager::Stats& stats) {
    phase_ = Phase::Done;
    failed_ = stats.failed;
    mismatches_ = verifyRoot_.empty() ? 0 : verify();
    worstBusy_ = streamingBusy_.empty() ? 0.0 : *std::max_element(streamingBusy_.begin(), streamingBusy_.end());

    uint64_t decompressed = 0;
    for (AssetHandle handle : handles_) decompressed += assets_.data(handle).size();
    const double mb = 1024.0 * 1024.0;
    report_.clear();
    report_.push_back(std::format("{} assets loaded, {} failed{}", stats.ready, stats.failed,
                                  verifyRoot_.empty() ? std::string(", not verified")
                                                      : std::format(", {} mismatched source files", mismatches_)));
    report_.push_back(std::format("all requested in one frame: {:.2f} ms for the {} load() calls", requestMs_, ids_.size()));
    report_.push_back(std::format("streamed in {:.3f} s over {} frames", loadSeconds_, streamingBusy_.size()));
    report_.push_back(std::format("disk {:.1f} MB at {:.0f} MB/s; {:.1f} MB after decompression ({:.0f} MB/s)",
                                  static_cast<double>(bytesRead_) / mb, static_cast<double>(bytesRead_) / mb / loadSeconds_,
                                  static_cast<double>(decompressed) / mb, static_cast<double>(decompressed) / mb / loadSeconds_));
    report_.push_back(std::format("main thread before streaming: median {:.3f} ms, worst {:.3f} ms",
                                  percentile(baselineBusy_, 0.5), percentile(baselineBusy_, 1.0)));
    report_.push_back(std::format("main thread while streaming:  median {:.3f} ms, p99 {:.3f} ms, worst {:.3f} ms",
                                  percentile(streamingBusy_, 0.5), percentile(streamingBusy_, 0.99), worstBusy_));
    report_.push_back(std::format("frame budget {:.2f} ms (60 fps): {}", kFrameBudgetMs,
                                  worstBusy_ <= kFrameBudgetMs ? "never exceeded" : "EXCEEDED"));
    logInfo("stream test: {}", passed() ? "PASS" : "FAIL");
    for (const std::string& line : report_) logInfo("  {}", line);
  }

  // Byte-for-byte comparison of every loaded asset with the file it was cooked from.
  uint32_t verify() const {
    uint32_t mismatches = 0;
    std::vector<char> source;
    for (size_t i = 0; i < ids_.size(); ++i) {
      const std::span<const std::byte> loaded = assets_.data(handles_[i]);
      std::ifstream file(std::filesystem::path(verifyRoot_) / std::string(assets_.name(ids_[i])), std::ios::binary | std::ios::ate);
      bool same = static_cast<bool>(file);
      if (same) {
        source.resize(static_cast<size_t>(file.tellg()));
        file.seekg(0);
        file.read(source.data(), static_cast<std::streamsize>(source.size()));
        same = source.size() == loaded.size() && std::memcmp(source.data(), loaded.data(), source.size()) == 0;
      }
      if (!same) {
        if (mismatches < 10) logError("mismatch: {}", assets_.name(ids_[i]));
        ++mismatches;
      }
    }
    return mismatches;
  }

  AssetManager& assets_;
  const IoQueue& io_;
  std::string verifyRoot_;
  std::vector<AssetId> ids_;
  std::vector<AssetHandle> handles_;
  Phase phase_ = Phase::Warmup;
  int frames_ = 0;
  Clock::time_point start_{};
  uint64_t bytesAtStart_ = 0, bytesRead_ = 0;
  double requestMs_ = 0.0, loadSeconds_ = 0.0, worstBusy_ = 0.0;
  uint32_t failed_ = 0, mismatches_ = 0;
  std::vector<double> baselineBusy_, streamingBusy_;
  std::vector<std::string> report_;
};

// --- Engine services shared by both modes --------------------------------------------------------

struct Services {
  JobSystem jobs;
  IoQueue io;
  AssetManager assets;
  std::unique_ptr<StreamTest> streamTest;
  PerfOverlay perf;
  std::function<void()> curtain;  // Engine2's loading curtain, over everything (LoadingScreens.h)

  explicit Services(const Options& o) : io(jobs, o.ioDepth), assets(jobs, io, {}) {
    if (!o.sprites.empty() && !assets.mount(o.sprites)) fatal(std::format("Cannot mount {}", o.sprites));
    if (!o.meshes.empty() && !assets.mount(o.meshes)) fatal(std::format("Cannot mount {}", o.meshes));
    // Ember's wall, door and gate textures (Cooker architecture), beside the mesh pack.
    if (!o.meshes.empty()) {
      const std::filesystem::path architecture = std::filesystem::path(o.meshes).parent_path() / "architecture.pak";
      std::error_code ec;
      if (std::filesystem::exists(architecture, ec) && !assets.mount(architecture.string())) fatal(std::format("Cannot mount {}", architecture.string()));
    }
    if (!o.ground.empty() && !assets.mount(o.ground)) fatal(std::format("Cannot mount {}", o.ground));
    for (size_t k = 0; k < o.compare.size() && k < 4; k += 2)
      if (!assets.mount(o.compare[k])) fatal(std::format("Cannot mount {}", o.compare[k]));
    if (o.stream.empty()) return;
    if (!assets.mount(o.stream)) fatal(std::format("Cannot mount {}", o.stream));
    streamTest = std::make_unique<StreamTest>(assets, io, o.verify);
  }

  // One frame of engine work around the render call. Returns the renderer's result.
  template <class Render>
  bool frame(DebugUi& ui, Renderer& renderer, Stage* stage, VkExtent2D size, float dt, const Render& render);
};

// Sprite streaming on the GPU: needs the renderer, so it is created after it (and destroyed before it).
struct SpriteStage final : Stage {
  UploadQueue uploads;
  SpriteSystem sprites;
  SpriteParade parade;
  std::vector<SpriteInstance> instances;
  Clock::time_point start = Clock::now();

  SpriteStage(VulkanContext& context, Renderer& renderer, AssetManager& assets, uint64_t budgetMb)
      : uploads(context), sprites(context, renderer.bindless(), assets, uploads, {budgetMb << 20}), parade(sprites) {
    sprites.start();
  }

  void frame(Renderer& renderer, VkExtent2D size) override {
    const double now = std::chrono::duration<double>(Clock::now() - start).count();
    parade.request(now);
    sprites.update();
    const Camera camera = SpriteParade::camera(static_cast<float>(size.width) / static_cast<float>(size.height));
    parade.build(now, camera, instances);
    uploads.flush();
    renderer.waitFor(uploads.timeline(), sprites.uploadWaitValue());
    renderer.setScene(camera, instances);
  }
  void drawPanel() override { parade.drawPanel(); }
  void endFrame(double busyMs) override { parade.endFrame(busyMs); }
  bool done() const override { return parade.done(); }
  bool passed() const override { return parade.passed(); }
};

// Nothing drawn (and nothing a closed screen owned left in the renderer's hands): the frame that puts the loading curtain
// up before a battle is built (its building blocks the main thread).
struct BlankStage final : Stage {
  void frame(Renderer& renderer, VkExtent2D) override {
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
  // A mission's game files still being made (main.cpp's `preparing`): the curtain stays up on its first step meanwhile.
  bool preparing = false;
  Loading loading() const override { return {0, 0, 0, !preparing}; }
};

// The window's first frames (the user, 2026-10-24: "it needs to open up sooner and leave it on loading"): the title's
// background and its loading bar while the game's data loads on a worker and the title's art decodes on another; the
// title then opens with the same bar (TitleStage::carryLoader), counting on from here.
struct BootStage final : Stage {
  UiPreload* art = nullptr;            // the title's art, handed to the title afterwards
  const std::atomic<int>* steps = nullptr;  // the data steps done
  int totalSteps = 0;
  loading::TitleLoader loader;
  BlankStage blank;

  void frame(Renderer& renderer, VkExtent2D size) override { blank.frame(renderer, size); }
  // What the bar counts here (the title's bar goes on from it).
  size_t unitsDone() const { return static_cast<size_t>(steps ? steps->load() : 0); }
  void drawPanel() override {
    const ImGuiIO& io = ImGui::GetIO();
    const auto [done, total] = art ? art->update() : std::pair<size_t, size_t>{0, 0};
    const size_t all = total + static_cast<size_t>(totalSteps);
    loader.set(all ? static_cast<float>(done + unitsDone()) / static_cast<float>(all) : 0.0f, false, ImGui::GetTime());
    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::Begin("##boot", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoInputs);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    TitleStage::drawBackdrop(dl, {0, 0}, io.DisplaySize);
    const float rem = 16.0f;  // the title's canvas units
    const auto [x, width] = TitleStage::loaderColumn(io.DisplaySize, rem);
    loader.draw(dl, io.DisplaySize, x, width, rem, ImGui::GetTime(), io.DeltaTime);
    ImGui::End();
    ImGui::PopStyleVar(2);
  }
};

MapStageOptions mapOptions(const Options& o);
// Modo teste: the session's heroes at its level (30) and the class path each chose on the Party screen; every hero knows every
// spell of its class at that level (the user, 2026-10-23: "todas as magias para todos os personagens no debug").
void applyTestHeroes(MapStageOptions& options, const e2::Json& tsave) {
  const e2::Json levels = tsave.value("levels", e2::Json::object());
  for (const auto& [name, level] : levels.items())
    if (level.is_number_integer()) options.heroProgress[name] = {level.get<int>(), 0}, options.fullSpells.insert(name);
  const e2::Json promotions = tsave.value("promotions", e2::Json::object());
  if (promotions.is_object())
    for (const auto& [name, cls] : promotions.items())
      if (cls.is_string()) options.heroClass[name] = cls.get<std::string>();
}

std::unique_ptr<Stage> makeStage(const Options& o, VulkanContext& context, Renderer& renderer, AssetManager& assets, JobSystem& jobs) {
  if (o.battle) {
    const BattleStage::Content content = o.battleContent == "3d"      ? BattleStage::Content::Meshes
                                         : o.battleContent == "cards" ? BattleStage::Content::Cards
                                                                      : BattleStage::Content::All;
    return std::make_unique<BattleStage>(context, renderer, assets, o.spriteBudgetMb, content, o.costSplit);
  }
  if (!o.map.empty()) {
    renderer.setDebugView(o.debugView);
    MapStageOptions options = mapOptions(o);
    if (!o.devGfx.empty()) DevGfx::load(o.devGfx).apply(options, renderer);
    return std::make_unique<MapStage>(context, renderer, assets, jobs, options);
  }
  if (!o.sprites.empty() && o.map.empty()) return std::make_unique<SpriteStage>(context, renderer, assets, o.spriteBudgetMb);
  if (!o.meshes.empty()) return std::make_unique<MeshStage>(context, renderer, assets, o.meshGrid, o.capture.empty());
  return nullptr;
}

MapStageOptions mapOptions(const Options& o) {
  {
    MapStageOptions m;
    m.map = o.map;
    m.scene = o.scene;
    m.spriteBudgetMb = o.spriteBudgetMb;
    m.boardData = o.boardData;
    m.engine2Public = o.gameAssets;
    m.look = o.look == "hard" ? e2::EdgeStyle::Hard : o.look == "tabletop" ? e2::EdgeStyle::Tabletop : e2::EdgeStyle::Smooth;
    m.zoom = o.mapZoom;
    m.turnDegrees = o.mapTurn;
    m.tiltDegrees = o.mapTilt;
    m.offset = {o.mapOffsetX, o.mapOffsetZ};
    m.layers = o.lightingLayers;
    m.sunAzimuth = o.sunAzimuth;
    m.sunElevation = o.sunElevation;
    m.movingLight = o.movingLight;
    m.spellLight = o.spellLight;
    m.fxTest = o.fxTest;
    m.battleScript = o.battleScript;
    m.cutscene = o.cutscene;
    m.cutsceneAuto = o.cutsceneAuto;
    m.tatica = o.tatica;
    m.spellsMenuOpen = o.spellsMenuOpen;
    m.measureSeconds = o.measureSeconds;
    m.fog = o.fog;
    m.scout = o.scout;
    m.volume = o.volume;
    m.fogLook = o.fogLook;
    m.gridLook = o.gridLook;
    m.cardStance = o.cardStance;
    m.barsFade = o.barsFade;
    m.actionCamera = o.actionCamera;
    m.floorImage = o.floorImage;
    m.windFog = o.windFog;
    m.windowLight = o.windowLight;
    m.atmos = o.atmos;
    m.background = o.background;
    m.waterLook = o.waterLook;
    m.water3dLook = o.water3dLook;
    m.skyReflect = o.skyReflect;
    m.clouds = o.clouds;
    m.glare = o.glare;
    m.shafts = o.shafts;
    m.moonLight = o.moonLight;
    m.hour = o.hour;
    m.sunDisc = o.sunDisc;
    m.gameHour = o.gameHour;
    m.mist = o.mist;
    m.lighting = o.lighting;
    m.sceneTime = o.sceneTime;
    m.moonPhase = o.moonPhase;
    m.moonPath = o.moonPath;
    m.starLook = o.starLook;
    m.heroes = o.heroes;
    m.battle = o.battleRules;
    m.heroClass = o.promote;
    for (const auto& [name, level] : o.promoteLevel) m.heroProgress[name] = {level, 0};
    m.arrivedFrom = o.arrivedFrom;
    m.leader = o.leader;
    const auto split = [](const std::string& s) {
      std::vector<std::string> out;
      for (size_t at = 0; at < s.size();) {
        const size_t end = std::min(s.find(',', at), s.size());
        if (end > at) out.push_back(s.substr(at, end - at));
        at = end + 1;
      }
      return out;
    };
    for (const std::string& id : split(o.defeatedSpawns)) m.defeatedSpawns.insert(id);
    for (const std::string& hero : split(o.knockedOut)) m.heroState[hero].hp = 0, m.preserveKnockouts = true;
    return m;
  }
}

// A campaign fight's heroes as the save has them: levels and XP, promotions (save.ts promotions: the Mago Negro's path chosen on the
// Party screen), their condition (GameApp startBattle's roster; HeroHandoff.h) and the party's affinities (save.affinityScores: the
// bonus between neighbours follows them, and the fight moves them). The game's loop and the mission captures alike.
void applySaveHeroes(MapStageOptions& options, const e2::Json& save) {
  const e2::Json levels = save.value("levels", e2::Json::object()), xp = save.value("xp", e2::Json::object());
  for (const auto& [name, level] : levels.items())
    if (level.is_number_integer()) options.heroProgress[name] = {level.get<int>(), xp.value(name, 0)};
  const e2::Json promotions = save.value("promotions", e2::Json::object());
  if (promotions.is_object())
    for (const auto& [name, cls] : promotions.items())
      if (cls.is_string()) options.heroClass[name] = cls.get<std::string>();
  options.heroState = heroStatesFromSave(save);
  if (const e2::Json scores = save.value("affinityScores", e2::Json::object()); scores.is_object())
    for (const auto& [pair, v] : scores.items())
      if (v.is_number()) options.affinityScores[pair] = v.get<double>();
}

// GameApp startBattle on a free-roam map (mission.explore, the walkable Inn): the companions not recruited yet are not seated
// there (a campaign only: Modo teste seats everyone), and the save's party leader walks (partyLeaderOf: the saved leader
// while recruited, else Kael).
// `interlude` (docs/INTERLUDE.md): while the historical chapter is on, its own leader walks.
void setUpExplore(MapStageOptions& options, const e2::Json& save, const e2::Overworld& world, bool test, const e2::InterludeData* interlude = nullptr) {
  std::ifstream in(options.battle, std::ios::binary);
  const e2::Json battle = in ? e2::Json::parse(in, nullptr, false) : e2::Json();
  if (!battle.is_object() || !battle.value("explore", false)) return;
  for (const char* hero : {"Neera", "Voss", "Salazar", "Aldric", "Malrec"})
    if (!test && !world.heroRecruited(hero, save)) options.absentNeutrals.insert(hero);
  if (options.leader.empty() && !test && interlude && e2::interludeState(save) == e2::InterludeState::Active) options.leader = interlude->leader;
  if (options.leader.empty())
    if (const auto l = save.find("partyLeader"); l != save.end() && l->is_string() && (test || world.heroRecruited(l->get<std::string>(), save)))
      options.leader = l->get<std::string>();
}

// The party of a fight (campaignParty.ts: "campaign progression defines the party; editor spawns only provide entry positions"):
// in the campaign the heroes the save has recruited (`campaign`), in Modo teste every hero of the battle file; laid out by the
// save's Formação inicial at the map's entry (MapStage::placeFormation). Free-roam maps are setUpExplore's.
// `interlude` (docs/INTERLUDE.md): while the historical chapter is on, the party is exactly its three heroes, nobody of the present.
void setUpParty(MapStageOptions& options, const e2::Json& save, const e2::Overworld& world, bool campaign, const e2::InterludeData* interlude = nullptr) {
  std::ifstream in(options.battle, std::ios::binary);
  const e2::Json battle = in ? e2::Json::parse(in, nullptr, false) : e2::Json();
  if (!battle.is_object() || battle.value("explore", false)) return;
  if (campaign) {
    std::set<std::string> roster;
    if (interlude && e2::interludeState(save) == e2::InterludeState::Active) {
      for (const e2::InterludeHero& h : interlude->party) roster.insert(h.name);
    } else {
      for (const char* hero : {"Kael", "Neera", "Voss", "Salazar", "Aldric", "Malrec"})
        if (world.heroRecruited(hero, save)) roster.insert(hero);
    }
    options.partyRoster = std::move(roster);
  }
  if (const auto f = save.find("partyFormation"); f != save.end() && f->is_array())
    for (const e2::Json& cell : *f) options.partyFormation.push_back(cell.is_string() ? cell.get<std::string>() : std::string());
  options.formation = true;
}

template <class Render>
bool Services::frame(DebugUi& ui, Renderer& renderer, Stage* stage, VkExtent2D size, float dt, const Render& render) {
  const auto start = Clock::now();
  if (streamTest) streamTest->beginFrame();
  assets.update();
  const double assetsMs = msSince(start);
  // The upload wait is the frame's own: each stage that uploads asks for it again in frame(). Kept across frames, a stage gone (a
  // battle left for the world map) left the renderer waiting on its destroyed timeline semaphore (vkQueueSubmit2: Invalid VkSemaphore,
  // the user's log of 2026-10-23).
  renderer.waitFor(VK_NULL_HANDLE, 0);
  if (stage) stage->frame(renderer, size);
  const double stageMs = msSince(start) - assetsMs;
  ui.newFrame(size, dt);
  perf.draw({&jobs, &io, &assets, &renderer});
  if (streamTest) streamTest->drawPanel();
  if (stage) stage->drawPanel();
  if (stage && stage->overlay) stage->overlay();
  if (curtain) curtain();
  const double uiMs = msSince(start) - assetsMs - stageMs;
  const bool ok = render();
  const double frameMs = msSince(start);
  const double busyMs = std::max(0.0, frameMs - renderer.lastWaitMilliseconds());
  // A frame the main thread spent over 50 ms on: where the time went (the hunt for the rare 140-220 ms hitch, 2026-10-19).
  if (busyMs > 50.0) {
    const Renderer::CpuSplit r = renderer.lastCpuSplit();
    logWarn("slow frame: {:.1f} ms busy = assets {:.1f}, stage {:.1f}, ui {:.1f}, render {:.1f} (record {:.1f}, submit {:.1f}, present {:.1f}; "
            "waits {:.1f} not counted)",
            busyMs, assetsMs, stageMs, uiMs, frameMs - assetsMs - stageMs - uiMs, r.record, r.submit, r.present, renderer.lastWaitMilliseconds());
  }
  perf.addFrame(frameMs, busyMs);
  if (streamTest) streamTest->endFrame(busyMs);
  if (stage) stage->endFrame(busyMs);
  return ok;
}

int runCompare(const Options& o) {
  Services services(o);
  VulkanContext context({o.validation, nullptr});
  bool ok = false;
  {
    Renderer renderer(context, nullptr, {o.width, o.height});
    ok = runMeshCompare(context, renderer, services.assets, assetId(o.compare[1]), assetId(o.compare[3]),
                        std::filesystem::path(o.compare[4]), {o.width, o.height});
  }
  if (context.validationErrors()) {
    logError("{} validation error(s)", context.validationErrors());
    return 2;
  }
  return ok ? 0 : 1;
}

int runCapture(const Options& o) {
  Services services(o);
  if (o.lang == "pt" || o.lang == "en") {
    GamePrefs& prefs = gamePrefs();  // this run only: nothing written
    prefs.uiLanguage = prefs.dialogueLanguage = prefs.subtitleLanguage = o.lang;
  }
  VulkanContext context({o.validation, nullptr});
  bool passed = true;
  {
    DebugUi ui(context, nullptr, Renderer::outputFormat());
    Renderer renderer(context, nullptr, {o.width, o.height});
    if (!o.noUi) renderer.setUi(&ui);  // --no-ui: the scene alone (pixel checks)
    // --title: the title screen off-screen (--title-screen new / load: its save slots, read from the real bank, nothing
    // written), to check its layout.
    e2::Campaign campaign;
    const bool hasCampaign = o.title && campaign.open("content/engine2/campaign.json", o.saveBank.empty() ? e2::Campaign::savePath() : std::filesystem::path(o.saveBank));
    std::unique_ptr<Stage> stage;
    e2::Overworld world;
    std::optional<TestSession> testSession;
    DevGfx devGfx;  // captures: Engine2's defaults, never the PC's saved switches
    CampaignPaths campaignPaths;
    campaignPaths.engine2Public = o.gameAssets;
    campaignPaths.maps = !o.maps.empty() ? std::filesystem::path(o.maps) : std::filesystem::path("content/maps/ember");
    campaignPaths.scenes = o.scenes;
    e2::InterludeData interludeData;  // the Interlude's gate and start on the world map (docs/INTERLUDE.md), as in the game
    const bool hasInterludeData = interludeData.load("content/interlude.json");
    if (o.title && o.titleScreen == "map" && hasCampaign && world.load("content/engine2/overworld.json")) {
      // --title-screen map: the active slot's world map (--map-ui: party / quests / options / event / location opened; enter:<id>).
      auto map = std::make_unique<OverworldStage>(campaignPaths, campaign, world, OverworldStage::Callbacks{}, std::string(), nullptr,
                                                  hasInterludeData ? &interludeData : nullptr);
      if (!o.mapUi.empty()) map->preview(o.mapUi);
      stage = std::move(map);
    } else if (o.title && o.titleScreen.starts_with("mission") && hasCampaign) {
      // --title-screen mission:<id> / mission-test:<id>: a campaign mission's battle, its files resolved as the campaign
      // (or Modo teste) resolves them (CampaignUi.h: the Map Editor's newer save, else Engine2's newest).
      const bool test = o.titleScreen.starts_with("mission-test:");
      const std::string id = o.titleScreen.substr(o.titleScreen.find(':') + 1);
      std::string error;
      std::optional<MissionEntry> m = hasInterludeData && interludeData.hasMission(id) ? interludeMission(campaignPaths, id, error, test)
                                      : test                                            ? testMission(campaign, campaignPaths, id, error)
                                                                                        : campaignMission(campaign, campaignPaths, id, error);
      if (!m) fatal(error);
      if (m->prepare) {  // the campaign's game files made first, as the game makes them when the mission opens
        const MapExportResult made = exportMap(m->source, m->id, campaignPaths, std::filesystem::path(o.gameAssets).parent_path(),
                                               campaignPaths.campaign / "logs" / (m->id + ".log"));
        if (!made.ok) fatal("mission " + id + ": its game files could not be made (" + made.error + ")");
        logInfo("mission {}: game files made from {}", id, m->source.generic_string());
      }
      logInfo("mission {}: {} / {} / {}", id, m->map.generic_string(), m->scene.generic_string(), m->battle.generic_string());
      Options mission = o;
      mission.map = m->map.string(), mission.scene = m->scene.string(), mission.battleRules = m->battle.string();
      MapStageOptions options = mapOptions(mission);
      options.spellsMenu = options.choicesPanel = test;  // Modo teste's tools, as in the game
      if (world.load("content/engine2/overworld.json")) options.world = &world;  // the status sheet's Mochila / Equipar
      if (test) testSession = freshTestSession(campaign);
      if (test) applyTestHeroes(options, testSession->save);
      else applySaveHeroes(options, campaign.activeSave());  // the campaign's heroes as the save has them (levels, promotions, condition)
      // The sky: a campaign mission at the save's hour; Modo teste at the map's own (gameHour -1).
      if (!test) {
        options.gameHour = static_cast<float>(e2::Overworld::campaignHour(campaign.activeSave()));
        if (options.moonPhase.empty()) options.moonPhase = e2::campaignMoonPhase(campaign.activeSave().value("gameClock", 0));
      }
      if (options.world) setUpExplore(options, test ? testSession->save : campaign.activeSave(), world, test, hasInterludeData ? &interludeData : nullptr);
      if (options.world) {  // the party and its formation (--formation "Kael,,Neera,...": the board's cells instead of the save's)
        e2::Json save = test ? testSession->save : campaign.activeSave();
        if (!o.formation.empty()) {
          save["partyFormation"] = e2::Json::array();
          for (size_t at = 0; at <= o.formation.size();) {
            const size_t end = std::min(o.formation.find(',', at), o.formation.size());
            save["partyFormation"].push_back(o.formation.substr(at, end - at));
            at = end + 1;
          }
        }
        setUpParty(options, save, world, !test, hasInterludeData ? &interludeData : nullptr);
      }
      auto missionStage = std::make_unique<MapStage>(context, renderer, services.assets, services.jobs, options);
      missionStage->setCampaign(&campaign, test ? std::string() : id, id, test);  // the campaign's own save backs the inventory
      // The people who talk (NpcTalk.h); a capture never writes the bank (no campaign record: the Inn's menus read a copy).
      auto talk = attachNpcTalk(*missionStage, o.gameAssets, options.world, nullptr, test ? &testSession->save : nullptr, campaign.activeSave());
      if (missionStage->draftId().rfind("estalagem", 0) == 0) missionStage->setMusic(music::theme("inn"));
      if (o.mapUi.starts_with("npc:")) talk->preview(o.mapUi.substr(4));
      if (o.mapUi == "menu" || o.mapUi == "options" || o.mapUi == "log" || o.mapUi == "fulllog" || o.mapUi == "quests" || o.mapUi == "hotbar" || o.mapUi.starts_with("status") || o.mapUi.starts_with("inventory:") || o.mapUi.starts_with("synced:")) missionStage->previewMenu(o.mapUi);
      stage = std::move(missionStage);
    } else if (o.title && o.titleScreen.starts_with("test") && hasCampaign && world.load("content/engine2/overworld.json")) {
      // --title-screen test / test-choice / test-classic / test-scenarios / test-rpg / test-dev / test-editor: Modo teste's screens (--map-ui
      // location:<id> opens a place's panel on the classic map; on the RPG map the usual --map-ui panels).
      testSession = freshTestSession(campaign);
      using S = TestModeStage::Screen;
      if (o.titleScreen == "test-editor") {
        // the Map Editor (--map-ui <map.json>: that map opened in it; --editor-script: clicks on its board)
        MapStageOptions base = mapOptions(o);
        std::optional<e2::Json> draft;
        if (!o.mapUi.empty()) {
          std::ifstream in(o.mapUi, std::ios::binary);
          const e2::Json j = e2::Json::parse(in, nullptr, false);
          if (!j.is_discarded()) draft = j.contains("draft") ? j["draft"] : j;
        }
        auto editor = std::make_unique<MapEditorStage>(MapEditorStage::Services{context, renderer, services.assets, services.jobs}, base, campaignPaths,
                                                       campaign, MapEditorStage::Callbacks{}, draft);
        if (!o.editorScript.empty()) editor->previewEdits(o.editorScript);
        stage = std::move(editor);
      } else if (o.titleScreen == "test-rpg") {
        testSession->mapMode = "rpg";
        auto map = std::make_unique<OverworldStage>(campaignPaths, campaign, world, OverworldStage::Callbacks{}, std::string{}, &testSession->save);
        if (!o.mapUi.empty()) map->preview(o.mapUi);
        stage = std::move(map);
      } else {
        const S screen = o.titleScreen == "test-dev" ? S::DevControls : o.titleScreen == "test-choice" ? S::MapChoice : o.titleScreen == "test-classic" ? S::WorldMap : o.titleScreen == "test-scenarios" ? S::Scenarios : S::Menu;
        auto test = std::make_unique<TestModeStage>(campaignPaths, campaign, world, *testSession, devGfx, TestModeStage::Callbacks{}, screen);
        if (o.mapUi.starts_with("location:")) test->previewLocation(o.mapUi.substr(9));
        stage = std::move(test);
      }
    } else if (o.title) {
      TitleStage::Paths paths;
      paths.engine2Public = o.gameAssets, paths.maps = campaignPaths.maps, paths.scenes = o.scenes;
      auto title = std::make_unique<TitleStage>(paths, hasCampaign ? &campaign : nullptr, [](const MissionEntry&) {}, [] {}, [] {});
      if (o.titleScreen == "new" || o.titleScreen == "load") title->previewSlots(o.titleScreen == "load");
      title->onOptions({}, &devGfx);  // Engine2's defaults (captures never write the PC's switches: nothing is clicked)
      if (o.titleScreen == "options") title->previewOptions();
      if (o.titleScreen.starts_with("help")) {  // help[:tab[:scroll]]
        int tab = 0;
        float scroll = 0.0f;
        std::sscanf(o.titleScreen.c_str(), "help:%d:%f", &tab, &scroll);
        title->previewHelp(tab, scroll);
      }
      if (o.titlePick >= 0) title->previewPick(o.titlePick);
      stage = std::move(title);
    } else {
      stage = makeStage(o, context, renderer, services.assets, services.jobs);
      // --map-ui menu / options: the battle's Esc menu or Opções open.
      if (auto* map = dynamic_cast<MapStage*>(stage.get()); map && (o.mapUi == "menu" || o.mapUi == "options" || o.mapUi == "log" || o.mapUi == "fulllog" || o.mapUi == "quests" || o.mapUi == "hotbar" || o.mapUi.starts_with("status") || o.mapUi.starts_with("inventory:") || o.mapUi.starts_with("synced:")))
        map->previewMenu(o.mapUi);
      // --resume <snapshot.json>: the battle as that saved fight left it.
      if (auto* map = dynamic_cast<MapStage*>(stage.get()); map && !o.resume.empty()) {
        std::ifstream in(o.resume, std::ios::binary);
        const e2::Json snap = e2::Json::parse(in, nullptr, false);
        map->setCampaign(nullptr, snap.is_object() ? snap.value("missionId", std::string()) : std::string(), std::string(), false);
        if (!map->resume(snap)) fatal("--resume: the snapshot does not fit this battle");
      }
      // --map-ui talk:<mission>[:outro]: that mission's intro (or outro) conversation over the battle, nothing recorded.
      if (auto* map = dynamic_cast<MapStage*>(stage.get()); map && o.mapUi.starts_with("talk:")) {
        std::string mission = o.mapUi.substr(5);
        const bool outro = mission.ends_with(":outro");
        if (outro) mission.resize(mission.size() - 6);
        world.load("content/engine2/overworld.json");
        if (auto talk = attachBattleTalk(*map, mission, loadDialogs("content/engine2/dialogs.json"), o.gameAssets, world, nullptr); talk && outro) {
          talk->introDone = true;
          talk->open("outro", "outroEnabled");
        }
      }
    }
    const VkExtent2D size{o.width, o.height};
    // --curtain: Engine2's loading curtain over the battle (its real steps) or the world map, as in the game; every change
    // of its status is logged with the time.
    loading::Curtain curtain;
    const auto runStart = Clock::now();
    std::string curtainShown;
    if (o.curtain && stage) {
      if (dynamic_cast<MapStage*>(stage.get())) curtain.open(loading::Curtain::Kind::Battle, 0.0);
      else if (dynamic_cast<OverworldStage*>(stage.get())) curtain.open(loading::Curtain::Kind::Map, 0.0);
        services.curtain = [&] { curtain.draw(o.gameAssets); };
    }
    const auto curtainStep = [&] {
      if (!curtain.up()) return;
      const double t = std::chrono::duration<double>(Clock::now() - runStart).count();
      curtain.update(stage->loading(), t, 1.0f / 60.0f);
      const std::string now = curtain.up() ? curtain.label() : std::string("lifted");
      if (now != curtainShown) logInfo("curtain {:.2f} s: {}", t, now), curtainShown = now;
    };
    const auto testDone = [&] { return (services.streamTest && services.streamTest->done()) || (stage && stage->done()); };
    if (services.streamTest || stage) {
      // Run until the test ends, plus a few frames so the overlay shows final numbers.
      // The sprite parade runs at 60 Hz like the game (streaming per second depends on the frame rate).
      int after = 0, frameNo = 0, taken = 0;
      auto next = Clock::now();
      // A capture sequence keeps the run alive until its frames are taken (the stage may be done before FROM).
      while (after < kFramesInFlight + 2 || (o.sequenceCount > 0 && taken < o.sequenceCount)) {
        const bool inSequence = o.sequenceCount > 0 && taken < o.sequenceCount && frameNo >= o.sequenceFrom &&
                                (frameNo - o.sequenceFrom) % o.sequenceEvery == 0;
        ++frameNo;
        curtainStep();
        if (inSequence) {
          const std::string path = std::format("{}-{:03}.png", o.capture.ends_with(".png") ? o.capture.substr(0, o.capture.size() - 4) : o.capture, taken++);
          services.frame(ui, renderer, stage.get(), size, 1.0f / 60.0f, [&] {
            renderer.captureFrame(o.time, path);
            return true;
          });
        } else
        services.frame(ui, renderer, stage.get(), size, 1.0f / 60.0f, [&] { return renderer.renderFrame(o.time); });
        if (testDone()) ++after;
        if (stage && !o.unpaced) {
          next += std::chrono::microseconds(16667);
          const auto now = Clock::now();
          if (next > now) SDL_DelayPrecise(static_cast<Uint64>(std::chrono::nanoseconds(next - now).count()));
          else next = now;
        }
      }
      passed = services.streamTest ? services.streamTest->passed() : stage->passed();
    } else {
      for (int i = 0; i < o.frames; ++i)
        services.frame(ui, renderer, nullptr, size, 1.0f / 60.0f, [&] { return renderer.renderFrame(o.time); });
    }
    ui.newFrame(size, 1.0f / 60.0f);
    services.perf.draw({&services.jobs, &services.io, &services.assets, &renderer});
    if (services.streamTest) services.streamTest->drawPanel();
    if (stage) stage->drawPanel();
    if (stage && stage->overlay) stage->overlay();
    if (services.curtain) services.curtain();
    renderer.captureFrame(o.time, o.capture);
    stage.reset();
  }
  if (context.validationErrors()) {
    logError("{} validation error(s)", context.validationErrors());
    return 2;
  }
  return passed ? 0 : 3;
}

int runWindow(const Options& o) {
  const auto bootStart = Clock::now();
  const auto bootMs = [&] { return std::chrono::duration<double, std::milli>(Clock::now() - bootStart).count(); };
  const bool titleMode = o.title || (o.map.empty() && !o.battle && o.sprites.empty() && o.meshes.empty() && o.stream.empty());
  // The campaign's save bank (save.ts) and the world map's rules (overworld.ts), Engine2's tables.
  e2::Campaign campaign;
  bool hasCampaign = false;
  e2::Overworld world;
  bool hasWorld = false;
  // The Interlude (docs/INTERLUDE.md): the Ruins' quest gate, the historical chapter's party and maps (content/interlude.json).
  e2::InterludeData interlude;
  bool hasInterlude = false;
  // The Map Editor's saved Locais and R-Encounter (content/maps): the campaign and the world map play them, again
  // whenever the editor saves them.
  auto applyLocais = [&] {
    const e2::Locais locais = e2::Locais::load("content/engine2/editor-catalog.json");
    if (hasCampaign) e2::applyLocais(campaign.data(), locais, savedMapsDir());
    if (hasWorld) world.applyLocais(locais, campaign.data());
  };
  // Every mission's intro / outro conversation (export_dialogs.ts).
  e2::Json dialogs;
  // Dev Controls' switches (Modo teste), applied to every map opened here.
  DevGfx devGfx;
  // All of it needs no GPU: it loads on a worker while the window and the GPU come up (BootStage shows the bar meanwhile),
  // and the title's art decodes on another.
  constexpr int kBootSteps = 4;
  std::atomic<int> bootSteps{0};
  std::future<void> boot = std::async(std::launch::async, [&] {
    hasCampaign = campaign.open("content/engine2/campaign.json", o.saveBank.empty() ? e2::Campaign::savePath() : std::filesystem::path(o.saveBank));
    ++bootSteps;
    hasWorld = world.load("content/engine2/overworld.json");
    {
      std::string why;
      hasInterlude = interlude.load("content/interlude.json", &why);
      if (!hasInterlude) logWarn("interlude: content/interlude.json not loaded ({}): the Ruins open as any location, no chapter", why);
    }
    ++bootSteps;
    applyLocais();
    ++bootSteps;
    dialogs = loadDialogs("content/engine2/dialogs.json");
    devGfx = DevGfx::load();
    ++bootSteps;
    logInfo("startup: data loaded at {:.0f} ms", bootMs());
  });
  std::unique_ptr<UiPreload> titleArt = titleMode ? std::make_unique<UiPreload>(TitleStage::artFiles(o.gameAssets)) : nullptr;
  Services services(o);
  services.perf.visible = false;  // the game shows only its own screens (Ctrl+F1 brings the numbers back, in Modo teste)
  Window window({"Ember", o.width, o.height, !o.windowed});
  logInfo("startup: window at {:.0f} ms", bootMs());
  VulkanContext context({o.validation, window.handle()});
  uint32_t width = 0, height = 0;
  window.pixelSize(width, height);
  auto swapchain = std::make_unique<Swapchain>(context, width, height, true);
  DebugUi ui(context, window.handle(), Renderer::outputFormat());
  Renderer renderer(context, swapchain.get(), swapchain->extent());
  renderer.setUi(&ui);
  logInfo("startup: GPU ready at {:.0f} ms", bootMs());
  AudioMixer::get().setRoot(o.gameAssets);
  // The window's first frames: the title's background and its loading bar until the data is in (BootStage); the title
  // carries the bar on.
  std::optional<loading::TitleLoader> bootLoader;
  if (boot.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
    BootStage bootStage;
    bootStage.art = titleArt.get(), bootStage.steps = &bootSteps, bootStage.totalSteps = kBootSteps;
    const auto first = Clock::now();
    auto last = first;
    while (boot.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
      if (!window.pumpEvents()) {
        boot.wait();
        return 0;
      }
      window.pixelSize(width, height);
      if (width == 0 || height == 0) {  // minimised
        SDL_Delay(10);
        continue;
      }
      const auto now = Clock::now();
      const float dt = std::chrono::duration<float>(now - last).count();
      last = now;
      const bool presented = !window.consumeResized() && services.frame(ui, renderer, &bootStage, swapchain->extent(), dt, [&] {
        return renderer.renderFrame(std::chrono::duration<float>(now - first).count());
      });
      if (!presented && swapchain->recreate(width, height)) renderer.resize(swapchain->extent());
    }
    bootLoader = bootStage.loader;
  }
  boot.get();
  // The title screen when asked (or when nothing else is). Its save slots open the campaign's world map; a mission
  // from there (or a test-mode map) plays in this window, then the world map comes back.
  std::optional<MissionEntry> pending;
  // A campaign mission's game files being made before it opens (MissionEntry::prepare), and that mission.
  std::future<MapExportResult> preparing;
  std::optional<MissionEntry> preparingEntry;
  bool backToTitle = false, openMap = false;
  std::unique_ptr<Stage> stage;
  std::string playing;       // the campaign mission being fought (its result goes into the active slot)
  std::string campaignNote;   // what the world map says when the fight is left
  CampaignPaths paths;
  paths.engine2Public = o.gameAssets;
  paths.maps = !o.maps.empty() ? std::filesystem::path(o.maps) : std::filesystem::path("content/maps/ember");
  paths.scenes = o.scenes;
  // Engine2's Modo teste (TestModeStage.h): the session (its own save, the travel style) from the title's "Modo teste"
  // until the player goes back to the title; nothing of it is written to the bank.
  std::optional<TestSession> testSession;
  std::optional<TestModeStage::Screen> openTest;  // a test-mode screen to open
  bool openTestRpg = false;                       // the RPG map on the test session's save
  bool openUnderworld = false;                    // the world map opens on the Atlas do Submundo (the Underdark's exit)
  std::string openLocation;                       // the world map opens with this location's mission list (back from the Interlude: the Ruins)
  bool openSpells = false, inSpells = false;      // Teste de Magias: the spells test battle (open it / it is open)
  // Map Editor: open it / a Testar battle from it is open; the draft it was editing (kept while the game runs, as
  // GameApp's editorDraft).
  bool openEditor = false, inEditorTest = false;
  std::optional<e2::Json> editorDraft;
  // GameApp enterFromSave: the world map opened from Continuar / a load resumes a fight saved mid-way (resumeOnMap);
  // the battle then opened takes the save's snapshot (resumeNext).
  bool resumeOnMap = false, resumeNext = false;
  // A floor connector taken (GameApp startBattle(target, ..., arrivedFrom)): the map it was taken on, for the next battle.
  std::string arriveFrom;
  // ... and the heroes who fell there stay out of the next floor (GameApp startBattle's preserveKnockouts).
  bool preserveKnockouts = false;
  auto toggleFullscreen = [&] {
    const bool full = (SDL_GetWindowFlags(window.handle()) & SDL_WINDOW_FULLSCREEN) != 0;
    SDL_SetWindowFullscreen(window.handle(), !full);
  };
  auto makeTitle = [&] {
    TitleStage::Paths titlePaths;
    titlePaths.engine2Public = paths.engine2Public, titlePaths.maps = paths.maps, titlePaths.scenes = paths.scenes;
    auto title = std::make_unique<TitleStage>(
        titlePaths, hasCampaign ? &campaign : nullptr, [&](const MissionEntry& m) { pending = m; }, [&] { openMap = true, resumeOnMap = true; },
        [] {
          SDL_Event quit{};
          quit.type = SDL_EVENT_QUIT;
          SDL_PushEvent(&quit);
        },
        std::move(titleArt));  // the first title takes the art decoding since the window opened; later ones start their own
    if (bootLoader) title->carryLoader(*bootLoader, kBootSteps), bootLoader.reset();
    if (hasCampaign && hasWorld)
      title->onTestMode([&] {
        testSession = freshTestSession(campaign);  // GameApp ?start=test: a fresh test session every entry
        openTest = TestModeStage::Screen::Menu;
      });
    title->onOptions(toggleFullscreen, &devGfx);
    if (!campaignNote.empty()) title->showCampaign(std::exchange(campaignNote, {}));
    return title;
  };
  auto makeTest = [&](TestModeStage::Screen screen) -> std::unique_ptr<Stage> {
    TestModeStage::Callbacks callbacks;
    callbacks.title = [&] { backToTitle = true; };
    callbacks.rpgMap = [&] { openTestRpg = true; };
    callbacks.play = [&](const MissionEntry& m) { pending = m; };
    callbacks.spells = [&] { openSpells = true; };
    callbacks.editor = [&] { openEditor = true; };
    callbacks.fullscreen = toggleFullscreen;
    return std::make_unique<TestModeStage>(paths, campaign, world, *testSession, devGfx, std::move(callbacks), screen, std::exchange(campaignNote, {}));
  };
  auto makeEditor = [&]() -> std::unique_ptr<Stage> {
    MapEditorStage::Callbacks callbacks;
    callbacks.back = [&](const e2::Json& draft) {
      editorDraft = draft;
      openTest = TestModeStage::Screen::Menu;
    };
    callbacks.play = [&](const MissionEntry& m, const e2::Json& draft) {
      editorDraft = draft;
      pending = m;
      inEditorTest = true;
    };
    callbacks.locaisSaved = applyLocais;
    MapStageOptions base = mapOptions(o);
    devGfx.apply(base, renderer);
    return std::make_unique<MapEditorStage>(MapEditorStage::Services{context, renderer, services.assets, services.jobs}, base, paths, campaign,
                                            std::move(callbacks), editorDraft);
  };
  auto makeTestRpg = [&]() -> std::unique_ptr<Stage> {
    OverworldStage::Callbacks callbacks;
    callbacks.play = [&](const MissionEntry& m) { pending = m; };
    callbacks.title = [&] { openTest = TestModeStage::Screen::Menu; };  // onBack / Voltar à tela inicial: the test menu
    callbacks.fullscreen = toggleFullscreen;
    callbacks.gfx = &devGfx;
    return std::make_unique<OverworldStage>(paths, campaign, world, std::move(callbacks), std::exchange(campaignNote, {}), &testSession->save,
                                            hasInterlude ? &interlude : nullptr);
  };
  auto makeMap = [&]() -> std::unique_ptr<Stage> {
    if (!hasWorld) {
      campaignNote = "Sem dados do mapa do mundo (content/engine2/overworld.json).";
      return makeTitle();
    }
    OverworldStage::Callbacks callbacks;
    callbacks.play = [&](const MissionEntry& m) { pending = m; };
    callbacks.title = [&] { backToTitle = true; };
    callbacks.fullscreen = toggleFullscreen;
    callbacks.gfx = &devGfx;
    callbacks.resume = [&](const MissionEntry& m) { pending = m, resumeNext = true; };
    callbacks.resumeSaved = std::exchange(resumeOnMap, false);
    auto map = std::make_unique<OverworldStage>(paths, campaign, world, std::move(callbacks), std::exchange(campaignNote, {}), nullptr,
                                                hasInterlude ? &interlude : nullptr);
    if (!openLocation.empty()) map->openLocationPanel(std::exchange(openLocation, {}));
    return map;
  };
  // campaign.ts finishBattle (its heroes part and the mission's completion): each hero's level, XP and HP into the
  // active slot, the pending mission cleared, and on a victory the mission completed. Loot, ember, rations, quests and
  // story progression are not ported yet.
  // `advancing`: a floor connector taken (the result screen's Subir / Descer): the next floor opens right after, so no note and
  // no world map.
  auto finishCampaignFight = [&](const MapStage& map, bool advancing = false) {
    if (playing.empty() || map.battleResult().empty()) return;
    e2::Json save = campaign.activeSave();
    const bool victory = map.battleResult() == "victory";
    if (victory) save["unitHp"] = e2::Json::object();  // persistVictory: unitHp is this fight's heroes, after the camp (campHp)
    for (const HeroOutcome& h : map.heroOutcomes()) {
      if (!victory || h.hp > 0) {  // persistVictory keeps the levels and XP of the heroes still standing
        save["levels"][h.name] = h.level;
        save["xp"][h.name] = h.xp;
      }
      save["unitHp"][h.name] = victory ? h.campHp : h.hp;
      // What the fight trained (weapons, Cura, resistances, Esquiva), added to the save's, capped at 100 (the user, 2026-10-20:
      // only Esquiva ever rose, the rest was dropped here)
      for (const auto& [id, gain] : h.skillGains) {
        e2::Json& skills = save["heroSkills"][h.name];
        const double before = skills.contains(id) && skills[id].is_number() ? skills[id].get<double>() : 0.0;
        skills[id] = std::min(100.0, std::round((before + gain) * 100.0) / 100.0);
      }
    }
    // mapstore keepsDefeatedSpawns: a dungeon keeps the enemies and neutrals killed here dead on every later visit
    if (const e2::MapMission* mm = hasWorld ? world.mission(playing) : nullptr; victory && mm && mm->keepsSpawns) {
      e2::Json& dead = save["crossingDefeatedSpawns"];
      if (!dead.is_object()) dead = e2::Json::object();
      e2::Json& list = dead[playing];
      if (!list.is_array()) list = e2::Json::array();
      for (const std::string& id : map.fallenSpawns())
        if (std::find(list.begin(), list.end(), id) == list.end()) list.push_back(id);
    }
    // potions left, disease, poison, hunger, rations (finishBattle; HeroHandoff.h)
    writeHeroConditions(save, map.heroOutcomes(), map.lootRations());
    for (const auto& [pair, score] : map.affinityScoresNow()) save["affinityScores"][pair] = score;
    save["pendingMission"] = nullptr;
    save["battle"] = nullptr;
    const bool won = map.battleResult() == "victory";
    if (won) {
      // The chests opened in the fight (GameApp victory fold): their Ember, and the weapons / gear when the party's bags have room
      if (map.lootEmber() > 0) save["ember"] = save.value("ember", 0) + map.lootEmber();
      // Itens desconhecidos (ours, docs/SKILLS_MAGE.md): what came unknown is marked so in the save (unknownWeapons / unknownEquipment /
      // unknownPotions) until Identificar reveals it
      for (const std::string& id : map.lootWeapons()) {
        if (save["weapons"].is_object() && save["weapons"].contains(id)) continue;
        e2::Json probe = save;
        probe["weapons"][id] = 0;
        if (!world.bagHasRoom(probe, 1)) continue;
        save["weapons"][id] = 0;
        if (map.lootUnknown().contains(id)) {
          if (!save["unknownWeapons"].is_array()) save["unknownWeapons"] = e2::Json::array();
          save["unknownWeapons"].push_back(id);
        }
      }
      for (const std::string& id : map.lootEquipment()) {
        const e2::Json& stash = save["looseEquipment"];
        const int have = stash.is_object() ? stash.value(id, 0) : 0;
        e2::Json probe = save;
        probe["looseEquipment"][id] = have + 1;
        if (!world.bagHasRoom(probe, 1)) continue;
        save["looseEquipment"][id] = have + 1;
        if (map.lootUnknown().contains(id)) {
          if (!save["unknownEquipment"].is_object()) save["unknownEquipment"] = e2::Json::object();
          save["unknownEquipment"][id] = save["unknownEquipment"].value(id, 0) + 1;
        }
      }
      for (const auto& [kind, holder] : map.lootUnknownPotions()) {
        if (!save["unknownPotions"].is_object()) save["unknownPotions"] = e2::Json::object();
        if (!save["unknownPotions"][holder].is_object()) save["unknownPotions"][holder] = e2::Json::object();
        save["unknownPotions"][holder][kind] = save["unknownPotions"][holder].value(kind, 0) + 1;
      }
      // wispCrossing.ts completedAfterWispVictory: the crossing counts only once its lair's plant falls (then both do)
      e2::Json& done = save["completed"];
      if (!done.is_array()) done = e2::Json::array();
      const bool wispDone = std::find(done.begin(), done.end(), "wisp-forest-crossing") != done.end() ||
                            std::find(done.begin(), done.end(), "wisp-forest-crossing-boss") != done.end();
      std::vector<std::string> ids{playing};
      if (playing == "wisp-forest-crossing" && !wispDone) ids.clear();
      if (playing == "wisp-forest-crossing-boss") ids.push_back("wisp-forest-crossing");
      for (const std::string& id : ids)
        if (std::find(done.begin(), done.end(), id) == done.end()) done.push_back(id);
      // Inn quests (persistVictory): the pickups found here, and this mission's quest targets that fell, accepted or not
      e2::foldQuestProgress(save, map.questFound(), map.questKills());
    } else {
      save["pendingMission"] = playing;  // a lost fight is played again from its briefing
    }
    // The Interlude (docs/INTERLUDE.md): a chapter mission won moves the chapter on (the next map pending), the last one ends it (the
    // present party back, Isolde's progression kept, the Ruins open as the present-day location), in the same write as the victory.
    bool interludeOver = false;
    if (won && hasInterlude && e2::interludeAdvance(interlude, save, playing)) {
      interludeOver = e2::interludeState(save) == e2::InterludeState::Completed;
      logInfo("interlude: {} done: {}", playing, interludeOver ? "the chapter is complete, the present party is back" : "next map " + save.value("pendingMission", std::string()));
    }
    const bool ok = campaign.saveActive(std::move(save));
    if (advancing) {
      if (!ok) logWarn("{}: the victory could not be written before the next floor", playing);
      return;
    }
    const e2::CampaignMission* m = campaign.data().mission(playing);
    const std::string name = m ? m->title : playing;
    campaignNote = !ok ? "Não foi possível gravar o progresso (" + e2::Campaign::savePath().string() + ")."
                   : won ? name + ": vitória gravada no espaço " + std::to_string(campaign.bank().lastSlot + 1) + "."
                         : name + ": derrota. O combate fica pendente no espaço " + std::to_string(campaign.bank().lastSlot + 1) + ".";
    if (ok && interludeOver) {
      campaignNote = tr("Interlúdio concluído: o grupo voltou ao presente.", "Interlude complete: the party is back in the present.");
      openLocation = interlude.ruinsLocation;  // the present-day Ruins, where the party stood when the chapter began
    }
    playing.clear();
    openMap = true;  // GameApp ResultScreen "Mapa": back to the world map
  };
  // GameApp onQuit on an explore map: the map's mission completed, no pending mission or resumable fight left in the slot.
  auto markVisited = [&](const std::string& id, const MapStage* map = nullptr) {
    e2::Json save = campaign.activeSave();
    if (map) e2::foldQuestProgress(save, map->questFound(), map->questKills());  // what the walk found stays found
    e2::Json& done = save["completed"];
    if (!done.is_array()) done = e2::Json::array();
    if (std::find(done.begin(), done.end(), id) == done.end()) done.push_back(id);
    save["pendingMission"] = nullptr;
    save["battle"] = nullptr;
    // An Interlude free-roam map left by its waypoint counts as done for the chapter (docs/INTERLUDE.md)
    if (hasInterlude && e2::interludeAdvance(interlude, save, id) && e2::interludeState(save) == e2::InterludeState::Completed) {
      campaignNote = tr("Interlúdio concluído: o grupo voltou ao presente.", "Interlude complete: the party is back in the present.");
      openLocation = interlude.ruinsLocation;
    }
    if (!campaign.saveActive(std::move(save))) logWarn("{}: the visit could not be written", id);
  };
  // Engine2's loading curtain (LoadingScreens.h): up before a battle is built, over the world map as it opens.
  loading::Curtain curtain;
  services.curtain = [&] { curtain.draw(paths.engine2Public); };
  const auto loopStart = Clock::now();
  const auto clockNow = [&] { return std::chrono::duration<double>(Clock::now() - loopStart).count(); };
  // A battle: the curtain shown for one frame over a blank screen before the battle is built (building it blocks).
  const auto curtainBeforeBattle = [&] {
    curtain.open(loading::Curtain::Kind::Battle, clockNow());
    stage = std::make_unique<BlankStage>();
    services.frame(ui, renderer, stage.get(), swapchain->extent(), 0.0f, [&] { return renderer.renderFrame(static_cast<float>(clockNow())); });
  };
  if (titleMode) stage = makeTitle();
  else stage = makeStage(o, context, renderer, services.assets, services.jobs);
  if (dynamic_cast<MapStage*>(stage.get())) curtain.open(loading::Curtain::Kind::Battle, clockNow());
  logInfo("startup: first screen at {:.0f} ms", bootMs());

  window.setEventHook([&](const SDL_Event& event) {
    if (ui.processEvent(event)) return;
    if (stage && !curtain.up()) stage->event(event);
    if (event.type != SDL_EVENT_KEY_DOWN || event.key.repeat) return;
    // Modo teste only; Ctrl+F1, since F1..F12 are the battle bar's sockets (the user, 2026-10-18)
    if (event.key.key == SDLK_F1 && (event.key.mod & SDL_KMOD_CTRL) && testSession) services.perf.visible = !services.perf.visible;
    if (event.key.key == SDLK_R && services.streamTest) services.streamTest->restart();
  });

  const auto start = Clock::now();
  auto last = start;
  while (window.pumpEvents()) {
    window.pixelSize(width, height);
    if (width == 0 || height == 0) continue;  // minimised
    const auto now = Clock::now();
    const float dt = std::chrono::duration<float>(now - last).count();
    last = now;
    const float time = std::chrono::duration<float>(now - start).count();
    const bool resized = window.consumeResized();
    if (curtain.up() && stage) curtain.update(stage->loading(), clockNow(), dt);
    const bool presented = !resized && services.frame(ui, renderer, stage.get(), swapchain->extent(), dt,
                                                      [&] { return renderer.renderFrame(time); });
    // The sound (audio.ts): Opções' volumes, the screen's music (GameApp's chain), delays and fades.
    {
      const GamePrefs& prefs = gamePrefs();
      AudioMixer& mixer = AudioMixer::get();
      mixer.setVolumes(prefs.music, prefs.sfx, prefs.muted);
      const Stage::Music m = stage ? stage->music() : Stage::Music{};
      if (m.file.empty() || prefs.muted) mixer.stopMusic();
      else mixer.playMusic(m.file, m.base);
      mixer.update(dt);
    }
    if (!presented && swapchain->recreate(width, height)) renderer.resize(swapchain->extent());
    // Screen changes happen between frames, with the GPU idle: the old screen's resources go before the new one loads.
    std::optional<MapStageOptions> retry;
    if (auto* map = dynamic_cast<MapStage*>(stage.get())) {
      if (map->wantsFloor() == OverworldStage::kUnderworldExit && !pending) {
        // The Underdark of Mordávia's exit (the Misty Cave's map 4): the world map opens on the Atlas do Submundo. The campaign
        // writes the map done (a fight won first, as on any floor) and "underworldReached", so the world map's Underworld word
        // switches from then on; a test session goes back to its RPG map (Modo teste always has the Underworld).
        map->clearFloor();
        if (testSession) {
          openTestRpg = true;
        } else if (!playing.empty()) {
          if (!map->explores() && map->battleResult() == "victory") finishCampaignFight(*map, true);
          else markVisited(playing, map);
          e2::Json save = campaign.activeSave();
          save["underworldReached"] = true;
          if (!campaign.saveActive(std::move(save))) logWarn("{}: the way to the Underworld could not be written", playing);
          playing.clear();
          openMap = true;
        } else {
          backToTitle = true;
        }
        openUnderworld = !backToTitle;
      } else if (const std::string floor = map->wantsFloor(); !floor.empty() && !pending) {
        // A floor connector (GameApp: the result screen's Subir / Descer, then startBattle(target, ..., arrivedFrom)): the linked
        // floor opens, the party beside the connector back.
        map->clearFloor();
        std::string error;
        const bool test = testSession || playing.empty();
        std::optional<MissionEntry> next = test ? testMission(campaign, paths, floor, error) : campaignMission(campaign, paths, floor, error);
        const e2::Json completed = test ? e2::Json::array() : campaign.activeSave().value("completed", e2::Json::array());
        const bool lairDone = std::find(completed.begin(), completed.end(), "wisp-forest-crossing-boss") != completed.end();
        if (!next) {
          logWarn("floor connector to {}: {}", floor, error);
        } else if (!test && floor == "wisp-forest-crossing-boss" && lairDone) {
          logWarn("floor connector to {}: the lair is done (GameApp startBattle refuses it)", floor);
        } else {
          // A fight won on its stairs: the victory is written first (persistVictory: HP carried, the dead stay dead), as Engine2
          // writes it when the result screen shows; a free-roam floor (the walkable Inn) writes nothing here.
          if (!test && !map->explores() && map->battleResult() == "victory") finishCampaignFight(*map, true);
          arriveFrom = map->draftId();
          preserveKnockouts = true;
          pending = std::move(next);
        }
      } else if (map->wantsLoad()) {
        // The battle menu's Load: the chosen slot's campaign, where it stands (GameApp enterFromSave; test mode ends).
        playing.clear(), testSession.reset(), inEditorTest = false, inSpells = false;
        openMap = true, resumeOnMap = true;
      } else if (map->wantsHome()) {
        backToTitle = true;  // the battle menu's "Tela inicial" (GameApp goToTitle)
      } else if (map->wantsTitle()) {
        if (inEditorTest) {
          openEditor = true;  // Encerrar teste: the editor again, on the same draft
        } else if (inSpells) {
          openTest = TestModeStage::Screen::Menu;  // Teste de Magias: back to the test menu
        } else if (testSession) {
          // A test battle never writes anything (GameApp: every save write is !testMode); back to the test map.
          if (testSession->mapMode == "rpg") openTestRpg = true;
          else openTest = TestModeStage::Screen::WorldMap;
        } else if (!playing.empty() && !map->battleResult().empty()) {
          finishCampaignFight(*map);
        } else if (!playing.empty() && map->explores()) {
          // Leaving a free-roam map (the walkable Inn's Sair): the visit recorded, no resumable snapshot left (GameApp onQuit).
          markVisited(playing, map);
          playing.clear();
          openMap = true;
        } else if (!playing.empty()) {
          playing.clear();
          openMap = true;  // Desistir: back to the world map, the mission still to fight
        } else {
          backToTitle = true;
        }
      }
      if (map->wantsRetry()) {
        retry = map->options();
        // Tentar de novo: the same mission again, from the save as it stands (the lost fight is not written).
      }
    }
    if (retry) {
      vkDeviceWaitIdle(context.device);
      stage.reset();
      curtainBeforeBattle();
      auto again = std::make_unique<MapStage>(context, renderer, services.assets, services.jobs, *retry);
      again->setMenus(toggleFullscreen, &devGfx);
      again->setCampaign(hasCampaign ? &campaign : nullptr, playing, std::filesystem::path(retry->map).stem().string(), testSession.has_value());
      again->setPlaytest(inEditorTest);
      if (!playing.empty() && dialogs.is_object())
        attachBattleTalk(*again, playing, dialogs, paths.engine2Public, world, testSession ? nullptr : &campaign);
      attachNpcTalk(*again, paths.engine2Public, &world, !testSession && !playing.empty() ? &campaign : nullptr, testSession ? &testSession->save : nullptr,
                    campaign.emptySave());
      stage = std::move(again);
    }
    // A campaign mission whose game files are not made yet (MissionEntry::prepare): the loading curtain goes up and they are made
    // off the main thread (MapExport.h Campaign: Engine2's exporters, a few seconds); then the mission opens as usual.
    if (pending && pending->prepare && !preparing.valid()) {
      vkDeviceWaitIdle(context.device);
      stage.reset();
      curtain.open(loading::Curtain::Kind::Battle, clockNow());
      auto blank = std::make_unique<BlankStage>();
      blank->preparing = true;
      stage = std::move(blank);
      preparingEntry = std::move(pending);
      pending.reset();
      logInfo("{}: making its game files from {}", preparingEntry->draftId, preparingEntry->source.generic_string());
      preparing = std::async(std::launch::async, [entry = *preparingEntry, paths, engine2 = std::filesystem::path(paths.engine2Public).parent_path()] {
        return exportMap(entry.source, entry.id, paths, engine2, paths.campaign / "logs" / (entry.id + ".log"));
      });
    }
    if (preparing.valid() && preparing.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
      const MapExportResult r = preparing.get();
      if (r.ok) {
        preparingEntry->prepare = false;
        pending = std::move(preparingEntry);
      } else {
        logWarn("{}: its game files could not be made ({})", preparingEntry->draftId, r.error);
        campaignNote = "Não foi possível preparar " + preparingEntry->title + " (" + r.error + ").";
        arriveFrom.clear(), preserveKnockouts = false;
        playing.clear();
        if (testSession) {
          if (testSession->mapMode == "rpg") openTestRpg = true;
          else openTest = TestModeStage::Screen::WorldMap;
        } else {
          openMap = true;
        }
      }
      preparingEntry.reset();
    }
    if (preparing.valid()) continue;  // still making them: the curtain stays up
    if (pending || backToTitle || openMap || openTest || openTestRpg || openSpells || openEditor) {
      vkDeviceWaitIdle(context.device);
      stage.reset();
      inSpells = false;
      if (openSpells && testSession) {
        // Teste de Magias: aldeia015's battle with the Magias window open (every spell's effect versions, Testar,
        // the loose effects), as the test zone of 2026-10-16.
        Options spells = o;
        spells.map = (paths.maps / "aldeia015.json").string();
        spells.scene = (std::filesystem::path(o.scenes) / "aldeia015.scene.json").string();
        spells.battleRules = "content/engine2/battles/aldeia015.battle.json";
        renderer.setDebugView(o.debugView);
        MapStageOptions options = mapOptions(spells);
        options.spellsMenuOpen = true;
        devGfx.apply(options, renderer);
        curtainBeforeBattle();
        auto spellsMap = std::make_unique<MapStage>(context, renderer, services.assets, services.jobs, options);
        spellsMap->setMenus(toggleFullscreen, &devGfx);
        stage = std::move(spellsMap);
        inSpells = true;
      } else if (backToTitle) {
        playing.clear();
        testSession.reset();  // GameApp goToTitle: every trace of test mode wiped
        services.perf.visible = false;  // the numbers are Modo teste's too
        stage = makeTitle();
      } else if (openEditor && testSession) {
        inEditorTest = false;
        stage = makeEditor();
      } else if (openTest && testSession) {
        stage = makeTest(*openTest);
      } else if (openTestRpg && testSession) {
        stage = makeTestRpg();
        curtain.open(loading::Curtain::Kind::Map, clockNow());
      } else if (pending) {
        Options mission = o;
        mission.map = pending->map.string();
        mission.scene = pending->scene.string();
        mission.battleRules = pending->battle.string();
        renderer.setDebugView(o.debugView);
        MapStageOptions options = mapOptions(mission);
        devGfx.apply(options, renderer);
        options.world = &world;  // the Mochila / Equipar of the status sheet
        playing = pending->campaignId;
        // Magias and Escolhas are Modo teste's tools: a campaign battle shows only the game (user 2026-10-16).
        options.spellsMenu = options.choicesPanel = testSession.has_value() || playing.empty();
        if (!playing.empty() && !testSession) applySaveHeroes(options, campaign.activeSave());
        if (testSession) applyTestHeroes(options, testSession->save);
        // The sky follows the campaign's clock (the save's hour) on every campaign map; Modo teste (classic and RPG maps alike) and the
        // editor's Testar take the map's own Hora do dia (gameHour -1; the user, 2026-10-23: "no debug o horário que estiver no mapa").
        if (!playing.empty() && !testSession) {
          const e2::Json save = campaign.activeSave();
          options.gameHour = static_cast<float>(e2::Overworld::campaignHour(save));
          // ... and its moon: the phase of the save's day (the world map's moon plate); Modo teste: the map's own Lua.
          if (options.moonPhase.empty()) options.moonPhase = e2::campaignMoonPhase(save.value("gameClock", 0));
        }
        // A free-roam map: who sits there and who walks (the campaign's save, or Modo teste's); where the party comes in.
        if (hasWorld) setUpExplore(options, testSession ? testSession->save : campaign.activeSave(), world, testSession.has_value() || playing.empty(), hasInterlude ? &interlude : nullptr);
        // The campaign's party (or Modo teste's), laid out by the Party screen's formation; the editor's Testar keeps the draft's places.
        if (hasWorld && !inEditorTest)
          setUpParty(options, testSession ? testSession->save : campaign.activeSave(), world, !testSession && !playing.empty(), hasInterlude ? &interlude : nullptr);
        options.arrivedFrom = std::exchange(arriveFrom, {});
        options.preserveKnockouts = std::exchange(preserveKnockouts, false);
        // A dungeon's dead stay dead (crossingDefeatedSpawns; a campaign only), and the Wisp crossing's exits lead to the lair until
        // it is done (routeWispCrossing; Modo teste: always to the lair).
        if (!playing.empty() && !testSession) {
          const e2::Json save = campaign.activeSave();
          if (const e2::MapMission* mm = hasWorld ? world.mission(playing) : nullptr; mm && mm->keepsSpawns)
            if (const e2::Json dead = save.value("crossingDefeatedSpawns", e2::Json::object()); dead.is_object() && dead.contains(playing) && dead[playing].is_array())
              for (const e2::Json& id : dead[playing])
                if (id.is_string()) options.defeatedSpawns.insert(id.get<std::string>());
          const e2::Json done = save.value("completed", e2::Json::array());
          options.wispCrossingDone = std::find(done.begin(), done.end(), "wisp-forest-crossing") != done.end() ||
                                     std::find(done.begin(), done.end(), "wisp-forest-crossing-boss") != done.end();
        }
        curtainBeforeBattle();
        auto map = std::make_unique<MapStage>(context, renderer, services.assets, services.jobs, options);
        map->setMenus(toggleFullscreen, &devGfx);
        map->setPlaytest(inEditorTest);
        map->setCampaign(hasCampaign ? &campaign : nullptr, playing, pending->id, testSession.has_value() || playing.empty());
        // A fight saved mid-way, resumed (GameApp startBattle with resume: applySnapshot); its intro already played.
        bool resumed = false;
        if (std::exchange(resumeNext, false) && !playing.empty()) {
          const e2::Json save = campaign.activeSave();
          resumed = save.contains("battle") && map->resume(save["battle"]);
          if (!resumed) logWarn("{}: the saved fight does not fit this map; it starts afresh", playing);
        }
        // The mission's intro conversation, its outro on a victory, and the campaign's epilogue films.
        if (!playing.empty() && dialogs.is_object())
          if (auto talk = attachBattleTalk(*map, playing, dialogs, paths.engine2Public, world, testSession ? nullptr : &campaign); talk && resumed)
            talk->introDone = true;
        // The people who talk on it (Brue's tavern, Vargan's forge, the old healer in the walkable Inn: NpcTalk.h).
        attachNpcTalk(*map, paths.engine2Public, hasWorld ? &world : nullptr, !testSession && !playing.empty() ? &campaign : nullptr,
                      testSession ? &testSession->save : nullptr, campaign.emptySave());
        // Both of the walkable Inn's floors keep the Inn's theme (GameApp: walking in and out of its menus never switches tracks).
        if (map->draftId().rfind("estalagem", 0) == 0) map->setMusic(music::theme("inn"));
        stage = std::move(map);
      } else {
        stage = makeMap();
        curtain.open(loading::Curtain::Kind::Map, clockNow());
      }
      if (std::exchange(openUnderworld, false))
        if (auto* atlas = dynamic_cast<OverworldStage*>(stage.get())) atlas->showUnderworld();
      pending.reset();
      openTest.reset();
      backToTitle = false, openMap = false, openTestRpg = false, openSpells = false, openEditor = false;
    }
  }
  AudioMixer::get().shutdown();  // before SDL closes with the window
  return 0;
}

}  // namespace

namespace {
// --loadout-check <battle.json> <reference battle.json> <save.json>: every hero of the first battle equipped from the save
// (e2::equipFromSave) must carry what Engine2 gave it in the reference (export_battle.ts with GEAR_SAVE=<save.json> on the same
// map): weapon dice, enhancement, reach, off hand, staff magic, the arrow, both hands' mastery. Exit 3 on any difference.
int loadoutCheck(const char* battleFile, const char* referenceFile, const char* saveFile) {
  std::optional<e2::Battle> battle = e2::Battle::load(battleFile), reference = e2::Battle::load(referenceFile);
  e2::Loadouts arms;
  std::ifstream in(saveFile, std::ios::binary);
  const e2::Json save = in ? e2::Json::parse(in, nullptr, false) : e2::Json();
  if (!battle || !reference || !arms.load("content/engine2/weapons.json") || !save.is_object()) return 1;
  int heroes = 0, differences = 0;
  for (e2::BattleUnit& u : battle->units()) {
    if (u.side != "player" || u.summoned) continue;
    const auto ref = std::find_if(reference->units().begin(), reference->units().end(), [&](const e2::BattleUnit& r) { return r.id == u.id; });
    if (ref == reference->units().end()) continue;
    e2::equipFromSave(u, arms, save);
    ++heroes;
    const e2::CombatProfile &a = u.combat, &b = ref->combat;
    std::string diff;
    auto check = [&](const char* what, auto mine, auto theirs) {
      if (mine != theirs) diff += std::format(" {} {} vs Engine2 {};", what, mine, theirs);
    };
    check("weapon", a.hasWeapon, b.hasWeapon), check("dice", a.dice, b.dice), check("faces", a.faces, b.faces), check("bonus", a.bonus, b.bonus);
    check("enh", a.enh, b.enh), check("ranged", a.ranged, b.ranged), check("classBonus", a.classBonus, b.classBonus);
    check("minRange", u.minRange, ref->minRange), check("maxRange", u.maxRange, ref->maxRange);
    check("accuracy", a.accuracy, b.accuracy), check("damageMul", a.damageMul, b.damageMul);
    check("offAccuracy", a.offAccuracy, b.offAccuracy), check("offDamageMul", a.offDamageMul, b.offDamageMul);
    check("offWeapon", a.offHandWeapon, b.offHandWeapon), check("offShield", a.offHandShield, b.offHandShield);
    check("offRange", a.offMaxRange, b.offMaxRange), check("offDice", a.offDice, b.offDice), check("offFaces", a.offFaces, b.offFaces);
    check("offBonus", a.offBonus, b.offBonus), check("offShieldMul", a.offShieldMul, b.offShieldMul);
    check("staff", a.staff, b.staff), check("staffElement", a.staffElement, b.staffElement), check("staffBonus", a.staffBonusPct, b.staffBonusPct);
    check("lifeSteal", a.staffLifeStealPct, b.staffLifeStealPct), check("regen", a.staffRegen, b.staffRegen), check("arrow", a.arrow, b.arrow);
    check("weaponType", u.weaponType, ref->weaponType), check("trains", u.trainsWeapon, ref->trainsWeapon), check("skill", u.weaponSkill, ref->weaponSkill);
    check("offType", u.offWeaponType, ref->offWeaponType), check("offTrains", u.offTrainsWeapon, ref->offTrainsWeapon), check("offSkill", u.offWeaponSkill, ref->offWeaponSkill);
    if (!diff.empty()) ++differences;
    logInfo("loadout {} ({}): {}d{}+{} enh {}, reach {}-{}, accuracy {} x{}, off {}{}, staff {} -> {}", u.name, u.classId, a.dice, a.faces, a.bonus, a.enh,
            u.minRange, u.maxRange, a.accuracy, a.damageMul, a.offHandWeapon ? "weapon" : a.offHandShield ? "shield" : "none",
            a.offHandWeapon ? std::format(" {}d{}", a.offDice, a.offFaces) : std::string(), a.staff ? a.staffElement : "no", diff.empty() ? "matches Engine2" : "DIFFERS:" + diff);
  }
  logInfo("loadout check: {} heroes, {} differ from Engine2", heroes, differences);
  return heroes && !differences ? 0 : 3;
}

// --battle-save-check <battle.json> [out snapshot]: a fight's state saved (Battle::snapshot) and laid on a fresh copy
// of the same battle (applySnapshot) must give the same snapshot back. The state: a round played into (initiative,
// the first hero walked to the far end of its reach, hurt, its XP and a tier-1 use spent), an enemy fallen, enmity
// noted, a second round started.
int battleSaveCheck(const char* file, const char* out) {
  std::optional<e2::Battle> a = e2::Battle::load(file);
  if (!a) return 1;
  e2::Mulberry32 rng(7);
  a->rollInitiative(rng);
  std::vector<e2::BattleUnit>& u = a->units();
  u.reserve(u.size() + 4);  // a familiar is added below: `h` must not move
  int hero = -1, foe = -1, foe2 = -1;
  for (size_t i = 0; i < u.size(); ++i) {
    if (u[i].side == "player" && hero < 0) hero = static_cast<int>(i);
    else if (u[i].side == "enemy" && foe < 0) foe = static_cast<int>(i);
    else if (u[i].side == "enemy" && foe2 < 0) foe2 = static_cast<int>(i);
  }
  if (hero < 0 || foe < 0 || foe2 < 0) return 1;
  const std::vector<e2::ReachCell> reach = a->reachable(static_cast<size_t>(hero));
  if (!reach.empty()) a->move(static_cast<size_t>(hero), reach.back().x, reach.back().y);
  e2::BattleUnit& h = u[static_cast<size_t>(hero)];
  h.hp -= 7, h.xp += 30, h.acted = true;
  if (h.spells[0] > 0) --h.spells[0];
  // a potion drunk, sick, poisoned, hungry, rations conjured
  h.bag[1] = 1, h.bag[4] = 2;
  h.diseased = true, h.diseaseBase = e2::BattleUnit::Stats{h.atk + 1, h.mag + 1, h.def + 1, h.dex + 1, h.mov + 1};
  h.poisoned = true, h.poisonTier = "lesser", h.fullness = 40.0, h.hungerPenaltyPct = 0.2, h.offWeaponSkill = 3.5;
  a->addLootRations(3);
  // Engine2's statuses, zones, a warp gate and a summoned familiar must come back too
  h.stunned = true, h.stunTurns = 2, h.bleeding = true, h.bleedRoundsLeft = 3, h.bleedRoundMarker = 1, h.crippled = true;
  h.blessedHitBonusPct = 0.07, h.blessedRoundsLeft = 4, h.fearTurns = 1, h.fearSourceId = u[static_cast<size_t>(foe)].id;
  u[static_cast<size_t>(foe2)].asleep = true, u[static_cast<size_t>(foe2)].sleepTurns = 3;
  a->webZones().push_back({{{h.x, h.y}, {h.x + 1, h.y}}, 2, {h.x, h.y}, 1, 0.55});
  a->iceZones().push_back({{{h.x, h.y}}, 3, {h.x, h.y}, 1, 2, 6, 0.75, 31, 12, h.id, "player"});
  a->auraZones().push_back({{{h.x, h.y}, {h.x, h.y + 1}}, 3, "protection", "player", 0.2});
  a->warpGates().push_back({{h.x + 2, h.y}, {h.x + 3, h.y}, 3, 9});
  a->setTurnRestrained(true);
  if (std::optional<e2::BattleUnit> fam = e2::summonSkeleton("player-familiar-" + std::to_string(u.size()), "familiar2")) {
    fam->name = "Familiar Maior de " + h.name, fam->summonerId = h.id, fam->x = h.x, fam->y = h.y + 2, fam->hp = 11, fam->maxHp = 23, fam->level = h.level;
    fam->atk = 7, fam->spellCharges = 2, fam->lifeDrainCharges = 1;
    u.push_back(std::move(*fam));
  }
  u[static_cast<size_t>(foe)].alive = false, u[static_cast<size_t>(foe)].hp = 0;
  a->noteWeaponDamage(static_cast<size_t>(hero), static_cast<size_t>(foe2), 5);
  a->startNewRound();
  const nlohmann::json s1 = a->snapshot();
  std::optional<e2::Battle> b = e2::Battle::load(file);
  e2::Mulberry32 other(99);
  b->rollInitiative(other);
  const bool applied = b->applySnapshot(s1);
  const nlohmann::json s2 = b->snapshot();
  const bool same = applied && s1 == s2;
  std::printf("battle save check %s: %s (%zu units, round %d, hero %s at %d,%d hp %d)\n", file, same ? "PASS" : "FAIL", u.size(),
              s1.value("turn", 0), h.name.c_str(), h.x, h.y, h.hp);
  if (!same) std::printf("%s\n%s\n", s1.dump().c_str(), s2.dump().c_str());
  if (out) {
    nlohmann::json snap = s1;
    snap["missionId"] = "check", snap["introDialogDone"] = true, snap["phase"] = "player", snap["turnBegan"] = false;
    std::ofstream(out, std::ios::binary) << snap.dump(1);
  }
  return same ? 0 : 1;
}
}  // namespace

namespace {
// --roster-check <battle.json> <engine2 battle.json> <save.json>: the heroes as a campaign save brings them
// (heroStatesFromSave + MapStage::applyHeroState on the plain export) must be Engine2's own (the same map exported with
// export_battle.ts SAVE=<the save>): HP, stats, disease, poison, hunger, potions. Then a fight's end written back
// (writeHeroConditions) must leave the save as Engine2's finishBattle merges would.
int rosterCheck(const char* plainFile, const char* engine2File, const char* saveFile) {
  std::optional<e2::Battle> plain = e2::Battle::load(plainFile), ref = e2::Battle::load(engine2File);
  std::ifstream in(saveFile, std::ios::binary);
  if (!plain || !ref || !in || plain->units().size() != ref->units().size()) return 1;
  const e2::Json save = e2::Json::parse(in);
  const auto states = heroStatesFromSave(save);
  int diffs = 0, heroes = 0;
  for (size_t i = 0; i < plain->units().size(); ++i) {
    e2::BattleUnit& u = plain->units()[i];
    const e2::BattleUnit& r = ref->units()[i];
    if (u.side != "player") continue;
    if (const auto it = states.find(u.name); it != states.end()) MapStage::applyHeroState(u, it->second);
    ++heroes;
    std::string d;
    auto same = [&](const char* what, double a, double b) {
      if (std::abs(a - b) > 1e-9) d += std::format(" {} {} vs {}", what, a, b);
    };
    same("hp", u.hp, r.hp), same("maxHp", u.maxHp, r.maxHp), same("atk", u.atk, r.atk), same("mag", u.mag, r.mag), same("def", u.def, r.def);
    same("dex", u.dex, r.dex), same("mov", u.mov, r.mov), same("diseased", u.diseased, r.diseased), same("poisoned", u.poisoned, r.poisoned);
    same("fullness", u.fullness.value_or(100), r.fullness.value_or(100)), same("hungerPenalty", u.hungerPenaltyPct, r.hungerPenaltyPct);
    for (int k = 0; k < 7; ++k) same(e2::kPotionIds[k], u.bag[k], r.bag[k]);
    same("diseaseBase", u.diseaseBase.has_value(), r.diseaseBase.has_value());
    if (u.diseaseBase && r.diseaseBase)
      same("baseAtk", u.diseaseBase->atk, r.diseaseBase->atk), same("baseMov", u.diseaseBase->mov, r.diseaseBase->mov);
    if (u.poisonTier != r.poisonTier) d += " poisonTier " + u.poisonTier + " vs " + r.poisonTier;
    if (!d.empty()) ++diffs, std::printf("  %s:%s\n", u.name.c_str(), d.c_str());
  }
  // The fight's end: the first hero drank a mid potion and was cured, the second fed to 120 and poisoned, 4 rations.
  std::vector<HeroOutcome> out;
  for (const e2::BattleUnit& u : plain->units())
    if (u.side == "player" && !u.summoned) {
      HeroOutcome h{u.name, u.level, u.xp, u.hp};
      std::copy(std::begin(u.bag), std::end(u.bag), h.bag.begin());
      h.diseased = u.diseased, h.poison = u.poisoned ? u.poisonTier : "", h.fullness = u.fullness;
      out.push_back(h);
    }
  int writeDiffs = 0;
  if (out.size() >= 2) {
    out[0].bag[1] -= 1, out[0].diseased = false, out[0].poison.clear();
    out[1].fullness = 120.0, out[1].poison = "lesser";
    e2::Json after = save;
    writeHeroConditions(after, out, 4);
    auto expect = [&](bool ok, const std::string& what) {
      if (!ok) ++writeDiffs, std::printf("  write-back: %s\n", what.c_str());
    };
    expect(after["bags"][out[0].name]["mid"] == out[0].bag[1], "first hero's mid potions");
    expect(!after["heroDiseases"].contains(out[0].name), "first hero still sick in the save");
    expect(!after["heroPoisons"].contains(out[0].name), "first hero still poisoned in the save");
    expect(after["heroHunger"][out[1].name] == 120.0, "second hero's hunger");
    expect(after["heroPoisons"][out[1].name] == "lesser", "second hero's poison");
    expect(after.value("rations", 0) == save.value("rations", 0) + 4, "rations");
    if (save.contains("bags") && save["bags"].contains(out[0].name) && save["bags"][out[0].name].contains("lockpick"))
      expect(after["bags"][out[0].name]["lockpick"] == save["bags"][out[0].name]["lockpick"], "lockpicks kept");
  }
  const bool pass = diffs == 0 && writeDiffs == 0 && heroes > 0;
  std::printf("roster check %s: %s (%d heroes, %d differ from Engine2, %d write-back errors)\n", plainFile, pass ? "PASS" : "FAIL", heroes, diffs,
              writeDiffs);
  return pass ? 0 : 1;
}
}  // namespace

int main(int argc, char** argv) {
  if (argc >= 3 && std::string_view(argv[1]) == "--battle-save-check") return battleSaveCheck(argv[2], argc >= 4 ? argv[3] : nullptr);
  if (argc >= 5 && std::string_view(argv[1]) == "--loadout-check") return loadoutCheck(argv[2], argv[3], argv[4]);
  if (argc >= 5 && std::string_view(argv[1]) == "--roster-check") return rosterCheck(argv[2], argv[3], argv[4]);
  // --audio-check [engine2 public folder]: decode every effect and the start of every track (exit 1 if any fails).
  if (argc >= 2 && std::string_view(argv[1]) == "--audio-check")
    return ember::AudioMixer::check(argc >= 3 ? argv[2] : "content/game/public") == 0 ? 0 : 1;
  // --film-check <film> [still.png [picture]]: decode it (and its .webm twin, for an .mp4) flat out; logs each decoder's cost
  // (exit 1 if any fails); with a still, that picture of each is written as <still>-h264.png / -av1.png.
  if (argc >= 3 && std::string_view(argv[1]) == "--film-check")
    return ember::VideoPlayer::check(argv[2], argc >= 4 ? argv[3] : "", argc >= 5 ? std::atoi(argv[4]) : 100) == 0 ? 0 : 1;
  if (argc >= 2 && std::string_view(argv[1]) == "--cursor-check")
    return ember::DebugUi::checkCursors(argc >= 3 ? argv[2] : "build/cursor-check");
  const Options options = parse(argc, argv);
  if (!options.compare.empty()) return runCompare(options);
  return options.capture.empty() ? runWindow(options) : runCapture(options);
}
