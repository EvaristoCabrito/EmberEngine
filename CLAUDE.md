# EmberEngine: handoff for the next Claude session

Read this first, then `docs/ARCHITECTURE.md` (the design, the measured numbers and the milestone exit tests).
Personal memory notes also live in `C:\Users\evari\.claude\projects\C--EmberEngine\memory\`.

## What this is

A game engine built from zero (C++20 + Vulkan 1.3) for the user's game *Ember*: a dark-fantasy JRPG
with real-life-looking 2D character sprites in a fully 3D world (HD-2D, but AAA). Goal in the user's words:
rival a AAA studio as a solo dev using AI only. The bar is Square Enix / Luminous Engine beauty with tools
simple enough for a kid. Predecessors (`C:\Engine2`, browser/three.js) hit memory and quality walls; this
engine sizes every system for the finished game from day one.

## How to work with this user (hard rules)

- **No outside engines**, ever (no Unreal/Epic, Unity, Godot). Open-source *libraries* are fine.
- **No shortcuts.** Size systems for the finished game (see the numbers in ARCHITECTURE.md). If a limit
  is unavoidable, say so up front with the number.
- **Never say "done"** unless it is verified. Say what changed and exactly how/where they will see it.
  No promises or dates about future plans.
- **Short answers**, ownership first. Own mistakes in one line, then fix them. No excuses.
- **Check the real latest version** of any library before adding it (`git ls-remote --tags` / GitHub
  releases API) and say where the version came from. Never from memory.
- **Commit** locally on your own once a change is verified working (builds clean, its test passes); the user granted this 2026-10-14 ("if it's working you can commit"). Say exactly what is and is not verified in the message. **Never push** unless they ask in that moment.
  Git identity is repo-local: EvaristoCabrito <evaristo172@gmail.com>. Remote: github.com/EvaristoCabrito/EmberEngine.
- **Research before building** big visual features (how shipped games do it). Never show raw test renders as progress.
- **2D/3D split (Engine2's logic, the only part of Engine2 carried over):** characters, props and decor are photoreal 2D cards; houses/buildings and terrain shape are real 3D; ground is photoreal texture on 3D terrain. Blender 5.2.2 + `blender` MCP available.
- Fold mid-turn messages into the work; don't stop to ask.
- Never touch `C:\emberashes03D-main` (Ember 3D). `C:\Engine2` is **read-only reference**.
- Measure before fixing. Guesses that didn't pan out (thread priorities) were measured and reverted;
  do the same.

## Build and run (Windows, MSVC 2022 Build Tools, Vulkan SDK 1.4.363.0, CMake + Ninja)

```
build.bat release          # or debug; output in build/<config>/bin
```
- `Sandbox.exe`: window with the performance overlay (F1).
- `Sandbox.exe --capture out.png [--no-validation]`: off-screen; exit 2 = Vulkan validation errors.
- `Sandbox.exe --stream build/stream-test/stream-test.pak --verify build/stream-test/src`: M1 streaming test.
- `Sandbox.exe --sprites build/sprites/roster.pak [--budget 512]`: M2 sprite parade (exit 3 = test failed).
- `Cooker.exe pack|list|synth|import-engine2|sprites|sprite-preview ...`: see the top of `apps/cooker/main.cpp`.
- Sprite roster: `Cooker sprites content/engine2.sprites.json C:/Engine2/public/game build/sprites/roster.pak`
  (cook cache in `build/sprites/cook-cache`; unchanged clips are never re-cooked; full cook ~2.3 min).
- Run perf tests in **release** with `--no-validation`; run correctness in **debug** with validation.

## Code map

- `engine/core`: Log, Jobs (work-stealing job system), Function (inline callables), Memory (BlockPool, Heap).
- `engine/platform`: Window (SDL3), IoQueue (Windows IOCP async reads).
- `engine/assets`: AssetId (FNV-1a of path), Pak format, AssetManager (priority + elevator-ordered merged reads).
- `engine/rhi`: VulkanContext (graphics + dedicated transfer queue), Bindless, Image, Pipeline, Swapchain,
  GpuTimer, UploadQueue (staging ring + transfer queue + timeline semaphore).
- `engine/render`: Renderer (HDR scene, ground, sprites, tonemap, UI), Camera (reversed-Z infinite),
  SpriteFormat (cooked layout), SpriteDraw (instance struct), SpriteSystem (residency), DebugUi (ImGui).
- `engine/debug`: PerfOverlay.
- `tools`: PakWriter, SpriteSource (manifest JSON + Engine2 importer), SpriteCooker (trim, dilate, mips, BC7, cache).
- `apps/sandbox`: test bed + exit tests (StreamTest in main.cpp, SpriteParade.h). `apps/cooker`: the Cooker CLI.
- `shaders`: HLSL compiled by DXC with `-Zpr` (row-major packing): multiply as `mul(v, M)` with glm matrices.
- `content/engine2.sprites.json`: sprite manifest imported from Engine2 (its timing values are placeholders).

## Status (2026-10-14)

- **M0 + M1 done**, committed and pushed as `67410f0` (Vulkan foundation; jobs, IO, pak, asset streaming, overlay).
- **M2 (sprite cooker + residency) done**, committed as `98c727a` and pushed.
  - Cooker: 375 clips / 9,849 frames → BC7 (bc7e scalar, perceptual) with alpha-weighted linear mips, colour
    dilation, union trim, bottom-centre canvas for mixed-size frames, quality report (PSNR), preview images.
    2.63 GB GPU, 19 MB tails, 934 MB on disk. Visual check: decoded frames match the source (no fringes).
  - Runtime: UploadQueue on the dedicated transfer queue (8 MB/frame); SpriteSystem (tails always resident,
    full clips under a GPU budget in a VMA pool made at load, LRU eviction, hole-making eviction when the pool
    is fragmented, deferred destruction); sprite + ground passes; SpriteParade test (paced at 60 Hz off-screen).
  - Results (512 MB budget, RX 6400, shuffled parade): today's 375 clips = PASS, worst frame 3.9 ms; **×10 = the
    finished-game workload** (3,750 clips, 26.3 GB GPU, `build/sprites/roster-x10.pak`, Cooker `--copies 10`) = PASS,
    0 late frames, worst 4.6 ms, peak demand 322 MB; debug + validation = PASS, 0 errors. Workload assumptions
    (MCs at 2x data, rotating, party max 8; the user: testing x10 is enough) in ARCHITECTURE.md §1.
  - Also fixed: window mode crashed (VK_ERROR_INITIALIZATION_FAILED) when a minimise gave the surface a 0×0
    extent; `Swapchain::recreate` now returns false and keeps the old swapchain.

## Next, the user's top priority after M2: video → sprite pipeline

The user's animation videos have the motion they want; Engine2's sprites looked "retarded bad" next to them.
Causes found: Engine2's cut scripts (`C:\Engine2\tools\build-*.py`) keep a fixed 36 frames per move whatever
its length (a 4.75 s idle → ~7.6 fps), mat each frame separately with rembg (shimmering edges), and the game
slides units at an unrelated speed (**feet glide**, which infuriates the user). The plan (agreed in principle,
see ARCHITECTURE.md §5):
- every native video frame at the video's own rate (no frame cap: the user's sheet tool caps at 64, not needed);
- temporally consistent video matting: **research the best current model first**;
- automatic loop points; optional frame interpolation judged side by side with the video;
- **root motion** measured from the planted foot, stored per clip; the game moves units by it.
- Videos go in `content/video/<Character>/<move>.mp4` (asked the user to provide/organize them; many sit in
  `C:\Users\evari\Downloads` and `Desktop\Characters` with generated names. Don't guess the mapping, ask).
- ffmpeg/ffprobe 9.0.1 is installed. Python with PIL is available.

## Later milestones

M3 meshes (GLB → meshlets, GPU culling, PBR) · M4 world cells + terrain · M5 lighting/atmosphere (sprites lit
and shadowed) · M6 editor · M7 gameplay port + cutscene timeline. Exit tests in ARCHITECTURE.md §9.
