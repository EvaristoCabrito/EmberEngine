# EmberEngine: handoff for the next Claude session

Read this first, then `docs/ARCHITECTURE.md` (design, measured numbers, milestone exit tests; §7.0–7.2 and §9
are the current frontier). Memory notes: `C:\Users\evari\.claude\projects\C--EmberEngine\memory\`.

## What this is

A game engine built from zero (C++20 + Vulkan 1.3, native Windows .exe, not three.js/browser) for the user's
game *Ember*: hex-grid tactics ("Tactics Ogre with HD-2D") with photorealistic 2D characters in a real 3D world,
cinematic feel, dramatic light and shadow, incredible spell effects, realistic ground. **Visual target: the
spirit of Square Enix's Luminous Engine (Agni's Philosophy, FFXV), taken into 2.75D: not Unreal, not HD-2D.**
The user has the image in mind; the vision is the standard (ARCHITECTURE.md §2 principle 9, §7.0 R1–R8).
Predecessor `C:\Engine2` (three.js) was dropped: it stuttered on bare maps.

## How to work with this user (hard rules)

- **No outside engines**, ever (no Unreal/Epic, Unity, Godot). Open-source *libraries* are fine.
- **No shortcuts; the vision is the standard.** Size systems for the finished game. Prototypes test the real
  vision; perf tests use the real workload; when something hits a limit, research a better architecture
  before reducing the art. If a limit is unavoidable, say so up front with the number.
- **Never say "done"** unless verified. Say what changed and where they see it. No promises or dates.
- **Short answers**, ownership first. Own mistakes in one line, then fix them. No excuses.
- **Check the real latest version** of any library before adding it (`git ls-remote --tags` / GitHub API),
  and say where it came from.
- **Commit** locally on your own once a change is verified (builds clean, its test passes; user, 2026-10-14).
  The message states exactly what is and is not verified. **Never push** unless asked in that moment.
  Git identity repo-local: EvaristoCabrito <evaristo172@gmail.com>. Remote: github.com/EvaristoCabrito/EmberEngine.
- **Research before building** big visual features. Never show raw test renders as progress.
- **`C:\Engine2` = asset source only, NOT a reference** (user, 2026-10-15, replaces the 2026-10-14 "game design
  yes" rule): "there is no engine 2, it's EMBER ENGINE"; "never alter anything there"; "it only serves to extract
  assets"; "use nothing from there, create new and better, triple-A". Never copy or propose copying its camera,
  buttons, UI, rules or design; design every feature new to an AAA standard. Never call the project Engine2.
  **Use its real assets for every test** (maps, ground, decor, models, sprites, fx): never placeholders.
- **Priorities (user, 2026-10-14):** stability (no crashes, validation errors, streaming failures) → smooth
  gameplay (consistent frame times while moving the camera, animating, casting; not averages) → streaming →
  rendering foundation (2.75D depth, occlusion, real dynamic light) → final content. Keep "a hook exists"
  separate from "the feature works": every report says which.
- **Lighting direction (user, 2026-10-15, replaces the Engine2 floor for light):** realistic, AAA standard
  (physical light units, physical camera exposure, ARCHITECTURE §7.6); Engine2 is NOT a reference for light or
  sky ("so bad I threw it away"). Our own C++ / Vulkan, no HW ray tracing required.
- **Sprites (user, 2026-10-15):** the engine exists to fix Engine2's sprite animation; sprites must look at least
  as good as Engine2's (sharpness, colour, clean alpha) and every animation must play. **Sprites and combat come
  before any more sky / fog / atmosphere work** (I spent a day on moon/stars/fog the user could not even see).
- **The user cannot run the Sandbox.** After building something visible, OPEN THE WINDOW FOR THEM:
  `Start-Process build\release\bin\Sandbox.exe -WorkingDirectory C:\EmberEngine -ArgumentList ...` then bring it to
  the front (user32 ShowWindow/SetForegroundWindow via Add-Type). Never close their windows (they refused);
  rename a locked exe/pak aside and build/write a new one instead.
- **No photos** to the user (they judge only live in the window). Never offer choices that look the same in play.
- **Start work only on an explicit "faz" / "pode fazer".** A confirmation, a pasted spec, a complaint or a question
  is not a go-ahead ("who authorized starting anything?"). Ask one short question when unsure; never guess twice.
- **The window shows only choices** (the "Escolhas" panel: big Segoe UI font, short Portuguese names, OFF first
  where it applies, no numbers, no statistics); no keyboard shortcuts for settings. Every feature gets an OFF.
- **The user speaks Portuguese** (and is often furious): answer in Portuguese, short, ownership first.
- **2D/3D split (the one piece of Engine2's tech carried over):** characters, props and decor = photoreal 2D cards;
  houses/buildings and terrain shape = real 3D (Meshy for buildings); ground = photoreal texture on 3D terrain.
  Do not port Engine2's lighting/camera/ground rules, and do not ask what is 2D vs 3D.
- **Camera:** the user had NO trouble with the original camera (Engine2-style orbit: right-drag turn/tilt, wheel
  zoom, arrows/WASD pan the target; MapStage before the 2026-10-15 afternoon). The free fly camera + on-screen
  buttons I built that day broke it ("insanity"). A "tactical camera" was proposed but NOT accepted. Any camera
  change: discuss exactly what it means with the user first, build only on "faz".
- **Workload:** finished game = ×10 today's sprite roster (MCs at 2× data, max party 8; "testing ×10 is enough").
  World density: the Ember maps are a floor ("bare naked and started stuttering"); M3 tests 10× their density.
- **Progress gates are the user's three demonstrations** (§9): D1 lit sprites in a spatial world, D2 complete
  environmental scene, D3 cinematic stress scene. Engineering tests are foundation, not visual progress.
- **Never throw visual work away (user, 2026-10-14: "everything always degrades the more you try").** Every
  look is saved in a numbered series (fog: `looks/fog/README.md`, `shaders/fog_lookNNN.hlsl`, tag
  `fog-look-NNN`), switchable live, and kept until the user deletes it after seeing it. A change is a new
  look; never edit or overwrite a saved one. Commit before trying the next idea.
- Fold mid-turn messages into the work. Never touch `C:\emberashes03D-main`. `C:\Engine2` is read-only.
- **Measure before fixing.** Guesses get measured and reverted if wrong (thread priorities; the occlusion
  "fix" for the 15.8/5.6 ms alternation, which was really GPU power saving).

## Build and run (Windows, MSVC 2022 Build Tools, Vulkan SDK 1.4.363.0, CMake + Ninja)

**Work is local on this PC again (user, 2026-10-15 night: "estamos trabalhando local agora, esquece o jogar").**
Build with `build.bat` / MSVC here and open `build\release\bin\Sandbox.exe` for the user (rule above). JOGAR.bat and
`jogo/` (the cloud zig build, `tools/cross/README.md`) are no longer maintained: do not rebuild `jogo/` or push for it.
The game opens Engine2's **title screen** with `--title --ground build/ground/ground-v2.pak --sprites
build/sprites/battle.pak --meshes build/meshes/props.pak --scenes build/engine2-scenes --maps C:/Engine2/maps/ember
--heroes build/engine2-scenes/roster.json`.

```
build.bat release          # or debug; output in build/<config>/bin
```
- Perf tests in **release** with `--no-validation`; correctness in **debug** with validation (exit 2 = errors).
- `Sandbox.exe` (window, F1 overlay) / `--capture out.png` (off-screen).
- M1: `Sandbox --stream build/stream-test/stream-test.pak --verify build/stream-test/src`.
- M2: `Sandbox --sprites build/sprites/roster.pak [--budget 512]` (×10: `roster-x10.pak`; exit 3 = fail).
- M3 bring-up: `Sandbox --meshes build/meshes/props.pak --grid N`.
- **D1 port (current): a real Engine2 map in EmberEngine.**
  1. Engine2 data (runs Engine2's TypeScript read-only with its own vite-node; cache in the scratchpad):
     `node C:/Engine2/node_modules/vite-node/vite-node.mjs --root <scratch> tools/scripts/engine2/export_board.ts
     -- <map.json> content/engine2 build/engine2-ref` (board tables + reference geometry);
     `... export_scene.ts -- <map.json> build/engine2-scenes/<map>.scene.json` then
     `python -I tools/scripts/engine2/measure_art.py <scene.json> C:/Engine2/public` (the map importer);
     `... export_textures.ts -- build/engine2-ref/textures` (procedural ground reference).
  2. `Cooker check-ground content/engine2/board.json <map.json> C:/Engine2/public build/engine2-ref` (C++ board /
     ground vs Engine2: must print "ground matches Engine2"); `Cooker ground content/engine2/board.json
     C:/Engine2/public build/ground/ground.pak [--check build/engine2-ref/textures]`.
  3. `Sandbox --map C:/Engine2/maps/ember/aldeia015.json --ground build/ground/ground.pak --scene
     build/engine2-scenes/aldeia015.scene.json --sprites build/sprites/battle.pak --meshes build/meshes/props.pak
     [--look smooth|hard] [--moving-light] [--spell-light] [--measure 20] [--zoom F --turn D --tilt D --offset X Z]
     [--sun AZ EL] [--layers MASK: 1 sun 2 shadows 4 indirect 8 local]`. Window: right-drag turn/tilt, middle-drag
     grab, wheel zoom to cursor, WASD/arrows slide, camera bar at the bottom (MapStage.h top comment). Proof maps: aldeia015
     (lights, houses, photo ground), random-rimewater-shrine001 (water, 3D trees); stress: wisp-forest-2004.
  4. Combat: `... export_battle.ts -- <map.json> content/engine2/battles/<map>.battle.json` (Engine2's BattleEngine
     run headless: cells as movement sees them, units, Engine2's reach per unit; ENGINE2=<dir> for another checkout);
     `Cooker check-battle content/engine2/battles/*.battle.json` (must print "reach matches Engine2" per map); then
     Sandbox ... `--heroes build/engine2-scenes/roster.json --battle-rules content/engine2/battles/aldeia015.battle.json`.
- **M3 exit test:** `Sandbox --battle --meshes <pak> --sprites build/sprites/battle.pak --capture x.png
  [--content all|3d|cards] [--unpaced] [--cost-split 0|1|2]`. Reports per phase: GPU median / 99th / worst,
  per-pass averages, frames over budget; per-frame CSV in `build/battle-<phase>.csv`. **`--unpaced` = cost at
  full GPU clock**; paced 60 Hz numbers are stretched by the RX 6400's power saving (ARCHITECTURE §7.2).
- Proxy quality: `Sandbox --compare build/meshes/props.pak meshes/props/gothic-cottage/v01/gothic-cottage-v01.mesh
  build/meshes/proxy128.pak meshes/gothic-cottage/v01/gothic-cottage-v01.mesh build/compare` then
  `python tools/scripts/compare_meshes.py build/compare` (silhouette IoU + normal angle error by width).
- Cooker (see top of `apps/cooker/main.cpp`):
  - sprites: `Cooker sprites content/engine2.sprites.json C:/Engine2/public/game build/sprites/roster.pak`;
    battle pack = engine2 + `content/decor.sprites.json` (443 decor images) merged into
    `build/sprites/battle.sprites.json` → `battle.pak` (one pack: two packs both define `sprites/roster`).
  - meshes: `Cooker meshes C:/Engine2/public/game build/meshes/props.pak` (15 GLBs);
    proxy experiment: `Cooker meshes C:/Engine2/public/game/props build/meshes/proxy128.pak --remesh 128
    --cache build/meshes/cook-cache-proxy` (cottages only, separate pack; originals untouched).
- GPU here: AMD RX 6400 (4 GB, RDNA2, mesh shaders, wave64). 32 CPU threads.

## Code map

- `engine/core`: Log, Jobs, Function, Memory. `engine/platform`: Window (SDL3), IoQueue (IOCP).
- `engine/assets`: AssetId, Pak, AssetManager.
- `engine/rhi`: VulkanContext (requires VK_EXT_mesh_shader, samplerFilterMinmax), Bindless (sampled/storage
  images, samplers, **storage buffers** for atomics; push constants reach task/mesh), Image (+createDeviceBuffer),
  Pipeline (graphics incl. task/mesh, compute), Swapchain, GpuTimer, UploadQueue (images **and buffers**).
- `engine/render`: Renderer (frame constants → light clusters → **atmosphere tables + SH ambient** → sun shadow
  (cached: ground + meshes) → card shadow (per frame) → froxel haze → mesh passes (sky behind them in resolve) →
  ground → water → cards → **fog-of-war mist (FogLooks)** → tonemap → UI), **Atmosphere.h** (Hillaire constants,
  CPU sun transmittance), **FogLooks** (numbered mist looks), Camera, SpriteFormat/SpriteDraw
  (80-byte cards: own axis, unit depth pull, flame glow, lit flag, casts flag)/SpriteSystem (M2), MeshFormat,
  TextureFormat, MeshGpu, MeshScene, **BoardGround** (both ground looks + water on the GPU, map palette of
  baked layers), DebugUi.
- `engine/world` (D1 port): **Noise/Hex/Board/GroundMesh** (Engine2's board and ground, bit-exact),
  **GroundAssets** (ground texture names), **SceneData** (imported scene: cards, models, units, lights, the map's
  lighting sliders + mist type), **FogOfWar** (sight, mask; R7), **TimeOfDay.h** (Engine2's time-of-day data:
  battleLight + stage presets + moon pose).
- `tools`: PakWriter, SpriteSource, SpriteCooker, TextureCooker, MeshCooker, **GroundCooker** + **Engine2Textures**
  (Engine2's procedural ground, byte-identical), **GroundCheck**, `tools/scripts/compare_meshes.py`,
  `tools/scripts/engine2/*` (exporters + importer above).
- `apps/sandbox`: Stage.h (test-scene interface + window events), SpriteParade.h, MeshStage.h, BattleStage.h (M3 exit
  test), MeshCompare.h, **MapStage.h** (D1: a real Engine2 map, its scene, lights, `--measure`), StreamTest in main.cpp. `apps/cooker`: the Cooker CLI.
- `shaders`: HLSL via DXC (`-Zpr`: `mul(v, M)` with glm). `scene.hlsli` = GPU layouts shared with MeshGpu.h /
  MeshFormat.h / MeshFrame (Renderer.cpp, static_asserted); `cull.hlsl` (early/late instance culling, BVH
  traversal, draw args), `meshlet.hlsl` (task/mesh/pixel → visibility buffer), `hiz.hlsl` (depth pyramid),
  `resolve.hlsl` (barycentrics + PBR), **`lighting.hlsli` = the lighting contract** (Surface + shade(): sun with
  two shadow maps, clustered local lights, indirect hooks (hemisphere today), visibility hook (R7, returns 1),
  layer switches), `terrain.hlsl` (Engine2's ground material), `water.hlsl`, `sprites.hlsl` (lit cards, card
  shadows), `lights.hlsl` (cluster build), `tonemap.hlsl` (three.js's ACES fit, like the prototype),
  **`atmosphere.hlsl(i)`** (sky tables, SH ambient, sky background), `volume.hlsl` (froxel haze),
  **`fog_lookNNN.hlsl`** (frozen mist looks).
- Vulkan gotcha found: fragment shaders may not bind a writable storage buffer here (no
  fragmentStoresAndAtomics): read GPU-written lists through device addresses (`vk::RawBufferLoad`).
- DXC gotchas found: `clip` and `triangle` are reserved names; per-primitive PS inputs need
  `[[vk::ext_decorate(5271)]]` + `[[vk::ext_capability(5283)]]` (DXC issue #6862).
- Shell gotcha: writing C++ through bash heredocs + Python mangled `'\0'` and `"\n"` twice; write patch scripts
  with the Write tool into the scratchpad and run them with `python -I`.

## Status (2026-10-15)

- **M0–M2 done**, pushed. **M3** GPU-driven meshes built and verified, committed locally (battle test numbers
  and the proxy findings in ARCHITECTURE §7.2). **Proxy bake paused** (assets not final): unfinished code on
  branch `wip/proxy-bake`; settle it later with a representative final-quality building.
- **D1 port, committed locally, NOT pushed** (`93f5920` … latest). Verified, 0 validation errors in debug:
  - Engine2's board + smooth/hard ground + water + heights in C++, identical to Engine2's own output on 5 maps;
    its 105 ground textures cooked (procedural ones byte-identical).
  - Real maps drawn: ground (Engine2's material), water, decor cards, 3D trees, units from spawns with Engine2's
    sizes, anchors, facing and idle timing (map importer).
  - Lighting contract; correct energy (1/pi on indirect), exposure in the lights, three.js ACES; sun shadows
    (4096, soft, cached for ground + meshes; cards per frame, units casting toward the sun); clustered local
    lights with Engine2's falloff (14 on aldeia: braziers, forge, burning houses, lanterns + test torch + spell).
  - Gameplay measurement (aldeia015, 20 s camera turn + zoom, 14 units animating, 14 lights, release):
    full GPU clock: main thread worst 1.60 ms; GPU median 6.74 / 99th 7.22 / worst 16.15 ms; 0 frames over
    16.7 ms (PASS). Paced 60 Hz: GPU median 9.48 / 99th 19.47 / worst 22.60 ms; 318 of 1196 frames over
    16.7 ms (FAIL: the RX 6400 down-clocks when paced; whether frames miss vblank needs the windowed vsync
    test). The ground material is 5.2 of 6.7 ms at full clock: the first thing to cut. 14 lights (12 map
    light sources + test torch + spell light).
  - **Fog of war, tactical mask (R7), ARCHITECTURE §7.4:** Engine2's states and mask look; **line of sight is
    our own, occlusion-based** (Engine2's was never finished; user chose occlusion: terrain height + 3D models +
    cards >= 1.2 tall block). Every pass reads the mask; lights dimmed by their source's sight; never-seen casts no
    shadow; hidden enemies not drawn. `check_fog.py` PASS (0 never-seen pixels lit at 4 moments), 0 validation
    errors; full clock with fog + scout: GPU worst 8.33 ms, sight update 3.4 ms worst on the main thread.
  - Fog test: `Sandbox --map .../aldeia015.json ... --fog 1 --scout --moving-light --spell-light --scene-time T
    --no-ui [--debug-view 3]` (pairs) → `python -I tools/scripts/check_fog.py shaded.png mask.png ...`.
  - Stale `Sandbox.exe` processes from crashes can lock `build/<config>/bin/Sandbox.exe` and refuse to die
    (access denied): rename the locked exe aside, then build.
  - Never-seen ground, water, 3D pieces and cards are not drawn at all (user's choice); pieces/cards by anchor hex.
  - Sight rules since (2026-10-15): only things >= 1.2 wide block (no lamp posts); a never-seen hex with all
    neighbours in sight, and any enclosed never-seen group of <= 3 hexes, counts as seen (0 holes on the scout
    walk; `captures/fog/holes.txt` + `tools/scripts/draw_fog_holes.py` map any that appear). Sight + mask run
    on a worker (`2522456`): main thread worst 1.2-2.0 ms with fog + scout.
  - **Fog of war mist (R7's literal fog): 7 saved looks** (`looks/fog/README.md`, F9 / `--fog-look N`, tags
    `fog-look-NNN`, pushed). The user kept 003 (default); 004-007 not yet judged; 006 = Engine2's mist4 from the
    user's reference. Cost at full clock: look 003 3.9 ms avg GPU (frame 8.8 ms median, 11.0 worst with fog +
    scout). Froxel haze (`volume.hlsl`) is on only in lighting mode 0 (it has an unexplained vertical streak).
    The user keeps every look as a library (different scenarios, different fog); none get deleted.
  - **Sky + atmosphere (ARCHITECTURE §7.5, `de72f3c`), built, first look only:** Hillaire 2020 tables rebuilt
    every frame, SH sky ambient (scaled to the map's ambient slider), sky behind the board (sun disc, stars),
    aerial perspective (0.025 km per world unit). Lighting modes (F10, `--lighting N`): 0 old prototype day
    (+ froxel haze: reproduces every saved capture), 1 Engine2's 3D presets for the map's time of day (floor),
    2 physical (default): time of day places the sun/moon, the map's sliders set the strengths (Engine2's 3D
    never applied them: the user had suspected it), the air colours the light; indoor maps keep the hemisphere.
    Captures: `captures/sky/*.png` (aldeia015 is **dusk**).
  - **2026-10-15 afternoon, committed locally, NOT pushed, NOT accepted by the user ("I accepted nothing"):**
    - Night sky: moon disc with Engine2's phase art (`sky/moon/<phase>.tex` in the ground pack), 3 placements
      (`--moon-path`; the user found them identical in play: the sky is off screen), moon size choice, star looks
      001-003 (`looks/stars/`, tags `star-look-NNN`), sky on mesh-less maps (CSSky pass). Low value: the game
      camera never sees the sky.
    - **Painted map backdrops** (the big visible win): Engine2's `public/game/map-backgrounds/*` (39 images) cooked
      into the ground pack as `backdrop/<file>.tex`; `export_scene.ts` writes the map's `backdrop` (Engine2
      `editor/mapBackgrounds.ts resolvedMapBackground`); drawn behind everything, cover-fit (resolve.hlsl
      `backgroundAt`); panel "Fundo": imagem do mapa / céu / OFF. aldeia015 = aldeia-bg.jpg, cemiterio =
      CemiteryBackground.jpg. **Use `build/ground/ground-v2.pak`** (ground.pak was locked by an open window).
    - Third board look **"mesa" (tabletop)**: `GroundMesh::buildTabletopGround` (diorama.html look 3: top at 0.93,
      bevel 0.07, walls 0.985 to the base, seam every height step in terrain.hlsl). The user then specified real
      pieces (NOT built, not authorized yet): each hex one pure piece of its own terrain, texture fitted per piece
      with per-piece rotation/offset, sides of that terrain's material, stacked levels.
    - Unit viewer: `tools/scripts/engine2/export_heroes.ts` exports every Engine2 unit (97, 293 poses, all in
      battle.pak) with Engine2's boxes and pacing → `build/engine2-scenes/roster.json` (`--heroes <file>`); panel
      "Animação" (Parado, Andar, Atacar, Conjurar, Curar, ...): one-shot poses replay every 3.5 s. Units are laid in
      rows of 12 at the board centre. The user then said: **just repopulate the map as it is** (the map's own units,
      each playing the chosen pose from its own catalog entry: look it up by the scene unit's `key`) - written in the
      cloud session (see Next 3), not built yet.
    - Panel "Escolhas" (Segoe UI from the Windows fonts folder via GetWindowsDirectoryA; the WINDIR variable is
      missing when launched via PowerShell Start-Process, which left the pixel font); performance overlay hidden in
      map windows; camera buttons box (top right) for the free camera.
    - Measured: sprites are NOT damaged by cooking (sprite-preview: cooked mip 0 identical to the source); in game
      Aldric is ~1.7x brighter with more contrast than his art (the lighting washes cards out). 97 units on screen
      stutter: likely the 1 GB sprite budget (full clips 30-60 MB each) thrashing - not measured yet.
  - **Power cuts here (user, 2026-10-15):** build with `CMAKE_BUILD_PARALLEL_LEVEL=4`, no full-clock GPU runs
    unless asked, commit often; after a crash, scan recent files for zero-filled content (346 build outputs were
    zeroed and set aside in `build/release-zeroed-20261015`). The paced 60 Hz FAIL ran under power saving.
  - DXC gotcha: passing the AtmosphereFrame (has a uint64_t) to a function by value read `moonTexture` wrong;
    pass fields one by one (`moonDisc`). ImGui is 1.93: `PushFont(nullptr, size)` (no SetWindowFontScale).
- **Not done (hooks only or missing):** walls not imported (block no sight); the sky step's open items
  (see Next 1); bloom; depth of field / tilt-shift; local lights cast no shadows; cards
  do not receive card shadows; walls/doors (architecture), floor waypoints, stand-on-log heights, the third
  board look (tabletop tiles, R8) not ported; unit animation is the idle only; no spell VFX; the ground material
  costs 4.1 ms at full clock on photo-layer maps (profile it before optimising).

## Cloud session 2026-10-15 (evening): the game around the map, copied from Engine2 (user: "copiar do engine 2",
"todo conteúdo gráfico"; built, NOT run here: no GPU)

- **Title** (`apps/sandbox/TitleStage.h`, Engine2 index.html + title.css): title-bg.jpg under the veil, EMBER ASHES, Ember,
  ember menu buttons (Nova campanha / Como jogar / Opções / Sair); mission list on the save-board art (every scene in
  `build/engine2-scenes` whose map exists; battles first); **briefing** (BRIEF_ART, briefing + objective, Entrar em combate).
  Fonts bundled (`content/fonts`: Pirata One, Newsreader, Figtree, OFL); Engine2 art read at run time via ImGui user
  textures (`render/UiArt`: images, ember-panel / rule / slot / kicker / title / big ember button).
- **Screens switch in `main.cpp`** (title -> map -> back); Esc pause menu (Continuar / Menu principal); **result screen**
  (Engine2 ResultScreen: Vitória/Derrota · T, hero growth, Mapa / Tentar de novo).
- **Battle HUD** (Engine2 GameApp footer): portrait, name · Nv · XP, class, HP, sheet line / forecast; Atacar / Esperar /
  Cancelar / spell sockets with Engine2 icons + uses / Fim do turno; HP bars over units (battleHud3d.ts); "Ordem" chips.
  The battle grid is painted on the ground by the terrain pass (`Renderer::setGroundMarks`).
- **Spells** (`engine/world/Spells`): Míssil Mágico, Tiro Longo, Tiro Perfurante, Investida Perfurante, Golpe Duplo,
  Relâmpago (+echo), Frost (game/frost.ts), Choque, Cura Menor; training, XP, level-ups, staff element/drain, kill-drop rolls,
  start-of-turn echo + hazards; enemy AI casts (cultists, crossbowmen). Verified against Engine2's game/engine.ts with
  `SPELL_REFS=1 export_battle.ts` + `Cooker check-battle` (every aim, board afterwards, random-number count) on the proof
  maps. The user told me to stop exporting all 242 maps ("most are badly made and not final"): proof maps only.
  Not ported: area / summon / status spells (fireball, cleave, bless, poison, bleeding, sleep, ...).
- **Atmosphere**: fog look 008 = the map's own mist (Engine2 mist3d presets, intensity, speed; no fog of war needed);
  Engine2 wisps + dust motes; full-scene bloom (map bloomIntensity) in the tonemap pass. Real light: lit cards' cap fixed
  (sprites were black). The user: sky / moon / stars **later** ("deixa o céu para depois"); they want the painting at the
  horizon with the real sky above it (not done).

## Local session 2026-10-15 (night): the cloud code built on the PC

- Release + debug build clean (MSVC). Fog look 008 (the map's own mist) removed (`ace00d8`): it lost the GPU
  (VK_ERROR_DEVICE_LOST) on every map; cause not found.
- Debug + validation: aldeia015 with scene, units, poses, battle rules: capture exit 0, 0 errors; title screen
  window 25 s: 0 errors. **Not verified:** clicking through title → mission → battle, Tática, combat play.
- `jogo/Sandbox.exe` now the local MSVC release build (needs the VC++ runtime, present on this PC); shaders
  unchanged. The previous cloud exe still had look 008. Tested: aldeia015 capture exit 0. NOT pushed yet.

## Next

The user's verdict (2026-10-15): the engine as seen is worse than Engine2 ("worst colours and render I've ever
seen", camera "nonsense", stutters with all units, sprites washed out, nothing visible improved). Order, each step
only on the user's explicit "faz", shown live in the window:
1. **Camera: new orbit camera built (2026-10-15); the user: "more or less right, skip it for now".** MapStage.h
   top comment: glide on every input, right-drag turn/tilt free (5-89.5 deg), middle-drag grab, wheel zoom to
   cursor, WASD/arrows slide with the pivot clamped to the board (edge reaches screen centre, user's rule), eye
   kept above ground and 3D pieces. Free fly camera and big box removed.
   **Camera bar + Tática (2D view), user's explicit request (2026-10-15): "the bar can't sit at the bottom: top of
   the screen, right side, copy 1:1 from Engine2"; "2D mode is a must have".** This overrides the "never copy
   Engine2's UI" rule for this bar only. Written in a cloud session (Linux, no GPU): NOT BUILT, NOT RUN, shaders
   not compiled; C++ passed `clang++ -fsyntax-only` only. Source: Engine2 `src/game/GameApp.tsx` (HUD bar),
   `src/render/cameraRig.ts` (`flat`, FLAT_FOV 1, FLAT_ROW_STEP 0.002), `src/world/decor.ts face()` +
   `src/units/actor.ts update()` (flat cards), `src/ember-ui/styles.css .ember-plate`. Bar top right, 8 px from
   the corner: [NORMAL/TÁTICA] ▼ ▲ (tilt 5°) ◀ ▶ (turn 15°) ↺ (normal angle, leaves Tática, zoom/pan kept); ImGui
   draws the plates and lucide icons by hand. Engine2's sound / Opções buttons were left out (no such features).
   Tática: eye straight above the pivot through a 1° lens at distance × tan15°/tan0.5°, near = distance − 200,
   up hint −z (`Camera::lookAt` gained `upHint`); no turning; cards and units `kSpriteFlat` (shaders/sprites.hlsl:
   height along −z, normal +y) at boardTop + z × 0.002 (units + half a step), no shadows, no unit depth pull;
   aerial perspective (kmPerUnit 0) and froxel haze off. Renderer: light-cluster slices now start at the near
   plane (`clusterNear = max(2, near)`, far = near + 398; normal lens unchanged at 2..400). To check on the PC:
   build, toggle Tática on aldeia015 (cards readable, rows overlap right, lights still lit, mist OK), debug run
   with validation.
2. **Measure and fix the stutter** (asked, not started; confirm first): frame times with every unit on screen, sprite
   residency vs the 1 GB budget; fix the real cause (budget sized for the real workload, not fewer units).
3. **Repopulate the map as it is** with the Animação choice driving the map's own units. Written in the cloud
   session, NOT BUILT: `--heroes build/engine2-scenes/roster.json` is now the pose catalog (`Hero::key`, read
   from export_heroes.ts's `key`); each scene unit finds its entry by `key` and plays the chosen pose with that
   pose's box / foot / pacing (one-shots: doneAt + 0.5 s, then again), keeping its own mirror; units without the
   pose stay idle; unknown keys logged. The panel lists only poses some map unit has, Parado first. The rows of
   97 test units and their camera override are gone. The existing `roster.json` already carries `key`.
4. **Combat control** (user: "it's to create the damn game", 2026-10-15). **Step 1 written in the cloud session:**
   `engine/world/Battle.h/.cpp` = Engine2 pathfinding.ts (footprint, footprintCost, computeReachable incl. its stable
   re-sorted queue and neighbour order, reconstructPath) + move / newTurn. **Verified here (Linux, no GPU):** C++ reach
   identical to Engine2's (hexes, costs, parents) for every unit of aldeia015, random-rimewater-shrine001,
   wisp-forest-2004 (56 units, 2487 hexes) and on all 242 maps in Engine2's maps/ember (2682 units, 246304
   hexes, 0 differences); paths and spent movement checked.
   Data: `tools/scripts/engine2/export_battle.ts` (runs Engine2's own BattleEngine headless with vite-node; the three
   proof maps' output is committed in `content/engine2/battles/`, from Engine2's GitHub HEAD, Oct 14: re-export from
   C:/Engine2 if its maps changed). MapStage (`--battle-rules`), **NOT BUILT / NOT RUN**: left click picks a unit (its
   card, or its hex) and shows its reach in Engine2's battle-grid colours (Battle3D gridMarks: movement wash + edges,
   enemy wash, route, active marker, cursor; drawn as an ImGui background overlay, so it lies over unit feet: a ground
   decal pass is the proper home later); hovering a reachable hex shows the route; clicking walks it (Engine2
   UnitActor: 0.22 s per hex, facing and back/front/side walk cuts relative to the camera, directional-walk sprites
   never mirrored, Lancer's swapped cuts); remaining movement can be used again; "Fim do turno" (bottom right plate)
   restores movement; Esc lets go. Units keep live positions (fog sight follows them).
   **Step 2, attack (cloud session):** `engine/world/Combat.h/.cpp` = Engine2 combat.ts (makeForecast,
   previewDamage, rollDamage, canCounter, isRearAttack, terrainBonus) + pathfinding.ts canHitFrom / clearShot /
   shotBlocker / hexLine + BattleEngine.affinityUnit and its arcane / staff element step on a hit, with JS number
   rules and Engine2's mulberry32. export_battle.ts now also writes each unit's combat profile (weapon, mastery,
   off-hand, staff magic, resistances, heading), the party's affinity bonuses, and reference forecasts + seeded rolls
   for every pair of foes (from the attacker's hex and every hex around the defender). **Verified here:** aldeia015
   462 forecasts + 462 seeded rolls identical (all-maps run: see the commit message / check output); a simulated
   fight runs to a death with counters. MapStage, **NOT BUILT / NOT RUN**: foes the picked party unit can hit from
   where it stands are marked in Engine2's target red; hovering one shows the forecast (ember plate by the cursor:
   names, target HP, damage + hit %, "mata", counter or "Sem contra-ataque"); clicking attacks: both face each other
   (board heading too, for rear attacks), attack pose (left cut when facing left), roll at 55% of the pose, number
   pops (gold, larger on a crit; "Errou"), hit pose or death pose (held 1.5 s, then the body is gone; the dead stop
   seeing and stop blocking), then the counter when canCounter. Attacking ends the unit's turn (acted; Fim do turno
   restores it).
   **Step 3, turns + enemies (cloud session):** `engine/world/BattleTurns.cpp` = Engine2 sortByInitiative (one
   battle-opening 1d20 + class bonus per unit, exported as `initiativeBonus`; highest first, party wins a draw, then id;
   id order is byte order here vs JS localeCompare, which only matters on an exact draw), activeTurnUnit,
   startNewRound (volatile enmity -300, everyone acts again, order re-sorted), beginUnitTurn (movement pool whole),
   enmity.ts (weapon damage: hero->enemy +1 ce +3 ve per point; enemy->hero -2 ce; cap 10000), terrainDistanceField
   (bucket Dijkstra) and runAiFor's plain branch (best strike over its reach: (maxHp-hp)*3 + ground def*2 + 20 if the
   foe has <= 8 HP; else close in on the nearest hero by walking distance). export_battle.ts runs Engine2's own runAiFor
   per enemy on a fresh battle (at the start, and placed 2 hexes from the lead hero). **Verified here:** C++ decisions
   identical (walk + target) on the proof maps: 57 decisions, 19 casts skipped (spells not ported: those enemies swing
   their weapon instead). A headless C++ battle on aldeia015 runs rounds in initiative order to a result.
   MapStage, **NOT BUILT / NOT RUN**: the party's unit whose turn it is gets picked; it walks (pool can be split),
   strikes once (Engine2 finishAction: the turn ends when no movement is left), foes reachable from any hex of its reach
   are marked (attackableEnemies: first hex in reach order), clicking one walks there and strikes; "Fim do turno" ends
   its turn; enemies play their turns (walk, then strike + counter); fog: an enemy holds until the party sees its hex
   (stand-in for Engine2 wakeIfSeesParty); top-centre plate "Rodada N · vez de X", centre "Vitória" / "Derrota" when a
   side has nobody standing (Engine2 mission win rules like bosses / waypoints not ported). Not yet: spells and
   abilities (cultists, crossbow long shot, ...), off-hand strikes, XP / level-ups, poison / bleed / status, hazards,
   neutrals, undo move.
5. **AAA light, step A written in the cloud session (user: "I want the atmosphere, the light"), NOT BUILT / NOT
   RUN:** light mode 3 "real" (default; panel "Luz": protótipo / Engine2 / física / real; `--lighting 3`): sun
   128,000 lux above the air through the atmosphere's transmittance, the physical sky (Hillaire tables lit by the
   same sun) as the ambient (`Lighting::physicalSky`, Renderer ambientTarget < 0 = the sky's own SH x multiplier,
   atmosphere.hlsl), full moon 0.5 lux x phase, indoor fill 3 lux + ambient 1.5 lux, map sliders as multipliers;
   dawn / dusk sun at 6 / 4 degrees; lamps: Engine2 intensity x reach^2 x 12 lm (lantern ~915 lm, fire ~1240,
   forge ~1700, candle ~200) as flux / 4pi cd with 1/d^2 falloff (shader's decay 2), range at 0.03 lux; physical
   camera: incident meter EV100 = log2(E / 2.5), darkening curve (0 by day, 0.3-0.4 stops at dawn / dusk, ~3.3 at
   full moon, ~3.7 on a dark night), eye adaptation 1.5 / s, exposure 1 / (1.2 x 2^EV); painted flames glow
   exposure-relative (0.15 by day, full at night). Expected (computed here, ACES display values, ground albedo
   0.25): day ground 0.41, dusk 0.31, moonlit ground 0.02 with a lantern pool 0.64 at 2 m / 0.13 at 5 m. Not yet:
   GPU metering (the meter is computed from the light, not the frame), night vision, bloom, AO (steps B-C).
   Then sprite look in light (cards washed out / flat).
6. Tabletop pieces as specified above; the redo of the sprite path if it still falls short of Engine2.
Skipped until final assets exist (user, 2026-10-15): the ground material's cost.

## Waiting on the user

- Video → sprite pipeline (their earlier top priority, ARCHITECTURE §5): needs their videos organised as
  `content/video/<Character>/<move>.mp4` (they sit in `Downloads` / `Desktop\Characters` with generated names;
  don't guess the mapping). ffmpeg 9.0.1 and Python + PIL are installed.
- Reference images / one clearly described moment, to turn R1–R8 into D1 pass/fail criteria.
- The final camera angles.

## Later milestones

M4 world cells + terrain · M5 lighting/atmosphere · M6 editor · M7 gameplay port + cutscene timeline.
Exit tests and demonstration gates in ARCHITECTURE.md §9.
