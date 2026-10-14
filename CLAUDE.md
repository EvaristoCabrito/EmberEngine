# EmberEngine: handoff for the next Claude session

Read this first, then `docs/ARCHITECTURE.md` (design, measured numbers, milestone exit tests; §7.0–7.2 and §9
are the current frontier). Memory notes: `C:\Users\evari\.claude\projects\C--EmberEngine\memory\`.

## What this is

A game engine built from zero (C++20 + Vulkan 1.3, native Windows .exe, not three.js/browser) for the user's
game *Ember*: hex-grid tactics ("Tactics Ogre with HD-2D") with photorealistic 2D characters in a real 3D world,
cinematic feel, dramatic light and shadow, incredible spell effects, realistic ground. **Visual target: the
spirit of Square Enix's Luminous Engine (Agni's Philosophy, FFXV), taken into 2.75D: not Unreal, not HD-2D.**
The user has the image in mind; the vision is the standard (ARCHITECTURE.md §2 principle 9, §7.0 R1–R6).
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
- **2D/3D split (the only thing carried over from Engine2):** characters, props and decor = photoreal 2D cards;
  houses/buildings and terrain shape = real 3D (Meshy for buildings); ground = photoreal texture on 3D terrain.
  Do not port Engine2's lighting/camera/ground rules, and do not ask what is 2D vs 3D.
- **Camera:** free (orbit + zoom + cinematic moves) until the user picks the angles; then locked.
- **Workload:** finished game = ×10 today's sprite roster (MCs at 2× data, max party 8; "testing ×10 is enough").
  World density: the Ember maps are a floor ("bare naked and started stuttering"); M3 tests 10× their density.
- **Progress gates are the user's three demonstrations** (§9): D1 lit sprites in a spatial world, D2 complete
  environmental scene, D3 cinematic stress scene. Engineering tests are foundation, not visual progress.
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
- `engine/render`: Renderer (mesh passes → ground → sprites → tonemap → UI), Camera, SpriteFormat/SpriteDraw/
  SpriteSystem (M2), **MeshFormat** (cooked cluster-LOD mesh), **TextureFormat**, **MeshGpu** (GPU records),
  **MeshScene** (meshes/textures/instances on the GPU, double-buffered tables), DebugUi.
- `tools`: PakWriter, SpriteSource, SpriteCooker, **TextureCooker** (BC7 sRGB/linear, BC5, correct mips),
  **MeshCooker** (cgltf → transforms baked, authored LOD1+ skipped, tangents, clusterlod DAG + BVH, protect
  mask, edge limit, `--remesh` experiment), `tools/scripts/compare_meshes.py`.
- `apps/sandbox`: Stage.h (test-scene interface), SpriteParade.h, MeshStage.h, **BattleStage.h** (M3 exit test),
  **MeshCompare.h**, StreamTest in main.cpp. `apps/cooker`: the Cooker CLI.
- `shaders`: HLSL via DXC (`-Zpr`: `mul(v, M)` with glm). `scene.hlsli` = GPU layouts shared with MeshGpu.h /
  MeshFormat.h / MeshFrame (Renderer.cpp, static_asserted); `cull.hlsl` (early/late instance culling, BVH
  traversal, draw args), `meshlet.hlsl` (task/mesh/pixel → visibility buffer), `hiz.hlsl` (depth pyramid),
  `resolve.hlsl` (barycentrics + PBR; **provisional sun + sky lighting**; debug normal views).
- DXC gotchas found: `clip` and `triangle` are reserved names; per-primitive PS inputs need
  `[[vk::ext_decorate(5271)]]` + `[[vk::ext_capability(5283)]]` (DXC issue #6862).
- Shell gotcha: writing C++ through bash heredocs + Python mangled `'\0'` and `"\n"` twice; write patch scripts
  with the Write tool into the scratchpad and run them with `python -I`.

## Status (2026-10-14)

- **M0 + M1 done**, pushed (`67410f0`). **M2 done**, pushed (`98c727a`, `c08ec38`): sprite cooker + residency;
  ×10 roster (3,750 clips, 26.3 GB) PASS, worst frame 4.6 ms, 0 validation errors.
- **M3 in progress, committed locally, NOT pushed** (`a4861c7` … `3f5639e`):
  - Built and verified (0 validation errors in debug): mesh cooker; GPU-driven meshes (instance culling →
    cluster-LOD BVH traversal at 1 px → task/mesh shaders → visibility buffer → resolve); two-phase occlusion
    per instance and per cluster (early clusters hidden by last frame's pyramid are re-tested late; pyramid
    rebuilt after the late pass); battle test; GPU cost split; proxy compare tool.
  - **Battle test (unpaced = full GPU clock):** original meshes 50–116 ms (fail; LOD can't simplify the
    cottage, which is 1,170 disconnected pieces); voxel-remesh proxies 2.5–4.2 ms (fast). Cards cost 0.15–0.4 ms.
  - **Proxy quality:** silhouette IoU 0.95–0.998, but shading-normal error 24–53° mean (p95 83–109°) vs the
    full-detail original → **not acceptable as is** (the user's point: silhouettes AND normals must hold for
    the lighting). All in ARCHITECTURE.md §7.2.
  - Lighting in the resolve is a stand-in; the 2D-card lighting model is undecided (D1 decides it, §7.1).

## Next (the user chooses; both offered)

1. **Proxy LOD for real:** keep the proxy's shape/speed, bake normal + colour + ORM maps from the original onto
   it (UV atlas for the proxy, ray-cast bake), switch per instance once the proxy's error is under a pixel; then
   re-run `--compare` (target: shading-normal error near the original-at-LOD rows) and `--battle --unpaced`.
   Check library versions first (e.g. a UV-atlas library such as xatlas).
2. **Vsync check:** run the battle in a window with vsync to see whether GPU power saving ever makes a frame
   miss its vblank (off-screen pacing can't show it).
Then: M3 exit test passing with real (or properly proxied) buildings → D1 (lit sprites in a spatial world).

## Waiting on the user

- Video → sprite pipeline (their earlier top priority, ARCHITECTURE §5): needs their videos organised as
  `content/video/<Character>/<move>.mp4` (they sit in `Downloads` / `Desktop\Characters` with generated names;
  don't guess the mapping). ffmpeg 9.0.1 and Python + PIL are installed.
- Reference images / one clearly described moment, to turn R1–R6 into D1 pass/fail criteria.
- The final camera angles (camera stays free until then).

## Later milestones

M4 world cells + terrain · M5 lighting/atmosphere · M6 editor · M7 gameplay port + cutscene timeline.
Exit tests and demonstration gates in ARCHITECTURE.md §9.
