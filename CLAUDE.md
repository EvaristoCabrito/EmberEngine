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
- **Engine2: game design yes, technology no** (user, 2026-10-14: "nothing else is wrong with its game
  design"; its engine stutters on spell effects and its sprite animation is subpar). Port its rules, maps,
  board looks (R8: smooth / hard / tabletop), editor features and content faithfully; build all tech fresh.
  **Use Engine2's real assets for every test** (maps, ground, decor, models, sprites, fx): never placeholders.
- **Priorities (user, 2026-10-14):** stability (no crashes, validation errors, streaming failures) → smooth
  gameplay (consistent frame times while moving the camera, animating, casting; not averages) → streaming →
  rendering foundation (2.75D depth, occlusion, real dynamic light) → final content. Keep "a hook exists"
  separate from "the feature works": every report says which.
- **Lighting direction (user, 2026-10-14):** a hybrid Luminous (FFXV) + Unreal-lessons system, our own C++ /
  Vulkan, no HW ray tracing required; the prototype's colour (Engine2 `diorama.html`) is the floor, not the
  ceiling (ARCHITECTURE §7.1b).
- **2D/3D split (the one piece of Engine2's tech carried over):** characters, props and decor = photoreal 2D cards;
  houses/buildings and terrain shape = real 3D (Meshy for buildings); ground = photoreal texture on 3D terrain.
  Do not port Engine2's lighting/camera/ground rules, and do not ask what is 2D vs 3D.
- **Camera:** free (orbit + zoom + cinematic moves) until the user picks the angles; then locked.
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
     [--sun AZ EL] [--layers MASK: 1 sun 2 shadows 4 indirect 8 local]`. Window keys: right-drag turn/tilt, wheel
     zoom, WASD pan, 1/2 looks, F5–F8 lighting layers, Home = Engine2's normal view. Proof maps: aldeia015
     (lights, houses, photo ground), random-rimewater-shrine001 (water, 3D trees); stress: wisp-forest-2004.
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
- `engine/render`: Renderer (frame constants → light clusters → sun shadow (cached: ground + meshes) → card
  shadow (per frame) → mesh passes → ground → water → cards → tonemap → UI), Camera, SpriteFormat/SpriteDraw
  (80-byte cards: own axis, unit depth pull, flame glow, lit flag, casts flag)/SpriteSystem (M2), MeshFormat,
  TextureFormat, MeshGpu, MeshScene, **BoardGround** (both ground looks + water on the GPU, map palette of
  baked layers), DebugUi.
- `engine/world` (D1 port): **Noise/Hex/Board/GroundMesh** (Engine2's board and ground, bit-exact),
  **GroundAssets** (ground texture names), **SceneData** (imported scene: cards, models, units, lights).
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
  shadows), `lights.hlsl` (cluster build), `tonemap.hlsl` (three.js's ACES fit, like the prototype).
- Vulkan gotcha found: fragment shaders may not bind a writable storage buffer here (no
  fragmentStoresAndAtomics): read GPU-written lists through device addresses (`vk::RawBufferLoad`).
- DXC gotchas found: `clip` and `triangle` are reserved names; per-primitive PS inputs need
  `[[vk::ext_decorate(5271)]]` + `[[vk::ext_capability(5283)]]` (DXC issue #6862).
- Shell gotcha: writing C++ through bash heredocs + Python mangled `'\0'` and `"\n"` twice; write patch scripts
  with the Write tool into the scratchpad and run them with `python -I`.

## Status (2026-10-14)

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
- **Not done (hooks only or missing):** volumetric fog (R7's literal fog, appearance) and the mask's open points
  (§7.4: walls not imported so block nothing; sight update on the main thread); indirect
  light is the hemisphere (no probes / GI); no sky, fog, bloom, tilt-shift; local lights cast no shadows; cards
  do not receive card shadows; walls/doors (architecture), floor waypoints, stand-on-log heights, the third
  board look (tabletop tiles, R8) not ported; unit animation is the idle only; no spell VFX; the ground material
  costs 4.1 ms at full clock on photo-layer maps (profile it before optimising).

## Next

1. R7's literal fog: world-space volumetric fog (froxels) constrained by the mask, gathering at the explored
   boundary (§7.4).
2. Profile and cut the ground material cost (photo layers + two 16-tap shadow lookups).
3. Sky + aerial perspective (Hillaire 2020) feeding indirectDiffuse/Specular; bloom; tilt-shift; then compare
   side by side with the prototype's colour (the floor).
4. Tabletop tiles look (R8 third look, `diorama.html` buildHexTerrain(true)); walls/doors.
5. Spell VFX on the GPU (one Engine2 spell end to end, no stutter), unit poses beyond idle.
Then D1 is shown to the user (§9).

## Waiting on the user

- Video → sprite pipeline (their earlier top priority, ARCHITECTURE §5): needs their videos organised as
  `content/video/<Character>/<move>.mp4` (they sit in `Downloads` / `Desktop\Characters` with generated names;
  don't guess the mapping). ffmpeg 9.0.1 and Python + PIL are installed.
- Reference images / one clearly described moment, to turn R1–R8 into D1 pass/fail criteria.
- The final camera angles (camera stays free until then).

## Later milestones

M4 world cells + terrain · M5 lighting/atmosphere · M6 editor · M7 gameplay port + cutscene timeline.
Exit tests and demonstration gates in ARCHITECTURE.md §9.
