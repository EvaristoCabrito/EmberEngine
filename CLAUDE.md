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

- **Forget Engine2 exists (the user, 2026-10-20: "eu não quero mais nada baseado no engine 2", "esquecer que ele existe").** It
  overrides every Engine2 mention in this file: never base a design, number, rule or look on it, never run or propose parity checks
  against it, never write its name in new code, commits, docs or replies. The game's own rules (Como jogar, the spell tables) are
  EmberEngine's. Data and art the game already loads stay as they are; nothing new comes from there.
- **The acid arrows are "Milf's Acid Arrow" in every language (the user, 2026-10-22: "eu serei processado"):** Neera's `venomArrow` and
  the Corrosive Slimes' `acidArrow`. Never write the AD&D original name (an "e" in place of the "i") anywhere: code, data, docs, commits, replies.
- **D&D = Advanced Dungeons & Dragons 2nd edition ONLY (the user, 2026-10-20: "advanced dungeons and dragons second edition ONLY",
  "its the only D&D system accepted").** Whenever a rule is based on D&D (spell reach, areas, scaling), use AD&D 2e's rules, never
  3e / 3.5 / 4e / 5e. Basing is not copying: the user's own rules come first, AD&D 2e only supports them.
- **Never change any fog (the user, 2026-10-20: "nunca mudar nenhuma névoa mesmo se eu implorar").** Not the fog of war's rules
  (sight 12), not any mist look, not the Atmosfera. A new fog may only be added as a new numbered look when the user asks for one.
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
  Git identity repo-local: EvaristoCabrito <evaristo172@gmail.com>. Remote `origin`: github.com/EvaristoCabrito/EmberEngine
  again (the user, 2026-10-21: "vamos voltar a usar esse aqui por hora"); github.com/EvaristoCabrito/EmberVulkan (origin
  2026-10-18..21, last pushed 460387a) is remote `embervulkan`.
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
- **Do not open the game window for the user anymore** (user, 2026-10-16: "não precisa mais abrir a janela, eu tenho
  um executável"): they run JOGAR.bat (= the latest `build\release\bin\Sandbox.exe`), so finish with a release build
  and say where to look. Never close their windows; rename a locked exe/pak aside and build a new one instead.
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
- **Free roam (user, 2026-10-17, do not make them explain it again):** on EVERY map with fog of war, no enemy in sight =
  free movement automatically; an enemy coming into sight = turn-based combat; no enemy in sight again (dead OR just out of
  view) = free movement again. Never depends on the editor's "explore" flag. Such maps end only at a waypoint. Entering the
  fight: movement pauses 1.8 s under "Inimigos avistados!" (user, 2026-10-18), then turns; going back to free roam says
  nothing (MapStage::updateRoaming / enemiesInSight / sightPause). **Don't build or capture more than needed: tokens.**
- **Never throw visual work away (user, 2026-10-14: "everything always degrades the more you try").** Every
  look is saved in a numbered series (fog: `looks/fog/README.md`, `shaders/fog_lookNNN.hlsl`, tag
  `fog-look-NNN`), switchable live, and kept until the user deletes it after seeing it. A change is a new
  look; never edit or overwrite a saved one. Commit before trying the next idea.
- **Each map is its own diorama (the user, 2026-10-19, project rule): "Cada mapa é um estilo de diorama diferente e vai ter
  técnicas diferentes aplicadas, nada é padronizado para todos em hipótese alguma."** Every visual technique (background, camera
  limits, ground image, look) is enabled per map by its id, one map at a time; every other map keeps exactly its old look.
  Before a release, capture another map and the Tática view to prove they did not change.
  **aldeia (map id "aldeia", aldeia015) is the first:** Fundo "imagem alinhada ao céu" by default (resolve.hlsl backgroundAt
  mode 2, `content/engine2/backdrop-fit.json` entry aldeia-bg.jpg: painting under the real sky, ruins on the horizon, the user's
  AldeiaGround as `content/backdrops/aldeia-ground-v01.jpg` flat under the board, the only image in Tática), camera tilt down to
  -8 deg (MapStage `minPitch()`); the user approved the two lowest-tilt views. Every other map: unchanged (cover-fit painting, 5 deg).
  JOGAR uses `build/ground/ground-v7.pak` (v6 + the user's "Paisagem Devastada" as aldeia's "panorama 03", the default there: its sky
  is true alpha, resolve.hlsl backgroundMode 4 / backdrop-fit "alphaSky"; content copy with colour bled into the clear pixels).
  v6 = v5 + the user's "aldeia fundo2" as aldeia's "panorama 02",
  and Ashen Forest's new background `ashen-forest-bg-v02.jpg`; its briefing is `content/ui/briefing/ashen-forest-v02.jpg`; the
  v01 files stay).
- **Never delete or throw away anything (the user, 2026-10-22: "não apagar as coisas sem entender o pedido, não jogar fora NENHUM
  ASSET sob nenhuma condição").** No code, asset, art, map or data is removed unless the user explicitly orders that specific
  removal; a correction or a clarification is never a delete order (stop, keep everything, ask one short question). Whatever was
  made by mistake or is not used stays in place and gets a line in "Kept, not in use" below.
- Fold mid-turn messages into the work. Never touch `C:\emberashes03D-main`. `C:\Engine2` is read-only.
- **Measure before fixing.** Guesses get measured and reverted if wrong (thread priorities; the occlusion
  "fix" for the 15.8/5.6 ms alternation, which was really GPU power saving).

## Build and run (Windows, MSVC 2022 Build Tools, Vulkan SDK 1.4.363.0, CMake + Ninja)

**Work is local on this PC again (user, 2026-10-15 night: "estamos trabalhando local agora, esquece o jogar").**
Build with `build.bat` / MSVC here. **The user still double-clicks `JOGAR.bat`; it now runs
`build\release\bin\Sandbox.exe`, so every release build reaches them by itself** ("tem que atualizar sozinho"):
always finish a change with a release build. `jogo/` (the cloud zig build) is no longer used.
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
    battle pack = engine2 + `content/decor.sprites.json` merged into
    `build/sprites/battle.sprites.json` → `battle.pak` (one pack: two packs both define `sprites/roster`).
  - **2D decorations = the PNGs in `decoracoes-2d/` (the user's folder, 2026-10-21: they edit, delete and add there by hand).**
    `python -I tools/scripts/sprites/decor_folder.py` makes the game follow it (clip list, battle manifest, editor catalog: files
    gone = decorations gone, duplicates → the kept copy via `content/decor-dupes.json` (SceneData::load too), the user's own
    pieces from `content/decor-own.json` as `ember-*` ids, section "Árido"), then cook battle.pak. New art on one sheet:
    `tools/scripts/sprites/split_decor_sheet.py <sheet> decoracoes-2d <name>...`. Editor thumbnails read the folder.
  - replaced art (the user's own frames for an Engine2 clip, e.g. Salazar's walk right v02): `content/sprite-overrides.json`
    + `content/sprites/<set>/<clip>-vNN/`; run `python -I tools/scripts/sprites/apply_sprite_overrides.py` after
    fx_sprites.py / export_heroes.ts (patches the manifest, the roster's pose box / pace, walk-strides), then cook again.
  - walk cadence: one stride = `kWalkStride` world units for every unit (MapStage.h), clip strides in
    `content/engine2/walk-strides.json` (also copied beside the roster); never scale it by the card's height again.
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
  `[[vk::ext_decorate(5271)]]` + `[[vk::ext_capability(5283)]]` (DXC issue #6862). A 3D-texture `SampleLevel` whose
  coordinate was built inline (`float3(uv, 1.0 - 0.5 / float(max(f.volumeSlices, 1u)))`) read 0 on one code path
  while a debug branch read the right value at the same coordinate (2026-10-16, lighting.hlsli): compute the
  coordinate into a local first, in a small helper (`volumeFarSample`). **It struck again in `volumeTransmittance`
  (found 2026-10-17): every spell effect (flipbook cards and the ported vfx) was invisible whenever the Atmosfera was on
  (the default since 2026-10-16 afternoon); fixed the same way (`volumeSampleAt`). Any new `SampleLevel` on the volume
  goes through a helper. `--atmos 0` is the first thing to try when an effect does not show.**
- Git after a power cut / app crash (2026-10-16): `.git/index`, `refs/heads/main`, `COMMIT_EDITMSG` and the reflog tails
  came back zero-filled while the commit objects survived; the repair = write the last commit's hash into
  `refs/heads/main` (find it with `git fsck --lost-found` / the objects written last) and `git reset` to rebuild the
  index. Backups of the damaged files were kept as `*.corrupt-20261016`.
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

## Local session 2026-10-15/16: Engine2's menus, in order (user: "faz nessa ordem": slots, world map, Inn,
inventory / equipment, dialogs / intros)

- **Menus readable:** one UI canvas 720 tall zoomed to the window (DebugUi::kUiHeight); full screen by default
  (`--windowed` / `--size` for a window). JOGAR.bat runs `build\release\bin\Sandbox.exe` (every release build reaches the
  user). Cancelar undoes walks one at a time.
- **Save slots (done, `2844fac`):** `engine/world/Campaign` = save.ts bank (6 slots, SaveData v19 as insertion-ordered
  JSON, `%LOCALAPPDATA%\Ember\ember-save-bank.json`, temp + rename); `Cooker check-campaign` (slotProgress = Engine2).
  Title: Nova campanha / Continuar / Como jogar / Opções / Sair + "Modo teste" (old map list). `CampaignUi.h` SlotScreen
  (new / load / in-game save), briefing, ember plates, lucide icons.
- **World map (rules `861ea11`, screen after):** `engine/world/Overworld` = overworld.ts + access / progression /
  quests / hunger / poison / skills / loot helpers, `Cooker check-overworld content/engine2/overworld.json
  build/engine2-ref/overworld-refs.json` (matches Engine2: 1155 seeded steps, 349 events, 4746 step pairs, 105 status
  checks). `export_overworld.ts` writes both. `apps/sandbox/OverworldStage.h` = OverworldMapScreen: header plates,
  world-map.jpg at 4 zooms + drag, fog, time tint, pins, leader idle sprite, step hexes, party panel, events, location
  list, quest log, Party (leader, formation, affinities, Skills training), first-visit explainer, Opções (Salvar /
  Carregar / Voltar / Tela cheia), briefing → fight → back to the map (main.cpp finishCampaignFight: levels / XP / HP,
  completed). Captures: `--title-screen map --save-bank F [--map-ui party|skills|quests|options|move|location:<id>|save]`.
  Campaign maps exported so far: vau029, bosque005, aldeia015, thebridge022, wisp-forest-crossing012 (scene + battle).
- **The Inn (done):** `engine/world/Inn` (GameApp's Inn handlers: meals, Adega potions / lockpicks, rations, night's rest,
  healer Cura Média + cures, Vargan buy / enhance / sell / equip, NPC talk / quests) on `content/engine2/inn.json`
  (`export_inn.ts`); `apps/sandbox/InnScreen.h` (salão with Brue / O Mudo / Cliente Suspeito / A Hóspede, rest, quests,
  Adega, Ferreiro, Curandeiro). The Inn pin opens it. Engine2 reaches Vargan and the healer by walking the Inn's own map
  (not ported): both are header buttons here. Captures `--map-ui inn|inn:smith|inn:healer|inn:scrollN`. UI image pool
  raised to 4096 (DebugUi). Not verified against Engine2 by a check (simple arithmetic, read off GameApp).
- **Inventory (done):** `apps/sandbox/InventoryScreen.h` = InventoryScreens.tsx (Equipar: paper doll, 12 sockets,
  picker; Mochila: party cells, hero's potions / lockpick, item sheet with Equipar em / Usar / Jogar Fora); rules in
  InnRules (equipSharedItem / unequip, discards, useHeroPotion with healing training, useRation, bag used / capacity).
  Opened from the map's portraits and the Inn / forge headers. Captures `--map-ui pack|doll|inn:pack`. No Status button.
  Gotcha: never `for (... : json.value(k, ...).items())` - items() of a temporary dangles; bind the copy first.
- **Films + dialogs (2026-10-16):** `engine/media/VideoPlayer` (Media Foundation decode on a worker, RGB32 rows by the
  type's MF_MT_DEFAULT_STRIDE, SDL audio stream = the clock); `CutsceneScreen.h` (cover-fit picture, pt-BR WebVTT
  subtitles near the bottom edge, sound toggle, Pular); films at: new campaign (title-open + vau-intro), first Wisp
  Forest mission (wisp-entrance), first Inn visit (inn-arrival), first forge (smith-intro), aldeia / thebridge / templo
  briefings, victory on templo / portao (epilogues). `DialogScreen.h` + `BattleTalk.h` (DialogOverlay): the mission's
  intro as the battle opens, outro on victory, battle held (`MapStage::holdTurns`), recorded in dialogsSeen
  (`intro:<mission>:<tree>`), reply affinity into the save (adjustHeroAffinity + changeAffinity); Modo teste records
  nothing. Data: `export_dialogs.ts` → `content/engine2/dialogs.json` (38 missions, portraits per line). Captures:
  `--map-ui film:game/<f>.mp4@<s>` on the world map, `--map-ui talk:<mission>[:outro]` on a battle. Debug + validation
  0 errors. Not ported: battle NPC dialogs (hud.pendingDialog), companion conversations, Engine2's English tracks.
- **Another session worked in this checkout on 2026-10-16** (spell effects, Modo teste; it touched OverworldStage.h,
  CampaignUi.h, Overworld.*): the user stopped it. Its uncommitted MapStage.h edits are its own: never commit them.
- **Not ported yet:** field spells (Create Food and Water, Warp), the status sheet, finishBattle's loot /
  ember / rations / quests / progression, formation applied to battle spawns, unitHp at battle start.
- **Testing:** never drive the user's PC (computer-use) — captures and Cooker checks only; the user plays it.
- **Modo teste (2026-10-16, `a06875c` / `954891e` / `0151cb0`; user: "finish test mode just like Engine2", all of it
  incl. the Map Editor):** the title's "MODO TESTE" opens `TestModeStage.h` (O que abrir?: Debug, Map Editor, Dev
  Controls, Teste de Magias). Debug = MapChoice (Classic / RPG, once per session), the classic WorldMapScreen +
  Cenários, or OverworldStage on the session's own save (`TestSession`, never the bank; Overworld `test` flag, checked
  by `Cooker check-overworld` incl. 335 test steps). Test battles: `build/engine2-test` for all 148 campaign maps
  (`tools/scripts/engine2/export_test_missions.sh`: make_test_map.ts adds the missing heroes, level 15, new save's
  gear, debugFreeCast). Dev Controls = `DevGfx.h` (quality presets, shadows / resolution / soft, map lights, fog, fog
  debug view 4, sun sliders; `%LOCALAPPDATA%\Ember\dev-gfx.json`). **Map Editor** = `MapEditorStage.h`: Engine2's
  panel + a live MapStage preview of the draft in memory (`engine/world/SceneBuilder`: scenes built in C++ from
  `content/engine2/editor-catalog.json`, identical to export_scene.ts on 148 maps: `Cooker check-scene-builder`);
  brushes, undo, Salvar to `content/maps/ember/<id><serial>.json` (Engine2's format; Engine2's folder untouched),
  Testar (Engine2's exporters on the draft, ~6 s, then the battle; back to the editor after). Captures:
  `--title-screen test|test-choice|test-classic|test-scenarios|test-rpg|test-dev|test-editor`, `--editor-script`,
  `--dev-gfx FILE`. Not ported: water / 3D walls brushes, Gerar terreno, Locais / R-Encounter, dialog editor; the
  campaign does not read maps saved by the editor yet; Dev Controls' VFX lab / HD-2D scene / contact shadows / AO.
- **Several sessions share this checkout:** commit only your own hunks (stage HEAD + your edits, build that index
  alone in a scratch folder, then commit); never `git add` a file another session is editing.
- **Como jogar, Opções, battle Esc menu = Engine2's own, PT-BR + EN (2026-10-16; user: "both languages ported with
  the options").** `apps/sandbox/EmberMenus.h`: HelpModal (GameApp HelpModal: Básicos / Usos / Dano / Fórmulas / Loot,
  from `content/engine2/help.json`), OptionsDialog (OptionsMenu.tsx: Jogo, Gráficos, Áudio, Idiomas, Acessibilidade;
  title, world maps, Modo teste, battle), BattleMenu (Velocidade 0.36/0.22/0.12 s per hex, poses at 0.4/0.65/1 pace;
  Áudio; Opções; Continuar; Desistir = back to the world map / Encerrar teste; Tela inicial). `GamePrefs.h`: the
  player's choices in `%LOCALAPPDATA%\Ember\game-preferences.json` + `tr()` (Engine2's uiText: its dictionary
  `content/engine2/ui-text.json`, or inline English). `DevGfxSettings.h` = DevGfx without MapStage (Gráficos acts live
  in a battle, MapStage::applyGfx). Data: `export_help.ts -- content/engine2/help.json content/engine2/ui-text.json`.
  In English: title, save slots, these windows, film subtitles (`.en-v2.vtt`); dialogues use a line's translation
  when Engine2 has one (it has none yet: they stay Portuguese, Engine2's rule); other screens are still Portuguese.
  Left out: battle Zoom (user: later; the battle already zooms with the wheel) and GameApp's "Regras" button (user:
  not wanted). Captures: `--lang pt|en`, `--title-screen help:T[:scroll] | options`, `--map-ui menu|options` on a battle.
- **Sound + Save / Load in battle (2026-10-16/17), committed. Verified:** `--audio-check` 101/101 decoded;
  `--battle-save-check` PASS on the 7 battles in content/engine2/battles; debug + validation captures exit 0 (battle,
  resumed battle, Esc menu, Opções); battles re-exported with sounds (7 in content, 148 in build/engine2-test, only the
  sound fields added). **Not verified:** hearing it in the window, clicking Save / Load through the slot screens.
- **Sound = Engine2's audio.ts (2026-10-16, user: "important").** `engine/media/AudioMixer`: SDL3 device, MP3 by Media
  Foundation (effects whole and cached, music streamed on a worker, looping, each track keeping its place), Engine2's
  layered / exclusive players, delays, fades; Opções' Música / Efeitos / Silenciar act on it. Each screen's music is
  `Stage::music()` (GameApp's chain; `apps/sandbox/GameMusic.h`: title page intro at base 1, world map, Inn, a
  mission's track for briefing and battle; films, test menus and the battle's Esc menu silent), handed to the mixer
  every frame by main.cpp. Battle cues are Engine2's own choice: `export_battle.ts` records, per unit, what audio.ts
  plays (stand-in Audio element) for attack, Kael's strike, counter, every spell, heal, walk (+ fade), hit, death, and
  the level-up cue and the mission's `music`; MapStage plays them (`sounds`, `playCues`). Battle files without them
  are silent: re-export (content/engine2/battles and build/engine2-test were). Check: `Sandbox --audio-check`
  (decodes all 101 files).
- **Save / Load in battle = Engine2's BattleSnapshot (2026-10-16).** `e2::Battle::snapshot / applySnapshot`
  (world/BattleSave.cpp: units' live state, round, initiative order, enmity, Engine2's field names) + MapStage
  (`captureSnapshot`: in-flight action spent, explored fog base64, awake enemies, phase, turnBegan; `resume`). Esc menu
  Save (campaign only, not Modo teste) writes it into a slot with pendingMission; Load (and Continuar / the world map's
  Carregar jogo) resumes it straight into the fight (GameApp enterFromSave; `OverworldStage::Callbacks::resume`); a
  fight entered from its briefing starts afresh, as in Engine2. Check: `Sandbox --battle-save-check <battle.json>
  [out]` (round trip must match), `--resume <snapshot.json>` capture.

## Atmosfera volumétrica v1 (2026-10-16; user: the pasted spec "cinematic atmospheric rendering system" + "upgrade
no engine gráfico", Luminous identity, UE5 as the technical reference, old assets kept, everything versioned with OFF)

Read `docs/ATMOSPHERE.md` first (audit of the renderer, measured baseline, design, integration points, validation).
- **Built:** `engine/render/Volumetrics.h/.cpp` (froxel fog module: per-level volume sets kept once made, regional
  profiles blended over seconds, up to 16 local fog boxes, adaptive quality with hysteresis, history invalidated on camera
  cuts and ramped in over 30 frames so it never flickers at the start), `shaders/volumetrics_v1.hlsl` (Koschmieder
  density, height falloff, fbm wind noise, valleys, sun through both shadow maps with 1 / 4 taps, sky SH in-scatter,
  cluster local lights occluded by the relief, the fog of war's rules, temporal history, Hillaire integration),
  `content/atmosphere/v1/profiles.json` (padrão, campo aberto, inverno, floresta, caverna, templo, terra devastada;
  `maps` substrings pick the map's own). Integration (reversible, documented in ATMOSPHERE.md §4): MeshFrame
  `volumeSlices` / `volumeFlags`, `applyVolume` by slice count, `volumeTransmittance` on additive effects / VFX,
  `volumeOverBackground` on the sky / painting, Renderer owns a `Volumetrics` and records the "volumetrics" scope after
  the card shadows (the old mode-0 haze stays off while it runs). Panel "Escolhas": **Atmosfera** (OFF / cinema / alta /
  desempenho; default alta) and **Clima** (the profiles). CLI: `--atmos N`, `--background N`, `--mist 0`, `--debug-view
  15..19` (the volume's slices), `--capture-sequence FROM:EVERY:COUNT` (consecutive frames of one run; the run stays
  alive until they are taken).
- **No depth jitter (2026-10-16 evening):** the user showed the angle where "a fumaça treme sem parar" (low camera along
  the board). Measured with static-camera sequences: jitter + exponential history boils for ever; without jitter the fog
  adds no frame-to-frame change (ATMOSPHERE.md §6.4). The noise is filtered to the slice length; the light is read at
  the slice centre; profiles lighter (visibility 600-2600 units, ground-hugging). The live shaders in
  build/release/bin/shaders were replaced by hand with DXC (the exe there is the other session's build).
- **Measured (release, 60 Hz paced, aldeia015 + fog of war):** "volumetrics" alta 0.59 ms avg (worst 2.85), cinema 1.86
  (worst 7.7); frame median 12.8 ms vs 11.7 base. The fog bank (look 003) still costs 6.4-6.7 ms = half the frame.
- **Verified:** debug + validation captures exit 0 on aldeia015 (every level, OFF, Tática); the fog lights the sky /
  painting, surfaces, effects; cemiterio's lanterns glow in the air. See ATMOSPHERE.md §6 for the rest of the checks
  and what is still open (density tuning by eye is the user's; never-seen bank in the volume optional, off).
- **The user's reports (2026-10-16):** "um leve glitch na cor quando move a tela" → later "não tem glitch de cor, é só
  esse flickering mesmo, que agora apareceu alguns segundos mais tarde": the flicker was the history restarting (the
  opening, then the adaptive level switches); fixed by the ramp + adaptive off by default. Then "não mudou nada,
  podemos desencanar disso?" and "padrão on": the atmosphere stays ON at "alta" by default, the user does not want
  more time spent on it now. Default profile densities are light (visibility 300-1400 units): the visible change on
  aldeia015 (dusk) is subtle; cemiterio (night) shows the lantern glow in the air.
- **Shared checkout, 2026-10-16 afternoon:** another session edited the sprite path (SpriteFormat / SpriteSystem /
  sprites.hlsl / SpriteCooker, "Sprites 3D" relief) in this checkout while this work ran; a release built from the
  mixed working tree failed to load the sprite pack ("damaged roster") and its captures never finished. This work was
  therefore verified from a scratch worktree holding HEAD + only its own staged hunks (`git worktree add --detach`,
  `git diff --cached | git apply`), and committed from that index.

## Sprites 3D (relief maps on the 2D cards): tried on 2026-10-16 and REMOVED on the user's order

Per-pixel normal / depth / occlusion maps for the sprites (Depth Anything V2 on each frame, a second BC7 array per
clip, Dev Controls switches) were built, shown in play and rejected ("ficou horrível", "retire do jogo todos esses
relevos", "faça com que esse chat nunca tenha acontecido"): the figures tore and doubled, a unit left a trail when
moving, the game slowed, the pack doubled. Everything was taken out again (code, data, the ONNX model); the sprite path
is back to the 80-byte card and the version-1 pack. Do not propose relief / normal maps / parallax on the cards again
unless the user asks for it by name.

## Battle grid look 001 (2026-10-16; user: "modernize the movement grid ... targeting grid, enemy target zones and
everything", the C++ one was "awful")

- `looks/grid/README.md`. Panel "Grade": OFF / antiga (the first C++ marks) / 001 (default); `--grid-look N`.
  Data: `Renderer::GridCell` per hex (flags + route step + reach cost share, `setGrid`), filled by
  `MapStage::collectGrid` (same states as collectMarks); drawn by the terrain pass through `shaders/grid_look001.hlsli`
  (signed-distance: region outlines with glow on both sides, lattice, flowing route chevrons + reticle, turning rings,
  target locks with hazard stripes, cursor brackets; `TerrainPush` is 80 bytes now). The old marks still serve the
  editor's rings.
- **Bloom gotcha:** anything painted bright in display space on the ground blooms through the tonemap's sparse 3-ring
  gather as crisp "sparks" over the cards in front (the user saw them on Neera's face). The grid is therefore capped
  under the bloom threshold (terrain.hlsl applyGroundMarks: display luminance <= max(0.72, the ground's own)).
- Battle script steps for captures: `near:<unit>:<other>` (placed beside), `pick:<unit>` (the turn handed to it),
  `aim:<x>:<y>` (the cursor held on a hex). PowerShell mangles non-ASCII unit names in `--battle-script`: use ASCII
  ones (Piqueiro, Kael).
  Clicks as the player makes them (2026-10-17): `click:<x>:<y>` / `click:<unit>` (the hex's centre on screen through the
  window's own click, cards first), `confirm` (the confirm panel's Confirmar; logs who lost HP, fails if no foe did),
  `learn:<unit>:<kind>` (a spell the class lacks, for a test). Every line / cone spell, clicked: scratchpad `aimtest.ps1`
  pattern = `near:Piqueiro:<caster>; spell:<caster>:<kind>; click:Piqueiro; confirm` (+ `--promote Voss=elementalist:25`).
- **Verified:** debug + validation captures exit 0 (party reach + route + target, close-up, a foe's zone, antiga, OFF);
  release build. **Not verified:** the look in the window (the user judges), spell aim / chosen targets in the new
  look (code path only), cost (not measured).
- **Zones (`f10dd44`; the user: "how do I even know where to target?? there is no targeting zone"):** a spell opened
  shows its whole zone (`e2::spellZone`: range, min range, the shot's line, rays where valid) as the violet region even
  with nobody in it; valid aims get a violet lock (kGridSpellTarget); "Atacar" opens always and shows the weapon's zone
  from where the unit stands (kGridStrikeZone) plus the red locks. Script steps `spell:<unit>:<kind>`, `attack:<unit>`.
  Verified: debug + validation captures exit 0 (Tiro Longo, Míssil Mágico with a foe locked, Golpe Duplo, Atacar).
- **Shared checkout gotchas (2026-10-16 evening):** another session rebuilt `build/debug` while captures ran (exe
  missing mid-link, SPIR-V interface errors from half-rebuilt shaders → DEVICE_LOST): verify from a scratch worktree with
  its own build dir (`git worktree add --detach`, apply only your hunks, `cmake --preset ... -DFETCHCONTENT_SOURCE_DIR_*`
  pointing at `build/release/_deps/*-src`, ~10 min at -j4). With the user's game open, captures need `--budget 160`
  (two 576 MB sprite budgets do not fit the RX 6400). Commit through a temporary index (`GIT_INDEX_FILE`, read-tree
  HEAD, apply --cached, commit-tree, update-ref) so the other session's staged work is untouched, then `git reset
  <commit> -- <your files>` so its next commit does not carry stale copies of them.
- **Decor "crooked" (the user, 2026-10-16, two screenshots: chest / wheel / fences leaning):** cards stand vertical
  (sprites.hlsl VSMain `up = (0,1,0)`), so under the pitched camera their verticals converge (keystone): at the
  screen's sides a card's top leans toward the centre, ~11 deg at the edge of the 30 deg lens at a 25 deg tilt. **Built on
  the user's "faz" (2026-10-16 evening):** the card's up = world up projected onto the image plane (sprites.hlsl VSMain,
  `SpritePush` cameraForward + lean, `Renderer::setCardLean`): the card leans back by the camera's tilt, feet in place,
  upright anywhere on screen, ~10% taller at 25 deg; own-axis cards (fences) take the same up; flat cards (Tática) and
  the sun-shadow card pass (turned to the sun, vertical) unchanged; picking, health bars and popups follow
  (`MapStage::cardUp`). Panel "Cards": em pé / para a câmera (default); `--cards 0|1`. Verified: debug + validation
  captures exit 0 (both stances at the screen's edge: `captures/cards/`); release built in the worktree and placed for
  JOGAR. Not verified: the look in the window; tall cards leaning into 3D pieces behind them at steep tilts.
- **Health bars near the camera fade (the user's "faz", 2026-10-16 evening):** `drawHealthBars`: a unit whose card
  fills more than 30% of the screen's height fades its bar and HP number, gone at 45% (`kBarsFadeFrom / To`), back as
  the camera moves away; the share follows the camera continuously, so nothing pops. Panel "Vida perto": OFF (always
  shown) / somem (default); `--bars-fade 0|1`. Verified: debug + validation captures exit 0 (`captures/cards/030-032`);
  release built in the worktree and placed for JOGAR. Not verified: the thresholds in play (the user judges). The other
  two options offered (fade while the camera moves; only the active unit's bar) were not built.

## 3D props: the kits + the catalogue (2026-10-16 night; user: "pode fazer, já existia só que eu não importei direito.
repare que tem alguns objetos 3D para serem catalogados")

Read `content/models/README.md` (the catalogue + pipeline). Findings: Engine2's 11 models (5 trees, 1 outcrop, 5 tavern
props) + 4 cottages were already cooked (props.pak) and catalogued (data.ts → editor-catalog.json); **the trees and the
outcrop have no textures at all** (vertex-colour bark, flat leaf colours, one flat grey on the rocks: that is what
"não importei direito" looks like in play); the user's Meshy hearth (`Downloads/Meshy_AI_Stone_Hearth_Glow_*.blend`, one
mesh, 391k tris, no UVs / material) was in no catalogue.
- **Built:** `content/models/` = EmberEngine's own 3D library: 34 props generated by headless Blender 5.2 scripts
  (`blender/emberlib.py` + `kit_forest|winter|props|city|tavern|dungeon.py`: procedural geometry, triplanar UVs,
  Engine2's photo textures downscaled in `textures/`, vertex-colour bark / leaves like Engine2's trees) + the Meshy hearth
  exported (`blender/export_hearth.py`, metres → units, masonry + soot). Every asset = `<kit>/<name>-v01.glb` + `.blend`
  (versioned, never overwritten; a change is v02). `models.json` = the catalogue (ids `ember-<kit>-<name>`, PT names,
  authored height / width, footprint, editor section); `tools/scripts/models/catalog_models.py` merges it into
  `content/engine2/editor-catalog.json` (idempotent; re-run after export_editor_catalog.ts). New mesh pack
  `build/meshes/props-v2.pak` cooked from the staging root `build/meshes/src-v2` (Engine2's 15 GLBs from the cache + 35
  new); **JOGAR.bat now uses props-v2.pak** (falls back to props.pak). `props.pak` and Engine2's files untouched. No C++
  changed (no rebuild needed).
- **Verified:** Blender exports 35 / 35 (sizes + tris in README); Cooker 50 meshes, 0 failed; release `--meshes
  props-v2.pak --grid 8` 50 / 50 resident; the Map Editor on a showcase draft (`tools/scripts/models/make_showcase_draft.py`:
  one of each on aldeia015) places all 35 models (`scene showcase-models: 35 models`), lit, textured; **debug + validation
  exit 0** on that scene. Captures (local, git-ignored): `captures/models/001-004`.
- **Not verified / honest limits:** the look in the window (the user judges); the mature tree's canopy is blobby
  spheres and the pine is stacked cones (procedural, not AAA foliage: redo with real leaf cards / a tree tool if the
  user wants them); glass is opaque white; the hearth's 391k tris are uncooked-LOD heavy (cluster LOD handles it, cost not
  measured); nothing placed on campaign maps yet (the editor's 3D palette places `ember-*`; export_scene.ts knows only
  Engine2's ids, so campaign maps with ember props must be saved by the editor / built by SceneBuilder). Not built: the
  extra list (fences, door, doorway, grave marker, wheel, firewood, bucket, torch bracket, crystal, mine supports,
  mushrooms).
- **Extra kit (`ad4b169`, a side session):** the 12 extras above built (`kit_extra.py`, content/models/extra), catalogued, cooked.
- **Wave 2 (2026-10-16 night; the user pasted a ~200-item list, "faz os que não existem"):** 173 more props in 8 kits
  (nature 19, treasure 20, structures 19, interior 22, frost 16, crypt 26, mines 31, ember 20) on `kitlib.py` (shared
  parametric builders). Catalogue: `content/models/names.json` (PT names + overrides) →
  `tools/scripts/models/add_kits_to_catalog.py <kits>` (reads the GLB bounds) → `models.json` (220 models) →
  `catalog_models.py` → editor catalog (686 decorations). `make_showcase_draft.py <map> <out> [kit,kit]` writes per-kit
  showcases. Pack re-cooked (235 meshes, 0 failed, 434 MB: every asset embeds its own copy of the shared JPGs; a
  content-hash texture dedupe in MeshCooker is the obvious follow-up). Verified: 8 per-kit Map Editor captures + the
  full grid in release, debug + validation on two kits (see the commit). Not verified: the look in the window; cost
  with many props on one map (not measured).

## UI icons and images (2026-10-16; the user: the gear "nojento", the gold "horrendo", every icon pixelated)

- **Images** (`uiImage`, any PNG / JPG from Engine2): nothing to do. `refineUiImages` (render/UiArt.cpp, called by
  DebugUi between ImGui::Render and the draw) swaps every image drawn smaller than its file for a copy resampled to its
  size on screen (premultiplied tent filter = the browser's smooth downscale; exact sizes cached, then a halving chain).
  Non-square art inside a square box: `imageContain` (CSS object-contain), never a stretched AddImage.
- **Icons: never draw an icon by hand with lines again.** Put the lucide SVG in `content/ui/lucide/<name>.svg` (from
  lucide 0.510.0 = the version Engine2's lucide-react uses: `raw.githubusercontent.com/lucide-icons/lucide/0.510.0/
  icons/<name>.svg`) and call `uiIcon(dl, "<name>", centre, size, ink[, strokeWidth])`; an inline SVG (Engine2's CSS
  data-URIs) goes through `uiIconSvg`. lunasvg 3.5.0 rasterises it at the exact pixel size, snapped to the pixel grid.
- Spell icons for EmberEngine's own spells: the user's art, `content/ui/spells/<kind>.jpg` (portrait 784 x 1168, drawn
  centre-cropped by `imageCover` in the HUD socket; MapStage::spellIconOf looks there first). A new spell = a new file.
- Sound button = `soundButton` (CampaignUi.h: GamePrefs muted, the same switch as Opções' Silenciar): title (top right),
  briefing, world maps (beside Opções), the Inn, the battle's camera bar (+ "Opções" = the Esc menu), as in Engine2.

## Habilidades do Guerreiro T4 / T5 (2026-10-18; the user: "duas habilidades novas Tier 4 e duas Tier 5 para guerreiro, with
level scaling, and 3D FX", "faz as quatro como propostas"; icons in `Attachments/warriorskills`)

- **Read `docs/SKILLS_WARRIOR.md`** (numbers, scaling 11-30, conditions, the landing rule, the effects). Redemoinho de Aço (T4, 11:
  weapon blow + dice on the 6 neighbours, self area), Quebra-Guarda (T4, 13: blow + dice, "Guarda quebrada" -DEF% for N turns),
  Grito de Guerra (T5, 15: instant, allies in the radius +ATK% for N turns, foes afraid from 22), Salto Devastador (T5, 17: a leap
  over everything to a free hex beside the foe, x mul + dice, no counter; a shockwave on the landing's other neighbours from 25).
  Weapon skills (the weapon's own roll + bonus dice: `bonusDice` / `weaponBonusDice`), no MAG; a miss is a miss. Learned by
  `grantClassSpells` for `swordsman` and its promotions (`emberClassFamily`: paladin, heavyKnight).
- Code: `Spells.cpp` (`kEmberSpells`, `emberNumbers`, `castEmber`, `leapLanding`, the countdown in `startOfTurnEffects`),
  `Battle.h` `Conditions::sunder* / rally*` (applied by `fighterAt`, in the snapshot), `render/vfx/WarriorFx.*` (the 4 effects, 3D,
  Combat material + lit pieces), `SpellEffects::leapFlight / warCryFear`, `MapStage.h` (`leapOffset`: the card's arc during the
  Move step; the Grito's / Redemoinho's unit spots; the Salto's shockwave step lands at once with no pose).
- Icons: the user's art copied to `content/ui/spells/<kind>.jpg` (`spellIconOf` finds them). Tests: `--promote Kael=swordsman:25`,
  `--fx-test <kind>`, `--battle-script "near:Piqueiro:Kael; cast:Kael:guardBreak:Piqueiro"` (scratchpad `run_warrior.ps1`).
- **Committed through a temporary index + a clean worktree build** (`ed78ce7`, rebased on the other session's commits); the user then
  asked for the Arqueiro's (below) in the same turn.

## Habilidades do Arqueiro T1-T5 + Caçada (2026-10-18; the user: "as habilidades que estão faltando para arqueiro até o T5, duas de
cada tier no mínimo e uma habilidade de campo para usar no world map")

- **Read `docs/SKILLS_ARCHER.md`.** Flecha Envenenada (T1, 2: bow roll + dice, Engine2's poison by tier), Chuva de Flechas (T3, 7: a
  volley over a hex's area, no line of fire), Flecha Explosiva (T4, 10: x mul + dice, the target's neighbours take a share), Tiro
  Paralisante (T4, 12: "Paralisado" -MOV% for N turns, `applyHobble` / `hobbleBase`), Tempestade de Flechas (T5, 13: a bigger volley),
  Tiro Mortal (T5, 15: x mul + dice, a foe under a share of its HP dies); Caçada (field, T2 from 4: rations on the world map,
  `FieldSpells::castHunt`, a row in OverworldStage's field-spells menu). Bow skills on the bow's own roll (`dmgMul` now also under 1).
  Learned for `archer` and its promotions (ranger, assassin). Effects: `render/vfx/RangerFx.*` (a real lit arrow on an arc, trails and
  impacts per skill, volleys falling from the sky); MapStage `archerKind` gives every struck body CombatFx's arrow hit.
- Icons: the user's art (`Attachments/ArcherIcon`) in `content/ui/spells/<kind>.jpg` + `hunt.jpg` on the Caçada's row;
  `--promote Neera=archer:25`; the `cast` script step stands the target beside the caster, inside the
  bow's minimum range: test the shots with `goto` + `spell` + `click` + `confirm`, the volleys with `cast`.

## Habilidades do Lanceiro T1-T5 (2026-10-18; the user chose the set he drew the icons for: "Faz o conjunto dos ícones: Arremesso,
Gancho, Salto de Vara, Desarmar, Troca de Posto, Empalar e Arrastar, Vigília da Sentinela (mecânicas de tabuleiro, elemento só nos efeitos)")

- **Read `docs/SKILLS_LANCER.md`** (numbers, scaling 2-30, what each refuses, the effects). Every one moves the board, none repeats an
  existing mechanic (the user: "LEIA AS SKILLS JA EXISTENTES"): Arremesso (T1·2, a neighbour thrown 2-3 hexes the way you choose,
  collision dice on both when it lands on a body), Gancho (T2·4, a foe 2-5 hexes away pulled beside you, Desequilibrado = no counter),
  Salto de Vara (T3·8, vault over the neighbour onto its back, rear sure, no counter), Desarmar (T4·11, Desarmado = -ATK% and no
  counter), Troca de Posto (T4·13, places changed with an ally, then the blow from there), Empalar e Arrastar (T5·15, the neighbour
  dragged 1-4 hexes in a line, +mul per hex), Vigília da Sentinela (T5·17, a stance: the sentinel thrusts first at whoever attacks it
  or an ally in the radius; a dead attacker never strikes). Lance skills (the lance's own roll); learned by `aldric` and its
  promotions (`emberClassFamily`: sentinel, templar, lancer).
- Code: `Spells.cpp` (`lancerKind`, the plans `tossPlan / hookPlan / vaultLanding / swapAlly / dragPlan`, `castEmber`, the riders:
  `throwTo / arc / collideWith / dragTo / rearSure / disarm* / stagger*`, the Effect steps swapPost / sentinelVigil, `vigilInterrupts`),
  `Battle.h` `Conditions::disarm* / staggered / vigil*` + `disarmBase`, `BattleEffect::arc / delay` (a card's flight; numbers that wait
  for it), `render/vfx/LancerFx.*` (7 effects, 3D), `SpellEffects` (`vaultFlight / lancerFlight / swapFlight / disarmed`), `MapStage.h`
  (`animateFlights`: thrown / pulled / dragged / swapped cards fly on `unitLeap_`; `playInterrupts` before an enemy's blow or cast in
  `updateTurns`, `Exchange::interrupt`; `syncVigils`; `--fx-test`; script `cast` aims the throw / drag at a hex past the target,
  `vigil:<attacker>:<target>` tests the interrupt).
- Icons: the user's art copied to `content/ui/spells/<kind>.jpg`. Tests: `--promote Aldric=aldric:25`, scratchpad `run_lancer.ps1`.

## Magias do Curandeiro T4 / T5 + dois feitiços de campo (2026-10-20; the user: "create these new healer spells with 3D effects and
proper scaling", the formulas written like Salazar's own; icons in `Attachments/HealerIcons`)

- **Read `docs/SKILLS_HEALER.md`** (numbers, scaling 9-30, what each refuses, the effects). Círculo de Cura (T4, 9: hex at range 4, radius 2,
  every hurt ally heals its own MAG/2 × 1.2 + 1D8), Ressurreição (T5, 12: the fallen ally on a hex within 1 stands again with MAG/2 × 1.5 + 2D8 HP,
  conditions wiped, acts next round), Julgamento (T5, 14: holy, range 6, radius 2, MAG/2 × 1.8 + 2D8 on every foe, undead × 1.5). Field:
  Consagrar Acampamento (T2 from 5: the party to full HP, tier uses untouched), Prece do Caminho (T3 from 9: `save.roadPrayer` hexes with no
  undead encounter and battles half as likely; `content/engine2/undead-encounters.json` from `tools/scripts/engine2/undead_encounters.py`).
  "healer" family = `salazar` / `cleric` / `bishop` (`emberClassFamily`).
- Code: `Spells.cpp` (`healerKind`, `fallenAllyAt`, the Heal branch heals every id of `ids`, the `resurrection` branch), `Spells.h`
  (`BattleEffect::revived`), `FieldSpells.*`, `Overworld.cpp step` (the Prece), `render/vfx/HealerFx.*` (3 effects, 3D), `MapStage.h` (the areas'
  spots, the card back on a revival, script `kill:<unit>`), `OverworldStage.h` (the field rows). Tests: `--promote Salazar=salazar:25`,
  `--battle-script "kill:Kael; cast:Salazar:resurrection:Kael"`, `--title-screen test-rpg --map-ui fieldspells`.

## Magias do Conjurador na abertura dos tiers + Marcar Runa / Retornar

- **Read `docs/SKILLS_CONJURER.md`.** Troca de Lugar (1, T1: swap with an ally / summon within 6), Reflexo Arcano (3, T2: reflects
  hostile magic, with absorption unlocked by its existing talent), Imagem Espelhada (3, T2: 2 illusions on an ally for 3 rounds, each
  hostile hit 1 in 3 strikes a copy: "Ilusão"), Detonar Invocação (6, T3: his summon blows up, MAG/2 × 1.8 + 2D8 within 1 hex, both
  sides), Pesadelo (8, T4: range 6, radius 2, MAG/2 × 1.5 + 2D8, sleepers ×2, 25 % fear), Fortalecer Invocações (8, T4: every summon
  +25 % all stats for 3 rounds), Elo de Sacrifício (11, T5: for the fight, hits on him go to his nearest summon; costs one T5 use).
  Combat learning follows tier openings: T1 at 1, T2 at 3, T3 at 6, T4 at 8, T5 at 11. All existing scaling dates stay unchanged.
  T3: Familiar Radiante + Detonar Invocação. T5: Familiar Titã + Elo de Sacrifício. Keep T1's three combat spells and T2's four.
  Field: Marcar Runa (5, T2: a rune of the hex as a bag item, `save.runes`, a slot each, note editable in the Mochila's sheet) and
  Retornar (7, T3: the party to a rune's hex; the rune stays). "conjurer" family = `conjurer` / `sorcerer` / `necromancer`.
- Code: `Spells.cpp` (`conjurerKind`, `summonsOf`, `ownSummonAt`, `mirrorAbsorbs` + `bondRedirects` hooked into resolveStrike and
  the spell-hit loop, `BattleStep::sacrifice` dies before the hits, the fear rider), `Battle.h` (`Conditions::mirror* / bondPct / empower*`,
  `empowerBase`), `BattleTurns.cpp` (rounds), `FieldSpells.*` (runes), `Inn.cpp bagUsed`, `InventoryScreen.h` (rune entries + note),
  `OverworldStage.h` (rows, `drawRecallMenu`, `--map-ui recall:<hero>`), `render/vfx/ConjurerFx.*` (7 effects + the standing illusions,
  `MapStage::syncMirrors`), script step `summon:<unit>:<kind>`. Tests: `--promote Malrec=conjurer:25`,
  `--battle-script "summon:Malrec:summonFamiliar; cast:Malrec:detonateSummon:Familiar de Malrec"`.

## Magias do Mago Negro: Nuvem de Adagas (2026-10-21; the user's pasted spec, "Criar essa magia e seu efeito visual")

- **Read `docs/SKILLS_MAGE.md`.** Nuvem de Adagas (`cloudOfDaggers`, T4 arcane, level 8, instant on self): 3 daggers circling Voss (4 from 12, 5
  from 15; the user's own levels, `FIXED` in tracks.py) until used or the fight ends; a recast is the whole count again. Each reacts on its own
  and is spent: an enemy ending its walk beside him (`EnemyTurn::walked`), a melee blow / weapon skill at him (not by an archer), each a
  `daggerStrike` step (MAG/2 × 0.8 + 1D6, arcane, no track) played as an interrupt before the enemy acts (`daggerInterrupts`, with the
  Vigília's `playInterrupts`); an arrow / bolt at him is knocked down ("Interceptado", `daggerIntercepts` in `resolveStrikeImpl` and the
  spell loop, `arrowKind`). The sheet / Status line shows "Adagas ×N"; in the snapshot. `emberClassFamily`: voss / warlock / elementalist →
  `mage` (the promotions keep it).
- Effect: `render/vfx/DaggerFx.*` (real lit daggers orbiting the body, over the card on the eye's side; the dart, the burial, the clash);
  `MapStage::syncDaggers` keeps the cloud (count synced only when the rules' count changes: a thrown dagger is counted in flight). Script
  steps `dagger:<attacker>:<target>` (arrival + melee: two daggers), `shoot:<archer>:<target>` (the interception); scratchpad `run_daggers.ps1`.
  Icons for this and the next three spells: `Attachments/MoreSpells` → `content/ui/spells/<kind>.jpg`.
- **Dissipar Magia** (`dispelMagic`, T3 arcane, level 6; hex at 6, radius 1, 2 from 10): no damage; foes in the area lose Bênção / Grito de
  Guerra / Vigília, the foes' zones touching the area are erased whole (webs, storms, auras, burning ground, Mortalha, a Meteoro's mark;
  `WebZone` now carries `side`), allies lose sleep / fear / silence / marks / curses (poison and bleeding stay). Effect `render/vfx/MageFx`
  (a violet shell bursting through the area, rings, the magic torn off each body).
- **Globo de Invulnerabilidade** (`globeOfInvulnerability`, T5, level 11; `spellSelfArea`): `Battle::Globe` fixed where cast, radius 1 /
  2 rounds / tiers 1-3 blocked, all one step up from 14 (`emberNumbers` reads the level); rounds count at the caster's turn starts
  (`startOfTurnEffectsImpl`; a dead caster's at `startNewRound`). `globeFilter` in `resolveStep` drops the shielded targets of any spell /
  heal / cure / effect step cast from outside ("Bloqueado"; weapon skills and arrows pass); `Battle::shielded()` turns off zones inside
  (`zoneEffects`, `iceStormsStrike`, `tileHazard`, the web's roll and clamp, `weakenAt`, `inZoneOf`, `zoneDamageMul`); `planEnemyCast`
  drops a plan the bubble shields entirely (its use given back). The standing dome is `MageFx::globeStart` (MapStage `syncGlobes`), a
  blocked spell flashes on it (`globeRipple`). **Test gotcha:** at level 15 the bubble has radius 2: stand the outside caster 3 hexes off.
- **Identificar** (`identify`, field, T1 from level 1; `FieldSpells::identifiers / castIdentify`) + **itens desconhecidos** (`InnRules::
  unknownItems / weaponUnknown / gearUnknown / potionsUnknown / sellUnknown / identify / discardUnknown`): `content/unknown-items.json` (the
  power line, the potion kinds, the Inn's share: first values, the user settles them) read by `UnknownItemRules::shared()`; `useLockpick`
  marks the drops (`Battle::lootUnknown`, `lootUnknownPotions`, in the snapshot; off in the Cooker's checks) and the chest popup shows a "?";
  `finishCampaignFight` writes `save.unknownWeapons / unknownEquipment / unknownPotions`; the Mochila lists them as "Espada desconhecida" /
  "Equipamento desconhecido (Cabeça)" / "Poção desconhecida (Voss)" with a "?" and only Jogar Fora; the pickers and `equipWeapon / equipItem`
  skip them; Vargan's forge sells them ("Itens desconhecidos", `sellShare` of the price); the world map's field spells list "Identificar"
  per mage (button Itens → the list, a click reveals one for good: `--map-ui identify:<hero>`). `InnWeapon::weaponType` now comes from
  `content/engine2/weapons.json` (the unknown weapon's only word).

## Promoções do Mago Negro: Bruxo e Elementalista, magias T6–T10 (2026-10-17; the user: "pode criar todas elas os
efeitos e wire into the game")

- **Read `docs/SPELLS_PROMOTIONS.md`** (every number, the scaling 15–30, what is wired where, what is not done). 20 spells,
  2 per tier T6–T10, EmberEngine's own (Engine2 has the classes and no spells for them): Bruxo = enfraquecimento e dano no
  tempo (Marca da Ruína, Praga Rastejante, Dreno de Vida, Mortalha de Sombras, Hemorragia, Silêncio Profano, Peste Negra,
  Maldição da Decadência, Colheita de Almas, Vazio Devorador); Elementalista = dano puro e área (Coluna de Fogo, Lança de
  Gelo, Tempestade Elétrica, Meteoro, Zero Absoluto, Cadeia de Raios, Erupção, Vendaval Glacial, Fúria dos Céus, Brasa
  Primordial). Learned at 15, 15, 16, 17, 18, 19, 21, 22, 23, 25.
- Code: `engine/world/Spells.cpp` (`kEmberSpells`, `emberNumbers`, `castEmber`, `layZone`; conditions tick in
  `startOfTurnEffects`: **poison now ticks** with Engine2's poisonTiers, bleeding, the curse, zones, a Meteoro landing),
  `Battle.h` (`BattleUnit::Conditions`, `Battle::Zone`, `weakenAt` read by `fighterAt`), `BattleSave.cpp` (all of it in the
  snapshot), `engine/render/SpellEffectsEmber.cpp` (version 01 of each effect, flipbook cards; `zoneStart` / `zoneEnd`
  standing zones; `meteorFall`), `MapStage.h` (`syncZones`, `pendingFx_` for the Meteoro's numbers, sockets with initials
  on the element's colour + `spellTooltip`, `conditionsText` in the sheet, Escolhas: each black mage's path live in test
  mode, `--promote Name=class[:level]`), `OverworldStage.h` (Party → Grupo → "Caminho de classe": the campaign's choice at
  level 15 → `save.promotions` → `MapStageOptions::heroClass` in main.cpp).
- **Verified (debug + validation, exit 0):** all 20 cast through `--battle-script` on the aldeia015 test battle with Voss
  at 25 (`captures/promo/*.log`: marks, poisons, bleeding, silence, curse, zones, pushes, hits); `--battle-save-check`
  PASS; `Cooker check-battle` aldeia015 still matches Engine2 (reach, 462 forecasts + rolls); four effects seen in
  captures with the atmosphere on (`captures/promo/fx7-*.png`). **Not verified:** the look of each effect in the window
  (the user judges), the campaign's promotion screen in play, enemy warlocks (no AI casts), balance (the numbers are
  the design's).
- **REJECTED by the user (2026-10-17): the 20 effects as flipbook cards.** "eu falei para fazer efeitos em 3D que bosta é
  essa". The rules and the wiring stay; the card versions (01) stay in the library but are NOT the deliverable. **Every
  spell effect is built in 3D on the effect runtime** (engine/render/vfx/*: meshes, instanced particles, shaders,
  lights, like Fireball / CausticVenom / HolyLight), one spell at a time as a new numbered version, on the user's
  "faz" ("depois você vai fazer certinho um por um"). Never again present a card effect as a spell's look.
- **Shader bug found and fixed on the way (affects every spell effect, not only ours):** with the Atmosfera on (the
  default), `volumeTransmittance` read 0 and every flipbook / vfx effect was invisible since 2026-10-16 afternoon. See
  the DXC gotcha above. If the user says "the spells show nothing", this was why.
- Test lines: `tools: run_promo.ps1` pattern = `Sandbox ... --battle-rules build/engine2-test/battles/aldeia015.battle.json
  --promote Voss=warlock:25 --battle-script "cast:Voss:markOfRuin:Kael; ..."`; `--fx-test <kind>` for the visuals alone.

## Session 2026-10-17 (night): fixes + the second Engine2 import wave (user: "continue importando tudo do engine 2"; both languages)

- **Battle clicks (`88b85a0`):** a click on the ground behind a friend's card walks the active unit (cards are taller than bodies: `pick(..., cards=false)`);
  two left clicks on the unit being played end its turn. **Resumed fight stuck mid-turn (`0bccd51`):** `resume()` set `turnUnit_`
  to the active hero, so nobody was picked and Esperar / Fim do turno / spells were dead; `pickActiveOnResume_` picks it once.
- **Map Editor (`e8dad59`):** Desfazer / Refazer on the header (always visible), history 10 -> 20 steps (`kHistory`).
- **Combat log (`d05beb6`, `3c3caf8`, `0bccd51`):** `Battle::pushLog` (200 lines, in the snapshot as `log`); lines are made where effects resolve
  (`resolveStrike` / `resolveStep` / `startOfTurnEffects` wrappers in Spells.cpp, `BattleEffect::cause` for ticks, `gNotes` for skill gains and
  level-ups); `setCombatLogEnglish(uiEnglish())` each frame, so lines follow the language when written. HUD bar: Log / Status / Abrir log
  (full screen); picking a unit or a new turn closes it (`select()`); the switch is the name row only. Not logged: Engine2's `Loot:` lines (loot is
  not kept here yet) and its tip lines. `--map-ui log|fulllog` for captures.
- **Companion conversations (`2bc0e3b`):** `world/Companions` + `export_companions.ts` (Engine2's own `buildCompanionDialogue`, 130 trees) ->
  `content/engine2/companions.json`; **the Portuguese is our own translation** (`companions-pt.json`, 223 strings, Neera is the only woman) because
  Engine2 has none. Party > Conversas tab + "Conversas" plate. Not checked against Engine2's `resolveCompanionReply` numbers yet (no Cooker check).
- **Field spells (`a23529c`):** `world/FieldSpells`: Criar Comida e Água (healers) and Warp (mages 7+, wheel of explored locations, gate picture) behind the
  field-spells icon of the world-map header. Engine2's Warp sound (summonFamiliar) is not played. No check against Engine2 yet.
- **Gotchas found:** ImGui 1.93 `AddRect(a, b, col, rounding, thickness, flags)` (flags last; the old order is a deleted overload, error C2280);
  `ImGui::SetNextItemAllowOverlap()` before a big InvisibleButton under other items; a const `nlohmann` `operator[]` on a missing key / index crashes
  with 0xC0000409 (guard sizes); a battle's `Sandbox.exe` is locked while the user plays: rename it aside and rebuild; another session shares the checkout
  (3D spell effects, Malrec's spells, world-map zoom): stage only your own hunks (`git diff -U0 | filter | git apply --cached --unidiff-zero`).
- **Open, asked of the user:** the "flame effect" on braziers / lights ("precisa arrumar esse efeito em todos os pontos de luz"): which effect;
  the world map's blocked paths (re-exported 2026-10-17: `content/engine2/overworld.json` is identical to Engine2's current output, so a path blocked here is blocked in Engine2 too, unless it is a UI / click problem: ask for the hex).

## The walkable Inn, both floors (2026-10-17; user: the Inn opens in every mode as its map with free movement, the Adega only by talking to Brue, import the second floor, hook it in the editor)

- Every mode opens the Inn's own map (estalagem006, explore = free roam): the RPG world map's pin (OverworldStage::enterInn, after the
  arrival film), Modo teste's classic map (briefing "Entrar") and RPG map. Data: `content/engine2/battles/estalagem006` /
  `estalagem-andar-2003.battle.json` (export_battle.ts now writes exits' `returnConnector` / `connectorDirection`), scenes in
  build/engine2-scenes and build/engine2-test.
- `apps/sandbox/NpcTalk.h` (on every mission's MapStage): a click on an NPC with a `dialog` (MapSpawn::dialog, read from the map) walks the
  leader beside it, then the conversation (DialogScreen, portraits from `content/engine2/portraits.json` = export_portraits.ts); a reply's
  action after it closes: tavern (InnScreen main = the Adega), smith (forge, Vargan's film the first time), healer, the hostage quest,
  recruitAldric. InnScreen::Entry; leaving a menu returns to the room. The InnScreen header lost "Curandeiro" (Engine2 keeps Ferreiro).
- MapStage: `absentNeutrals` (companions not recruited, campaign only), `leader` (save.partyLeader walks in Kael's place: the seated
  hero, else its catalog art), `arrivedFrom` (GameApp arriveAtConnector); free roam's bar = hint + "Sair" (records the visit, GameApp
  onQuit); a floor connector's prompt "Sim" -> `wantsFloor()` -> main.cpp opens the other floor. Both floors play the Inn's theme.
- Map Editor: "CityHubs…" also lists maps reached by a hub's floor connectors (the Inn's Andar II).
- Script steps `talk:<npc>`, `stairs`; captures `--title-screen mission:estalagem [--map-ui npc:tavern|smith|healer]`, `--arrived-from`,
  `--leader`. DialogScreen's wrap was in screen space (lines clipped right in every conversation): fixed.
- **Andar II enlarged (2026-10-17, user's "faz": each room and the corridor at twice the area):** our own save
  `content/maps/ember/estalagem-andar-2004.json` (serial 4 wins over Engine2's 003; Engine2's file untouched), 33x21: rooms 7 rows
  tall, corridor 3, room widths 7/6/6/9 above and 10/7/6/5 below (x1.96-2.33 area, corridor x2.11), walls re-laid, the stairs room's
  `door-3d-closed` now an open `door-3d-frame` (the user: no lock there). Made by a script (scratchpad make_upstairs.py: items
  touching a wall keep that distance, the rest scaled; NPCs with their dialogs, all 58 cards, stairs + connector at (29,18)).
  The campaign builds its game files from it itself (build/engine2-campaign); test files in build/engine2-test. Every walkable
  cell (493) reachable from Kael's spawn; the same 17 lights now light a larger floor (darker rooms: the user judges).

## Status sheet in battle (2026-10-17; user: "importa direito a ficha": images, details, point system, Mochila / Equipar, the same scroller)

- `apps/sandbox/BattleStatus.h` = Engine2's StatusPanel (portrait + hunger, name / class / level / XP, condition badge + detail, HP, "Pontos de atributo"
  (3 per level, +1 to VIT / ATK / MAG / DEF / DEX per point, `BattleUnit::point*`, saved in `statPointAllocations`, in the fight's snapshot), 8 attribute
  cells with gear tooltips, spells / skills from `content/engine2/status-rows.json` (`export_status_rows.ts`: Engine2's own row text per class × level),
  potions + lockpicks), with Engine2's ember scrollbar drawn by hand (14 px track, bronze pill thumb). The ⌊ ⌋ of the formulas come from Segoe UI
  Symbol merged into the Figtree font (UiArt.cpp loadUiFonts).
- **Mochila / Equipar** inside the sheet open `InventoryScreen` over the fight (MapStage `openInventory` / `syncInventory`; `MapStageOptions::world`;
  Modo teste uses a brand-new save's gear and lists every hero). The inventory's changes are written back live: worn pieces' stats
  (`InnRules::gearBonus`), potions, lockpicks, HP, disease, poison. **Not live:** a changed main / off-hand weapon (dice and reach come with the next fight).
- Captures: `--title --title-screen mission-test:aldeia --save-bank <copy of the bank> ... --map-ui status:<name> | inventory:<name>[:doll] | synced:<name>`.
- Verified: release + debug with validation (exit 0, 0 validation lines) on those captures; `--battle-save-check` PASS on the 7 battles; `Cooker check-battle`
  matches Engine2. Not verified: clicking in the window (Mochila / Equipar / +/- / scrollbar drag), points persisting through a real campaign save.

## Combat rules port (2026-10-17; user: "porta todas as regras do combate", hit % + confirm, pathfinding)

- **DEX = hit, dodge and escape only (ours, the user, 2026-10-18; NOT Engine2's):** every weapon blow (main, off-hand, counter)
  goes through `e2::hitChance` (Combat.h): clamp(0..100, accuracy + 10 from behind + (attacker DEX − defender DEX) / `kDexDivisor`
  (3, Battle.h) − defender's Esquiva / 2); escape = min(100, 60 + DEX / kDexDivisor), unchanged. **Esquiva** (skill
  `dodging`, heroes only, 0..100, `Fighter::dodge`): trains when an enemy strikes at the hero (`trainDodge`, trainWeapon's rule,
  off in Cooker checks), read from / written to `save.heroSkills[hero].dodging` (MapStage campaign start, finishCampaignFight),
  on the road like any skill (Party → Skills), shown in the battle sheet. Bênção counts once (weaponAccuracy, not Loadout).
- **Enemy ranks F .. S++ (the user, 2026-10-18): read `docs/ENEMY_RANKS.md`.** Data `content/enemy-ranks.json` (per-level value
  per rank, class → rank; humans F by default); `Progression.cpp applyRank` gives every non-party unit skill = value × level
  (cap 100) as its weapon mastery (75 + skill, replacing 90 + DEX), Esquiva and every resistance; shown on the enemy's sheet. Spells.cpp's own weapon-ability roll (resolveStepImpl) was
  left as it was on the user's order. `Cooker check-battle`'s hit % no longer matches Engine2 by design.
- **Confirm + hit %:** `MapStage::drawConfirm` (strike / spell shows the hit chance, Confirmar / Cancelar, as Engine2). Pathfinding: the
  algorithm already matched Engine2; the bug was rim hexes that could not be clicked (`pickAt` edge padding, fixed). A hand-bent route would
  be NEW behaviour: ask the user first.
- **Ported and checked against Engine2 (`Cooker check-battle` on the 3 proof maps, all pass: reach, turn starts under 8 conditions, ~65k aims /
  ~19k casts incl. affinity scores, enemy decisions, forecasts + seeded rolls):** every skill and spell in `engine/world/SpellsE2.inc`
  (summons, zones, cleave, cones, bull rush ...), the status machine (stun / sleep / bleed / crippled / fear / bless), web / ice / aura / warp
  zones, `beginUnitTurn`, enemy-cast AI branches, dynamic affinity (friendly fire -1, support +0.2, neighbours +0.1; auras build none).
  Refs: `TEST_LEVEL=15 SPELL_REFS=1 node .../vite-node.mjs ... export_battle.ts -- build/engine2-test/maps/<map>.json build/engine2-ref/battles/<map>.refs.json` (~2 min a map).
- **Also checked against Engine2 on the 3 proof maps (2026-10-17 night, all pass):** the staff's regeneration (turn start under 10 conditions), the off-hand
  action "Mão Secundária" (a dagger's own blow, a shield's bash: 75% damage, 70% to stun; `offHandStep`, `canOffHandAct`, forecast without mastery for a
  shield), the Warp pursuit (human-sized hostiles walk to the nearer gate mouth), the plants' AI (tendril swipe, Veneno Cáustico from tier 3, Sopro
  Venenoso, Veneno Menor; proven on `aldeia015` with its enemies swapped for plants: `PLANT_TEST=1`, `aldeia015.plants.refs.json`), and the locks:
  `useLockpick` (chests in 3 tiers, door decorations, door tiles; Ember, potion for whoever has room, gear, rations; ~90 openings compared: bags, loot,
  draws, affinity). Data: battle JSONs now carry `locks` (+ the cells each opening changes) and `ownedWeapons`: **re-export a battle file to get chests**
  (every campaign battle in `content/engine2/battles`; on 2026-10-17, on the user's order "todos os mapas com baús precisam funcionar", every
  `build/engine2-test` battle whose map has a chest, door tile or door decoration: 116 maps, battle file only, TEST_LEVEL=15 NO_REFS=1 from its own
  build/engine2-test/maps file, ~3 s each; maps without locks were left alone). Loot tables are read from `content/engine2/overworld.json`. In the game: "Arrombar" beside a lock, the loot popup with icons,
  the opened chest's card and an opened door's model leave the board, Ember / weapons / gear are folded into the save on a victory
  (`finishCampaignFight`), saved with the fight's snapshot. Capture step `lockpick:<unit>:<lock>`.
- **Ported, NOT checked against Engine2:** win rules (rout / boss / escape waypoints, `Battle::winAvailable`, `confirmFinish`, the finish prompt and
  footer button), hunger drain per action, the save's `affinityScores`, the loot fold into the save, the "Mão Secundária" button / door and chest visuals.
  Script steps `wipe`, `goto:<unit>:<x>:<y>`, `lockpick:<unit>:<lock>`. Not tried in the window by a person.
- **Not ported / limits:** a door TILE keeps its closed ground look after it opens (the ground is baked; the rules open it); an opened door's open-frame model is
  not placed; kill drops are still not kept (only chests fill `lootWeapons` / `lootEquipment`); the weapon's "tip" text on the popup; enemy warp
  crossing beyond the walk; per-pose features.

## Lighting plan, phase 4: sky reflections, clouds, water look 001, cinema moonlight (2026-10-19; the user's pasted plan phases 0-9,
"concordo com seu plano, faz uma nuvem básica para ver com off", "aceito novos tiles de água", "não faça uma nuvem muito baixo
esforço ... céu bonito, estrelado ... luz do luar também")

- **Plan order agreed (the user, 2026-10-19):** phase 0 fog bank cost (6.4 of 11.7 ms) first, then 1 tonemap / exposure, 2 AO,
  3 local-light shadows, 4 (this), 5 probe GI, 7 SSR; 6 TAA (blurs the cards), 8 volumetric clouds (the camera barely sees the sky),
  9 ray tracing (4 GB) cut. The pasted phases 0-3 are in the conversation of 2026-10-19 (CLAUDE.md keeps only this line).
  **MeshFrame pads:** phases 2-3 need them; this phase took only two (`skyReflect`, `cloudFlags`); the clouds' numbers live in the
  AtmosphereFrame (304 bytes now, static_asserted).
- **Sky reflection map** (`shaders/skyreflect.hlsl`, Renderer `skyReflect_`): lat-long 256 x 128 x 6 levels, rebuilt every frame
  after CSAmbient from the sky-view table (clouds in, lit ground below the horizon, no sun disc), levels 1-5 GGX-prefiltered
  (32 samples, filtered importance sampling, roughness = level / 5); `lighting.hlsli indirectSpecular()` reads it when
  `f.skyReflect != kNoTexture`, on the SH ambient's scale (CSAmbient stores its scale in the ambient record's first w, the raw
  up irradiance in w 1..3). Panel "Reflexo do céu": OFF (the SH sky, as before) / céu real; `--sky-reflect N`. GPU scope "sky reflect".
- **Nuvens** (`shaders/clouds.hlsli` + `cloudSheet / cloudsOverSky` in atmosphere.hlsli, Renderer::Clouds): two sheets (cumulus at
  meanGround + 900, cirrus 1.6 x higher, finer, thinner), cover = drifting fbm thresholded by coverage 0.4, eroded edges, lit by the
  key light on top with self-shadow taps towards it, two-lobe phase (silver rims), powder, dark undersides, sky + ground fill,
  distance haze; the same eroded cover field gives the sky, the reflection map and the board's shadows (`cloudShadow()` in the
  sun term of shadeInSightSplit: every surface, cards too; by day only, off when the sun is under 12 deg). Composited last in
  skyBackground, over the stars and the moon (a thin cloud veils the moon). Panel "Nuvens": OFF / básica; `--clouds N`;
  `--debug-view 20` = the shadow factor on surfaces + the cover in the sky. **The cloud numbers are read by byte offset
  (`loadCloudParams`, offset 276) because passed inside the AtmosphereFrame by value DXC read them as 0 (the moonTexture gotcha).**
  Scale 300 units: with 500 the whole 100-unit board darkened at once (measured with the debug view from two sun azimuths).
- **Water look 001 "ember"** (`shaders/water_look001.hlsli`, `looks/water/README.md`; water.hlsl `Push.look`, water3d look 2):
  six analytic waves + noise ripple, Engine2's river blue deepening with the depth (Beer's law, k 0.6: a unit of water stays
  blue), GGX sun at roughness 0.10, the sky's reflection by Fresnel, Fresnel see-through, shore foam where the sheet knows its
  depth. Panel "Água": original / ember (default original); "Água 3D": original (ThreeWater, default) / rio / rio ember, no OFF;
  `--water N`. **The user, 2026-10-19: "NUNCA TROCAR O ASSET PADRÃO", "EU QUERO TODAS AS ÁGUAS": the original is always the
  default, a new look is only ever an extra option.**
- **Luar: cinema** (panel "Luar", light mode real at night; `--moonlight N`): the physical moon (0.5 lux) never showed on the
  ground (cemiterio capture: black beside the lanterns), so cinema = 16 lux (`kCinemaMoonLux`), the night stops capped at 1
  (`moonNight_`), the disc's glow following the moon's light, and the stars boosted up to 12 x against the brighter sky.
- **Why the user "lost" the moon and the sky:** maps with a painting defaulted Fundo = "imagem do mapa" (the sky never showed), and
  the camera's lowest tilt (5 deg) puts the sky only in the top third of the screen; nothing else. Captures that show the sky need
  `--tilt -15` (and `--fog 0` on fog maps, or the mist bank covers the horizon). **Never run a measurement window on the user's
  screen** (2026-10-19: they saw the `--sun 200 55` run on the dusk map, overexposed by the dusk meter: "lavado, péssimo"; the
  `--sun` override is now applied before the meter, in realLighting).
- **Fundo "imagem até o horizonte"** (an experiment in the panel, NOT the default; the user: "a imagem de fundo pode terminar no
  horizonte onde começa o céu. experimente na aldeia", then, seeing it: "you will need to try some sort of smarter edition for this
  to work"): resolve.hlsl backgroundAt mode 1 (`ResolvePush.backgroundMode`, `setBackground(bg, true)`): the painting's horizon row
  (kBackdropHorizon 0.5 of its height) pinned to the world's horizon, its rows below following the view's elevation down to 30 deg,
  a 1-deg band of sky over the horizon. **The smarter version (not built, needs a "faz"):** a per-painting horizon row and a matte
  of the painting's own sky (found once in the cooker, stored beside the backdrop), the painting kept at its own scale (no vertical
  stretch) and extended at the sides, the seam hazed with the sky's own horizon colour.
- **Sol** (panel, outdoors by day; `--sun-disc N`): real (0.26 deg, a dot nobody finds) / cinema (default: the moon's small disc, 2.7
  deg, with a corona; `AtmosphereFrame.sunTanRadius`, `Lighting::sunRadius`). The moon's default size is now pequena (2.7 deg: média
  and grande read as a "super moon" with the cinema glow); the disc glow = max(2.3, moon lux x 0.35).
- **Cloud shadow map:** the per-surface cover cost +0.7 ms in the terrain pass (windowed 1080p measurement, below), so the sun
  through the low sheet is computed once a frame into a 256 x 256 R32F map over the sun shadow's bounds (`skyreflect.hlsl
  CSCloudShadow`, scope "cloud shadow"; `AtmosphereFrame.cloudShadowRect / cloudShadowMap`) and every surface reads one texel;
  off the map (or without it) the cover is computed in place.
- **Verified:** debug + validation captures exit 0, 0 validation lines (aldeia015 dusk / day, cemiterio night, rimewater shrine;
  `captures/water/0xx`), clouds on/off, sky reflect on/off, water looks, both moons, debug view 20. **Not verified:** the look in
  the window (the user judges: sky needs Fundo = céu and the camera at its lowest tilt), cost numbers (the measurement run in the
  commit message), the reflection on the 3D meshes' metals (only water looked at).

## Céu look 001: sol AAA, nuvens volumétricas, clarão, raios de luz (2026-10-19; the user: "VOCÊ VAI FAZER UM SOL DIGNO DE UM
TRIPLE AAA AGORA", "um céu inteiro com 24 horas de transição com sol, lua, tudo que existe na atmosfera", "cadê o céu do FFXV?")

- **Read `looks/sky/README.md`** (what each piece is, the capture line). Panel: **Sol** real / **cinema** (default: the disc the user
  had) / 001, **Nuvens** OFF / básica / **volumétricas** (default), **Clarão** OFF (default) / 001, **Raios de luz** OFF (default) /
  001 (both only with Fundo = céu: against a painting they took its fire for the sun); `--sun-disc N --clouds 2 --glare N --shafts N`.
  **The user never asked for the sun look / glare / rays** ("que novo brilho? quem pediu?"): they stay as options, never defaults.
  The 24 h cycle ("Hora") drives all of it: the moon lights the clouds and gets its own aureole / halo at night.
- **What the user judged (2026-10-19 evening): "as nuvens ficaram boas"** after: the cone light march with a per-pixel offset, the
  cheap density eroded like the drawn one (grey clouds otherwise), 0.004 rad per march step (0.01 showed shells as stripes 20 px
  apart; the per-pixel dither alone did not hide them), the weather clearing with the world distance (a per-row thinning sheared
  the far clouds into "curtains"). Dusk exposure: 2 stops under the incident reading (`nightDarkening`); the cycle's meter has a
  2-deg horizon floor and amanhecer / pôr do sol sit at 6.35 / 17.65 h (the disc 6 deg up). The moon's dark side no longer covers
  the sky (it read as a black disc at dusk). Cost looking straight at the clouds: "clouds" ~1.2 ms before the fine steps; re-measure.
- Code: `atmosphere.hlsli sunLook001` (disc + limb darkening + corona + Mie aureole; read `sunLook` at AtmosphereFrame byte 376),
  `clouds.hlsli` (the volumetric density: `CloudVolume`, `cloudVolDensity`, `cloudVolShadow`; fields at bytes 352-380, read by
  offset), `cloudsvol.hlsli` (`cloudVolMarch`), `cloudsvol.hlsl` (CSNoiseBase 128³ / CSNoiseDetail 32³ once, CSMarch at half
  resolution each frame), `sunglare.hlsl` (CSGlare at half resolution from the finished scene colour, added in tonemap.hlsl),
  Renderer (`Lighting::sunLook`, `Clouds::volumetric / base / top`, `SunGlare`, scopes "clouds" and "sun glare"), MapStage
  (`sunSize_`, `clouds_`, `glare_`, `shafts_`). AtmosphereFrame is 384 bytes now (static_asserted).
- **Captures need `--ground build/ground/ground-v5.pak`** (v4 lacks aldeia's fire panorama: the capture never becomes ready and runs
  for ever) and no `--budget 160` unless the user's game is open.
- Not done: temporal reprojection for the clouds (40 fixed steps instead: measure before adding), the light shafts through the
  volumetric fog (volumetrics_v1 could read `cloudShadow`), the sun's reflection highlight on the water from the new disc.

## Céu noturno real (2026-10-19, late; the user: "a lua não aparenta emanar a luz ... as sombras na direção oposta da lua", "estrelas
100% randômicas ... nenhuma tentativa de parecer um céu de verdade com constelações"; then "sky is fine just finish it")

- **Stars look 4 "real" (default; `--star-look 4`):** `content/sky/stars.json` = the ~220 brightest real stars (RA, Dec, magnitude,
  B-V), `engine/render/StarSky.h` turns them for latitude 23 S at the hour's sidereal time (the cycle's hour; 22 h on a night map),
  `Renderer::setStars` uploads them per frame (host buffer, `AtmosphereFrame.starBuffer` at byte 400, 480 bytes now),
  `atmosphere.hlsli realStars` draws each as a point at its magnitude with a glow on the bright ones, look 003 at a tenth under
  them, the Milky Way as a band about the galactic plane (star clouds, the Great Rift, the bulge), the Magellanic clouds. World
  frame: north = +z, east = +x (TimeOfDay.h lightDirection).
- **The moon's light comes from its disc** (MapStage mapLighting: `placed.light = placed.disc` unless the disc is off) and the moon
  always has its Mie aureole and rim glow in the sky (`moonAureole`, physics, every look).
- **Night exposure:** "Luar: cinema" caps the darkening at 2 stops (was 1: cemiterio measured meter 4.4 lux, exposure 0.24, a pale
  blue day-like night). `camera:` log line once per capture (meter, EV, darkening, exposure, key lux, elevation).
- Not done: constellation lines (never asked), twinkle, the planets; the band's strength is a first guess.

## Cemitério dos Esquecidos 006, the modernized map (2026-10-20; the user: "versão modernizada do mapa do cemitério ... novos tiles,
recursos, elevações variadas, monstros, e deixe o birolho bem mais escondido", "we need a waypoint out", "deixar o seu mapa ativo na campanha")

- **First version rejected** ("worse than the original, a total mess, crazy tiles, nonsense walls that don't close ... there was lots of
  elevation and walls crypts and cooler things"): loose low walls on one or two sides of each plot, a crypt hill of half-level terraces,
  ~250 grey graves. **Then (the user, same night):** "no jogo ficou bem realista" (the tiles), zombie dogs out (they are summons), an
  Aparição and a Boi Zumbi loose in two corners, 2D tombs out (3D only, fewer), every other decoration as it was, "termina o cenário com mais
  elevações". The untextured "Mureta de pedra 3D" (`wall-3d-low`, no wall texture) was deleted from the catalog on the user's order
  (`tools/scripts/engine2/removed_decorations.py`; the exporters skip it). `wall-3d-stone` has no texture either: never use it.
- `content/maps/ember/cemiterio-esquecidos006.json`, made by `tools/scripts/maps/make_cemetery_v2.py` (reads its own mission settings from
  the same file; serial 6, the campaign plays it). Relief in whole levels: rims north / west / east to 7, the barrow over the crypt with a flat
  crown at 5, the chapel plateau at 3 stepping down, the mausoleum terrace at 2, open graves and the pond at -1, the south strip level so the
  gothic wall runs at one height. The crypt = a walled cut (`wall-3d-crypt` both sides + end wall, `dungeon-stone-door`, connector inside)
  into the barrow's south face, guardians at its mouth. The gothic parapet closes the whole south side, the gate (28..31) its only opening.
  Graves: 3D only, family plots in straight rows, one stone kind per plot (~50). **The Birolho's hollow** (north-west): a pit at -1 inside a
  ridge at 5 whose inner ring is closed by solid rocks / dead trees (movement ignores height), one cleft from the north-west woods. The script
  fails if the pit opens elsewhere, if any exit / spawn is unreachable, if the south wall or the crypt cut has a gap.
- **Tiles: the user's own photos** (`Attachments/texturas/cemi01-06.jpg`, after rejecting my code-made snow), cut by make_tile.py into
  `content/ground/cemiterio-*-v01.png`. `kEmberGroundTiles` has a `terrain` field; a draft picks a set with its `emberTiles` field
  (`MapDraft::emberTiles`; this map: "cemiterio-v01"). Ground pack `build/ground/ground-v14.pak`.
- **EmberEngine 3D props in a campaign map:** the map exporters know only the old ids, so MapStage builds the scene with SceneBuilder when the
  draft has any `ember-*` decoration (`hasEmberProps`), and `blockEmberProps` makes the cells of `ember-*` props with blocksPath solid in the
  battle (impassable, stops shots). Maps without ember props: unchanged.
- Captures: scratchpad `cap_cem.ps1` pattern (test battle from `build/engine2-test`, `--fog 0 --zoom 0.7` overview, `--zoom 2.2 --offset -0.07
  0.06` the crypt).
- **008 (2026-10-21, the live version; the user: "fica o 008"; old versions are the user's to delete by hand, never ask):** 007 (the
  photographed v02 / v03 tombs) with the crypt cut three hexes wide ("that narrow passage right in the entrance felt clunky and too
  narrow") and the two Guardiões da Cripta out ("não tem cultista assim dando sopa na entrada de mapas"); the Saqueador stays inside the
  cut. The generator now reads 007's settings and writes 008 (`TOMB_VERSIONS`, `SERIAL`, `CUT_HALF`); 006 and 007 untouched.

## A Ponte de Pedra 023 (2026-10-21; the user: "make the bridge 6 hexes wider, use the current background image as a Ground Floor Image,
a strong volumetric fog with strong gusts of wind flowing outside of the bridge walls", "make sure the tiles finish right at the walls")

- `content/maps/ember/thebridge023.json` by `tools/scripts/maps/make_bridge_v2.py <thebridge022.json>`: 40x21 (was 15 rows; every shift even,
  things keep their place against their own wall), both parapets at columns 1, 6, ..., 31, 35 (a parapet card is drawn 2 columns east of its
  anchor hex). 022 untouched. Test files made by hand in build/engine2-test (make_test_map / export_scene / measure_art / export_battle on 023).
- Draft extras (MapDraft, Board.cpp): `emberTiles` "ponte-v01" (the user's BridgeTiles photo, `content/ground/ponte-calcamento-v01.png`, tile
  3.2, `overBaked`: also over the "ruins" baked art), `emberFloor` + `emberFloorDepth` 40 + `emberFloorTile` 240 (resolve.hlsl backgroundAt
  mode 6: the map's painting flat under the bridge, mode 5's painting on the far band; panel "Chão do vão", `--floor N`), `emberWindFog` 10
  (fog look 010, `FogLook::fogOfWar` false: drawn without a fog mask; panel "Névoa do vento", `--wind-fog N`), `emberDeckClip` (MapStage
  deckClip: the ground drawn only between the wall cards' planes and within their ends; TerrainPush.clip, 96 bytes now). Ground pack
  `build/ground/ground-v15.pak`. The user will settle the horizon panorama later.
- Limits: the parapets' east end runs 0.71 past the deck (columns are 1.73 apart; the west end, where the party starts, fits within 0.13).

## Torre de Vigia: paredes e janelas 3D + Luz das janelas (2026-10-21; the user's pasted spec "iluminação padrão para ambientes internos
onde a luz natural entra pelas janelas ... sem renderizar o céu", then "create real windows and watchtower walls that actually feel like a real
design with the textures at watchtowerwalls", then "update the editor to make it easier to use these 3D walls, segments, corners and windows")

- **Read `content/models/README.md` § Architecture.** The kit = `content/models/architecture.json` (ids `ember-arch-watchtower-wall / -window /
  -slit / -wall-torch / -portal / -door (animated: opens for anyone within a hex, `updateDoors`) / -gate (4 hexes, open, the entrance) /
  -stairs-up / -stairs-down (2026-10-21 late, the user's "faz": 3D over the stairs waypoints, each with a torch; the down well cuts the
  ground, `Renderer::setGroundHoles`, TerrainPush now 128 bytes = the push constant limit)`),
  built in C++ by `world/Architecture.cpp emberGeometry` (ArchitectureDef `emberStyle`, `textureRepeat`;
  extra materials tagged in the UVs, `emberMaterialTag`; Engine2's own pieces untouched, `check-architecture` still bit-exact), textured with the
  user's `watchtowerwalls.jpg` (`content/textures/walls/watchtower-v01.jpg`, `/ember/...` URLs cooked from content/ by `Cooker architecture` into
  `build/meshes/architecture.pak`; the old pack kept as `architecture-20261021-before-watchtower.pak`). `catalog_models.py` now also merges the
  architecture (decorations + `editor.architectureDefs` + `doorVariants` + first in `architectureOrder`): re-run it after export_editor_catalog.ts.
  Diagonal neighbours join (one continuous diagonal wall at the octagon's corners); a window's splay / a torch faces the board's centre by itself
  (MapStage buildArchitecture, `emberAxisAngle`); walls / windows / the door block like Engine2's walls (`blockEmberProps`), the portal is open.
  The torch wall carries its own light (`ArchPiece::light`, buildLights) and an emissive flame mesh (no VFX flame yet).
- **Shadow-pass finding (2026-10-21):** meshlet.hlsl culls single-sided triangles by their screen winding in every pass, and in the sun's
  orthographic pass it is the FAR faces that survive (a slab's underside is what the shadow map stores). Closed solids still shadow the ground,
  thinner by their thickness; anything inside or just above a caster's far face is lit. The roof is double-sided for that reason; the general
  case was left as it is (measure before changing every shadow).
- **Luz das janelas** (panel, indoor maps: OFF / real; `--window-light N`; on by default for `watchtower-gate-floor / -barracks / -command`
  (`kWindowLightMaps`), every other map unchanged): the indoor map takes the outdoor light path (the hour's sun / the moon through the air,
  the 24 h "Hora" choices), under a shadow-only roof slab (`buildRoof`, InstanceGpu flags `kInstanceShadowOnly | kInstanceAlwaysSeen`,
  cull.hlsl; double-sided; from the lowest wall's ground + 4.0 to the highest wall's + 4.6; reaching the outermost wall pieces and no
  further, centred on them, never on the camera's pivot: three leaks found and fixed on 2026-10-21, captures 002-020), so the key light
  reaches the floor only through the windows, slits and the doorways; the sky's ambient at
  `kWindowSkyShare` (0.08) of the sky, the meter at `kWindowMeterShare` (0.04) of the outdoor reading (floor 2.7 lux = the old torch-lit
  room's reading; at 0.3 the night came out 36 x too bright and every torch burned its wall white), no twilight darkening by day, at night
  the indoor darkening without the moon's 2-stop cap, no aerial perspective, and Fundo forced black (the sky is never drawn). OFF = the old
  3 lux fill (`darkIndoor()`).
  **100% interior (2026-10-21, the user: "a única coisa que vem de fora é a luz em si"):** every outer-ring piece has a shadow-only copy
  `kSkirtOffset` (0.75) outside it (`ArchPiece::skirt`, same holes), a shadow-only plinth under the board (`plinthMesh_`), the roof's
  margin 1.3: the sun lights no outer face, only what it reaches through the openings. Local lights cast no shadow, so `LocalLight` has
  `facing` / `behind` (48 bytes now; `lightFacing()` in scene.hlsli): the openings' lights and the wall torches light their room side only.
  Outside, only the map's ground image: `MapDraft::emberFloor` on an indoor map = resolve.hlsl mode 7 (the image flat at the board's base,
  `emberFloorTile` wide, mirrored; black above the horizon, no sky); the watchtower's 008 uses its own background `watchtower-entry-bg.jpg`.
- **The user's photographed materials (2026-10-21 late, `Attachments/watchtower`): read `content/models/README.md` § "The user's
  photographed materials".** Kit: oak / iron / dark oak / steps / fire textures on the tagged parts (`emberMaterialName`, ArchitectureDefs
  `materials`, catalog `editor.architectureMaterials`). Props: 12 Torre props and 5 cemetery tombs re-exported with the photos as new
  versions (`blender/retexture_photos.py`), pack `build/meshes/props-v4.pak` (JOGAR.bat uses it first), placed on
  `watchtower-gate-floor008` and the new `cemiterio-esquecidos007` (= 006 with the tombs swapped; the campaign plays the highest serial).
- **The light rule (the user, 2026-10-21, furious, said in parts: "o jogo tem um relógio real com hora real com sol real e lua real, isso
  tem que valer em todos os mapas a única exceção são mapas marcados como subterrâneos"; "sliders servem unicamente para decorar e
  testar"; "Internos também ficam sem o céu, terão um teto desenhado" (the ceiling: not built, a later part)).** Never invent floors /
  boosts for the light (a moonlight floor was built and rejected), never undo work on your own, never act before understanding: ask.
  **Subterrâneo** = the Map Editor's Ambiente, third option (`"underground": true` + environment "indoor"; `MapDraft::underground`;
  MapStage `underground()`: the indoor path with no light from outside, Luz das janelas never, its panel row hidden). No map carries the
  mark yet. MapStage `gameHour_` falls back to `kGameStartHour` (8 h, a new save's) when no clock is given (Modo teste's classic map,
  captures), and the panel's Hora "do mapa" is now "do jogo": built, NOT yet seen by the user. Not done yet: indoor maps without windows
  (the Inn) still take no light from outside (darkIndoor); the drawn ceiling.
- **Gotcha (2026-10-22): `build/debug` had NO header dependencies recorded (`ninja -t deps <obj>` said "#deps 0" for every object),
  so a header change (Battle.h's `BattleCell::level`, 18:30) never recompiled Battle.cpp / SceneData.cpp / SpriteSystem.cpp while
  Spells.cpp and main.cpp were compiled with the new layout: the debug exe mixed two layouts of the same structs and crashed right after
  "poses: N of N map units in the catalog" on every map, old or new files alike.** The release folder tracked them fine (6 deps on
  Battle.cpp.obj). The stale folder was set aside as `build/debug-stale-deps-20261022` and debug configured afresh (scratchpad
  `debug-fresh.cmd`: vcvars + `cmake --preset debug -DFETCHCONTENT_FULLY_DISCONNECTED=ON -DFETCHCONTENT_SOURCE_DIR_<DEP>=build/release/_deps/<dep>-src`).
  **Whenever a debug run crashes where release does not, run `ninja -t deps` on an object of a changed header before hunting the code.**
- **Gotcha (2026-10-21): `build.bat` piped through PowerShell reported exit 0 on a failed shader compile** (a release with the old shaders
  was captured as if new). Always grep the log for `FAILED` and check the .spv / exe timestamps.
- **Map Editor, 3D Walls:** "TORRE DE VIGIA" buttons (Parede / Janela / Seteira / Tocha / Portal / Porta) and the wall tools: Clique (as before),
  **Traçar parede** (drag on the preview from hex to hex: the armed piece in a straight or diagonal run; walls under it swapped for the brush,
  other props kept), **Fechar sala** (drag corner to corner: the four walls). The run is shown as amber rings while dragging (`wallCells`,
  `layWalls`). Abrir porta / Fechar passagem swaps the kit's door and portal.
- **The map:** `content/maps/ember/watchtower-gate-floor008.json` = 007 through `tools/scripts/maps/watchtower_walls.py --windows --torches
  --gate --stairs` (the 3D stairs over both waypoints, up (29,5) and down (10,29); 154 walls, the 4-hex gate at the entrance (2 wall pieces removed for it), 25 openings on the outer ring, 19 wall-torch cards moved
  onto the walls as torch pieces). No animated door is placed on it yet (the kit's "Porta" in the editor, or `--door` instead of `--gate`). **The user was finishing their own improved version of this map while this was built:** if they
  save over 008 from a draft of 007, run the script on their file again. Test files: `build/engine2-test/{maps,scenes,battles}/watchtower-gate-floor008.*`
  (Engine2's exporters skip the `ember-arch-*` ids: MapStage rebuilds the scene with SceneBuilder, as for any `ember-*` prop).
- Captures: `Sandbox --map build/engine2-test/maps/watchtower-gate-floor008.json --ground build/ground/ground-v16.pak --scene ... --sprites
  build/sprites/battle.pak --meshes build/meshes/props-v3.pak --heroes build/engine2-scenes/roster.json --battle-rules
  build/engine2-test/battles/watchtower-gate-floor008.battle.json --hour 4 --zoom 0.7 --capture captures/watchtower/NNN.png` (local, git-ignored).
  The door's opening: a draft with `--door` exported under another name (`build/engine2-test/*/watchtower-gate-floor-doortest.*`) and
  `--battle-script "goto:Kael:19:31"` (Kael beside the door: it stands open in the capture).
- **Verified (2026-10-21):** debug + validation exit 0, 0 validation lines (the gate floor by morning; the door test with Kael beside the open
  door); release captures: the room dark under the roof with the sun up (no wall / pillar top lit, no strip along any wall), the low western
  sun throwing streaks through the west windows and slits onto the floor (capture 053, `--sun 180 15 --layers 3`), the torches' pools on the
  walls by night, the door's leaves swung open beside Kael, the 4-hex gate with its open leaves catching the sun through the arch;
  `Cooker check-architecture` on Engine2's 1672 reference cases, every piece bit-exact (the exporter's 44 wall-3d-low cases dropped: that
  piece was removed from the catalog). **Not verified:** the look in
  the window (the user judges: Luz das janelas + Hora in the Escolhas panel on the watchtower's gate floor), the editor's drag tools by hand,
  the door's passability in play (code path only), cost (155 pieces + leaves, the roof: not measured).

## Veneno / Doença em 3D (2026-10-20; the user: Engine2's were WebGL and missing here, "recria algo similar em 3D")

- `engine/render/vfx/StatusFx.*`: Engine2's drawStatusFx idea (haze, motes feet to head, vapour lines; green quick = poison, violet slow =
  disease, both layered) standing round the body: lit vapour puffs wrapping the card (eye side over it, far side behind), a pulsing stain on
  the ground, three spiralling wisps (tubes), motes, a faint light. `MapStage::syncStatus` (u.poisoned / u.diseased, only while the card is
  drawn). Script step `sick:<unit>:poison|disease`. One look, no OFF (combat VFX rule). Verified: debug + validation capture exit 0
  (`captures/status/`), release built. Not verified: the look in the window (the user judges).
- World map field spells: Criar Comida e Água's row shows Engine2's `game/icons/create-food-and-water.png`.

## Muralha gótica: os parapeitos da Ponte de Pedra em 3D (2026-10-21; the user: "make those parapeitos from the Stone Bridge in 3D using
them as base and texture ... carefully designed detailed gothic walls to be used later in the game too", "its both sides of the same design")

- **Read `content/models/README.md` § "the gothic parapet kit".** Three pieces in the watchtower's kit (`architecture.json` styles `gothic-wall` /
  `gothic-niche` / `gothic-pier`, `Architecture.cpp gothicGeometry`): one wall with two faces, the guardians + tracery windows towards the
  board's centre (MapStage `faced`, like the windows), the tally-marked panels + drain holes towards the void; pilasters with pinnacles at every
  run end, corner and junction by themselves, mid-run with the Pilastra piece; a real pierced arcade on top. The art comes from the user's two
  parapet PNGs cut into an atlas (`tools/scripts/models/gothic_parapet_atlas.py` → `content/textures/walls/gothic-parapet-v01.jpg`, material tag
  7 "gothic" with UVs authored in the builder; `gothic-stone-v01.jpg` for the plain stone). Editor: 3D Walls → "MURALHA GÓTICA".
- **Bug found on the way (MapStage `meshOf`): a `MeshId` is the mesh's index, so the map's first mesh is 0, which the piece builder read as
  "no mesh": on a map with no 3D models the first wall piece was dropped.** Now `std::optional<MeshId>`, with a warn naming any piece that
  builds nothing. (`roofMesh_` / `plinthMesh_` keep 0 = none: they are built after the pieces.)
- Verified: debug + validation capture exit 0, 0 validation lines on the test draft (`make_gothic_bridge_test.py`, both parapet rows + an L,
  82 of 82 pieces built); `Cooker check-architecture` on Engine2's reference cases unchanged; release captures `captures/gothic/011-014`
  (deck face, corner, void face, close-up). Not verified: the look in the window (the user judges); the editor's buttons by hand; cost (82
  pieces on the bridge, not measured). thebridge023 unchanged (cards).
- **A Ponte de Pedra 024 (the user: "Faz a thebridge024 com os parapeitos 3D (kit gótico) no lugar das cartas, a 023 fica como está"):**
  `content/maps/ember/thebridge024.json` by `tools/scripts/maps/make_bridge_gothic.py` (= 023, the 16 parapet cards replaced by 80 kit pieces
  over both outer rows: pier / wall / niche / wall every four hexes; the campaign plays the highest serial, so 024 is live; 023 untouched).
  `deckClip` now also ends the deck at a 3D parapet's outer face (`ArchPiece::deckHalf`, the ground runs under the wall), so "the tiles
  finish right at the walls" still holds without the cards. Test files `build/engine2-test/*/thebridge024.*`; captures `captures/gothic/020-024`.

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

## Picking by the figure's body (2026-10-18; the user: "o path não se forma atrás de outras unidades", "quase impossível de
selecionar os alvos")

- A card's whole rectangle used to take the click / hover, so any card in front hid the hexes and the targets behind it.
  Now `CardRect` carries the body (`bodyX / bodyBottom / bodyTop / bodyHalf`: the frame's trimmed picture from the pack's
  crop box, its middle 60 %), `pickAt` tests that band only; `pickWalk` (hover and click with no spell open): the ground hex
  wins over every card whenever the picked unit can walk there (`inReach`); a spell's aim keeps `aimPickAt` (a foe's body,
  else the ground). No re-cook: the crop box was already in every clip header. Not done (the user: no re-cooking): per-frame
  alpha masks for pixel-exact picking; a stretched arm or weapon outside the band is not clickable.
- Verified (debug + validation): the script click on (14,10) picks the Piqueiro where the war dog's card in front used to
  win; `pick:Kael; click:13:22` walks him behind Neera's card. `picktest` still reports 13 edge hexes at a height step
  picking the hex in front (the ground ray, pre-existing, not the cards).

## Characters loaded whole before the fight; Personagens Alta / Leve (2026-10-18, the user's plan; read this before touching sprites)

- **The blur on a change of animation** ("efeito horrendo de blur ... de idle para att"): a pose was streamed in only when it
  started, and until it landed the card drew the clip's 64 px tail; with the map's 512 MB budget clips also evicted each other.
- **Tried and rejected the same day: layers** (`6c1d7bb`: a half-resolution base pinned, the full resolution streamed in when the
  camera got close). Measured beside Voss (20 s turn + zoom): main-thread worst 156 ms when his full clip (up to 144 MB) loaded
  mid-play vs 20 ms with everything loaded at the start; the user felt it. **Never stream a fight's characters during play.**
- **The plan (the user: "esse é o plano"):** every pose of every unit on the map is loaded before the fight opens and held to its
  end (`SpriteSystem::pin`, MapStage `pinUnitArt` / `pinArt`; the loading screen waits for them). The **cast with two versions**
  (`content/sprite-quality.json`: the 8 main characters, recurring villains, hi-def mega bosses; catalog keys) follows Opções >
  Gráficos > **Personagens: Alta / Leve** (`GamePrefs::characters`, taken when a fight loads): Alta = full resolution, Leve = the
  same frames and timing at half resolution (the cooked clip's level below the top, cut off the same asset: no second pack, a
  quarter of the memory). Everyone else has one version, always whole. **Alta that does not fit the GPU becomes Leve by itself**
  (logged: "Personagens Alta needs N MB, the GPU has M MB free"). Why: the game outlives today's cards; Alta exists for players
  with big GPUs even when this RX 6400 cannot run 8 Voss-sized heroes (~4.5 GB at full resolution).
- Numbers (aldeia015 test battle, 17 units, 71 clips, RX 6400): everything whole 1243 MB (voss-veo 564), game GPU memory 2556 MB
  of the driver's 3059 MB; Voss at half = 141 MB.
- **Battle hitches (`92ff68f`, 2026-10-19):** slow frames log where the time went ("slow frame" / "slow stage frame" / "slow ui
  record", > 50 ms). Never let ImGui's Vulkan backend upload textures (a vkQueueWaitIdle per texture: DebugUi::uploadTextures does
  it); never resample or decode UI images on the main thread (UiArt Resampler, MapStage::preloadHud); never draw text at a size
  that follows the zoom (each new size bakes glyphs). Measured beside Voss: main thread 99th 19.5 -> 2.1 ms, 0 frames over 16.7.
- **Never force-kill a Sandbox window** (Stop-Process -Force while the GPU works): the process sticks "exiting" with 0 handles,
  access denied, holding GPU memory until a reboot. Close test windows like the X (`CloseMainWindow`).

## Cultista Ancestral: dano, dano seguido, morte from sprite sheets (2026-10-22)

- The user's sheets in `Attachments/Cultist` (36 frames, 6 x 6, TexturePacker JSON beside each). **The user counts frames from 1;
  never renumber them.** Dano = frames 1-14 of the root sheet; frame 15 (the red slash) is in no clip; Dano seguido = 16-36; Morte =
  the whole `Fall` sheet. The two mp4s there are those sheets played as video (lossy, chequerboard baked in): never a source.
- `video_sprite.py` reads a clip from a sheet (`"sheet"` in the cut spec: frames, timing and alpha as drawn, no matting / unmixing /
  resampling); spec `content/sprites/cultist-v2-ember/cut.json`, set `cultist-v2-ember` (hero `cultistV2`; idle / attack / cast / walks
  keep cultist-v2), poses hit / hit2 / death.
- MapStage `showEffects`: a hit from the same action as the unit's previous hit (Golpe Duplo's next blow), or landing while its hit
  reaction plays, is "in a row": a unit with a `hit2` pose plays it (a hit while hit2 plays lets it play on); every other unit unchanged.
  Script step `hp:<unit>:<n>` (a target that must survive). Golpe Duplo at 15 = 3 blows: the 3rd starts hit2 again (the 21-frame
  clip, 4 s at the normal pace, is over by then).

## Battle bar: 12 sockets F1-F12 + Atalhos (2026-10-18; the user: "12 sockets ... F1 ao F12 e um outro botão para editar")

- MapStage `hotbarOf` / `storeHotbar` / `drawHotbarEditor` / `pressHotbar`: each hero's 12 sockets (a spell / skill kind or
  `potion:<id>`); no layout yet = its spells then its potions. Campaign: `save.hotbars[name]` (12 strings); Modo teste: kept for the
  session only. "Atalhos" (after the sockets) = drag from Disponíveis into a socket, socket onto socket swaps, right click empties.
  Sockets shrink (36 -> 24 px) when the row is crowded. F1-F12 press the socket; Modo teste's numbers moved to **Ctrl+F1**.
  The bar's "Status" button is gone (the portrait opens the sheet). The confirm panel's hit: two decimals ("93,83 % de acerto").
  Capture: `--map-ui hotbar` (the first hero's editor). Not verified: dragging and the F keys in the window (no scripted input).

## Left click = Cancelar; daggers behind the body (2026-10-22, `1a25694`)

- Battle: while a spell / potion is aimed, a strike staged or Atacar / Mão Secundária open, a left click on anything that is
  not a valid aim / marked foe cancels it and does nothing else (MapStage `cancelAction`, also the bar's Cancelar). Walks are
  never undone by a click. Proven by script (release + debug with validation, exit 0, 0 validation lines,
  `captures/cancel/003`, `004-debug`): a spell opened and Atacar opened are both closed by a click outside, nothing walks.
- Effects are drawn over every card (the user, 2026-10-18); `VfxDraw::behindCard` + `cardAt` is the one exception: the
  Renderer splits the sorted card draw and puts that draw right before the card standing at cardAt. DaggerFx uses it for
  the orbit's far half. Capture: `captures/daggers/back-001.png`.
- Personagens Alta / Média / Leve is read only when a fight loads (`pinUnitArt`): a change mid-fight shows from the next
  fight. Alta that does not fit the GPU falls back to Leve by itself (the log says "no GPU room for Personagens Alta").
- **High ground for archers (the user's "faz", 2026-10-22, `0cbc011`):** `BattleCell::level` = the board's real elevation
  (set by MapStage at fight setup). An arrow flies over any in-between cell lower than the archer's (clearShot); a bow /
  crossbow blow from a cell higher than the target's gets +5 to hit and +10 % of ATK (`kHighGroundHit`, `kHighGroundAtkPct`
  in Battle.h: first values, the user may change them). Script step `highshot:<archer>:<target>`.
- `--battle-save-check` fails on 4 battles in the working tree because of another session's uncommitted
  `grantClassSpells(u)` in `Battle::applySnapshot` (passes at `0cbc011` without it); not this work's.

## Waiting on the user

- **Video → sprite pipeline: built (2026-10-18, `1fceba8`), first used on Voss** (`content/sprites/voss-veo/cut.json`, the
  user's Veo videos in `Attachments/VeoVoss`, one folder per move; the user names each video's move and start time).
  `tools/scripts/sprites/video_matte.py <video> <out> <first> <last>` (BiRefNet general on the CPU, ~11 s / frame, 13 GB
  RAM: one run at a time; resumable; run it detached with Start-Process, sessions and power cuts kill child runs) →
  `video_sprite.py <cut.json>` (every native frame, no resampling, wall unmixed, each clip its own scale + feet line,
  walks held in place + loop search, crossfade for idle seams, impact frames) → `content/sprite-overrides.json` "sets"
  (`apply_sprite_overrides.py`, new set + the hero's poses; MapStage takes a replaced idle from the catalog) → cook.
  The user chose every frame at the video's resolution ("max quality, downsize later"): Voss's 6 clips = 564 MB on the
  GPU (voss-final ~160). The staff counter (`counterStaff`) plays when he counters a foe beside him (MapStage
  `strikeBeside`); a pose's `impact` sets when its blow lands. Not judged by the user in the window yet.
- Reference images / one clearly described moment, to turn R1–R8 into D1 pass/fail criteria.
- The final camera angles.

## Video monsters: Corrosive Slime (+ Slime36 / Slime64) and Giant Spider (2026-10-22)

- **EmberEngine's own unit classes** = `content/ember-classes.json` (a battle file's exporter knows only the old classes: export_battle.ts
  builds the spawn as the `standIn` and writes it back under its own class; Battle.cpp `applyEmberClass` makes it what the file says at load:
  stats from class-stats.json at its level, mov, size in hexes, the ATK blow, resistances, tier uses, spells). MapStage `hasEmberProps`: a
  map with such a unit builds its scene with SceneBuilder. `tools/scripts/sprites/catalog_ember_classes.py` adds them to the Map Editor.
- **Corrosive Slime** (`corrosiveSlime`, 1 hex, rank D; `corrosiveSlimeLarge`, 2 hexes, stats x 1.75, rank C): Milf's Acid Arrow (`acidArrow`,
  tier 2: 2 / 3 a fight) and Névoa Ácida (`acidCloud`, tier 3: 1 / 2; the persistent zone + AcidCloud vfx taken over from another session),
  AI branch in planEnemyCastRaw (`cloudCast`). Art from the user's videos (`Attachments/Corrosive Slime`), one art for both sizes.
- **Slime36 / Slime64** (`corrosiveSlime36` / `corrosiveSlime64`, the user: "to see what happens in game"): the Small Slime with each clip
  cut to 36 / 64 frames picked evenly (video_sprite.py `frameCount`, no blend, the clip's duration kept). GPU memory, Alta / Média:
  slime 445 / 139 MB, Slime64 311 / 97, Slime36 189 / 59 (Leve ~ a quarter of Média). Their floating bubble beside the body is
  cleared (video_sprite.py `despeck`, the user: "a legit bubble, but in game it looks weird").
- **Voss by quality (the user: "36 is on with LEVE setup and 64 is on with Medium. High plays the full version"):** Alta = voss-veo (all
  frames), Média = `voss-veo-64-media` (64 frames, ~610 px), Leve = `voss-veo-36-media` (36 frames, halved like every Leve); the old
  all-frames Média (`voss-veo-media`) stays in the pack, `"unused": true`. Roster poses carry `medium` / `light` (apply_sprite_overrides
  `mediumOf` / `lightOf`), MapStage `artSet` picks by Personagens, and a copy with fewer frames plays on the full clip's timing (the frame
  index mapped). Personagens is read when a fight loads, never mid-fight. Measured on beasttest: Média 202 MB, Leve 30 MB (Alta 657 MB
  did not fit with the rest, 3161 needed vs 2442 free, and fell back to Leve by itself).
- **Neera Hydra+ (2026-10-22):** her idle, off-hand strike (attackShort), walk right and bow attack from the user's sheets
  (`Attachments/NeeraHydra`, 36 frames each, timing as drawn), set `neera-hydra`; walkLeft / attackLeft / attack2 / cast keep the old art.
  These are "max-px" sheets (each scaled to fill its frame): every clip has its own `figurePx` (her upright height in that sheet) so she
  stands 2.66 units like before. Her bow sound is `Bow2-Balanced-High.mp3` (content/game/public/game/SoundFX), played at the release
  (MapStage `neeraBow`, `arrowRelease` "neera-hydra:atk" = frame 27). `content/unit-size.json` scales a unit's card (MapStage
  `unitSize`): empty; the user is deciding on the Cultista Ancestral at 0.9.
- **Giant Spider** (`giantSpider`, 3 hexes, rank D, mov 4, bite 2D6, poison 50 %, no spells; working stats, the user sets the real ones later):
  `Attachments/Giant Spider` (idle + cast, move + attack, hit, death at 5 s). Matted with BiRefNet general (idle/cast, move/attack) and
  **general lite** (hit, death; video_matte.py `--model lite`, the user's "light mode": ~11 s a frame vs ~13). Alta 312 MB, Média 98 MB.
- Pipeline for a video monster: `video_matte.py` → `slime_pipeline.py --dir <set> --width <idle body width> [--cut-only]` (cut, world size
  from the idle's width, Média copy at 0.555; with several new sets, cut them all with `--cut-only`, then `apply_sprite_overrides.py` +
  `catalog_ember_classes.py` once: the overrides list every set, so each one's cut must exist) → cook. Long mattes run unattended from Task
  Scheduler (`EmberSpiderMatte`: scratchpad spider/matte.ps1 through hidden.vbs, no window; two mattes at once, resumes after power cuts,
  then cuts + roster + cook).
- Test battle: `content/maps/ember/beasttest001.json` (Map Editor: open, Testar) = `build/engine2-test/{maps,battles}/beasttest.*` (aldeia015 +
  Small, Large, Slime36, Slime64, spider). Verified: release capture exit 0, all five drawn, their Média sets pinned. **Not verified:** the AI
  casting both acid spells in play, debug + validation, the spider's look and size in the window (the user judges).

## Preparo de mapas em C++, sem node em tempo de execução (2026-10-22; the user: "porta o preparo de mapas (make_test_map,
export_scene, measure_art, export_battle) para C++, sem node em tempo de execução, verificado contra os battle.json atuais dos mapas de prova")

- **`engine/world/BattleExport.h/.cpp`** = the four steps the game ran through node (vite-node + the rule source's TypeScript) whenever a
  mission's files were older than its map: `prepareCampaignMap` (make_test_map.ts: the six heroes placed by the same breadth-first walk from
  the first spawn, in roster order; legacy class ids repaired; the whole map file written again), `sceneJson` (export_scene.ts + measure_art.py:
  SceneBuilder's scene written as the scene file SceneData::load reads), `exportBattle` (export_battle.ts: cells with the decorations folded in
  (hexprops), units as spawnUnit builds them for a fresh party and enemies at the mission's level, exits, locks with the cells their opening
  changes, every unit's sound cues, the mission's music). The reference answers `Cooker check-battle` compares against (forecasts, rolls, casts,
  turn starts, AI) are NOT written: they remain the TypeScript exporter's (`SPELL_REFS=1`). `apps/sandbox/MapExport.h` runs the three steps
  (the campaign's missions and the editor's Testar); its `MapExportKind` overload stays for the committed callers.
- **The rule tables as data: `content/engine2/battle-rules.json`**, written by `tools/scripts/engine2/export_battle_rules.ts` (classes with
  stats / spell tiers / spell lists at every level, charges, starter weapons; the enemy spell pools; terrain; decorations with footprints,
  solidity sets, doors and exits; weapons and gear; the sound cues recorded through audio.ts per cue and per sprite; the music themes).
  Re-run it after a rule change: `node content/game/node_modules/vite-node/vite-node.mjs --root <scratch> --config
  content/game/vite.export.config.mjs tools/scripts/engine2/export_battle_rules.ts -- content/engine2/battle-rules.json` (a build-time tool;
  the game never runs node).
- **Cooker:** `check-battle-export <maps dir> [--player-level N] <battle.json...>` (the C++ file against the exporter's, the fields the game
  reads), `prepare-map <map> <out>`, `export-battle <map> <out> [--player-level N]`, `export-scene <board.json> <catalog> <public> <map> <out>`.
- **Verified (2026-10-22):** the 9 proof battles in `content/engine2/battles` match field by field (units with sounds, cells, locks, exits,
  music; the two wisp-forest files predate the exits' `returnConnector` / `connectorDirection` keys, compared on the keys they have); the 17
  campaign battles the TypeScript pipeline had made in `build/engine2-campaign` match (except the 2 cemetery maps whose files predate the
  cemetery theme); `prepare-map` identical to the TypeScript's prepared maps on 5 maps (watchtower and cemetery included); the scene writer
  round-trips (`check-scene-builder` 0 differences on 5 maps); a release capture `--title --title-screen mission-test:aldeia` prepared
  aldeia017 inside the game (map, scene, battle, 13 units in play) and its battle file equals the TypeScript one; the same capture in
  debug with validation: exit 0, 0 validation lines (once `build/debug` was rebuilt afresh: debug-stale-deps gotcha above). The 164 battles in
  `build/engine2-test` are NOT a reference (an older exporter: the test roster at 15 with a save's gear, `debugFreeCast`, no locks).
- **Bug found in the TypeScript exporter, gone here by design:** a map whose tiles use the board's own terrains `highwood` / `highruin` /
  `deadtree` (9 of 261 in content/maps/ember: colina004, cripta002, fortaleza004, muralha001, passagem001, profundezas002,
  random-encounter-1-001, random-encounter-2-001, templo002) got misaligned combat cells (TILE_CHAR had no char for them, so "undefined" became
  nine tiles for one). Here those tiles count as plains in combat; what they should be worth (woods? hill?) is the user's decision, not taken.
- Nothing removed: the TypeScript exporters stay as tools (the `check-battle` references need them); `content/game` (the snapshot of the rule
  source and assets another session made on 2026-10-22, 5.4 GB, untracked, only its node_modules ignored) is still where the game reads its
  assets from (`--game-assets`), but node, vite-node and the TypeScript are no longer needed to play.

## A Estalagem: panorama 02 over the user's Tavernground (2026-10-22; the user: "make the background image from the tavern into a
Panorama image", "Tavernground in attachments for you to use on the ground layers")

- The tavern (`estalagem`) already had its panorama (`inn-panorama-v01.jpg`, panorama 01). Panorama 02 = the same painting with the user's
  `Attachments/Tavernground.jpg` (copied as `content/backdrops/tavern-ground-v01.jpg`) flat around the board: backdrop-fit.json entry
  `"inn-panorama-v01.jpg#chao"` (a `file#name` panorama entry = another fit of the same painting), `ceiling: true` (mode 3 always: the
  room's own ceiling, never the real sky), groundTile 72 (first value, the user judges). Panorama 01 kept as it is, in the Fundo choice.
- `MapStage::loadMapPanoramas` leaves out a panorama whose painting or ground image is not in the ground pack (logged "left out"): panorama
  02 joins the tavern (and opens first, the newest) only once a ground pack cooked after this carries `tavern-ground-v01.jpg`.

## Kept, not in use (the user, 2026-10-22: nothing is thrown away; list here what was made by mistake or is not used)

- `build/debug-stale-deps-20261022`: the debug build folder as it was on 2026-10-22 night (no header dependencies recorded, objects of
  two struct layouts mixed; see the gotcha in Build and run). Set aside, not used; `build/debug` was configured afresh.
- Sprite packs set aside when a new one was swapped in for JOGAR: `build/sprites/battle-20261021-before-voss-hitdeath.pak`,
  `battle-20261021-before-voss-media.pak` (and the older `battle*.pak` beside them). The pack from before the Cultista Ancestral's
  hit / hit2 / death was deleted on the user's order (2026-10-22: "pode remover o backup do cultista ancestral").
- `build/release/CMakeCache-20261022-broken-tools-notfound.txt` (+ `-before-sdl-checks.txt`, the same before a first repair): the
  release cache as configured at 00:14 on 2026-10-22 without the MSVC tools (linker / lib / mt / rc NOTFOUND, every SDL check
  failed on C1902): SDL3 stopped compiling; the folder was configured again from a fresh cache.
- Release build outputs damaged when a build was cut off at 17:48 on 2026-10-22 (zero-filled / half-written, set aside, not
  used): `build/release/engine/CMakeFiles/EmberEngine.dir.damaged-20261022` (its shared compile PDB was broken: link.exe then
  crashed with an access violation), `*.damaged-20261022` objects in `apps/sandbox/CMakeFiles/Sandbox.dir`, and in `build/release/bin`
  `Sandbox.{exe,ilk,pdb}.broken-link*-20261022` + `Sandbox.exe.broken-manual-link-20261022` (crashed links) and
  `Sandbox-20261022-1730-in-use.exe` (the user's 16:57 exe, renamed while it ran). Two `link.exe` processes from the crashed links
  stay stuck with 0 handles until a reboot.
- Voss's first hit cut (`content/sprites/voss-veo/hit-v01` before the wall key) was rewritten in place on 2026-10-21 before any commit;
  its video frames stay in `build/video-cut/voss-veo/hit`, so it can be cut again with `video_sprite.py`.
- **Milf's Acid Arrow (`acidArrow`, Spells.cpp / SpellsE2.inc, 2026-10-22):** built when the user's "Milf's acid arrow" was read as
  AD&D's spell; the user's name belongs to Neera's `venomArrow`. Kept and in use: the Corrosive Slimes cast it.

## Later milestones

M4 world cells + terrain · M5 lighting/atmosphere · M6 editor · M7 gameplay port + cutscene timeline.
Exit tests and demonstration gates in ARCHITECTURE.md §9.
